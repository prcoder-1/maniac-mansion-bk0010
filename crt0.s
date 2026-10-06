/ Вход (01000). ANDOS грузит MANIAC с автостартом: 8 слов 0760..0776 = 01000.
	.text
	.globl _main, ___main, _start, ___bss_start, ___bss_end, _boot_psw
_start:
	mfps	_boot_psw
	mtps	$0340			/ прерывания только по необходимости (ПЗУ в ОЗУ10 скрыто)
	mov	$040000, sp		/ стек Си — вверху базового ОЗУ
	mov	$01330, @$0177664
	mov	$_start, @$04		/ «СТОП» — перезапуск
	mov	$0340, @$06
	bis	$0100, @$0177660	/ клавиатура — опросом
	mov	$___bss_start, r0
1:	cmp	r0, $___bss_end
	bhis	2f
	clr	(r0)+
	br	1b
2:	jsr	pc, _main
	halt

___main:
	rts	pc
