/ Деление 16 бит (libgcc: __udivhi3, __umodhi3, __divhi3, __modhi3; udiv/div —
/ прежний интерфейс). Сдвиг-вычитание в регистрах: 16 витков по 6-7 команд
/ (прежняя версия — Си без оптимизации, ~15000 тактов на деление).
	.text
	.globl _udiv, _div, ___udivhi3, ___umodhi3, ___divhi3, ___modhi3

/ r0 / r2 без знака -> r0 частное, r1 остаток; портит r3
udm:	clr	r1
	mov	$020, r3
1:	asl	r0
	rol	r1
	cmp	r1, r2
	blo	2f
	sub	r2, r1
	inc	r0
2:	sob	r3, 1b
	rts	pc

/ uint16_t udiv(uint16_t a, uint16_t b, int mod): mod — остаток, иначе частное
_udiv:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	6(sp), r0
	mov	010(sp), r2
	jsr	pc, udm
	tst	012(sp)
	beq	9f
	mov	r1, r0
9:	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

___udivhi3:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	6(sp), r0
	mov	010(sp), r2
	jsr	pc, udm
	br	9b

___umodhi3:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	6(sp), r0
	mov	010(sp), r2
	jsr	pc, udm
	mov	r1, r0
	br	9b

/ int div(int a, int b, int mod): как в Си — частное к нулю, остаток со знаком a
_div:
	tst	6(sp)
	bne	___modhi3
___divhi3:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	clr	r4			/ знак частного
	mov	010(sp), r0
	bpl	1f
	neg	r0
	inc	r4
1:	mov	012(sp), r2
	bpl	2f
	neg	r2
	inc	r4
2:	jsr	pc, udm
	ror	r4
	bcc	8f
	neg	r0
8:	mov	(sp)+, r4
	br	9b

___modhi3:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	clr	r4			/ знак остатка — знак делимого
	mov	010(sp), r0
	bpl	1f
	neg	r0
	inc	r4
1:	mov	012(sp), r2
	bpl	2f
	neg	r2
2:	jsr	pc, udm
	mov	r1, r0
	tst	r4
	beq	8b
	neg	r0
	br	8b
