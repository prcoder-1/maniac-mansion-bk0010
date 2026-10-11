/ Свип частоты, тон и шум Player_V1 (банк D, sound.c): такты плеера и такты
/ кольца snd.s подряд, без Си на каждом такте — Си-образец в sound.c (HOST).
/ Свип и тон — в кольцо полупериод с битом 1 (фронты без ожидания: высота и так
/ меняется). Шум: такт плеера (st & случайное) | 0x180 — средний, на такт кольца
/ ((st & ~0x180) / 2 + 0x180); в кольцо — граница st | 1 (фронты берёт snd.s).
	.globl _v1_fill, _snd_ring
	.text

/ uint16_t v1_fill(struct v1st *p, uint16_t rw, uint16_t n): p = {a, m2, st, e, d,
/ lsr, freq, rep, type}; n тактов кольца (по 5040 пар отсчётов PIT) в _snd_ring с
/ rw; n == 0 — только догнать такты плеера (a < m2). -> записано тактов; | 0400 —
/ кусок кончился (такт, на котором он кончился, не записан: время его — в a)
_v1_fill:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	clr	-(sp)			/ (sp) — записано; 014(sp) — p, 016 — rw, 020 — n
	mov	014(sp), r5
	mov	(r5), r0		/ a
	mov	2(r5), r1		/ m2
	mov	4(r5), r2		/ st
	mov	012(r5), r3		/ lsr
	tst	020(sp)
	beq	2f
1:	add	$5040, r0		/ такт кольца
2:	cmp	020(r5), $3		/ шум: средний такт плеера
	bne	3f
	mov	r2, r1
	bic	$0600, r1
	clc
	ror	r1
	clc
	ror	r1
	add	$0300, r1
3:	cmp	r0, r1			/ такты плеера, пока хватает времени
	blo	20f
	sub	r1, r0
	cmp	020(r5), $2
	beq	12f
	bhi	13f
	add	010(r5), 014(r5)	/ 1: свип частоты
	cmp	014(r5), 6(r5)
	bne	3b
	dec	016(r5)
	beq	30f
	mov	r2, 014(r5)
	br	3b
12:	add	010(r5), r2		/ 2: тон, такт плеера = st (set_mplex)
	cmp	r2, 6(r5)
	beq	30f
	mov	r2, r1
	clc
	ror	r1
	bne	3b
	mov	$1, r1			/ st == 1 -> 1, st == 0 -> 65536 / 2
	tst	r2
	bne	3b
	mov	$0100000, r1
	br	3b
13:	add	010(r5), r2		/ 3: шум (такт — средний, тот же до конца такта кольца)
	cmp	r2, 6(r5)
	bne	3b
	br	30f
20:	tst	020(sp)			/ n == 0 — только догнать
	beq	40f
	cmp	020(r5), $2		/ значение такта кольца
	beq	22f
	bhi	23f
	mov	014(r5), r4		/ 1: делитель — частота
	br	24f
22:	mov	r1, r4			/ 2: делитель — два такта плеера
	cmp	r4, $040000
	bhis	25f
	asl	r4
	asl	r4
	br	24f
23:	mov	r2, r4			/ 3: граница шума, бит 0 — шум
	bis	$1, r4
	br	27f
24:	cmp	r4, $26000		/ делитель -> полупериод 8.8: d * 2.5156 (бит 0 — 0)
	bhi	25f
	tst	r4			/ 0 — тишина
	beq	27f
	mov	r4, -(sp)
	asl	r4
	asr	(sp)
	add	(sp), r4
	asr	(sp)
	asr	(sp)
	asr	(sp)
	asr	(sp)
	asr	(sp)
	add	(sp)+, r4
	bic	$1, r4
	br	26f
25:	mov	$0177776, r4
26:	bis	$2, r4			/ свип, тон: без ожидания фронта
27:	mov	016(sp), -(sp)		/ snd_ring[rw & 63]
	bic	$0177700, (sp)
	asl	(sp)
	add	$_snd_ring, (sp)
	mov	r4, @(sp)+
	inc	016(sp)
	inc	(sp)
	dec	020(sp)
	beq	40f
	jmp	1b
30:	bis	$0400, (sp)		/ кусок кончился
40:	mov	r0, (r5)
	mov	r1, 2(r5)
	mov	r2, 4(r5)
	mov	r3, 012(r5)
	mov	(sp)+, r0
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
