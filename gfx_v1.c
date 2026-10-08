/* Графика SCUMM v1 (MM DOS 1988) в формат БК: gfx.cpp GdiV1 (roomChanged,
 * drawStripV1Background / drawStripV1Object / drawStripV1Mask, decodeV1Gfx).
 *
 * Фон v1 — как на C64: таблица символов 8x8 (charMap, 256 символов), карта
 * символов комнаты (picMap), цвет клетки (colorMap, младшие 3 бита) и маска
 * (maskMap -> maskChar). Карты комнаты — по колонкам (y + полоса * высота).
 * Точка символа — 2 бита, «толстая» (2 точки мира): цвета 0..2 — общие на
 * комнату, 3 — цвет клетки. Объект — свои карты по строкам (символ, цвет, маска).
 *
 * Сжатие 4:5 и узор 2x2 по (bx&1, y&1) — как у v2: колонка мира x%5==4
 * отбрасывается, bx = x - x/5. Картинка — в PG_BITMAP (строки по bm_stride
 * байт), маска — в PG_MASK (строки по bm_strips байт). Таблицы символов фона и
 * маски — в странице PG_SWAP (V1_CHR, V1_MCH) на время комнаты. */
#ifdef HOST
#include <stdio.h>
#include <stdlib.h>
#endif
#include "scumm.h"
#include "gfx.h"
void c64_cache_clear(void);   /* costume_r.c */

#define V1_SMAX 110       /* окно фона: 880 точек, 176 x 136 байт — в странице, маска 110 x 136 — в PG_MASK */

/* Палитра v1 (palette.cpp tableV1Palette) -> 2x2 цветов БК [TL, TR, BL, BR]:
 * 0 чёрный, 1 синий, 2 зелёный, 3 красный — как соответствующие цвета EGA в v2 */
static const uint8_t v1_pat[16][4] =
{
    {0, 0, 0, 0}, {3, 2, 2, 2}, {3, 3, 3, 3}, {1, 2, 2, 1},   /* чёрный, белый, красный, голубой */
    {3, 1, 1, 3}, {2, 2, 2, 2}, {1, 1, 1, 1}, {3, 2, 2, 3},   /* пурпурный, зелёный, синий, жёлтый */
    {3, 3, 3, 3}, {3, 0, 0, 3}, {3, 3, 3, 3}, {1, 0, 0, 1},   /* св.красный, коричневый, св.красный, т.серый */
    {1, 1, 2, 2}, {2, 2, 2, 2}, {2, 1, 1, 1}, {1, 0, 0, 1},   /* серый, св.зелёный, св.синий, т.серый */
};

extern uint8_t band[1024];   /* sys.ld (дальше — боксы, _boxes); на хосте — host_gfx.c */
static uint8_t room_col[4];   /* GdiV1: _V1.colors[0..2] — общие цвета комнаты */
static uint8_t gp_t_ok;
static uint8_t clip_s0, clip_s1 = 0xFF;   /* объекты — только в полосах мира [clip_s0, clip_s1) */          /* gp_t построена для room_col и палитры */

/* v2_pat[y&1][k][цвет]: биты точки k (0..3) байта БК; чётность bx = k&1 */
void gfx_set_pal(void)
{
    for (uint8_t yp = 0; yp < 2; ++yp)
        for (uint8_t k = 0; k < 4; ++k)
            for (uint8_t c = 0; c < 16; ++c)
                v2_pat[yp][k][c] = (uint8_t)(v1_pat[roomPalette[c] & 15][(yp << 1) | (k & 1)] << (k * 2));
    gp_t_ok = 0;
}

void gfx_init_pal(void)
{
    for (uint8_t c = 0; c < 16; ++c) roomPalette[c] = c;
    gfx_set_pal();
}

/* gfx.cpp: GdiV1::decodeV1Gfx как поток: 4 «общих» байта, затем серии —
 * 1ccnnnnn (общий цвет cc, n+1 раз), 01nnnnnn b (b, n+1 раз), 00nnnnnn (n+1 байт) */
struct rle { uint8_t page; uint16_t p; uint8_t common[4]; uint8_t n, rep, val; };

static void rle_init(struct rle *r, uint8_t page, uint16_t p)
{
    r->page = page;
    far_rd(r->common, page, p, 4);
    r->p = p + 4;
    r->n = 0;
}

static uint8_t rle_next(struct rle *r)
{
    if (!r->n)
    {
        uint8_t run = far_rb(r->page, r->p++);
        if (run & 0x80)
        {
            r->rep = 1;
            r->val = r->common[(run >> 5) & 3];
            r->n = (uint8_t)((run & 0x1F) + 1);
        }
        else if (run & 0x40)
        {
            r->rep = 1;
            r->val = far_rb(r->page, r->p++);
            r->n = (uint8_t)((run & 0x3F) + 1);
        }
        else
        {
            r->rep = 0;
            r->n = (uint8_t)(run + 1);
        }
    }
    r->n--;
    return r->rep ? r->val : far_rb(r->page, r->p++);
}

/* поток в PG_SWAP (dst) кусками через _band */
static void rle_far(uint8_t page, uint16_t p, uint16_t dst, uint16_t n)
{
    struct rle r;
    rle_init(&r, page, p);
    while (n)
    {
        uint16_t k = n < sizeof band ? n : sizeof band;
        for (uint16_t i = 0; i < k; i++) band[i] = rle_next(&r);
        far_wr(PG_SWAP, dst, band, k);
        dst += k;
        n -= k;
    }
}

/* весь поток в буфер базового ОЗУ (карты объекта, таблицы символов) */
static void rle_to(uint8_t page, uint16_t p, uint8_t *dst, uint16_t n)
{
    struct rle r;
    rle_init(&r, page, p);
    while (n--) *dst++ = rle_next(&r);
}

/* Группа из 5 полос (40 точек мира) — 32 точки БК, 8 байт строки фона. Точка p
 * группы — толстая точка полосы gp_k[p] со сдвигом gp_sh[p] в байте символа.
 * Строка группы собирается таблицами: gp_e[e][b] — 2-битные индексы цвета точек
 * байта символа b полосы в байте строки (11 пар «полоса, байт»: e_k, e_i),
 * gp_t[чётность][I] — байт БК индексов I в общих цветах комнаты (индекс 3 — 0),
 * gp_m3[I] — точки с индексом 3 (цвет клетки). Сдвиги и умножения на ВМ1
 * программные — в цикле строк их нет. */
static const uint8_t e_k[11] = { 0, 0, 1, 1, 1, 2, 2, 3, 3, 4, 4 };
static const uint8_t e_i[11] = { 0, 1, 1, 2, 3, 3, 4, 5, 6, 6, 7 };
uint8_t gp_e[11][256], gp_t[2][256], gp_m3[256];   /* и v1rows.s */
static uint8_t gp_smask[5][8], gp_ok;

static void gp_init(void)
{
    uint8_t p = 0, gk[32], gs[32];
    for (uint8_t wx = 0; wx < 40; wx++)
    {
        if (wx % 5 == 4) continue;
        gk[p] = (uint8_t)(wx >> 3);
        gs[p] = (uint8_t)(6 - (wx & 6));
        gp_smask[gk[p]][p >> 2] |= (uint8_t)(3 << ((p & 3) * 2));
        p++;
    }
    for (uint8_t e = 0; e < 11; e++)
        for (uint16_t b = 0; b < 256; b++)
        {
            uint8_t v = 0;
            for (p = (uint8_t)(e_i[e] * 4); p < e_i[e] * 4 + 4; p++)
                if (gk[p] == e_k[e]) v |= (uint8_t)(((b >> gs[p]) & 3) << ((p & 3) * 2));
            gp_e[e][b] = v;
        }
    for (uint16_t i = 0; i < 256; i++)
    {
        uint8_t m = 0;
        for (uint8_t q = 0; q < 4; q++)
            if (((i >> (q * 2)) & 3) == 3) m |= (uint8_t)(3 << (q * 2));
        gp_m3[i] = m;
    }
    gp_ok = 1;
}

/* gp_t по room_col и v2_pat (после смены комнаты или палитры) */
static void gp_t_init(void)
{
    for (uint8_t yp = 0; yp < 2; yp++)
        for (uint16_t i = 0; i < 256; i++)
        {
            uint8_t v = 0;
            for (uint8_t q = 0; q < 4; q++)
            {
                uint8_t x = (uint8_t)((i >> (q * 2)) & 3);
                if (x != 3) v |= v2_pat[yp][q][room_col[x] & 15];
            }
            gp_t[yp][i] = v;
        }
    gp_t_ok = 1;
}

/* клетки группы: символ, цвет клетки, символ маски по полосе k и строке клеток */
#define CY_MAX 17
struct cells { uint8_t chr[5][CY_MAX], c3[5][CY_MAX], m[5][CY_MAX]; };

/* полосы [k0, k1) группы g (от начала окна), строки клеток [0, ncy) с точки y0;
 * фон и маска (maskChar инвертирован: в v1 1 — «не закрыто») */
void v1_rows(uint8_t lines[8][8], const uint8_t rows[5][8], const uint8_t ncb[2][8], uint16_t par);   /* v1rows.s */
static void render_group(uint8_t g, uint8_t k0, uint8_t k1, uint8_t y0, uint8_t ncy, const struct cells *c)
{
    /* строка клеток собирается в базовом ОЗУ и пишется в страницы одним far_wr2d */
    uint8_t rows[5][8], mrows[5][8], c3b[2][8], ncb[2][8], lines[8][8], old[8][8], vm[8], mlines[8][5];
    uint16_t col = (uint16_t)g * 8;
    if (col >= bm_stride) return;
    if (!gp_t_ok) gp_t_init();
    uint8_t nb = bm_stride - col < 8 ? (uint8_t)(bm_stride - col) : 8;
    uint8_t full = k0 == 0 && k1 == 5;
    for (uint8_t i = 0; i < 8; i++)
    {
        vm[i] = 0;
        for (uint8_t k = k0; k < k1; k++) vm[i] |= gp_smask[k][i];
    }
    for (uint8_t k = 0; k < 5; k++)
        for (uint8_t r = 0; r < 8; r++) rows[k][r] = 0;
    uint16_t a = PAGE_ADDR + (uint16_t)y0 * bm_stride + col;
    uint16_t ma = PAGE_ADDR + (uint16_t)y0 * bm_strips + (uint16_t)g * 5 + k0;
    uint16_t a8 = bm_stride << 3, ma8 = (uint16_t)bm_strips << 3;
    for (uint8_t cy = 0; cy < ncy; cy++)
    {
        for (uint8_t i = 0; i < 8; i++) c3b[0][i] = c3b[1][i] = 0;
        for (uint8_t k = k0; k < k1; k++)
        {
            far_rd(rows[k], PG_SWAP, V1_CHR + (uint16_t)c->chr[k][cy] * 8, 8);
            far_rd(mrows[k], PG_SWAP, V1_MCH + (uint16_t)c->m[k][cy] * 8, 8);
            uint8_t e = c->c3[k][cy];
            for (uint8_t yp = 0; yp < 2; yp++)
            {
                uint8_t f = v2_pat[yp][0][e] | v2_pat[yp][1][e] | v2_pat[yp][2][e] | v2_pat[yp][3][e];
                for (uint8_t i = 0; i < 8; i++) c3b[yp][i] |= gp_smask[k][i] & f;
            }
            for (uint8_t r = 0; r < 8; r++) mlines[r][k] = (uint8_t)(mrows[k][r] ^ 0xFF);
        }
        for (uint8_t i = 0; i < 8; i++)
        {
            ncb[0][i] = (uint8_t)~c3b[0][i];
            ncb[1][i] = (uint8_t)~c3b[1][i];
        }
        if (!full) far_rd2d(PG_BITMAP, a, bm_stride, old, 8, nb, 8);
#ifdef HOST   /* на БК — v1rows.s */
        for (uint8_t r = 0; r < 8; r++)
        {
            const uint8_t *t = gp_t[(y0 + r) & 1], *nc = ncb[(y0 + r) & 1];
            uint8_t *line = lines[r];
            uint8_t b0 = rows[0][r], b1 = rows[1][r], b2 = rows[2][r], b3 = rows[3][r], b4 = rows[4][r], x;
            x = gp_e[0][b0];               line[0] = t[x] | (gp_m3[x] & ~nc[0]);
            x = gp_e[1][b0] | gp_e[2][b1]; line[1] = t[x] | (gp_m3[x] & ~nc[1]);
            x = gp_e[3][b1];               line[2] = t[x] | (gp_m3[x] & ~nc[2]);
            x = gp_e[4][b1] | gp_e[5][b2]; line[3] = t[x] | (gp_m3[x] & ~nc[3]);
            x = gp_e[6][b2];               line[4] = t[x] | (gp_m3[x] & ~nc[4]);
            x = gp_e[7][b3];               line[5] = t[x] | (gp_m3[x] & ~nc[5]);
            x = gp_e[8][b3] | gp_e[9][b4]; line[6] = t[x] | (gp_m3[x] & ~nc[6]);
            x = gp_e[10][b4];              line[7] = t[x] | (gp_m3[x] & ~nc[7]);
        }
#else
        v1_rows(lines, rows, ncb, y0);
#endif
        if (!full)
            for (uint8_t r = 0; r < 8; r++)
                for (uint8_t i = 0; i < nb; i++) lines[r][i] = (uint8_t)((lines[r][i] & vm[i]) | (old[r][i] & ~vm[i]));
        far_wr2d(PG_BITMAP, a, bm_stride, lines, 8, nb, 8);
        far_wr2d(PG_MASK, ma, bm_strips, &mlines[0][k0], 5, (uint16_t)(k1 - k0), 8);
        a += a8;
        ma += ma8;
        y0 += 8;
        SND_POLL();
    }
}

/* полосы мира фона и маски [s0, s1) (внутри окна) из карт комнаты (redrawBGStrip):
 * карты — по колонкам, полосы собираются по группам */
static void room_strips(uint8_t rpage, uint16_t room, uint8_t s0, uint8_t s1)
{
    uint8_t hc = far_rb(rpage, room + 5);   /* высота в клетках */
    if (hc > CY_MAX) hc = CY_MAX;
    struct rle pic, col, msk;
    struct cells c;
    rle_init(&pic, rpage, room + far_rw(rpage, room + 12));
    rle_init(&col, rpage, room + far_rw(rpage, room + 14));
    rle_init(&msk, rpage, room + far_rw(rpage, room + 16));
    for (uint16_t i = (uint16_t)s0 * hc; i; i--)   /* пропуск полос до s0 */
    {
        rle_next(&pic);
        rle_next(&col);
        rle_next(&msk);
    }
    uint8_t s = s0;
    while (s < s1)
    {
        uint8_t g = (uint8_t)((s - win_s0) / 5);
        uint8_t k = (uint8_t)((s - win_s0) % 5), k0 = k;
        for (; k < 5 && s < s1; k++, s++)
        {
            for (uint8_t cy = 0; cy < hc; cy++)
            {
                c.chr[k][cy] = rle_next(&pic);
                c.c3[k][cy] = rle_next(&col) & 7;
                c.m[k][cy] = rle_next(&msk);
            }
        }
        render_group(g, k0, k, 0, hc, &c);
    }
}

void room_redraw_strips(uint8_t s0, uint8_t s1)
{
    if (s0 < win_s0) s0 = win_s0;
    if (s1 > win_s0 + bm_strips) s1 = (uint8_t)(win_s0 + bm_strips);
    if (s0 >= s1) return;
    room_strips(room_page, room_addr, s0, s1);
}

/* картинка объекта (GdiV1::prepareDrawBitmap + drawStripV1Object / Mask): карты
 * по строкам — символы, цвета, маска, по (w/8)*(h/8) байт каждая */
uint16_t v2_decode(uint8_t spage, uint16_t src, uint16_t x0, uint8_t y0, uint16_t w, uint8_t h)
{
    uint8_t wc = (uint8_t)(w >> 3), hc = (uint8_t)(h >> 3);
    uint16_t n = (uint16_t)wc * hc;
    if (n * 3 > sizeof band || hc > CY_MAX) return src;
    rle_to(spage, src, band, n * 3);
    struct cells c;
    uint8_t cx = 0;
    while (cx < wc)
    {
        int16_t s = (int16_t)(x0 >> 3) + cx - win_s0;   /* полоса от начала окна */
        if (s < 0)
        {
            cx++;
            continue;
        }
        if (s >= bm_strips) break;
        if (s + win_s0 < clip_s0 || s + win_s0 >= clip_s1)   /* вне отсечения — по полосе */
        {
            cx++;
            continue;
        }
        uint8_t g = (uint8_t)(s / 5), k = (uint8_t)(s % 5), k0 = k;
        for (; k < 5 && cx < wc && s < bm_strips && s + win_s0 < clip_s1; k++, cx++, s++)
        {
            for (uint8_t cy = 0; cy < hc; cy++)
            {
                uint16_t i = (uint16_t)cy * wc + cx;
                c.chr[k][cy] = band[i];
                c.c3[k][cy] = band[n + i] & 7;
                c.m[k][cy] = band[2 * n + i];
            }
        }
        render_group(g, k0, k, y0, hc, &c);
    }
    return src;
}

uint8_t win_fits(void)
{
    return screenStartStrip >= win_s0 && screenStartStrip + 40 <= win_s0 + bm_strips;
}

/* окно комнаты шириной ws полос: начало (кратно 5) вокруг видимой части; ws —
 * полос в окне */
static void win_pick(uint8_t *ws)
{
    win_s0 = 0;
    if (*ws > V1_SMAX)
    {
        int16_t s = screenStartStrip - (V1_SMAX - 40) / 2;
        int16_t smax = (int16_t)(*ws - V1_SMAX + 4) / 5 * 5;
        if (s > smax) s = smax;
        if (s < 0) s = 0;
        win_s0 = (uint8_t)(s / 5 * 5);
        *ws -= win_s0;
        if (*ws > V1_SMAX) *ws = V1_SMAX;
    }
}

/* Камера вышла из окна: общая со старым окном часть фона и маски сдвигается в
 * страницах (на целые группы по 8 байт), рисуются только новые полосы и объекты
 * на них (с отсечением по ним же: порядок наложения в старых полосах цел) */
uint8_t win_move(void)
{
#ifdef HOST
    if (getenv("NOWINMOVE")) return 0;   /* сверка с полной перерисовкой */
#endif
    uint8_t old = win_s0, ws = far_rb(room_page, room_addr + 4);
    win_pick(&ws);
    uint8_t ns = win_s0;
    win_s0 = old;
    if (ws != bm_strips || ns == old) return 0;
    int16_t d = (int16_t)ns - old;   /* полос, кратно 5 */
    uint8_t ad = (uint8_t)(d < 0 ? -d : d);
    if (ad >= bm_strips) return 0;
    uint16_t db = (uint16_t)(ad / 5) * 8, keepb = bm_stride - db, keepm = bm_strips - ad;
    uint16_t a = PAGE_ADDR, ma = PAGE_ADDR;
    uint8_t h = (uint8_t)(far_rb(room_page, room_addr + 5) * 8);
    for (uint8_t y = 0; y < h; y++, a += bm_stride, ma += bm_strips)
    {
        if (d > 0)
        {
            far_cp(PG_BITMAP, a, PG_BITMAP, a + db, keepb);
            far_cp(PG_MASK, ma, PG_MASK, ma + ad, keepm);
        }
        else
        {
            far_cp(PG_BITMAP, a + db, PG_BITMAP, a, keepb);
            far_cp(PG_MASK, ma + ad, PG_MASK, ma, keepm);
        }
        SND_POLL();
    }
    win_s0 = ns;
    uint8_t s0 = d > 0 ? (uint8_t)(ns + bm_strips - ad) : ns, s1 = (uint8_t)(s0 + ad);
    room_strips(room_page, room_addr, s0, s1);
    clip_s0 = s0;
    clip_s1 = s1;
    drawRoomObjectsRange(s0, s1);
    clip_s0 = 0;
    clip_s1 = 0xFF;
    return 1;
}

/* Фон комнаты (GdiV1::roomChanged + полная перерисовка): общие цвета, таблицы
 * символов фона и маски (в PG_SWAP), полосы окна. Комната шире V1_SMAX полос
 * хранится окном с началом, кратным 5 полосам (сжатие 4:5 не сдвигается),
 * вокруг видимой части; камера вышла из окна — перерисовка заново. */
void room_decode(uint8_t rpage, uint16_t room)
{
    uint8_t ws = far_rb(rpage, room + 4);
    c64_cache_clear();   /* костюмы могли переехать в куче */
#ifdef HOST
    { extern FILE *host_trace; if (host_trace) fprintf(host_trace, "ROOM_DECODE %u scr %d\n", currentRoom, screenStartStrip); }
#endif
    for (uint8_t i = 0; i < 3; i++) room_col[i] = far_rb(rpage, room + 6 + i);
    gp_t_ok = 0;
    win_pick(&ws);
    uint16_t w = (uint16_t)ws * 8;
    if (!gp_ok) gp_init();
    bm_stride = (uint16_t)((BK_W(w) + 3) >> 2);
    bm_strips = ws;
    /* таблицы символов: через _band (1 Кбайт) в PG_SWAP */
    rle_far(rpage, room + far_rw(rpage, room + 10), V1_CHR, 2048);
    uint16_t mp = room + far_rw(rpage, room + 18);
    uint16_t mn = far_rw(rpage, mp) - 8;   /* длина в данных на 8 больше (bug #1837375) */
    if (mn > 2048) mn = 2048;
    rle_far(rpage, mp + 2, V1_MCH, mn);
    if (w % 5) far_fill(PG_BITMAP, PAGE_ADDR, 0, (uint16_t)(bm_stride * far_rb(rpage, room + 5) * 8));
    room_strips(rpage, room, win_s0, (uint8_t)(win_s0 + bm_strips));
}
