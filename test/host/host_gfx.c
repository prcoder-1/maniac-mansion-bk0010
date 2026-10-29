/* Си-модель gfxv2.s: та же логика по сериям, буферы — те же массивы */
#include <string.h>
#include "scumm.h"
#include "gfx.h"

extern uint8_t smk_mem[16][0100000];
uint8_t v2_pat[2][4][32];
uint8_t v2_dt[128];
uint8_t v2_col[4][128];
uint8_t v2_strips[120][4], v2_zstrips[120][4];

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
    uint8_t *d = &smk_mem[PG_BITMAP][*dest + p->doff - 0100000u];
    uint8_t yp = p->yodd & 1;   /* чётность строки ry0 */
    for (uint16_t y = p->ry0; y < p->ry1; ++y, d += p->stride, yp ^= 1)
    {
        uint8_t b = v2_pat[yp][0][v2_col[0][y] & 31] | v2_pat[yp][1][v2_col[1][y] & 31]
                  | v2_pat[yp][2][v2_col[2][y] & 31] | v2_pat[yp][3][v2_col[3][y] & 31];
        *d = m == 0377 ? b : (uint8_t)((*d & ~m) | (b & m));
    }
    ++*dest;
    *filled = 0;
}

uint16_t v2_pix(struct v2p *p)
{
    src = &smk_mem[p->spage][p->src - 0100000u];
    const uint8_t *s0 = src;
    run = p->run0;
    color = (int16_t)p->color0;
    memset(v2_dt, 0, sizeof v2_dt);
    uint16_t cols = p->w, m5 = p->m5, dest = p->dest, m8 = p->m8;
    uint8_t *tab = p->tab ? &v2_strips[0][0] : 0;   /* на хосте адрес не 16-битный */
    uint8_t filled = 0;
    while (cols)
    {
        --cols;
        if (tab && m8 == 0)
        {
            uint16_t a = (uint16_t)(p->src + (src - s0));
            *tab++ = (uint8_t)a;
            *tab++ = (uint8_t)(a >> 8);
            *tab++ = (uint8_t)run;
            *tab++ = (uint8_t)color;
        }
        m8 = (m8 + 1) & 7;
        uint8_t *dt = v2_dt, *c = m5 == 4 ? 0 : v2_col[m5];
        if (c) filled |= (uint8_t)(1 << m5);
        for (uint16_t y = 0; y < p->h;)   /* пишутся только строки [ry0, ry1) */
        {
            if (!run) fetch();
            uint16_t lim = y < p->ry0 ? p->ry0 : y < p->ry1 ? p->ry1 : p->h;
            uint16_t n = run < lim - y ? run : lim - y;
            uint8_t wr = y >= p->ry0 && y < p->ry1;
            run -= n;
            for (; n; --n, ++y)
            {
                if (wr && color >= 0) dt[y] = (uint8_t)color;
                if (wr && c) c[y] = dt[y];
            }
        }
        if (m5 == 4)
        {
            if ((filled & 014) == 014)   /* точка 3 — из колонки 4, если колонка 3 как колонка 2 */
                for (uint16_t y = p->ry0; y < p->ry1; ++y)
                    if (v2_col[3][y] == v2_col[2][y]) v2_col[3][y] = v2_dt[y];
            if (filled) flush(p, &dest, &filled);
            m5 = 0;
            continue;
        }
        if (!cols) flush(p, &dest, &filled);
        ++m5;
    }
    return (uint16_t)(p->src + (src - s0));
}

uint16_t v2_mask(struct v2m *p)
{
    const uint8_t *s = &smk_mem[p->spage][p->src - 0100000u], *s0 = s;
    uint16_t n = p->run0;
    uint8_t rep = (uint8_t)p->flag0, data = (uint8_t)p->data0;
    uint8_t *tab = p->tab ? &v2_zstrips[0][0] : 0;
    for (uint16_t k = p->nstrips; k; --k)
    {
        if (tab)
        {
            uint16_t a = (uint16_t)(p->src + (s - s0));
            *tab++ = (uint8_t)a;
            *tab++ = (uint8_t)(a >> 8);
            *tab++ = (uint8_t)(n | (rep << 7));
            *tab++ = data;
        }
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

/* Си-модель blit.s и экран БК: 256 строк по 64 байта */
uint8_t host_vram[040000];

void bm_blit(uint16_t src, uint16_t stride, uint16_t dst, uint16_t ncols, uint16_t rows)
{
    for (uint16_t r = 0; r < rows; r++)
        for (uint16_t c = 0; c < ncols; c++)
            host_vram[dst - 040000u + r * 64 + c] = smk_mem[PG_BITMAP][src - 0100000u + r * stride + c];
}
uint8_t band[2048];
uint8_t div5[1024];
__attribute__((constructor)) static void div5_init(void) { for (int i = 0; i < 1024; i++) div5[i] = (uint8_t)(i / 5); }

/* proc3.s: внеэкранный буфер актёра в PG_MASK */
void cv_load(uint16_t cv, uint16_t v, uint16_t ncols, uint16_t nrows, uint16_t pad)
{
    for (uint16_t y = 0; y < nrows; y++)
        for (uint16_t x = 0; x < ncols; x++) far_wb(5, cv + y * (ncols + pad) + x, host_vram[v - 040000 + y * 64 + x]);
}
void cv_store(uint16_t cv, uint16_t v, uint16_t ncols, uint16_t nrows, uint16_t pad)
{
    for (uint16_t y = 0; y < nrows; y++)
        for (uint16_t x = 0; x < ncols; x++) host_vram[v - 040000 + y * 64 + x] = far_rb(5, cv + y * (ncols + pad) + x);
}
void cv_bg(uint16_t cv, uint16_t cvstride, uint16_t bm, uint16_t bmstride, uint16_t ncols, uint16_t nrows)
{
    for (uint16_t y = 0; y < nrows; y++)
        for (uint16_t x = 0; x < ncols; x++) far_wb(5, cv + y * cvstride + x, far_rb(4, bm + y * bmstride + x));
}
