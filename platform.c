/* Платформа БК (ядро): клавиатура и виртуальная мышь, таймер в «джиффи» 60 Гц,
 * генератор случайных чисел ScummVM (common/random.cpp). */
#include "vm.h"

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
        /* буква в любом регистре (ЛАТ: 0101..0136, 0141..0176; РУС — КОИ-7 на тех же
         * местах: я = q, ч = ^) -> строчная: Ш Э Щ Ч — { | } ~ (клавиши глаголов, verbs.c) */
        if ((k & 037) >= 1 && (k & 037) <= 30 && (k & 0100)) k = (uint8_t)(0140 | (k & 037));
        return k;
    }
    if (held_arrow && !(SYSREG & 0100)) move_mouse(mx, my, held_arrow);
    else held_arrow = 0;
    return 0;
}

/* кэш байт-кода VM (script.c): не в состоянии игры (state.c) — после загрузки
 * всё равно сбрасывается. Теги строк — тоже здесь: в данных банка A они ушли бы в
 * сохранение без самих строк и поколения */
uint8_t vm_c_page, *vm_c_line;
uint16_t vm_c_addr, vm_c_gen = 1;   /* поколение 0 — у пустых тегов */
uint8_t vm_lines[VM_LINES][VM_CACHE];
uint8_t vm_l_page[VM_LINES];
uint16_t vm_l_addr[VM_LINES], vm_l_gen[VM_LINES];
uint8_t vm_sb_num[VM_SB], vm_sb_page[VM_SB];
uint16_t vm_sb_addr[VM_SB], vm_sb_gen[VM_SB];

/* палитра БК-0011М комнаты (gfx_v2.c gfx_set_pal): узоры 2x2 цветов EGA и сплошные
 * цвета текста — в ядре: их читают фон и текст (банк C) и актёры (банк B) */
uint8_t cur_pat[16][4];
uint8_t cur_tpal[16];
uint8_t pal_cur, pal_want;   /* включена в 0177662; нужна комнате (включит gfx_view_flush) */
uint8_t ui_repaint;          /* палитра сменилась: интерфейс — заново (scumm.c) */

/* RandomSource::getRandomNumber, но дёшево: xorshift16 вместо 32-битного LCG, а
 * 0..max — старшее слово x * (max + 1) сдвигами и сложениями (32-битные умножение
 * и остаток libgcc стоили ~1 мс на вызов) */
static uint16_t rand_x = 0xACE1;
uint16_t getRandomNumber(uint16_t max)
{
    uint16_t x = rand_x;
    x ^= (uint16_t)(x << 7);
    x ^= x >> 9;
    x ^= (uint16_t)(x << 8);
    rand_x = x;
    uint16_t m = (uint16_t)(max + 1u);
    if (!m) return x;
    uint32_t a = x, r = 0;
    for (; m; m >>= 1, a <<= 1)
    {
        uint16_t b = m & 1;
        __asm__ volatile("" : "+r"(b));   /* иначе gcc узнаёт умножение и зовёт __mulsi3 */
        if (b) r += a;
    }
    return (uint16_t)(r >> 16);
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
