/ Модуль AZБК: спрайты конечностей (az_act.c) — постройка из RLE и наложение на холст.
/ Плоскости спрайта (окно 13): цвет (тетрады, левая точка — младшая) и маска (017 на
/ непрозрачную точку), строки по ws слов подряд.

	.globl	_spr_dec, _spr_mix, _az_ex4
	.text

/ void spr_dec(struct sd *p): кадр RLE -> плоскости (обе сначала обнуляются)
/ struct sd { 0 src, 2 w (колонок), 4 h, 6 ws*2, 010 plane (байт плоскости), 012 dst (слово 0
/   строки 0 плоскости цвета), 014 sx колонки 0, 016 шаг sx (+-1), 020 tbl[4][16] —
/   цвет в тетраде sh (0 — прозрачный) }
/ RLE костюма v2: байт = цвет << 4 | длина, длина 0 — следующим байтом (0 — 256);
/ серии идут через границы колонок
_spr_dec:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r4
	mov	r4, sdp
	mov	012(r4), r3		/ обе плоскости — нулями
	mov	010(r4), r2
	beq	1f
2:	clr	(r3)+
	sob	r2, 2b			/ плоскость в байтах = обе в словах
1:	mov	010(r4), pl1+4
	mov	6(r4), ad1+2
	mov	6(r4), ad2+2
	mov	2(r4), ncol
	mov	014(r4), sx
	mov	(r4), r1
	clr	r0			/ осталось точек серии
	clr	r5			/ её цвет
col:	dec	ncol
	bmi	done
	mov	sdp, r4
	mov	sx, r2
	mov	r2, r3
	asr	r3
	asr	r3
	asl	r3
	add	012(r4), r3		/ колонка в плоскости цвета
	bic	$0177774, r2		/ тетрада
	asl	r2
	mov	nmt(r2), pl1+2
	asl	r2
	asl	r2
	asl	r2
	asl	r2			/ sh * 32
	add	$020, r2
	add	r4, r2
	mov	r2, tptr
	add	016(r4), sx
	mov	4(r4), r2		/ строк колонки
	mov	r5, r4			/ серия идёт дальше: её цвет в этой тетраде
	asl	r4
	add	tptr, r4
	mov	(r4), r4
	tst	r0
	bne	run
next:	movb	(r1)+, r5
	bic	$0177400, r5
	mov	r5, r0
	bic	$0177760, r0
	asr	r5
	asr	r5
	asr	r5
	asr	r5
	tst	r0
	bne	1f
	movb	(r1)+, r0
	bic	$0177400, r0
	bne	1f
	mov	$0400, r0
1:	mov	r5, r4
	asl	r4
	add	tptr, r4
	mov	(r4), r4
run:	tst	r5
	beq	tr
op:	bis	r4, (r3)
pl1:	bis	$0, 0(r3)		/ маска: 017 << 4sh; смещение плоскости маски
ad1:	add	$0, r3			/ ws*2
	dec	r2
	beq	ce
	sob	r0, op
	br	next
tr:
ad2:	add	$0, r3
	dec	r2
	beq	ce
	sob	r0, tr
	br	next
ce:	dec	r0			/ точка колонки взята
	br	col
done:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

nmt:	.word	017, 0360, 07400, 0170000
sdp:	.word	0
tptr:	.word	0
ncol:	.word	0
sx:	.word	0

/ void spr_mix(struct sp *p): спрайт -> холст: dst = dst & ~m | цвет & m, m — маска
/ спрайта без точек z-маски (PG_MASK, бит 7 — левая точка байта; 8 точек — 2 слова)
/ struct sp { 0 src (слово цвета), 2 plane (байт), 4 nw (слов в строке), 6 rows,
/   010 sskip, 012 dst, 014 dskip (байт до следующей строки), 016 mrow (байт маски
/   первого слова первой строки, 0 — без маски), 020 mstr (bm_strips), 022 half
/   (первое слово — правая половина байта маски) }
_spr_mix:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r4
	mov	2(r4), pa+2
	mov	2(r4), pb+2
	mov	(r4), r1
	mov	012(r4), r3
	mov	6(r4), rows
	mov	016(r4), r2		/ маска строки
prow:	dec	rows
	bmi	pdone
	mov	4(r4), r5
	tst	r2
	beq	fast
/ строка с маской: если её байты под спрайтом — нули, как без маски
	mov	r2, r0
	mov	r5, -(sp)
	add	022(r4), (sp)
	inc	(sp)
	asr	(sp)			/ байтов: (nw + half + 1) / 2
1:	tstb	(r0)+
	bne	slow0
	dec	(sp)
	bne	1b
	tst	(sp)+
	br	fast
slow0:	tst	(sp)+
	mov	r2, mp
	mov	022(r4), mh
slow:
pa:	mov	0(r1), r0		/ маска спрайта
	beq	4f
	movb	@mp, -(sp)		/ байт z-маски: тетрада слова
	tst	mh
	bne	2f
	asr	(sp)
	asr	(sp)
	asr	(sp)
	asr	(sp)
2:	bic	$0177760, (sp)
	asl	(sp)
	add	$_az_ex4, (sp)
	bic	@(sp)+, r0		/ без точек за объектами
	bic	r0, (r3)
	mov	(r1), -(sp)
	com	r0
	bic	r0, (sp)
	bis	(sp)+, (r3)
4:	tst	(r1)+
	tst	(r3)+
	tst	mh
	beq	5f
	inc	mp
5:	com	mh
	bic	$0177776, mh
	sob	r5, slow
	br	pnext
fast:
pb:	mov	0(r1), r0
	beq	4f
	bic	r0, (r3)
	bis	(r1), (r3)
4:	tst	(r1)+
	tst	(r3)+
	sob	r5, fast
pnext:	add	010(r4), r1
	add	014(r4), r3
	tst	r2
	beq	prow
	add	020(r4), r2
	br	prow
pdone:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
rows:	.word	0
mp:	.word	0
mh:	.word	0
