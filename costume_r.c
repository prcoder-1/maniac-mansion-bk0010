/* Отрисовка актёров (банк B): перевод ClassicCostumeRenderer (costume.cpp:
 * drawLimb / mainRoutine / proc3, base-costume.cpp: drawCostume /
 * codec1_ignorePakCols) и Actor_v2::prepareDrawActorCostume (actor.cpp) для
 * костюмов v2 (формат 0x58, GF_OLD_BUNDLE, без масштабирования).
 *
 * Координаты — точки главного виртуального экрана MM (320x128). Колонка экрана x
 * (мир x + xstart) с x_мира%5 == 4 пропускается, остальные встают в точку БК
 * bx_мира - view0 (view0 — начало видимой части, кратное 4). Кадр конечности
 * (RLE) копируется в _band; proc3 (proc3.s) читает z-маску прямо в её
 * странице и пишет в видеопамять. */
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

/* цвет EGA -> 2x2 цветов БК (как фон, gfx_v2.c) */
static const uint8_t pat[16][4] =
{
    {0, 0, 0, 0}, {1, 1, 1, 1}, {2, 2, 2, 2}, {1, 2, 2, 1},
    {3, 3, 3, 3}, {3, 1, 1, 3}, {3, 2, 0, 3}, {1, 2, 2, 1},
    {1, 0, 0, 2}, {1, 1, 1, 1}, {2, 2, 2, 2}, {1, 2, 2, 1},
    {3, 3, 3, 3}, {3, 1, 1, 3}, {3, 2, 2, 3}, {1, 2, 2, 1},
};
static const uint8_t pix_mask[4] = { 03, 014, 060, 0300 };
static const uint8_t pix_val[4][4] = { {0, 1, 2, 3}, {0, 4, 010, 014}, {0, 020, 040, 060}, {0, 0100, 0200, 0300} };
static const uint8_t revBitMask[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

/* состояние рендерера (ClassicCostumeRenderer) */
static int16_t actorX, actorY, xmove, ymove, width, height;
static uint8_t mirror, zbuf, cpal[16];
static uint8_t c_page;
static uint16_t c_res, c_frameOffsets, c_animCmds;
static uint8_t c_palette0;
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
    c_page = res_get(rtCostume, id);
    c_res = res_addr;
    if (!c_page) return 0;
    uint8_t format = far_rb(c_page, c_res + 5) & 0x7F;
    c_palette0 = far_rb(c_page, c_res + 6);
    uint16_t ptr = c_res - 2 + 8 + (format == 0x57 ? 0 : 1);
    c_frameOffsets = ptr + 2;
    c_animCmds = c_res + far_rw(c_page, ptr);
    return 1;
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
    void (*poll)(void);     /* 032: snd_poll или 0 */
    uint8_t cpat[16][4];    /* 034: цвет -> [(y&1)<<1 | (bx&1)] цвет БК */
};
#ifndef HOST
_Static_assert(__builtin_offsetof(struct P3, cpat) == 034, "struct P3: смещения в proc3.s");
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
            if (color && (uint16_t)x < OUT_W && (uint16_t)y < OUT_H)
            {
                int16_t wx = x + p->xstart;
                uint16_t bx = BK_X(wx) - p->view0;
                uint8_t masked = p->zbuf &&
                    (far_rb(PG_MASK, (uint16_t)(p->mrow0 + (y - p->vy) * p->mstride + wx / 8)) & revBitMask[wx & 7]);
                if (wx - div5[wx] * 5 != 4 && bx < 256 && !masked)
                {
                    uint8_t k = (uint8_t)(bx & 3);
                    uint8_t c = p->cpat[color][((y & 1) << 1) | (bx & 1)];
                    uint16_t a = (uint16_t)((VIEW_ROW + y) * 64 + (bx >> 2));
                    vram[a] = (uint8_t)((vram[a] & ~pix_mask[k]) | pix_val[k][c]);
                }
            }
            y++;
            if (!--h)
            {
                if (!--skip) return;
                if (p->poll) p->poll();
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
    p->vy = v_y;
    p->h = (uint16_t)height;
    p->skip = (uint16_t)skip_width;
    p->step = step;
    p->x = v_x;
    p->xstart = xstart;
    p->view0 = view0;
    p->zbuf = zbuf;
    p->mrow0 = (uint16_t)(PAGE_ADDR + v_y * (int16_t)bm_strips);
    p->mstride = bm_strips;
#ifdef HOST
    p->poll = 0;
#else
    p->poll = snd_active ? snd_poll : 0;
#endif
    for (uint8_t c = 0; c < 16; c++)
    {
        const uint8_t *q = pat[cpal[c] & 15];
        p->cpat[c][0] = q[0];
        p->cpat[c][1] = q[1];
        p->cpat[c][2] = q[2];
        p->cpat[c][3] = q[3];
    }
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

/* costume.cpp: ClassicCostumeRenderer::mainRoutine (без масштабирования) */
static uint8_t mainRoutine(int16_t xmoveCur, int16_t ymoveCur)
{
    int16_t vx = actorX, vy = actorY, skip = 0;
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
    uint8_t replen = 0, repcolor = 0;
    /* RLE кадра: в _band (ScummVM читает его на месте) */
    uint16_t n = sizeof band;
    if ((uint32_t)srcptr + n > PAGE_ADDR + PAGE_DATA_SIZE) n = (uint16_t)(PAGE_ADDR + PAGE_DATA_SIZE - srcptr);
    far_rd(band, c_page, srcptr, n);
    const uint8_t *src = band;
    if (mirror)
    {
        skip = -vx;
        if (skip > 0)
        {
            skip_width -= skip;
            src = ignorePakCols(src, skip, &replen, &repcolor);
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
            src = ignorePakCols(src, skip, &replen, &repcolor);
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
    if (height + t >= 256) return 2;
    proc3(vx, vy, skip_width, step, src, replen, repcolor);
    return drawFlag;
}

/* costume.cpp: ClassicCostumeRenderer::drawLimb */
static uint8_t drawLimb(struct Actor *a, uint8_t limb)
{
    if (a->cost_curpos[limb] == 0xFFFF || (a->cost_stopped & (1u << limb))) return 0;
    uint16_t i = a->cost_curpos[limb] & 0x7FFF;
    uint16_t frameptr = c_res + far_rw(c_page, c_frameOffsets + limb * 2);
    uint8_t code = far_rb(c_page, c_animCmds + i) & 0x7F;
    if (code == 0x7B) return 0;
    srcptr = c_res + far_rw(c_page, frameptr + code * 2);
    width = (int16_t)far_rw(c_page, srcptr);
    height = (int16_t)far_rw(c_page, srcptr + 2);
    int16_t xmoveCur = xmove + (int16_t)far_rw(c_page, srcptr + 4);
    int16_t ymoveCur = ymove + (int16_t)far_rw(c_page, srcptr + 6);
    xmove += (int16_t)far_rw(c_page, srcptr + 8);
    ymove -= (int16_t)far_rw(c_page, srcptr + 10);
    srcptr += 12;
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
    actorY = (int16_t)((a->y - a->elevation) * V12_Y_MULTIPLIER);
    actorX -= xstart;
    if (a->facing == 270) actorX += 16;
    else actorX += 8;
    /* setPalette (GF_OLD_BUNDLE, свет для актёров включён) */
    for (uint8_t i = 0; i < 16; i++) cpal[i] = a->palette[i];
    cpal[c_palette0 & 15] = cpal[0];
    /* setFacing */
    int16_t f = a->facing;
    uint8_t d = 3;
    if (f >= 71 && f <= 109) d = 1;
    else if (f >= 109 && f <= 251) d = 2;
    else if (f >= 251 && f <= 289) d = 0;
    uint8_t fmt = far_rb(c_page, c_res + 5);
    mirror = d != 0 || (fmt & 0x80);
    /* _zbuf: маска из бокса (v3 и ниже — не больше 1) */
    zbuf = getMaskFromBox(a->walkbox);
    if (zbuf > 1) zbuf = 1;
    draw_top = draw_left = 0x7FFF;
    draw_bottom = draw_right = -0x7FFF;
    xmove = -72;
    ymove = -100;
    uint8_t result = 0;
    for (uint8_t i = 0; i < 16; i++) result |= drawLimb(a, i);
    if (result & 1) a->needRedraw = 1;   /* _needRedraw = (version <= 6) */
    a->top = draw_top;
    a->bottom = draw_bottom;
}

/* gfxUsageBits для getActorFromPos */
uint8_t actor_in_strip(int16_t strip, uint8_t act)
{
    int16_t s = strip - screenStartStrip;
    if (s < 0 || s >= 40) return 0;
    return (strip_usage[s][act >> 3] >> (act & 7)) & 1;
}

/* восстановить фон экрана в прямоугольнике точек MM (x: 0..319, y: 0..127) */
static void restoreRect(int16_t l, int16_t t, int16_t r, int16_t b)
{
    if (l < 0) l = 0;
    if (r > OUT_W) r = OUT_W;
    if (t < 0) t = 0;
    if (b > OUT_H) b = OUT_H;
    if (l >= r || t >= b) return;
    uint16_t c0 = (uint16_t)((bk_x(l + xstart) - view0) >> 2);
    uint16_t c1 = (uint16_t)((bk_x(r - 1 + xstart) - view0) >> 2) + 1;
    if (c1 > 64) c1 = 64;
    uint16_t vcol = view0 >> 2;
    if (vcol + c1 > bm_stride) c1 = bm_stride - vcol;
    if (c0 >= c1) return;
    bm_blit(PAGE_ADDR + (uint16_t)t * bm_stride + vcol + c0, bm_stride,
            040000u + (uint16_t)(VIEW_ROW + t) * 64 + c0, c1 - c0, (uint16_t)(b - t));
}

/* actor.cpp: ScummEngine::processActors с resetActorBgs/setActorRedrawFlags:
 * при изменениях — фон под прошлыми актёрами, затем все актёры комнаты по y */
void processActors(void)
{
    xstart = (int16_t)(screenStartStrip * 8);
    view0 = bk_x(xstart) & ~3u;
    uint8_t flushed = gfx_view_flush();
    uint8_t view_redrawn = flushed == 1;   /* вся видимая часть: старых актёров уже нет */
    uint8_t any = flushed != 0;
    for (uint8_t i = 1; i < NUM_ACTORS; i++)
    {
        struct Actor *a = &actors[i];
        if ((a->room == currentRoom && a->visible && a->needRedraw) || (drawn[i].r > drawn[i].l && a->needBgReset))
            any = 1;
    }
    if (!any)
    {
        animate_only();
        return;
    }
    for (uint8_t i = 1; i < NUM_ACTORS; i++)
    {
        if (!view_redrawn && drawn[i].r > drawn[i].l) restoreRect(drawn[i].l, drawn[i].t, drawn[i].r, drawn[i].b);
        drawn[i].l = drawn[i].r = 0;
        actors[i].needBgReset = 0;
    }
    for (uint8_t s = 0; s < 40; s++) strip_usage[s][0] = strip_usage[s][1] = strip_usage[s][2] = strip_usage[s][3] = 0;
    uint8_t sorted[NUM_ACTORS], n = 0;
    for (uint8_t i = 1; i < NUM_ACTORS; i++)
        if (actors[i].room == currentRoom) sorted[n++] = i;
    for (uint8_t j = 0; j < n; ++j)
        for (uint8_t i = 0; i < n; ++i)
            if (actors[sorted[j]].y < actors[sorted[i]].y)
            {
                uint8_t t = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = t;
            }
    for (uint8_t k = 0; k < n; k++)
    {
        struct Actor *a = &actors[sorted[k]];
        if (!a->costume) continue;
        if (a->visible)
        {
            a->needRedraw = 1;   /* setActorRedrawFlags: фон под ним восстановлен */
            drawActorCostume(a);
            if (a->bottom > a->top && draw_right > draw_left)
            {
                struct DrawnRect *d = &drawn[a->number];   /* markRectAsDirty всех конечностей */
                d->l = draw_left;
                d->r = draw_right;
                d->t = a->top;
                d->b = a->bottom;
                for (int16_t s = d->l / 8; s < (d->r + 7) / 8; s++)
                    if (s >= 0 && s < 40) strip_usage[s][a->number >> 3] |= (uint8_t)(1u << (a->number & 7));
            }
        }
        animateCostume(a);
    }
}
