/ Вывод готового спрайта актёра v1 (общая копия кода; costume_r.c spr_draw): строки
/ спрайта в PG_SPR — пары байтов {точки спрайта (маска), их цвет}, на байт цели —
/ две команды. Цель — видеопамять (видна при любой странице) или холст в PG_MASK:
/ тогда спрайт идёт через базовое ОЗУ (_band) одним куском. Строка — цепочкой пар
/ (prow), вход вычисляется раз на спрайт; срок звука — без вызова, на строке.
	.globl _spr_blit, _spr_blitb, pcode, _cur_page, _map_now, _snd_tick, _band
	.globl _sndst, _snd_tk
	.text

	PG_SPR = 017

/ snd_tick, только если подошёл срок DL; портит r0
.macro	SNDCHK
	mov	@$0177710, r0
	sub	@$_sndst+022, r0
	dec	r0
	cmp	r0, @$_snd_tk
	blo	.Ls\@
	jsr	pc, _snd_tick
.Ls\@:
.endm

/ включить страницу r0; портит r0
map:	asl	r0
	mov	pcode(r0), r0
	bis	$0120, r0
	mov	$6, @$0177130
	mov	r0, @$0177130
	mov	$0, @$0177130
	mov	r0, @$_map_now
	rts	pc

/ void spr_blit(uint16_t src, uint16_t w, uint16_t h, uint16_t dst, uint16_t dstride, uint16_t dpage)
/ src — адрес в PG_SPR, w — байт цели в строке (до 16), dpage 0 — видеопамять
_spr_blit:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r5		/ src
	mov	020(sp), r3		/ dst
	mov	016(sp), r4		/ строк
	mov	$PG_SPR, r0
	jsr	pc, map
	tst	024(sp)
	beq	3f
	mov	014(sp), r0		/ холст: весь спрайт — в _band (w * h слов), потом холст
	clr	r1
1:	add	r0, r1
	sob	r4, 1b
	mov	016(sp), r4
	mov	$_band, r2
	mov	r1, r0
	bic	$0177770, r0		/ слов % 8
	beq	2f
21:	mov	(r5)+, (r2)+
	sob	r0, 21b
2:	asr	r1
	asr	r1
	asr	r1			/ по 8 слов
	beq	22f
23:	mov	(r5)+, (r2)+
	mov	(r5)+, (r2)+
	mov	(r5)+, (r2)+
	mov	(r5)+, (r2)+
	mov	(r5)+, (r2)+
	mov	(r5)+, (r2)+
	mov	(r5)+, (r2)+
	mov	(r5)+, (r2)+
	sob	r1, 23b
22:	SNDCHK
	mov	024(sp), r0
	jsr	pc, map
	mov	$_band, r5
3:	mov	014(sp), r0
	jsr	pc, sprent
	mov	r0, r1			/ вход в цепочку строки
4:	mov	r3, r2
	jsr	pc, (r1)
	add	022(sp), r3
	SNDCHK
	sob	r4, 4b
spexit:	mov	$6, @$0177130		/ страница вызывающего
	mov	_cur_page, @$0177130
	mov	$0, @$0177130
	mov	_cur_page, @$_map_now
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ void spr_blitb(uint16_t w, uint16_t h, uint16_t dst, uint16_t dstride, uint16_t dpage,
/ uint16_t skip, uint16_t tail): то же из _band (спрайт за маской глубины или
/ обрезанный краем цели, limb.c): w байт цели в строке; пар в строке _band —
/ skip + w + tail (skip, tail — в байтах _band, по 2 на байт цели)
_spr_blitb:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	016(sp), r3		/ dst
	mov	014(sp), r4		/ строк
	mov	022(sp), r0
	beq	1f
	jsr	pc, map
1:	mov	012(sp), r0
	jsr	pc, sprent
	mov	r0, r1
	mov	$_band, r5
2:	add	024(sp), r5
	mov	r3, r2
	jsr	pc, (r1)
	add	026(sp), r5
	add	020(sp), r3
	SNDCHK
	sob	r4, 2b
	br	spexit

/ r0 = w (1..16) -> r0 = вход в цепочку строки (4 байта кода на пару)
sprent:	asl	r0
	asl	r0
	neg	r0
	add	$prow, r0
	rts	pc

/ строка: пары {маска, цвет} из (r5)+ в байты (r2)+
	.rept	16
	bicb	(r5)+, (r2)
	bisb	(r5)+, (r2)+
	.endr
prow:	rts	pc

/ Построение спрайта в кэш (limb.c spr_draw) — строками, без proc3. Кадр C64
/ (колонки по 8 точек мира, байт — 4 толстые точки строки, RLE по колонкам)
/ разбирается в _band строками (байт [r][c]); строка спрайта — порциями по 4
/ точки мира: порция по таблице палитры (фаза 4:5 и пара толстых точек) даёт
/ {маска, цвет} двух соседних байтов БК — их ИЛИ в строку пар. Порции строки —
/ в _bounce (limb.c): по чётности строки K пар слов {адрес строки таблицы
/ палитры, шаг указателя пары}. Таблицы — в PG_SPR (limb.c spr_tables).
	.globl	_spr_mk
	NIBAM = 0151000+0200		/ байт C64 -> 4 * первая порция (зеркало), со сдвигом 128
	NIBBM = 0151400+0200		/ -> вторая
	NIBAN = 0152000+0200		/ без зеркала
	NIBBN = 0152400+0200

/ void spr_mk(uint16_t page, uint16_t src, uint16_t ncol, uint16_t h, uint16_t mirror,
/ uint16_t dst, uint16_t w2, uint16_t k4, uint16_t par): кадр с src в странице page
/ -> пары в PG_SPR с dst (строки по w2 байт); k4 — байт порций одной чётности в
/ _bounce, par — чётность первой строки
/ аргументы: page 012, src 014, ncol 016, h 020, mirror 022, dst 024, w2 026,
/ k4 030, par 032 (после r2..r5 на стеке)
_spr_mk:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r0		/ страница кадра
	jsr	pc, map
	mov	014(sp), r1		/ RLE
	clr	r2			/ остаток серии
	clr	r3			/ байт серии
	mov	016(sp), -(sp)		/ колонок осталось
	mov	$_band, -(sp)		/ начало колонки в _band: строки — слева направо
	tst	026(sp)
	bne	0f
	add	022(sp), (sp)		/ без зеркала первая колонка кадра — справа
	dec	(sp)
0:	clr	-(sp)			/ байт длины (бит 7 — повтор)
	/ (sp) длина, 2 начало, 4 колонок; ncol 024, h 026, mirror 030
1:	mov	2(sp), r4
	mov	026(sp), r5
2:	tst	r2
	bne	4f
3:	clr	r2
	bisb	(r1)+, r2
	mov	r2, (sp)
	bic	$0177600, r2
	tstb	(sp)
	bpl	31f
	movb	(r1)+, r3		/ повтор: байт серии
31:	tst	r2
	beq	3b
4:	tstb	(sp)
	bmi	5f
	movb	(r1)+, r3		/ подряд: свой байт
5:	dec	r2
	movb	r3, (r4)
	add	024(sp), r4
	sob	r5, 2b
	SNDCHK
	tst	030(sp)
	beq	51f
	inc	2(sp)
	br	52f
51:	dec	2(sp)
52:	dec	4(sp)
	bne	1b
	add	$6, sp
	mov	$PG_SPR, r0		/ строки пар — нули
	jsr	pc, map
	mov	024(sp), r1
	mov	026(sp), r0
	asr	r0			/ слов в строке
	mov	020(sp), r2
6:	mov	r0, r3
7:	clr	(r1)+
	sob	r3, 7b
	sob	r2, 6b
	mov	$NIBAM, rqa+2		/ таблицы порций — по зеркалу (копия кода в PG_SPR)
	mov	$NIBBM, rqb+2
	tst	022(sp)
	bne	8f
	mov	$NIBAN, rqa+2
	mov	$NIBBN, rqb+2
8:	mov	$_band, r2		/ байты кадра строками
	mov	024(sp), r5		/ строка пар
	mov	020(sp), -(sp)		/ строк осталось
	mov	$_bounce, -(sp)		/ порции чётности строки
	/ (sp) порции, 2 строк; ncol 022, w2 032, k4 034, par 036
	tst	036(sp)
	beq	9f
	add	034(sp), (sp)
9:	mov	(sp), r3
	mov	r5, r4
	mov	022(sp), -(sp)		/ байтов кадра в строке
10:	movb	(r2)+, r0
rqb:	movb	0(r0), r1		/ 4 * вторая порция байта
rqa:	movb	0(r0), r0		/ 4 * первая
	add	(r3)+, r0
	add	(r3)+, r4
	bisb	(r0)+, (r4)+
	bisb	(r0)+, (r4)+
	bisb	(r0)+, (r4)+
	bisb	(r0), (r4)
	add	(r3)+, r1
	add	(r3)+, r4
	bisb	(r1)+, (r4)+
	bisb	(r1)+, (r4)+
	bisb	(r1)+, (r4)+
	bisb	(r1), (r4)
	dec	(sp)
	bne	10b
	tst	(sp)+
	add	032(sp), r5		/ следующая строка пар
	mov	(sp), r0		/ порции другой чётности
	cmp	r0, $_bounce
	bne	11f
	add	034(sp), r0
	br	12f
11:	mov	$_bounce, r0
12:	mov	r0, (sp)
	SNDCHK
	dec	2(sp)
	bne	9b
	cmp	(sp)+, (sp)+
	jmp	spexit
