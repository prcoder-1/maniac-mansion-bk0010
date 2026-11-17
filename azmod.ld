/* Модуль AZБК: страница 6 ОЗУ БК-0011М в окне 040000 (az.h), первой — таблица входов
 * (azhdr.s), bss — внутри файла */
OUTPUT_FORMAT("a.out-pdp11")
INCLUDE sys.ld
SECTIONS
{
    . = 040000;
    .text : { azhdr.o(.text) *(.text) }
    .data : { *(.data) *(.bss) *(COMMON) . = ALIGN(2); }
}
