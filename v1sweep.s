/ Свип тона и шума Player_V1 (банк D, sound.c advance): такты плеера, пока хватает
/ времени — Си-образец в sound.c (HOST). Шум: случайное lsr = rotr3(lsr + 0x9248)
/ на каждом такте; на ВМ1 сдвиг на 13 бит из Си — десятки команд, здесь — шесть.
	.globl _v1_sweep
	.text

/ uint16_t v1_sweep(struct v1sw *p): p = {a, m2, st, e, d, lsr, noise};
/ -> 1, если свип кончился (st == e), иначе 0 (времени меньше такта)
_v1_sweep:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r5
	mov	(r5), r0		/ a
	mov	2(r5), r1		/ m2
	mov	4(r5), r2		/ st
	mov	012(r5), r3		/ lsr
	tst	014(r5)
	beq	5f
1:	cmp	r0, r1			/ шум
	blo	8f
	sub	r1, r0
	add	010(r5), r2
	cmp	r2, 6(r5)
	beq	7f
	add	$0111110, r3		/ lsr += 0x9248, поворот вправо на 3
	mov	r3, r4
	ror	r4
	ror	r3
	mov	r3, r4
	ror	r4
	ror	r3
	mov	r3, r4
	ror	r4
	ror	r3
	mov	r3, r1			/ m2 = ((st & lsr) | 0x180) >> 1
	com	r1
	mov	r2, r4
	bic	r1, r4
	bis	$0600, r4
	clc
	ror	r4
	mov	r4, r1
	br	1b
5:	cmp	r0, r1			/ тон: m2 = st >> 1 (set_mplex)
	blo	8f
	sub	r1, r0
	add	010(r5), r2
	cmp	r2, 6(r5)
	beq	7f
	mov	r2, r1
	clc
	ror	r1
	bne	5b
	mov	$1, r1			/ st == 1 -> 1, st == 0 -> 65536 / 2
	tst	r2
	bne	5b
	mov	$0100000, r1
	br	5b
7:	mov	$1, r4
	br	9f
8:	clr	r4
9:	mov	r0, (r5)
	mov	r1, 2(r5)
	mov	r2, 4(r5)
	mov	r3, 012(r5)
	mov	r4, r0
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
