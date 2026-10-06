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

/* тон на динамике играет процессор: в простое цикла — кусками по такту
 * секвенсора (236 Гц = 99 отсчётов таймера), в работе — через snd_poll.
 * Замер tone_sq (ОЗУ СМК): полупериод = 5.0 мкс * hp + 93 мкс + 57 мкс на
 * каждые 256 витков; нужен d / 2386364 с (d — делитель PIT). */
struct tone { uint16_t hp, rem, level, t_end; };
void tone_sq(struct tone *t, uint16_t counts);
uint16_t umulhi(uint16_t a, uint16_t b);
#define TMR_COUNT (*(volatile uint16_t *)0177710)
static struct tone tn;
static uint16_t hp_div;

static uint16_t half_period(uint16_t d)
{
    uint16_t h = umulhi(d, 5492);   /* d * 0.41905 мкс / 5.0 мкс */
    if (h > 256)
    {
        uint16_t e = h - 256;   /* e / 23 ~ e * 0.0415 */
        h -= (e >> 5) + (e >> 7) + (e >> 9) + (e >> 11);
    }
    return h > 19 ? h - 19 : 1;
}

static void snd_wait(uint16_t counts)
{
    sound_update();
    if (!tone_div)
        tn.hp = 0;
    else
    {
        if (tone_div != hp_div)
        {
            hp_div = tone_div;
            tn.hp = half_period(tone_div);
        }
        /* фаза низкого тона шла и в паузе: 1 отсчёт таймера = 8.53 витка
         * (у высоких период короче куска — фаза не слышна) */
        if (tn.hp >= 128)
        {
            uint16_t el = tn.t_end - TMR_COUNT;
            if (el > 1000) el = 1000;
            uint16_t it = (uint16_t)(el * 8 + el / 2 + el / 32);
            if (it < tn.rem)
                tn.rem -= it;
            else
            {
                it -= tn.rem;
                tn.level ^= 0100;
                while (it >= tn.hp)
                {
                    it -= tn.hp;
                    tn.level ^= 0100;
                }
                tn.rem = tn.hp - it;
                *(volatile uint16_t *)0177716 = tn.level ^ 0100;   /* level — уровень следующего фронта */
            }
        }
    }
    tone_sq(&tn, counts);
}

/* в работе цикла: кусок тона 2.1 мс через каждые 4.3 мс (треть времени — звуку) */
void snd_poll(void)
{
    if ((int16_t)(TMR_COUNT - snd_due) > 0) return;   /* таймер считает вниз */
    snd_wait(50);
    snd_due = TMR_COUNT - 100;
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
        while (acc < (uint16_t)delta)
        {
            snd_wait(99);
            acc += timer_jiffies();
        }
        acc -= (uint16_t)delta;
        if (acc > 60) acc = 0;   /* после чтения с диска не догоняем */
        scummLoop(delta);
        loop_count++;
    }
}

/* сохранение игры (saveload.c): курсор образа для sv_heap банков A и B */
uint8_t sv_page;
uint16_t sv_off;
