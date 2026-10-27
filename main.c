/* Запуск на БК: СМК-512, общая копия кода и банки из файлов ANDOS, затем игровой
 * цикл ScummEngine::go — шаг каждые VAR_TIMER_NEXT джиффи (60 Гц). */
#include "scumm.h"
#include "con.h"
#include "snddrv.h"

void timer_init(void);
uint16_t loop_count;   /* отладка: шагов цикла */
uint32_t idle_t;       /* отладка: простой в отсчётах таймера ВИ1 (31250 Гц, 640 на кадр) */
uint16_t timer_jiffies(void);

void fatal(const char *msg, uint16_t v)
{
    smk_map(PG_BANKA);
    con_clear();
    con_puts(10, 0, msg, 3);
    con_oct(11, 0, v, 3);
    for (;;) ;
}

/* блок, длина, первое слово REPL, банков A..D и драйвера звука на диске — пишет
 * tools/ftab.py в образ (0 — через ДОС) */
uint16_t code_ft[6][3] = { { 0, 0, 1 } };   /* не нули: в .data (в файле) */

static void load_code(const char *name, uint16_t flags, uint16_t page, uint16_t dest, const uint16_t *f)
{
    if (page) far_fill(page, PAGE_ADDR, 0, PAGE_DATA_SIZE);   /* файлы — без нулей в конце (mkbin --trim) */
    struct ld_req q = { name, flags, page, dest, PAGE_DATA_SIZE, PG_MASK, OVF_ADDR, f[0], f[1], f[2] };
    if (ld_file(&q) < 0) fatal(name, ld_resp);
}

int main(void)
{
    con_clear();
    if (*(volatile uint8_t *)0177717 == 0200)   /* адрес пуска 0100000 — БК-0010 */
    {
        con_puts(10, 0, "NEED BK-0011M", 3);
        for (;;) ;
    }
    smk_home = smk_probe();
    if (!smk_home) fatal("NEED SMK-512", 0);
    cur_page = smk_home;
    far_sp = 03174;
    smk_twins();
    load_code("REPL", LD_ALL, 0, REPL_ADDR, code_ft[0]);
    smk_map(PG_BANKA);   /* общая копия кода (far_fill) видна только в страницах 2..15 */
    extern uint8_t gstate_start[], __bss_start[];
    void snap_save(void *, void *);
    snap_save(gstate_start, __bss_start);   /* данные ядра — для перезапуска по «СТОП» (loader.s _rst) */
    load_code("BANKA", 0, PG_BANKA, PAGE_ADDR, code_ft[1]);
    load_code("BANKB", 0, PG_BANKB, PAGE_ADDR, code_ft[2]);
    load_code("BANKC", 0, PG_BANKC, PAGE_ADDR, code_ft[3]);
    load_code("BANKD", 0, PG_BANKD, PAGE_ADDR, code_ft[4]);
    /* звуковой драйвер AY: через страницу кэша — в страницу 1 ОЗУ 11М */
    load_code("SNDDRV", 0, PG_CACHE, PAGE_ADDR, code_ft[5]);
    w1_copy(DRV_PAGE11, PG_CACHE, DRV_END - DRV_ADDR);
    snd_call(DRV_INIT, 0, 0, 0);
    snd_on = 1;
    smk_map(PG_BANKA);
    con_clear();
    timer_init();
    void kbd_init(void);
    kbd_init();
    scr_fill(6);   /* вторая экранная страница — чистая */
    scr_fill(2);   /* кэш спрайтов актёров (costume_r.c): пуст */
    vctl_set(VCTL_PAL(0));   /* палитра 0, страница 5, кадровое прерывание */
    scumm_init();
    runBootscript();
    uint16_t acc = 0;
    uint8_t skips = 0;
    for (;;)
    {
        int16_t delta = scumm_delta();
        while (acc < (uint16_t)delta)
        {
            uint16_t t0 = *(volatile uint16_t *)0177710;
            __asm__ volatile("wait");   /* до кадрового прерывания */
            idle_t += (uint16_t)(t0 - *(volatile uint16_t *)0177710);   /* простой: отсчёты ВИ1 */
            acc += timer_jiffies();
            /* мышь сдвинулась — курсор и подсветка глагола сразу, не через шаг (их
             * 15 в секунду): иначе подсветка заметно отстаёт */
            extern int16_t joy_dx, joy_dy;
            if (*(volatile int16_t *)&joy_dx || (uint16_t)(*(volatile int16_t *)&joy_dy + 1) > 2) ui_hover();
        }
        acc -= (uint16_t)delta;
        if (acc > 60) acc = 0;   /* после чтения с диска не догоняем */
        /* отстали больше чем на шаг — шаг без отрисовки, но не больше двух подряд:
         * герои идут с игровой скоростью, реже обновляется только картинка */
        draw_frame = acc < (uint16_t)delta || skips >= 2;
        skips = draw_frame ? 0 : (uint8_t)(skips + 1);
        scummLoop(delta);
        loop_count++;
    }
}

/* сохранение игры (saveload.c): курсор образа для sv_heap банков A и B */
uint8_t sv_page;
uint16_t sv_off;
