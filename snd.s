/ Тон на динамике (общая копия кода, без тактов ожидания ОЗУ СМК). Бит 6
/ регистра 0177716 переключается каждые hp витков sob, как tone_sq Alley Cat,
/ но остаток полупериода переносится между вызовами: меандр не рвётся, когда
/ тон играет короткими кусками между работой (низкие ноты длиннее куска).
	.globl _tone_sq, _umulhi, _fm_scale
	.text

	STEP = 256			/ витков между проверками таймера

/ void tone_sq(struct tone *t, uint16_t counts): t = { hp, rem, level, t_end };
/ играть counts отсчётов таймера (hp = 0 — тишина), t_end — таймер на выходе
_tone_sq:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	010(sp), r4
	mov	012(sp), r2
	mov	@$0177710, r3
	tst	(r4)
	bne	3f
1:	mov	r3, r0
	sub	@$0177710, r0
	cmp	r0, r2
	blo	1b
	br	9f
3:	mov	2(r4), r0		/ остаток полупериода
	bne	4f
2:	mov	4(r4), r1		/ фронт
	mov	r1, @$0177716
	neg	r1
	add	$0100, r1		/ 0 <-> 0100
	mov	r1, 4(r4)
	mov	(r4), r0
4:	mov	r0, r1
	cmp	r1, $STEP
	blos	5f
	mov	$STEP, r1
5:	sub	r1, r0
6:	sob	r1, 6b
	mov	r3, r1
	sub	@$0177710, r1
	cmp	r1, r2
	bhis	8f
	tst	r0
	beq	2b
	br	4b
8:	mov	r0, 2(r4)
9:	mov	@$0177710, 6(r4)
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ uint16_t umulhi(uint16_t a, uint16_t b): старшее слово произведения
_umulhi:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	010(sp), r2
	mov	012(sp), r3
	clr	r0
	clr	r1
	mov	$020, r4
1:	asl	r1
	rol	r0
	asl	r3
	bcc	2f
	add	r2, r1
	adc	r0
2:	sob	r4, 1b
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ int16_t fm_scale(int16_t t, uint16_t m): t * m / 256 с усечением к нулю,
/ |t| <= 128 (freqmod_table Player_V2) — 8 шагов сдвига вместо __mulhi3
_fm_scale:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	010(sp), r2
	mov	012(sp), r3
	clr	r0
	clr	r1
	clr	r4			/ знак
	tst	r2
	bpl	1f
	neg	r2
	inc	r4
1:	swab	r2			/ |t| — в старший байт
	mov	$8, -(sp)
2:	asl	r1			/ (r0:r1) *= 2
	rol	r0
	asl	r2
	bcc	3f
	add	r3, r1
	adc	r0
3:	dec	(sp)
	bne	2b
	tst	(sp)+
	swab	r1			/ (r0:r1) >> 8
	bic	$0177400, r1
	swab	r0
	clrb	r0
	bis	r1, r0
	tst	r4
	beq	4f
	neg	r0
4:	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
