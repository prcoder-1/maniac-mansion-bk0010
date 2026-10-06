/* Проверка StripTable: перерисовка полос [s0, s1) даёт те же фон и маску, что и
 * полное декодирование комнаты — для всех комнат и всех диапазонов из 1..5 полос */
#include <stdio.h>
#include <string.h>
#include "scumm.h"
#include "gfx.h"

extern const char *host_data_dir;
extern uint8_t smk_mem[16][0100000];

int main(int argc, char **argv)
{
    if (argc > 1) host_data_dir = argv[1];
    res_init();
    gfx_init_pal();
    static uint8_t bm[060000], mk[060000];
    unsigned bad = 0, checks = 0;
    for (uint16_t r = 1; r < NUM_ROOMS; r++)
    {
        char p[512];
        snprintf(p, sizeof p, "%s/%02u.LFL", host_data_dir, r);
        FILE *f = fopen(p, "rb");
        if (!f) continue;
        fclose(f);
        room_page = res_get(rtRoom, r);
        room_addr = res_addr;
        roomHeight = far_rw(room_page, room_addr + 6);
        room_decode(room_page, room_addr);
        memcpy(bm, smk_mem[PG_BITMAP], sizeof bm);
        memcpy(mk, smk_mem[PG_MASK], sizeof mk);
        for (uint8_t s0 = 0; s0 < bm_strips; s0 += 3)
            for (uint8_t n = 1; n <= 5; n += 2)
            {
                memset(smk_mem[PG_BITMAP], 0x55, sizeof bm);   /* мусор под перерисовку */
                memcpy(smk_mem[PG_BITMAP], bm, sizeof bm);
                uint16_t x0 = s0 * 8u, x1 = (uint16_t)((s0 + n) * 8u);
                if (x1 > bm_strips * 8u) x1 = bm_strips * 8u;
                for (uint16_t y = 0; y < roomHeight; y++)   /* испортить полосы */
                    for (uint16_t x = x0; x < x1; x++)
                        if (x % 5 != 4) smk_mem[PG_BITMAP][y * bm_stride + ((x - x / 5) >> 2)] ^= (uint8_t)(3 << (((x - x / 5) & 3) * 2));
                for (uint16_t y = 0; y < roomHeight; y++)
                    for (uint8_t s = s0; s < s0 + n && s < bm_strips; s++) smk_mem[PG_MASK][y * bm_strips + s] ^= 0xFF;
                room_redraw_strips(s0, (uint8_t)(s0 + n));
                checks++;
                if (memcmp(bm, smk_mem[PG_BITMAP], bm_stride * roomHeight) || memcmp(mk, smk_mem[PG_MASK], bm_strips * roomHeight))
                {
                    if (bad < 10) printf("комната %u полосы %u..%u: расхождение\n", r, s0, s0 + n);
                    bad++;
                }
            }
    }
    printf("striptest: %u проверок, %u расхождений\n", checks, bad);
    return bad != 0;
}
