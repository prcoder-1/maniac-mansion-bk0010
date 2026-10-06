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
	mov	r1, r3			/ чётность: оба адреса чётные — словами
	bis	r2, r3
	mov	022(sp), r0		/ ncols (+2: на стеке 64-ncols)
	bit	$1, r3
	bne	3f
	/ словами: вход в развёрнутую цепочку за ncols/2 команд до конца
	asr	r0
	bcs	2f
	asl	r0
	mov	$wend, r3
	sub	r0, r3
1:	jmp	(r3)
	.rept	32
	mov	(r1)+, (r2)+
	.endr
wend:	add	r4, r1
	add	(sp), r2
	sob	r5, 1b
	br	9f
2:	asl	r0			/ нечётное ncols: последний байт отдельно
	mov	$wend2, r3
	sub	r0, r3
5:	jmp	(r3)
	.rept	32
	mov	(r1)+, (r2)+
	.endr
wend2:	movb	(r1)+, (r2)+
	add	r4, r1
	add	(sp), r2
	sob	r5, 5b
	br	9f
	/ байтами: цепочка из ncols команд
3:	asl	r0
	mov	$bend, r3
	sub	r0, r3
4:	jmp	(r3)
	.rept	64
	movb	(r1)+, (r2)+
	.endr
bend:	add	r4, r1
	add	(sp), r2
	dec	r5			/ sob не дотянется через 64 команды
	bne	4b
9:
	tst	(sp)+
	mov	$6, @$0177130
	mov	_cur_page, @$0177130
	mov	$0, @$0177130
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
