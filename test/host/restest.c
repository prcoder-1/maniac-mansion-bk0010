/* M1: ресурсы через кэш страниц — тот же вывод, что tools/lfl_stats.py */
#include <stdio.h>
#include <stdlib.h>
#include "scumm.h"

extern const char *host_data_dir;
extern unsigned host_loads, host_load_bytes;

static void dump(const char *name, uint8_t type, uint16_t idx)
{
    uint8_t pg = res_get(type, idx);
    uint16_t a = res_addr;
    if (!pg)
    {
        printf("%s %u BAD\n", name, idx);
        return;
    }
    uint16_t size = far_rw(pg, a), sum = 0;
    for (uint16_t i = 0; i < size; ++i) sum += far_rb(pg, a + i);
    printf("%s %u %u %u\n", name, idx, size, sum);
}

int main(int argc, char **argv)
{
    if (argc > 1) host_data_dir = argv[1];
    res_init();
    for (uint16_t r = 1; r < NUM_ROOMS; ++r)
    {
        char p[512];
        snprintf(p, sizeof p, "%s/%02u.LFL", host_data_dir, r);
        FILE *f = fopen(p, "rb");
        if (!f) continue;
        fclose(f);
        dump("room", rtRoom, r);
    }
    for (uint16_t i = 1; i < res_num(rtCostume); ++i) dump("costume", rtCostume, i);
    for (uint16_t i = 1; i < res_num(rtScript); ++i) dump("script", rtScript, i);
    for (uint16_t i = 1; i < res_num(rtSound); ++i) dump("sound", rtSound, i);
    fprintf(stderr, "загрузок файлов: %u, байт: %u\n", host_loads, host_load_bytes);
    return 0;
}
