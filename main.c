/* Запуск на БК: СМК-512, общая копия кода и банки из файлов ANDOS, затем игровой
 * цикл ScummEngine::go — шаг каждые VAR_TIMER_NEXT джиффи (60 Гц). */
#include "scumm.h"
#include "con.h"

void timer_init(void);
/* дно стека Си (sys.ld): метка затёрта — стек ушёл в данные ниже него */
extern volatile uint16_t stack_low, far_stk0;   /* и дно дальнего стека (sys.ld) */
#define STACK_MARK 0123456
uint16_t loop_count;   /* отладка: шагов цикла */
uint16_t timer_jiffies(void);

void fatal(const char *msg, uint16_t v)
{
    smk_map(PG_BANKA);
    con_clear();
    *(volatile uint16_t *)0177664 = 01330;   /* полный экран: при запуске мог остаться малый (LD_SMALL) */
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

/* блок, длина, первое слово REPL и банков A..D на диске — пишет tools/ftab.py в
 * образ (0 — через ДОС) */
uint16_t code_ft[5][3] = { { 0, 0, 1 } };   /* не нули: в .data (в файле) */

static void load_code(const char *name, uint16_t flags, uint16_t page, uint16_t dest, const uint16_t *f)
{
    struct ld_req q = { name, flags, page, dest, PAGE_DATA_SIZE, PG_MASK, OVF_ADDR, f[0], f[1], f[2] };
    if (ld_file(&q) < 0) fatal(name, ld_resp);
}

#ifndef PAN_SKIPS
#define PAN_SKIPS 1
#endif

int main(void)
{
    con_clear();
    smk_home = smk_probe();
    if (!smk_home) fatal("NEED SMK-512", 0);
    cur_page = smk_home;
    far_sp = 03174;
    load_code("REPL", LD_ALL | LD_SMALL, 0, REPL_ADDR, code_ft[0]);
    load_code("BANKA", LD_SMALL, PG_BANKA, PAGE_ADDR, code_ft[1]);
    load_code("BANKB", LD_SMALL, PG_BANKB, PAGE_ADDR, code_ft[2]);
    load_code("BANKC", LD_SMALL, PG_BANKC, PAGE_ADDR, code_ft[3]);
    load_code("BANKD", LD_SMALL, PG_BANKD, PAGE_ADDR, code_ft[4]);
    smk_map(PG_BANKA);
    con_clear();   /* буфер загрузки был в экране — виден лишь «малый» (LD_SMALL) */
    *(volatile uint16_t *)0177664 = 01330;   /* полный экран */
    {   /* фоновый тон (sys.ld _sndst): там остатки загрузки — выключить */
        volatile uint16_t *z = (volatile uint16_t *)023720;   /* окно, шум (snd.s), _sndst.._snd_active (sys.ld) */
        for (uint8_t i = 0; i < 24; i++) z[i] = 0;
        for (z = (volatile uint16_t *)027332; z < (volatile uint16_t *)027500; z++) *z = 0;   /* rd (sys.ld) */
        snd_seqv = (uint16_t)snd_seq;
    }
    timer_init();
    void kbd_init(void);
    kbd_init();
    scumm_init();
    runBootscript();
    uint16_t acc = 0;
    uint8_t skips = 0;
    stack_low = far_stk0 = STACK_MARK;
    for (;;)
    {
        if (stack_low != STACK_MARK || far_stk0 != STACK_MARK) fatal("STACK", far_stk0 != STACK_MARK);
        int16_t delta = scumm_delta();
        while (acc < (uint16_t)delta)
        {
            snd_spin(99);
            acc += timer_jiffies();
        }
        acc -= (uint16_t)delta;
        if (acc > 60) acc = 0;   /* после чтения с диска не догоняем */
        /* отстали больше чем на шаг — шаг без отрисовки, но не больше четырёх подряд
         * (реже пропускать — медленнее игра, а кадров в секунду не больше):
         * герои идут с игровой скоростью, реже обновляется только картинка.
         * Камера едет по скрипту (panCameraTo, заставка) — пропусков меньше: прокрутка
         * медленнее, но мелкими шагами */
        draw_frame = acc < (uint16_t)delta || skips >= (camera.mode == kPanningCameraMode && camera.moved ? PAN_SKIPS : 4);
        skips = draw_frame ? 0 : (uint8_t)(skips + 1);
        scummLoop(delta);
        loop_count++;
    }
}

/* сохранение игры (saveload.c): курсор образа для sv_heap банков A и B */
uint8_t sv_page;
uint16_t sv_off;
