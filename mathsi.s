/ 32-битное деление для gcc (общая копия кода): ___udivsi3, ___umodsi3,
/ ___divsi3, ___modsi3 вместо libgcc — её __divsi3 (gcc 14.2, pdp11) неверен:
/ 1310720 / -12 даёт -8191 вместо -109226 (calcMovementFactor: актёр шёл влево
/ спиной). Аргументы на стеке, у числа старшее слово первым; ответ — r0 (старшее),
/ r1 (младшее); r2..r5 сохраняются.
	.globl ___udivsi3, ___umodsi3, ___divsi3, ___modsi3
	.text

/ r0:r1 / r2:r3 -> частное r0:r1, остаток r4:r5 (без знака, сдвиг с вычитанием)
udm:	clr	r4
	clr	r5
	mov	$040, -(sp)
1:	asl	r1
	rol	r0
	rol	r5
	rol	r4
	cmp	r4, r2
	bhi	2f
	blo	3f
	cmp	r5, r3
	blo	3f
2:	sub	r3, r5
	sbc	r4
	sub	r2, r4
	inc	r1
3:	dec	(sp)
	bne	1b
	tst	(sp)+
	rts	pc

/ загрузить аргументы (после сохранения r2..r5 и адреса возврата в обёртке:
/ делимое — 014(sp), делитель — 020(sp) с точки зрения ld)
ld:	mov	014(sp), r0
	mov	016(sp), r1
	mov	020(sp), r2
	mov	022(sp), r3
	rts	pc

save:	mov	(sp)+, r0		/ адрес возврата из save
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	jmp	(r0)

___udivsi3:
	jsr	pc, save
	jsr	pc, ld
	jsr	pc, udm
	br	out

___umodsi3:
	jsr	pc, save
	jsr	pc, ld
	jsr	pc, udm
	mov	r4, r0
	mov	r5, r1
	br	out

/ знак: модули, затем частное — со знаком «различны ли знаки», остаток — со
/ знаком делимого (как в C)
___divsi3:
	jsr	pc, save
	jsr	pc, ld
	clr	-(sp)
	jsr	pc, abs
	jsr	pc, udm
	bit	$1, (sp)+		/ знаки различны
	beq	out
	neg	r0
	neg	r1
	sbc	r0
	br	out

___modsi3:
	jsr	pc, save
	jsr	pc, ld
	clr	-(sp)
	jsr	pc, abs
	jsr	pc, udm
	mov	r4, r0
	mov	r5, r1
	bit	$2, (sp)+		/ знак делимого
	beq	out
	neg	r0
	neg	r1
	sbc	r0
	br	out

/ r0:r1 и r2:r3 — по модулю; 2(sp) (флаги вызывающего): бит 0 — знаки различны,
/ бит 1 — делимое отрицательно
abs:	tst	r0
	bpl	1f
	neg	r0
	neg	r1
	sbc	r0
	bis	$3, 2(sp)
1:	tst	r2
	bpl	2f
	neg	r2
	neg	r3
	sbc	r2
	bit	$1, 2(sp)		/ бит 0 ^= 1
	beq	3f
	bic	$1, 2(sp)
	br	2f
3:	bis	$1, 2(sp)
2:	rts	pc

out:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
