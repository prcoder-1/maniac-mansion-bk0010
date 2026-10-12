/* Отрисовка актёров (банк B): Actor_v2::prepareDrawActorCostume (actor.cpp) и
 * ScummEngine::processActors — какие актёры перерисовать, фон под прежними
 * кадрами, холст для группы. Конечности кадра рисует банк C (limb.c), параметры
 * актёра — в rd (rend.h). Координаты — точки главного виртуального экрана MM
 * (320x136). */
#ifdef HOST
#include <stdio.h>
#include <stdlib.h>
#endif
#include "actor.h"
#include "gfx.h"
#include "rend.h"

/* costume.cpp: v1MMActorPalatte1/2 — цвета 1 и 2 кадра по номеру актёра */
static const uint8_t v1_pal1[25] = { 8, 8, 8, 8, 4, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8 };
static const uint8_t v1_pal2[25] = { 0, 7, 2, 6, 9, 1, 3, 7, 7, 1, 1, 9, 1, 4, 5, 5, 4, 1, 0, 5, 4, 2, 2, 7, 7 };

static uint8_t c_fmt;

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
    rd.c_page = ci->page;
    rd.c_res = ci->res;
    c_fmt = ci->fmt;
    rd.c_animCmds = ci->animCmds;
    return 1;
}

/* actor.cpp: Actor::drawActorCostume + prepareDrawActorCostume (v2) */
static void drawActorCostume(struct Actor *a)
{
    if (a->costume == 0) return;
    if (!a->needRedraw) return;
    a->needRedraw = 0;
    if (!loadCostume(a->costume)) return;
    rd.actorX = (int16_t)(a->x * V12_X_MULTIPLIER);
    rd.actorY = (int16_t)((a->y - a->elevation) * V12_Y_MULTIPLIER) + 1;   /* v1: y + 1 */
    rd.actorX -= (int16_t)(screenStartStrip * 8);
    if (a->facing == 270) rd.actorX += 16;
    /* procC64: при свете цвета 1, 2 — по актёру, 3 — чёрный */
    uint8_t dark = !(VAR(VAR_CURRENT_LIGHTS) & 8);   /* без LIGHTMODE_actor_use_colors — силуэт: 0, 11, 11 */
    rd.cpal[0] = 0;
    rd.cpal[1] = dark ? 0 : a->number < 25 ? v1_pal1[a->number] : 8;
    rd.cpal[2] = dark ? 11 : a->number < 25 ? v1_pal2[a->number] : 0;
    rd.cpal[3] = dark ? 11 : 0;
    /* setFacing */
    int16_t f = a->facing;
    uint8_t d = 3;
    if (f >= 71 && f <= 109) d = 1;
    else if (f >= 109 && f <= 251) d = 2;
    else if (f >= 251 && f <= 289) d = 0;
    uint8_t fmt = c_fmt;
    rd.mirror = d != 0 || (fmt & 0x80);
    /* _zbuf: маска из бокса (v3 и ниже — не больше 1) */
    rd.zbuf = getMaskFromBox(a->walkbox);
    if (rd.zbuf > 1) rd.zbuf = 1;
    rd.spr_ok = 1;
    rd.spr_act = (uint8_t)(a->number | (dark ? 0x40 : 0));   /* кэш спрайтов: палитра — по актёру и свету */
    for (uint8_t i = 0; i < 16; i++) rd.pos[i] = a->cost_curpos[i];
    rd.stopped = a->cost_stopped;
    uint8_t result = rend_limbs();   /* банк C */
    if (result & 1) a->needRedraw = 1;   /* _needRedraw = (version <= 6) */
    a->top = rd.draw_top;
    a->bottom = rd.draw_bottom;
    if (!rd.measuring)
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
            rd.draw_left = o->l;
            rd.draw_right = o->r;
            return 1;
        }
    }
    /* прогон 0 — новый кадр (и draw_* целиком), 1 — прежний: обмен с dr_pos туда и обратно */
    uint8_t (*rec)[64] = meas_rec_buf;   /* не на стеке: при отрисовке он и так глубок */
    int16_t nl = 0, nrt = 0, nt = 0, nb = 0;
    for (uint8_t p = 0; p < 2; p++)
    {
        for (uint8_t i = 0; i < 64; i++) rec[p][i] = 0xFF;
        rd.meas_rec = same ? rec[p] : 0;
        rd.measuring = 1;
        a->needRedraw = 1;
        drawActorCostume(a);
        if (!p)
        {
            nl = rd.draw_left;
            nrt = rd.draw_right;
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
    rd.meas_rec = 0;
    rd.measuring = 0;
    a->needRedraw = nr;
    rd.draw_left = nl;
    rd.draw_right = nrt;
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
    /* окно не задевает других актёров — на экране в нём лишь фон и прежний кадр:
     * фон на всё окно сразу, без чтения экрана */
    struct DrawnRect wnd = { l, t, r, b };
    uint8_t load = cv_need_load(l, t, r, b);
    if (!cv_open(l, t, r, b, load)) return 0;
    cv_bgrect(load ? o : &wnd);
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
/* gfxUsageBits актёра num по полосам drawn[num]: set — поставить, иначе снять
 * (здесь, а не в банке C: зовётся на каждого перерисованного актёра дважды) */
static void actor_strips(uint8_t num, uint8_t set)
{
    uint8_t m = (uint8_t)(1u << (num & 7));
    for (int16_t st = drawn[num].l / 8; st < (drawn[num].r + 7) / 8; st++)
        if (st >= 0 && st < 40)
        {
            if (set) strip_usage[st][num >> 3] |= m;
            else strip_usage[st][num >> 3] &= (uint8_t)~m;
        }
}
#define strips_clear(num) actor_strips(num, 0)   /* gfxUsageBits прошлого кадра */

/* прямоугольник нового кадра (прогон без рисования) */
static void measure(struct Actor *a, struct DrawnRect *n)
{
    uint8_t nr = a->needRedraw;
    int16_t tp = a->top, bt = a->bottom;
    rd.meas_rec = 0;
    rd.measuring = 1;
    a->needRedraw = 1;
    drawActorCostume(a);
    rd.measuring = 0;
    a->needRedraw = nr;
    n->l = rd.draw_left;
    n->r = rd.draw_right;
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
    if (a->bottom <= a->top || rd.draw_right <= rd.draw_left) return;
    struct DrawnRect *d = &drawn[num];
    d->l = rd.draw_left;
    d->r = rd.draw_right;
    d->t = a->top;
    d->b = a->bottom;
    actor_strips(num, 1);
}

/* в общий холст: вид целиком — видимые актёры комнаты; иначе — перерисовываемые
 * с прежним кадром, не «одинокие» */
static uint8_t grp_member(uint8_t i, uint8_t whole, const uint8_t *R, const struct DrawnRect *late)
{
    if (whole) return actors[i].room == currentRoom && actors[i].visible;
    return R[i] && drawn[i].r > drawn[i].l && late[i].r <= late[i].l;
}

void processActors(void)
{
    rd.xstart = (int16_t)PHYS_X(screenStartStrip * 8);   /* точка кольца фона (gfx_v1.c) */
    rd.spr_budget = 3;
    rd.view0 = bk_x(rd.xstart) & ~3u;
    target_screen();
    /* вся видимая часть заново (прокрутка): её вывод — после холста с актёрами */
    uint8_t flushed = gfx_view_pending() ? 1 : gfx_view_flush();
    uint8_t view_redrawn = flushed == 1;   /* старых актёров на экране не будет */
    /* актёры, которых касается кадр: в комнате или с прошлым кадром на экране */
    uint8_t L[NUM_ACTORS], nl = 0, R[NUM_ACTORS], P[NUM_ACTORS], any = 0;
    SND_POLL();
    struct Actor *a = &actors[1];
    struct DrawnRect *d = &drawn[1];
    for (uint8_t i = 1; i < NUM_ACTORS; i++, a++, d++)
    {
        if (!(i & 7)) SND_POLL();
        uint8_t in = a->room == currentRoom;
        if (!in && d->r <= d->l) continue;
        uint8_t r = flushed || (d->r > d->l && a->needBgReset) || (in && a->visible && a->needRedraw);
        L[nl++] = i;
        R[i] = P[i] = r;   /* P — сменившиеся сами (без задетых ими) */
        any |= r;
    }
    SND_POLL();
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
    /* в холсте вместе (R == 2), на экран одной записью — без мерцания:
     * перекрывающиеся — фон под прежними кадрами, новые кадры от дальних к ближним.
     * Окно — прежние и новые кадры сменившихся (P); задетые ими соседи
     * рисуются в нём с обрезкой (на экране вне окна они те же), без фона под ними;
     * вид целиком (прокрутка) — все видимые на фоне, а вывод вида — в обход холста
     * (окно — и под прежним кадром): актёр не пропадает между выводом и холстом */
    uint8_t grp = 0;
    struct DrawnRect u = { 0x7FFF, 0x7FFF, -0x7FFF, -0x7FFF }, nw;
    for (uint8_t p = 0; p < nl; p++)
    {
        uint8_t i = L[p];
        struct Actor *a = &actors[i];
        if (!grp_member(i, view_redrawn, R, late)) continue;
        grp = 1;
        if (!view_redrawn && !P[i]) continue;
        rect_union(&u, &drawn[i]);   /* и прежний кадр: на экране — до записи холста */
        if (a->room == currentRoom && a->visible)
        {
            measure(a, &nw);
            rect_union(&u, &nw);
        }
    }
    if (grp && cv_open(u.l, u.t, u.r, u.b, !view_redrawn))
    {
        for (uint8_t p = 0; p < nl; p++)
        {
            uint8_t i = L[p];
            if (!grp_member(i, view_redrawn, R, late)) continue;
            R[i] = 2;
            if (!view_redrawn && P[i]) cv_bgrect(&drawn[i]);
        }
        if (view_redrawn)
        {
            cv_bgrect(&u);
            cv_view_excl();
        }
    }
    else
        grp = 0;

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
        if (view_redrawn) gfx_view_flush();   /* вид — в обход готового холста, сразу затем он */
        cv_close();
    }
    else if (view_redrawn)
        gfx_view_flush();
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
