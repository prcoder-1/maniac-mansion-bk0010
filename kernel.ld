/* Ядро (MANIAC.BIN): с 01000; загрузчик обязан кончиться до дальнего стека (02200),
 * остальное — с 02400 (эта часть сохраняется в СМК на время чтения файла) */
OUTPUT_FORMAT("a.out-pdp11")
ENTRY(_start)
INCLUDE sys.ld
SECTIONS
{
    . = 01000;
    .text :
    {
        crt0.o(.text) loader.o(.text) loader.o(.data)
        ASSERT(. <= 02200, "loader: область загрузчика переполнена");
        . = 02400;
        *(.text)
    }
    .data : { *(.data) }
    .bss :
    {
        ___bss_start = .;
        *(.bss) *(COMMON)
        . = ALIGN(2);
        ___bss_end = .;
    }
    _kernel_end = .;
}
