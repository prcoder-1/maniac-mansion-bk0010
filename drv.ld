/* Звуковой драйвер: страница 1 ОЗУ БК-0011М в окне 040000 (snddrv.h), bss — внутри файла */
OUTPUT_FORMAT("a.out-pdp11")
INCLUDE sys.ld
SECTIONS
{
    . = 040000;
    .text : { drvhdr.o(.text) *(.text) }
    .data : { *(.data) *(.bss) *(COMMON) . = ALIGN(2); }
}
