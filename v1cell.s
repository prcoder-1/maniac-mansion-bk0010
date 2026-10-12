/ Строка клеток фона v1 (общая копия кода; gfx_v1.c render_group): 8 строк группы
/ из 5 полос и маска. Таблицы (gfx_v1.c gp_*) и символы комнаты — в PG_SWAP: она
/ подключается на время сборки, работа идёт в ОЗУ СМК без тактов ожидания; в
/ базовом ОЗУ — только параметры и готовые строки (V1P, V1LB), затем PG_BITMAP и
/ PG_MASK — запись. Си-образец — render_group (HOST).
	.globl _v1_cell, pcode, _cur_page, _map_now, _snd_tick
	.text

	PG_BITMAP = 4
	PG_MASK = 5
	PG_SWAP = 6
	V1_CHR = 0134600		/ PG_SWAP: символы комнаты
	V1_MCH = 0140600		/ символы маски
	GPE = 0144600			/ [11][256] индексы точек байта символа (сдвиг 128)
	GPT = 0152200			/ [2][256] байт БК в общих цветах (по чётности строки; сдвиг 128)
	GPM3 = 0153200			/ [256] точки цвета клетки (сдвиг 128)
	C3T = 0153600			/ [5][8][2][8] биты полосы k цвета клетки c
	SCR = 0155000			/ рабочее: символы [5][8], затем цвет клеток [2][8]
	NCB = SCR+050			/ дополнение цвета клетки по чётности строки

	P = 025000			/ параметры (gfx_v1.c struct v1p)
	NB = P+0			/ байт строки фона в группе (до 8)
	BA = P+2			/ байт фона первой строки
	BS = P+4			/ шаг строки фона
	MA = P+6			/ байт маски первой строки (полоса k0)
	MS = P+010			/ шаг строки маски
	K0 = P+012
	K1 = P+014
	PAR = P+016			/ чётность первой строки
	FULL = P+020			/ все 5 полос: без слияния со старым
	VM = P+022			/ [8] биты выбранных полос в байте строки
	CHR = P+032			/ [5] символ, цвет клетки, символ маски по полосе
	C3 = P+037
	MCHR = P+044
	NVM = P+052			/ [8] дополнение VM
	NCY = P+062			/ строк клеток
	CELLS = P+064			/ их записи по 16 байт: символы, цвета, маска
	BA8 = P+066			/ шаг 8 строк фона
	MA8 = P+070			/ шаг 8 строк маски
	LV = P+072			/ LAST действительна (Си ставит 0 на группу)
	LAST = P+074			/ [15] клетки прошлой строки: такая же — строки те же
	WFAST = P+0114			/ все полосы, 8 байт, чётные адреса — словами
	LB = 025200			/ [8][8] готовые строки фона
	MB = 025300			/ [8][5] строки маски (полосы k0..k1-1)

/ включить страницу r0; портит r0
map:	asl	r0
	mov	pcode(r0), r0
	bis	$0120, r0
	mov	$6, @$0177130
	mov	r0, @$0177130
	mov	$0, @$0177130
	mov	r0, @$_map_now
	rts	pc

/ байт строки из одной полосы: x = GPE[E][символ полосы K, строка r]. Таблицы GPE,
/ GPM3, GPT — со сдвигом на 128 (gfx_v1.c gp_put_x): индексы — байты со знаком
.macro	ONE	k, e, i
	movb	\k*8(r1), r0
	movb	GPE+0200+\e*0400(r0), r5
	movb	GPM3+0200(r5), r0
	bicb	\i(r3), r0		/ & цвет клетки (r3 — дополнение)
	add	r2, r5
	bisb	(r5), r0
	movb	r0, (r4)+
.endm
.macro	TWO	k1, e1, k2, e2, i
	movb	\k1*8(r1), r0
	movb	GPE+0200+\e1*0400(r0), r5
	movb	\k2*8(r1), r0
	bisb	GPE+0200+\e2*0400(r0), r5
	movb	r5, r5			/ знак — по новому младшему байту
	movb	GPM3+0200(r5), r0
	bicb	\i(r3), r0
	add	r2, r5
	bisb	(r5), r0
	movb	r0, (r4)+
.endm

/ snd_tick, только если подошёл срок DL (snd.s: _sndst+022, такт 0306); портит r0
.macro	SNDP
	mov	@$0177710, r0
	sub	@$023762, r0
	dec	r0
	cmp	r0, $0306
	blo	.Lsp\@
	jsr	pc, _snd_tick
.Lsp\@:
.endm

/ void v1_cell(void): строки клеток группы, параметры — в V1P
_v1_cell:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
cell:	mov	@$CELLS, r1		/ запись клеток -> CHR, C3, MCHR (15 байт)
	mov	$CHR, r2
	mov	$017, r3
1:	movb	(r1)+, (r2)+
	sob	r3, 1b
	mov	$CHR, r1		/ та же, что прошлая (небо, стены) — LB и MB те же
	mov	$LAST, r2
	mov	$017, r3
	tst	@$LV
	beq	3f
2:	cmpb	(r1)+, (r2)+
	bne	3f
	sob	r3, 2b
	jmp	wr
3:	jsr	pc, _snd_tick
	mov	$CHR, r1
	mov	$LAST, r2
	mov	$017, r3
4:	movb	(r1)+, (r2)+
	sob	r3, 4b
	mov	$1, @$LV
	jsr	pc, _snd_tick		/ сверка и копия клеток — ~100 команд
	mov	$PG_SWAP, r0
	jsr	pc, map
/ символы полос в SCR (вне [k0, k1) — нули), строки маски в MB, цвет клеток
	mov	$SCR, r4
	mov	$NCB, r1		/ сначала — сам цвет (ИЛИ по полосам)
	mov	$010, r2
1:	clr	(r1)+
	sob	r2, 1b
	clr	r3			/ k
2:	cmp	r3, @$K0
	blo	5f
	cmp	r3, @$K1
	bhis	5f
	clr	r1			/ символ: V1_CHR + chr * 8
	bisb	CHR(r3), r1
	asl	r1
	asl	r1
	asl	r1
	add	$V1_CHR, r1
	mov	(r1)+, (r4)+
	mov	(r1)+, (r4)+
	mov	(r1)+, (r4)+
	mov	(r1)+, (r4)+
	clr	r1			/ маска: ~V1_MCH[mchr * 8 + r] -> MB[r][k - k0]
	bisb	MCHR(r3), r1
	asl	r1
	asl	r1
	asl	r1
	add	$V1_MCH, r1
	mov	r3, r2
	sub	@$K0, r2
	add	$MB, r2
	mov	$010, r5
3:	movb	(r1)+, r0
	comb	r0
	movb	r0, (r2)
	add	$5, r2
	sob	r5, 3b
	SNDP
	clr	r1			/ цвет клетки: NCB |= C3T[k][c3]
	bisb	C3(r3), r1
	mov	r3, r2
	asl	r2
	asl	r2
	asl	r2
	add	r2, r1			/ k * 8 + c
	swab	r1
	clrb	r1
	asr	r1
	asr	r1
	asr	r1
	asr	r1			/ * 16
	add	$C3T, r1
	mov	$NCB, r2
	mov	$010, r5
4:	bis	(r1)+, (r2)+
	sob	r5, 4b
	br	6f
5:	clr	(r4)+			/ полоса вне диапазона — нули
	clr	(r4)+
	clr	(r4)+
	clr	(r4)+
6:	SNDP		/ полоса — ~80 команд
	inc	r3
	cmp	r3, $5
	blo	2b
	mov	$NCB, r1		/ дополнение
	mov	$010, r2
7:	com	(r1)+
	sob	r2, 7b
/ 8 строк в LB
	mov	$LB, r4
	mov	$SCR, r1
	mov	@$PAR, r0
	bic	$0177776, r0
	mov	r0, -(sp)		/ 2(sp) — чётность строки
	mov	$010, -(sp)		/ (sp) — строк осталось
10:	cmp	(sp), $6		/ строка r >= 2 с теми же символами полос, что r - 2
	bhi	22f			/ (чётность та же) — копия готовой
	cmpb	(r1), -2(r1)
	bne	22f
	cmpb	010(r1), 6(r1)
	bne	22f
	cmpb	020(r1), 016(r1)
	bne	22f
	cmpb	030(r1), 026(r1)
	bne	22f
	cmpb	040(r1), 036(r1)
	bne	22f
	mov	-020(r4), (r4)+
	mov	-020(r4), (r4)+
	mov	-020(r4), (r4)+
	mov	-020(r4), (r4)+
	SNDP
	jmp	23f
22:	mov	2(sp), r0
	mov	r0, r2
	swab	r2
	add	$GPT+0200, r2
	mov	r0, r3
	asl	r3
	asl	r3
	asl	r3
	add	$NCB, r3
	ONE	0, 0, 0
	TWO	0, 1, 1, 2, 1
	ONE	1, 3, 2
	SNDP		/ треть строки (база ОЗУ медленная: строка — до 2 мс)
	TWO	1, 4, 2, 5, 3
	ONE	2, 6, 4
	ONE	3, 7, 5
	SNDP
	TWO	3, 8, 4, 9, 6
	ONE	4, 10, 7
	SNDP
23:	inc	r1
	mov	2(sp), r0
	inc	r0
	bic	$0177776, r0
	mov	r0, 2(sp)
	dec	(sp)
	beq	11f
	jmp	10b
11:	cmp	(sp)+, (sp)+
/ фон: 8 строк по NB байт (не все полосы — слияние со старым по VM)
wr:	mov	$PG_BITMAP, r0
	jsr	pc, map
	mov	$LB, r1
	mov	@$BA, r3
	mov	$010, r4
	tst	@$WFAST
	beq	12f
19:	mov	r3, r2			/ словами
	mov	(r1)+, (r2)+
	mov	(r1)+, (r2)+
	mov	(r1)+, (r2)+
	mov	(r1)+, (r2)+
	add	@$BS, r3
	sob	r4, 19b
	br	20f
12:	mov	r3, r2
	clr	r5			/ i
	tst	@$FULL
	beq	14f
13:	movb	(r1)+, (r2)+
	inc	r5
	cmp	r5, @$NB
	blo	13b
	br	15f
14:	movb	(r2), r0		/ (старое & ~VM) | (новое & VM)
	bicb	VM(r5), r0
	mov	r0, -(sp)
	movb	(r1)+, r0
	bicb	NVM(r5), r0
	bisb	(sp)+, r0
	movb	r0, (r2)+
	inc	r5
	cmp	r5, @$NB
	blo	14b
15:	add	$010, r1		/ следующая строка LB: начало + 8
	sub	r5, r1
	add	@$BS, r3
	SNDP		/ строка со слиянием — до 80 команд
	sob	r4, 12b
20:	jsr	pc, _snd_tick
/ маска: 8 строк по k1 - k0 байт
	mov	$PG_MASK, r0
	jsr	pc, map
	mov	$MB, r1
	mov	@$MA, r3
	mov	$010, r4
16:	mov	r3, r2
	mov	@$K1, r5
	sub	@$K0, r5
	mov	r1, r0
17:	movb	(r0)+, (r2)+
	sob	r5, 17b
	add	$5, r1
	add	@$MS, r3
	cmp	r4, $4			/ полпути — фронт тона
	bne	21f
	SNDP
21:	sob	r4, 16b
	jsr	pc, _snd_tick
	add	$020, @$CELLS		/ следующая строка клеток
	add	@$BA8, @$BA
	add	@$MA8, @$MA
	dec	@$NCY
	beq	18f
	jmp	cell
18:	mov	$6, @$0177130		/ страница вызывающего
	mov	_cur_page, @$0177130
	mov	$0, @$0177130
	mov	_cur_page, @$_map_now
	jsr	pc, _snd_tick
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
