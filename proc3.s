/ proc3 ClassicCostumeRenderer (costume.cpp) для БК (общая копия кода): точки
/ кадра актёра по колонкам в видеопамять или во внеэкранный буфер (в странице
/ маски). Колонка мира x%5 == 4 выброшена (сжатие 4:5), z-маска читается в её
/ странице (PG_MASK подключается на время работы). Параметры — struct P3 в
/ _bounce (costume_r.c). Там же cv_load/cv_bg/cv_store — буфер <-> экран, фон.
	.globl _proc3_run, _bounce, _cur_page, _div5, pcode, _snd_tick
	.globl _cv_load, _cv_store, _cv_bg
	.text

	B = _bounce
	SRC = B+0
	COLOR = B+4
	VY = B+6
	H = B+010
	SKIP = B+012
	STEP = B+014
	X = B+016
	XSTART = B+020
	VIEW0 = B+022
	ZBUF = B+024
	MROW0 = B+026
	MSTRIDE = B+030
	CPAT = B+032
	DROW0 = B+0132			/ байт цели: строка VY, bx = 0
	DSTRIDE = B+0134		/ шаг строки цели
	BXLIM = B+0136			/ точек БК в строке цели
	YLIM = B+0140			/ строк цели (y — от её верха)
	/ рабочие поля — в странице маски (ОЗУ СМК без тактов ожидания; хвост
	/ файла OVF_ADDR там только при загрузке и кончается до 0156600)
	S = 0157400
	VE = S+0			/ байт: точка в чётной строке (слово VE|VO)
	VO = S+1			/ байт: точка в нечётной строке
	OK = S+2			/ байт: колонка видна
	NK = S+3			/ байт: маска точки в байте БК
	MBIT = S+4			/ байт: бит полосы маски (0 — без маски)
	ODD = S+5			/ байт: bx & 1
	ZERO = S+6			/ байт 0: «маска» без маски
	K4 = S+010			/ (bx & 3) * 4
	MSTEP = S+012			/ шаг строки маски (0 — без маски)
	MPTR = S+014			/ байт маски текущей точки
	SDSTR = S+016			/ копии DSTRIDE, YLIM в быстром ОЗУ
	SYLIM = S+020
	R4S = S+022			/ серия и остаток колонки после быстрого цикла
	R5S = S+024
	VYH = S+026			/ VY + H: чётность строки в быстром режиме — (VYH + r5) & 1
	COLTB = S+030			/ таблица цветов колонки (colset)
	TBL = 0151000			/ 256 байт: [k][bx & 1][цвет] -> VE | VO << 8 (mktbl)
	TBLC = 0151400			/ 64 байта: CPAT, по которой построена TBL
	STAB = 0136000			/ 256 слов: n * DSTRIDE (mkmul; маска кончается до 0136000)
	MTAB = 0137000			/ 256 слов: n * MSTRIDE

	OUT_W = 320
	PG_MASK = 5
	PG_BITMAP = 4

/ void proc3_run(struct P3 *p): p == _bounce
/ регистры: r0 — цвет точки (младший байт; старший — для следующей строки),
/ r1 — байт видеопамяти, r2 — временный, r3 — y, r4 — серия, r5 — h
_proc3_run:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	jsr	pc, mapmask
	clrb	@$ZERO
	mov	@$DSTRIDE, @$SDSTR
	mov	@$YLIM, @$SYLIM
	mov	@$DSTRIDE, fu_st+2	/ константы быстрых циклов — прямо в команды
	mov	@$DSTRIDE, fm_st+2	/ (копия кода в странице маски, она подключена)
	mov	@$MSTRIDE, fm_ms+2
	mov	@$MSTRIDE, ffm_ms+2
	mov	@$DSTRIDE, ffu_st+2
	mov	@$DSTRIDE, ffm_st+2
	cmp	@$VY, @$SYLIM		/ все строки VY..VY+H-1 в цели — быстрый режим
	bhis	1f
	mov	@$VY, r0
	add	@$H, r0
	cmp	r0, @$SYLIM
	bhi	1f
	jmp	fast
1:	mov	@$VY, r3
	mov	@$H, r5
	jsr	pc, colset
	mov	@$B+2, r4		/ незаконченная серия: вход в StartPos
	beq	newrun
	jmp	cont
newrun:	mov	r1, -(sp)
	mov	@$SRC, r0
	clr	r4
	bisb	(r0)+, r4
	mov	r4, r1
	asr	r1
	asr	r1
	asr	r1
	asr	r1
	mov	r1, @$COLOR		/ цвет — старшая тетрада
	bic	$0177760, r4
	bne	1f
	bisb	(r0)+, r4
	bne	1f
	mov	$0400, r4		/ 0 — 256 точек
1:	mov	r0, @$SRC
	mov	(sp)+, r1
/ r4 точек серии (с текущей) в колонке
dorun:	tstb	@$OK
	beq	skip
	tst	@$COLOR
	beq	skip
	jsr	pc, runset
/ быстрый путь: n = min(серия, остаток колонки) строк, все в [0, YLIM) — без
/ проверки y и второго счётчика
	mov	r4, r2
	cmp	r2, r5
	blos	1f
	mov	r5, r2
1:	cmp	r3, @$SYLIM
	bhis	slow
	mov	r2, -(sp)
	add	r3, (sp)
	cmp	(sp)+, @$SYLIM
	bhi	slow
	sub	r2, r4
	sub	r2, r5
	add	r2, r3
	mov	r4, @$R4S
	mov	r5, @$R5S
	mov	r2, r4			/ счётчик
	tst	@$MSTEP
	bne	fm
fu:	movb	(r1), r2		/ одно чтение, одна запись
fu_nk:	bicb	$0, r2			/ NK (colset)
	bisb	r0, r2
	movb	r2, (r1)
fu_st:	add	$0, r1			/ DSTRIDE
	swab	r0
	sob	r4, fu
	br	fend
fm:	mov	@$MPTR, r5
fm_mb:	bitb	$0, (r5)		/ MBIT (colset)
	bne	1f
	movb	(r1), r2
fm_nk:	bicb	$0, r2
	bisb	r0, r2
	movb	r2, (r1)
1:
fm_st:	add	$0, r1
fm_ms:	add	$0, r5			/ MSTRIDE
	swab	r0
	sob	r4, fm_mb
	mov	r5, @$MPTR
fend:	mov	@$R4S, r4
	mov	@$R5S, r5
	bne	2f			/ колонка не кончилась: кончилась серия (r4 = 0)
	inc	r4			/ как в uloop: r4 — с последней точкой
	br	colend
2:	jmp	newrun
slow:	tst	@$MSTEP
	bne	mloop
uloop:	cmp	r3, @$SYLIM		/ y без знака: < 0 тоже мимо
	bhis	1f
	movb	(r1), r2		/ видеопамять: одно чтение, одна запись
	bicb	@$NK, r2
	bisb	r0, r2
	movb	r2, (r1)
1:	add	@$SDSTR, r1
	inc	r3
	swab	r0
	dec	r5
	beq	colend
	dec	r4
	bne	uloop
	jmp	newrun
mloop:	cmp	r3, @$SYLIM
	bhis	1f
	bitb	@$MBIT, @MPTR
	bne	1f
	movb	(r1), r2
	bicb	@$NK, r2
	bisb	r0, r2
	movb	r2, (r1)
1:	add	@$SDSTR, r1
	add	@$MSTEP, @$MPTR
	inc	r3
	swab	r0
	dec	r5
	beq	colend
	dec	r4
	bne	mloop
	jmp	newrun
/ прозрачная серия или невидимая колонка — целиком
skip:	cmp	r4, r5
	bhis	2f
	sub	r4, r5			/ серия кончается в колонке
	add	r4, r3
	mov	r4, r0
	mov	@$SDSTR, r2
4:	add	r2, r1
	sob	r0, 4b
	tst	@$MSTEP
	beq	5f
	mov	r4, r0
	mov	@$MSTEP, r2
	mov	r1, -(sp)
	mov	@$MPTR, r1
3:	add	r2, r1
	sob	r0, 3b
	mov	r1, @$MPTR
	mov	(sp)+, r1
5:	jmp	newrun
2:	sub	r5, r4			/ до конца колонки: как r5 шагов цикла
	inc	r4
colend:	dec	@$SKIP			/ конец колонки
	beq	done
	jsr	pc, _snd_tick		/ фронт фонового тона между колонками
	mov	@$H, r5
	mov	@$VY, r3
	add	@$STEP, @$X
	cmp	@$X, $OUT_W
	bhis	done
	jsr	pc, colset
cont:	dec	r4
	beq	1f
	jmp	dorun
1:	jmp	newrun
done:	mov	$6, @$0177130
	mov	_cur_page, @$0177130
	mov	$0, @$0177130
	mov	_cur_page, @$_map_now
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ быстрый режим (все строки в цели): r3 — указатель RLE, r4 — остаток серии,
/ r5 — строк до конца колонки, r1 — байт цели, цвет серии — COLOR. Пары точек
/ серии — из таблицы TBL (mktbl), без runset; y не считается
fast:	jsr	pc, mktbl
	jsr	pc, mkmul
	mov	@$VY, r0
	add	@$H, r0
	mov	r0, @$VYH
	mov	@$H, r5
	jsr	pc, colset
	mov	@$SRC, r3
	mov	@$B+2, r4		/ незаконченная серия (с уже пройденной точкой)
	beq	fnew
	dec	r4
	bne	frun
fnew:	clr	r4
	bisb	(r3)+, r4
	mov	r4, r2
	asr	r2
	asr	r2
	asr	r2
	asr	r2
	mov	r2, @$COLOR
	bic	$0177760, r4
	bne	frun
	bisb	(r3)+, r4
	bne	frun
	mov	$0400, r4
frun:	mov	r4, r2			/ n = min(серия, остаток колонки)
	cmp	r2, r5
	blos	1f
	mov	r5, r2
1:	sub	r2, r4
	sub	r2, r5
	mov	r4, @$R4S
	mov	r2, r4			/ счётчик
	tstb	@$OK
	beq	fskip
	mov	@$COLOR, r0
	beq	fskip
	asl	r0
	add	@$COLTB, r0
	mov	(r0), r0		/ VE | VO << 8
	mov	r5, r2			/ строка первой точки: VY + H - (r5 + n)
	add	r4, r2
	add	@$VYH, r2
	bit	$1, r2
	beq	2f
	swab	r0
2:	tst	@$MSTEP
	bne	ffm
ffu:	movb	(r1), r2
ffu_nk:	bicb	$0, r2
	bisb	r0, r2
	movb	r2, (r1)
ffu_st:	add	$0, r1
	swab	r0
	sob	r4, ffu
	br	gend
ffm:	mov	r3, -(sp)
	mov	@$MPTR, r3
ffm_mb:	bitb	$0, (r3)
	bne	3f
	movb	(r1), r2
ffm_nk:	bicb	$0, r2
	bisb	r0, r2
	movb	r2, (r1)
3:
ffm_st:	add	$0, r1
ffm_ms:	add	$0, r3
	swab	r0
	sob	r4, ffm_mb
	mov	r3, @$MPTR
	mov	(sp)+, r3
	br	gend
/ прозрачная серия: указатели — на n строк (таблицы); в невидимой колонке они
/ не нужны (colset считает их заново)
fskip:	tstb	@$OK
	beq	gend
	asl	r4
	add	STAB(r4), r1
	tst	@$MSTEP
	beq	gend
	add	MTAB(r4), @$MPTR
gend:	mov	@$R4S, r4
	tst	r5
	bne	fnew			/ колонка продолжается: серия кончилась
	dec	@$SKIP			/ конец колонки
	beq	fdone
	jsr	pc, _snd_tick
	mov	@$H, r5
	add	@$STEP, @$X
	cmp	@$X, $OUT_W
	bhis	fdone
	jsr	pc, colset
	tst	r4
	beq	fnew
	br	frun
fdone:	mov	r3, @$SRC		/ конец прочитанного RLE (costume_r.c: длина кадра)
	jmp	done

/ таблицы STAB, MTAB (n * шаг) — если шаги сменились; портит r0..r5
mkmul:	cmp	@$STAB+2, @$DSTRIDE
	bne	1f
	cmp	@$MTAB+2, @$MSTRIDE
	bne	1f
	tst	@$STAB
	bne	1f
	rts	pc
1:	mov	@$DSTRIDE, r4
	mov	@$MSTRIDE, r5
	clr	r0
	clr	r1
	mov	$STAB, r2
	mov	$MTAB, r3
	mov	$0400, @$R4S
2:	mov	r0, (r2)+
	mov	r1, (r3)+
	add	r4, r0
	add	r5, r1
	dec	@$R4S
	bne	2b
	rts	pc

/ таблица TBL для CPAT: [k 0..3][bx & 1][цвет 0..15] -> VE | VO << 8 (как runset),
/ если CPAT сменилась с прошлого раза (копия — TBLC); портит r0..r5
mktbl:	mov	$CPAT, r0
	mov	$TBLC, r1
	mov	$040, r2
1:	cmp	(r0)+, (r1)+
	bne	2f
	sob	r2, 1b
	rts	pc
2:	mov	$CPAT, r0
	mov	$TBLC, r1
	mov	$040, r2
3:	mov	(r0)+, (r1)+
	sob	r2, 3b
	mov	$TBL, r2
	clr	r4			/ k * 4
1:	clr	r5			/ bx & 1
2:	mov	$CPAT, r3
	add	r5, r3
	mov	$020, r1
3:	clr	r0
	bisb	(r3), r0
	add	r4, r0
	movb	pixval(r0), (r2)+
	clr	r0
	bisb	2(r3), r0
	add	r4, r0
	movb	pixval(r0), (r2)+
	add	$4, r3
	sob	r1, 3b
	inc	r5
	cmp	r5, $2
	blo	2b
	add	$4, r4
	cmp	r4, $020
	blo	1b
	rts	pc

/ подключить страницу маски; портит r0
mapmask:
	mov	@$pcode+2*PG_MASK, r0
	bis	$0120, r0
	mov	$6, @$0177130
	mov	r0, @$0177130
	mov	$0, @$0177130
	mov	r0, @$_map_now
	rts	pc

/ колонка X: OK, NK, ODD, K4; r1 — байт видеопамяти строки VY, MPTR — байт
/ маски, MBIT/MSTEP; портит r0, r2
colset:
	clrb	@$OK
	mov	$ZERO, @$MPTR
	clr	@$MSTEP
	clrb	@$MBIT
	mov	@$X, r0
	cmp	r0, $OUT_W
	bhis	9f
	add	@$XSTART, r0		/ wx
	clr	r1
	bisb	_div5(r0), r1		/ wx / 5
	mov	r1, r2
	asl	r2
	asl	r2
	add	r1, r2			/ 5 * (wx / 5)
	neg	r2
	add	r0, r2
	cmp	r2, $4			/ wx % 5 == 4 — выброшена
	beq	8f
	mov	r0, r2
	sub	r1, r2			/ BK_X(wx)
	sub	@$VIEW0, r2		/ bx
	cmp	r2, @$BXLIM
	bhis	8f
	mov	r2, r1
	bic	$0177774, r1		/ k
	movb	pixmask(r1), @$NK
	mov	r1, -(sp)		/ COLTB = TBL + (2k + (k & 1)) * 32
	asl	(sp)
	bit	$1, r1
	beq	1f
	inc	(sp)
1:	asl	(sp)
	asl	(sp)
	asl	(sp)
	asl	(sp)
	asl	(sp)
	add	$TBL, (sp)
	mov	(sp)+, @$COLTB
	movb	pixmask(r1), fu_nk+2
	movb	pixmask(r1), fm_nk+2
	movb	pixmask(r1), ffu_nk+2
	movb	pixmask(r1), ffm_nk+2
	asl	r1
	asl	r1
	mov	r1, @$K4
	movb	r2, r1
	bic	$0177776, r1
	movb	r1, @$ODD
	asr	r2
	asr	r2			/ bx >> 2
	mov	@$DROW0, r1
	add	r2, r1			/ байт цели
	tst	@$ZBUF
	beq	7f
	mov	r0, r2
	asr	r2
	asr	r2
	asr	r2
	add	@$MROW0, r2		/ байт маски: строка VY, полоса wx / 8
	mov	r2, @$MPTR
	bic	$0177770, r0
	movb	revbit(r0), @$MBIT
	movb	revbit(r0), fm_mb+2
	movb	revbit(r0), ffm_mb+2
	mov	@$MSTRIDE, @$MSTEP
7:	movb	$1, @$OK
8:
9:	rts	pc

/ серия COLOR (не 0) в видимой колонке -> r0: младший байт — точка строки r3,
/ старший — следующей
runset:
	mov	@$COLOR, r0
	asl	r0
	asl	r0
	add	$CPAT, r0
	mov	r1, -(sp)
	clr	r1
	bisb	@$ODD, r1
	add	r1, r0			/ &cpat[color][odd]
	clr	r1
	bisb	(r0), r1
	add	@$K4, r1
	movb	pixval(r1), @$VE
	clr	r1
	bisb	2(r0), r1
	add	@$K4, r1
	movb	pixval(r1), @$VO
	mov	(sp)+, r1
	mov	@$VE, r0		/ VE | VO << 8
	bit	$1, r3
	beq	9f
	swab	r0
9:	rts	pc

pixmask:	.byte	03, 014, 060, 0300
pixval:		.byte	0, 1, 2, 3, 0, 4, 010, 014, 0, 020, 040, 060, 0, 0100, 0200, 0300
revbit:		.byte	0200, 0100, 040, 020, 010, 4, 2, 1
	.even

/ включить страницу r0 (0..15); портит r0
mapr0:
	asl	r0
	mov	pcode(r0), r0
	bis	$0120, r0
	mov	$6, @$0177130
	mov	r0, @$0177130
	mov	$0, @$0177130
	mov	r0, @$_map_now
	rts	pc

/ вернуть страницу вызывающего
back:
	mov	$6, @$0177130
	mov	_cur_page, @$0177130
	mov	$0, @$0177130
	mov	_cur_page, @$_map_now
	rts	pc

/ void cv_load(uint16_t cv, uint16_t vram, uint16_t ncols, uint16_t nrows):
/ экран (строки по 64 байта) -> буфер в PG_MASK (строки по ncols)
_cv_load:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	$PG_MASK, r0
	jsr	pc, mapr0
	mov	010(sp), r1		/ буфер
	mov	012(sp), r2		/ экран
	mov	016(sp), r4		/ строк
1:	mov	014(sp), r3
	mov	r2, r0
2:	movb	(r0)+, (r1)+
	sob	r3, 2b
	add	$0100, r2
	jsr	pc, _snd_tick
	sob	r4, 1b
	br	cvend

/ void cv_store(uint16_t cv, uint16_t vram, uint16_t ncols, uint16_t nrows):
/ буфер -> экран
_cv_store:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	$PG_MASK, r0
	jsr	pc, mapr0
	mov	010(sp), r1
	mov	012(sp), r2
	mov	016(sp), r4
1:	mov	014(sp), r3
	mov	r2, r0
2:	movb	(r1)+, (r0)+
	sob	r3, 2b
	add	$0100, r2
	jsr	pc, _snd_tick
	sob	r4, 1b
cvend:	jsr	pc, back
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ void cv_bg(uint16_t cv, uint16_t cvstride, uint16_t bm, uint16_t bmstride,
/ uint16_t ncols, uint16_t nrows): фон PG_BITMAP -> буфер, строка за строкой
/ через _bounce
_cv_bg:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r4		/ буфер
	mov	016(sp), r5		/ фон
	mov	024(sp), r2		/ строк
1:	mov	$PG_BITMAP, r0
	jsr	pc, mapr0
	mov	r5, r0
	mov	$_bounce, r1
	mov	022(sp), r3
2:	movb	(r0)+, (r1)+
	sob	r3, 2b
	mov	$PG_MASK, r0
	jsr	pc, mapr0
	mov	$_bounce, r0
	mov	r4, r1
	mov	022(sp), r3
3:	movb	(r0)+, (r1)+
	sob	r3, 3b
	add	014(sp), r4
	add	020(sp), r5
	jsr	pc, _snd_tick
	sob	r2, 1b
	jsr	pc, back
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
