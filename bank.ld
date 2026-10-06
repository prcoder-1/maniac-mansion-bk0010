/* Банк кода в странице СМК с 0100000: первой идёт таблица переходов, bss — внутри файла */
OUTPUT_FORMAT("a.out-pdp11")
INCLUDE sys.ld
SECTIONS
{
    . = 0100000;
    .text : { *(.text) }
    .data : { *(.data) *(.bss) *(COMMON) . = ALIGN(2); }
}
