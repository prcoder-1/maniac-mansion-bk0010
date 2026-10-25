/ Вывод готового спрайта актёра v1 (общая копия кода; costume_r.c spr_draw): строки
/ спрайта в PG_SPR — пары байтов {точки спрайта (маска), их цвет}, на байт цели —
/ две команды. Цель — видеопамять (видна при любой странице) или холст в PG_MASK:
/ тогда спрайт идёт через базовое ОЗУ (_band) одним куском. Строка — цепочкой пар
/ (prow), вход вычисляется раз на спрайт; срок звука — без вызова, на строке.
	.globl _spr_blit, _spr_blitb, pcode, _cur_page, _map_now, _snd_tick, _band
	.globl _sndst, _snd_tk
	.text

	PG_SPR = 017

/ snd_tick, только если подошёл срок DL; портит r0
.macro	SNDCHK
	mov	@$0177710, r0
	sub	@$_sndst+022, r0
	dec	r0
	cmp	r0, @$_snd_tk
	blo	.Ls\@
	jsr	pc, _snd_tick
.Ls\@:
.endm

/ включить страницу r0; портит r0
map:	asl	r0
	mov	pcode(r0), r0
	bis	$0120, r0
	mov	$6, @$0177130
	mov	r0, @$0177130
	mov	$0, @$0177130
	mov	r0, @$_map_now
	rts	pc

/ void spr_blit(uint16_t src, uint16_t w, uint16_t h, uint16_t dst, uint16_t dstride, uint16_t dpage)
/ src — адрес в PG_SPR, w — байт цели в строке (до 16), dpage 0 — видеопамять
_spr_blit:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r5		/ src
	mov	020(sp), r3		/ dst
	mov	016(sp), r4		/ строк
	mov	$PG_SPR, r0
	jsr	pc, map
	tst	024(sp)
	beq	3f
	mov	014(sp), r0		/ холст: весь спрайт — в _band (w * h слов), потом холст
	clr	r1
1:	add	r0, r1
	sob	r4, 1b
	mov	016(sp), r4
	mov	$_band, r2
	mov	r1, r0
	bic	$0177770, r0		/ слов % 8
	beq	2f
21:	mov	(r5)+, (r2)+
	sob	r0, 21b
2:	asr	r1
	asr	r1
	asr	r1			/ по 8 слов
	beq	22f
23:	mov	(r5)+, (r2)+
	mov	(r5)+, (r2)+
	mov	(r5)+, (r2)+
	mov	(r5)+, (r2)+
	mov	(r5)+, (r2)+
	mov	(r5)+, (r2)+
	mov	(r5)+, (r2)+
	mov	(r5)+, (r2)+
	sob	r1, 23b
22:	SNDCHK
	mov	024(sp), r0
	jsr	pc, map
	mov	$_band, r5
3:	mov	014(sp), r0
	jsr	pc, sprent
	mov	r0, r1			/ вход в цепочку строки
4:	mov	r3, r2
	jsr	pc, (r1)
	add	022(sp), r3
	SNDCHK
	sob	r4, 4b
9:	mov	$6, @$0177130		/ страница вызывающего
	mov	_cur_page, @$0177130
	mov	$0, @$0177130
	mov	_cur_page, @$_map_now
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ void spr_blitb(uint16_t w, uint16_t h, uint16_t dst, uint16_t dstride, uint16_t dpage,
/ uint16_t skip, uint16_t tail): то же из _band (спрайт за маской глубины или
/ обрезанный краем цели, limb.c): w байт цели в строке; пар в строке _band —
/ skip + w + tail (skip, tail — в байтах _band, по 2 на байт цели)
_spr_blitb:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	016(sp), r3		/ dst
	mov	014(sp), r4		/ строк
	mov	022(sp), r0
	beq	1f
	jsr	pc, map
1:	mov	012(sp), r0
	jsr	pc, sprent
	mov	r0, r1
	mov	$_band, r5
2:	add	024(sp), r5
	mov	r3, r2
	jsr	pc, (r1)
	add	026(sp), r5
	add	020(sp), r3
	SNDCHK
	sob	r4, 2b
	br	9b

/ r0 = w (1..16) -> r0 = вход в цепочку строки (4 байта кода на пару)
sprent:	asl	r0
	asl	r0
	neg	r0
	add	$prow, r0
	rts	pc

/ строка: пары {маска, цвет} из (r5)+ в байты (r2)+
	.rept	16
	bicb	(r5)+, (r2)
	bisb	(r5)+, (r2)+
	.endr
prow:	rts	pc
