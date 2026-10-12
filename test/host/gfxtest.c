/* M2: фон и маска всех комнат — суммы видимой части при x = 0 (её строит
 * room_decode; остальное в странице — от прежних комнат), как у tools/v1gfx.py all;
 * PNG по запросу */
#include <stdio.h>
#include <stdlib.h>
#include "scumm.h"
#include "gfx.h"

extern const char *host_data_dir;
extern uint8_t smk_mem[16][0100000];
void png_bk(const char *path, const uint8_t *bm, uint16_t stride, uint16_t w, uint16_t h);

/* сумма и Флетчер-16 байт [o, o + n) каждой из h строк по stride (накопительно) */
static unsigned ac, f1, f2;
static void sums_reset(void) { ac = f1 = f2 = 0; }
static void sums_add(const uint8_t *p, uint16_t stride, uint16_t o, uint16_t n, uint16_t y)
{
    for (uint16_t i = 0; i < n; i++)
    {
        uint8_t b = p[y * stride + o + i];
        ac = (ac + b) & 0xFFFF;
        f1 = (f1 + b) % 255;
        f2 = (f2 + f1) % 255;
    }
}

/* вид с полосы st (как tools/v1gfx.py view): при st = 0 — видимые байты и полосы
 * подряд, иначе — полные группы по слотам кольца */
static void view(uint8_t pg, uint16_t a, uint16_t ws, uint16_t h, uint16_t st, uint16_t r, uint16_t w)
{
    screenStartStrip = (int16_t)st;
    room_decode(pg, a);
    uint16_t g0 = st / 5, g1 = ((st + 40 < ws ? st + 40 : ws) - 1) / 5;
    uint16_t bs, bf, ms, mf;
    uint8_t ring = (ws + 4) / 5 > RING_G;
    for (int part = 0; part < 2; part++)
    {
        sums_reset();
        for (uint16_t y = 0; y < h; y++)
            if (st == 0)
            {
                uint16_t nb = (uint16_t)((g1 + 1) * 8), vis = ws < 40 ? ws : 40;
                if (nb > bm_stride) nb = bm_stride;
                if (!part) sums_add(smk_mem[PG_BITMAP], bm_stride, 0, nb, y);
                else sums_add(smk_mem[PG_MASK], bm_strips, 0, vis, y);
            }
            else
                for (uint16_t g = g0; g <= g1; g++)
                {
                    if (g * 5 + 5 > ws) continue;
                    uint16_t q = ring ? g % RING_G : g;
                    if (!part) sums_add(smk_mem[PG_BITMAP], bm_stride, (uint16_t)(q * 8), 8, y);
                    else sums_add(smk_mem[PG_MASK], bm_strips, (uint16_t)(q * 5), 5, y);
                }
        if (!part) { bs = (uint16_t)ac; bf = (uint16_t)((f2 << 8) | f1); }
        else { ms = (uint16_t)ac; mf = (uint16_t)((f2 << 8) | f1); }
    }
    printf("room %u @%u %u %u %u %u %u %u %u\n", r, st, w, h, bm_stride, bs, bf, ms, mf);
}

int main(int argc, char **argv)
{
    if (argc > 1) host_data_dir = argv[1];
    int only = argc > 2 ? atoi(argv[2]) : 0;
    res_init();
    gfx_init_pal();
    const char *only_c = getenv("ONLY_COLOR");   /* только цвет v1 c — красным, прочее чёрным */
    if (only_c)
        for (uint8_t yp = 0; yp < 2; ++yp)
            for (uint8_t k = 0; k < 4; ++k)
                for (uint8_t c = 0; c < 32; ++c)
                    v2_pat[yp][k][c] = (uint8_t)((c == atoi(only_c) ? 3 : 0) << (k * 2));
    for (uint16_t r = 1; r < NUM_ROOMS; ++r)
    {
        char p[512];
        snprintf(p, sizeof p, "%s/%02u.LFL", host_data_dir, r);
        FILE *f = fopen(p, "rb");
        if (!f) continue;
        fclose(f);
        if (only && r != only) continue;
        uint8_t pg = res_get(rtRoom, r);
        uint16_t a = res_addr;
        uint16_t w = far_rb(pg, a + 4) * 8, h = far_rb(pg, a + 5) * 8;   /* v1 */
        uint16_t bw = BK_W(w), ws = w / 8;
        view(pg, a, ws, h, 0, r, w);
        if (argc > 3) png_bk(argv[3], smk_mem[PG_BITMAP], bm_stride, bw, h);
        if (ws > 40) view(pg, a, ws, h, (uint16_t)(ws - 40), r, w);
    }
    return 0;
}
