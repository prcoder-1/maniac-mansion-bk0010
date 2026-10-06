/* Экран БК (банк C): фон комнаты и объекты в PG_BITMAP, вывод видимой части,
 * текст шрифтом 6x8 с шагом 6.4 точки (x_БК = x*4/5), области глаголов.
 * Соответствует ScummVM gfx.cpp: redrawBGAreas / drawObject / drawDirtyScreenParts,
 * charset.cpp: CharsetRendererV2::printChar, restoreBackground.
 *
 * Экран MM 320x200 -> БК: строки MM y -> строки БК 28 + y; x -> x*4/5. */
#include "scumm.h"
#include "gfx.h"

#define ROW0 28
#define VRAM_ADDR 040000u
#ifdef HOST
extern uint8_t host_vram[040000];
#define vram host_vram
#else
#define vram ((volatile uint8_t *)VRAM_ADDR)
#endif

void bm_blit(uint16_t src, uint16_t stride, uint16_t dst, uint16_t ncols, uint16_t rows);
extern const uint8_t font6[];

static uint8_t bm_room;      /* комната, чей фон с объектами лежит в PG_BITMAP */
static uint8_t view_dirty;

/* цвет EGA текста -> сплошной цвет БК (0 чёрный, 1 синий, 2 зелёный, 3 красный) */
static const uint8_t text_pal[16] = { 0, 1, 2, 1, 3, 3, 3, 2, 1, 1, 2, 2, 3, 1, 3, 2 };

void gfx_room_changed(void)
{
    bm_room = 0;
}

/* object.cpp: drawObject — картинка объекта целиком в фон мира */
void drawObject(uint8_t idx)
{
    struct ObjectData *od = &objs[idx];
    if (od->obj_nr == 0) return;
    uint16_t width = od->width & ~7u, height = od->height & ~7u;
    if (width == 0 || height == 0 || !od->OBIMoffset) return;
    v2_decode(room_page, room_addr + od->OBIMoffset, (uint16_t)od->x_pos, (uint8_t)od->y_pos, width, (uint8_t)height);
    view_dirty = 1;
}

/* изменённые полосы фона мира [dirty_s0, dirty_s1) */
static uint8_t dirty_s0 = 0xFF, dirty_s1;

void markStripsDirty(int16_t minStrip, int16_t maxStrip)
{
    if (minStrip < 0) minStrip = 0;
    if (maxStrip > 120) maxStrip = 120;
    if (minStrip >= maxStrip) return;
    if (minStrip < dirty_s0) dirty_s0 = (uint8_t)minStrip;
    if (maxStrip > dirty_s1) dirty_s1 = (uint8_t)maxStrip;
}

/* gfx.cpp: redrawBGAreas. Фон мира хранится целиком: при сдвиге камеры только
 * перевыводится видимая часть; изменённые полосы (redrawBGStrip) декодируются
 * заново по StripTable, затем объекты на них. Диапазон расширяется до всех
 * перекрывающихся с ним объектов — порядок наложения как при полной отрисовке. */
void redrawBGAreas(void)
{
    if (fullRedraw || bm_room != currentRoom)
    {
        gfx_set_pal();   /* roomOps мог сменить цвета комнаты */
        room_decode(room_page, room_addr);
        bm_room = currentRoom;
        drawRoomObjects();
    }
    else if (bgNeedsRedraw && dirty_s0 < dirty_s1)
    {
        uint8_t s0 = dirty_s0, s1 = dirty_s1, grown;
        do
        {
            grown = 0;
            for (uint8_t i = 1; i < NUM_LOCAL_OBJECTS; i++)
            {
                struct ObjectData *o = &objs[i];
                if (o->obj_nr == 0 || !o->width) continue;
                int16_t a = o->x_pos / 8, b = a + (int16_t)(o->width / 8);
                if (b <= s0 || a >= s1) continue;
                if (a < s0) { s0 = (uint8_t)a; grown = 1; }
                if (b > s1) { s1 = (uint8_t)b; grown = 1; }
            }
        } while (grown);
        room_redraw_strips(s0, s1);
        drawRoomObjectsRange(s0, s1);
    }
    dirty_s0 = 0xFF;
    dirty_s1 = 0;
    bgNeedsRedraw = 0;
    view_dirty = 1;
}

/* gfx.cpp: drawDirtyScreenParts — видимая часть фона (начало округлено до байта) */
void drawDirtyScreenParts(void)
{
    if (!view_dirty || currentRoom == 0) return;
    view_dirty = 0;
    uint16_t x = (uint16_t)(screenStartStrip * 8);
    uint16_t bcol = (uint16_t)((x - x / 5) >> 2);
    uint16_t ncols = 64;
    if (bcol + ncols > bm_stride) ncols = bm_stride > bcol ? bm_stride - bcol : 0;
    bm_blit(PAGE_ADDR + bcol, bm_stride, VRAM_ADDR + (ROW0 + 16) * 64, ncols, (uint16_t)roomHeight);
}

void actor_draw(uint8_t act)
{
    (void)act;
}

uint8_t actor_in_strip(int16_t strip, uint8_t act)
{
    (void)strip;
    (void)act;
    return 0;
}

/* --- текст --- */

static uint16_t bk_x(int16_t x)
{
    return (uint16_t)(x - x / 5);
}

/* точки [bx0, bx1) строки row залить цветом c (0..3) */
static void hline(uint16_t row, uint16_t bx0, uint16_t bx1, uint8_t c)
{
    volatile uint8_t *p = vram + row * 64;
    uint8_t fill = (uint8_t)(c * 0125);
    for (uint16_t x = bx0; x < bx1; x++)
    {
        uint8_t sh = (uint8_t)((x & 3) * 2);
        p[x >> 2] = (uint8_t)((p[x >> 2] & ~(3 << sh)) | (fill & (3 << sh)));
    }
}

/* restoreBackground: прямоугольник экрана MM — чёрным */
void screen_clear_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    if (y1 > 200) y1 = 200;
    if (x1 > 320) x1 = 320;
    for (int16_t y = y0; y < y1; y++) hline((uint16_t)(ROW0 + y), bk_x(x0), bk_x(x1), 0);
}

/* перекраска точек цвета from в to (подсветка инвентаря) */
void screen_recolor(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t from, uint8_t to)
{
    uint8_t f = text_pal[from & 15], t = text_pal[to & 15];
    if (y1 > 200) y1 = 200;
    for (int16_t y = y0; y < y1; y++)
    {
        volatile uint8_t *p = vram + (uint16_t)(ROW0 + y) * 64;
        for (uint16_t x = bk_x(x0); x < bk_x(x1); x++)
        {
            uint8_t sh = (uint8_t)((x & 3) * 2);
            if (((p[x >> 2] >> sh) & 3) == f) p[x >> 2] = (uint8_t)((p[x >> 2] & ~(3 << sh)) | (t << sh));
        }
    }
}

/* CharsetRendererV2: знак 8x8 -> 6x8 в точке (x, y) экрана MM; фон знака — чёрный */
void charset_printChar(int16_t x, int16_t y, uint8_t c, uint8_t color)
{
    if (y < 0 || y > 192) return;
    uint16_t bx = bk_x(x);
    uint8_t col = text_pal[color & 15];
    const uint8_t *g = font6 + (uint16_t)(c & 0x7F) * 8;
    for (uint8_t r = 0; r < 8; r++)
    {
        volatile uint8_t *p = vram + (uint16_t)(ROW0 + y + r) * 64;
        uint8_t bits = g[r];
        for (uint8_t i = 0; i < 6; i++)
        {
            uint16_t px = bx + i;
            if (px >= 256) break;
            uint8_t sh = (uint8_t)((px & 3) * 2);
            uint8_t v = (bits & (040 >> i)) ? col : 0;
            p[px >> 2] = (uint8_t)((p[px >> 2] & ~(3 << sh)) | (v << sh));
        }
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

/* --- курсор (cursor.cpp: ScummEngine_v2::setBuiltinCursor, крестик ПК 23x21) --- */

static int8_t cur_dx[64], cur_dy[64];
static uint8_t cur_n;
static uint16_t under_addr[64];
static uint8_t under_val[64], under_n, cur_anim;

static void cur_pt(int8_t dx, int8_t dy)
{
    cur_dx[cur_n] = dx;
    cur_dy[cur_n] = dy;
    cur_n++;
}

static void cursor_shape(void)
{
    for (int8_t i = 0; i < 7; i++)
    {
        cur_pt((int8_t)(-5 - i), 0);
        cur_pt((int8_t)(5 + i), 0);
    }
    for (int8_t i = 0; i < 8; i++)
    {
        cur_pt(0, (int8_t)(-3 - i));
        cur_pt(0, (int8_t)(3 + i));
    }
    for (int8_t i = 1; i <= 3; i++)
    {
        cur_pt((int8_t)(-5 - i), (int8_t)-i);
        cur_pt((int8_t)(-5 - i), i);
        cur_pt((int8_t)(5 + i), (int8_t)-i);
        cur_pt((int8_t)(5 + i), i);
        cur_pt((int8_t)-i, (int8_t)(-i - 3));
        cur_pt(i, (int8_t)(-i - 3));
        cur_pt((int8_t)-i, (int8_t)(i + 3));
        cur_pt(i, (int8_t)(i + 3));
    }
    cur_pt(-7, -1);
    cur_pt(7, -1);
    cur_pt(-7, 1);
    cur_pt(7, 1);
    cur_pt(-1, -5);
    cur_pt(1, -5);
    cur_pt(-1, 5);
    cur_pt(1, 5);
}

/* убрать курсор: вернуть байты экрана под ним */
void cursor_hide(void)
{
    while (under_n)
    {
        under_n--;
        vram[under_addr[under_n]] = under_val[under_n];
    }
}

/* нарисовать курсор в (mouse_x, mouse_y); цвет мигает как animateCursor */
void cursor_show(void)
{
    static const uint8_t colors[4] = { 15, 15, 7, 8 };   /* default_cursor_colors */
    if (!cur_n) cursor_shape();
    cur_anim++;
    if (cursorState <= 0) return;
    uint8_t c = text_pal[colors[(cur_anim >> 1) & 3]];
    for (uint8_t i = 0; i < cur_n; i++)
    {
        int16_t x = mouse_x + cur_dx[i], y = mouse_y + cur_dy[i];
        if (x < 0 || x >= 320 || y < 0 || y >= 200) continue;
        uint16_t bx = bk_x(x), a = (uint16_t)((ROW0 + y) * 64 + (bx >> 2));
        uint8_t sh = (uint8_t)((bx & 3) * 2);
        under_addr[under_n] = a;
        under_val[under_n] = vram[a];
        under_n++;
        vram[a] = (uint8_t)((vram[a] & ~(3 << sh)) | (c << sh));
    }
}

/* чтение с диска затёрло экран: всё заново */
void gfx_clear_all(void)
{
    for (uint16_t i = 0; i < 040000; i++) vram[i] = 0;
    under_n = 0;
    view_dirty = 1;
}
