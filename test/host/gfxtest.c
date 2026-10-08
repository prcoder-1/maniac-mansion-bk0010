/* M2: фон и маска всех комнат — суммы, как у tools/v2gfx.py all; PNG по запросу */
#include <stdio.h>
#include <stdlib.h>
#include "scumm.h"
#include "gfx.h"

extern const char *host_data_dir;
extern uint8_t smk_mem[16][0100000];
void png_bk(const char *path, const uint8_t *bm, uint16_t stride, uint16_t w, uint16_t h);

int main(int argc, char **argv)
{
    if (argc > 1) host_data_dir = argv[1];
    int only = argc > 2 ? atoi(argv[2]) : 0;
    res_init();
    gfx_init_pal();
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
        uint16_t bw = BK_W(w);
        room_decode(pg, a);
        uint16_t s1 = 0, s2 = 0;
        for (uint16_t i = 0; i < bm_stride * h; ++i) s1 += smk_mem[PG_BITMAP][i];
        for (uint16_t i = 0; i < bm_strips * h; ++i) s2 += smk_mem[PG_MASK][i];
        printf("room %u %u %u %u %u %u\n", r, w, h, bm_stride, s1, s2);
        if (argc > 3) png_bk(argv[3], smk_mem[PG_BITMAP], bm_stride, bw, h);
    }
    return 0;
}
