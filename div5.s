/ x/5 для x < 1280 (точка кольца + ширина экрана; сжатие 4:5: bx = x - x/5) — общая
/ копия кода. Вместо таблицы на 1280 байт: q = 51x / 256 — это x/5 или на 1 меньше.
/ Аргумент — в r0, не на стеке: вызов из Си (udiv5 в scumm.h) — вставкой ассемблера;
/ со стековым аргументом gcc при отложенном снятии путал смещения (cursor_show)
	.globl _udiv5
	.text

/ r0 = x -> r0 = x / 5, r1 = x % 5
_udiv5:
	mov	r0, r1
	asl	r0
	add	r1, r0			/ 3x
	mov	r0, -(sp)
	asl	r0
	asl	r0
	asl	r0
	asl	r0
	add	(sp)+, r0		/ 51x (x < 1280 — без переполнения)
	clrb	r0
	swab	r0			/ q
	sub	r0, r1
	sub	r0, r1
	sub	r0, r1
	sub	r0, r1
	sub	r0, r1			/ x - 5q
	cmp	r1, $5
	blo	1f
	inc	r0
	sub	$5, r1
1:	rts	pc
