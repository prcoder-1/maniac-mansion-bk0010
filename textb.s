/ Строки знака текста (банк C; gfx.c charset_printChar): 8 строк экрана с row,
/ строка знака g[r] -> 3 байта точек s = t + 4 * g[r] (chr_t), байты экрана —
/ (экран & ~cm[k]) | (s[k] & fill). Знак целиком (6 точек); обрезанный краем —
/ на Си. Экран и стек — в медленном ОЗУ БК: байт экрана читается и пишется по
/ разу, маски клетки, таблица и конец — прямо в командах (код банка — в ОЗУ СМК)
	.globl	_chr_put
	.text

/ void chr_put(volatile uint8_t *row, const uint8_t *g, const uint8_t *t, const uint8_t *cm, uint16_t nfill)
_chr_put:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r1		/ строка экрана
	mov	014(sp), r2		/ строки знака
	mov	016(sp), pt+2		/ t: точки строк знака
	mov	020(sp), r0		/ маски клетки по байтам
	clr	r3
	bisb	(r0)+, r3
	mov	r3, pc0+2
	clr	r3
	bisb	(r0)+, r3
	mov	r3, pc1+2
	clr	r3
	bisb	(r0), r3
	mov	r3, pc2+2
	mov	r2, r0
	add	$010, r0
	mov	r0, pe+2		/ конец строк знака
	mov	022(sp), r4		/ ~fill
1:	movb	(r2)+, r0
	bic	$0177700, r0
	asl	r0
	asl	r0
pt:	add	$0, r0
	movb	(r1), r5
pc0:	bicb	$0, r5
	movb	(r0)+, r3
	bicb	r4, r3
	bisb	r3, r5
	movb	r5, (r1)+
	movb	(r1), r5
pc1:	bicb	$0, r5
	movb	(r0)+, r3
	bicb	r4, r3
	bisb	r3, r5
	movb	r5, (r1)+
	movb	(r1), r5
pc2:	bicb	$0, r5
	movb	(r0), r3
	bicb	r4, r3
	bisb	r3, r5
	movb	r5, (r1)
	add	$076, r1		/ следующая строка экрана
pe:	cmp	r2, $0
	bne	1b
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
