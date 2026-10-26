/ БК-0011М (ядро): кадровое прерывание, тени регистров 0177662/0177716, нули в хвосте
/ сегмента 3 страниц СМК.
/
/ Регистры 0177662 (палитра, экранная страница, разрешение кадрового прерывания) и
/ 0177716 (страницы ОЗУ 11М в окнах) только на запись: их значения — в тенях _vctl и
/ _mem11. Основной код сначала пишет тень, потом регистр — прерывание между этими
/ записями безвредно.
	.include "smk.inc"
	.globl _irq_frame, _vctl, _vctl_req, _mem11, _jiffies, _frames
	.globl _vctl_set, _vctl_flip, _smk_twins, _page_code, _cur_page
	.text

/ вектор 0100, 48.828125 Гц (3125/64): отложенная запись 0177662 — в гашении, джиффи
/ 60 Гц (+3840 на 3125: 07400, 06065), счётчик кадров
_irq_frame:
	tst	_vctl_req
	beq	1f
	mov	_vctl, @$0177662
	clr	_vctl_req
1:	add	$07400, jacc
2:	cmp	jacc, $06065
	blo	3f
	sub	$06065, jacc
	inc	_jiffies
	br	2b
3:	inc	_frames
	rti

/ void vctl_set(uint16_t v): записать 0177662 сразу
_vctl_set:
	mov	2(sp), _vctl
	mov	_vctl, @$0177662
	rts	pc

/ void vctl_flip(uint16_t v): записать 0177662 в ближайшем кадровом прерывании
_vctl_flip:
	mov	2(sp), _vctl
	mov	pc, _vctl_req
	rts	pc

/ void smk_twins(void): в режиме All сегмент 3 страницы виден на чтение и в
/ 0177000..0177737, поверх регистров БК — там должны быть нули. Пишутся они через ОЗУ10,
/ где сегмент 3 стоит в 0130000 (запись в ОЗУ ПЗУ БОС не мешает). Страницы 2..15.
_smk_twins:
	mov	r2, -(sp)
	mfps	-(sp)
	mtps	$0340
	mov	$4, r1			/ страница 2
1:	mov	_page_code(r1), r0
	bic	$0160, r0
	bis	$0120, r0		/ ОЗУ10
	mov	$6, @$0177130
	mov	r0, @$0177130
	mov	$0, @$0177130
	mov	$0137000, r0
	mov	$0370, r2		/ 0177000..0177737 — 240 слов
2:	clr	(r0)+
	sob	r2, 2b
	tst	(r1)+
	cmp	r1, $040
	blo	1b
	mov	$6, @$0177130
	mov	_cur_page, @$0177130
	mov	$0, @$0177130
	mtps	(sp)+
	mov	(sp)+, r2
	rts	pc

	.data
_vctl:		.word	0		/ палитра 0, страница 5, прерывание разрешено
_vctl_req:	.word	0
_mem11:		.word	016000		/ окно 0 — страница 5, окно 1 — 4 (ядро ANDOS)
_jiffies:	.word	0
_frames:	.word	0
jacc:		.word	0
