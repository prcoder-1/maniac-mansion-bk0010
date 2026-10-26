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

/* две экранные страницы: видимая; перерисовка в скрытой идёт (gfx.c scr_begin);
 * поколение вывода в строки реплики и интерфейса и с какого поколения они в
 * каждой странице (5, 6) — копировать ли их в скрытую */
uint8_t scr_shown = 5, flip_pending;
uint16_t ui_gen = 1, pg_ui_gen[2], vctl_next;

/* конец шага: перерисованная страница — на экран (с палитрой) в кадровом прерывании */
void scr_flip(void)
{
    if (!flip_pending) return;
    vctl_flip(vctl_next);
    while (vctl_req) __asm__ volatile("wait");
    scr_shown ^= 5 ^ 6;
    flip_pending = 0;
}

/* Джойстик на параллельном порту 0177714 (раскладка «Стандарт»: нажато = 1; ВВЕРХ 001,
 * ВПРАВО 002, ВНИЗ 004, ВЛЕВО 010; A/X 040/0100 — щелчок, B/Y и Start 0200/0400/020 —
 * правая кнопка, в v0-v3 — ESC) и мышь «Марсианка» (УВК-01: шаги шара — на тех же
 * разрядах направлений, кнопки 040/0100). Порт читается в кадровом прерывании (irq.s,
 * 48.8 Гц): курсор — пока ручка отклонена, с разгоном; набранное забирает шаг цикла
 * (joy_take). Порт только читается — записи в AY не мешает. */
#define JOY_PORT (*(volatile uint16_t *)0177714)
static uint8_t joy_held, joy_fire;
static int16_t joy_dx, joy_dy;
static uint8_t joy_click;

void joy_irq(void)
{
    uint16_t v = JOY_PORT;
    if ((v & 005) == 005 || (v & 012) == 012) v = 0;   /* вверх+вниз, влево+вправо: не джойстик */
    if (v & 017)
    {
        if (joy_held < 255) joy_held++;
    }
    else
        joy_held = 0;
    uint8_t s = joy_held <= 12 ? 1 : joy_held <= 30 ? 2 : 4;   /* разгон: сначала точно */
    if (v & 010) joy_dx -= s;
    if (v & 002) joy_dx += s;
    if (v & 001) joy_dy -= s;
    if (v & 004) joy_dy += s;
    uint8_t f = (uint8_t)((v & 0140 ? 1 : 0) | (v & 0620 ? 2 : 0));
    joy_click |= (uint8_t)(f & ~joy_fire);
    joy_fire = f;
}

/* набранное прерыванием с прошлого шага: сдвиг курсора и щелчки (бит 0 — левая, 1 —
 * правая кнопка) */
uint8_t joy_take(int16_t *mx, int16_t *my)
{
    __asm__ volatile("mtps $0340");
    *mx += joy_dx;
    *my += joy_dy >> 1;   /* по вертикали — вдвое медленнее (точки MM — вдвое выше) */
    joy_dy &= 1;
    joy_dx = 0;
    uint8_t c = joy_click;
    joy_click = 0;
    __asm__ volatile("mtps $0");
    return c;
}


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
