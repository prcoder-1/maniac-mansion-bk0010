/* Модуль AZБК: актёры в слое 2 (вызов — из processActors банка B, он подключён: struct
 * az_actx). Перевод ClassicCostumeRenderer (costume.cpp: drawLimb / mainRoutine / proc3)
 * для костюмов v2, как costume_r.c, но в 4 бита на точку и без сжатия 4:5.
 *
 * Координаты — точки и строки комнаты (слой 2 — в них же): актёры стоят в слое на своих
 * местах, прокрутка их не трогает. Шаг: полосы (8 точек), где актёр был или стал, —
 * «грязные»; на каждый их отрезок — холст в памяти AZ (строки — от верха до низа
 * задетых прямоугольников): фон — блиттером из склада, все актёры, задевающие холст, —
 * процессором от дальних к ближним (z-маска — PG_MASK), холст -> слой 2 — блиттером в
 * пакете шага (вместе с прокруткой и полосами фона). Конечность — спрайтом из кэша в
 * памяти AZ (плоскости цвета и маски по словам; кадр, зеркало, цвета); сдвиг внутри
 * слова от места не зависит: x актёра кратен 8.
 *
 * Данные банка B (актёры, struct az_actx) лежат и выше 0140000: пока окна 12/13 или
 * страница СМК заняты (холст, спрайт, PG_MASK), их не трогать — сначала win_back. */
#include "actor.h"
#include "gfx.h"
#include "az.h"
#include "azm.h"

extern uint8_t band[2048];          /* sys.ld: RLE кадра */
extern uint8_t boxes[BOXES_SIZE];   /* sys.ld: боксы комнаты (room.c) */

struct ARect { int16_t l, t, r, b; };   /* точки [l, r) и строки [t, b) комнаты */
static struct ARect dr[NUM_ACTORS];    /* нарисованное в слое 2 */
static uint8_t ndr;                    /* сколько их */

/* --- костюм --- */
static uint8_t c_page, c_cost, c_pal0, c_fmt;
static uint16_t c_res, c_fo, c_ac;
static uint8_t cpal[16] __attribute__((aligned(2)));   /* сравнение словами */
static uint8_t pal_id;              /* слот цветов (кэш спрайтов) */
static int16_t ax, ay, xm, ym;      /* якорь актёра (точки комнаты), xmove, ymove */
static uint8_t mir, zb;
static int16_t vx0;                 /* точка комнаты левого края вида */
static int16_t r_l, r_t, r_r, r_b;  /* прямоугольник конечностей (как draw_* у ScummVM) */
static uint8_t res_flags;           /* | drawFlag конечностей */

static uint8_t load_cost(const struct az_actx *x, uint8_t id)
{
    const struct CostInfo *ci = x->cost_info(id);
    az_w12 = az_w13 = 0;   /* мог читать ресурс: окна — от СМК */
    if (!ci) return 0;
    c_page = ci->page;
    c_cost = id;
    c_res = ci->res;
    c_fmt = ci->fmt;
    c_pal0 = ci->palette0;
    c_fo = ci->ptr + 2;
    c_ac = ci->animCmds;
    return 1;
}

/* --- заголовки кадров конечностей: кэш без дальних чтений --- */
#define LH_N 64
struct LH { uint16_t pos, res, foff; uint8_t limb, cost, pg, skip; int16_t hdr[6]; uint16_t pad[5]; };
_Static_assert(sizeof(struct LH) == 32, "struct LH: индекс — сдвигом");
static struct LH lh[LH_N];
static uint16_t srcptr;
static const uint8_t bit8[8] = { 1, 2, 4, 010, 020, 040, 0100, 0200 };

static const int16_t *limb_hdr(uint8_t limb, uint16_t i)
{
    /* ячейка — смещением кратно 32 (без сдвигов на 5: у -m10 они — циклы) */
    struct LH *e = (struct LH *)((uint8_t *)lh + ((((i ^ c_cost) << 3) ^ ((uint16_t)limb << 8) ^ (i >> 8)) & ((LH_N - 1) << 5)));
    if (e->pos != i || e->limb != limb || e->cost != c_cost || e->res != c_res || e->pg != c_page)
    {
        e->pos = i;
        e->limb = limb;
        e->cost = c_cost;
        e->res = c_res;
        e->pg = c_page;
        uint16_t frameptr = c_res + far_rw(c_page, c_fo + limb * 2);
        uint8_t code = far_rb(c_page, c_ac + i) & 0x7F;
        e->skip = code == 0x7B;
        az_w12 = az_w13 = 0;   /* дальние чтения вернули окна СМК */
        if (e->skip) return 0;
        uint16_t s = c_res + far_rw(c_page, frameptr + code * 2);
        e->foff = s - c_res;
        far_rd(e->hdr, c_page, s, sizeof e->hdr);
        az_w12 = az_w13 = 0;   /* дальние чтения вернули окна СМК */
    }
    if (e->skip) return 0;
    srcptr = c_res + e->foff;
    return e->hdr;
}

/* --- цвета: слоты палитр для ключа спрайта --- */
#define PAL_N 16
static uint8_t pals[PAL_N][16] __attribute__((aligned(2))), npals;
static uint8_t pal_slot(void)
{
    for (uint8_t k = 0; k < npals; k++)
    {
        const uint16_t *a = (const uint16_t *)pals[k], *b = (const uint16_t *)cpal;
        uint8_t i = 0;
        while (i < 8 && a[i] == b[i]) i++;
        if (i == 8) return k;
    }
    return 0xFF;
}

/* --- кэш спрайтов в памяти AZ (страницы AZ_SPR..): спрайт в одной странице 4 Кбайт;
 * переполнение — кэш заново --- */
#define SPR_N 128
#define AZ_SPR 02000u
#define AZ_SPR_END 017000u
struct Spr { uint16_t foff, res, page4, off, plane; uint8_t pg, cost, key, ws, h, pad; };   /* plane — байт */
static struct Spr spr[SPR_N];
static uint16_t spr_pg = AZ_SPR, spr_off;   /* где строить следующий */
static uint8_t spr_rr;                     /* вытеснение в четвёрке — по кругу */

/* az_spr.s */
struct SD { const uint8_t *src; uint16_t w, h, ws2, plane, dst; int16_t sx, dx; uint16_t tbl[4][16]; };
struct SP { uint16_t src, plane, nw, rows, sskip, dst, dskip, mrow, mstr, half; };
void spr_dec(struct SD *p);
void spr_mix(struct SP *p);
static struct SD sd;
static struct SP sp;
static uint8_t sd_pal = 0xFF;   /* для какого слота цветов sd.tbl */

static void spr_reset(void)
{
    for (uint16_t i = 0; i < SPR_N; i++) spr[i].ws = 0;
    spr_pg = AZ_SPR;
    spr_off = 0;
    npals = 0;
    sd_pal = 0xFF;
}

/* спрайт кадра srcptr: RLE (rle — в _band) -> плоскости цвета и маски ws слов x h
 * строк, колонка 0 кадра — в тетраде ph; 0 — не влез */
static struct Spr *spr_build(struct Spr *e, uint8_t key, uint8_t ph, int16_t w, int16_t h)
{
    uint8_t ws = (uint8_t)((ph + w + 3) >> 2);
    uint16_t sz = (uint16_t)ws * (uint16_t)h * 4;   /* две плоскости */
    if (sz > 4096) return 0;
    if (spr_off + sz > 4096)
    {
        spr_off = 0;
        if (++spr_pg >= AZ_SPR_END) spr_reset();
    }
    /* RLE кадра -> _band: байт RLE — не меньше точки */
    uint16_t n = (uint16_t)w * (uint16_t)h + 2;
    if (n > sizeof band) n = sizeof band;
    if ((uint32_t)srcptr + n > PAGE_ADDR + PAGE_DATA_SIZE) n = (uint16_t)(PAGE_ADDR + PAGE_DATA_SIZE - srcptr);
    far_rd(band, c_page, srcptr, n);
    az_w12 = az_w13 = 0;   /* дальнее чтение вернуло окна СМК */
    win13(spr_pg);
    sd.src = band;
    sd.w = (uint16_t)w;
    sd.h = (uint16_t)h;
    sd.ws2 = (uint16_t)(ws * 2);
    sd.plane = sz >> 1;
    sd.dst = 0150000u + spr_off;
    sd.sx = (int16_t)(ph + (mir ? 0 : w - 1));
    sd.dx = mir ? 1 : -1;
    if (sd_pal != pal_id)   /* цвета в тетрадах 0..3 */
    {
        sd_pal = pal_id;
        for (uint8_t c = 1; c < 16; c++)
        {
            uint16_t v = cpal[c];
            for (uint8_t s = 0; s < 4; s++, v <<= 4) sd.tbl[s][c] = v;
        }
    }
    spr_dec(&sd);
    e->foff = srcptr - c_res;
    e->res = c_res;
    e->pg = c_page;
    e->cost = c_cost;
    e->key = key;
    e->ws = ws;
    e->h = (uint8_t)h;
    e->page4 = spr_pg;
    e->off = spr_off;
    e->plane = sz >> 1;
    spr_off += sz;
    return e;
}

/* --- холст: окно 12, слова от точки комнаты cx0, строки от ct --- */
static int16_t cx0, ct, cw, chh;   /* точка, строка, ширина в словах, строк */
static uint16_t c_pg;              /* страница AZ холста */

extern const uint16_t az_ex4[16];
static uint16_t mrow[128];    /* адрес строки z-маски (PG_MASK) */
static uint8_t mrow_n;        /* для какой ширины маски построена */
/* спрайт e в холст: левое слово спрайта — слово комнаты sw, верх — строка sy */
static void spr_put(const struct Spr *e, int16_t sw, int16_t sy)
{
    int16_t w0 = sw, w1 = sw + e->ws, y0 = sy, y1 = sy + e->h;
    int16_t cw0 = cx0 >> 2;
    if (w0 < cw0) w0 = cw0;
    if (w1 > cw0 + cw) w1 = cw0 + cw;
    if (y0 < ct) y0 = ct;
    if (y1 > ct + chh) y1 = ct + chh;
    if (w0 >= w1 || y0 >= y1) return;
    if (zb) smk_to(PG_MASK);   /* z-маска — в окнах 8..11 (актёр — уже в a_pos) */
    win12(c_pg);
    win13(e->page4);
    uint16_t ws = e->ws, nw = (uint16_t)(w1 - w0);
    sp.src = 0150000u + e->off + ((uint16_t)(y0 - sy) * ws + (uint16_t)(w0 - sw)) * 2;
    sp.plane = e->plane;
    sp.nw = nw;
    sp.rows = (uint16_t)(y1 - y0);
    sp.sskip = (ws - nw) * 2;
    sp.dst = 0140000u + ((uint16_t)(y0 - ct) * (uint16_t)cw + (uint16_t)(w0 - cw0)) * 2;
    sp.dskip = ((uint16_t)cw - nw) * 2;
    sp.mrow = zb ? (uint16_t)(mrow[y0] + ((uint16_t)w0 >> 1)) : 0;
    sp.mstr = bm_strips;
    sp.half = (uint16_t)w0 & 1;
    spr_mix(&sp);
}

/* --- конечности (costume.cpp: drawLimb, mainRoutine) --- */
static uint8_t drawing;   /* 0 — только прямоугольник (measure) */

static uint8_t main_routine(int16_t xmc, int16_t ymc, int16_t w, int16_t h)
{
    if (!mir) xmc = -xmc;
    int16_t vx = ax + xmc, vy = ay + ymc;
    int16_t l = mir ? vx : vx - w, r = mir ? vx + w : vx, t = vy, b = t + h;
    /* вид: точки [vx0, vx0 + 320), строки [0, 128) */
    int16_t lv = l - vx0, rv = r - vx0, xv = vx - vx0;
    if (t >= 128 || b <= 0 || lv >= 320 || rv <= 0) return 0;
    int16_t skip_width = w;
    uint8_t flag = 1;
    if (mir)
    {
        if (xv < 0) skip_width += xv;
        else if (rv - 320 <= 0) flag = 2;
        else skip_width -= rv - 320;
    }
    else
    {
        if (rv - 320 > 0) skip_width -= rv - 320;
        else if (-1 - lv <= 0) flag = 2;
        else skip_width -= -1 - lv;
    }
    if (skip_width <= 0) return 0;
    if (lv < 0) l = vx0;
    if (t < 0) t = 0;
    if (b > 128) b = 128;
    if (r_t > t) r_t = t;
    if (r_b < b) r_b = b;
    if (r_l > l) r_l = l;
    if (r_r < r + 1) r_r = r + 1;
    if (h + t >= 256) return 2;
    if (!drawing) return flag;
    /* в холст: колонки кадра — [xl, xl + w) */
    int16_t xl = mir ? vx : vx - w + 1;
    if (xl >= cx0 + cw * 4 || xl + w <= cx0 || vy >= ct + chh || vy + h <= ct) return flag;
    uint8_t ph = (uint8_t)(xl & 3);
    uint8_t key = (uint8_t)(mir | ph << 1 | pal_id << 3);
    uint16_t foff = srcptr - c_res;
    /* 4 ячейки на ключ: занятая этим кадром, иначе пустая или по кругу */
    struct Spr *e = &spr[(uint8_t)(foff ^ (foff >> 6) ^ (c_cost << 2) ^ (key << 4)) & (SPR_N - 4)], *f = 0;
    for (uint8_t j = 0; j < 4; j++, e++)
    {
        if (!e->ws)
        {
            if (!f) f = e;
            continue;
        }
        if (e->foff == foff && e->key == key && e->res == c_res && e->pg == c_page && e->cost == c_cost) goto hit;
    }
    e -= 4;
    e = spr_build(f ? f : e + (spr_rr++ & 3), key, ph, w, h);
    if (!e) return flag;
hit:
    spr_put(e, (xl - ph) >> 2, vy);
    return flag;
}

/* кадры конечностей актёра: копия — банк B при рисовании отключается (PG_MASK) */
static uint16_t a_pos[16], a_stopped;

static uint8_t draw_limb(uint8_t limb)
{
    if (a_pos[limb] == 0xFFFF) return 0;
    uint16_t i = a_pos[limb] & 0x7FFF;
    const int16_t *hdr = limb_hdr(limb, i);
    if (!hdr) return 0;
    int16_t xmc = xm + hdr[2], ymc = ym + hdr[3];
    xm += hdr[4];
    ym -= hdr[5];
    srcptr += 12;
    return main_routine(xmc, ymc, hdr[0], hdr[1]);
}

/* room.c getMaskFromBox без дальнего вызова: боксы — в базовом ОЗУ */
static uint8_t box_mask(uint8_t box)
{
    if (box == 255) return 1;
    if (boxes[0] == box) box--;
    if (box >= boxes[0]) return getMaskFromBox(box);
    return boxes[(uint16_t)box * 8 + 7];
}

/* actor.cpp: prepareDrawActorCostume + drawActorCostume (v2); с drawing — в холст
 * (страница PG_MASK уже подключена; конечности — после чтения заголовков) */
static uint8_t actor_run(const struct az_actx *x, struct Actor *a)
{
    if (!load_cost(x, a->costume)) return 0;
    ax = (int16_t)(a->x * V12_X_MULTIPLIER + (a->facing == 270 ? 16 : 8));
    ay = (int16_t)((a->y - a->elevation) * V12_Y_MULTIPLIER);
    int16_t f = a->facing;
    uint8_t d = 3;
    if (f >= 71 && f <= 109) d = 1;
    else if (f >= 109 && f <= 251) d = 2;
    else if (f >= 251 && f <= 289) d = 0;
    mir = d != 0 || (c_fmt & 0x80);
    r_t = r_l = 0x7FFF;
    r_b = r_r = -0x7FFF;
    xm = -72;
    ym = -100;
    for (uint8_t i = 0; i < 16; i++) a_pos[i] = a->cost_curpos[i];
    a_stopped = a->cost_stopped;
    uint8_t result = 0;
    uint16_t m = 1;
    for (uint8_t i = 0; i < 16; i++, m <<= 1)
        if (!(a_stopped & m)) result |= draw_limb(i);
    return result;
}

/* цвета актёра (ClassicCostumeRenderer::setPalette, GF_OLD_BUNDLE) и маска бокса */
static void actor_pal(struct Actor *a)
{
    uint8_t lit = VAR(VAR_CURRENT_LIGHTS) & 8;
    for (uint8_t i = 0; i < 16; i++) cpal[i] = lit ? a->palette[i] & 15 : 8;
    if (!lit) cpal[12] = 0;
    cpal[c_pal0 & 15] = cpal[0];
    pal_id = pal_slot();
    if (pal_id == 0xFF)
    {
        if (npals == PAL_N) spr_reset();
        pal_id = npals++;
        for (uint8_t i = 0; i < 16; i++) pals[pal_id][i] = cpal[i];
    }
    zb = box_mask(a->walkbox) ? 1 : 0;
    az_w12 = az_w13 = 0;
}

static uint8_t rect_hit(const struct ARect *p, int16_t l, int16_t t, int16_t r, int16_t b)
{
    return p->r > p->l && p->l < r && l < p->r && p->t < b && t < p->b;
}

static uint16_t c_next;   /* следующая страница холста в шаге (az.c az_flip сбрасывает) */
uint16_t az_canv;

/* холст: полосы [s0, s1), строки [t, b) — фон из склада (блиттер сейчас) */
static void canvas_open(uint8_t s0, uint8_t s1, int16_t t, int16_t b)
{
    cx0 = (int16_t)(s0 * 8);
    cw = (int16_t)((s1 - s0) * 2);
    ct = t;
    chh = b - t;
    c_pg = AZ_CANV + az_canv++;
    uint16_t hi = AZ_HI(AZ_STORE) << 8 | AZ_HI(c_pg), lo = AZ_LO(c_pg);
    if (az_lmode)
        bi(AZ_HI(c_pg), 0, lo, 0, (uint16_t)cw, (uint16_t)chh, (uint16_t)cw, 0, 0);
    if (az_lmode != 1)
        for (uint8_t s = s0; s < s1; s++)
        {
            int16_t y0 = t, y1 = b;
            if (az_lmode)   /* фонарик: строки и полосы его прямоугольника */
            {
                int16_t a0 = (vx0 + az_lx0) >> 3, a1 = (vx0 + az_lx1 + 7) >> 3;
                if (s < a0 || s >= a1) continue;
                if (y0 < az_ly0) y0 = az_ly0;
                if (y1 > az_ly1) y1 = az_ly1;
                if (y0 >= y1) continue;
            }
            /* SRC — с кратной 16 строки (t кратно 16); над фонариком — снова чёрным */
            int16_t ya = y0 & ~15;
            uint16_t d = (uint16_t)(lo + (s - s0) * 2 + (ya - t) * cw);
            bi(hi, (uint16_t)(AZ_LO(AZ_STORE) + s * 256 + ya * 2), d, 011, 2, (uint16_t)(y1 - ya), (uint16_t)cw, 0, 0);
            if (ya < y0) bi(AZ_HI(c_pg), 0, d, 0, 2, (uint16_t)(y0 - ya), (uint16_t)cw, 0, 0);
        }
    blt_now();
    win_back();   /* данные банка B — и выше 0140000 */
}

/* холст -> слой 2 (пакет шага) */
static void canvas_close(void)
{
    bq(AZ_HI(c_pg) << 8 | AZ_HI(AZ_L2), AZ_LO(c_pg), (uint16_t)(cx0 >> 2), 011, (uint16_t)cw, (uint16_t)chh, 256,
       (uint16_t)(AZ_ROW0 + 16 + ct), 0);
    win_back();
}

uint16_t az_actreset(uint16_t fn)
{
    (void)fn;
    for (uint8_t i = 0; i < NUM_ACTORS; i++) dr[i].l = dr[i].r = 0;
    ndr = 0;
    return 0;
}

/* рабочие массивы az_actors — не в стеке Си (он до 036200, под ним az_on и view_x) */
static uint8_t L[NUM_ACTORS], ch[NUM_ACTORS], vis[NUM_ACTORS], S[NUM_ACTORS], res[NUM_ACTORS];
static struct ARect nr[NUM_ACTORS];
static uint8_t dirty[120] __attribute__((aligned(2)));   /* полоса — байт: битовая карта стоила бы сдвигов-циклов */

/* actor.cpp: processActors (v2). flushed — gfx_view_flush: вид сдвинут или полосы фона
 * выведены заново — актёры в них затёрты: все — заново */
uint16_t az_actors(uint16_t fn, const struct az_actx *x, uint16_t flushed)
{
    (void)fn;
    struct Actor *A = x->actors;
    if (mrow_n != bm_strips)
    {
        mrow_n = bm_strips;
        uint16_t a = PAGE_ADDR;
        for (uint8_t y = 0; y < 128; y++, a += bm_strips) mrow[y] = a;
    }
    vx0 = view_x;
    if (*x->xstart != vx0) *x->xstart = vx0;
    x->ract_build();
    uint8_t nl = 0, any = 0;
    for (uint8_t k = 0; k < *x->nract; k++) L[nl++] = x->ract[k];
    if (ndr)   /* ушедшие из комнаты с кадром на экране */
        for (uint8_t i = 0; i < NUM_ACTORS; i++)
            if (dr[i].r > dr[i].l && A[i].room != currentRoom) L[nl++] = i;
    if (!nl)
    {
        x->animate_only();
        return 0;
    }

    for (uint16_t *d = (uint16_t *)dirty, k = 60; k; k--) *d++ = 0;
    drawing = 0;
    for (uint8_t p = 0; p < nl; p++)
    {
        uint8_t i = L[p];
        struct Actor *a = &A[i];
        uint8_t was = dr[i].r > dr[i].l;
        vis[i] = a->room == currentRoom && a->visible && a->costume;
        ch[i] = flushed || (vis[i] && a->needRedraw) || (was && (a->needBgReset || !vis[i]));
        nr[i].l = nr[i].r = 0;
        if (!ch[i])
        {
            nr[i] = dr[i];
            continue;
        }
        any = 1;
        if (vis[i] && actor_run(x, a) && r_r > r_l && r_b > r_t)
        {
            nr[i].l = r_l;
            nr[i].t = r_t;
            nr[i].r = r_r < vx0 + 320 ? r_r : vx0 + 320;
            nr[i].b = r_b;
            a->top = r_t;
            a->bottom = r_b;
        }
        /* полосы прежнего и нового места */
        for (uint8_t q = 0; q < 2; q++)
        {
            const struct ARect *r = q ? &nr[i] : &dr[i];
            if (r->r <= r->l) continue;
            for (int16_t s = r->l >> 3; s < (r->r + 7) >> 3 && s < 120; s++)
                if (s >= 0) dirty[s] = 1;
        }
    }
    if (!any)
    {
        x->animate_only();
        return 0;
    }
    /* от дальних к ближним */
    uint8_t n = 0;
    for (uint8_t p = 0; p < nl; p++)
        if (vis[L[p]]) S[n++] = L[p];
    for (uint8_t j = 0; j < n; ++j)
        for (uint8_t i = 0; i < n; ++i)
            if (A[S[j]].y < A[S[i]].y)
            {
                uint8_t t = S[i];
                S[i] = S[j];
                S[j] = t;
            }
    /* отрезки грязных полос вида — холстами */
    uint8_t vs = az_vs, ve = (uint8_t)(az_vs + 40);
    for (uint8_t k = 0; k < n; k++) res[k] = 0;
    for (uint8_t s = vs; s < ve;)
    {
        if (!dirty[s])
        {
            s++;
            continue;
        }
        uint8_t e = s;
        while (e < ve && dirty[e]) e++;
        /* строки: прежние и новые места сменившихся в этих полосах */
        int16_t l = (int16_t)(s * 8), r = (int16_t)(e * 8), t = 128, b = 0;
        for (uint8_t p = 0; p < nl; p++)
        {
            uint8_t i = L[p];
            if (!ch[i]) continue;
            for (uint8_t q = 0; q < 2; q++)
            {
                const struct ARect *rr = q ? &nr[i] : &dr[i];
                if (!rect_hit(rr, l, 0, r, 128)) continue;
                if (rr->t < t) t = rr->t;
                if (rr->b > b) b = rr->b;
            }
        }
        if (t < 0) t = 0;
        if (b > 128) b = 128;
        t &= ~15;   /* фон холста — из склада блиттером: SRC кратен 32 словам */
        /* холст не больше 4 Кбайт: полос — сколько влезет, до 16 (1024 / 1, 2, 4 строки в
         * байте — 0: холсты нулевой ширины, копии 256 слов x 255 строк — по складу фона) */
        uint8_t m = b - t > 64 ? (uint8_t)(1024 / (b - t)) : 16;
        for (uint8_t a0 = s; a0 < e && t < b; a0 = (uint8_t)(a0 + m))
        {
            uint8_t a1 = a0 + m < e ? (uint8_t)(a0 + m) : e;
            if (az_canv >= AZ_CANV_N) break;   /* холсты шага кончились */
            canvas_open(a0, a1, t, b);
            drawing = 1;
            for (uint8_t k = 0; k < n; k++)
            {
                uint8_t i = S[k];
                if (!rect_hit(&nr[i], cx0, ct, cx0 + cw * 4, ct + chh)) continue;
                struct Actor *a = &A[i];
                if (!load_cost(x, a->costume)) continue;
                actor_pal(a);
                res[k] |= actor_run(x, a);
                smk_back();   /* актёры — в странице банка B */
            }
            drawing = 0;
            canvas_close();
        }
        s = e;
    }
    /* итог: нарисованное, флаги, gfxUsageBits, анимация */
    for (uint16_t *u = (uint16_t *)x->strip_usage, i = 41 * 2; i; i--) *u++ = 0;
    for (uint8_t p = 0; p < nl; p++)
    {
        uint8_t i = L[p];
        if (!ch[i]) continue;
        if (dr[i].r > dr[i].l) ndr--;
        dr[i] = vis[i] ? nr[i] : (struct ARect){ 0, 0, 0, 0 };
        if (dr[i].r > dr[i].l) ndr++;
        A[i].needBgReset = 0;
    }
    for (uint8_t k = 0; k < n; k++)
    {
        uint8_t i = S[k];
        struct Actor *a = &A[i];
        if (ch[i]) a->needRedraw = res[k] & 1;
        if (dr[i].r > dr[i].l)
        {
            int16_t o = vx0 >> 3, s0 = (dr[i].l >> 3) - o, s1 = ((dr[i].r + 7) >> 3) - o;
            if (s0 < 0) s0 = 0;
            if (s1 > 41) s1 = 41;
            uint8_t b = bit8[i & 7], *u = x->strip_usage + s0 * 4 + (i >> 3);
            for (; s0 < s1; s0++, u += 4) *u |= b;
        }
        x->animateCostume(a);
    }
    win_back();
    return 0;
}
