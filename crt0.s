/ Вход (01000). ANDOS грузит MANIAC с автостартом: 8 слов 0760..0776 = 01000.
	.text
	.globl _main, ___main, _start, ___bss_start, ___bss_end, _boot_psw, _boot_v4, _boot_tmr, _cur_page
	.globl _boot_v60, _boot_v274, _boot_v100
_start:
	mfps	_boot_psw
	mtps	$0340			/ прерывания только по необходимости (ПЗУ в ОЗУ10 скрыто)
	cmp	@$04, $trap4		/ после «СТОП» вектор уже наш
	beq	1f
	mov	@$04, _boot_v4		/ векторы ДОС: драйвер НГМД ловит тайм-аут шины,
	mov	@$06, _boot_v4+2	/ монитор — клавиатуру
	mov	@$060, _boot_v60
	mov	@$062, _boot_v60+2
	mov	@$0274, _boot_v274
	mov	@$0276, _boot_v274+2
	mov	@$0100, _boot_v100
	mov	@$0102, _boot_v100+2
	mov	@$0177706, _boot_tmr
	mov	@$0177712, _boot_tmr+2
1:
	mov	$040000, sp		/ стек Си — вверху базового ОЗУ
	mov	$01330, @$0177664
	mov	$trap4, @$04		/ ошибка шины / «СТОП»: остановка с кадром в 03000
	mov	$0340, @$06
	mov	$irq, @$060		/ клавиатура, АР2, 50 Гц: в ОЗУ10 обработчиков ПЗУ нет
	mov	$0340, @$062
	mov	$irq, @$0274
	mov	$0340, @$0276
	mov	$irq, @$0100
	mov	$0340, @$0102
	bis	$0100, @$0177660	/ клавиатура — опросом
	mov	$___bss_start, r0
3:	cmp	r0, $___bss_end
	bhis	2f
	clr	(r0)+
	br	3b
2:	jsr	pc, _main
	halt

trap4:	mov	(sp), @$03000		/ PC
	mov	2(sp), @$03002		/ PSW
	mov	sp, @$03004
	mov	_cur_page, @$03006
	br	.			/ HALT на БК сам уходит по вектору 4

irq:	rti

___main:
	rts	pc
