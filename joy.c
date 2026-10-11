/* Джойстик на параллельном порту 0177714 (банк B), раскладка «Стандарт» (Джойвокс):
 * нажато = 1; ВВЕРХ 001, ВПРАВО 002, ВНИЗ 004, ВЛЕВО 010; огонь A/X (040/0100) —
 * щелчок, B/Y и Start (0200/0400/020) — правая кнопка (в v0-v3 — ESC). */
#include "scumm.h"

#define JOY_PORT (*(volatile uint16_t *)0177714)

static uint8_t joy_held, joy_fire;

/* курсор — пока ручка отклонена (с разгоном), кнопки — по нажатию: бит 0 —
 * щелчок, 1 — правая кнопка */
uint8_t joy_poll(int16_t *mx, int16_t *my)
{
    uint16_t v = JOY_PORT;
    if ((v & 005) == 005 || (v & 012) == 012) v = 0;   /* вверх+вниз, влево+вправо: не джойстик */
    if (v & 017)
    {
        if (joy_held < 255) joy_held++;
    }
    else
        joy_held = 0;
    uint8_t s = joy_held <= 4 ? 4 : joy_held <= 10 ? 8 : 16;   /* разгон: сначала точно */
    if (v & 010) *mx -= s;
    if (v & 002) *mx += s;
    if (v & 001) *my -= s >> 1;
    if (v & 004) *my += s >> 1;
    uint8_t f = (uint8_t)((v & 0140 ? 1 : 0) | (v & 0620 ? 2 : 0));
    uint8_t click = (uint8_t)(f & ~joy_fire);
    joy_fire = f;
    return click;
}
