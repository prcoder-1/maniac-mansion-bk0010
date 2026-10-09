/* Отрисовка актёров (банк B): перевод ClassicCostumeRenderer (costume.cpp:
 * drawLimb / mainRoutine / proc3, base-costume.cpp: drawCostume /
 * codec1_ignorePakCols) и Actor_v2::prepareDrawActorCostume (actor.cpp) для
 * костюмов v2 (формат 0x58, GF_OLD_BUNDLE, без масштабирования).
 *
 * Координаты — точки главного виртуального экрана MM (320x136). Колонка экрана x
 * (мир x + xstart) с x_мира%5 == 4 пропускается, остальные встают в точку БК
 * bx_мира - view0 (view0 — начало видимой части, кратное 4). Кадр конечности
 * (RLE) копируется в _band; proc3 (proc3.s) читает z-маску прямо в её
 * странице и пишет в видеопамять. */
#ifdef HOST
#include <stdio.h>
#include <stdlib.h>
#endif
#include "actor.h"
#include "gfx.h"
#ifndef HOST
#undef SND_POLL
#define SND_POLL() snd_tick()   /* банк B полон: вызов короче проверки на месте */
#endif

#define VIEW_ROW 44                 /* строка БК начала комнаты: 28 + 16 */
#define OUT_W 320
#define OUT_H 136   /* v1: комната 136 строк */
#ifdef HOST
extern uint8_t host_vram[040000];
#define vram host_vram
extern uint8_t band[1024];
#else
#define vram ((volatile uint8_t *)040000)
extern uint8_t band[1024], bounce[256];   /* sys.ld */
#endif

/* цвет v1 -> 2x2 цветов БК (как фон, gfx_v1.c) */
static const uint8_t pat[16][4] =
{
    {0, 0, 0, 0}, {2, 2, 2, 3}, {3, 0, 0, 3}, {2, 1, 1, 2},
    {3, 1, 1, 3}, {2, 2, 2, 2}, {1, 0, 0, 1}, {3, 2, 2, 3},
    {3, 3, 3, 3}, {3, 0, 0, 3}, {3, 3, 3, 3}, {1, 1, 1, 1},
    {2, 1, 1, 2}, {2, 2, 2, 2}, {1, 1, 1, 1}, {1, 1, 1, 1},
};
/* costume.cpp: v1MMActorPalatte1/2 — цвета 1 и 2 кадра по номеру актёра */
static const uint8_t v1_pal1[25] = { 8, 8, 8, 8, 4, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8 };
static const uint8_t v1_pal2[25] = { 0, 7, 2, 6, 9, 1, 3, 7, 7, 1, 1, 9, 1, 4, 5, 5, 4, 1, 0, 5, 4, 2, 2, 7, 7 };
static const uint8_t pix_mask[4] = { 03, 014, 060, 0300 };
static const uint8_t pix_val[4][4] = { {0, 1, 2, 3}, {0, 4, 010, 014}, {0, 020, 040, 060}, {0, 0100, 0200, 0300} };
static const uint8_t revBitMask[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

/* состояние рендерера (ClassicCostumeRenderer) */
static int16_t actorX, actorY, xmove, ymove, width, height;
static uint8_t mirror, zbuf, cpal[16];
static uint8_t c_page;
static uint16_t c_res, c_frameOffsets, c_animCmds;
static uint8_t c_fmt;
static uint16_t srcptr;       /* адрес кадра в странице костюма */
static int16_t draw_top, draw_bottom, draw_left, draw_right;
static int16_t xstart;        /* _virtscr[kMainVirtScreen].xstart */
static uint16_t view0;        /* точка БК мира в левом крае экрана */

/* прямоугольник, занятый актёром в прошлом кадре (для восстановления фона) */
struct DrawnRect { int16_t l, t, r, b; };
#ifdef HOST
static struct DrawnRect drawn[NUM_ACTORS];
uint8_t strip_usage[40][4], meas_rec_buf[2][64];
#else
extern struct DrawnRect drawn[NUM_ACTORS];   /* sys.ld: базовое ОЗУ */
#endif

static uint16_t bk_x(int16_t x)
{
    return BK_X(x);
}

/* costume.cpp: ClassicCostumeLoader::loadCostume для рендерера (как в costume.c) */
static uint8_t loadCostume(uint8_t id)
{
    const struct CostInfo *ci = cost_info(id);
    if (!ci) return 0;
    c_page = ci->page;
    c_res = ci->res;
    c_fmt = ci->fmt;
    c_frameOffsets = ci->res + 4;
    c_animCmds = ci->animCmds;
    return 1;
}

/* цель отрисовки: видеопамять или буфер в PG_MASK (draw_via_canvas) */
struct Target { uint16_t base, stride, bxlim, ylim, view0, page; int16_t y0; };
static struct Target tg;
static void target_screen(void)
{
    tg.base = 040000u + VIEW_ROW * 64;
    tg.stride = 64;
    tg.bxlim = 256;
    tg.ylim = OUT_H;
    tg.view0 = view0;
    tg.page = 0;
    tg.y0 = 0;
}
static uint8_t measuring;   /* прогон конечностей без рисования: только draw_* */
static uint8_t cur_limb, *meas_rec;   /* измерение: место конечностей {t, b, l} по 4 байта */
static uint8_t cpat_ok;     /* P3.cpat построена для текущего актёра */

/* proc3 по колонкам (proc3.s, общая копия кода): параметры — в _bounce
 * (базовое ОЗУ: proc3 подключает страницу маски и читает её на месте) */
struct P3
{
    const uint8_t *src;     /* 0: RLE (в _band) */
    uint16_t len, color;    /* 2, 4: незаконченная серия (len 0 — нет) */
    int16_t vy;             /* 6: верх, точка MM */
    uint16_t h, skip;       /* 010, 012: высота, колонок */
    int16_t step, x;        /* 014, 016: шаг колонки (+-1), колонка MM */
    int16_t xstart;         /* 020 */
    uint16_t view0;         /* 022 */
    uint16_t zbuf;          /* 024 */
    uint16_t mrow0;         /* 026: адрес строки vy маски в PG_MASK */
    uint16_t mstride;       /* 030: bm_strips */
    uint8_t cpat[16][4];    /* 032: цвет -> [(y&1)<<1 | (bx&1)] цвет БК */
    uint16_t drow0;         /* 0132: байт цели строки vy (bx = 0) */
    uint16_t dstride;       /* 0134: шаг строки цели */
    uint16_t bxlim, ylim;   /* 0136, 0140: точек БК и строк цели */
    uint16_t dpage;         /* 0142: 0 — видеопамять, иначе страница буфера */
    uint16_t mring;         /* 0144: полос маски в кольце (полоса wx/8 — по модулю) */
};
#ifndef HOST
_Static_assert(__builtin_offsetof(struct P3, cpat) == 032 && __builtin_offsetof(struct P3, ylim) == 0140 &&
               __builtin_offsetof(struct P3, mring) == 0144,
               "struct P3: смещения в proc3.s");
#endif

#ifdef HOST
/* модель proc3.s */
static void proc3_run(struct P3 *p)
{
    int16_t y = p->vy, x = p->x;
    uint16_t h = p->h, skip = p->skip, len = p->len;
    uint8_t color = (uint8_t)p->color;
    const uint8_t *src = p->src;
    if (len) goto StartPos;
    for (;;)
    {
        len = *src++;
        color = (uint8_t)(len >> 4);
        len &= 15;
        if (!len)
        {
            len = *src++;
            if (!len) len = 256;
        }
        do
        {
            if (color && (uint16_t)x < OUT_W && (uint16_t)y < p->ylim)
            {
                int16_t wx = x + p->xstart;
                uint16_t bx = BK_X(wx) - p->view0;
                uint16_t ms = (uint16_t)(wx / 8);
                if (ms >= p->mring) ms -= p->mring;
                uint8_t masked = p->zbuf &&
                    (far_rb(PG_MASK, (uint16_t)(p->mrow0 + (y - p->vy) * p->mstride + ms)) & revBitMask[wx & 7]);
                if (wx - div5[wx] * 5 != 4 && bx < p->bxlim && !masked)
                {
                    uint8_t k = (uint8_t)(bx & 3);
                    uint8_t c = p->cpat[color][((y & 1) << 1) | (bx & 1)];
                    uint16_t a = (uint16_t)(p->drow0 + (y - p->vy) * (int16_t)p->dstride + (bx >> 2));
                    if (p->dpage)
                        far_wb(p->dpage, a, (uint8_t)((far_rb(p->dpage, a) & ~pix_mask[k]) | pix_val[k][c]));
                    else
                        vram[a - 040000] = (uint8_t)((vram[a - 040000] & ~pix_mask[k]) | pix_val[k][c]);
                }
            }
            y++;
            if (!--h)
            {
                if (!--skip) return;
                h = p->h;
                y = p->vy;
                x += p->step;
                if ((uint16_t)x >= OUT_W) return;
            }
        StartPos:;
        } while (--len);
    }
}
static struct P3 p3;
#define P3_BUF (&p3)
#else
void proc3_run(struct P3 *p);
#define P3_BUF ((struct P3 *)bounce)
#endif

/* proc3: v_x — колонка экрана MM, v_y — верх, skip_width — колонок;
 * len0/color0 — незаконченная серия после codec1_ignorePakCols (вход в StartPos) */
static void proc3(int16_t v_x, int16_t v_y, int16_t skip_width, int8_t step, const uint8_t *src,
                  uint8_t len0, uint8_t color0)
{
    struct P3 *p = P3_BUF;
    p->src = src;
    p->len = len0;
    p->color = color0;
    p->vy = v_y - tg.y0;
    p->h = (uint16_t)height;
    p->skip = (uint16_t)skip_width;
    p->step = step;
    p->x = v_x;
    p->xstart = xstart;
    p->view0 = tg.view0;
    int16_t dy = v_y - tg.y0;
    p->drow0 = (uint16_t)(tg.base + (tg.stride == 64 ? dy << 6 : dy * (int16_t)tg.stride));
    p->dstride = tg.stride;
    p->bxlim = tg.bxlim;
    p->ylim = tg.ylim;
    p->dpage = tg.page;
    p->zbuf = zbuf;
    p->mrow0 = zbuf ? (uint16_t)(PAGE_ADDR + v_y * (int16_t)bm_strips) : 0;
    p->mstride = bm_strips;
    p->mring = bm_ring ? bm_strips : 0xFFFF;
    /* цвета — на актёра (палитра и чётность цели те же); _bounce между
     * конечностями никто не трогает */
    if (!cpat_ok)
    {
        cpat_ok = 1;
        uint8_t sw = (uint8_t)(tg.y0 & 1);   /* y цели — от верха окна: чётность как на экране */
        for (uint8_t c = 0; c < 16; c++)
        {
            if (!(c & 7)) SND_POLL();
            uint8_t e = cpal[c] & 15, *o = p->cpat[c];
            const uint8_t *q = pat[e];
            if (sw)
            {
                o[0] = q[2];
                o[1] = q[3];
                o[2] = q[0];
                o[3] = q[1];
            }
            else
            {
                o[0] = q[0];
                o[1] = q[1];
                o[2] = q[2];
                o[3] = q[3];
            }
        }
    }
    SND_POLL();
    proc3_run(p);
}

/* costume.cpp: procC64. Кадр v1 — RLE (байт длины, бит 7 — повтор следующего
 * байта), по колонкам в 8 точек сверху вниз; байт — 4 «толстые» точки по 2 бита
 * (0 — прозрачно). Перекодируется в _band в RLE proc3 по колонкам в 1 точку
 * (цвет << 4 | длина) с отсечением по краям экрана и рисуется proc3. */
#define C64_HMAX 144
uint8_t c64_col[4][C64_HMAX];   /* индексы толстых точек колонки: [f][y] (и c64tr.s) */
#ifndef HOST
uint16_t c64_tr(uint8_t *out, const uint8_t *src, uint16_t h, uint16_t ncol, const uint8_t *src_end, const uint8_t *out_end,
                uint8_t (*col)[C64_HMAX]);
#endif

/* RLE proc3 одной колонки индексов a[0..h): серии цвет << 4 | длина (длина > 15 —
 * байт 0x?0 и длина отдельно); 0 — не влезло */
static uint8_t *c64_enc(uint8_t *o, const uint8_t *a, uint8_t h, const uint8_t *end)
{
    uint8_t y = 0;
    while (y < h)
    {
        uint8_t pc = a[y], n = 1;
        while (y + n < h && a[y + n] == pc) n++;
        if (o >= end) return 0;
        if (n < 16) *o++ = (uint8_t)(pc << 4 | n);
        else
        {
            *o++ = (uint8_t)(pc << 4);
            *o++ = n;
        }
        y += n;
    }
    return o;
}

/* кэш перекодированных кадров (C64_CACHE в C64_PAGE): кадр, видимый целиком,
 * перекодируется один раз; слот — по адресу кадра, странице и зеркалу */
#define C64_SLOTS 24
#define C64_SLOT_N 128
static uint16_t c64_tsrc[C64_SLOTS];
static uint8_t c64_tpg[C64_SLOTS], c64_tlen[C64_SLOTS];   /* tpg: страница | 0x40 есть | 0x80 зеркало */

#ifndef HOST
/* Кэш спрайтов (PG_SPR): конечность стоящего актёра без маски, видимая целиком,
 * хранится готовыми байтами БК — строки по 2w байт {точки, цвет}; вывод —
 * sprblit.s, две команды на байт вместо proc3 по точкам. Ключ — кадр, зеркало,
 * актёр (палитра), фаза сжатия 4:5 (x мира mod 20) и чётность строки. */
#define SPR_SIZE 24576u
static uint16_t spr_top;
static uint8_t spr_rr, spr_ok, spr_act;
void spr_blit(uint16_t src, uint16_t w, uint16_t h, uint16_t dst, uint16_t dstride, uint16_t dpage);
#endif

void c64_cache_clear(void)
{
    for (uint8_t i = 0; i < C64_SLOTS; i++) c64_tpg[i] = 0;
#ifndef HOST
    for (uint8_t i = 0; i < SPR_SLOTS; i++) spr[i].h = 0;
    spr_top = 0;
#endif
}

#ifndef HOST
/* кадр — из кэша спрайтов или (build, RLE proc3 в _band, n байт) построить его
 * там; 0 — не годится (велик, не помещается в цель) или нет в кэше без build */
static uint8_t spr_draw(int16_t vx, int16_t vy, uint16_t n, uint8_t build)
{
    int16_t xl = mirror ? vx : vx - (int16_t)((width >> 3) - 1) * 8;
    uint16_t wl = (uint16_t)(xl + xstart), wr = wl + (uint16_t)width - 1;
    if (wr >= 1280) return 0;
    uint16_t b0 = BK_X(wl) & ~3u;
    uint8_t w = (uint8_t)(((BK_X(wr) - b0) >> 2) + 1), h = (uint8_t)height;
    if (w > 16 || (uint16_t)w * h > 384 || (build && n > 256) || b0 < tg.view0) return 0;
    uint16_t bc = (b0 - tg.view0) >> 2;
    if (bc + w > (tg.bxlim >> 2)) return 0;
    uint8_t ph = (uint8_t)((wl - (div5[wl] >> 2) * 20) | ((vy & 1) << 5));
    uint8_t pgm = (uint8_t)(c_page | (mirror ? 0x80 : 0));
    struct SprSlot *e = 0;
    SND_POLL();
    for (uint8_t i = 0; i < SPR_SLOTS; i++)
        if (spr[i].h && spr[i].src == srcptr && spr[i].pgm == pgm && spr[i].act == spr_act && spr[i].ph == ph)
        {
            e = &spr[i];
            break;
        }
    if (!e && !build) return 0;
    if (!e)
    {
        /* построить: proc3 в SPR_SCR (PG_MASK) на фоне 0 и на фоне 0377 */
        uint16_t sz = (uint16_t)w * h, need = sz * 2;
        uint8_t *A = band + 256, *B = band + 640;
        struct Target keep = tg;
        tg.base = SPR_SCR;
        tg.stride = w;
        tg.bxlim = (uint16_t)w * 4;
        tg.ylim = h;
        tg.view0 = b0;
        tg.page = PG_MASK;
        tg.y0 = vy;
        for (uint8_t pass = 0; pass < 2; pass++)
        {
            far_fill(PG_MASK, SPR_SCR, pass ? 0377 : 0, sz);
            cpat_ok = 0;
            proc3(mirror ? vx : vx + 7, vy, width, mirror ? 1 : -1, band, 0, 0);
            far_rd(pass ? B : A, PG_MASK, SPR_SCR, sz);
        }
        tg = keep;
        cpat_ok = 0;
        SND_POLL();
        if (spr_top + need > SPR_SIZE) spr_top = 0;
        for (uint8_t i = 0; i < SPR_SLOTS; i++)   /* занятое новым — вон */
            if (spr[i].h && spr[i].off < spr_top + need && spr_top < spr[i].off + (uint16_t)spr[i].w * spr[i].h * 2)
                spr[i].h = 0;
        SND_POLL();
        e = &spr[spr_rr];
        if (++spr_rr == SPR_SLOTS) spr_rr = 0;
        e->src = srcptr;
        e->pgm = pgm;
        e->act = spr_act;
        e->ph = ph;
        e->w = w;
        e->h = h;
        e->off = spr_top;
        spr_top += need;
        uint8_t row[32];
        for (uint8_t r = 0, *a = A, *b = B; r < h; r++)
        {
            for (uint8_t i = 0; i < w; i++, a++, b++)
            {
                uint8_t m = (uint8_t)~(*a ^ *b);   /* фон 0 и 0377 совпали — точка спрайта */
                row[2 * i] = m;
                row[2 * i + 1] = *a & m;
            }
            far_wr(PG_SPR, PAGE_ADDR + e->off + (uint16_t)r * w * 2, row, (uint16_t)w * 2);   /* back: фронт тона */
        }
    }
    /* строки цели: [0, ylim) от tg.y0 */
    int16_t dy = vy - tg.y0, r0 = 0, r1 = e->h;
    if (dy < 0) r0 = -dy;
    if (dy + r1 > (int16_t)tg.ylim) r1 = (int16_t)tg.ylim - dy;
    if (r0 >= r1) return 1;
    spr_blit(PAGE_ADDR + e->off + (uint16_t)r0 * w * 2, w, (uint16_t)(r1 - r0),
             tg.base + (uint16_t)(dy + r0) * tg.stride + bc, tg.stride, tg.page);
    return 1;
}
#endif

/* вывод кадра (RLE proc3 в _band, n байт) */
static void c64_out(int16_t vx, int16_t vy, uint16_t n)
{
#ifndef HOST
    if (spr_ok && spr_draw(vx, vy, n, 1)) return;
#else
    (void)n;
#endif
    proc3(mirror ? vx : vx + 7, vy, width, mirror ? 1 : -1, band, 0, 0);
}

static void procC64(int16_t vx, int16_t vy)
{
    uint8_t ncol = (uint8_t)(width >> 3), h = (uint8_t)height;
    uint8_t cbuf[64], cpos = 64;   /* исходный RLE — кусками из страницы костюма */
    uint16_t sp = srcptr;
    uint8_t len = 0, color = 0, rep = 0;
    uint8_t *o = band, *end = band + sizeof band - 4;
    int16_t x0 = 0;
    uint16_t nsub = 0;
    if (!h || h > C64_HMAX) return;
    /* кадр целиком на экране — из кэша */
    int16_t xl = mirror ? vx : vx - (int16_t)(ncol - 1) * 8, xr = mirror ? vx + (int16_t)width : vx + 8;
    uint8_t whole = xl >= 0 && xr <= OUT_W;
    uint8_t slot = (uint8_t)(((srcptr ^ (srcptr >> 5) ^ c_page ^ (mirror << 4)) & 31) % C64_SLOTS);
    uint8_t tag = (uint8_t)(c_page | 0x40 | (mirror ? 0x80 : 0));
#ifndef HOST
    if (whole && spr_ok && spr_draw(vx, vy, 0, 0)) return;   /* готовый спрайт: без RLE */
#endif
    if (whole && c64_tpg[slot] == tag && c64_tsrc[slot] == srcptr)
    {
        far_rd(band, C64_PAGE, C64_CACHE + (uint16_t)slot * C64_SLOT_N, c64_tlen[slot]);
        c64_out(vx, vy, c64_tlen[slot]);
        return;
    }
#ifndef HOST
    if (whole)   /* видимый целиком — c64tr.s; исходник — в _bounce (на время перекодировки) */
    {
        uint16_t sn = 256;
        if ((uint32_t)srcptr + sn > PAGE_ADDR + PAGE_DATA_SIZE) sn = (uint16_t)(PAGE_ADDR + PAGE_DATA_SIZE - srcptr);
        far_rd(bounce, c_page, srcptr, sn);
        cpat_ok = 0;   /* в _bounce была палитра proc3 (struct P3) */
        uint16_t n = c64_tr(band, bounce, h, ncol, bounce + sn, band + sizeof band - 4, c64_col);
        if (n)
        {
            if (n <= C64_SLOT_N)
            {
                far_wr(C64_PAGE, C64_CACHE + (uint16_t)slot * C64_SLOT_N, band, n);
                c64_tsrc[slot] = srcptr;
                c64_tpg[slot] = tag;
                c64_tlen[slot] = (uint8_t)n;
            }
            c64_out(vx, vy, n);
            return;
        }
    }
#endif
#define C64_BYTE() (cpos < 64 ? cbuf[cpos++] : (far_rd(cbuf, c_page, sp, 64), sp += 64, cpos = 1, cbuf[0]))
    for (uint8_t c = 0; c < ncol; c++)
    {
        for (uint8_t y = 0; y < h; y++)   /* колонка C64 целиком */
        {
            while (!len)
            {
                len = C64_BYTE();
                rep = len & 0x80;
                if (rep) color = C64_BYTE();
                len &= 0x7F;
            }
            if (!rep) color = C64_BYTE();
            len--;
            uint8_t b = color;
            c64_col[3][y] = b & 3;
            b >>= 2;
            c64_col[2][y] = b & 3;
            b >>= 2;
            c64_col[1][y] = b & 3;
            c64_col[0][y] = b >> 2;
        }
        SND_POLL();
        /* точки колонки в порядке proc3: при mirror вправо от cx, иначе влево от
         * cx + 7; LINE: без mirror точки 0, 1 — биты 0..1, с mirror — биты 6..7 */
        int16_t cx = mirror ? vx + c * 8 : vx - c * 8;
        uint8_t *prev = 0, *pend = 0;
        for (uint8_t k = 0; k < 8; k++)
        {
            uint8_t p = mirror ? k : (uint8_t)(7 - k);
            int16_t px = cx + p;
            if (px < 0 || px >= OUT_W)
            {
                if (nsub) goto done;   /* дальше — только за краем */
                prev = 0;
                continue;
            }
            if (!nsub) x0 = px;
            uint8_t *s = o;
            if (prev && (k & 1))   /* вторая точка толстой — та же колонка */
            {
                uint16_t m = (uint16_t)(pend - prev);
                if (o + m > end) goto done;
                for (uint16_t i = 0; i < m; i++) *o++ = prev[i];
            }
            else
            {
                uint8_t f = mirror ? (uint8_t)(p >> 1) : (uint8_t)(3 - (p >> 1));
                o = c64_enc(o, c64_col[f], h, end);
                if (!o) { o = s; goto done; }
            }
            prev = s;
            pend = o;
            nsub++;
            SND_POLL();
        }
    }
done:
#undef C64_BYTE
    if (!nsub) return;
    if (whole && nsub == (uint16_t)width && (uint16_t)(o - band) <= C64_SLOT_N)
    {
        far_wr(C64_PAGE, C64_CACHE + (uint16_t)slot * C64_SLOT_N, band, (uint16_t)(o - band));
        c64_tsrc[slot] = srcptr;
        c64_tpg[slot] = tag;
        c64_tlen[slot] = (uint8_t)(o - band);
    }
#ifdef HOST
    { extern FILE *host_trace; if (getenv("C64STAT") && host_trace) fprintf(host_trace, "C64 pg%u src%u m%u w%d h%d nsub%u out%u\n", c_page, srcptr, mirror, width, height, nsub, (unsigned)(o - band)); }
#endif
    proc3(x0, vy, (int16_t)nsub, mirror ? 1 : -1, band, 0, 0);
}

/* costume.cpp: ClassicCostumeRenderer::mainRoutine (без масштабирования) */
static uint8_t mainRoutine(int16_t xmoveCur, int16_t ymoveCur)
{
    int16_t vx = actorX, vy = actorY;
    uint8_t drawFlag = 1;
    if (!mirror) xmoveCur = -xmoveCur;
    vx += xmoveCur;
    vy += ymoveCur;
    int16_t l, r, t, b;
    if (mirror)
    {
        l = vx;
        r = vx + width;
    }
    else
    {
        l = vx - width;
        r = vx;
    }
    t = vy;
    b = t + height;
    /* markRectAsDirty: полосы, занятые актёром */
    if (t >= OUT_H || b <= 0) return 0;
    if (l >= OUT_W || r <= 0) return 0;
    /* формат 0x57: колонки по 8 точек, ignorePakCols не нужен (procC64
     * отсекает по точкам); markRectAsDirty — на 8 точек правее */
    if (mirror) drawFlag = r <= OUT_W ? 2 : 1;
    else drawFlag = l >= -8 ? 2 : 1;
    if (l < 0) l = 0;
    if (t < 0) t = 0;
    if (b > OUT_H) b = OUT_H;
    if (draw_top > t) draw_top = t;
    if (draw_bottom < b) draw_bottom = b;
    if (draw_left > l) draw_left = l;
    if (draw_right < r + 8) draw_right = r + 8;
    if (meas_rec)
    {
        uint8_t *q = meas_rec + cur_limb * 4;
        q[0] = (uint8_t)t;
        q[1] = (uint8_t)b;
        q[2] = (uint8_t)l;
        q[3] = (uint8_t)(l >> 8);
    }
    if (height + t >= 256) return 2;
    if (!measuring) procC64(vx, vy);
    return drawFlag;
}

/* costume.cpp: ClassicCostumeRenderer::drawLimb */
static uint8_t drawLimb(struct Actor *a, uint8_t limb)
{
    cur_limb = limb;
    if (a->cost_curpos[limb] == 0xFFFF || (a->cost_stopped & (1u << limb))) return 0;
    uint16_t i = a->cost_curpos[limb] & 0x7FFF;
    uint16_t frameptr = c_res + far_rw(c_page, c_frameOffsets + limb * 2);
    uint8_t code = far_rb(c_page, c_animCmds + i) & 0x7F;
    if (code == 0x7B) return 0;
    srcptr = c_res + far_rw(c_page, frameptr + code * 2);
    int8_t hdr[6];   /* v1: ширина/8, высота, смещения x/8 и y кадра, сдвиг x/8 и y */
    far_rd(hdr, c_page, srcptr, sizeof hdr);
    width = (uint8_t)hdr[0] * 8;
    height = (uint8_t)hdr[1];
    int16_t xmoveCur = xmove + hdr[2] * 8;
    int16_t ymoveCur = ymove - hdr[3];
    xmove += hdr[4] * 8;
    ymove -= hdr[5];
    srcptr += 6;
    return mainRoutine(xmoveCur, ymoveCur);
}

/* z-маска под актёром: строки 0..127, полосы мира [s0, s0 + n) */
/* actor.cpp: Actor::drawActorCostume + prepareDrawActorCostume (v2) */
static void drawActorCostume(struct Actor *a)
{
    if (a->costume == 0) return;
    if (!a->needRedraw) return;
    a->needRedraw = 0;
    if (!loadCostume(a->costume)) return;
    actorX = (int16_t)(a->x * V12_X_MULTIPLIER);
    actorY = (int16_t)((a->y - a->elevation) * V12_Y_MULTIPLIER) + 1;   /* v1: y + 1 */
    actorX -= (int16_t)(screenStartStrip * 8);
    if (a->facing == 270) actorX += 16;
    /* procC64 (MM, свет для актёров включён): цвета 1, 2 — по актёру, 3 — чёрный */
    cpal[0] = cpal[3] = 0;
    cpal[1] = a->number < 25 ? v1_pal1[a->number] : 8;
    cpal[2] = a->number < 25 ? v1_pal2[a->number] : 0;
    /* setFacing */
    int16_t f = a->facing;
    uint8_t d = 3;
    if (f >= 71 && f <= 109) d = 1;
    else if (f >= 109 && f <= 251) d = 2;
    else if (f >= 251 && f <= 289) d = 0;
    uint8_t fmt = c_fmt;
    mirror = d != 0 || (fmt & 0x80);
    /* _zbuf: маска из бокса (v3 и ниже — не больше 1) */
    zbuf = getMaskFromBox(a->walkbox);
    if (zbuf > 1) zbuf = 1;
#ifndef HOST
    spr_ok = !a->moving && !zbuf;   /* идущий: построение (2 x proc3) окупается редко */
    spr_act = a->number;
#endif
    draw_top = draw_left = 0x7FFF;
    draw_bottom = draw_right = -0x7FFF;
    xmove = ymove = 0;
    cpat_ok = 0;
    uint8_t result = 0;
    for (uint8_t i = 0; i < 16; i++)
    {
        result |= drawLimb(a, i);
        SND_POLL();   /* конечность без рисования — тоже ~100 команд (far_rw, far_rb) */
    }
    if (result & 1) a->needRedraw = 1;   /* _needRedraw = (version <= 6) */
    a->top = draw_top;
    a->bottom = draw_bottom;
    if (!measuring)
    {
        for (uint8_t i = 0; i < 16; i++) a->dr_pos[i] = a->cost_curpos[i];
        a->dr_stopped = a->cost_stopped;
        a->dr_x = a->x;
        a->dr_y = a->y - a->elevation;
        a->dr_facing = a->facing;
        a->dr_sss = screenStartStrip;
        a->dr_cost = a->costume;
    }
}

/* gfxUsageBits для getActorFromPos */
uint8_t actor_in_strip(int16_t strip, uint8_t act)
{
    int16_t s = strip - screenStartStrip;
    if (s < 0 || s >= 40) return 0;
    return (strip_usage[s][act >> 3] >> (act & 7)) & 1;
}

/* восстановить фон экрана в прямоугольнике точек MM (x: 0..319, y: 0..127) */
/* прямоугольник точек MM -> колонки БК [c0, c1) экрана и строки [t, b); 0 — пусто */
static uint8_t rect_cols(int16_t l, int16_t *t, int16_t r, int16_t *b, uint16_t *c0, uint16_t *c1)
{
    if (l < 0) l = 0;
    if (r > OUT_W) r = OUT_W;
    if (*t < 0) *t = 0;
    if (*b > OUT_H) *b = OUT_H;
    if (l >= r || *t >= *b) return 0;
    *c0 = (uint16_t)((bk_x(l + xstart) - view0) >> 2);
    *c1 = (uint16_t)((bk_x(r - 1 + xstart) - view0) >> 2) + 1;
    if (*c1 > 64) *c1 = 64;
    uint16_t vcol = view0 >> 2;
    if (!bm_ring && vcol + *c1 > bm_stride) *c1 = bm_stride - vcol;   /* кольцо — с переносом */
    return *c0 < *c1;
}

static void restoreRect(int16_t l, int16_t t, int16_t r, int16_t b)
{
    uint16_t c0, c1;
    if (!rect_cols(l, &t, r, &b, &c0, &c1)) return;
    bm_blit_ring((view0 >> 2) + c0, (uint16_t)t, 040000u + (uint16_t)(VIEW_ROW + t) * 64 + c0, c1 - c0, (uint16_t)(b - t));
}

/* актёр без соседей — во внеэкранный буфер (PG_MASK, CANVAS): окно экрана под
 * старым и новым кадром -> буфер, фон под старым, кадр, буфер -> экран одной
 * записью: без мерцания. 0 — окно не помещается (рисовать на экране) */
#define CANVAS 0152000u
#define CANVAS_SIZE 05400u
void cv_load(uint16_t cv, uint16_t vram_addr, uint16_t ncols, uint16_t nrows);
void cv_store(uint16_t cv, uint16_t vram_addr, uint16_t ncols, uint16_t nrows);
void cv_bg(uint16_t cv, uint16_t cvstride, uint16_t bm, uint16_t bmstride, uint16_t ncols, uint16_t nrows);
static void drawActorCostume(struct Actor *a);

/* холст: окно экрана [l, r) x [t, b) точек MM; 0 — не помещается. Цель — холст */
static uint16_t cv_nc, cv_c0, cv_rows, cv_scr;
static int16_t cv_t;
static uint8_t cv_open(int16_t l, int16_t t, int16_t r, int16_t b)
{
    uint16_t c0, c1;
    if (!rect_cols(l, &t, r, &b, &c0, &c1)) return 0;
    uint16_t nc = c1 - c0, nrows = (uint16_t)(b - t);
    if (nc * nrows > CANVAS_SIZE) return 0;
    cv_nc = nc;
    cv_c0 = c0;
    cv_rows = nrows;
    cv_t = t;
    cv_scr = 040000u + (uint16_t)(VIEW_ROW + t) * 64 + c0;
    SND_POLL();
    cv_load(CANVAS, cv_scr, nc, nrows);
    tg.base = CANVAS;
    tg.stride = nc;
    tg.bxlim = nc * 4;
    tg.ylim = nrows;
    tg.view0 = view0 + c0 * 4;
    tg.page = PG_MASK;
    tg.y0 = t;
    return 1;
}

/* фон в холст под прямоугольником o (в пределах окна) */
static void cv_bgrect(const struct DrawnRect *o)
{
    int16_t ot = o->t < cv_t ? cv_t : o->t, ob = o->b > cv_t + (int16_t)cv_rows ? cv_t + (int16_t)cv_rows : o->b;
    uint16_t oc0, oc1;
    if (!rect_cols(o->l, &ot, o->r, &ob, &oc0, &oc1)) return;
    /* фон кольца: через край — двумя кусками */
    uint16_t col = (view0 >> 2) + oc0, n = oc1 - oc0, n1;
    if (col >= bm_stride) col -= bm_stride;
    n1 = bm_stride - col < n ? bm_stride - col : n;
    uint16_t cv = CANVAS + (uint16_t)(ot - cv_t) * cv_nc + (oc0 - cv_c0), row = PAGE_ADDR + (uint16_t)ot * bm_stride;
    cv_bg(cv, cv_nc, row + col, bm_stride, n1, (uint16_t)(ob - ot));
    if (n1 < n) cv_bg(cv + n1, cv_nc, row, bm_stride, n - n1, (uint16_t)(ob - ot));
}

static void cv_close(void)
{
    target_screen();
    cv_store(CANVAS, cv_scr, cv_nc, cv_rows);
}

static uint8_t draw_via_canvas(struct Actor *a, const struct DrawnRect *o, uint8_t part)
{
    uint8_t nr = a->needRedraw;
    int16_t tp = a->top, bt = a->bottom;
    /* актёр на месте (фон под ним цел — part), сменились лишь кадры конечностей
     * (речь): окно — только строки сменившихся конечностей и сдвинутых ими
     * следующих, в старом и новом кадре */
    uint8_t same = part && a->dr_cost == a->costume && a->dr_x == a->x && a->dr_y == a->y - a->elevation &&
                   a->dr_facing == a->facing && a->dr_sss == screenStartStrip;
    if (same)
    {
        uint16_t m = a->cost_stopped ^ a->dr_stopped;
        for (uint8_t i = 0; i < 16; i++)
            if (a->cost_curpos[i] != a->dr_pos[i]) m = 1;
        if (!m)   /* на экране всё как есть */
        {
            a->needRedraw = 0;
            draw_left = o->l;
            draw_right = o->r;
            return 1;
        }
    }
    /* прогон 0 — новый кадр (и draw_* целиком), 1 — прежний: обмен с dr_pos туда и обратно */
    uint8_t (*rec)[64] = meas_rec_buf;   /* не на стеке: при отрисовке он и так глубок */
    int16_t nl = 0, nrt = 0, nt = 0, nb = 0;
    for (uint8_t p = 0; p < 2; p++)
    {
        for (uint8_t i = 0; i < 64; i++) rec[p][i] = 0xFF;
        meas_rec = same ? rec[p] : 0;
        measuring = 1;
        a->needRedraw = 1;
        drawActorCostume(a);
        if (!p)
        {
            nl = draw_left;
            nrt = draw_right;
            nt = a->top;
            nb = a->bottom;
        }
        if (!same) break;
        for (uint8_t i = 0; i < 16; i++)
        {
            uint16_t v = a->cost_curpos[i];
            a->cost_curpos[i] = a->dr_pos[i];
            a->dr_pos[i] = v;
        }
        uint16_t v = a->cost_stopped;
        a->cost_stopped = a->dr_stopped;
        a->dr_stopped = v;
    }
    meas_rec = 0;
    measuring = 0;
    a->needRedraw = nr;
    draw_left = nl;
    draw_right = nrt;
    int16_t l = o->l, r = o->r, t = o->t, b = o->b;
    if (nrt > nl && nb > nt)
    {
        if (nl < l) l = nl;
        if (nrt > r) r = nrt;
        if (nt < t) t = nt;
        if (nb > b) b = nb;
    }
    if (same)
    {
        int16_t bt0 = 0x7FFF, bb0 = -1;
        for (uint8_t i = 0; i < 16; i++)
        {
            const uint8_t *n = rec[0] + i * 4, *q = rec[1] + i * 4;
            if (a->cost_curpos[i] == a->dr_pos[i] && !(((a->cost_stopped ^ a->dr_stopped) >> i) & 1) &&
                n[0] == q[0] && n[1] == q[1] && n[2] == q[2] && n[3] == q[3])
                continue;
            for (uint8_t k = 0; k < 2; k++, n = q)
                if (n[0] != 0xFF)
                {
                    if (n[0] < bt0) bt0 = n[0];
                    if (n[1] > bb0) bb0 = n[1];
                }
        }
        if (bb0 < 0)
        {
            a->top = tp;
            a->bottom = bt;
            return 1;
        }
        if (t < bt0) t = bt0;
        if (b > bb0) b = bb0;
    }
    a->top = tp;
    a->bottom = bt;
    if (!cv_open(l, t, r, b)) return 0;
    cv_bgrect(o);
    drawActorCostume(a);
    cv_close();
    return 1;
}

static uint8_t rect_overlap(const struct DrawnRect *p, const struct DrawnRect *q)
{
    return p->r > p->l && q->r > q->l && p->l < q->r && q->l < p->r && p->t < q->b && q->t < p->b;
}

/* actor.cpp: ScummEngine::processActors с resetActorBgs/setActorRedrawFlags:
 * перерисовываются изменившиеся актёры (R) и задетые восстановлением фона под
 * ними; актёр ближе к экрану, на которого лёг новый кадр из R, — поверх */
/* биты полос экрана, занятых прошлым кадром актёра (gfxUsageBits) */
static void strips_clear(uint8_t num)
{
    struct DrawnRect *d = &drawn[num];
    for (int16_t st = d->l / 8; st < (d->r + 7) / 8; st++)
        if (st >= 0 && st < 40) strip_usage[st][num >> 3] &= (uint8_t)~(1u << (num & 7));
}

/* прямоугольник нового кадра (прогон без рисования) */
static void measure(struct Actor *a, struct DrawnRect *n)
{
    uint8_t nr = a->needRedraw;
    int16_t tp = a->top, bt = a->bottom;
    meas_rec = 0;
    measuring = 1;
    a->needRedraw = 1;
    drawActorCostume(a);
    measuring = 0;
    a->needRedraw = nr;
    n->l = draw_left;
    n->r = draw_right;
    n->t = a->top;
    n->b = a->bottom;
    a->top = tp;
    a->bottom = bt;
}

static void rect_union(struct DrawnRect *u, const struct DrawnRect *q)
{
    if (q->r <= q->l || q->b <= q->t) return;
    if (q->l < u->l) u->l = q->l;
    if (q->r > u->r) u->r = q->r;
    if (q->t < u->t) u->t = q->t;
    if (q->b > u->b) u->b = q->b;
}

/* markRectAsDirty всех конечностей: drawn и gfxUsageBits по последней отрисовке */
static void mark_drawn(uint8_t num, const struct Actor *a)
{
    if (a->bottom <= a->top || draw_right <= draw_left) return;
    struct DrawnRect *d = &drawn[num];
    d->l = draw_left;
    d->r = draw_right;
    d->t = a->top;
    d->b = a->bottom;
    for (int16_t st = d->l / 8; st < (d->r + 7) / 8; st++)
        if (st >= 0 && st < 40) strip_usage[st][num >> 3] |= (uint8_t)(1u << (num & 7));
}

void processActors(void)
{
    xstart = (int16_t)PHYS_X(screenStartStrip * 8);   /* точка кольца фона (gfx_v1.c) */
    view0 = bk_x(xstart) & ~3u;
    target_screen();
    uint8_t flushed = gfx_view_flush();
    uint8_t view_redrawn = flushed == 1;   /* вся видимая часть: старых актёров уже нет */
    /* актёры, которых касается кадр: в комнате или с прошлым кадром на экране */
    uint8_t L[NUM_ACTORS], nl = 0, R[NUM_ACTORS], any = 0;
    SND_POLL();
    for (uint8_t i = 1; i < NUM_ACTORS; i++)
    {
        if (!(i & 3)) SND_POLL();
        struct Actor *a = &actors[i];
        uint8_t was = drawn[i].r > drawn[i].l;
        if (a->room != currentRoom && !was) continue;
        L[nl++] = i;
        R[i] = flushed != 0 || (a->room == currentRoom && a->visible && a->needRedraw) || (was && a->needBgReset);
        any |= R[i];
    }
    if (!any)
    {
        animate_only();
        return;
    }
    cursor_hide_actors(L, nl, R);   /* рисование актёров (и чтение экрана в холст) — без курсора */
    /* фон под R затрёт пересекающихся с ними */
    for (uint8_t grown = 1; grown;)
    {
        grown = 0;
        for (uint8_t p = 0; p < nl; p++)
            if (R[L[p]])
                for (uint8_t q = 0; q < nl; q++)
                    if (!R[L[q]] && rect_overlap(&drawn[L[p]], &drawn[L[q]])) R[L[q]] = grown = 1;
    }
    /* фон под актёром, ни с кем не пересекавшимся, — прямо перед его отрисовкой
     * (меньше мерцание); под пересекавшимися — сразу */
    struct DrawnRect late[NUM_ACTORS];
    for (uint8_t p = 0; p < nl; p++)   /* до обнуления drawn: иначе второй из пары — «одинокий» */
    {
        uint8_t i = L[p];
        late[i].l = late[i].r = 0;
        if (!R[i] || view_redrawn || drawn[i].r <= drawn[i].l) continue;
        uint8_t alone = actors[i].room == currentRoom && actors[i].visible;
        for (uint8_t q = 0; q < nl && alone; q++)
            if (L[q] != i && rect_overlap(&drawn[i], &drawn[L[q]])) alone = 0;
        if (alone) late[i] = drawn[i];
    }
    /* перекрывающиеся (R == 2) — вместе в холсте: фон под прежними кадрами, новые
     * кадры от дальних к ближним, на экран одной записью — без мерцания */
    uint8_t grp = 0;
    if (!view_redrawn)
    {
        struct DrawnRect u = { 0x7FFF, 0x7FFF, -0x7FFF, -0x7FFF }, nw;
        for (uint8_t p = 0; p < nl; p++)
        {
            uint8_t i = L[p];
            struct Actor *a = &actors[i];
            if (!R[i] || drawn[i].r <= drawn[i].l || late[i].r > late[i].l) continue;
            rect_union(&u, &drawn[i]);
            if (a->room == currentRoom && a->visible)
            {
                measure(a, &nw);
                rect_union(&u, &nw);
            }
            grp = 1;
        }
        if (grp && cv_open(u.l, u.t, u.r, u.b))
        {
            for (uint8_t p = 0; p < nl; p++)
            {
                uint8_t i = L[p];
                if (!R[i] || drawn[i].r <= drawn[i].l || late[i].r > late[i].l) continue;
                R[i] = 2;
                cv_bgrect(&drawn[i]);
            }
        }
        else
            grp = 0;
    }
    for (uint8_t p = 0; p < nl; p++)
    {
        uint8_t i = L[p];
        if (!R[i]) continue;
        if (R[i] == 1 && !view_redrawn && drawn[i].r > drawn[i].l && late[i].r <= late[i].l)
        {
            restoreRect(drawn[i].l, drawn[i].t, drawn[i].r, drawn[i].b);
            for (uint8_t q = 0; q < nl; q++)   /* задетые: перерисовывать только целиком */
                if (L[q] != i && rect_overlap(&drawn[i], &drawn[L[q]])) actors[L[q]].dr_cost = 0;
        }
        strips_clear(i);
        drawn[i].l = drawn[i].r = 0;
        actors[i].needBgReset = 0;
    }
    /* актёры комнаты — от дальних к ближним (порядок обменов — как в ScummVM) */
    uint8_t sorted[NUM_ACTORS], n = 0;
    for (uint8_t p = 0; p < nl; p++)
        if (actors[L[p]].room == currentRoom) sorted[n++] = L[p];
    for (uint8_t j = 0; j < n; ++j)
        for (uint8_t i = 0; i < n; ++i)
            if (actors[sorted[j]].y < actors[sorted[i]].y)
            {
                uint8_t t = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = t;
            }
    if (grp)
    {
        for (uint8_t k = 0; k < n; k++)
        {
            struct Actor *a = &actors[sorted[k]];
            if (R[sorted[k]] != 2 || !a->visible) continue;
            a->needRedraw = 1;
            drawActorCostume(a);
            mark_drawn(sorted[k], a);
        }
        cv_close();
    }
    for (uint8_t k = 0; k < n; k++)
    {
        uint8_t num = sorted[k];
        struct Actor *a = &actors[num];
        SND_POLL();
        if (!a->costume) continue;
        if (R[num] == 2)
        {
            animateCostume(a);
            continue;
        }
        if (!R[num])
            for (uint8_t q = 0; q < k; q++)   /* дальше него перерисован и лёг на него */
                if (R[sorted[q]] && rect_overlap(&drawn[sorted[q]], &drawn[num]))
                {
                    R[num] = 1;
                    strips_clear(num);
                    drawn[num].l = drawn[num].r = 0;
                    break;
                }
        uint8_t done = 0;
        if (late[num].r > late[num].l)
        {
            if (R[num] && a->visible)
            {
                a->needRedraw = 1;
                done = draw_via_canvas(a, &late[num], !flushed);
            }
            if (!done)
            {
                restoreRect(late[num].l, late[num].t, late[num].r, late[num].b);
                for (uint8_t q = k + 1; q < n; q++)   /* дальше по порядку: только целиком */
                    if (rect_overlap(&late[num], &late[sorted[q]]) || rect_overlap(&late[num], &drawn[sorted[q]]))
                        actors[sorted[q]].dr_cost = 0;
            }
        }
        if (R[num] && a->visible)
        {
            if (!done)
            {
                a->needRedraw = 1;   /* setActorRedrawFlags: фон под ним восстановлен */
                drawActorCostume(a);
            }
            mark_drawn(num, a);
        }
        animateCostume(a);
    }
    /* перед поздним восстановлением фона под A мог уже лечь новый кадр актёра
     * дальше него — тот перерисуется в следующем шаге */
    for (uint8_t k = 0; k < n; k++)
    {
        uint8_t num = sorted[k];
        if (late[num].r <= late[num].l) continue;
        for (uint8_t q = 0; q < k; q++)
            if (rect_overlap(&late[num], &drawn[sorted[q]]))
            {
                actors[sorted[q]].needRedraw = 1;
                actors[sorted[q]].dr_cost = 0;   /* фон лёг на него: только целиком */
            }
    }
}

/* экран очищен и перерисован целиком (чтение файла, загрузка игры): прошлых
 * кадров актёров на нём нет — нарисовать всех заново */
void actors_redraw_all(void)
{
    for (uint8_t i = 0; i < NUM_ACTORS; i++)
    {
        drawn[i].l = drawn[i].r = 0;
        actors[i].needRedraw = 1;
        actors[i].needBgReset = 0;
    }
    for (uint8_t s = 0; s < 40; s++) strip_usage[s][0] = strip_usage[s][1] = strip_usage[s][2] = strip_usage[s][3] = 0;
}
