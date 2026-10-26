/ Вход (01000). ANDOS грузит MANIAC с автостартом: 8 слов 0760..0776 = 01000.
	.text
	.globl _main, ___main, _start, ___bss_start, ___bss_end, _boot_psw, _boot_v4, _boot_tmr, _cur_page
	.globl _boot_v60, _boot_v274, _boot_v100, _irq_frame
_start:
	mfps	_boot_psw
	mtps	$0340			/ прерывания только по необходимости (ПЗУ в ОЗУ10 скрыто)
	cmp	@$04, $2		/ повторный пуск: вектор 4 уже наш
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
	mov	$0161716, @$2		/ клавиша «СТОП» заблокирована: по вектору 4 — SUB (PC),(SP)
	mov	$2, @$4			/ (PC в стеке после «СТОП» на 2 больше) и RTI (слово 2 по адресу 4)
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

/ прерывание клавиатуры: код в кольцо _kbd_buf (16 байт), голова _kbd_head
	.globl	_kbd_buf, _kbd_head, _kbd_tail
kbd_irq:
	mov	r0, -(sp)
	mov	_kbd_head, r0
	movb	@$0177662, _kbd_buf(r0)
	inc	r0
	bic	$0177760, r0
	cmp	r0, _kbd_tail
	beq	1f			/ полно — код теряется
	mov	r0, _kbd_head
1:	mov	(sp)+, r0
	rti

___main:
	rts	pc
