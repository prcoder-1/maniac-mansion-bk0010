/* Экран БК (банк C): фон комнаты и объекты в PG_BITMAP, вывод видимой части,
 * курсор (текст — text.c, банк B).
 * Соответствует ScummVM gfx.cpp: redrawBGAreas / drawObject / drawDirtyScreenParts.
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
    uint16_t row = bm_row[y];
    bm_blit(row + col, bm_stride, vaddr, n1, nrows);
    if (n1 < ncols) bm_blit(row, bm_stride, vaddr + n1, ncols - n1, nrows);
}

/* вывод всей видимой части впереди (processActors: актёры — в холст до него) */
uint8_t gfx_view_pending(void)
{
    return view_dirty == 1 && currentRoom != 0;
}

/* окно, которое ближайший вывод вида пропустит: колонки [c0, c0 + nc) вида,
 * строки комнаты [t, t + n) — там холст с актёрами (processActors) */
static uint16_t ex_c0, ex_c1, ex_t, ex_b;
void gfx_view_excl(uint16_t c0, uint16_t nc, uint16_t t, uint16_t n)
{
    ex_c0 = c0;
    ex_c1 = c0 + nc;
    ex_t = t;
    ex_b = t + n;
}

/* Свет комнаты (ScummVM isLightOn, drawFlashlight): без света фон не выводится —
 * чёрный; с фонариком — его прямоугольник xStrips x yStrips клеток вокруг курсора
 * (у MM — у мыши, не у героя), углы скруглены. Края — по байтам БК (4 точки) */
static int16_t fl_x0, fl_x1, fl_y0, fl_y1;   /* точки экрана MM [x0, x1), строки комнаты [y0, y1) */
static const uint8_t fl_cd[8] = { 6, 5, 3, 2, 2, 2, 1, 1 };   /* углы: { 8, 6, 4, 3, 2, 2, 1, 1 } точек MM -> БК */

/* шаг цикла (scummLoop, перед выводом фона), когда темно или было темно */
void gfx_lights(void)
{
    uint8_t l = (uint8_t)VAR(VAR_CURRENT_LIGHTS);
    uint8_t mode = (l & 2) ? 0 : ((l & 4) && flashlight_xStrips && flashlight_yStrips) ? 2 : 1;
    int16_t x0 = 0, y0 = 0, w = 0, h = 0;
    if (mode == 2)
    {
        w = (int16_t)(flashlight_xStrips * 8);
        h = (int16_t)(flashlight_yStrips * 8);
        x0 = mouse_x - w / 2;
        y0 = mouse_y - 16 - h / 2;
        if (x0 < 0) x0 = 0;
        else if (x0 + w > 320) x0 = (int16_t)(320 - w);
        if (y0 < 0) y0 = 0;
        else if (y0 + h > BM_ROWS) y0 = (int16_t)(BM_ROWS - h);
    }
    if (mode != lt_mode)
        view_dirty = 1;   /* весь вид заново (и актёры) */
    else if (mode == 2 && (x0 != fl_x0 || y0 != fl_y0))
    {
        int16_t a = x0 < fl_x0 ? x0 : fl_x0, b = x0 + w > fl_x1 ? x0 + w : fl_x1, s = screenStartStrip * 8;
        view_mark((uint8_t)((s + a) / 8), (uint8_t)((s + b + 7) / 8));   /* прежнее место — в темноту, новое — светлым */
    }
    lt_mode = mode;
    fl_x0 = x0;
    fl_x1 = x0 + w;
    fl_y0 = y0;
    fl_y1 = y0 + h;
}

/* колонки вида фонарика [l0, l1) (bcol — колонка фона левого края вида) */
void fl_cols(uint16_t bcol, uint16_t *l0, uint16_t *l1)
{
    uint16_t px = PHYS_X((uint16_t)(screenStartStrip * 8));
    *l0 = (BK_X(px + fl_x0) >> 2) - bcol;
    *l1 = (BK_X(px + fl_x1 - 1) >> 2) - bcol + 1;
}

/* пересечение колонок вида [c0, c1) и строк [y0, y1) с фонариком; 0 — не задевает */
uint8_t fl_clip(uint16_t bcol, uint16_t *c0, uint16_t *c1, uint16_t *y0, uint16_t *y1)
{
    uint16_t l0, l1;
    fl_cols(bcol, &l0, &l1);
    if (*c0 < l0) *c0 = l0;
    if (*c1 > l1) *c1 = l1;
    if (*y0 < (uint16_t)fl_y0) *y0 = (uint16_t)fl_y0;
    if (*y1 > (uint16_t)fl_y1) *y1 = (uint16_t)fl_y1;
    return *c0 < *c1 && *y0 < *y1;
}

/* скруглённый угол в строке комнаты y: сколько точек БК у левого и правого края
 * фонарика — в темноту (0 — строка без угла) */
uint8_t fl_corner(uint16_t y)
{
    uint16_t k = y - (uint16_t)fl_y0, k2 = (uint16_t)fl_y1 - 1 - y;
    uint8_t d = k < 8 ? fl_cd[k] : 0;
    if (k2 < 8 && fl_cd[k2] > d) d = fl_cd[k2];
    return d;
}

/* фон строк комнаты [y, y + n), колонок вида [c0, c1) -> экран: с учётом света */
void bg_vram(uint16_t bcol, uint16_t y, uint16_t n, uint16_t c0, uint16_t c1)
{
    if (!lt_mode)
    {
        bm_blit_ring(bcol + c0, y, VRAM_ADDR + (ROW0 + 16 + y) * 64 + c0, c1 - c0, n);
        return;
    }
    volatile uint8_t *v = vram + (ROW0 + 16 + y) * 64;
    for (uint16_t r = 0; r < n; r++, v += 64)
    {
        for (uint16_t c = c0; c < c1; c++) v[c] = 0;
        if (!(r & 7)) SND_POLL();
    }
    uint16_t a = c0, b = c1, ya = y, yb = y + n;
    if (lt_mode != 2 || !fl_clip(bcol, &a, &b, &ya, &yb)) return;
    bm_blit_ring(bcol + a, ya, VRAM_ADDR + (ROW0 + 16 + ya) * 64 + a, b - a, yb - ya);
    uint16_t l0, l1;
    fl_cols(bcol, &l0, &l1);
    for (uint16_t ry = ya; ry < yb; ry++)
    {
        uint8_t d = fl_corner(ry);
        if (!d) continue;
        volatile uint8_t *p = vram + (ROW0 + 16 + ry) * 64;
        for (uint8_t k = 0; k < d; k++)
        {
            uint16_t bl = l0 * 4 + k, br = l1 * 4 - 1 - k;
            if ((bl >> 2) >= a && (bl >> 2) < b) p[bl >> 2] &= (uint8_t)~(3 << ((bl & 3) * 2));
            if ((br >> 2) >= a && (br >> 2) < b) p[br >> 2] &= (uint8_t)~(3 << ((br & 3) * 2));
        }
    }
}

/* строки комнаты [y0, y1), колонки вида [c0, c1) -> экран кусками (тон между ними) */
static void view_rows(uint16_t bcol, uint16_t y0, uint16_t y1, uint16_t c0, uint16_t c1)
{
    if (c0 >= c1) return;
    for (uint16_t y = y0; y < y1; y += 32)
    {
        bg_vram(bcol, y, y1 - y < 32 ? y1 - y : 32, c0, c1);
        SND_POLL();
    }
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
    cursor_hide_rect(0, 16, 320, 16 + (int16_t)roomHeight);
    if (ex_c1 > ex_c0)   /* в обход окна холста */
    {
        view_rows(bcol, 0, ex_t, c0, c1);
        view_rows(bcol, ex_t, ex_b, c0, ex_c0 < c1 ? ex_c0 : c1);
        view_rows(bcol, ex_t, ex_b, ex_c1 > c0 ? ex_c1 : c0, c1);
        view_rows(bcol, ex_b, roomHeight, c0, c1);
        ex_c1 = 0;
    }
    else
        view_rows(bcol, 0, roomHeight, c0, c1);
    return whole ? 1 : 2;
}


static uint16_t bk_x(int16_t x)
{
    return BK_X(x);
}

/* маски точки k (0..3) в байте БК */
static const uint8_t px_mask[4] = { 03, 014, 060, 0300 };

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
    /* по строкам, в строке слева направо: точки одного байта — подряд (cursor_bytes) */
    for (uint8_t i = 1; i < cur_n; i++)
        for (uint8_t j = i; j && cur_dy[j - 1] * 32 + cur_dx[j - 1] > cur_dy[j] * 32 + cur_dx[j]; j--)
        {
            int8_t t = cur_dx[j];
            cur_dx[j] = cur_dx[j - 1];
            cur_dx[j - 1] = t;
            t = cur_dy[j];
            cur_dy[j] = cur_dy[j - 1];
            cur_dy[j - 1] = t;
        }
}

/* Курсор прячется лениво — только перед выводом на экран (функции вывода зовут
 * cursor_hide) и рисуется снова в конце шага цикла: виден всё время, пока игра
 * считает, а не лишь в простое между шагами */
static uint8_t cur_c;   /* цвет нарисованного; нарисован ли и где — cur_vis, cur_x, cur_y (ядро) */

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

/* убрать курсор, если вывод в прямоугольник экрана MM [x0, x1) x [y0, y1) его
 * задевает (курсор — до 11 точек от центра): иначе он мигал бы при любом выводе */
void cursor_hide_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    if (cur_vis && cur_x + 12 > x0 && cur_x - 12 < x1 && cur_y + 11 > y0 && cur_y - 11 < y1) cursor_hide();
}

#ifndef HOST
extern struct { int16_t l, t, r, b; } drawn[];   /* sys.ld: прямоугольники кадров актёров (costume_r.c) */
#endif

/* перед рисованием актёров L[0..nl) с R[i] != 0: курсор — прочь, если он рядом с
 * их прежними кадрами (drawn, базовое ОЗУ; y — от верха комнаты, она с y = 16);
 * запас — на шаг и смену кадра; актёр без прежнего кадра — где угодно */
void cursor_hide_actors(const uint8_t *L, uint8_t nl, const uint8_t *R)
{
#ifndef HOST
    int16_t l = 0x7FFF, t = 0x7FFF, r = -0x7FFF, b = -0x7FFF;
    for (uint8_t p = 0; p < nl; p++)
    {
        uint8_t i = L[p];
        if (!R[i]) continue;
        if (drawn[i].r <= drawn[i].l)
        {
            cursor_hide();
            return;
        }
        if (drawn[i].l < l) l = drawn[i].l;
        if (drawn[i].r > r) r = drawn[i].r;
        if (drawn[i].t < t) t = drawn[i].t;
        if (drawn[i].b > b) b = drawn[i].b;
    }
    cursor_hide_rect(l - 16, t, r + 16, b + 32);
#else
    (void)L, (void)nl, (void)R;
    cursor_hide();
#endif
}

/* холст одинокого актёра: окно [l, r) x [t, b) (y — от верха комнаты) задевает
 * прежние кадры других актёров — 1: экран читать в холст; 0 — в окне лишь фон и
 * свой прежний кадр, курсор из окна убран */
uint8_t cv_need_load(int16_t l, int16_t t, int16_t r, int16_t b)
{
#ifndef HOST
    for (uint8_t i = 1; i < NUM_ACTORS; i++)
        if (drawn[i].r > drawn[i].l && drawn[i].l < r && l < drawn[i].r && drawn[i].t < b && t < drawn[i].b) return 1;
    cursor_hide_rect(l, t + 16, r, b + 16);
    return 0;
#else
    (void)l, (void)t, (void)r, (void)b;
    return 1;
#endif
}

/* байты экрана курсора с центром (x, y): точки одного байта идут подряд
 * (cursor_shape) — сверка только с прошлым байтом */
static void cursor_bytes_at(int16_t x0, int16_t y0)
{
    cb_n = 0;
    for (uint8_t i = 0; i < cur_n; i++)
    {
        if (!(i & 7)) SND_POLL();
        int16_t x = x0 + cur_dx[i], y = y0 + cur_dy[i];
        if (x < 0 || x >= 320 || y < 0 || y >= 200) continue;
        uint16_t bx = bk_x(x);
        if (bx >= 256) continue;   /* x 319 -> точка 256: байт 64 — уже следующая строка слева */
        uint16_t a = (uint16_t)((ROW0 + y) * 64 + (bx >> 2));
        uint8_t m = px_mask[bx & 3];
        if (cb_n && cb_addr[cb_n - 1] == a)
            cb_mask[cb_n - 1] |= m;
        else
        {
            cb_addr[cb_n] = a;
            cb_mask[cb_n++] = m;
        }
    }
}

/* Курсор целиком на экране: байты — шаблоном. Сжатие 4:5 делает их места
 * относительно байта центра зависящими лишь от x mod 5 — пять шаблонов
 * {смещение, маска}, строятся при первом случае (точками ~3000 команд на сдвиг) */
#define CT_N 40
static int16_t ct_off[5][CT_N];
static uint8_t ct_mask[5][CT_N], ct_n[5];

static void cursor_bytes(void)
{
    int16_t x = mouse_x, y = mouse_y;
    if (x < 12 || x > 307 || y < 11 || y > 188)
    {
        cursor_bytes_at(x, y);
        return;
    }
    uint8_t q = DIV5(x), r = (uint8_t)(x - (q << 2) - q);
    if (!ct_n[r])
    {
        cursor_bytes_at((int16_t)(100 + r), 100);
        uint16_t b0 = (uint16_t)((ROW0 + 100) * 64 + (bk_x((int16_t)(100 + r)) >> 2));
        for (uint8_t j = 0; j < cb_n && j < CT_N; j++)
        {
            ct_off[r][j] = (int16_t)(cb_addr[j] - b0);
            ct_mask[r][j] = cb_mask[j];
        }
        ct_n[r] = cb_n < CT_N ? cb_n : CT_N;
    }
    uint16_t b = (uint16_t)((ROW0 + y) * 64 + (bk_x(x) >> 2));
    const int16_t *o = ct_off[r];
    const uint8_t *m = ct_mask[r];
    cb_n = ct_n[r];
    for (uint8_t j = 0; j < cb_n; j++)
    {
        cb_addr[j] = b + (uint16_t)o[j];
        cb_mask[j] = m[j];
    }
}

/* нарисовать курсор в (mouse_x, mouse_y); цвет мигает как animateCursor */
void cursor_show(void)
{
    static const uint8_t colors[4] = { 2, 2, 2, 1 };   /* default_v1_cursor_colors 1, 1, 12, 11 — цвета БК (text.c text_pal) */
    if (!cur_n) cursor_shape();
    cur_anim++;
    if (cursorState <= 0)
    {
        cursor_hide();
        return;
    }
    uint8_t c = colors[(cur_anim >> 1) & 3];
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
