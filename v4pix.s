/ Модуль AZБК: декодер графики SCUMM v2 в 4 бита на точку — в склад фона (az.c).
/ Разбор RLE — как v2_pix (gfxv2.s): колонки сверху вниз, серия — цвет (бит 7 = 0) или
/ «повтор» точек предыдущей колонки (бит 7 = 1, таблица _v2_dt); сжатия нет: цвета EGA
/ четырёх колонок копятся в _v2_col (4 x 128), после четвёртой — слово тетрад на строку
/ (через _az_pk: замена _roomPalette) в склад. Полоса 8 точек склада — 128 строк по 2
/ слова подряд (страница AZ_STORE + полоса/8, 512 байт на полосу), пишется через окно 12;
/ в окнах 8..13 при разборе — страница-источник СМК, код модуля — в 040000.

	.include "smk.inc"
STORE = 01140				/ azm.h AZ_STORE

	.globl _v4_pix, _v2_dt, _v2_col, _cur_page, _az_pk, pcode, rl_len, rl_col
	.text

/ uint16_t v4_pix(struct v2p *p) -> адрес за картинкой
/ struct v2p (gfx.h): 0 spage, 2 src, 4 w (точек, кратно 8), 6 h, 012 dest (полоса склада
/   первой колонки), 020 run0, 022 color0 (состояние RLE на входе), 030 ry0, 032 ry1
/   (строки картинки, которые пишутся), 034 doff (строка склада первой строки картинки)
/ Локальные (после r2..r5 и sub $010): 0 колонок осталось, 2 номер колонки, 4 код
/ страницы-источника, 6 её страница AZ в окне 12; p — 022(sp)
_v4_pix:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	sub	$010, sp
	mov	022(sp), r0
	mov	(r0), r1
	asl	r1
	mov	pcode(r1), 4(sp)
	mov	4(r0), (sp)
	clr	2(sp)
	mov	2(r0), r1		/ src
	mov	020(r0), r4		/ осталось точек серии: 0 — читать байт
	mov	022(r0), r5		/ цвет или -1 (повтор)
	mov	$_v2_dt, r2		/ dither_table обнулена
	mov	$0100, r3
1:	clr	(r2)+
	sob	r3, 1b
	SMKSET	4(sp)
	mov	@$0177330, 6(sp)
	mov	6(r0), r2		/ границы строк — в команды
	add	$_v2_dt, r2
	mov	r2, pend
	mov	030(r0), r2
	add	$_v2_dt, r2
	mov	r2, plim0
	mov	032(r0), r2
	add	$_v2_dt, r2
	mov	r2, sv_e1+2
	mov	r2, sv_e2+2

col:	tst	(sp)
	bne	1f
	jmp	done
1:	dec	(sp)
	mov	$_v2_dt, r2
	mov	2(sp), r0
	bic	$0177774, r0
	swab	r0			/ r3 = _v2_col + (колонка & 3) * 128
	asr	r0
	add	$_v2_col, r0
	mov	r0, r3
	jsr	pc, pskip0
seg:	tst	r4
	bne	2f
	movb	(r1)+, r0		/ байт RLE
	bmi	1f
	movb	rl_col(r0), r5		/ цвет — младшая тетрада
	movb	rl_len(r0), r4		/ длина — биты 4..6
	bne	2f
	br	3f
1:	mov	r0, r4			/ повтор колонки слева: длина — биты 0..6
	bic	$0177600, r4
	mov	$-1, r5
	tst	r4
	bne	2f
3:	clr	r4			/ длина 0 — следующим байтом
	bisb	(r1)+, r4
2:
sv_e1:	mov	$0, r0			/ n = min(серия, строк осталось)
	sub	r2, r0
	cmp	r4, r0
	bhis	4f
	mov	r4, r0
4:	sub	r0, r4
	tst	r5
	bmi	6f
5:	movb	r5, (r2)+		/ цвет: и в таблицу повторов, и в колонку
	movb	r5, (r3)+
	sob	r0, 5b
	br	7f
6:	bit	$1, r2			/ повтор колонки слева — словами
	beq	61f
	movb	(r2)+, (r3)+
	dec	r0
	beq	7f
61:	asr	r0
	beq	63f
62:	mov	(r2)+, (r3)+
	sob	r0, 62b
63:	bcc	7f
	movb	(r2)+, (r3)+
7:
sv_e2:	cmp	r2, $0
	blo	seg
	jsr	pc, pskipe
	mov	2(sp), r0		/ четвёртая колонка слова — слово в склад
	inc	2(sp)
	com	r0
	bit	$3, r0
	bne	col
	jsr	pc, flush
	br	col

/ строки колонки до первой записываемой (pskip0) и после записанных (pskipe): серии
/ без записи; r2 — строка в _v2_dt, r3 — в колонке _v2_col
pskip0:	mov	plim0, prl
	br	pskip
pskipe:	mov	pend, prl
pskip:	cmp	r2, (pc)+
prl:	.word	0
	bhis	9f
	tst	r4
	bne	2f
	movb	(r1)+, r0
	bmi	1f
	movb	rl_col(r0), r5
	movb	rl_len(r0), r4
	bne	2f
	br	3f
1:	mov	r0, r4
	bic	$0177600, r4
	mov	$-1, r5
	tst	r4
	bne	2f
3:	clr	r4
	bisb	(r1)+, r4
2:	mov	prl, r0
	sub	r2, r0
	cmp	r4, r0
	bhis	4f
	mov	r4, r0
4:	sub	r0, r4
	add	r0, r2
	add	r0, r3
	br	pskip
9:	rts	pc
plim0:	.word	0
pend:	.word	0

done:	SMKSET	_cur_page
	mov	r1, r0
	add	$010, sp
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ четыре колонки _v2_col -> слово тетрад строк [ry0, ry1) в склад. Кадр: r1 r4 r5 (6) +
/ возврат (2): локальные — со сдвигом 010 (колонок 010, номер колонки 012 — уже следующей,
/ код 014, окно 016), p — 032(sp)
flush:
	mov	r1, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	032(sp), r0
	mov	012(sp), r2
	dec	r2			/ последняя колонка слова
	mov	r2, r3
	asr	r3
	asr	r3
	asr	r3
	add	012(r0), r3		/ полоса склада
	mov	r3, r4
	asr	r4
	asr	r4
	asr	r4
	add	$STORE, r4
	mov	r4, @$0177330		/ окно 12 — страница полосы
	bic	$0177770, r3
	swab	r3
	asl	r3			/ * 512
	add	$0140000, r3
	bit	$4, r2			/ второе слово полосы
	beq	1f
	tst	(r3)+
1:	mov	034(r0), r4		/ строка склада: doff + ry0, по 4 байта
	add	030(r0), r4
	asl	r4
	asl	r4
	add	r4, r3
	mov	032(r0), r5
	sub	030(r0), r5
	beq	9f
	mov	$_v2_col, r1
	add	030(r0), r1
2:	movb	0400(r1), r0		/ колонка 2 — младшая тетрада старшего байта
	movb	_az_pk(r0), r2
	movb	0600(r1), r0		/ колонка 3
	bisb	_az_pk+020(r0), r2
	swab	r2
	movb	(r1)+, r0		/ колонка 0
	bisb	_az_pk(r0), r2
	movb	0177(r1), r0		/ колонка 1
	bisb	_az_pk+020(r0), r2
	mov	r2, (r3)
	add	$4, r3
	sob	r5, 2b
9:	mov	016(sp), @$0177330	/ окно 12 — снова источник
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r1
	rts	pc
