/ Перекодировка кадра костюма v1 (общая копия кода; зовёт банк B, costume_r.c procC64) для кадра, видимого
/ целиком: RLE C64 (байт длины, бит 7 — повтор следующего байта) по колонкам в 8
/ точек -> RLE proc3 по колонкам в 1 точку (цвет << 4 | длина; длина > 15 — байт
/ цвета и байт длины). Толстая точка f (0..3, биты 6-2f) даёт две одинаковые
/ колонки; порядок f — 0..3 при любом зеркале. Си-образец — procC64.
	.globl _c64_tr, _snd_tick
	.text
	HMAX = 144			/ шаг строк col[4][HMAX]

/ uint16_t c64_tr(uint8_t *out, const uint8_t *src, uint16_t h, uint16_t ncol,
/                 const uint8_t *src_end, const uint8_t *out_end, uint8_t col[4][HMAX])
/ -> длина вывода; 0 — не влезло или исходник кончился
_c64_tr:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r4		/ out
	mov	014(sp), r1		/ src
	mov	016(sp), h
	mov	020(sp), nc
	mov	022(sp), send
	mov	024(sp), oend
	mov	026(sp), colp		/ буфер колонки — в банке вызывающего
	clr	r2			/ остаток серии
	clr	rep
	clr	r3			/ байт серии
	br	col
dfail:	jmp	fail			/ BR до fail не достаёт
col:	mov	r4, outp		/ колонка: распаковать h байт в col
	mov	colp, r4
	mov	h, r5
dy:	tst	r2
	bne	2f
1:	cmp	r1, send
	bhis	dfail
	clr	r2
	bisb	(r1)+, r2
	clr	rep
	bit	$0200, r2
	beq	3f
	inc	rep
	cmp	r1, send
	bhis	dfail
	movb	(r1)+, r3
3:	bic	$0177600, r2
	beq	1b
2:	tst	rep
	bne	4f
	cmp	r1, send
	bhis	dfail
	movb	(r1)+, r3
4:	dec	r2
	mov	r3, r0
	bic	$0177774, r0
	movb	r0, 3*HMAX(r4)
	mov	r3, r0
	asr	r0
	asr	r0
	bic	$0177774, r0
	movb	r0, 2*HMAX(r4)
	mov	r3, r0
	asr	r0
	asr	r0
	asr	r0
	asr	r0
	bic	$0177774, r0
	movb	r0, HMAX(r4)
	mov	r3, r0
	asr	r0
	asr	r0
	asr	r0
	asr	r0
	asr	r0
	asr	r0
	bic	$0177774, r0
	movb	r0, (r4)+
	sob	r5, dy
	jsr	pc, _snd_tick		/ фронт тона: колонка распакована
	mov	r1, srcp
	mov	r2, len
	mov	r3, colr
	mov	outp, r4
	mov	colp, r0		/ толстые точки f = 0..3
	mov	$4, fcnt
fat:	mov	r4, start
	mov	h, r5
e1:	movb	(r0)+, r1		/ серия: значение r1, длина r3
	mov	$1, r3
	dec	r5
e2:	tst	r5
	beq	emit
	cmpb	(r0), r1
	bne	emit
	inc	r0
	inc	r3
	dec	r5
	br	e2
emit:	cmp	r4, oend
	bhis	fail
	asl	r1
	asl	r1
	asl	r1
	asl	r1
	cmp	r3, $16
	bhis	5f
	bis	r3, r1
	movb	r1, (r4)+
	br	6f
5:	movb	r1, (r4)+
	movb	r3, (r4)+
6:	tst	r5
	bne	e1
	mov	r4, r2			/ вторая колонка толстой точки — копия
	sub	start, r2
	mov	r4, r3
	add	r2, r3
	cmp	r3, oend
	bhis	fail
	mov	start, r3
7:	movb	(r3)+, (r4)+
	sob	r2, 7b
	jsr	pc, _snd_tick
	add	$HMAX, r0		/ следующая f: начало её строки
	sub	h, r0
	dec	fcnt
	bne	fat
	mov	srcp, r1
	mov	len, r2
	mov	colr, r3
	dec	nc
	beq	done
	jmp	col
done:	mov	r4, r0
	sub	012(sp), r0
	br	9f
fail:	clr	r0
9:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

	.data
h:	.word	0
nc:	.word	0
send:	.word	0
oend:	.word	0
rep:	.word	0
outp:	.word	0
srcp:	.word	0
len:	.word	0
colr:	.word	0
start:	.word	0
fcnt:	.word	0
colp:	.word	0
	.text
