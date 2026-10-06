/* Ядро (MANIAC.BIN): с 01000; загрузчик обязан кончиться до дальнего стека (03000),
 * остальное — с 03200 (эта часть сохраняется в СМК на время чтения файла) */
OUTPUT_FORMAT("a.out-pdp11")
ENTRY(_start)
INCLUDE sys.ld
SECTIONS
{
    . = 01000;
    .text :
    {
        crt0.o(.text) loader.o(.text) loader.o(.data)
        _loader_end = .;
        . = 03200;
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
ASSERT(_loader_end <= 03000, "loader: область загрузчика переполнена (до 03000)")
