/* Запуск на БК: СМК-512, общая копия кода и банки из файлов ANDOS, затем игровой
 * цикл ScummEngine::go — шаг каждые VAR_TIMER_NEXT джиффи (60 Гц). */
#include "scumm.h"
#include "con.h"

void timer_init(void);
uint16_t loop_count;   /* отладка: шагов цикла */
uint16_t timer_jiffies(void);

void fatal(const char *msg, uint16_t v)
{
    smk_map(PG_BANKA);
    con_clear();
    con_puts(10, 0, msg, 3);
    con_oct(11, 0, v, 3);
    for (;;) ;
}

static void load_code(const char *name, uint16_t flags, uint16_t page, uint16_t dest)
{
    struct ld_req q = { name, flags, page, dest, PAGE_DATA_SIZE, PG_MASK, OVF_ADDR };
    if (ld_file(&q) < 0) fatal(name, ld_resp);
}

int main(void)
{
    con_clear();
    smk_home = smk_probe();
    if (!smk_home) fatal("NEED SMK-512", 0);
    cur_page = smk_home;
    far_sp = 03174;
    load_code("REPL", LD_ALL, 0, REPL_ADDR);
    load_code("BANKA", 0, PG_BANKA, PAGE_ADDR);
    load_code("BANKB", 0, PG_BANKB, PAGE_ADDR);
    load_code("BANKC", 0, PG_BANKC, PAGE_ADDR);
    smk_map(PG_BANKA);
    con_clear();
    timer_init();
    void kbd_init(void);
    kbd_init();
    scumm_init();
    runBootscript();
    uint16_t acc = 0;
    for (;;)
    {
        int16_t delta = scumm_delta();
        while (acc < (uint16_t)delta) acc += timer_jiffies();
        acc -= (uint16_t)delta;
        if (acc > 60) acc = 0;   /* после чтения с диска не догоняем */
        scummLoop(delta);
        loop_count++;
    }
}
