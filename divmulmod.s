/ Деление 16 бит без знака (libgcc: __udivhi3, __umodhi3). Сдвиг-вычитание в
/ регистрах: 16 витков по 6-7 команд (прежняя версия — Си без оптимизации, ~15000
/ тактов на деление). Деления со знаком в игре нет — понадобится, libgcc даст своё.
	.text
	.globl ___udivhi3, ___umodhi3

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

___udivhi3:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	6(sp), r0
	mov	010(sp), r2
	jsr	pc, udm
9:	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

___umodhi3:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	6(sp), r0
	mov	010(sp), r2
	jsr	pc, udm
	mov	r1, r0
	br	9b
