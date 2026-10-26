/* Платформа БК (ядро): клавиатура и виртуальная мышь, таймер в «джиффи» 60 Гц,
 * генератор случайных чисел ScummVM (common/random.cpp). */
#include "scumm.h"

#define KBD_CSR  (*(volatile uint16_t *)0177660)
#define KBD_DATA (*(volatile uint16_t *)0177662)
#define SYSREG   (*(volatile uint16_t *)0177716)
#define TMR_LIMIT (*(volatile uint16_t *)0177706)
#define TMR_COUNT (*(volatile uint16_t *)0177710)
#define TMR_CSR   (*(volatile uint16_t *)0177712)

/* коды клавиш БК */
#define K_LEFT 010
#define K_RIGHT 031
#define K_UP 032
#define K_DOWN 033
#define K_ENTER 012
#define K_KT 003      /* КТ — пропуск катсцены (ESC) */

static uint8_t held_arrow;
uint8_t kbd_buf[16];
uint16_t kbd_head, kbd_tail;
extern uint16_t kbd_irq_on;

/* клавиатура по прерыванию (crt0.s kbd_irq): ни одна клавиша не теряется */
void kbd_init(void)
{
    kbd_head = kbd_tail = 0;
    kbd_irq_on = 1;
    KBD_CSR &= ~0100;
    __asm__ volatile("mtps $0");
}

static void move_mouse(int16_t *mx, int16_t *my, uint8_t k)
{
    switch (k)
    {
    case K_LEFT: *mx -= 8; break;
    case K_RIGHT: *mx += 8; break;
    case K_UP: *my -= 4; break;
    case K_DOWN: *my += 4; break;
    }
}

/* ввод за кадр: стрелки двигают курсор (и повторяются, пока клавиша нажата),
 * ВВОД — щелчок левой, КТ — ESC; буквы — строчные ASCII (клавиши глаголов) */
uint16_t input_poll(int16_t *mx, int16_t *my, uint8_t *click)
{
    *click = 0;
    if (kbd_head != kbd_tail)
    {
        uint8_t k = kbd_buf[kbd_tail] & 0177;
        kbd_tail = (kbd_tail + 1) & 15;
        held_arrow = 0;
        switch (k)
        {
        case K_LEFT:
        case K_RIGHT:
        case K_UP:
        case K_DOWN:
            held_arrow = k;
            move_mouse(mx, my, k);
            return 0;
        case K_ENTER:
            *click = 1;
            return 0;
        case K_KT:
            return 27;
        }
        if (k >= 'A' && k <= 'Z') k += 'a' - 'A';
        if (k >= 0140 && k <= 0177) k -= 0140 - 'a' + 'a';   /* регистр строчных БК */
        return k;
    }
    if (held_arrow && !(SYSREG & 0100)) move_mouse(mx, my, held_arrow);
    else held_arrow = 0;
    return 0;
}

/* common/random.cpp: RandomSource::getRandomNumber */
static uint32_t rand_seed = 0x12345678;
uint16_t getRandomNumber(uint16_t max)
{
    rand_seed = 0xDEADBF03UL * (rand_seed + 1);
    rand_seed = (rand_seed >> 13) | (rand_seed << 19);
    return (uint16_t)(rand_seed % (max + 1UL));
}

void trace_opcode(uint16_t script, uint16_t offs, uint8_t op) { (void)script; (void)offs; (void)op; }
void trace_write_var(uint16_t var, int16_t value) { (void)var; (void)value; }

/* таймер ВИ1 (31250 Гц на 11М) — свободный счёт, по нему звук (snd.s) */
static uint16_t jprev;
void timer_init(void)
{
    TMR_LIMIT = 0177777;
    TMR_CSR = 020;
    jprev = jiffies;
}

/* прошедшие джиффи (60 Гц) с прошлого вызова — по кадровому прерыванию (irq.s) */
uint16_t timer_jiffies(void)
{
    uint16_t j = jiffies, d = j - jprev;
    jprev = j;
    return d;
}
