/* Отрисовка актёров (банк B): перевод ClassicCostumeRenderer (costume.cpp:
 * drawLimb / mainRoutine / proc3, base-costume.cpp: drawCostume /
 * codec1_ignorePakCols) и Actor_v2::prepareDrawActorCostume (actor.cpp) для
 * костюмов v2 (формат 0x58, GF_OLD_BUNDLE, без масштабирования).
 *
 * Координаты — точки главного виртуального экрана MM (320x128). Колонка экрана x
 * (мир x + xstart) с x_мира%5 == 4 пропускается, остальные встают в точку БК
 * bx_мира - view0 (view0 — начало видимой части, кратное 4). Кадр конечности
 * (RLE) копируется в _band; proc3 (proc3.s) читает z-маску прямо в её
 * странице и пишет в видеопамять или в холст. Конечность, видимая целиком и без
 * маски глубины, рисуется в холст из кэша спрайтов (готовые байты БК). */
#include "actor.h"
#include "gfx.h"

#define VIEW_ROW 44                 /* строка БК начала комнаты: 28 + 16 */
#define OUT_W 320
#define OUT_H 128
#ifdef HOST
extern uint8_t host_vram[040000];
#define vram host_vram
extern uint8_t band[2048];
#else
#define vram ((volatile uint8_t *)040000)
extern uint8_t band[2048], bounce[256];   /* sys.ld */
#endif

/* цвет EGA -> 2x2 цветов БК — как фон: cur_pat палитры комнаты (ядро, gfx_v2.c) */
#define pat cur_pat
static const uint8_t pix_mask[4] = { 03, 014, 060, 0300 };
static const uint8_t pix_val[4][4] = { {0, 1, 2, 3}, {0, 4, 010, 014}, {0, 020, 040, 060}, {0, 0100, 0200, 0300} };
static const uint8_t revBitMask[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

/* состояние рендерера (ClassicCostumeRenderer) */
static int16_t actorX, actorY, xmove, ymove, width, height;
static uint8_t mirror, zbuf, cpal[16];
static uint8_t c_page, c_cost;
static uint16_t c_res, c_frameOffsets, c_animCmds;
static uint8_t c_palette0, c_fmt;
static uint16_t srcptr;       /* адрес кадра в странице костюма */
static int16_t draw_top, draw_bottom, draw_left, draw_right;
static int16_t xstart;        /* _virtscr[kMainVirtScreen].xstart */
static uint16_t view0;        /* точка БК мира в левом крае экрана */

/* прямоугольник, занятый актёром в прошлом кадре (для восстановления фона) */
struct DrawnRect { int16_t l, t, r, b; };
static struct DrawnRect drawn[NUM_ACTORS];
static uint8_t strip_usage[40][4];   /* gfxUsageBits: актёры по полосам экрана */

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
    c_cost = id;
    c_res = ci->res;
    c_fmt = ci->fmt;
    c_palette0 = ci->palette0;
    c_frameOffsets = ci->ptr + 2;
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
static int16_t *meas_rec;   /* и прямоугольники конечностей {l, t, r, b} (t 0x7FFF — нет) */
static uint8_t cur_limb;

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
    uint16_t dpage;         /* модель: 0 — видеопамять, иначе страница буфера */
    uint16_t bmoff;         /* 0144: постройка спрайта — смещение маски от цвета */
    uint16_t newpal;        /* 0146: cpat построена заново */
};
#ifndef HOST
_Static_assert(__builtin_offsetof(struct P3, cpat) == 032 && __builtin_offsetof(struct P3, ylim) == 0140 &&
               __builtin_offsetof(struct P3, newpal) == 0146, "struct P3: смещения в proc3.s");
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
                uint8_t masked = p->zbuf &&
                    (far_rb(PG_MASK, (uint16_t)(p->mrow0 + (y - p->vy) * p->mstride + wx / 8)) & revBitMask[wx & 7]);
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

/* цвета актёра для proc3: cpal -> узоры палитры комнаты. Строка цели y0 всегда
 * чётная: чётность строки от её верха — та же, что на экране */
static void cpat_build(void)
{
    struct P3 *p = P3_BUF;
    for (uint8_t c = 0; c < 16; c++)
    {
        const uint8_t *q = pat[cpal[c] & 15];
        uint8_t *o = p->cpat[c];
        o[0] = q[0];
        o[1] = q[1];
        o[2] = q[2];
        o[3] = q[3];
    }
    p->newpal = 1;
}

static uint16_t p3_bmoff;   /* постройка спрайта (spr_draw) */

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
    p->bmoff = p3_bmoff;
    /* cpat — на актёра (cpat_build): _bounce между конечностями никто не трогает */
    proc3_run(p);
}

/* base-costume.cpp: codec1_ignorePakCols — пропуск num колонок RLE */
static const uint8_t *ignorePakCols(const uint8_t *src, int16_t num, uint8_t *replen, uint8_t *repcolor)
{
    int32_t n = (int32_t)num * height;
    for (;;)
    {
        *replen = *src++;
        *repcolor = *replen >> 4;
        *replen &= 15;
        if (!*replen) *replen = *src++;
        do
        {
            if (!--n) return src;
        } while (--*replen);
    }
}

#ifndef HOST
/* длина RLE кадров, нарисованных целиком (по концу чтения в proc3): в _band
 * копируется она, а не оценка width * height */
#define RL_N 64
static struct { uint16_t addr, len; uint8_t page; } rl_len[RL_N];
#define RL_SLOT(pg, a) ((uint8_t)(((a) ^ (a) >> 6 ^ (pg)) & (RL_N - 1)))
static uint8_t rl_s;
#endif
static uint16_t rl_n;

/* RLE кадра (srcptr) -> _band (ScummVM читает его на месте); байт RLE — не
 * меньше точки: кадр не длиннее width * height */
static void rle_read(void)
{
    uint16_t n;
#ifndef HOST
    rl_s = RL_SLOT(c_page, srcptr);
    if (rl_len[rl_s].addr == srcptr && rl_len[rl_s].page == c_page)
        n = rl_len[rl_s].len;
    else
#endif
    {
        n = sizeof band;
        if ((uint16_t)width < 256 && (uint16_t)height < 256 && (uint16_t)width * (uint16_t)height + 2 < n)
            n = (uint16_t)width * (uint16_t)height + 2;
        if ((uint32_t)srcptr + n > PAGE_ADDR + PAGE_DATA_SIZE) n = (uint16_t)(PAGE_ADDR + PAGE_DATA_SIZE - srcptr);
    }
    rl_n = n;
    far_rd(band, c_page, srcptr, n);
}

/* после proc3 по кадру целиком: длина RLE — до конца прочитанного */
static void rle_used(void)
{
#ifndef HOST
    uint16_t used = (uint16_t)(P3_BUF->src - band);
    if (used <= rl_n)
    {
        rl_len[rl_s].addr = srcptr;
        rl_len[rl_s].page = c_page;
        rl_len[rl_s].len = used;
    }
#endif
}

#ifndef HOST
/* Кэш спрайтов — страница 2 ОЗУ 11М в окне 040000 (подключается на время
 * рисования актёра в холст). Спрайт — конечность, видимая целиком: плоскости
 * цвета и маски (байты БК, h строк по w), строит proc3 (режим постройки).
 * Ключ: костюм и его место в куче (вытесненный и прочитанный заново — промах),
 * кадр, зеркало, фаза сжатия 4:5 и точка в байте (по x мира), чётность строки,
 * цвета (слот палитры). Память — кольцо кругами lap: спрайт годен, если построен
 * на этом круге или на прошлом за вершиной. После запуска страница — в нулях */
#define SPR_PG 2
#define SPR_SLOTS 64
#define SPR_WIN 8
#define SPR_NPAL 8
#define SPR_BUILDS 4            /* построений за шаг */
struct SprHdr { uint16_t top; uint8_t lap, rr, prr, pad[11]; };
struct SprPal { uint16_t sum; uint8_t cpat[64]; };
struct SprSlot { uint16_t res, foff, off, ps; uint8_t pg, cost, key, pal, w, h, lap, pad; };
_Static_assert(sizeof(struct SprSlot) == 16, "SprSlot: 16 байт");
#define SPR_HDR ((struct SprHdr *)040000)
#define SPR_PAL ((struct SprPal *)040020)
#define SPR_SLOT ((struct SprSlot *)041100)
#define SPR_REC 043100u        /* 2 x 16 x {l, t, r, b}: прогоны draw_via_canvas */
#define SPR_LH 043500u         /* заголовки кадров конечностей (drawLimb) */
#define SPR_RING 051500u
#define LH_N 128
struct LimbHdr { uint16_t res, pos, foff; int16_t hdr[6]; uint8_t pg, cost, limb, skip, pad[2]; };
_Static_assert(sizeof(struct LimbHdr) == 24 && SPR_LH + LH_N * 24u == SPR_RING, "LimbHdr: место в странице 2");
#define LH ((struct LimbHdr *)SPR_LH)
#define SPR_END 0100000u
void spr_blit(uint16_t src, uint16_t msk, uint16_t w, uint16_t h, uint16_t dst, uint16_t dskip);

static uint8_t spr_can, spr_pl, spr_budget, spr_mk, lh_on;   /* spr_mk — строить можно */
static uint16_t spr_m11;

static void spr_on(void)
{
    spr_m11 = mem11;
    w1_map(SPR_PG);
    lh_on = 1;
}

static void spr_off(void)
{
    lh_on = 0;
    mem11 = spr_m11;   /* тень — первой: кадровое прерывание возвращает окно по ней */
    *(volatile uint16_t *)0177716 = spr_m11;
}

/* слот палитры с цветами P3.cpat; новый — вместо старейшего, его спрайты негодны */
static uint8_t spr_pal(void)
{
    const uint16_t *c = (const uint16_t *)P3_BUF->cpat;
    uint16_t sum = 0;
    for (uint8_t i = 0; i < 32; i++) sum = (uint16_t)(((sum << 1) | (sum >> 15)) + c[i]);
    struct SprPal *pp = SPR_PAL;
    for (uint8_t k = 0; k < SPR_NPAL; k++, pp++)
        if (pp->sum == sum)
        {
            const uint16_t *q = (const uint16_t *)pp->cpat;
            uint8_t i = 0;
            while (i < 32 && q[i] == c[i]) i++;
            if (i == 32) return k;
        }
    uint8_t k = SPR_HDR->prr;
    SPR_HDR->prr = (uint8_t)((k + 1) & (SPR_NPAL - 1));
    pp = &SPR_PAL[k];
    pp->sum = sum;
    uint16_t *q = (uint16_t *)pp->cpat;
    for (uint8_t i = 0; i < 32; i++) q[i] = c[i];
    for (uint8_t j = 0; j < SPR_SLOTS; j++)
        if (SPR_SLOT[j].pal == k) SPR_SLOT[j].h = 0;
    return k;
}

static uint8_t spr_valid(const struct SprSlot *q)
{
    return q->h && (q->lap == SPR_HDR->lap || ((uint8_t)(q->lap + 1) == SPR_HDR->lap && q->off >= SPR_HDR->top));
}

/* конечность (видна целиком по x, без маски глубины) в холст из кэша, строки — с
 * обрезкой по холсту; 0 — рисовать proc3. vx — первая колонка proc3, step — её шаг */
static uint8_t spr_draw(int16_t vx, int16_t vy, int8_t step)
{
    if ((uint16_t)width > 255 || (uint16_t)height > 255) return 0;
    int16_t wx = (step > 0 ? vx : vx - width + 1) + xstart;   /* левая колонка мира */
    uint16_t b0 = BK_X(wx), b1 = BK_X(wx + width - 1);
    if (b0 < tg.view0) return 0;
    uint16_t col = (b0 - tg.view0) >> 2;
    uint8_t w = (uint8_t)(((b0 & 3) + (b1 - b0) + 4) >> 2);
    if (col + w > tg.bxlim >> 2) return 0;
    uint8_t key = (uint8_t)((wx - div5[wx] * 5) | (b0 & 3) << 3 | (vy & 1) << 5 | mirror << 6);
    uint16_t foff = srcptr - c_res;
    uint8_t h = (uint8_t)(foff ^ (foff >> 5) ^ c_cost ^ (key << 2) ^ (spr_pl << 5));
    struct SprSlot *sl = 0, *fr = 0;
    for (uint8_t k = 0; k < SPR_WIN; k++)
    {
        struct SprSlot *q = &SPR_SLOT[(uint8_t)(h + k) & (SPR_SLOTS - 1)];
        if (!spr_valid(q))
        {
            if (!fr) fr = q;
        }
        else if (q->foff == foff && q->key == key && q->res == c_res && q->pg == c_page && q->cost == c_cost &&
                 q->pal == spr_pl)
        {
            sl = q;
            break;
        }
    }
    if (!sl)
    {
        uint16_t ps = ((uint16_t)w * (uint16_t)height + 1) & ~1u;   /* плоскость: чётная */
        if (!spr_mk || !spr_budget || ps > (SPR_END - SPR_RING) / 2) return 0;
        spr_budget--;
        if (!fr)
        {
            fr = &SPR_SLOT[(uint8_t)(h + SPR_HDR->rr) & (SPR_SLOTS - 1)];
            SPR_HDR->rr = (uint8_t)((SPR_HDR->rr + 1) & (SPR_WIN - 1));
        }
        sl = fr;
        if (SPR_HDR->top < SPR_RING || SPR_HDR->top + 2 * ps > SPR_END)
        {
            SPR_HDR->top = SPR_RING;
            if (!++SPR_HDR->lap)   /* слоты 256 кругов назад сочлись бы годными */
            {
                for (uint8_t j = 0; j < SPR_SLOTS; j++) SPR_SLOT[j].h = 0;
                SPR_HDR->lap = 1;
            }
        }
        sl->off = SPR_HDR->top;
        SPR_HDR->top += 2 * ps;
        for (uint16_t *z = (uint16_t *)sl->off, n = ps; n; n--) *z++ = 0;
        rle_read();
        struct Target sv = tg;
        tg.base = sl->off - (vy & 1 ? w : 0);
        tg.stride = w;
        tg.bxlim = (uint16_t)w * 4;
        tg.ylim = (uint16_t)height + (vy & 1);
        tg.view0 = b0 & ~3u;
        tg.y0 = vy & ~1;
        p3_bmoff = ps;
        proc3(vx, vy, width, step, band, 0, 0);
        p3_bmoff = 0;
        tg = sv;
        rle_used();
        sl->res = c_res;
        sl->foff = foff;
        sl->ps = ps;
        sl->pg = c_page;
        sl->cost = c_cost;
        sl->key = key;
        sl->pal = spr_pl;
        sl->w = w;
        sl->h = (uint8_t)height;
        sl->lap = SPR_HDR->lap;
    }
    int16_t r0 = tg.y0 - vy, r1 = tg.y0 + (int16_t)tg.ylim - vy;   /* строки спрайта в холсте */
    if (r0 < 0) r0 = 0;
    if (r1 > height) r1 = height;
    uint16_t so = (uint16_t)r0 * w;
    spr_blit(sl->off + so, sl->off + sl->ps + so, w, (uint16_t)(r1 - r0),
             tg.base + (uint16_t)(vy + r0 - tg.y0) * tg.stride + col, tg.stride - w);
    return 1;
}
#endif

/* costume.cpp: ClassicCostumeRenderer::mainRoutine (без масштабирования) */
static uint8_t mainRoutine(int16_t xmoveCur, int16_t ymoveCur)
{
    int16_t vx = actorX, vy = actorY, skip = 0, pre = 0;
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
    int16_t skip_width = width;
    int8_t step = mirror ? 1 : -1;
    if (mirror)
    {
        skip = -vx;
        if (skip > 0)
        {
            skip_width -= skip;
            pre = skip;   /* codec1_ignorePakCols */
            vx = 0;
        }
        else
        {
            skip = r - OUT_W;
            if (skip <= 0) drawFlag = 2;
            else skip_width -= skip;
        }
    }
    else
    {
        skip = r - OUT_W;
        if (skip > 0)
        {
            skip_width -= skip;
            pre = skip;
            vx = OUT_W - 1;
        }
        else
        {
            skip = -1 - l;
            if (skip <= 0) drawFlag = 2;
            else skip_width -= skip;
        }
    }
    if (skip_width <= 0) return 0;
    if (l < 0) l = 0;
    if (t < 0) t = 0;
    if (b > OUT_H) b = OUT_H;
    if (draw_top > t) draw_top = t;
    if (draw_bottom < b) draw_bottom = b;
    if (draw_left > l) draw_left = l;
    if (draw_right < r + 1) draw_right = r + 1;
    if (meas_rec)
    {
        int16_t *q = meas_rec + cur_limb * 4;
        q[0] = l;
        q[1] = t;
        q[2] = r;
        q[3] = b;
    }
    if (height + t >= 256) return 2;
    if (measuring) return drawFlag;
    if (vy >= tg.y0 + (int16_t)tg.ylim || vy + height <= tg.y0) return drawFlag;   /* вне строк цели */
#ifndef HOST
    if (spr_can && drawFlag == 2 && spr_draw(vx, vy, step)) return drawFlag;
#endif
    rle_read();
    const uint8_t *src = band;
    uint8_t replen = 0, repcolor = 0;
    if (pre) src = ignorePakCols(src, pre, &replen, &repcolor);
    proc3(vx, vy, skip_width, step, src, replen, repcolor);
    if (drawFlag == 2) rle_used();
    return drawFlag;
}

/* costume.cpp: ClassicCostumeRenderer::drawLimb */
static uint8_t drawLimb(struct Actor *a, uint8_t limb)
{
    if (a->cost_curpos[limb] == 0xFFFF || (a->cost_stopped & (1u << limb))) return 0;
    uint16_t i = a->cost_curpos[limb] & 0x7FFF;
    int16_t hdr[6];   /* width, height, смещения x/y кадра, сдвиг x/y */
#ifndef HOST
    /* заголовок кадра — из кэша (страница 2 подключена): без дальних чтений */
    struct LimbHdr *e = 0;
    if (lh_on)
    {
        e = &LH[(uint8_t)(i ^ (i >> 5) ^ (limb << 3) ^ c_cost) & (LH_N - 1)];
        if (e->pos == i && e->limb == limb && e->cost == c_cost && e->res == c_res && e->pg == c_page)
        {
            if (e->skip) return 0;
            srcptr = c_res + e->foff;
            for (uint8_t k = 0; k < 6; k++) hdr[k] = e->hdr[k];
            goto have;
        }
    }
#endif
    uint16_t frameptr = c_res + far_rw(c_page, c_frameOffsets + limb * 2);
    uint8_t code = far_rb(c_page, c_animCmds + i) & 0x7F;
#ifndef HOST
    if (e)
    {
        e->pos = i;
        e->limb = limb;
        e->cost = c_cost;
        e->res = c_res;
        e->pg = c_page;
        e->skip = code == 0x7B;
    }
#endif
    if (code == 0x7B) return 0;
    srcptr = c_res + far_rw(c_page, frameptr + code * 2);
    far_rd(hdr, c_page, srcptr, sizeof hdr);
#ifndef HOST
    if (e)
    {
        e->foff = srcptr - c_res;
        for (uint8_t k = 0; k < 6; k++) e->hdr[k] = hdr[k];
    }
have:
#endif
    width = hdr[0];
    height = hdr[1];
    int16_t xmoveCur = xmove + hdr[2];
    int16_t ymoveCur = ymove + hdr[3];
    xmove += hdr[4];
    ymove -= hdr[5];
    srcptr += 12;
    cur_limb = limb;
    return mainRoutine(xmoveCur, ymoveCur);
}

/* цвета актёра для сверки с прошлой отрисовкой (dr_pal) */
static uint16_t act_pal(const struct Actor *a)
{
    uint16_t s = (uint16_t)((VAR(VAR_CURRENT_LIGHTS) & 8) | (uint16_t)pal_cur << 4);
    for (uint8_t i = 0; i < 16; i++) s = (uint16_t)(((s << 1) | (s >> 15)) + a->palette[i]);
    return s;
}

/* actor.cpp: Actor::drawActorCostume + prepareDrawActorCostume (v2) */
static void drawActorCostume(struct Actor *a)
{
    if (a->costume == 0) return;
    if (!a->needRedraw) return;
    a->needRedraw = 0;
    if (!loadCostume(a->costume)) return;
    actorX = (int16_t)(a->x * V12_X_MULTIPLIER);
    actorY = (int16_t)((a->y - a->elevation) * V12_Y_MULTIPLIER);
    actorX -= xstart;
    if (a->facing == 270) actorX += 16;
    else actorX += 8;
    /* ClassicCostumeRenderer::setPalette (GF_OLD_BUNDLE): без LIGHTMODE_actor_use_colors
     * (темно) — силуэт: все цвета 8, цвет 12 — чёрный */
    if (VAR(VAR_CURRENT_LIGHTS) & 8)
        for (uint8_t i = 0; i < 16; i++) cpal[i] = a->palette[i];
    else
    {
        for (uint8_t i = 0; i < 16; i++) cpal[i] = 8;
        cpal[12] = 0;
    }
    cpal[c_palette0 & 15] = cpal[0];
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
    draw_top = draw_left = 0x7FFF;
    draw_bottom = draw_right = -0x7FFF;
    xmove = -72;
    ymove = -100;
    if (!measuring) cpat_build();
#ifndef HOST
    if (!measuring && tg.page && !zbuf)   /* в холст: кэш спрайтов */
    {
        spr_on();
        spr_can = 1;
        spr_pl = spr_pal();
        /* идущий (или при прокрутке) — без построения: фаза сжатия у каждого шага своя */
        spr_mk = !a->dr_cost || (a->dr_x == a->x && a->dr_y == a->y - a->elevation && a->dr_sss == screenStartStrip);
    }
#endif
    uint8_t result = 0;
    for (uint8_t i = 0; i < 16; i++) result |= drawLimb(a, i);
#ifndef HOST
    if (spr_can)
    {
        spr_can = 0;
        spr_off();
    }
#endif
    if (result & 1) a->needRedraw = 1;   /* _needRedraw = (version <= 6) */
    a->top = draw_top;
    a->bottom = draw_bottom;
    if (!measuring)
    {
        for (uint8_t i = 0; i < 16; i++) a->dr_pos[i] = a->cost_curpos[i];
        a->dr_stopped = a->cost_stopped;
        a->dr_pal = act_pal(a);
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
    if (vcol + *c1 > bm_stride) *c1 = bm_stride - vcol;
    return *c0 < *c1;
}

static void restoreRect(int16_t l, int16_t t, int16_t r, int16_t b)
{
    uint16_t c0, c1;
    cursor_hide_view(l, t, r, b);
    if (!rect_cols(l, &t, r, &b, &c0, &c1)) return;
    if (lt_mode)   /* темно: фон — чёрный, у фонарика — из фона (gfx.c) */
    {
        bg_vram(view0 >> 2, (uint16_t)t, (uint16_t)(b - t), c0, c1);
        return;
    }
    bm_blit(PAGE_ADDR + (uint16_t)t * bm_stride + (view0 >> 2) + c0, bm_stride,
            040000u + (uint16_t)(VIEW_ROW + t) * 64 + c0, c1 - c0, (uint16_t)(b - t));
}

/* Холст — внеэкранный буфер (PG_MASK, CANVAS): окно экрана -> буфер, фон под
 * прежними кадрами, новые кадры, буфер -> экран одной записью: без мерцания.
 * Строки — с чётным шагом cv_ns от cv_base той же чётности, что колонка экрана
 * (копии словами); верх окна — чётная строка (cpat_build) */
#define CANVAS 0152000u
#define CANVAS_SIZE 05400u
void cv_load(uint16_t cv, uint16_t vram_addr, uint16_t ncols, uint16_t nrows, uint16_t pad);
void cv_store(uint16_t cv, uint16_t vram_addr, uint16_t ncols, uint16_t nrows, uint16_t pad);
void cv_bg(uint16_t cv, uint16_t cvstride, uint16_t bm, uint16_t bmstride, uint16_t ncols, uint16_t nrows);

static uint16_t cv_nc, cv_ns, cv_base, cv_c0, cv_rows, cv_scr;
static int16_t cv_t;

/* окно [l, r) x [t, b) точек MM -> холст, цель — он; 0 — не помещается */
static uint8_t cv_open(int16_t l, int16_t t, int16_t r, int16_t b)
{
    uint16_t c0, c1;
    if (t & 1) t--;
    if (!rect_cols(l, &t, r, &b, &c0, &c1)) return 0;
    uint16_t nc = c1 - c0, nrows = (uint16_t)(b - t), ns = (nc + 1) & ~1u;
    if ((uint16_t)(nrows * ns) >= CANVAS_SIZE) return 0;   /* и байт выравнивания cv_base */
    cv_nc = nc;
    cv_ns = ns;
    cv_base = CANVAS + (c0 & 1);
    cv_c0 = c0;
    cv_rows = nrows;
    cv_t = t;
    cv_scr = 040000u + (uint16_t)(VIEW_ROW + t) * 64 + c0;
    cursor_hide_view(l, t, r, b);   /* в холст он попал бы с экрана */
    cv_load(cv_base, cv_scr, nc, nrows, ns - nc);
    tg.base = cv_base;
    tg.stride = ns;
    tg.bxlim = nc * 4;
    tg.ylim = nrows;
    tg.view0 = view0 + c0 * 4;
    tg.page = PG_MASK;
    tg.y0 = t;
    return 1;
}

/* фон под прямоугольником o (в пределах окна) — в холст */
static void cv_bgrect(const struct DrawnRect *o)
{
    int16_t ot = o->t < cv_t ? cv_t : o->t, ob = o->b > cv_t + (int16_t)cv_rows ? cv_t + (int16_t)cv_rows : o->b;
    uint16_t oc0, oc1;
    if (!rect_cols(o->l, &ot, o->r, &ob, &oc0, &oc1)) return;
    if (oc0 < cv_c0) oc0 = cv_c0;
    if (oc1 > cv_c0 + cv_nc) oc1 = cv_c0 + cv_nc;
    if (oc0 >= oc1) return;
    cv_bg(cv_base + (uint16_t)(ot - cv_t) * cv_ns + (oc0 - cv_c0), cv_ns,
          PAGE_ADDR + (uint16_t)ot * bm_stride + (view0 >> 2) + oc0, bm_stride, oc1 - oc0, (uint16_t)(ob - ot));
}

static void cv_close(void)
{
    target_screen();
    cv_store(cv_base, cv_scr, cv_nc, cv_rows, cv_ns - cv_nc);
}

static void rect_union(struct DrawnRect *u, const struct DrawnRect *q)
{
    if (q->r <= q->l || q->b <= q->t) return;
    if (q->l < u->l) u->l = q->l;
    if (q->r > u->r) u->r = q->r;
    if (q->t < u->t) u->t = q->t;
    if (q->b > u->b) u->b = q->b;
}

/* прямоугольник нового кадра (прогон без рисования) */
static void measure(struct Actor *a, struct DrawnRect *n)
{
    uint8_t nr = a->needRedraw;
    int16_t tp = a->top, bt = a->bottom;
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

#ifdef HOST
static int16_t rec_buf[2][64];
#define REC(p) (rec_buf[p])
#define spr_on()
#define spr_off()
#else
#define REC(p) ((int16_t *)(SPR_REC + (p) * 0200u))
#endif

static void rec_clear(int16_t *q)
{
    for (uint8_t i = 0; i < 16; i++, q += 4) q[1] = 0x7FFF;
}

/* актёр в своём окне (новый кадр и прежний o, если есть: фон под ним); 0 — окно
 * не помещается (рисовать на экране). part — на экране его прежний кадр (dr_*) на
 * целом фоне: если актёр на месте, окно — строки сменившихся конечностей (и
 * сдвинутых ими) в прежнем и новом кадре */
static uint8_t draw_via_canvas(struct Actor *a, const struct DrawnRect *o, uint8_t part)
{
    uint8_t same = part && o->r > o->l && a->dr_cost == a->costume && a->dr_x == a->x &&
                   a->dr_y == a->y - a->elevation && a->dr_facing == a->facing && a->dr_sss == screenStartStrip &&
                   a->dr_pal == act_pal(a);
    if (same)
    {
        uint8_t m = a->cost_stopped != a->dr_stopped;
        for (uint8_t i = 0; i < 16 && !m; i++)
            if (a->cost_curpos[i] != a->dr_pos[i]) m = 1;
        if (!m)   /* на экране всё как есть */
        {
            a->needRedraw = 0;
            draw_left = o->l;
            draw_right = o->r;
            return 1;
        }
    }
    struct DrawnRect w, w1;
    int16_t bt0 = 0x7FFF, bb0 = -1;
    spr_on();   /* записи прогонов — в странице кэша */
    meas_rec = REC(0);
    rec_clear(meas_rec);
    measure(a, &w);
    if (same)   /* прогон по прежнему кадру: обмен с dr_pos туда и обратно */
    {
        meas_rec = REC(1);
        rec_clear(meas_rec);
        for (uint8_t i = 0; i < 16; i++)
        {
            uint16_t v = a->cost_curpos[i];
            a->cost_curpos[i] = a->dr_pos[i];
            a->dr_pos[i] = v;
        }
        uint16_t v = a->cost_stopped;
        a->cost_stopped = a->dr_stopped;
        a->dr_stopped = v;
        measure(a, &w1);
        for (uint8_t i = 0; i < 16; i++)
        {
            v = a->cost_curpos[i];
            a->cost_curpos[i] = a->dr_pos[i];
            a->dr_pos[i] = v;
        }
        v = a->cost_stopped;
        a->cost_stopped = a->dr_stopped;
        a->dr_stopped = v;
        const int16_t *n = REC(0), *q = REC(1);
        for (uint8_t i = 0; i < 16; i++, n += 4, q += 4)
        {
            if (a->cost_curpos[i] == a->dr_pos[i] && !(((a->cost_stopped ^ a->dr_stopped) >> i) & 1) &&
                n[0] == q[0] && n[1] == q[1] && n[2] == q[2] && n[3] == q[3])
                continue;
            if (n[1] != 0x7FFF)
            {
                if (n[1] < bt0) bt0 = n[1];
                if (n[3] > bb0) bb0 = n[3];
            }
            if (q[1] != 0x7FFF)
            {
                if (q[1] < bt0) bt0 = q[1];
                if (q[3] > bb0) bb0 = q[3];
            }
        }
    }
    meas_rec = 0;
    spr_off();
    if (same && bb0 < 0)   /* сменились только невидимые конечности */
    {
        draw_left = w.l;
        draw_right = w.r;
        return 1;
    }
    if (w.r <= w.l || w.b <= w.t) w = *o;
    else rect_union(&w, o);
    if (same)
    {
        if (w.t < bt0) w.t = bt0;
        if (w.b > bb0) w.b = bb0;
    }
    if (w.r <= w.l || w.b <= w.t || !cv_open(w.l, w.t, w.r, w.b)) return 0;
    if (o->r > o->l) cv_bgrect(o);
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

/* в общий холст: перерисовываемые с прежним кадром, пересекающиеся с кем-то */
#define GRP(i) (R[i] && drawn[i].r > drawn[i].l && late[i].r <= late[i].l)

void processActors(void)
{
    xstart = (int16_t)(screenStartStrip * 8);
    view0 = bk_x(xstart) & ~3u;
    target_screen();
#ifndef HOST
    spr_budget = SPR_BUILDS;
#endif
    uint8_t flushed = gfx_view_flush();
    uint8_t view_redrawn = flushed == 1;   /* вся видимая часть: старых актёров уже нет */
    /* актёры, которых касается кадр: в комнате или с прошлым кадром на экране */
    uint8_t L[NUM_ACTORS], nl = 0, R[NUM_ACTORS], P[NUM_ACTORS], any = 0;
    for (uint8_t i = 1; i < NUM_ACTORS; i++)
    {
        struct Actor *a = &actors[i];
        uint8_t was = drawn[i].r > drawn[i].l;
        if (a->room != currentRoom && !was) continue;
        L[nl++] = i;
        R[i] = P[i] = flushed != 0 || (a->room == currentRoom && a->visible && a->needRedraw) || (was && a->needBgReset);
        any |= R[i];   /* P — сменившиеся сами (без задетых ими) */
    }
    if (!any)
    {
        animate_only();
        return;
    }
    uint8_t cv = !lt_mode;   /* в темноте — на экране (фон — bg_vram) */
    if (!cv) cursor_hide();
    /* фон под R затрёт пересекающихся с ними */
    for (uint8_t grown = 1; grown;)
    {
        grown = 0;
        for (uint8_t p = 0; p < nl; p++)
            if (R[L[p]])
                for (uint8_t q = 0; q < nl; q++)
                    if (!R[L[q]] && rect_overlap(&drawn[L[p]], &drawn[L[q]])) R[L[q]] = grown = 1;
    }
    /* актёр, ни с кем не пересекавшийся, — в своём окне (фон под прежним кадром —
     * там же или прямо перед отрисовкой) */
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
    /* пересекавшиеся — в общем холсте (R = 2): окно — прежние и новые кадры
     * сменившихся (P), фон под их прежними кадрами; задетые ими соседи рисуются в
     * нём с обрезкой (вне окна на экране они те же) */
    uint8_t grp = 0;
    if (cv && !view_redrawn)
    {
        struct DrawnRect u = { 0x7FFF, 0x7FFF, -0x7FFF, -0x7FFF }, nw;
        for (uint8_t p = 0; p < nl; p++)
        {
            uint8_t i = L[p];
            if (!GRP(i)) continue;
            grp = 1;
            if (!P[i]) continue;
            rect_union(&u, &drawn[i]);
            if (actors[i].room == currentRoom && actors[i].visible)
            {
                spr_on();
                measure(&actors[i], &nw);
                spr_off();
                rect_union(&u, &nw);
            }
        }
        if (grp && u.r > u.l && cv_open(u.l, u.t, u.r, u.b))
        {
            for (uint8_t p = 0; p < nl; p++)
            {
                uint8_t i = L[p];
                if (!GRP(i)) continue;
                if (P[i]) cv_bgrect(&drawn[i]);
                R[i] = 2;
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
            if (R[sorted[k]] != 2 || !a->costume || !a->visible) continue;
            a->needRedraw = 1;
            drawActorCostume(a);
            mark_drawn(sorted[k], a);
        }
        cv_close();
    }
    static const struct DrawnRect none;
    for (uint8_t k = 0; k < n; k++)
    {
        uint8_t num = sorted[k];
        struct Actor *a = &actors[num];
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
                if (cv) done = draw_via_canvas(a, &late[num], !flushed);
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
                if (cv) done = draw_via_canvas(a, &none, 0);
                if (!done)
                {
                    cursor_hide();
                    drawActorCostume(a);
                }
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
        actors[i].dr_cost = 0;
    }
    for (uint8_t s = 0; s < 40; s++) strip_usage[s][0] = strip_usage[s][1] = strip_usage[s][2] = strip_usage[s][3] = 0;
}
