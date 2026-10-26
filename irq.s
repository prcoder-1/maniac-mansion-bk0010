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
	.globl _snd_on, _snd_cur, _snd_next, _snd_mtimer, _ay_dirty, _snd_pcjr
	.globl _snd_call, _w1_copy, _ay_mute, _ld_file, _ld_file0
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
3:	tst	_snd_on			/ звуковой драйвер: страница 1 в окне 040000
	beq	4f
	mov	r0, -(sp)
	mov	r1, -(sp)
	mov	_mem11, r0
	bic	$070000, r0		/ окно 040000 — страница 1 (код 0)
	mov	r0, @$0177716
	jsr	pc, @$040000
	mov	_mem11, @$0177716
	mov	(sp)+, r1
	mov	(sp)+, r0
4:	inc	_frames
	rti

/ uint16_t snd_call(uint16_t entry, uint16_t a, uint16_t b, uint16_t c): вход драйвера
/ (snddrv.h DRV_*) с закрытыми прерываниями, страница 1 — в окне 040000
_snd_call:
	mfps	-(sp)
	mtps	$0340
	mov	_mem11, r0
	bic	$070000, r0
	mov	r0, @$0177716
	mov	012(sp), -(sp)		/ c
	mov	012(sp), -(sp)		/ b
	mov	012(sp), -(sp)		/ a
	mov	012(sp), r0		/ вход
	asl	r0
	asl	r0
	jsr	pc, 040000(r0)
	add	$6, sp
	mov	_mem11, @$0177716
	mtps	(sp)+
	rts	pc

/ void w1_copy(uint16_t page11, uint16_t smk_page, uint16_t n): n байт из страницы СМК
/ (0100000) в страницу ОЗУ 11М (040000)
_w1_copy:
	mov	r2, -(sp)
	mfps	-(sp)
	mtps	$0340
	mov	6(sp), r0
	movb	pg11(r0), r0
	swab	r0
	asl	r0
	asl	r0
	asl	r0
	asl	r0
	mov	_mem11, r1
	bic	$070000, r1
	bis	r0, r1
	mov	r1, @$0177716
	mov	010(sp), r0
	asl	r0
	mov	_page_code(r0), r0
	mov	$6, @$0177130
	mov	r0, @$0177130
	mov	$0, @$0177130
	mov	$0100000, r0
	mov	$040000, r1
	mov	012(sp), r2
	inc	r2
	asr	r2
1:	mov	(r0)+, (r1)+
	sob	r2, 1b
	mov	$6, @$0177130
	mov	_cur_page, @$0177130
	mov	$0, @$0177130
	mov	_mem11, @$0177716
	mtps	(sp)+
	mov	(sp)+, r2
	rts	pc

/ void ay_mute(void): громкость каналов AY — 0 (на время диска: кадровое прерывание
/ выключено, драйвер не пишет); после — драйвер перепишет все регистры
_ay_mute:
	mfps	-(sp)
	mtps	$0340
	mov	$0177767, r0		/ R8 в инверсии
1:	clr	@$0177714		/ чип 1 TurboSound
	mov	r0, @$0177714
	movb	$-1, @$0177714		/ 0 в инверсии
	dec	r0
	cmp	r0, $0177765
	bhis	1b
	mov	pc, _ay_dirty
	mtps	(sp)+
	rts	pc

/ int16_t ld_file(const struct ld_req *q): чтение/запись файла (loader.s) без звука
_ld_file:
	jsr	pc, _ay_mute
	mov	2(sp), -(sp)
	jsr	pc, _ld_file0
	tst	(sp)+
	rts	pc

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
_snd_on:	.word	0		/ драйвер загружен (main.c)
_snd_cur:	.word	0		/ почтовый ящик драйвера: звучащий и следующий звуки,
_snd_next:	.word	0		/ таймер музыки (VAR_MUSIC_TIMER)
_snd_mtimer:	.word	0
_ay_dirty:	.word	1		/ переписать все регистры AY
_snd_pcjr:	.word	1		/ 1 — PCjr, 0 — спикер
pg11:		.byte	6, 0, 2, 3, 4, 1, 7, 5	/ код страницы ОЗУ 11М в 0177716
	.even
_frames:	.word	0
jacc:		.word	0
