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

/* звук: секвенсор Player_V1 (банк D, sound_fill) идёт вперёд времени и пишет тоны
 * по тактам в кольцо; играет их snd.s (snd_tick — из горячих циклов) и сам зовёт
 * секвенсор через snd_seq, когда в кольце мало */
void snd_tick(void);
void snd_spin(uint16_t counts);
void sound_fill(void);
void sound_fill1(void);
extern volatile uint16_t sndst[11];
extern volatile uint16_t snd_seqv;

void snd_poll(void)
{
    snd_tick();
}

static void snd_seq(void)
{
    sound_fill1();
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
    load_code("BANKD", 0, PG_BANKD, PAGE_ADDR);
    smk_map(PG_BANKA);
    con_clear();
    {   /* фоновый тон (sys.ld _sndst): там остатки загрузки — выключить */
        volatile uint16_t *z = (volatile uint16_t *)023740;   /* _sndst.._snd_active (sys.ld) */
        for (uint8_t i = 0; i < 16; i++) z[i] = 0;
        snd_seqv = (uint16_t)snd_seq;
    }
    timer_init();
    void kbd_init(void);
    kbd_init();
    scumm_init();
    runBootscript();
    uint16_t acc = 0;
    uint8_t skips = 0;
    for (;;)
    {
        int16_t delta = scumm_delta();
        while (acc < (uint16_t)delta)
        {
            snd_spin(99);
            acc += timer_jiffies();
        }
        acc -= (uint16_t)delta;
        if (acc > 60) acc = 0;   /* после чтения с диска не догоняем */
        /* отстали больше чем на шаг — шаг без отрисовки, но не больше двух подряд:
         * герои идут с игровой скоростью, реже обновляется только картинка */
        draw_frame = acc < (uint16_t)delta || skips >= 4;
        skips = draw_frame ? 0 : (uint8_t)(skips + 1);
        scummLoop(delta);
        loop_count++;
    }
}

/* сохранение игры (saveload.c): курсор образа для sv_heap банков A и B */
uint8_t sv_page;
uint16_t sv_off;
