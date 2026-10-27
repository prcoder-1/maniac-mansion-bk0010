/ Вход (01000). ANDOS грузит MANIAC с автостартом: 8 слов 0760..0776 = 01000.
	.include "smk.inc"
	.text
	.globl _main, ___main, _start, ___bss_start, ___bss_end, _boot_psw, _boot_tmr, _cur_page, _rst, restart
	.globl _boot_v60, _boot_v274, _boot_v100, _irq_frame, kbd_irq
_start:
	mfps	_boot_psw
	mtps	$0340			/ прерывания только по необходимости (ПЗУ в ОЗУ10 скрыто)
	cmp	@$04, $_rst		/ повторный пуск: вектор 4 уже наш
	beq	restart
	mov	@$060, _boot_v60	/ векторы ДОС: монитор — клавиатуру
	mov	@$062, _boot_v60+2
	mov	@$0274, _boot_v274
	mov	@$0276, _boot_v274+2
	mov	@$0100, _boot_v100
	mov	@$0102, _boot_v100+2
	mov	@$0177706, _boot_tmr
	mov	@$0177712, _boot_tmr+2
/ повторный пуск (_rst, loader.s): данные ядра — из снимка, векторы ДОС — уже взяты
restart:
	mov	$040000, sp		/ стек Си — вверху базового ОЗУ
	mov	$01330, @$0177664
	mov	$_rst, @$4		/ клавиша «СТОП», HALT, зависание: игра заново (_rst)
	mov	$0340, @$06
	mov	$010200, @$0177716	/ и аппаратно (БК-0011М, регистр магнитофона: разряд 11 = 0): разряд 12 —
					/ «СТОП» не доходит до шины, СМК не сбрасывается по ОСТ; 7 — двигатель стоит
	mov	$kbd_irq, @$060		/ клавиатура, АР2, кадровое (irq.s): обработчиков ПЗУ не видно
	mov	$0340, @$062
	mov	$kbd_irq, @$0274
	mov	$0340, @$0276
	mov	$_irq_frame, @$0100
	mov	$0340, @$0102
	bis	$0100, @$0177660	/ клавиатура — опросом
	mov	$___bss_start, r0
3:	cmp	r0, $___bss_end
	bhis	2f
	clr	(r0)+
	br	3b
2:	jsr	pc, _main
	halt

___main:
	rts	pc
