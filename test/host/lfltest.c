/* M0: файлы игры — MM v1 DOS с правками порта (длины и суммы — test/m0sum.h).
 * Несовпадение — другая версия игры или правки tools/lfl_patch.py не наложены
 * (их накладывает сборка образа) */
#include <stdio.h>
#include "m0sum.h"

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    unsigned bad = 0;
    for (unsigned i = 0; i < LFL_FILES; i++)
    {
        char p[512];
        snprintf(p, sizeof p, "%s/%02u.LFL", dir, i);
        FILE *f = fopen(p, "rb");
        if (!f)
        {
            printf("%02u.LFL: нет файла\n", i);
            bad++;
            continue;
        }
        unsigned len = 0;
        uint16_t sum = 0;
        int c;
        while ((c = fgetc(f)) != EOF)
        {
            sum = (uint16_t)(sum + (c ^ 0xFF));
            len++;
        }
        fclose(f);
        if (len != lfl_len[i] || sum != lfl_sum[i])
        {
            printf("%02u.LFL: длина %u сумма %u, ожидалось %u %u\n", i, len, sum, lfl_len[i], lfl_sum[i]);
            bad++;
        }
    }
    printf("lfl: %u файлов, расхождений %u\n", LFL_FILES, bad);
    return bad != 0;
}
