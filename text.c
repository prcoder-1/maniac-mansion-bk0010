/* Текст на экране БК (банк B; в банке C места нет): шрифт 6x8 с шагом 6.4 точки
 * (x_БК = x*4/5), очистка и перекраска областей глаголов и инвентаря.
 * charset.cpp: CharsetRendererV2::printChar, restoreBackground. Вызывают банки A и D
 * (дальний вызов — как и прежде из банка C); курсор — в банке C, его место —
 * в ядре: прятать его дальним вызовом только когда вывод его задевает. */
#include "scumm.h"
#include "gfx.h"

#define ROW0 28
#ifdef HOST
extern uint8_t host_vram[040000];
#define vram host_vram
#else
#define vram ((volatile uint8_t *)040000u)
#endif

extern const uint8_t font6[];

/* цвет палитры v1 (tableV1Palette, 17 цветов) -> сплошной цвет БК текста
 * (0 чёрный, 1 синий, 2 зелёный, 3 красный): белый, серые светлые — зелёный,
 * красные, жёлтый, коричневый — красный, синие, голубой, тёмно-серые — синий */
static const uint8_t text_pal[17] = { 0, 2, 3, 1, 3, 2, 1, 3, 3, 3, 3, 1, 2, 2, 1, 1, 1 };

static uint8_t text_color(uint8_t c)
{
    if (c > 16) c = 16;
    return text_pal[c];
}

/* как cursor_hide_rect (gfx.c), но проверка — здесь: дальний вызов только при перекрытии */
static void hide_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    if (cur_vis && cur_x + 12 > x0 && cur_x - 12 < x1 && cur_y + 11 > y0 && cur_y - 11 < y1) cursor_hide();
}



static uint16_t bk_x(int16_t x)
{
    return BK_X(x);
}

/* маски точки k (0..3) в байте БК */
static const uint8_t px_mask[4] = { 03, 014, 060, 0300 };

/* точки [bx0, bx1) строки row залить цветом c (0..3): края — по маске, середина — байтами */
static void hline(uint16_t row, uint16_t bx0, uint16_t bx1, uint8_t c)
{
    if (bx0 >= bx1) return;
    volatile uint8_t *p = vram + row * 64;
    uint8_t fill = (uint8_t)(c * 0125);
    uint16_t b0 = bx0 >> 2, b1 = (bx1 - 1) >> 2;
    uint8_t m0 = (uint8_t)(0377 << ((bx0 & 3) * 2));          /* точки от bx0 до конца байта */
    uint8_t m1 = (uint8_t)(0377 >> ((3 - ((bx1 - 1) & 3)) * 2)); /* от начала байта до bx1-1 */
    if (b0 == b1)
    {
        uint8_t m = m0 & m1;
        p[b0] = (uint8_t)((p[b0] & ~m) | (fill & m));
        return;
    }
    p[b0] = (uint8_t)((p[b0] & ~m0) | (fill & m0));
    for (uint16_t b = b0 + 1; b < b1; b++) p[b] = fill;
    p[b1] = (uint8_t)((p[b1] & ~m1) | (fill & m1));
}

/* restoreBackground: прямоугольник экрана MM — чёрным */
void screen_clear_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    hide_rect(x0, y0, x1, y1);
    if (y1 > 200) y1 = 200;
    if (x1 > 320) x1 = 320;
    for (int16_t y = y0; y < y1; y++)
    {
        hline((uint16_t)(ROW0 + y), bk_x(x0), bk_x(x1), 0);
        SND_POLL();
    }
}

/* перекраска точек цвета from в to (подсветка инвентаря) */
void screen_recolor(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t from, uint8_t to)
{
    uint8_t f = text_color(from), t = text_color(to);
    hide_rect(x0, y0, x1, y1);
    if (y1 > 200) y1 = 200;
    for (int16_t y = y0; y < y1; y++)
    {
        volatile uint8_t *p = vram + (uint16_t)(ROW0 + y) * 64;
        for (uint16_t x = bk_x(x0); x < bk_x(x1); x++)
        {
            uint8_t sh = (uint8_t)((x & 3) * 2);
            if (((p[x >> 2] >> sh) & 3) == f) p[x >> 2] = (uint8_t)((p[x >> 2] & ~(3 << sh)) | (t << sh));
        }
        SND_POLL();
    }
}

/* chr_t[p][g]: точки строки знака g (6 бит, старший — левая точка) с точки p байта
 * БК — по 2 бита на точку в трёх байтах (четвёртый — чтобы индекс был сдвигом) */
static uint8_t chr_t[4][64][4];
static uint8_t chr_t_ok;

static void chr_t_init(void)
{
    for (uint8_t p = 0; p < 4; p++)
        for (uint8_t g = 0; g < 64; g++)
            for (uint8_t i = 0; i < 6; i++)
                if (g & (040 >> i)) chr_t[p][g][(p + i) >> 2] |= px_mask[(p + i) & 3];
    chr_t_ok = 1;
}

/* CharsetRendererV2: знак 8x8 -> 6x8 в точке (x, y) экрана MM; фон знака — чёрный.
 * Строка знака — байты из chr_t (перебор точек стоил ~3 мс на знак) */
void charset_printChar(int16_t x, int16_t y, uint8_t c, uint8_t color)
{
    if (y < 0 || y > 192) return;
    hide_rect(x, y, x + 8, y + 8);
    uint16_t bx = bk_x(x);
    if (bx >= 256) return;
    if (!chr_t_ok) chr_t_init();
    static const uint8_t fill4[4] = { 0, 0125, 0252, 0377 };   /* цвет во всех точках байта */
    uint8_t fill = fill4[text_color(color)];
    const uint8_t *g = font6 + (uint16_t)(c & 0x7F) * 8;
    uint8_t gm = 077;   /* у правого края — только точки до 256 */
    if (bx + 6 > 256) gm = (uint8_t)((077 << (bx + 6 - 256)) & 077);
    const uint8_t (*t)[4] = chr_t[bx & 3];
    const uint8_t *cm = t[gm];
    uint8_t c0 = (uint8_t)~cm[0], c1 = (uint8_t)~cm[1], c2 = (uint8_t)~cm[2];
    volatile uint8_t *row = vram + (uint16_t)(ROW0 + y) * 64 + (bx >> 2);
    for (uint8_t r = 0; r < 8; r++, row += 64)
    {
        const uint8_t *s = t[g[r] & gm];
        row[0] = (uint8_t)((row[0] & c0) | (s[0] & fill));
        if (cm[1]) row[1] = (uint8_t)((row[1] & c1) | (s[1] & fill));
        if (cm[2]) row[2] = (uint8_t)((row[2] & c2) | (s[2] & fill));
        if (r & 1) SND_POLL();
    }
}

void charset_clear(void)
{
    screen_clear_rect(0, 0, 320, 16);
}

/* drawString: строка без кодов (глаголы, инвентарь, предложение); правый край в точках MM */
int16_t charset_drawString(int16_t x, int16_t y, uint8_t color, const uint8_t *s)
{
    for (; *s; s++)
    {
        if (*s == '@') continue;
        charset_printChar(x, y, *s, color);
        x += 8;
    }
    return x;
}
