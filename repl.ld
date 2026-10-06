/* Общая копия кода: одинакова во всех страницах СМК с 0160000; записываемых данных нет */
OUTPUT_FORMAT("a.out-pdp11")
INCLUDE sys.ld
SECTIONS
{
    . = 0160000;
    .text : { *(.text) *(.data) }
    .bss : { *(.bss) *(COMMON) ASSERT(SIZEOF(.bss) == 0, "repl: записываемые данные запрещены"); }
}
