/* Служебный текст: шрифт 8x8 (font_pc.s), 32 колонки по 8 точек, 2 бита на точку */
#include "con.h"

extern const uint8_t font[];

static const uint8_t spread[16] =
{
    0, 03, 014, 017, 060, 063, 074, 077, 0300, 0303, 0314, 0317, 0360, 0363, 0374, 0377,
};

/* color: 1 синий, 2 зелёный, 3 красный */
void con_puts(uint8_t row, uint8_t col, const char *s, uint8_t color)
{
    uint8_t m = (uint8_t)(color * 0125);
    volatile uint8_t *p = VRAM + (uint16_t)row * (8 * 64) + (uint16_t)col * 2;
    for (; *s; ++s, p += 2)
    {
        uint8_t c = (uint8_t)*s;
        if (c >= 'a' && c <= 'z') c -= 040;
        if (c < 040 || c > 0137) c = ' ';
        const uint8_t *g = font + ((uint16_t)(c - 040) << 3);
        volatile uint8_t *d = p;
        for (uint8_t r = 0; r < 8; ++r, d += 64)
        {
            d[0] = spread[g[r] & 15] & m;
            d[1] = spread[g[r] >> 4] & m;
        }
    }
}

void con_oct(uint8_t row, uint8_t col, uint16_t v, uint8_t color)
{
    char s[7];
    for (int8_t i = 5; i >= 0; --i)
    {
        s[i] = (char)('0' + (v & 7));
        v >>= 3;
    }
    s[6] = 0;
    con_puts(row, col, s, color);
}

void con_clear(void)
{
    volatile uint16_t *p = (volatile uint16_t *)VRAM;
    for (uint16_t n = 020000; n; --n) *p++ = 0;
}
