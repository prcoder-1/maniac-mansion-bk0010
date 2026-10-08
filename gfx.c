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

extern const uint8_t font6[];

static uint8_t bm_room;      /* комната, чей фон с объектами лежит в PG_BITMAP */
static uint8_t view_dirty;          /* 1 — вся видимая часть, 2 — полосы мира [vs0, vs1) */
static uint8_t vs0, vs1;

static void view_mark(uint8_t s0, uint8_t s1)
{
    if (view_dirty == 1) return;
    if (view_dirty == 0)
    {
        vs0 = s0;
        vs1 = s1;
    }
    else
    {
        if (s0 < vs0) vs0 = s0;
        if (s1 > vs1) vs1 = s1;
    }
    view_dirty = 2;
}

/* цвет палитры v1 (tableV1Palette, 17 цветов) -> сплошной цвет БК текста
 * (0 чёрный, 1 синий, 2 зелёный, 3 красный): белый, серые светлые — зелёный,
 * красные, жёлтый, коричневый — красный, синие, голубой, тёмно-серые — синий */
static const uint8_t text_pal[17] = { 0, 2, 3, 1, 3, 2, 1, 3, 3, 3, 3, 1, 2, 2, 1, 1, 1 };
/* CGA: charset.cpp translateColor (CGAtextColorMap), затем ditherCGA — сплошные
 * голубой/пурпурный/белый -> зелёный/синий/красный БК */
static const uint8_t text_pal_cga[16] = { 0, 2, 2, 2, 1, 1, 1, 3, 3, 2, 2, 2, 1, 1, 3, 3 };

static uint8_t text_color(uint8_t c)
{
    if (c > 16) c = 16;
    return render_cga ? text_pal_cga[c & 15] : text_pal[c];
}

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
    view_mark((uint8_t)(od->x_pos / 8), (uint8_t)((od->x_pos + width) / 8));
}

/* изменённые полосы фона мира [dirty_s0, dirty_s1) */
static uint8_t dirty_s0 = 0xFF, dirty_s1;

void markStripsDirty(int16_t minStrip, int16_t maxStrip)
{
    if (minStrip < 0) minStrip = 0;
    if (maxStrip > 200) maxStrip = 200;
    if (minStrip >= maxStrip) return;
    if (minStrip < dirty_s0) dirty_s0 = (uint8_t)minStrip;
    if (maxStrip > dirty_s1) dirty_s1 = (uint8_t)maxStrip;
}

/* gfx.cpp: redrawBGAreas. Фон — кольцо групп (gfx_v1.c): при сдвиге камеры
 * недостающие группы видимой части дорисовываются, видимая часть перевыводится;
 * изменённые полосы (redrawBGStrip) декодируются заново, затем объекты на них.
 * Диапазон расширяется до всех перекрывающихся с ним объектов — порядок наложения
 * как при полной отрисовке. */
void redrawBGAreas(void)
{
    uint8_t full = fullRedraw || bm_room != currentRoom || camera.cur_x != camera.last_x;
    if (fullRedraw || bm_room != currentRoom)
    {
        gfx_set_pal();   /* roomOps мог сменить цвета комнаты */
        room_decode(room_page, room_addr);
        bm_room = currentRoom;
        drawRoomObjects();
    }
    else
    {
        if (camera.cur_x != camera.last_x) view_ensure();
        if (bgNeedsRedraw && dirty_s0 < dirty_s1)
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
            view_mark(s0, s1);
        }
    }
    dirty_s0 = 0xFF;
    dirty_s1 = 0;
    bgNeedsRedraw = 0;
    if (full) view_dirty = 1;
}

/* gfx.cpp: drawDirtyScreenParts. Видимая часть фона выводится до актёров
 * (gfx_view_flush из processActors) — здесь только если актёров не рисовали */
void drawDirtyScreenParts(void)
{
    gfx_view_flush();
}

/* колонки фона [col, col + ncols) кольца (mod bm_stride), строки [y, y + nrows) ->
 * видеопамять vaddr (строки по 64): через край кольца — двумя кусками */
void bm_blit_ring(uint16_t col, uint16_t y, uint16_t vaddr, uint16_t ncols, uint16_t nrows)
{
    if (col >= bm_stride) col -= bm_stride;
    uint16_t n1 = bm_stride - col < ncols ? bm_stride - col : ncols;
    uint16_t row = PAGE_ADDR + y * bm_stride;
    bm_blit(row + col, bm_stride, vaddr, n1, nrows);
    if (n1 < ncols) bm_blit(row, bm_stride, vaddr + n1, ncols - n1, nrows);
}

/* видимая часть фона (начало округлено до байта) -> экран; 1 — вся, 2 — часть */
uint8_t gfx_view_flush(void)
{
    if (!view_dirty || currentRoom == 0) return 0;
    uint16_t x = (uint16_t)(screenStartStrip * 8), px = PHYS_X(x);   /* точка кольца */
    uint16_t bcol = BK_X(px) >> 2;
    uint16_t c0 = 0, c1 = 64;
    if (view_dirty == 2)
    {
        /* только полосы [vs0, vs1), если видны */
        int16_t a = (int16_t)vs0 * 8 - (int16_t)x, b = (int16_t)vs1 * 8 - (int16_t)x;
        if (b <= 0 || a >= 320)
        {
            view_dirty = 0;
            return 0;
        }
        if (a < 0) a = 0;
        if (b > 320) b = 320;
        c0 = (uint16_t)((BK_X(px + a) >> 2) - bcol);
        c1 = (uint16_t)(((BK_X(px + b - 1)) >> 2) - bcol + 1);
        if (c1 > 64) c1 = 64;
    }
    uint8_t whole = view_dirty == 1;
    view_dirty = 0;
    if (!bm_ring && bcol + c1 > bm_stride) c1 = bm_stride > bcol ? bm_stride - bcol : 0;
    if (c0 >= c1) return 0;
    cursor_hide();
    for (uint16_t y = 0; y < roomHeight; y += 32)   /* кусками: тон между ними */
    {
        bm_blit_ring(bcol + c0, y, VRAM_ADDR + (ROW0 + 16 + y) * 64 + c0, c1 - c0,
                     roomHeight - y < 32 ? roomHeight - y : 32);
        SND_POLL();
    }
    return whole ? 1 : 2;
}


/* --- текст --- */

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
    cursor_hide();
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
    cursor_hide();
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

/* CharsetRendererV2: знак 8x8 -> 6x8 в точке (x, y) экрана MM; фон знака — чёрный */
void charset_printChar(int16_t x, int16_t y, uint8_t c, uint8_t color)
{
    if (y < 0 || y > 192) return;
    cursor_hide();
    uint16_t bx = bk_x(x);
    uint8_t fill = (uint8_t)(text_color(color) * 0125);
    const uint8_t *g = font6 + (uint16_t)(c & 0x7F) * 8;
    uint8_t n = 6;
    if (bx >= 256) return;
    if (bx + 6 > 256) n = (uint8_t)(256 - bx);
    /* точки знака — в 2-3 байтах строки: маски байтов на точку считаются раз */
    uint8_t off[6], pm[6], clr[3] = { 0, 0, 0 };
    for (uint8_t i = 0; i < n; i++)
    {
        uint16_t px = bx + i;
        off[i] = (uint8_t)((px >> 2) - (bx >> 2));
        pm[i] = (uint8_t)(px_mask[px & 3] & fill);
        clr[off[i]] |= px_mask[px & 3];
    }
    volatile uint8_t *row = vram + (uint16_t)(ROW0 + y) * 64 + (bx >> 2);
    for (uint8_t r = 0; r < 8; r++, row += 64)
    {
        uint8_t bits = g[r], set[3] = { 0, 0, 0 };
        for (uint8_t i = 0; i < n; i++, bits <<= 1)
            if (bits & 040) set[off[i]] |= pm[i];
        row[0] = (uint8_t)((row[0] & ~clr[0]) | set[0]);
        if (clr[1]) row[1] = (uint8_t)((row[1] & ~clr[1]) | set[1]);
        if (clr[2]) row[2] = (uint8_t)((row[2] & ~clr[2]) | set[2]);
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

/* --- курсор (cursor.cpp: ScummEngine_v2::setBuiltinCursor, крестик ПК 23x21) --- */

static int8_t cur_dx[64], cur_dy[64];
static uint8_t cur_n;
static uint8_t cur_anim;
/* байты экрана под курсором: адрес, биты точек курсора, прежнее значение */
static uint16_t cb_addr[64];
static uint8_t cb_mask[64], cb_under[64], cb_n;
static int16_t cb_x = -1, cb_y;   /* для какого места построены cb_* */

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

/* Курсор прячется лениво — только перед выводом на экран (функции вывода зовут
 * cursor_hide) и рисуется снова в конце шага цикла: виден всё время, пока игра
 * считает, а не лишь в простое между шагами */
static uint8_t cur_vis, cur_c;   /* нарисован; его цвет */
static int16_t cur_x, cur_y;     /* и место */

/* убрать курсор: вернуть байты экрана под ним */
void cursor_hide(void)
{
    if (!cur_vis) return;
    cur_vis = 0;
    for (uint8_t j = 0; j < cb_n; j++)
    {
        if (!(j & 7)) SND_TICK();
        vram[cb_addr[j]] = cb_under[j];
    }
}

/* байты экрана курсора в (mouse_x, mouse_y): точки одного байта — вместе */
static void cursor_bytes(void)
{
    cb_n = 0;
    for (uint8_t i = 0; i < cur_n; i++)
    {
        int16_t x = mouse_x + cur_dx[i], y = mouse_y + cur_dy[i];
        if (x < 0 || x >= 320 || y < 0 || y >= 200) continue;
        uint16_t bx = bk_x(x), a = (uint16_t)((ROW0 + y) * 64 + (bx >> 2));
        uint8_t m = (uint8_t)(3 << ((bx & 3) * 2)), j = cb_n;
        while (j && cb_addr[j - 1] != a) j--;
        if (j)
            cb_mask[j - 1] |= m;
        else
        {
            cb_addr[cb_n] = a;
            cb_mask[cb_n++] = m;
        }
    }
}

/* нарисовать курсор в (mouse_x, mouse_y); цвет мигает как animateCursor */
void cursor_show(void)
{
    static const uint8_t colors[4] = { 1, 1, 12, 11 };   /* default_v1_cursor_colors */
    if (!cur_n) cursor_shape();
    cur_anim++;
    if (cursorState <= 0)
    {
        cursor_hide();
        return;
    }
    uint8_t c = text_color(colors[(cur_anim >> 1) & 3]);
    if (cur_vis && c == cur_c && mouse_x == cur_x && mouse_y == cur_y) return;
    if (!cur_vis || mouse_x != cur_x || mouse_y != cur_y)
    {
        cursor_hide();
        if (mouse_x != cb_x || mouse_y != cb_y)
        {
            cb_x = mouse_x;
            cb_y = mouse_y;
            cursor_bytes();
        }
        cur_x = mouse_x;
        cur_y = mouse_y;
        for (uint8_t j = 0; j < cb_n; j++) cb_under[j] = vram[cb_addr[j]];
    }
    cur_vis = 1;
    cur_c = c;
    static const uint8_t fills[4] = { 0, 0125, 0252, 0377 };
    uint8_t fill = fills[c & 3];   /* цвет во всех 4 точках байта */
    for (uint8_t j = 0; j < cb_n; j++)
    {
        if (!(j & 7)) SND_TICK();
        uint8_t m = cb_mask[j];
        vram[cb_addr[j]] = (uint8_t)((cb_under[j] & ~m) | (fill & m));
    }
}

/* нижняя четверть экрана (070000..077777: глаголы, инвентарь) — после надписи
 * LOADING; курсор сначала снят, иначе он вернёт её обрывки */
void gfx_clear_bottom(void)
{
    cursor_hide();
    volatile uint16_t *w = (volatile uint16_t *)(vram + 030000);   /* 070000 */
    for (uint16_t i = 0; i < 04000; i++)
    {
        w[i] = 0;
        if (!(i & 037)) SND_POLL();
    }
}

/* чтение с диска затёрло экран: всё заново */
void gfx_clear_all(void)
{
    volatile uint16_t *w = (volatile uint16_t *)vram;
    for (uint16_t i = 0; i < 020000; i++)
    {
        w[i] = 0;
        if (!(i & 037)) SND_POLL();   /* 16 Кбайт — не меньше 30 мс */
    }
    cur_vis = 0;
    view_dirty = 1;
}
