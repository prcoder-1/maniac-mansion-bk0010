/ Вывод готового спрайта актёра v1 (общая копия кода; costume_r.c spr_draw): строки
/ спрайта в PG_SPR — пары байтов {точки спрайта (маска), их цвет}, на байт цели —
/ две команды. Цель — видеопамять (видна при любой странице) или холст в PG_MASK:
/ тогда строка идёт через базовое ОЗУ (V1LB).
	.globl _spr_blit, pcode, _cur_page, _map_now, _snd_tick
	.text

	PG_SPR = 017
	LB = 025200			/ строка спрайта по пути в холст (v1cell.s: строки фона)

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
/ src — адрес в PG_SPR, w — байт цели в строке, dpage 0 — видеопамять
_spr_blit:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r1		/ src
	mov	020(sp), r3		/ dst
	tst	024(sp)
	bne	5f
	mov	$PG_SPR, r0
	jsr	pc, map
1:	mov	r3, r2			/ видеопамять
	mov	014(sp), r4
2:	bicb	(r1)+, (r2)
	bisb	(r1)+, (r2)+
	sob	r4, 2b
	add	022(sp), r3
	jsr	pc, _snd_tick
	dec	016(sp)
	bne	1b
	br	9f
5:	mov	$PG_SPR, r0		/ холст: строка через базовое ОЗУ
	jsr	pc, map
	mov	$LB, r2
	mov	014(sp), r4
	asl	r4
6:	movb	(r1)+, (r2)+
	sob	r4, 6b
	mov	024(sp), r0
	jsr	pc, map
	mov	$LB, r5
	mov	r3, r2
	mov	014(sp), r4
7:	bicb	(r5)+, (r2)
	bisb	(r5)+, (r2)+
	sob	r4, 7b
	add	022(sp), r3
	jsr	pc, _snd_tick
	dec	016(sp)
	bne	5b
9:	mov	$6, @$0177130		/ страница вызывающего
	mov	_cur_page, @$0177130
	mov	$0, @$0177130
	mov	_cur_page, @$_map_now
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
