/* Конечности кадра актёра (банк C): перевод ClassicCostumeRenderer (costume.cpp:
 * drawLimb / mainRoutine / procC64 / proc3) для костюмов MM v1, кэш спрайтов
 * и холст. Какие актёры и в каком порядке — costume_r.c (банк B); параметры
 * актёра и цель — в rd (rend.h). */
#ifdef HOST
#include <stdio.h>
#include <stdlib.h>
#endif
#include "rend.h"
#include "gfx.h"

#ifdef HOST
extern uint8_t host_vram[040000];
#define vram host_vram
extern uint8_t band[1024];
struct Rend rd;
#else
#define vram ((volatile uint8_t *)040000)
extern uint8_t band[1024], bounce[256];   /* sys.ld */
_Static_assert(sizeof(struct Rend) <= 0146, "struct Rend: до _band (sys.ld)");
#endif


/* цвет v1 -> 2x2 цветов БК (как фон, gfx_v1.c) */
static const uint8_t pat[16][4] =
{
    {0, 0, 0, 0}, {2, 2, 2, 3}, {3, 0, 0, 3}, {2, 1, 1, 2},
    {3, 1, 1, 3}, {2, 2, 2, 2}, {1, 0, 0, 1}, {3, 2, 2, 3},
    {3, 3, 3, 3}, {3, 0, 0, 3}, {3, 3, 3, 3}, {1, 1, 1, 1},
    {2, 1, 1, 2}, {2, 2, 2, 2}, {1, 1, 1, 1}, {1, 1, 1, 1},
};
static const uint8_t pix_mask[4] = { 03, 014, 060, 0300 };
static const uint8_t pix_val[4][4] = { {0, 1, 2, 3}, {0, 4, 010, 014}, {0, 020, 040, 060}, {0, 0100, 0200, 0300} };
static const uint8_t revBitMask[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

/* состояние рендерера (ClassicCostumeRenderer) */
static int16_t xmove, ymove, width, height;
static uint16_t c_frameOffsets;
static uint16_t srcptr;       /* адрес кадра в странице костюма */
static uint8_t cur_limb;
static uint8_t cpat_ok;     /* P3.cpat построена для текущего актёра */

static uint16_t bk_x(int16_t x)
{
    return BK_X(x);
}

void target_screen(void)
{
    rd.tg.base = 040000u + VIEW_ROW * 64;
    rd.tg.stride = 64;
    rd.tg.bxlim = 256;
    rd.tg.ylim = OUT_H;
    rd.tg.view0 = rd.view0;
    rd.tg.page = 0;
    rd.tg.y0 = 0;
}


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

/* a * b, b — малое (ширина, шаг строки): __mulhi3 (умножения у ВМ1 нет) — цикл на 16 бит */
static uint16_t mulu(uint16_t a, uint8_t b)
{
    uint16_t r = 0;
    for (; b; b >>= 1, a <<= 1)
    {
        uint8_t k = b & 1;
        __asm__ volatile("" : "+r"(k));   /* иначе gcc узнаёт умножение и зовёт __mulhi3 */
        if (k) r += a;
    }
    return r;
}

/* proc3: v_x — колонка экрана MM, v_y — верх, skip_width — колонок;
 * len0/color0 — незаконченная серия после codec1_ignorePakCols (вход в StartPos) */
static void proc3(int16_t v_x, int16_t v_y, int16_t skip_width, int8_t step, const uint8_t *src,
                  uint8_t len0, uint8_t color0)
{
    struct P3 *p = P3_BUF;
    p->src = src;
    p->len = len0;
    p->color = color0;
    p->vy = v_y - rd.tg.y0;
    p->h = (uint16_t)height;
    p->skip = (uint16_t)skip_width;
    p->step = step;
    p->x = v_x;
    p->xstart = rd.xstart;
    p->view0 = rd.tg.view0;
    int16_t dy = v_y - rd.tg.y0;
    p->drow0 = rd.tg.base + (rd.tg.stride == 64 ? (uint16_t)dy << 6 : mulu((uint16_t)dy, (uint8_t)rd.tg.stride));
    p->dstride = rd.tg.stride;
    p->bxlim = rd.tg.bxlim;
    p->ylim = rd.tg.ylim;
    p->dpage = rd.tg.page;
    p->zbuf = rd.zbuf;
    p->mrow0 = rd.zbuf ? PAGE_ADDR + mulu((uint16_t)v_y, bm_strips) : 0;
    p->mstride = bm_strips;
    p->mring = bm_ring ? bm_strips : 0xFFFF;
    /* цвета — на актёра (палитра и чётность цели те же); _bounce между
     * конечностями никто не трогает */
    if (!cpat_ok)
    {
        cpat_ok = 1;
        uint8_t sw = (uint8_t)(rd.tg.y0 & 1);   /* y цели — от верха окна: чётность как на экране */
        for (uint8_t c = 0; c < 16; c++)
        {
            if (!(c & 7)) SND_POLL();
            uint8_t e = c < 4 ? rd.cpal[c] & 15 : 0, *o = p->cpat[c];
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
/* Кэш спрайтов (PG_SPR): конечность, видимая целиком (маска глубины — при
 * выводе, spr_mask), хранится готовыми байтами БК — строки по 2w байт {точки, цвет};
 * вывод — sprblit.s, две команды на байт вместо proc3 по точкам. Ключ — кадр,
 * зеркало, актёр (палитра), точка в байте БК и чётность строки. Костюмы в куче не
 * переезжают: кэш живёт и при смене комнаты. Построение (proc3 и пары mrow.s) —
 * не больше трёх за шаг (rd.spr_budget): иначе первые шаги в комнате — по секунде.
 *
 * Слоты — хеш ключа, окно из SPR_WIN слотов (перебор всех стоил ~2.5%). Память —
 * кольцо (spr_top) кругами spr_lap: спрайт годен, если построен на этом круге или
 * на прошлом за вершиной кольца — без цикла вытеснения при построении */
#define SPR_SIZE 24576u
#define SPR_SLOTS 128
#define SPR_WIN 4
struct SprSlot { uint16_t src, off; uint8_t pgm, act, ph, w, h, lap, pad[6]; };   /* 16 байт: индекс — сдвигом */
static struct SprSlot spr[SPR_SLOTS];
static uint16_t spr_top;
static uint8_t spr_lap, spr_rr;

static uint8_t spr_valid(const struct SprSlot *s)
{
    return s->h && (s->lap == spr_lap || ((uint8_t)(s->lap + 1) == spr_lap && s->off >= spr_top));
}
void spr_blit(uint16_t src, uint16_t w, uint16_t h, uint16_t dst, uint16_t dstride, uint16_t dpage);
void spr_blitb(uint16_t w, uint16_t h, uint16_t dst, uint16_t dstride, uint16_t dpage);
void spr_mask(uint8_t *pairs, uint16_t w, const uint8_t *m, uint16_t bo, uint16_t ns, uint16_t nrows);
void spr_pairs(uint8_t *a, uint16_t n, uint16_t f);
#endif

/* смена комнаты: C64_CACHE лежит в странице маски — её затёрла новая маска */
void c64_cache_clear(void)
{
    for (uint8_t i = 0; i < C64_SLOTS; i++) c64_tpg[i] = 0;
}

/* костюмы могли переехать (загрузка игры, старт) */
void spr_cache_clear(void)
{
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
    int16_t xl = rd.mirror ? vx : vx - (int16_t)((width >> 3) - 1) * 8;
    uint16_t wl = (uint16_t)(xl + rd.xstart), wr = wl + (uint16_t)width - 1;
    if (wr >= 1280) return 0;
    /* спрайт строится в канонической точке мира cw (точка в байте БК, округлённая
     * до чётной: узор тот же, сдвиг влево на точку — в пределах байта, за прежние
     * столбцы не выходит), а не в своей: выпадающие при сжатии 4:5 столбцы — на
     * точку-другую иначе, зато у идущего кадры повторяются (шаг 8 точек меняет
     * фазу мира каждый раз) и помещаются в кэш */
    uint16_t b0 = BK_X(wl) & ~3u, cw = BK_X(wl) & 2u;
    uint8_t w = (uint8_t)((BK_X(cw + (uint16_t)width - 1) >> 2) + 1), h = (uint8_t)height;
    uint16_t sz = mulu(h, w);
    if (w > 16 || sz > 384 || (build && n > 256) || b0 < rd.tg.view0) return 0;
    uint16_t bc = (b0 - rd.tg.view0) >> 2;
    if (bc + w > (rd.tg.bxlim >> 2)) return 0;
    /* маска глубины в прямоугольнике байтов БК кадра: байт мира B — точки MM
     * 5B..5B+3. Задевает кадр — накладывается на спрайт при выводе (spr_mask;
     * байты маски — в band + 768, до 256), пуста — спрайт как есть */
    uint16_t p0 = (b0 >> 2) * 5, ns = 0, mrd = 0;
    int16_t y0 = 0, y1 = 0;
    uint8_t mk = 0;
    if (rd.zbuf)
    {
        uint16_t s0 = p0 >> 3, s1 = (p0 + (uint16_t)w * 5 - 2) >> 3;
        if (bm_ring)
        {
            s0 %= bm_strips;
            s1 %= bm_strips;
        }
        else if (s1 >= bm_strips) s1 = bm_strips - 1;
        y0 = vy < 0 ? 0 : vy;
        y1 = vy + h > (int16_t)roomHeight ? (int16_t)roomHeight : vy + h;
        ns = s1 - s0 + 1;
        uint16_t n2 = ns * (uint16_t)(y1 - y0);
        if (s1 < s0 || y1 <= y0 || n2 > 512) return 0;
        uint8_t *mb = n2 > 256 ? band + 256 : band + 768;
        mrd = PAGE_ADDR + (uint16_t)y0 * bm_strips + s0;
        far_rd2d(PG_MASK, mrd, bm_strips, mb, ns, ns, (uint16_t)(y1 - y0));
        for (uint8_t *m = mb; n2; n2--)
            if (*m++)
            {
                if (mb != band + 768) return 0;
                mk = 1;
                break;
            }
    }
    uint8_t ph = (uint8_t)(cw | ((vy & 1) << 5));
    uint8_t pgm = (uint8_t)(rd.c_page | (rd.mirror ? 0x80 : 0));
    struct SprSlot *e = 0, *v = 0;
    SND_POLL();
    uint8_t hs = (uint8_t)(srcptr ^ (srcptr >> 7) ^ pgm ^ (uint8_t)(rd.spr_act << 2) ^ ph);
    for (uint8_t k = 0; k < SPR_WIN; k++)
    {
        struct SprSlot *s = &spr[(uint8_t)(hs + k) & (SPR_SLOTS - 1)];
        if (!spr_valid(s))
        {
            if (!v) v = s;   /* место для построения */
        }
        else if (s->src == srcptr && s->pgm == pgm && s->act == rd.spr_act && s->ph == ph)
        {
            e = s;
            break;
        }
    }
    if (!e && (!build || !rd.spr_budget)) return 0;
    if (!e)
    {
        /* построить: proc3 в SPR_SCR (PG_MASK) на заливке значением точки, которого
         * нет в цветах актёра, — один проход: точка, отличная от заливки, — спрайта
         * (заняты все четыре — голубая одежда — без кэша, по точкам) */
        uint16_t need = sz * 2;
        uint8_t used = 0, f = 0;
        for (uint8_t c = 1; c < 4; c++)
            for (uint8_t k = 0; k < 4; k++) used |= (uint8_t)(1u << pat[rd.cpal[c] & 15][k]);
        while ((used >> f) & 1)
            if (++f == 4) return 0;
        static const uint8_t fill4[4] = { 0, 0125, 0252, 0377 };
        uint8_t F = fill4[f], *A = band + 256;
        struct Target keep = rd.tg;
        rd.tg.base = SPR_SCR;
        rd.tg.stride = w;
        rd.tg.bxlim = (uint16_t)w * 4;
        rd.tg.ylim = h;
        rd.tg.view0 = 0;
        rd.tg.page = PG_MASK;
        rd.tg.y0 = vy;
        int16_t xs = rd.xstart;
        uint8_t zb = rd.zbuf;
        rd.xstart += (int16_t)(cw - wl);
        rd.zbuf = 0;   /* маска у настоящего места пуста; у канонического — чужая */
        far_fill(PG_MASK, SPR_SCR, F, sz);
        cpat_ok = 0;
        proc3(rd.mirror ? vx : vx + 7, vy, width, rd.mirror ? 1 : -1, band, 0, 0);
        far_rd(A, PG_MASK, SPR_SCR, sz);
        rd.xstart = xs;
        rd.zbuf = zb;
        rd.tg = keep;
        cpat_ok = 0;
        rd.spr_budget--;
        SND_POLL();
        if (spr_top + need > SPR_SIZE)
        {
            spr_top = 0;
            spr_lap++;
            for (uint8_t i = 0; i < SPR_SLOTS; i++)   /* старше прошлого круга (номер круга — байт) */
                if (spr[i].lap != (uint8_t)(spr_lap - 1)) spr[i].h = 0;
        }
        SND_POLL();
        e = v ? v : &spr[(uint8_t)(hs + (spr_rr++ & (SPR_WIN - 1))) & (SPR_SLOTS - 1)];   /* окно занято — по кругу в нём */
        e->src = srcptr;
        e->pgm = pgm;
        e->act = rd.spr_act;
        e->ph = ph;
        e->w = w;
        e->h = h;
        e->off = spr_top;
        e->lap = spr_lap;
        spr_top += need;
        spr_pairs(A, sz, F);   /* {маска, цвет} на месте (mrow.s) */
        for (uint16_t o = 0; o < need; o += 128)   /* кусками: фронт тона между ними */
            far_wr(PG_SPR, PAGE_ADDR + e->off + o, A + o, need - o < 128 ? need - o : 128);
        if (mk)   /* построение затёрло band + 256.. */
            far_rd2d(PG_MASK, mrd, bm_strips, band + 768, ns, ns, (uint16_t)(y1 - y0));
    }
    /* строки цели: [0, ylim) от rd.tg.y0 */
    int16_t dy = vy - rd.tg.y0, r0 = 0, r1 = e->h;
    if (dy < 0) r0 = -dy;
    if (dy + r1 > (int16_t)rd.tg.ylim) r1 = (int16_t)rd.tg.ylim - dy;
    if (r0 >= r1) return 1;
    uint16_t w2 = (uint16_t)w * 2, src = PAGE_ADDR + e->off + mulu((uint16_t)r0, (uint8_t)w2),
             dst = rd.tg.base + mulu((uint16_t)(dy + r0), (uint8_t)rd.tg.stride) + bc, nr = (uint16_t)(r1 - r0);
    if (!mk)
    {
        spr_blit(src, w, nr, dst, rd.tg.stride, rd.tg.page);
        return 1;
    }
    /* за маской: строки — в band, снять закрытые точки, вывести оттуда */
    uint16_t nb = mulu(nr, (uint8_t)w2);
    for (uint16_t o = 0; o < nb; o += 128)
    {
        far_rd(band + o, PG_SPR, src + o, nb - o < 128 ? nb - o : 128);
        SND_POLL();
    }
    int16_t ya = vy + r0 > y0 ? vy + r0 : y0, yb = vy + r1 < y1 ? vy + r1 : y1;
    if (ya < yb)
        spr_mask(band + mulu((uint16_t)(ya - vy - r0), (uint8_t)w2), w, band + 768 + mulu((uint16_t)(ya - y0), (uint8_t)ns),
                 p0 & 7, ns, (uint16_t)(yb - ya));
    spr_blitb(w, nr, dst, rd.tg.stride, rd.tg.page);
    return 1;
}
#endif

/* вывод кадра (RLE proc3 в _band, n байт) */
static void c64_out(int16_t vx, int16_t vy, uint16_t n)
{
#ifndef HOST
    if (rd.spr_ok && spr_draw(vx, vy, n, 1)) return;
#else
    (void)n;
#endif
    proc3(rd.mirror ? vx : vx + 7, vy, width, rd.mirror ? 1 : -1, band, 0, 0);
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
    int16_t xl = rd.mirror ? vx : vx - (int16_t)(ncol - 1) * 8, xr = rd.mirror ? vx + (int16_t)width : vx + 8;
    uint8_t whole = xl >= 0 && xr <= OUT_W;
    uint8_t slot = (uint8_t)(((srcptr ^ (srcptr >> 5) ^ rd.c_page ^ (rd.mirror << 4)) & 31) % C64_SLOTS);
    uint8_t tag = (uint8_t)(rd.c_page | 0x40 | (rd.mirror ? 0x80 : 0));
#ifndef HOST
    if (whole && rd.spr_ok && spr_draw(vx, vy, 0, 0)) return;   /* готовый спрайт: без RLE */
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
        far_rd(bounce, rd.c_page, srcptr, sn);
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
#define C64_BYTE() (cpos < 64 ? cbuf[cpos++] : (far_rd(cbuf, rd.c_page, sp, 64), sp += 64, cpos = 1, cbuf[0]))
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
        /* точки колонки в порядке proc3: при rd.mirror вправо от cx, иначе влево от
         * cx + 7; LINE: без rd.mirror точки 0, 1 — биты 0..1, с rd.mirror — биты 6..7 */
        int16_t cx = rd.mirror ? vx + c * 8 : vx - c * 8;
        uint8_t *prev = 0, *pend = 0;
        for (uint8_t k = 0; k < 8; k++)
        {
            uint8_t p = rd.mirror ? k : (uint8_t)(7 - k);
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
                uint8_t f = rd.mirror ? (uint8_t)(p >> 1) : (uint8_t)(3 - (p >> 1));
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
    { extern FILE *host_trace; if (getenv("C64STAT") && host_trace) fprintf(host_trace, "C64 pg%u src%u m%u w%d h%d nsub%u out%u\n", rd.c_page, srcptr, rd.mirror, width, height, nsub, (unsigned)(o - band)); }
#endif
    proc3(x0, vy, (int16_t)nsub, rd.mirror ? 1 : -1, band, 0, 0);
}

/* costume.cpp: ClassicCostumeRenderer::mainRoutine (без масштабирования) */
static uint8_t mainRoutine(int16_t xmoveCur, int16_t ymoveCur)
{
    int16_t vx = rd.actorX, vy = rd.actorY;
    uint8_t drawFlag = 1;
    if (!rd.mirror) xmoveCur = -xmoveCur;
    vx += xmoveCur;
    vy += ymoveCur;
    int16_t l, r, t, b;
    if (rd.mirror)
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
    if (rd.mirror) drawFlag = r <= OUT_W ? 2 : 1;
    else drawFlag = l >= -8 ? 2 : 1;
    if (l < 0) l = 0;
    if (t < 0) t = 0;
    if (b > OUT_H) b = OUT_H;
    if (rd.draw_top > t) rd.draw_top = t;
    if (rd.draw_bottom < b) rd.draw_bottom = b;
    if (rd.draw_left > l) rd.draw_left = l;
    if (rd.draw_right < r + 8) rd.draw_right = r + 8;
    if (rd.meas_rec)
    {
        uint8_t *q = rd.meas_rec + cur_limb * 4;
        q[0] = (uint8_t)t;
        q[1] = (uint8_t)b;
        q[2] = (uint8_t)l;
        q[3] = (uint8_t)(l >> 8);
    }
    if (height + t >= 256) return 2;
    if (!rd.measuring) procC64(vx, vy);
    return drawFlag;
}

/* costume.cpp: ClassicCostumeRenderer::drawLimb */
static uint8_t drawLimb(uint8_t limb)
{
    cur_limb = limb;
    if (rd.pos[limb] == 0xFFFF || (rd.stopped & (1u << limb))) return 0;
    uint16_t i = rd.pos[limb] & 0x7FFF;
    uint16_t frameptr = rd.c_res + far_rw(rd.c_page, c_frameOffsets + limb * 2);
    uint8_t code = far_rb(rd.c_page, rd.c_animCmds + i) & 0x7F;
    if (code == 0x7B) return 0;
    srcptr = rd.c_res + far_rw(rd.c_page, frameptr + code * 2);
    int8_t hdr[6];   /* v1: ширина/8, высота, смещения x/8 и y кадра, сдвиг x/8 и y */
    far_rd(hdr, rd.c_page, srcptr, sizeof hdr);
    width = (uint8_t)hdr[0] * 8;
    height = (uint8_t)hdr[1];
    int16_t xmoveCur = xmove + hdr[2] * 8;
    int16_t ymoveCur = ymove - hdr[3];
    xmove += hdr[4] * 8;
    ymove -= hdr[5];
    srcptr += 6;
    return mainRoutine(xmoveCur, ymoveCur);
}


/* Actor::drawActorCostume: все конечности кадра (параметры — rd) */
uint8_t rend_limbs(void)
{
    c_frameOffsets = rd.c_res + 4;
    rd.draw_top = rd.draw_left = 0x7FFF;
    rd.draw_bottom = rd.draw_right = -0x7FFF;
    xmove = ymove = 0;
    cpat_ok = 0;
    uint8_t result = 0;
    for (uint8_t i = 0; i < 16; i++)
    {
        result |= drawLimb(i);
        SND_POLL();   /* конечность без рисования — тоже ~100 команд (far_rw, far_rb) */
    }
    return result;
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
    *c0 = (uint16_t)((bk_x(l + rd.xstart) - rd.view0) >> 2);
    *c1 = (uint16_t)((bk_x(r - 1 + rd.xstart) - rd.view0) >> 2) + 1;
    if (*c1 > 64) *c1 = 64;
    uint16_t vcol = rd.view0 >> 2;
    if (!bm_ring && vcol + *c1 > bm_stride) *c1 = bm_stride - vcol;   /* кольцо — с переносом */
    return *c0 < *c1;
}

void restoreRect(int16_t l, int16_t t, int16_t r, int16_t b)
{
    uint16_t c0, c1;
    if (!rect_cols(l, &t, r, &b, &c0, &c1)) return;
    bm_blit_ring((rd.view0 >> 2) + c0, (uint16_t)t, 040000u + (uint16_t)(VIEW_ROW + t) * 64 + c0, c1 - c0, (uint16_t)(b - t));
}

/* актёр без соседей — во внеэкранный буфер (PG_MASK, CANVAS): окно экрана под
 * старым и новым кадром -> буфер, фон под старым, кадр, буфер -> экран одной
 * записью: без мерцания. 0 — окно не помещается (рисовать на экране) */
#define CANVAS 0152000u
#define CANVAS_SIZE 05400u
void cv_load(uint16_t cv, uint16_t vram_addr, uint16_t ncols, uint16_t nrows, uint16_t pad);
void cv_store(uint16_t cv, uint16_t vram_addr, uint16_t ncols, uint16_t nrows, uint16_t pad);
void cv_bg(uint16_t cv, uint16_t cvstride, uint16_t bm, uint16_t bmstride, uint16_t ncols, uint16_t nrows);

/* холст: окно экрана [l, r) x [t, b) точек MM; 0 — не помещается. Цель — холст.
 * Строки — с чётным шагом cv_ns от cv_base той же чётности, что столбец экрана:
 * переносы proc3.s cv_* идут словами */
static uint16_t cv_nc, cv_ns, cv_base, cv_c0, cv_rows, cv_scr;
static int16_t cv_t;
uint8_t cv_open(int16_t l, int16_t t, int16_t r, int16_t b, uint8_t load)
{
    uint16_t c0, c1;
    if (!rect_cols(l, &t, r, &b, &c0, &c1)) return 0;
    uint16_t nc = c1 - c0, nrows = (uint16_t)(b - t), ns = (nc + 1) & ~1u;
    if (mulu(nrows, (uint8_t)ns) + 1 > CANVAS_SIZE) return 0;
    cv_nc = nc;
    cv_ns = ns;
    cv_base = CANVAS + (c0 & 1);
    cv_c0 = c0;
    cv_rows = nrows;
    cv_t = t;
    cv_scr = 040000u + (uint16_t)(VIEW_ROW + t) * 64 + c0;
    SND_POLL();
    if (load) cv_load(cv_base, cv_scr, nc, nrows, ns - nc);
    rd.tg.base = cv_base;
    rd.tg.stride = ns;
    rd.tg.bxlim = nc * 4;
    rd.tg.ylim = nrows;
    rd.tg.view0 = rd.view0 + c0 * 4;
    rd.tg.page = PG_MASK;
    rd.tg.y0 = t;
    return 1;
}

/* фон в холст под прямоугольником o (в пределах окна) */
void cv_bgrect(const struct DrawnRect *o)
{
    int16_t ot = o->t < cv_t ? cv_t : o->t, ob = o->b > cv_t + (int16_t)cv_rows ? cv_t + (int16_t)cv_rows : o->b;
    uint16_t oc0, oc1;
    if (!rect_cols(o->l, &ot, o->r, &ob, &oc0, &oc1)) return;
    /* фон кольца: через край — двумя кусками */
    uint16_t col = (rd.view0 >> 2) + oc0, n = oc1 - oc0, n1;
    if (col >= bm_stride) col -= bm_stride;
    n1 = bm_stride - col < n ? bm_stride - col : n;
    uint16_t cv = cv_base + mulu((uint16_t)(ot - cv_t), (uint8_t)cv_ns) + (oc0 - cv_c0), row = PAGE_ADDR + (uint16_t)ot * bm_stride;
    cv_bg(cv, cv_ns, row + col, bm_stride, n1, (uint16_t)(ob - ot));
    if (n1 < n) cv_bg(cv + n1, cv_ns, row, bm_stride, n - n1, (uint16_t)(ob - ot));
}

/* вывод всего вида — в обход окна холста (gfx.c) */
void cv_view_excl(void)
{
    gfx_view_excl(cv_c0, cv_nc, (uint16_t)cv_t, cv_rows);
}

void cv_close(void)
{
    target_screen();
    cv_store(cv_base, cv_scr, cv_nc, cv_rows, cv_ns - cv_nc);
}

