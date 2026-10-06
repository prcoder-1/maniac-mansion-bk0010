/ Вывод фона комнаты в экран (общая копия кода). PG_BITMAP — страница 4.

PG_BITMAP = 4
	.globl _bm_blit, _cur_page
	.text

/ void bm_blit(uint16_t src, uint16_t stride, uint16_t vram, uint16_t ncols, uint16_t rows):
/ прямоугольник ncols байт x rows строк из PG_BITMAP (src, строки по stride) в
/ видеоОЗУ (vram, строки по 64 байта)
_bm_blit:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	$6, @$0177130
	mov	$0130, @$0177130	/ ОЗУ10 (0120) + страница 4 (код 010)
	mov	$0, @$0177130
	mov	012(sp), r1		/ src
	mov	016(sp), r2		/ vram
	mov	022(sp), r5		/ строк
	mov	014(sp), r4		/ stride
	sub	020(sp), r4		/ stride - ncols
	mov	$0100, r3
	sub	020(sp), r3		/ 64 - ncols
	mov	r3, -(sp)
1:	mov	022(sp), r0		/ ncols (+2: на стеке 64-ncols)
	mov	r1, r3
	bis	r2, r3
	bit	$1, r3
	bne	3f
	asr	r0			/ оба адреса чётные — словами
	beq	4f
2:	mov	(r1)+, (r2)+
	sob	r0, 2b
4:	bit	$1, 022(sp)
	beq	5f
	movb	(r1)+, (r2)+
	br	5f
3:	movb	(r1)+, (r2)+
	sob	r0, 3b
5:	add	r4, r1
	add	(sp), r2
	sob	r5, 1b
	tst	(sp)+
	mov	$6, @$0177130
	mov	_cur_page, @$0177130
	mov	$0, @$0177130
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
