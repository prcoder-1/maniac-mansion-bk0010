/ Вывод фона комнаты в экран (общая копия кода). PG_BITMAP — страница 4.

PG_BITMAP = 4
	.globl _bm_blit, _cur_page, _snd_tick
	.text

/ void bm_blit(uint16_t src, uint16_t stride, uint16_t vram, uint16_t ncols, uint16_t rows):
/ прямоугольник ncols байт x rows строк из PG_BITMAP (src, строки по stride) в
/ видеоОЗУ (vram, строки по 64 байта). Словами — где у строки оба адреса чётные:
/ при нечётном stride (ширина комнаты) чётность меняется от строки к строке, а
/ слово с нечётного адреса ВМ1 берёт с чётного — строка съехала бы на байт
_bm_blit:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	$6, @$0177130
	mov	$0130, @$0177130	/ ОЗУ10 (0120) + страница 4 (код 010)
	mov	$0, @$0177130
	mov	$0130, @$_map_now
	mov	012(sp), r1		/ src
	mov	016(sp), r2		/ vram
	mov	022(sp), r5		/ строк
	mov	014(sp), r4		/ stride
	sub	020(sp), r4		/ stride - ncols
	mov	$0100, r3
	sub	020(sp), r3		/ 64 - ncols
	mov	r3, -(sp)
	mov	022(sp), r0		/ ncols (на стеке +2)
	mov	r0, r3			/ вход в цепочку байтов: за ncols команд до конца
	asl	r3
	neg	r3
	add	$bend, r3
	mov	r3, -(sp)
	bic	$1, r0			/ вход в цепочку слов: за ncols/2 команд
	neg	r0
	add	$wend, r0
	mov	r0, -(sp)		/ (sp) — слова, 2(sp) — байты, 4(sp) — 64 - ncols
1:	mov	r1, r3			/ строка: оба адреса чётные — словами
	bis	r2, r3
	bit	$1, r3
	bne	2f
	jmp	@0(sp)
2:	jmp	@2(sp)
	.rept	32
	mov	(r1)+, (r2)+
	.endr
wend:	bit	$1, 026(sp)		/ нечётное ncols: последний байт
	beq	bend
	movb	(r1)+, (r2)+
	br	bend
	.rept	64
	movb	(r1)+, (r2)+
	.endr
bend:	add	r4, r1
	add	4(sp), r2
	mov	@$0177710, r0		/ фронт фонового тона — на каждой строке (snd_tick — если
	sub	@$023762, r0		/ подошёл срок DL)
	dec	r0
	cmp	r0, @$_snd_tk
	blo	3f
	jsr	pc, _snd_tick
3:
	dec	r5			/ sob не дотянется через цепочку
	bne	1b
	add	$6, sp
	mov	$6, @$0177130
	mov	_cur_page, @$0177130
	mov	$0, @$0177130
	mov	_cur_page, @$_map_now
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
