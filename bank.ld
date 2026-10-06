/* Банк кода в странице СМК с 0100000: первой идёт таблица переходов, bss — внутри файла */
OUTPUT_FORMAT("a.out-pdp11")
INCLUDE sys.ld
SECTIONS
{
    . = 0100000;
    .text : { *(.text) }
    /* данные банка: сохранение игры (saveload.c) пишет их целиком */
    .data : { _bdata_start = .; *(.data) *(.bss) *(COMMON) . = ALIGN(2); _bdata_end = .; }
}
