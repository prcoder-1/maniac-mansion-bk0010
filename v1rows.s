/ Строки клетки фона v1 (банк C, gfx_v1.c render_group): 8 строк группы из 5 полос
/ по таблицам gp_e (индексы цветов точек байта символа), gp_t (байт БК в общих
/ цветах комнаты), gp_m3 (точки цвета клетки). Си-образец — в gfx_v1.c (HOST).
	.globl _v1_rows, _gp_e, _gp_t, _gp_m3
	.text

/ байт строки из одной полосы: x = gp_e[E][rows[K][r]]
.macro	ONE	k, e, i
	movb	\k*8(r1), r0
	bic	$0177400, r0
	movb	_gp_e+\e*0400(r0), r5
	bic	$0177400, r5
	movb	_gp_m3(r5), r0
	bicb	\i(r3), r0		/ & цвет клетки (r3 — дополнение)
	add	r2, r5
	bisb	(r5), r0
	movb	r0, (r4)+
.endm
/ из двух полос: x = gp_e[E1][rows[K1][r]] | gp_e[E2][rows[K2][r]]
.macro	TWO	k1, e1, k2, e2, i
	movb	\k1*8(r1), r0
	bic	$0177400, r0
	movb	_gp_e+\e1*0400(r0), r5
	movb	\k2*8(r1), r0
	bic	$0177400, r0
	bisb	_gp_e+\e2*0400(r0), r5
	bic	$0177400, r5
	movb	_gp_m3(r5), r0
	bicb	\i(r3), r0
	add	r2, r5
	bisb	(r5), r0
	movb	r0, (r4)+
.endm

/ void v1_rows(uint8_t lines[8][8], const uint8_t rows[5][8], const uint8_t ncb[2][8], uint16_t par)
/ ncb — дополнение байтов цвета клетки по чётности строки; par — чётность строки 0
_v1_rows:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r4		/ lines
	mov	014(sp), r1		/ rows (+ r)
	mov	020(sp), r0		/ par
	bic	$0177776, r0
	mov	r0, -(sp)		/ 2(sp) — чётность строки
	mov	$8, -(sp)		/ (sp) — строк осталось
1:	mov	2(sp), r0		/ чётность -> таблица и цвет клетки
	mov	r0, r2
	swab	r2			/ * 256
	add	$_gp_t, r2
	mov	022(sp), r3		/ ncb
	asl	r0
	asl	r0
	asl	r0
	add	r0, r3
	ONE	0, 0, 0
	TWO	0, 1, 1, 2, 1
	ONE	1, 3, 2
	TWO	1, 4, 2, 5, 3
	ONE	2, 6, 4
	ONE	3, 7, 5
	TWO	3, 8, 4, 9, 6
	ONE	4, 10, 7
	inc	r1
	mov	2(sp), r0
	inc	r0
	bic	$0177776, r0
	mov	r0, 2(sp)
	dec	(sp)
	beq	2f
	jmp	1b			/ тело цикла длиннее BR
2:	cmp	(sp)+, (sp)+
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
