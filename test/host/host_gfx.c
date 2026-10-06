/* Си-модель gfxv2.s: та же логика по сериям, буферы — те же массивы */
#include <string.h>
#include "scumm.h"
#include "gfx.h"

extern uint8_t smk_mem[16][0100000];
uint8_t v2_pat[2][4][32];
uint8_t v2_dt[128];
uint8_t v2_col[4][128];

static uint16_t run;
static int16_t color;   /* -1 — повтор */
static uint8_t *src;

static void fetch(void)
{
    uint8_t d = *src++;
    if (d & 0x80) { run = d & 0x7f; color = -1; }
    else { run = d >> 4; color = d & 0x0f; }
    if (run == 0) run = *src++;
}

static void flush(const struct v2p *p, uint16_t *dest, uint8_t *filled)
{
    static const uint8_t fmask[16] = { 0, 03, 014, 017, 060, 063, 074, 077, 0300, 0303, 0314, 0317, 0360, 0363, 0374, 0377 };
    uint8_t m = fmask[*filled];
    uint8_t *d = &smk_mem[PG_BITMAP][*dest - 0100000u];
    uint8_t yp = p->yodd & 1;
    for (uint16_t y = 0; y < p->h; ++y, d += p->stride, yp ^= 1)
    {
        uint8_t b = v2_pat[yp][0][v2_col[0][y] & 31] | v2_pat[yp][1][v2_col[1][y] & 31]
                  | v2_pat[yp][2][v2_col[2][y] & 31] | v2_pat[yp][3][v2_col[3][y] & 31];
        *d = m == 0377 ? b : (uint8_t)((*d & ~m) | (b & m));
    }
    ++*dest;
    *filled = 0;
}

uint16_t v2_pix(const struct v2p *p)
{
    src = &smk_mem[p->spage][p->src - 0100000u];
    const uint8_t *s0 = src;
    run = 0;
    color = 0;
    memset(v2_dt, 0, sizeof v2_dt);
    uint16_t cols = p->w, m5 = p->m5, dest = p->dest;
    uint8_t filled = 0;
    while (cols)
    {
        --cols;
        uint8_t *dt = v2_dt, *c = m5 == 4 ? 0 : v2_col[m5];
        if (c) filled |= (uint8_t)(1 << m5);
        for (uint16_t rows = p->h; rows;)
        {
            if (!run) fetch();
            uint16_t n = run < rows ? run : rows;
            run -= n;
            rows -= n;
            for (; n; --n)
            {
                if (color >= 0) *dt = (uint8_t)color;
                if (c) *c++ = *dt;
                ++dt;
            }
        }
        if (m5 == 4)
        {
            if (!cols && filled) flush(p, &dest, &filled);
            m5 = 0;
            continue;
        }
        if (m5 == 3 || !cols) flush(p, &dest, &filled);
        ++m5;
    }
    return (uint16_t)(p->src + (src - s0));
}

uint16_t v2_mask(struct v2m *p)
{
    const uint8_t *s = &smk_mem[p->spage][p->src - 0100000u], *s0 = s;
    uint16_t n = 0;
    uint8_t rep = 0, data = 0;
    for (uint16_t k = p->nstrips; k; --k)
    {
        for (uint16_t y = 0; y < p->h; ++y)
        {
            if (!n)
            {
                n = *s++;
                rep = 0;
                if (n & 0x80) { n &= 0x7f; rep = 1; data = *s++; }
            }
            --n;
            if (!rep) data = *s++;
            smk_mem[PG_MASK][p->dest - 0100000u + y * p->stride] = data;
        }
        ++p->dest;
    }
    return (uint16_t)(p->src + (s - s0));
}
