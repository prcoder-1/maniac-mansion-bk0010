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

/* RING_G/RING_PX — gfx.h; групп в комнате не больше RING_GMAX (1600 точек) */
#define RING_GMAX 40

/* Палитра v1 (palette.cpp tableV1Palette) -> 2x2 цветов БК [TL, TR, BL, BR]:
 * 0 чёрный, 1 синий, 2 зелёный, 3 красный. Мелкие детали не должны тонуть в
 * узоре: самые частые цвета — сплошные, смеси — только шахматкой из двух цветов
 * (белый — с редкой красной точкой). Совпадают лишь цвета, почти не граничащие в
 * комнатах (по всем 52): коричневый — красный с чёрным (рядом со светло-красными
 * стенами), тёмно-серые — синий (рядом с коричневыми кирпичами) */
static const uint8_t v1_pat[16][4] =
{
    {0, 0, 0, 0}, {2, 2, 2, 3}, {3, 0, 0, 3}, {2, 1, 1, 2},   /* чёрный, белый, красный, голубой */
    {3, 1, 1, 3}, {2, 2, 2, 2}, {1, 0, 0, 1}, {3, 2, 2, 3},   /* пурпурный, зелёный, синий, жёлтый */
    {3, 3, 3, 3}, {3, 0, 0, 3}, {3, 3, 3, 3}, {1, 1, 1, 1},   /* св.красный, коричневый, св.красный, т.серый */
    {2, 1, 1, 2}, {2, 2, 2, 2}, {1, 1, 1, 1}, {1, 1, 1, 1},   /* серый, св.зелёный, св.синий, т.серый */
};

extern uint8_t band[1024];   /* sys.ld (дальше — боксы, _boxes); на хосте — host_gfx.c */
static uint8_t room_col[4];   /* GdiV1: _V1.colors[0..2] — общие цвета комнаты */
static uint8_t gp_t_ok;            /* gp_t построена для room_col и палитры */
static uint8_t clip_s0, clip_s1 = 0xFF;   /* объекты — только в полосах мира [clip_s0, clip_s1) */

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
        for (uint16_t i = 0; i < k; i++)
        {
            band[i] = rle_next(&r);
            if (!(i & 15)) SND_POLL();
        }
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
    while (n--)
    {
        *dst++ = rle_next(&r);
        if (!(n & 15)) SND_POLL();
    }
}

/* Группа из 5 полос (40 точек мира) — 32 точки БК, 8 байт строки фона.
 * Строка группы собирается таблицами: gp_e[e][b] — 2-битные индексы цвета точек
 * байта символа b полосы в байте строки (11 пар «полоса, байт»: e_k, e_i),
 * gp_t[чётность][I] — байт БК индексов I в общих цветах комнаты (индекс 3 — 0),
 * gp_m3[I] — точки с индексом 3 (цвет клетки), gp_c3t[k][c][чётность] — биты
 * полосы k в цвете клетки c. На БК таблицы и символы комнаты — в PG_SWAP, строку
 * клеток собирает v1cell.s в ОЗУ СМК: базовое ОЗУ БК медленное. */
static const uint8_t e_k[11] = { 0, 0, 1, 1, 1, 2, 2, 3, 3, 4, 4 };
static const uint8_t e_i[11] = { 0, 1, 1, 2, 3, 3, 4, 5, 6, 6, 7 };
static uint8_t gp_smask[5][8], gp_ok;
#ifdef HOST   /* на БК таблицы — в PG_SWAP (V1_GP*, v1cell.s) */
static uint8_t gp_e[11][256], gp_t[2][256], gp_m3[256], gp_c3t[5][8][2][8];
#endif

/* строка таблицы в PG_SWAP (через _band) или в массив хоста */
static void gp_put(uint16_t addr, uint8_t *host, const uint8_t *row, uint16_t n)
{
#ifdef HOST
    (void)addr;
    for (uint16_t i = 0; i < n; i++) host[i] = row[i];
#else
    (void)host;
    far_wr(PG_SWAP, addr, row, n);
#endif
}

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
    {
        for (uint16_t b = 0; b < 256; b++)
        {
            uint8_t v = 0;
            for (p = (uint8_t)(e_i[e] * 4); p < e_i[e] * 4 + 4; p++)
                if (gk[p] == e_k[e]) v |= (uint8_t)(((b >> gs[p]) & 3) << ((p & 3) * 2));
            band[b] = v;
        }
#ifdef HOST
        gp_put(0, gp_e[e], band, 256);
#else
        gp_put(V1_GPE + (uint16_t)e * 256, 0, band, 256);
#endif
        SND_POLL();
    }
    for (uint16_t i = 0; i < 256; i++)
    {
        uint8_t m = 0;
        for (uint8_t q = 0; q < 4; q++)
            if (((i >> (q * 2)) & 3) == 3) m |= (uint8_t)(3 << (q * 2));
        band[i] = m;
    }
#ifdef HOST
    gp_put(0, gp_m3, band, 256);
#else
    gp_put(V1_GPM3, 0, band, 256);
#endif
    gp_ok = 1;
}

/* по room_col и v2_pat (после смены комнаты или палитры): gp_t — байт БК индексов
 * в общих цветах комнаты, gp_c3t — биты полосы в цвете клетки */
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
            band[yp * 256 + i] = v;
            if (!(i & 3)) SND_POLL();
        }
#ifdef HOST
    gp_put(0, gp_t[0], band, 512);
#else
    gp_put(V1_GPT, 0, band, 512);
#endif
    uint8_t *o = band;
    for (uint8_t k = 0; k < 5; k++)
        for (uint8_t c = 0; c < 8; c++)
            for (uint8_t yp = 0; yp < 2; yp++)
            {
                uint8_t f = v2_pat[yp][0][c] | v2_pat[yp][1][c] | v2_pat[yp][2][c] | v2_pat[yp][3][c];
                for (uint8_t i = 0; i < 8; i++) *o++ = gp_smask[k][i] & f;
                SND_POLL();
            }
#ifdef HOST
    gp_put(0, &gp_c3t[0][0][0][0], band, 640);
#else
    gp_put(V1_C3T, 0, band, 640);
#endif
    gp_t_ok = 1;
}

/* клетки группы: символ, цвет клетки, символ маски по полосе k и строке клеток */
#define CY_MAX 17
/* строка клеток — 16 байт (шаг сдвигом): символы [0..4], цвета [5..9], маска [10..14] */
struct cells { uint8_t d[CY_MAX][16]; };
#define C_CHR 0
#define C_C3 5
#define C_M 10

/* параметры v1cell.s — в базовом ОЗУ (025000; строки — с 025200) */
struct v1p
{
    uint16_t nb, ba, bs, ma, ms, k0, k1, par, full;
    uint8_t vm[8], chr[5], c3[5], mchr[5], pad, nvm[8];
    uint16_t ncy, cells, ba8, ma8;   /* строк клеток, их записи (по 16 байт), шаги 8 строк */
    uint16_t lv;                     /* last действительна */
    uint8_t last[16];                /* клетки прошлой строки */
    uint16_t wfast;                  /* запись словами */
};
#ifndef HOST
_Static_assert(__builtin_offsetof(struct v1p, nvm) == 052 && __builtin_offsetof(struct v1p, c3) == 037 &&
               __builtin_offsetof(struct v1p, ma8) == 070 && __builtin_offsetof(struct v1p, wfast) == 0114,
               "struct v1p: смещения в v1cell.s");
#define V1P ((struct v1p *)025000)
void v1_cell(void);
#endif

/* полосы [k0, k1) группы g (от начала окна), строки клеток [0, ncy) с точки y0;
 * фон и маска (maskChar инвертирован: в v1 1 — «не закрыто») */
static void render_group(uint8_t g, uint8_t k0, uint8_t k1, uint8_t y0, uint8_t ncy, const struct cells *c)
{
    uint16_t col = (uint16_t)g * 8;
    if (col >= bm_stride) return;
    if (!gp_t_ok) gp_t_init();
    SND_POLL();
    uint8_t nb = bm_stride - col < 8 ? (uint8_t)(bm_stride - col) : 8;
    uint8_t full = k0 == 0 && k1 == 5, vm[8];
    for (uint8_t i = 0; i < 8; i++)
    {
        vm[i] = 0;
        for (uint8_t k = k0; k < k1; k++) vm[i] |= gp_smask[k][i];
    }
    uint16_t a = PAGE_ADDR + (uint16_t)y0 * bm_stride + col;
    uint16_t ma = PAGE_ADDR + (uint16_t)y0 * bm_strips + (uint16_t)g * 5 + k0;
    uint16_t a8 = bm_stride << 3, ma8 = (uint16_t)bm_strips << 3;
#ifndef HOST
    struct v1p *p = V1P;
    p->nb = nb;
    p->bs = bm_stride;
    p->ms = bm_strips;
    p->k0 = k0;
    p->k1 = k1;
    p->full = full;
    for (uint8_t i = 0; i < 8; i++)
    {
        p->vm[i] = vm[i];
        p->nvm[i] = (uint8_t)~vm[i];
    }
    p->ba = a;
    p->ma = ma;
    p->par = y0;
    p->ba8 = a8;
    p->ma8 = ma8;
    p->ncy = ncy;
    p->cells = (uint16_t)c->d;
    p->lv = 0;
    p->wfast = full && nb == 8 && !((a | bm_stride) & 1);
    SND_POLL();
    v1_cell();
#else
    uint8_t rows[5][8], mrows[5][8], c3b[2][8], lines[8][8], old[8][8], mlines[8][5];
    for (uint8_t k = 0; k < 5; k++)
        for (uint8_t r = 0; r < 8; r++) rows[k][r] = 0;
    for (uint8_t cy = 0; cy < ncy; cy++)
    {
        for (uint8_t i = 0; i < 8; i++) c3b[0][i] = c3b[1][i] = 0;
        for (uint8_t k = k0; k < k1; k++)
        {
            far_rd(rows[k], PG_SWAP, V1_CHR + (uint16_t)c->d[cy][C_CHR + k] * 8, 8);
            far_rd(mrows[k], PG_SWAP, V1_MCH + (uint16_t)c->d[cy][C_M + k] * 8, 8);
            const uint8_t *t3 = gp_c3t[k][c->d[cy][C_C3 + k]][0];
            uint8_t *cb3 = &c3b[0][0];
            for (uint8_t i = 0; i < 16; i++) cb3[i] |= t3[i];
            for (uint8_t r = 0; r < 8; r++) mlines[r][k] = (uint8_t)(mrows[k][r] ^ 0xFF);
        }
        if (!full) far_rd2d(PG_BITMAP, a, bm_stride, old, 8, nb, 8);
        for (uint8_t r = 0; r < 8; r++)
        {
            const uint8_t *t = gp_t[(y0 + r) & 1], *cb = c3b[(y0 + r) & 1];
            uint8_t *line = lines[r];
            uint8_t b0 = rows[0][r], b1 = rows[1][r], b2 = rows[2][r], b3 = rows[3][r], b4 = rows[4][r], x;
            x = gp_e[0][b0];               line[0] = t[x] | (gp_m3[x] & cb[0]);
            x = gp_e[1][b0] | gp_e[2][b1]; line[1] = t[x] | (gp_m3[x] & cb[1]);
            x = gp_e[3][b1];               line[2] = t[x] | (gp_m3[x] & cb[2]);
            x = gp_e[4][b1] | gp_e[5][b2]; line[3] = t[x] | (gp_m3[x] & cb[3]);
            x = gp_e[6][b2];               line[4] = t[x] | (gp_m3[x] & cb[4]);
            x = gp_e[7][b3];               line[5] = t[x] | (gp_m3[x] & cb[5]);
            x = gp_e[8][b3] | gp_e[9][b4]; line[6] = t[x] | (gp_m3[x] & cb[6]);
            x = gp_e[10][b4];              line[7] = t[x] | (gp_m3[x] & cb[7]);
            if (!full)
                for (uint8_t i = 0; i < nb; i++) line[i] = (uint8_t)((line[i] & vm[i]) | (old[r][i] & ~vm[i]));
        }
        far_wr2d(PG_BITMAP, a, bm_stride, lines, 8, nb, 8);
        far_wr2d(PG_MASK, ma, bm_strips, &mlines[0][k0], 5, (uint16_t)(k1 - k0), 8);
        a += a8;
        ma += ma8;
        y0 += 8;
    }
#endif
}

/* Кольцо групп. Группа мира G (полосы 5G..5G+4) всегда лежит в слоте G mod
 * RING_G (байты фона 8*слот, полосы маски 5*слот): точка мира x — в точке
 * кольца x mod RING_PX. Слот помнит свою группу; видимая часть дорисовывается
 * сразу (view_ensure), соседние группы — по одной за цикл (gfx_prefetch). Узкая
 * комната (до RING_G групп) — та же схема без переноса. */
static uint8_t slot_g[RING_G];       /* группа + 1, 0 — пусто */
static uint8_t room_ws, room_hc, room_ng;   /* полос, строк клеток, групп */
static uint8_t ring_room;            /* комната в кольце */

/* состояние распаковки карт (символы, цвета, маска) на границах групп: карты —
 * RLE по колонкам, без них группа справа требовала бы распаковки с начала */
struct rcp { uint16_t p; uint8_t n, rep, val, pad; };
static struct rcp cp[RING_GMAX + 1][3];
static uint8_t cp_n;                 /* действительны для групп [0, cp_n) */
static struct rle rl[3];             /* карты: общие байты — из rle_init */

static void cp_save(uint8_t g)
{
    for (uint8_t i = 0; i < 3; i++)
    {
        cp[g][i].p = rl[i].p;
        cp[g][i].n = rl[i].n;
        cp[g][i].rep = rl[i].rep;
        cp[g][i].val = rl[i].val;
    }
}

static void cp_load(uint8_t g)
{
    for (uint8_t i = 0; i < 3; i++)
    {
        rl[i].p = cp[g][i].p;
        rl[i].n = cp[g][i].n;
        rl[i].rep = cp[g][i].rep;
        rl[i].val = cp[g][i].val;
    }
}

/* клетки группы G (полосы k < kn) в c */
static void decode_group(uint8_t G, struct cells *c, uint8_t kn)
{
    uint8_t g = (uint8_t)(G < cp_n ? G : cp_n - 1);
    cp_load(g);
    for (; g < G; g++)   /* вперёд до G, по пути — точки на границах */
    {
        uint8_t m = (uint8_t)(room_ws - g * 5 < 5 ? room_ws - g * 5 : 5);
        for (uint16_t i = (uint16_t)m * room_hc; i; i--)
        {
            rle_next(&rl[0]);
            rle_next(&rl[1]);
            rle_next(&rl[2]);
            if (!(i & 3)) SND_POLL();   /* ВМ1: ~115 команд на 1 мс */
        }
        if (g + 1 == cp_n)
        {
            cp_save(g + 1);
            cp_n++;
        }
    }
    for (uint8_t k = 0; k < kn; k++)
        for (uint8_t cy = 0; cy < room_hc; cy++)
        {
            uint8_t *e = &c->d[cy][k];
            e[C_CHR] = rle_next(&rl[0]);
            e[C_C3] = rle_next(&rl[1]) & 7;
            e[C_M] = rle_next(&rl[2]);
            SND_POLL();
        }
    if (G + 1 == cp_n && G + 1 <= RING_GMAX)
    {
        cp_save(G + 1);
        cp_n++;
    }
}

static uint8_t g_slot(uint8_t G)
{
    return (uint8_t)(G >= RING_G ? G - RING_G : G);
}

static uint8_t g_present(uint8_t G)
{
    return slot_g[g_slot(G)] == G + 1;
}

/* полосы [k0, k1) группы G — фон и маска */
static void group_strips(uint8_t G, uint8_t k0, uint8_t k1)
{
    struct cells c;
    uint8_t kn = (uint8_t)(room_ws - G * 5 < 5 ? room_ws - G * 5 : 5);
    if (k1 > kn) k1 = kn;
    if (k0 >= k1) return;
    decode_group(G, &c, kn);
    render_group(g_slot(G), k0, k1, 0, room_hc, &c);
    SND_POLL();
}

/* объекты на группе G (отсечение по её полосам: порядок наложения в остальных
 * полосах не трогается); группа — в кольце */
static void group_done(uint8_t G)
{
    slot_g[g_slot(G)] = G + 1;
    clip_s0 = (uint8_t)(G * 5);
    clip_s1 = (uint8_t)(G * 5 + 5);
    drawRoomObjectsRange(clip_s0, clip_s1);
    clip_s0 = 0;
    clip_s1 = 0xFF;
}

/* подкачка по частям: клетки группы — в базовом ОЗУ (v1cell.s читает их при любой
 * подключённой странице), строки клеток — по PF_ROWS за вызов */
#ifdef HOST
static struct cells pf_cells;
#define PF_CELLS (&pf_cells)
#else
#define PF_CELLS ((struct cells *)026000)   /* sys.ld: _v2_strips — у v1 свободно */
#endif
#define PF_ROWS 4
static uint8_t pf_G = 0xFF, pf_cy, pf_kn;

/* строки клеток подкачиваемой группы [pf_cy, pf_cy + n); 1 — группа готова */
static uint8_t pf_step(uint8_t n)
{
    if (pf_cy + n > room_hc) n = (uint8_t)(room_hc - pf_cy);
    render_group(g_slot(pf_G), 0, pf_kn, (uint8_t)(pf_cy * 8), n, (const struct cells *)&PF_CELLS->d[pf_cy]);
    pf_cy += n;
    if (pf_cy < room_hc) return 0;
    uint8_t G = pf_G;
    pf_G = 0xFF;
    group_done(G);
    return 1;
}

/* группа G целиком в свой слот и объекты на ней */
static void group_load(uint8_t G)
{
    if (pf_G == G)   /* подкачивалась — дорисовать */
    {
        pf_step(room_hc);
        return;
    }
    slot_g[g_slot(G)] = 0;
    group_strips(G, 0, 5);
    group_done(G);
}

/* группы видимой части [g0, g1] */
static void view_groups(uint8_t *g0, uint8_t *g1)
{
    int16_t s = screenStartStrip;
    if (s < 0) s = 0;
    *g0 = div5[s];
    int16_t e = s + 39;
    if (e >= room_ws) e = room_ws - 1;
    *g1 = div5[e];
}

/* видимая часть — в кольце; 1 — что-то дорисовано */
uint8_t view_ensure(void)
{
    uint8_t g0, g1, r = 0;
    view_groups(&g0, &g1);
    for (uint8_t G = g0; G <= g1; G++)
        if (!g_present(G))
        {
            group_load(G);
            r = 1;
        }
    return r;
}

/* одна недостающая группа рядом с видимой частью (6 групп в каждую сторону:
 * вместе с видимой — не больше RING_G, видимые не вытесняются); 1 — была */
uint8_t gfx_prefetch(void)
{
    if (!ring_room || ring_room != currentRoom) return 0;
    if (pf_G != 0xFF) return pf_step(PF_ROWS), 1;
    uint8_t g0, g1, G = 0xFF;
    view_groups(&g0, &g1);
    for (uint8_t d = 1; d <= 6 && G == 0xFF; d++)
    {
        if (d == 3) SND_POLL();
        if (g1 + d < room_ng && !g_present((uint8_t)(g1 + d))) G = (uint8_t)(g1 + d);
        else if (g0 >= d && !g_present((uint8_t)(g0 - d))) G = (uint8_t)(g0 - d);
    }
    if (G == 0xFF) return 0;
    /* начать: слот — под G, клетки — в PF_CELLS, первые строки */
    slot_g[g_slot(G)] = 0;
    pf_kn = (uint8_t)(room_ws - G * 5 < 5 ? room_ws - G * 5 : 5);
    decode_group(G, PF_CELLS, pf_kn);
    pf_G = G;
    pf_cy = 0;
    pf_step(PF_ROWS);
    return 1;
}

/* полосы мира [s0, s1) — из карт комнаты (redrawBGStrip), только в кольце */
void room_redraw_strips(uint8_t s0, uint8_t s1)
{
    if (s1 > room_ws) s1 = room_ws;
    while (s0 < s1)
    {
        uint8_t G = div5[s0], k0 = (uint8_t)(s0 - G * 5), k1 = (uint8_t)(s1 - G * 5);
        if (k1 > 5) k1 = 5;
        if (g_present(G)) group_strips(G, k0, k1);
        s0 = (uint8_t)(G * 5 + 5);
    }
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
        uint8_t s = (uint8_t)((x0 >> 3) + cx);   /* полоса мира */
        uint8_t G = div5[s];
        if (s >= room_ws || s < clip_s0 || s >= clip_s1 || !g_present(G))   /* вне кольца, отсечения */
        {
            cx++;
            continue;
        }
        uint8_t k = (uint8_t)(s - G * 5), k0 = k;
        for (; k < 5 && cx < wc && s < clip_s1 && s < room_ws; k++, cx++, s++)
        {
            for (uint8_t cy = 0; cy < hc; cy++)
            {
                uint16_t i = (uint16_t)cy * wc + cx;
                uint8_t *e = &c.d[cy][k];
                e[C_CHR] = band[i];
                e[C_C3] = band[n + i] & 7;
                e[C_M] = band[2 * n + i];
            }
        }
        render_group(g_slot(G), k0, k, y0, hc, &c);
        SND_POLL();
    }
    return src;
}

/* Фон комнаты (GdiV1::roomChanged + полная перерисовка): общие цвета, таблицы
 * символов фона и маски (в PG_SWAP), кольцо пусто — видимая часть заново */
void room_decode(uint8_t rpage, uint16_t room)
{
    uint8_t ws = far_rb(rpage, room + 4);
    c64_cache_clear();   /* костюмы могли переехать в куче */
#ifdef HOST
    { extern FILE *host_trace; if (host_trace) fprintf(host_trace, "ROOM_DECODE %u scr %d\n", currentRoom, screenStartStrip); }
#endif
    for (uint8_t i = 0; i < 3; i++) room_col[i] = far_rb(rpage, room + 6 + i);
    gp_t_ok = 0;
    if (!gp_ok) gp_init();
    if (ws > RING_GMAX * 5) ws = RING_GMAX * 5;
    room_ws = ws;
    room_ng = div5[ws + 4];
    room_hc = far_rb(rpage, room + 5);
    if (room_hc > CY_MAX) room_hc = CY_MAX;
    bm_ring = room_ng > RING_G;
    uint16_t w = bm_ring ? RING_PX : (uint16_t)ws * 8;
    bm_stride = (uint16_t)((BK_W(w) + 3) >> 2);
    bm_strips = (uint8_t)(w >> 3);
    for (uint8_t i = 0; i < RING_G; i++) slot_g[i] = 0;
    ring_room = currentRoom;
    pf_G = 0xFF;
    /* таблицы символов: через _band (1 Кбайт) в PG_SWAP */
    rle_far(rpage, room + far_rw(rpage, room + 10), V1_CHR, 2048);
    uint16_t mp = room + far_rw(rpage, room + 18);
    uint16_t mn = far_rw(rpage, mp) - 8;   /* длина в данных на 8 больше (bug #1837375) */
    if (mn > 2048) mn = 2048;
    rle_far(rpage, mp + 2, V1_MCH, mn);
    for (uint8_t i = 0; i < 3; i++)   /* через стек: far_rd пишет только в базовое ОЗУ */
    {
        struct rle t;
        rle_init(&t, rpage, room + far_rw(rpage, room + 12 + i * 2));
        rl[i] = t;
    }
    cp_save(0);
    cp_n = 1;
    if (!bm_ring && (ws % 5)) far_fill(PG_BITMAP, PAGE_ADDR, 0, (uint16_t)(bm_stride * room_hc * 8));
    /* видимая часть: объекты — drawRoomObjects (redrawBGAreas) */
    uint8_t g0, g1;
    view_groups(&g0, &g1);
    for (uint8_t G = g0; G <= g1; G++)
    {
        group_strips(G, 0, 5);
        slot_g[g_slot(G)] = G + 1;
    }
}
