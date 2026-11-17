/* Модуль AZБК: интерфейс в слое 1 (текст 8x8, очистка, перекраска, LOADING) и курсор в
 * слое 0. Точка x MM — 96 + x слоя, строка y — 28 + y; цвет 0 слоя прозрачен — под ним
 * чёрные строки слоя 2. */
#include "scumm.h"
#include "az.h"
#include "azm.h"


/* 4 точки (бит 3 — левая) -> слово тетрад 0/017 (левая точка — младшая тетрада) */
const uint16_t az_ex4[16] = {
    0, 0170000, 07400, 0177400, 0360, 0170360, 07760, 0177760,
    017, 0170017, 07417, 0177417, 0377, 0170377, 07777, 0177777,
};

/* строка line слоя 1 в окнах 12-13 (видны строки line..line+7): адрес её начала */
static volatile uint16_t *l1row(uint16_t line)
{
    win12(AZ_L1 + (line >> 3));
    win13(AZ_L1 + (line >> 3) + 1);
    return (volatile uint16_t *)(0140000u + ((line & 7) << 9));
}

/* слово w со сдвигом на s тетрад вправо по экрану (влево по битам) и его хвост */
static uint16_t shl(uint16_t w, uint8_t s)
{
    if (s & 2) w <<= 8;
    if (s & 1) w <<= 4;
    return w;
}

static uint16_t shr(uint16_t w, uint8_t s)
{
    s = (uint8_t)(4 - s);
    if (s & 2) w >>= 8;
    if (s & 1) w >>= 4;
    return w;
}

/* знак (строки glyph — в банке вызывающего) цветом color в точке (x, y) экрана MM; фон
 * знака — 0 (чёрный) */
uint16_t az_char(uint16_t fn, int16_t x, int16_t y, const uint8_t *glyph, uint16_t color)
{
    (void)fn;
    if (y < 0 || y > 248 - AZ_ROW0 || x < 0 || x >= 320) return 0;
    uint16_t px = (uint16_t)(AZ_X0 + x);
    uint8_t g[8];   /* до окон 12/13: знак может лежать выше 0140000 */
    for (uint8_t r = 0; r < 8; r++) g[r] = glyph[r];
    uint16_t fill = (uint16_t)((color & 15) * 010421);
    volatile uint16_t *row = l1row((uint16_t)(AZ_ROW0 + y)) + (px >> 2);
    uint8_t s = (uint8_t)(px & 3);
    uint16_t keep = s ? (uint16_t)(0177777u >> (16 - 4 * s)) : 0;
    for (uint8_t r = 0; r < 8; r++, row += 256)
    {
        uint16_t wl = az_ex4[g[r] >> 4] & fill, wr = az_ex4[g[r] & 15] & fill;
        if (!s)
        {
            row[0] = wl;
            row[1] = wr;
            continue;
        }
        row[0] = (uint16_t)((row[0] & keep) | shl(wl, s));
        row[1] = (uint16_t)(shr(wl, s) | shl(wr, s));
        row[2] = (uint16_t)((row[2] & ~keep) | shr(wr, s));
    }
    win_back();
    return 0;
}

/* прямоугольник экрана MM [x0, x1) x [y0, y1) слоя 1 — нулями: по словам — блиттером,
 * неровные края — процессором */
uint16_t az_clear(uint16_t fn, int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    (void)fn;
    if (x1 > 320) x1 = 320;
    if (y1 > 248 - AZ_ROW0) y1 = 248 - AZ_ROW0;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x0 >= x1 || y0 >= y1) return 0;
    uint16_t a = (uint16_t)(AZ_X0 + x0), b = (uint16_t)(AZ_X0 + x1);
    uint16_t wa = (a + 3) >> 2, wb = b >> 2;
    if (wa + 1 < wb)
    {
        bi(AZ_HI(AZ_L1), 0, wa, 0, wb - wa, (uint16_t)(y1 - y0), 256, (uint16_t)(AZ_ROW0 + y0), 0);
        blt_now();
    }
    else
        wa = wb = b >> 2;   /* узкий — весь процессором */
    if ((a & 3) || (b & 3) || wa == wb)
        for (int16_t y = y0; y < y1; y++)
        {
            volatile uint16_t *row = l1row((uint16_t)(AZ_ROW0 + y));
            for (uint16_t p = a; p < b; p++)
            {
                if (p >> 2 >= wa && p >> 2 < wb) continue;
                row[p >> 2] &= (uint16_t)~(017 << ((p & 3) << 2));
            }
        }
    win_back();
    return 0;
}

/* точки цвета from -> to (подсветка инвентаря) */
uint16_t az_recolor(uint16_t fn, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t from, uint16_t to)
{
    (void)fn;
    if (x1 > 320) x1 = 320;
    if (y1 > 200) y1 = 200;
    from &= 15;
    to &= 15;
    for (int16_t y = y0; y < y1; y++)
    {
        volatile uint16_t *row = l1row((uint16_t)(AZ_ROW0 + y));
        for (uint16_t p = (uint16_t)(AZ_X0 + x0); p < (uint16_t)(AZ_X0 + x1); p++)
        {
            uint8_t sh = (uint8_t)((p & 3) << 2);
            uint16_t w = row[p >> 2];
            if (((w >> sh) & 017) == from) row[p >> 2] = (uint16_t)((w & ~(017 << sh)) | (to << sh));
        }
    }
    win_back();
    return 0;
}

/* надпись LOADING и имя файла под экраном игры (строка y 204 MM); 0 — стереть */
uint16_t az_label(uint16_t fn, const char *name, const uint8_t *font)
{
    (void)fn;
    az_clear(0, 0, 204, 320, 212);
    if (!name) return 0;
    int16_t x = 72;
    for (const char *s = "LOADING "; *s; s++, x += 8) az_char(0, x, 204, font + ((*s & 0177) << 3), 7);
    for (; *name && x < 320; name++, x += 8) az_char(0, x, 204, font + ((*name & 0177) << 3), 7);
    return 0;
}

/* --- курсор (крестик ПК 23x21, gfx.c cursor_shape) в слое 0 ---
 * Картинки 21 строка по CP слов (точки — в первых CW): сдвиг 0..3 точки x 2 цвета (15 и
 * 7), картинка k — в странице AZ_CURS + k. Строка в 32 слова — чтобы SRC копии был кратен
 * 32 словам и при обрезке сверху. Блиттером: старое место — заливкой 0, новое — копией
 * (лишние нули ничего не портят: в слое 0 больше ничего нет, прятать не нужно). */
#define CW 7
#define CP 32
#define CH 21
static int16_t cur_w = -1, cur_l;   /* слово и строка слоя прошлого вывода (-1 — нет) */
static uint8_t cur_h, cur_k;        /* строк; картинка */
static int16_t cur_x, cur_y;        /* точка MM прошлого вывода */

static void cpt(volatile uint16_t *img, int8_t dx, int8_t dy, uint8_t ph, uint16_t c)
{
    uint8_t p = (uint8_t)(ph + dx + 11);
    img[(dy + 10) * CP + (p >> 2)] |= (uint16_t)(c << ((p & 3) << 2));
}

static void cshape(volatile uint16_t *img, uint8_t ph, uint16_t c)
{
    for (int8_t i = 0; i < 7; i++)
    {
        cpt(img, (int8_t)(-5 - i), 0, ph, c);
        cpt(img, (int8_t)(5 + i), 0, ph, c);
    }
    for (int8_t i = 0; i < 8; i++)
    {
        cpt(img, 0, (int8_t)(-3 - i), ph, c);
        cpt(img, 0, (int8_t)(3 + i), ph, c);
    }
    for (int8_t i = 1; i <= 3; i++)
    {
        cpt(img, (int8_t)(-5 - i), (int8_t)-i, ph, c);
        cpt(img, (int8_t)(-5 - i), i, ph, c);
        cpt(img, (int8_t)(5 + i), (int8_t)-i, ph, c);
        cpt(img, (int8_t)(5 + i), i, ph, c);
        cpt(img, (int8_t)-i, (int8_t)(-i - 3), ph, c);
        cpt(img, i, (int8_t)(-i - 3), ph, c);
        cpt(img, (int8_t)-i, (int8_t)(i + 3), ph, c);
        cpt(img, i, (int8_t)(i + 3), ph, c);
    }
    cpt(img, -7, -1, ph, c);
    cpt(img, 7, -1, ph, c);
    cpt(img, -7, 1, ph, c);
    cpt(img, 7, 1, ph, c);
    cpt(img, -1, -5, ph, c);
    cpt(img, 1, -5, ph, c);
    cpt(img, -1, 5, ph, c);
    cpt(img, 1, 5, ph, c);
}

void az_cur_init(void)
{
    volatile uint16_t *img = (volatile uint16_t *)0140000;
    for (uint8_t k = 0; k < 8; k++)
    {
        win12(AZ_CURS + k);
        for (uint16_t i = 0; i < CP * CH; i++) img[i] = 0;
        cshape(img, (uint8_t)(k >> 1), (k & 1) ? 7 : 15);
    }
    cur_w = -1;
}

void az_cur_lost(void)
{
    cur_w = -1;
}

/* курсор с центром (x, y) экрана MM цветом color (15 или 7); 0 — убрать */
uint16_t az_cursor(uint16_t fn, int16_t x, int16_t y, uint16_t color)
{
    (void)fn;
    int16_t px = (int16_t)(AZ_X0 - 11 + x), l = (int16_t)(AZ_ROW0 - 10 + y), skip = 0;
    uint8_t k = (uint8_t)(((px & 3) << 1) | (color == 7));
    if (color && cur_w >= 0 && x == cur_x && y == cur_y && k == cur_k) return 0;
    if (cur_w >= 0)
        bi(AZ_HI(AZ_L0), 0, (uint16_t)cur_w, 0, CW, cur_h, 256, (uint16_t)cur_l, 0);
    cur_w = -1;
    if (color)
    {
        int16_t h = CH;
        if (l < AZ_ROW0)
        {
            skip = AZ_ROW0 - l;
            l = AZ_ROW0;
        }
        if (l + h - skip > AZ_ROW0 + 200) h = (int16_t)(AZ_ROW0 + 200 - l + skip);
        h -= skip;
        if (h > 0)
        {
            bi(AZ_HI(AZ_CURS + k) << 8 | AZ_HI(AZ_L0), (uint16_t)(AZ_LO(AZ_CURS + k) + skip * CP), (uint16_t)(px >> 2), 011,
               CP, (uint16_t)h, 256, (uint16_t)l, 0);
            cur_w = (int16_t)(px >> 2);
            cur_l = l;
            cur_h = (uint8_t)h;
            cur_k = k;
            cur_x = x;
            cur_y = y;
        }
    }
    blt_now();
    win_back();
    return 0;
}
