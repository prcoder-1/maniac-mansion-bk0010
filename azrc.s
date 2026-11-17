/ Модуль AZБК: перекраска точек слоя 1 (подсветка инвентаря и строки предложения) — по
/ словам, тетрады масками. На Си по точке (сдвиги на переменную — циклы у -m10) строка
/ предложения перекрашивалась ~0.5 с.

	.globl	_az_recolor, _win12, _win13, _win_back
	.text

/ uint16_t az_recolor(fn, x0, y0, x1, y1, from, to): прямоугольник экрана MM [x0, x1) x
/ [y0, y1), x0 и x1 кратны 4 (у подсветки MM — кратны 8): точки цвета from -> to; слова 0
/ (фон) — мимо: from 0 не бывает
_az_recolor:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	sp, r5
	add	$014, r5
	mov	(r5)+, r2		/ x0
	mov	(r5)+, r3		/ y0
	mov	(r5)+, r4		/ x1
	mov	(r5)+, r1		/ y1
	cmp	r4, $0500
	ble	1f
	mov	$0500, r4
1:	cmp	r1, $0310
	ble	1f
	mov	$0310, r1
1:	sub	r3, r1
	ble	rdone
	mov	r1, rows
	add	$034, r3			/ строка слоя
	mov	r3, line
	asr	r2
	asr	r2
	asr	r4
	asr	r4
	sub	r2, r4
	ble	rdone
	mov	r4, nw			/ слов в строке
	add	$030, r2			/ слово точки 96 + x0
	asl	r2
	add	$0140000, r2
	mov	r2, xoff
	mov	(r5)+, r0
	jsr	pc, rep4
	mov	r0, cf
	mov	(r5), r0
	jsr	pc, rep4
	mov	r0, ct
row:	mov	line, r2		/ окна 12-13 — блоки строк line / 8 и следующий
	asr	r2
	asr	r2
	asr	r2
	add	$01040, r2
	mov	r2, -(sp)
	jsr	pc, _win12
	inc	r2
	mov	r2, (sp)
	jsr	pc, _win13
	tst	(sp)+
	mov	line, r1
	bic	$0177770, r1
	swab	r1
	asl	r1			/ (line & 7) * 512 байт
	add	xoff, r1
	mov	nw, r5
wd:	mov	(r1), r0
	beq	nx
	mov	cf, r2
	xor	r0, r2			/ тетрады цвета from — нули
	clr	r3
	bit	$017, r2
	bne	1f
	bis	$017, r3
1:	bit	$0360, r2
	bne	1f
	bis	$0360, r3
1:	bit	$07400, r2
	bne	1f
	bis	$07400, r3
1:	bit	$0170000, r2
	bne	1f
	bis	$0170000, r3
1:	tst	r3
	beq	nx
	bic	r3, r0
	mov	ct, r2
	com	r3
	bic	r3, r2
	bis	r2, r0
	mov	r0, (r1)
nx:	tst	(r1)+
	sob	r5, wd
	inc	line
	dec	rows
	bne	row
	jsr	pc, _win_back
rdone:	clr	r0
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ r0: цвет -> он же во всех четырёх тетрадах
rep4:	bic	$0177760, r0
	mov	r0, r2
	asl	r2
	asl	r2
	asl	r2
	asl	r2
	bis	r2, r0
	mov	r0, r2
	swab	r2
	bis	r2, r0
	rts	pc

rows:	.word	0
line:	.word	0
nw:	.word	0
xoff:	.word	0
cf:	.word	0
ct:	.word	0
