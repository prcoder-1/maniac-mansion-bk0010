/* Модуль AZБК (azmod.ld: страница 6 ОЗУ 11М в окне 040000; вызов — azc, az.h).
 *
 * VGA 512x256, 4 бита на точку, три слоя (0177230 = 076136): строка слоя — 256 слов
 * (1024 точки), видно 128 слов с прокрутки слоя. Строка экрана = 28 + y MM.
 *   слой 0 (верхний, палитра 0..15)  — курсор;
 *   слой 1 (палитра 16..31)          — интерфейс: точка x MM — 96 + x;
 *   слой 2 (нижний, палитра 32..47)  — комната и актёры: точка слоя = точка мира, прокрутка
 *                                       0177256 = view_x/4 - 24 (вид — с 96-й точки экрана).
 * В слоях 0 и 1 цвет 0 прозрачен; строки слоя 2 вне комнаты и её точки вне вида — чёрные
 * (полоса, ушедшая из вида, гасится), так что сбоку от вида и под интерфейсом — чёрное.
 *
 * Фон комнаты декодируется в «склад» (полоса 8 точек — 128 строк по 2 слова подряд: для
 * блиттера SRC линеен) и копируется блиттером в слой 2, когда полоса входит в вид или
 * меняется. Пакет шага исполняется в конце шага (az_flip); если сдвинулась прокрутка —
 * в гашении кадра VGA, вместе с записью 0177256 (маяк — последняя команда пакета).
 *
 * Доступ процессора к памяти AZ — окна 12 и 13 (0140000..0157777): их не трогает
 * запись 0177716 (её делает кадровое прерывание ради драйвера звука). После работы
 * окна возвращаются записью режима СМК (smk_code(cur_page)). */
#include "scumm.h"
#include "gfx.h"
#include "az.h"
#include "azm.h"

/* --- окна и блиттер --- */

uint16_t az_w12, az_w13;   /* страницы AZ в окнах 12 и 13 (0 — как у СМК) */

void win12(uint16_t p)
{
    if (az_w12 != p) REG(0177330) = az_w12 = p;
}

void win13(uint16_t p)
{
    if (az_w13 != p) REG(0177332) = az_w13 = p;
}

void win_back(void)
{
    if (az_w12 | az_w13) smk_back();
}

/* страница СМК (окна 8..15 заново — и 12, 13) */
void smk_to(uint16_t page)
{
    az_smk(page);
    az_w12 = az_w13 = 0;
}

void smk_back(void)
{
    az_back();
    az_w12 = az_w13 = 0;
}

uint16_t az_nq, az_ni;      /* команд в пакете шага и в пакете «сейчас» */
static uint8_t q_sync;      /* пакет шага — в гашении, с прокруткой */
static uint16_t hs_cur, hs_want = 0xFFFF;

void blt_idle(void)
{
    for (uint16_t n = 0; REG(0177270) & 0100000;)
        if (!--n) break;   /* ~0.5 с: блиттер не отвечает — дальше без него */
}

/* команда в пакет page под номером n: SRC — линейно с адреса слова {shi, slo}, DST —
 * прямоугольник w слов x h строк с шагом строки pitch слов от {dhi, dlo + y*256} */
static void put(uint16_t page, uint16_t n, uint16_t hi, uint16_t slo, uint16_t dlo, uint16_t op,
                uint16_t w, uint16_t h, uint16_t pitch, uint16_t y, uint16_t k)
{
    win13(page + (n >> 8));
    volatile uint16_t *d = (volatile uint16_t *)(0150000u + ((n & 0377) << 4));
    d[0] = hi;
    d[1] = slo;
    d[2] = dlo;
    d[3] = op;
    d[4] = (uint16_t)((h << 8) | (w - 1));
    d[5] = (uint16_t)(pitch - w + 1);
    d[6] = y;
    d[7] = k;
}

/* в пакет шага; первая команда шага — когда блиттер свободен (прошлый пакет мог идти) */
void bq(uint16_t hi, uint16_t slo, uint16_t dlo, uint16_t op, uint16_t w, uint16_t h, uint16_t pitch, uint16_t y, uint16_t k)
{
    if (az_nq >= 510) return;
    if (!az_nq) blt_idle();
    put(AZ_CMDS, az_nq++, hi, slo, dlo, op, w, h, pitch, y, k);
}

/* в пакет «сейчас» (исполняет blt_now) */
void bi(uint16_t hi, uint16_t slo, uint16_t dlo, uint16_t op, uint16_t w, uint16_t h, uint16_t pitch, uint16_t y, uint16_t k)
{
    if (az_ni >= 255) return;
    if (!az_ni) blt_idle();
    put(AZ_CMDI, az_ni++, hi, slo, dlo, op, w, h, pitch, y, k);
}

static void blt_start(uint16_t page, uint16_t n)
{
    REG(0177272) = page;
    REG(0177270) = 040000;          /* ручной режим, затем пуск с числом команд */
    REG(0177270) = (uint16_t)(050000 | n);
}

/* пакет «сейчас»: пуск и ожидание конца */
void blt_now(void)
{
    if (!az_ni) return;
    blt_idle();
    blt_start(AZ_CMDI, az_ni);
    az_ni = 0;
    blt_idle();
}

/* --- палитра, режим, слои --- */

static const uint16_t ega[16] = {
    0, 025, 01240, 01265, 052000, 052025, 052500, 053265,
    024512, 024537, 025752, 025777, 076512, 076537, 077752, 077777,
};

static void pal_set(uint16_t cell, uint16_t v)
{
    REG(0177234) = cell;
    REG(0177236) = v;
}

/* слой — чёрным: 256 строк по 256 слов двумя командами заливки */
static void layer_clear(uint16_t page)
{
    uint16_t hi = page >> 5;
    bi(hi, 0, 0, 0, 256, 255, 256, 0, 0);
    bi(hi, 0, 0, 0, 256, 1, 256, 255, 0);
}

static uint8_t rvs = 0xFF;          /* полоса левого края вида в слое 2 (0xFF — вид пуст) */
static uint8_t rw;                  /* полос комнаты при выводе вида */

uint16_t az_init(uint16_t fn)
{
    (void)fn;
    for (uint8_t c = 0; c < 16; c++)
    {
        pal_set(c, ega[c]);
        pal_set(16 + c, ega[c]);
        pal_set(32 + c, ega[c]);
        pal_set(320 + c, ega[c]);   /* однослойный 16-цветный — на всякий случай тоже */
    }
    REG(0177270) = 040000;          /* блиттер: ручной режим, стоп */
    az_nq = az_ni = 0;
    q_sync = 0;
    layer_clear(AZ_L0);
    layer_clear(AZ_L1);
    layer_clear(AZ_L2);
    blt_now();
    REG(0177232) = AZ_L0;
    REG(0177240) = AZ_L1;
    REG(0177242) = AZ_L2;
    REG(0177250) = 0;
    REG(0177246) = 0;
    REG(0177244) = 0;
    REG(0177252) = 0;
    REG(0177254) = 0;
    REG(0177256) = hs_cur = 0;
    hs_want = 0;
    REG(0177230) = AZ_VMODE;
    rvs = 0xFF;
    az_cur_init();
    win_back();
    return REG(0177370);
}

/* фонарик: скруглённые углы (ScummVM drawFlashlight: 8, 6, 4, 3, 2, 2, 1, 1 точек) —
 * процессором в слое 2, когда пакет шага (полосы и холсты) уже выведен */
static void fl_corners(void)
{
    static const uint8_t cd[8] = { 8, 6, 4, 3, 2, 2, 1, 1 };
    static const uint16_t nm[4] = { 0177760, 0177417, 0170377, 07777 };
    int16_t l = view_x + az_lx0, r = view_x + az_lx1 - 1;
    blt_idle();
    for (uint8_t k = 0; k < 8; k++)
        for (uint8_t q = 0; q < 2; q++)
        {
            uint16_t line = (uint16_t)(AZ_ROW0 + 16 + (q ? az_ly1 - 1 - k : az_ly0 + k));
            win12(AZ_L2 + (line >> 3));
            volatile uint16_t *row = (volatile uint16_t *)(0140000u + ((line & 7) << 9));
            for (uint8_t p = 0; p < cd[k]; p++)
            {
                row[(uint16_t)(l + p) >> 2] &= nm[(l + p) & 3];
                row[(uint16_t)(r - p) >> 2] &= nm[(r - p) & 3];
            }
        }
}

/* конец шага: пакет шага; прокрутка — в гашении после него */
uint16_t az_flip(uint16_t fn)
{
    (void)fn;
    if (q_sync || hs_want != hs_cur)
    {
        bq(AZ_HI(AZ_SENT) << 8 | AZ_HI(AZ_SENT), 0, AZ_LO(AZ_SENT), 0, 1, 1, 1, 0, 1);   /* маяк: 0x0101 */
        win13(AZ_SENT);
        volatile uint16_t *s = (volatile uint16_t *)0150000;
        *s = 0;
        REG(0177272) = AZ_CMDS;
        REG(0177270) = az_nq;               /* авто: в конце кадра VGA */
        for (uint16_t n = 0; !*s && --n;) ;
        REG(0177256) = hs_cur = hs_want;    /* в гашении: с первой строки следующего кадра */
        blt_idle();
        REG(0177270) = 040000;              /* снова ручной: пакет не повторится */
    }
    else if (az_nq)
        blt_start(AZ_CMDS, az_nq);
    if (az_lmode == 2 && az_ly1 - az_ly0 >= 16) fl_corners();
    az_nq = 0;
    q_sync = 0;
    az_canv = 0;
    win_back();
    return 0;
}

/* --- склад фона и вид --- */

uint8_t az_pk[32];   /* v4pix.s: цвет EGA -> тетрада (с заменой _roomPalette), младшая и старшая */

uint16_t az_strips(uint16_t fn, struct v2p *p)
{
    (void)fn;
    for (uint8_t c = 0; c < 16; c++)
    {
        uint8_t e = roomPalette[c] & 15;
        az_pk[c] = e;
        az_pk[16 + c] = (uint8_t)(e << 4);
    }
    uint16_t r = v4_pix(p);
    az_w12 = az_w13 = 0;   /* v4_pix вернул страницу вызывающего */
    return r;
}

/* свет (gfx.c gfx_lights): 0 — светло, 1 — темно, 2 — фонарик в точках вида [x0, x1) и
 * строках комнаты [y0, y1) */
uint8_t az_lmode;
int16_t az_lx0, az_lx1, az_ly0, az_ly1;

/* полосы [s0, s1) слоя 2 — из склада (с учётом света) */
static void strips_out(uint8_t s0, uint8_t s1)
{
    if (s1 > rw) s1 = rw;
    if (s0 >= s1) return;
    uint16_t h = roomHeight;
    if (az_lmode)
    {
        bq(AZ_HI(AZ_L2), 0, (uint16_t)(s0 * 2), 0, (uint16_t)((s1 - s0) * 2), h, 256, AZ_ROW0 + 16, 0);
        if (az_lmode == 1) return;
        /* фонарик: колонки по 8 точек (слова парами), строки [az_ly0, az_ly1); копия — с
         * кратной 16 строки (SRC), строки над фонариком — снова чёрным */
        int16_t a = (int16_t)((view_x + az_lx0) >> 3), b = (int16_t)((view_x + az_lx1 + 7) >> 3);
        uint16_t ya = (uint16_t)az_ly0 & ~15u;
        if (a < s0) a = s0;
        if (b > s1) b = s1;
        for (; a < b; a++)
        {
            bq(AZ_HI(AZ_STORE) << 8 | AZ_HI(AZ_L2), (uint16_t)(AZ_LO(AZ_STORE) + a * 256 + ya * 2), (uint16_t)(a * 2), 011,
               2, (uint16_t)(az_ly1 - ya), 256, (uint16_t)(AZ_ROW0 + 16 + ya), 0);
            if (ya < (uint16_t)az_ly0)
                bq(AZ_HI(AZ_L2), 0, (uint16_t)(a * 2), 0, 2, (uint16_t)(az_ly0 - ya), 256, (uint16_t)(AZ_ROW0 + 16 + ya), 0);
        }
        return;
    }
    for (uint8_t s = s0; s < s1; s++)
        bq(AZ_HI(AZ_STORE) << 8 | AZ_HI(AZ_L2), (uint16_t)(AZ_LO(AZ_STORE) + s * 256), (uint16_t)(s * 2), 011, 2, h, 256, AZ_ROW0 + 16, 0);
}

/* полосы [s0, s1) слоя 2 — чёрным */
static void strips_off(uint8_t s0, uint8_t s1)
{
    if (s0 >= s1) return;
    bq(AZ_HI(AZ_L2), 0, (uint16_t)(s0 * 2), 0, (uint16_t)((s1 - s0) * 2), 128, 256, AZ_ROW0 + 16, 0);
}

uint8_t az_vs = 0xFF;   /* полоса левого края вида (az_act.c: актёры в виде) */

/* gfx_view_flush: vs — полоса левого края вида, dirty: 1 — весь, 2 — полосы [d0, d1).
 * 1 — вид заново, 2 — сдвиг или часть полос (актёры — заново), 0 — ничего */
uint16_t az_view(uint16_t fn, uint16_t vs, uint16_t dirty, uint16_t d0, uint16_t d1)
{
    (void)fn;
    view_x = (int16_t)(vs * 8);
    az_vs = (uint8_t)vs;
    uint16_t r = 0;
    if (dirty == 1 || rvs == 0xFF)
    {
        if (rvs != 0xFF) strips_off(rvs, (uint8_t)(rvs + 40));
        rw = bm_strips;
        strips_out((uint8_t)vs, (uint8_t)(vs + 40));
        r = 1;
    }
    else
    {
        if (vs != rvs)
        {
            if (vs > rvs)
            {
                uint8_t e = (uint8_t)(rvs + 40);
                strips_off(rvs, vs < e ? (uint8_t)vs : e);
                strips_out(vs < e ? e : (uint8_t)vs, (uint8_t)(vs + 40));
            }
            else
            {
                uint8_t e = (uint8_t)(vs + 40);
                strips_off(e > rvs ? e : rvs, (uint8_t)(rvs + 40));
                strips_out((uint8_t)vs, e < rvs ? e : rvs);
            }
            r = 2;
        }
        if (dirty == 2)
        {
            if (d0 < vs) d0 = vs;
            if (d1 > vs + 40) d1 = vs + 40;
            if (d0 < d1)
            {
                strips_out((uint8_t)d0, (uint8_t)d1);
                r = 2;
            }
        }
    }
    rvs = (uint8_t)vs;
    hs_want = (uint16_t)((vs * 2 - 24) & 0377);
    if (r == 1) q_sync = 1;
    win_back();
    return r;
}

uint16_t az_lights(uint16_t fn, uint16_t mode, int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    (void)fn;
    az_lmode = (uint8_t)mode;
    az_lx0 = x0 & ~7;   /* колонки — по 8 точек (вид — с полосы) */
    az_lx1 = (x1 + 7) & ~7;
    az_ly0 = y0;
    az_ly1 = y1;
    return 0;
}

/* все слои — чёрным (чтение с диска через ДОС затёрло экран — у AZ не затёрло, но
 * игра перерисует всё) */
uint16_t az_clearall(uint16_t fn)
{
    (void)fn;
    blt_idle();
    layer_clear(AZ_L0);
    layer_clear(AZ_L1);
    layer_clear(AZ_L2);
    blt_now();
    rvs = 0xFF;
    az_cur_lost();
    win_back();
    return 0;
}
