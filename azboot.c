/* AZБК при пуске (банк D, из main.c): проба платы, модуль azmod — в страницу 6 ОЗУ 11М
 * (через страницу кэша, как драйвер звука), видеорежим. Без платы или без файла
 * AZMOD — игра как на БК-0011М (az_on = 0). */
#include "scumm.h"
#include "con.h"
#include "snddrv.h"
#include "az.h"

uint16_t az_probe(void);   /* azprobe.s */
extern uint16_t code_ft[7][3];

void az_boot(void)
{
    az_on = 0;
    if (!az_probe()) return;
    const uint16_t *f = code_ft[6];
    struct ld_req q = { "AZMOD", 0, PG_CACHE, PAGE_ADDR, PAGE_DATA_SIZE, PG_MASK, OVF_ADDR, f[0], f[1], f[2] };
    int16_t n = ld_file(&q);
    if (n <= 0 || (uint16_t)n > AZ_MOD_END - AZ_MOD_ADDR) return;
    w1_copy(AZ_PAGE11, PG_CACHE, (uint16_t)n);
    /* Запись 0177662 (экранная страница БК) у AZБК ставит страницу слоя 0, 0177664 — его
     * вертикальную прокрутку и «малый экран»: загрузчик (гасит кадровое прерывание на
     * время чтения) и «СТОП» пишут их, а слой 0 — курсор. Экран БК не нужен: записи — в
     * адрес ячейки палитры AZ (безвредно); прерывание и так закрыто (PSW 0340). */
    for (uint16_t *p = (uint16_t *)01000; p < (uint16_t *)02776; p++)
        if (*p == 0177662 || *p == 0177664) *p = 0177234;
    az_on = 1;
    azc(AZ_INIT);
    con_puts(12, 8, "AZBK: VGA", 3);   /* экран самой БК дальше не обновляется */
}
