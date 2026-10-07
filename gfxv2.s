/ Декодер графики SCUMM v2 (общая копия кода): gfx.cpp GdiV2::prepareDrawBitmap.
/ Си-модель с той же логикой — test/host/host_gfx.c; обёртка — gfx_v2.c.
/
/ Картинка: RLE по колонкам сверху вниз, серия — цвет (бит 7 = 0) или «повтор»
/ точек предыдущей колонки (бит 7 = 1, таблица _v2_dt). Колонка мира с x%5 == 4
/ (m5 == 4) отбрасывается, остальные m5 = 0..3 — точки k байта БК; цвета EGA
/ колонок копятся в _v2_col (4 x 128), байт собирается по таблицам _v2_pat и
/ пишется в PG_BITMAP раз на 5 колонок.

PG_BITMAP_CODE = 0130		/ ОЗУ10 + страница 4
PG_MASK_CODE   = 02130		/ ОЗУ10 + страница 5

	.globl _v2_pix, _v2_mask, _v2_dt, _v2_col, _v2_pat, _cur_page, pcode, _snd_tick
	.text

/ uint16_t v2_pix(struct v2p *p) -> адрес за картинкой
/ struct v2p { 0 spage, 2 src, 4 w, 6 h, 010 m5, 012 dest, 014 stride, 016 yodd,
/   020 run0, 022 color0 (состояние RLE на входе), 024 tab (StripTable или 0),
/   026 m8 (x0 % 8) }. На колонке с x%8 == 0 в tab пишется {src, серия, цвет}.
/ Локальные (после сохранения r2..r5 и sub $024): 0 строк осталось, 2 колонок
/ осталось, 4 m5, 6 dest, 010 заполнено (биты k), 012 код страницы-источника,
/ 014 stride, 016 h, 020 m8, 022 tab; аргумент p — 036(sp).
_v2_pix:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	sub	$024, sp
	mov	036(sp), r0
	mov	(r0), r1
	asl	r1
	mov	pcode(r1), r1
	bis	$0120, r1
	mov	r1, 012(sp)
	mov	2(r0), r1		/ src
	mov	4(r0), 2(sp)
	mov	010(r0), 4(sp)
	mov	012(r0), 6(sp)
	mov	014(r0), 014(sp)
	mov	6(r0), 016(sp)
	mov	026(r0), 020(sp)
	mov	024(r0), 022(sp)
	clr	010(sp)
	mov	020(r0), r4		/ осталось точек серии: 0 — читать байт
	mov	022(r0), r5		/ цвет или -1 (повтор)
	mov	$_v2_dt, r0		/ dither_table обнулена
	mov	$0100, r2
1:	clr	(r0)+
	sob	r2, 1b
	mov	$6, @$0177130
	mov	012(sp), @$0177130
	mov	$0, @$0177130
	mov	012(sp), @$_map_now
	mov	016(sp), r0		/ END = _v2_dt + h — в команды серий (в копии кода
	add	$_v2_dt, r0		/ страницы-источника: она уже подключена)
	mov	r0, sv_e1+2
	mov	r0, sv_e2+2
	mov	r0, sv_e3+2
	mov	r0, sv_e4+2

col:	jsr	pc, _snd_tick		/ фронт фонового тона — на каждой колонке
	tst	2(sp)
	bne	1f
	jmp	done
1:	dec	2(sp)
	tst	022(sp)			/ начало полосы: запись в StripTable
	beq	2f
	tst	020(sp)
	bne	2f
	mov	022(sp), r0
	mov	r1, (r0)+
	movb	r4, (r0)+
	movb	r5, (r0)+
	mov	r0, 022(sp)
2:	inc	020(sp)
	bic	$0177770, 020(sp)
	mov	$_v2_dt, r2
	mov	4(sp), r0
	cmp	r0, $4
	beq	skip
	bisb	fbit(r0), 010(sp)
	swab	r0			/ r3 = _v2_col + m5 * 128
	asr	r0
	add	$_v2_col, r0
	mov	r0, r3
/ серии колонки: строк осталось = END - r2 (END = _v2_dt + h — константа в
/ командах, sv_end); байт RLE разбирается по таблицам rl_len/rl_col
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
6:	movb	(r2)+, (r3)+		/ повтор колонки слева
	sob	r0, 6b
7:
sv_e2:	cmp	r2, $0
	blo	seg
	cmp	4(sp), $3
	beq	8f
	tst	2(sp)
	bne	9f
8:	jsr	pc, flush
9:	inc	4(sp)
	jmp	col

/ отброшенная колонка (x%5 == 4): только таблица повторов
skip:	tst	r4
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
2:
sv_e3:	mov	$0, r0
	sub	r2, r0
	cmp	r4, r0
	bhis	4f
	mov	r4, r0
4:	sub	r0, r4
	tst	r5
	bmi	6f
5:	movb	r5, (r2)+
	sob	r0, 5b
	br	7f
6:	add	r0, r2
7:
sv_e4:	cmp	r2, $0
	blo	skip
	tst	2(sp)			/ последняя колонка, а байт не записан
	bne	8f
	tst	010(sp)
	beq	8f
	jsr	pc, flush
8:	clr	4(sp)
	jmp	col

done:	mov	$6, @$0177130
	mov	_cur_page, @$0177130
	mov	$0, @$0177130
	mov	_cur_page, @$_map_now
	mov	r1, r0
	add	$024, sp
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ байт RLE (бит 7 = 0) -> длина (биты 4..6) и цвет (биты 0..3)
rl_len:
	.byte	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	.byte	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
	.byte	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2
	.byte	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3
	.byte	4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
	.byte	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5
	.byte	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6
	.byte	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
rl_col:
	.byte	0,1,2,3,4,5,6,7,010,011,012,013,014,015,016,017
	.byte	0,1,2,3,4,5,6,7,010,011,012,013,014,015,016,017
	.byte	0,1,2,3,4,5,6,7,010,011,012,013,014,015,016,017
	.byte	0,1,2,3,4,5,6,7,010,011,012,013,014,015,016,017
	.byte	0,1,2,3,4,5,6,7,010,011,012,013,014,015,016,017
	.byte	0,1,2,3,4,5,6,7,010,011,012,013,014,015,016,017
	.byte	0,1,2,3,4,5,6,7,010,011,012,013,014,015,016,017
	.byte	0,1,2,3,4,5,6,7,010,011,012,013,014,015,016,017
	.even

/ собрать байты колонки из _v2_col и записать в PG_BITMAP (dest, шаг stride);
/ незаполненные точки байта (края объекта) не трогаются
/ кадр: r1 r4 r5 сохранены (6) + адрес возврата (2): локальные — со сдвигом 010
flush:
	mov	r1, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	$6, @$0177130
	mov	$PG_BITMAP_CODE, @$0177130
	mov	$0, @$0177130
	mov	$PG_BITMAP_CODE, @$_map_now
	mov	020(sp), r0		/ заполнено
	clr	r2
	bisb	fmask(r0), r2		/ биты заполненных точек
	mov	r2, -(sp)		/ (sp) = маска; локальные — со сдвигом 012
	mov	$_v2_col, r1
	mov	020(sp), r4		/ dest
	mov	030(sp), r5		/ h
	mov	$_v2_pat, r3
	mov	050(sp), r0		/ p (036 + 012)
	tst	016(r0)			/ нечётная первая строка
	beq	1f
	add	$0200, r3
1:	cmpb	(sp), $0377
	beq	fl2
/ край объекта (байт заполнен не весь): по строке, с маской
1:	movb	(r1), r0
	add	r3, r0
	movb	(r0), r2
	movb	0200(r1), r0
	add	r3, r0
	bisb	040(r0), r2
	movb	0400(r1), r0
	add	r3, r0
	bisb	0100(r0), r2
	movb	0600(r1), r0
	add	r3, r0
	bisb	0140(r0), r2
	inc	r1
	bicb	(sp), (r4)		/ старые точки вне заполненных
	mov	(sp), r0
	com	r0
	bic	r0, r2
	bisb	r2, (r4)
	add	026(sp), r4		/ stride
	mov	$0200, r0
	xor	r0, r3			/ таблица другой чётности строки
	bit	$3, r5
	bne	4f
	jsr	pc, _snd_tick
4:	sob	r5, 1b
	br	fl9
/ байт целиком: строки парами — первая по таблице r3, вторая по r3 ^ 0200
/ (смещение записано в команды); шаг строки — тоже в командах
fl2:	mov	026(sp), fla_st+2
	mov	026(sp), flb_st+2
	mov	$0200, r0
	bit	r0, r3
	beq	1f
	neg	r0			/ r3 — нечётная таблица: вторая строка на 0200 ниже
1:	mov	r0, flb0+2
	add	$040, r0
	mov	r0, flb1+2
	add	$040, r0
	mov	r0, flb2+2
	add	$040, r0
	mov	r0, flb3+2
	asr	r5			/ пар строк; нечётная последняя — после
	bcc	2f
	inc	r5			/ (флаг нечётности — в старшем бите счётчика)
	bis	$0100000, r5
2:	mov	r5, -(sp)
	bic	$0100000, r5
	beq	8f
3:	movb	(r1), r0
	add	r3, r0
	movb	(r0), r2
	movb	0200(r1), r0
	add	r3, r0
	bisb	040(r0), r2
	movb	0400(r1), r0
	add	r3, r0
	bisb	0100(r0), r2
	movb	0600(r1), r0
	add	r3, r0
	bisb	0140(r0), r2
	movb	r2, (r4)
fla_st:	add	$0, r4
	dec	r5			/ нечётная высота: последняя пара — одна строка
	bne	5f
	tst	(sp)
	bmi	8f
5:	inc	r5
	movb	1(r1), r0
	add	r3, r0
flb0:	movb	0(r0), r2
	movb	0201(r1), r0
	add	r3, r0
flb1:	bisb	0(r0), r2
	movb	0401(r1), r0
	add	r3, r0
flb2:	bisb	0(r0), r2
	movb	0601(r1), r0
	add	r3, r0
flb3:	bisb	0(r0), r2
	movb	r2, (r4)
flb_st:	add	$0, r4
	add	$2, r1
	bit	$7, r5
	bne	6f
	jsr	pc, _snd_tick
6:	sob	r5, 3b
8:	tst	(sp)+
fl9:
	tst	(sp)+
	inc	016(sp)			/ dest: следующий байт
	clr	020(sp)			/ заполнено
	mov	$6, @$0177130
	mov	022(sp), @$0177130	/ снова страница-источник
	mov	$0, @$0177130
	mov	022(sp), @$_map_now
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r1
	rts	pc

fbit:	.byte	1, 2, 4, 010
fmask:	.byte	0, 03, 014, 017, 060, 063, 074, 077
	.byte	0300, 0303, 0314, 0317, 0360, 0363, 0374, 0377
	.even

/ uint16_t v2_mask(struct v2m *p) -> адрес за маской
/ struct v2m { 0 spage, 2 src, 4 nstrips, 6 h, 010 dest, 012 stride, 014 run0,
/   016 flag0, 020 data0 (состояние RLE на входе), 022 tab (StripTable или 0:
/   на полосу {src, серия | флаг<<7, байт}) } — маска
/ в PG_MASK: байт на полосу 8 точек на строку. Серия: длина с битом 7 —
/ повтор следующего байта, иначе столько байтов подряд; серия переходит через
/ границу полос. Полоса собирается в _v2_dt и пишется в PG_MASK целиком.
_v2_mask:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r0
	mov	(r0), r1
	asl	r1
	mov	pcode(r1), r1
	bis	$0120, r1
	mov	r1, -(sp)		/ (sp) код страницы-источника, p -> 014(sp)
	mov	2(r0), r1
	mov	014(r0), r4		/ осталось в серии
	mov	016(r0), r3		/ 1 — серия повтора
	mov	020(r0), r5
	mov	$6, @$0177130
	mov	(sp), @$0177130
	mov	$0, @$0177130
	mov	(sp), @$_map_now
	mov	014(sp), r0
	mov	6(r0), r2		/ END = _v2_dt + h — в команды (копия кода
	add	$_v2_dt, r2		/ страницы-источника: она уже подключена)
	mov	r2, mk_e1+2
	mov	r2, mk_e2+2
	mov	4(r0), -(sp)		/ (sp) полос осталось, код — 2(sp), p — 016(sp)
	beq	9f
1:	mov	016(sp), r0		/ начало полосы: запись в StripTable
	mov	022(r0), r2
	beq	3f
	mov	r1, (r2)+
	movb	r4, (r2)
	tst	r3
	beq	4f
	bisb	$0200, (r2)
4:	inc	r2
	movb	r5, (r2)+
	mov	r2, 022(r0)
3:	mov	$_v2_dt, r2		/ полоса: серии до конца буфера (END — в командах)
2:	tst	r4
	bne	3f
	movb	(r1)+, r4
	bic	$0177400, r4
	clr	r3
	bit	$0200, r4
	beq	3f
	bic	$0200, r4
	inc	r3
	movb	(r1)+, r5
3:
mk_e1:	mov	$0, r0			/ n = min(серия, строк осталось)
	sub	r2, r0
	cmp	r4, r0
	bhis	4f
	mov	r4, r0
4:	sub	r0, r4
	tst	r3
	beq	6f
5:	movb	r5, (r2)+		/ повтор байта
	sob	r0, 5b
	br	7f
6:	movb	(r1)+, r5		/ байты подряд
	movb	r5, (r2)+
	sob	r0, 6b
7:
mk_e2:	cmp	r2, $0
	blo	2b
	mov	$6, @$0177130		/ полоса -> PG_MASK
	mov	$PG_MASK_CODE, @$0177130
	mov	$0, @$0177130
	mov	$PG_MASK_CODE, @$_map_now
	mov	016(sp), r0
	mov	010(r0), r2		/ адрес
	inc	010(r0)			/ следующая полоса
	mov	r5, -(sp)		/ байт повтора: серия RLE продолжается в следующей полосе
	mov	r1, -(sp)
	mov	012(r0), r5		/ шаг
	mov	6(r0), r0
	mov	$_v2_dt, r1
5:	movb	(r1)+, (r2)
	add	r5, r2
	sob	r0, 5b
	mov	(sp)+, r1
	mov	(sp)+, r5
	mov	$6, @$0177130
	mov	2(sp), @$0177130
	mov	$0, @$0177130
	mov	2(sp), @$_map_now
	dec	(sp)
	bne	1b
9:	tst	(sp)+
	tst	(sp)+
	mov	$6, @$0177130
	mov	_cur_page, @$0177130
	mov	$0, @$0177130
	mov	_cur_page, @$_map_now
	mov	r1, r0
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
