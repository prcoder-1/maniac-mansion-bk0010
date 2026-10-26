/ proc3 ClassicCostumeRenderer (costume.cpp) для БК (общая копия кода): точки
/ кадра актёра по колонкам в видеопамять или во внеэкранный буфер (в странице
/ маски). Колонка мира x%5 == 4 выброшена (сжатие 4:5), z-маска читается в её
/ странице (PG_MASK подключается на время работы). Параметры — struct P3 в
/ _bounce (costume_r.c). Там же cv_load/cv_bg/cv_store — буфер <-> экран, фон.
	.globl _proc3_run, _bounce, _cur_page, _div5, pcode
	.globl _cv_load, _cv_store, _cv_bg, _spr_blit, _spr_blits, _spr_blitm
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
	BMOFF = B+0144			/ постройка спрайта: маска — по этому смещению от цвета
	NEWPAL = B+0146			/ CPAT могла смениться (mktbl сверяет её с TBLC)
	SHP = B+0150			/ чётность сдвига сетки актёра: узор — по bx ^ SHP
	MXOFF = B+0152			/ байт фона точки 0 цели: маска глубины — по колонке
					/ фона под точкой (5 * байт + точка в байте)
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
	CX = S+030			/ цвет серии * 2 (быстрый режим)
	DTOK = S+034			/ DTMAGIC: таблицы LENT, COLT построены
	DTMAGIC = 0132471
	LENT = 0140400			/ 256 слов (0140000..0140777): длина серии по байту
	COLT = 0141400			/ 256 слов (0141000..0141777): цвет * 2
	SHT = 0142200			/ сдвиг спрайта на s точек (1..3), центры таблиц
					/ (индекс — байт со знаком): SHT + (s-1)*01000 —
					/ (x << 2s) & 0377, ещё +0400 — x >> (8 - 2s)
	SHT0 = 0145200			/ то же для s = 0
	OCC = 0146000			/ строка «заслонено» (spr_blitm), 2 бита на точку
	ZOCC = 0146200			/ 66 нулей: строка без маски
	EXP = 0146400			/ 16 байт: 4 бита (первая точка — бит 3) -> 2 бита на точку
	MR = S+040			/ spr_blitm: байт маски первой колонки строки
	MN = S+042			/ байт маски на строку
	MB = S+044			/ бит первой колонки в байте
	MS = S+046			/ шаг строки маски
	SK = S+050			/ пропуск в строке спрайта
	NBV = S+052			/ байт цели в строке
	BEQ = 001400
	BNE = 001000
	MKN = S+032			/ записей в STAB/MTAB (mkmul)
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
	mov	@$DSTRIDE, ffb_st+2
	mov	@$BMOFF, ffb_m+4
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
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ быстрый режим (все строки в цели): r3 — указатель RLE, r4 — серия, r5 — строк до
/ конца колонки, r1 — байт цели, CX — цвет серии * 2. Длина и цвет серии — из
/ таблиц по байту RLE (mkdec), пары точек — из TBL (mktbl); y не считается.
/ Чётность строки начала серии — (VYH + r5) & 1: ветвление по r5 заплатано под VYH;
/ цикл точек (ffu, ffm, ffb) — заплатой перехода
fast:	jsr	pc, mktbl
	jsr	pc, mkmul
	jsr	pc, mkdec
	mov	$ffu, fr_jmp+2
	tst	@$ZBUF
	beq	1f
	mov	$ffm, fr_jmp+2
1:	tst	@$BMOFF
	beq	1f
	mov	$ffb, fr_jmp+2
1:	mov	@$VY, r0
	add	@$H, r0
	mov	$BEQ+1, fr_par		/ beq через swab: нечётный r5 — swab
	bit	$1, r0
	beq	1f
	mov	$BNE+1, fr_par
1:	mov	@$H, r5
	jsr	pc, colset
	mov	@$SRC, r3
	mov	@$COLOR, r0
	asl	r0
	mov	r0, @$CX
	mov	@$B+2, r4		/ незаконченная серия (с уже пройденной точкой)
	beq	fnew
	dec	r4
	bne	frun
fnew:	movb	(r3)+, r2
	asl	r2
	mov	LENT(r2), r4
	beq	fext
	mov	COLT(r2), @$CX
/ r4 (> 0) точек серии цвета CX, r5 (> 0) строк до конца колонки
frun:	mov	@$CX, r0
	beq	fskip
	tstb	@$OK
	beq	fskip
fr_tb:	mov	0(r0), r0		/ COLTB (colset): VE | VO << 8
	bit	$1, r5
fr_par:	beq	1f
	swab	r0
1:	cmp	r4, r5
	bhis	fclip
	sub	r4, r5
fr_jmp:	jmp	@$ffu
fclip:	sub	r5, r4			/ серия дойдёт до конца колонки: остаток — на стек
	mov	r4, -(sp)
	mov	r5, r4
	clr	r5
	br	fr_jmp
fext:	mov	COLT(r2), @$CX		/ длина — в следующем байте (0 — 256)
	clr	r4
	bisb	(r3)+, r4
	bne	frun
	mov	$0400, r4
	br	frun
/ прозрачная серия или невидимая колонка: указатели — на n строк (таблицы); до
/ конца колонки — не нужны (colset считает их заново)
fskip:	cmp	r4, r5
	bhis	fskc
	sub	r4, r5
	tstb	@$OK
	beq	fnew
	asl	r4
	add	STAB(r4), r1
	tst	@$MSTEP
	beq	fnew
	add	MTAB(r4), @$MPTR
	br	fnew
fskc:	sub	r5, r4
	br	fcol
ffu:	movb	(r1), r2
ffu_nk:	bicb	$0, r2
	bisb	r0, r2
	movb	r2, (r1)
ffu_st:	add	$0, r1
	swab	r0
	sob	r4, ffu
ffend:	tst	r5
	bne	fnew
	mov	(sp)+, r4
/ конец колонки: r4 — остаток серии
fcol:	dec	@$SKIP
	beq	fdone
	mov	@$H, r5
	add	@$STEP, @$X
	cmp	@$X, $OUT_W
	bhis	fdone
	jsr	pc, colset
	tst	r4
	beq	fnew
	br	frun
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
	br	ffend
/ постройка спрайта (буфер в нулях, каждая точка — раз): цвет и маска точки
ffb:	bisb	r0, (r1)
ffb_m:	bisb	$0, 0(r1)		/ NK, BMOFF
ffb_st:	add	$0, r1
	swab	r0
	sob	r4, ffb
	br	ffend
fdone:	mov	r3, @$SRC		/ конец прочитанного RLE (costume_r.c: длина кадра)
	jmp	done

/ таблицы разбора байта RLE (раз): LENT — длина (0 — в следующем байте), COLT —
/ цвет * 2; индекс — байт со знаком * 2; и таблицы сдвига SHT. Признак — DTOK;
/ портит r0..r5
mkdec:	cmp	@$DTOK, $DTMAGIC
	bne	1f
	rts	pc
1:	clr	r0
2:	movb	r0, r1			/ байт со знаком
	asl	r1
	mov	r0, r2
	bic	$0177760, r2
	mov	r2, LENT(r1)
	mov	r0, r2
	asr	r2
	asr	r2
	asr	r2
	bic	$0177741, r2		/ (байт >> 4) * 2
	mov	r2, COLT(r1)
	inc	r0
	cmp	r0, $0400
	blo	2b
	clr	r3			/ таблицы сдвига: s = 0 (SHT0), 1..3 (SHT)
3:	mov	r3, r4
	bne	31f
	mov	$SHT0, r4
	br	32f
31:	dec	r4
	swab	r4
	asl	r4
	add	$SHT, r4
32:	clr	r0
4:	movb	r0, r1
	add	r4, r1
	mov	r0, r2
	mov	r3, r5
	beq	51f
5:	asl	r2
	asl	r2
	sob	r5, 5b			/ x << 2s
51:	movb	r2, (r1)
	swab	r2
	movb	r2, 0400(r1)		/ x >> (8 - 2s)
	inc	r0
	cmp	r0, $0400
	blo	4b
	inc	r3
	cmp	r3, $4
	blo	3b
	mov	$ZOCC, r0
	mov	$041, r1
6:	clr	(r0)+
	sob	r1, 6b
	clr	r0			/ EXP
7:	clr	r2
	bit	$010, r0
	beq	1f
	bis	$3, r2
1:	bit	$4, r0
	beq	1f
	bis	$014, r2
1:	bit	$2, r0
	beq	1f
	bis	$060, r2
1:	bit	$1, r0
	beq	1f
	bis	$0300, r2
1:	movb	r2, EXP(r0)
	inc	r0
	cmp	r0, $020
	blo	7b
	mov	$DTMAGIC, @$DTOK
	rts	pc

/ таблицы STAB, MTAB (n * шаг) для n = 0..H — если шаги сменились или их мало
/ (шаг холста у каждого актёра свой: все 256 записей — ~10 мс); портит r0..r5
mkmul:	mov	@$H, r0
	inc	r0
	cmp	r0, $0400
	blos	4f
	mov	$0400, r0
4:	cmp	@$STAB+2, @$DSTRIDE
	bne	1f
	cmp	@$MTAB+2, @$MSTRIDE
	bne	1f
	tst	@$STAB
	bne	1f
	cmp	@$MKN, r0
	blo	1f
	rts	pc
1:	mov	r0, @$MKN
	mov	r0, @$R4S
	mov	@$DSTRIDE, r4
	mov	@$MSTRIDE, r5
	clr	r0
	clr	r1
	mov	$STAB, r2
	mov	$MTAB, r3
2:	mov	r0, (r2)+
	mov	r1, (r3)+
	add	r4, r0
	add	r5, r1
	dec	@$R4S
	bne	2b
	rts	pc

/ таблица TBL для CPAT: [k 0..3][bx & 1][цвет 0..15] -> VE | VO << 8 (как runset),
/ если CPAT сменилась с прошлого раза (копия — TBLC; сверка — раз на актёра,
/ по NEWPAL); портит r0..r5
mktbl:	tst	@$NEWPAL
	bne	4f
	rts	pc
4:	clr	@$NEWPAL
	mov	$CPAT, r0
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
	mov	$6, @$0177130
	mov	r0, @$0177130
	mov	$0, @$0177130
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
	mov	r1, -(sp)		/ COLTB = TBL + (2k + ((bx ^ SHP) & 1)) * 32
	asl	(sp)
	mov	@$SHP, -(sp)
	xor	r2, (sp)
	bit	$1, (sp)+
	beq	1f
	inc	(sp)
1:	asl	(sp)
	asl	(sp)
	asl	(sp)
	asl	(sp)
	asl	(sp)
	add	$TBL, (sp)
	mov	(sp)+, fr_tb+2
	movb	pixmask(r1), fu_nk+2
	movb	pixmask(r1), fm_nk+2
	movb	pixmask(r1), ffu_nk+2
	movb	pixmask(r1), ffm_nk+2
	movb	pixmask(r1), ffb_m+2
	asl	r1
	asl	r1
	mov	r1, @$K4
	mov	@$SHP, r1
	xor	r2, r1
	bic	$0177776, r1
	movb	r1, @$ODD		/ чётность узора
	asr	r2
	asr	r2			/ bx >> 2
	mov	@$DROW0, r1
	add	r2, r1			/ байт цели
	tst	@$ZBUF
	beq	7f
	add	@$MXOFF, r2		/ байт фона под точкой
	mov	r2, r0
	asl	r0
	asl	r0
	add	r2, r0
	mov	@$K4, r2
	asr	r2
	asr	r2
	add	r2, r0			/ колонка мира фона под точкой: 5 * байт + k
	mov	r0, r2
	asr	r2
	asr	r2
	asr	r2
	add	@$MROW0, r2		/ байт маски: строка VY, полоса / 8
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
	mov	$6, @$0177130
	mov	r0, @$0177130
	mov	$0, @$0177130
	rts	pc

/ вернуть страницу вызывающего
back:
	mov	$6, @$0177130
	mov	_cur_page, @$0177130
	mov	$0, @$0177130
	rts	pc

/ Буфер холста: строки по ncols + pad байт. Строки копирует rcopy.
/ void cv_load(uint16_t cv, uint16_t vram, uint16_t ncols, uint16_t nrows, uint16_t pad):
/ экран (строки по 64 байта) -> буфер в PG_MASK
_cv_load:
	jsr	pc, cvarg
	mov	014(sp), r0		/ экран
	mov	012(sp), r1		/ буфер
	mov	$0100, r4
	mov	r3, r5
	add	022(sp), r5
	br	cvcp

/ void cv_store(uint16_t cv, uint16_t vram, uint16_t ncols, uint16_t nrows, uint16_t pad):
/ буфер -> экран
_cv_store:
	jsr	pc, cvarg
	mov	012(sp), r0
	mov	014(sp), r1
	mov	r3, r4
	add	022(sp), r4
	mov	$0100, r5
cvcp:	jsr	pc, rcopy
cvend:	jsr	pc, back
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ cv_load, cv_store: регистры — на стек (под адрес возврата вызывающего), PG_MASK,
/ r2 — строк, r3 — байт в строке
cvarg:	mov	(sp), r0
	mov	r2, (sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	r0, -(sp)
	mov	$PG_MASK, r0
	jsr	pc, mapr0
	mov	022(sp), r2
	mov	020(sp), r3
	rts	pc

/ void spr_blit(uint16_t src, uint16_t msk, uint16_t w, uint16_t h, uint16_t dst,
/ uint16_t dskip, uint16_t sskip): спрайт кэша (окно 040000, плоскости цвета и
/ маски) -> холст в PG_MASK: байт = (байт & ~маска) | цвет
_spr_blit:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	jsr	pc, mapmask
	mov	012(sp), r0
	mov	014(sp), r3
	mov	020(sp), r5
	mov	022(sp), r1
1:	mov	016(sp), r4
2:	movb	(r1), r2
	bicb	(r3)+, r2
	bisb	(r0)+, r2
	movb	r2, (r1)+
	sob	r4, 2b
	add	024(sp), r1
	add	026(sp), r0
	add	026(sp), r3
	sob	r5, 1b
	br	cvend

/ void spr_blits(uint16_t src, uint16_t ps, uint16_t w1, uint16_t h, uint16_t dst,
/ uint16_t dskip, uint16_t s): то же со сдвигом на s точек (1..3) вправо: байт цели
/ m (0..w1-1) — (src[m] << 2s) | (src[m-1] >> (8-2s)). Строки спрайта по w1 байт
/ с нулевым впереди, за последней — ноль; src — на первом байте данных, маска — +ps
_spr_blits:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	jsr	pc, mapmask		/ дальше — копия кода в странице маски: заплаты в ней
	jsr	pc, mkdec
	mov	026(sp), r0
	dec	r0
	swab	r0
	asl	r0			/ (s-1) * 01000
	add	$SHT, r0
	mov	r0, sl_lt1+2
	mov	r0, sl_lt2+2
	add	$0400, r0
	mov	r0, sl_rt1+2
	mov	r0, sl_rt2+2
	mov	014(sp), r0
	mov	r0, sl_pc+2
	dec	r0
	mov	r0, sl_pp+2
	mov	012(sp), r0
	mov	020(sp), r5
	mov	022(sp), r1
1:	mov	016(sp), r4
2:
sl_pp:	movb	0(r0), r2		/ маска src[m-1]
sl_rt1:	movb	0(r2), r2
sl_pc:	movb	0(r0), r3		/ маска src[m]
sl_lt1:	bisb	0(r3), r2
	bicb	r2, (r1)
	movb	-1(r0), r2		/ цвет
sl_rt2:	movb	0(r2), r2
	movb	(r0)+, r3
sl_lt2:	bisb	0(r3), r2
	bisb	r2, (r1)+
	sob	r4, 2b
	add	024(sp), r1
	sob	r5, 1b
	jmp	cvend

/ void spr_blitm(uint16_t src, uint16_t ps, uint16_t w1, uint16_t nb, uint16_t h,
/ uint16_t dst, uint16_t dskip, uint16_t s, uint16_t mrow, uint16_t cbit,
/ uint16_t mstride): как spr_blits (s = 0..3, nb байт цели в строке) с маской
/ глубины: точка заслонена, если в маске (строки с mrow, шаг mstride) бит колонки
/ фона под ней; cbit — колонка фона первой точки (5 * байт). Строка без маски под
/ ней — с нулями ZOCC, иначе «заслонено» собирается в OCC по битам
_spr_blitm:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	jsr	pc, mapmask
	jsr	pc, mkdec
	mov	030(sp), r0		/ s
	bne	1f
	mov	$SHT0, r0
	br	2f
1:	dec	r0
	swab	r0
	asl	r0
	add	$SHT, r0
2:	mov	r0, ml_lt1+2
	mov	r0, ml_lt2+2
	add	$0400, r0
	mov	r0, ml_rt1+2
	mov	r0, ml_rt2+2
	mov	014(sp), r0
	mov	r0, ml_pc+2
	dec	r0
	mov	r0, ml_pp+2
	mov	016(sp), r0
	sub	020(sp), r0
	mov	r0, @$SK
	mov	020(sp), @$NBV
	mov	036(sp), @$MS
	mov	034(sp), r0		/ cbit
	mov	r0, r1
	bic	$0177770, r1
	mov	r1, @$MB
	asr	r0
	asr	r0
	asr	r0
	mov	r0, r3			/ байт маски первой колонки
	add	032(sp), r0
	mov	r0, @$MR
	mov	020(sp), r1		/ последняя колонка: cbit + 5 * nb - 1
	mov	r1, r2
	asl	r2
	asl	r2
	add	r1, r2
	add	034(sp), r2
	dec	r2
	asr	r2
	asr	r2
	asr	r2
	sub	r3, r2
	inc	r2
	mov	r2, @$MN
	mov	012(sp), r0
	mov	022(sp), r5
	mov	024(sp), r1
/ строка: «заслонено» — в OCC или ZOCC, смещение от начала строки цели — в команды
mrow:	mov	r0, -(sp)
	mov	r1, -(sp)
	mov	r5, -(sp)
	mov	@$MR, r1
	mov	@$MN, r2
3:	tstb	(r1)+
	bne	4f
	sob	r2, 3b
	mov	$ZOCC, r0
	jmp	mocc
4:	mov	@$MR, r1
	movb	(r1)+, r2
	mov	$010, r3
	mov	@$MB, r0
	beq	5f
6:	aslb	r2			/ к первой колонке
	dec	r3
	sob	r0, 6b
5:	mov	$OCC, r5
	mov	@$NBV, r4
7:	clr	r0			/ байт цели: колонки фона 5c..5c+3, 5c+4 — мимо
	aslb	r2
	rol	r0
	dec	r3
	bne	1f
	movb	(r1)+, r2
	mov	$010, r3
1:	aslb	r2
	rol	r0
	dec	r3
	bne	1f
	movb	(r1)+, r2
	mov	$010, r3
1:	aslb	r2
	rol	r0
	dec	r3
	bne	1f
	movb	(r1)+, r2
	mov	$010, r3
1:	aslb	r2
	rol	r0
	dec	r3
	bne	1f
	movb	(r1)+, r2
	mov	$010, r3
1:	aslb	r2
	dec	r3
	bne	1f
	movb	(r1)+, r2
	mov	$010, r3
1:	movb	EXP(r0), (r5)+
	sob	r4, 7b
	mov	$OCC, r0
mocc:	sub	2(sp), r0		/ - начало строки цели
	mov	r0, ml_oc+2
	mov	r0, ml_oc2+2
	mov	(sp)+, r5
	mov	(sp)+, r1
	mov	(sp)+, r0
	mov	@$NBV, r4
2:
ml_pp:	movb	0(r0), r2		/ маска src[m-1]
ml_rt1:	movb	0(r2), r2
ml_pc:	movb	0(r0), r3		/ маска src[m]
ml_lt1:	bisb	0(r3), r2
ml_oc:	bicb	0(r1), r2		/ без заслонённых
	bicb	r2, (r1)
	movb	-1(r0), r2		/ цвет
ml_rt2:	movb	0(r2), r2
	movb	(r0)+, r3
ml_lt2:	bisb	0(r3), r2
ml_oc2:	bicb	0(r1), r2
	bisb	r2, (r1)+
	sob	r4, 2b
	add	026(sp), r1
	add	@$SK, r0
	add	@$MS, @$MR
	dec	r5
	beq	8f
	jmp	mrow
8:	jmp	cvend

/ void cv_bg(uint16_t cv, uint16_t cvstride, uint16_t bm, uint16_t bmstride,
/ uint16_t ncols, uint16_t nrows): фон PG_BITMAP -> буфер через _bounce пачками
/ строк (до 256 байт): страница — раз на пачку. Шаг строк в _bounce — той же
/ чётности, что у фона, и первая строка — с той же чётностью: первый перенос
/ всегда словами
_cv_bg:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	022(sp), r3		/ ncols
	mov	020(sp), r4
	xor	r3, r4
	bic	$0177776, r4
	add	r3, r4			/ шаг в _bounce: ncols той же чётности, что шаг фона
	mov	r4, -(sp)		/ (sp) — шаг в _bounce, 2(sp) — строк в пачке
	mov	$0377, r0		/ 0400 байт — с запасом на байт выравнивания
	clr	-(sp)
1:	sub	r4, r0
	bcs	2f
	inc	(sp)
	br	1b
2:	mov	(sp)+, r2
	mov	r2, -(sp)		/ строк в пачке
	tst	030(sp)
	beq	9f
3:	mov	(sp), r2		/ пачка: min(строк в пачке, осталось)
	cmp	r2, 030(sp)
	blos	4f
	mov	030(sp), r2
4:	sub	r2, 030(sp)
	mov	r2, -(sp)		/ (sp) — строк в пачке, 2 — пачка, 4 — шаг
	mov	$PG_BITMAP, r0
	jsr	pc, mapr0
	mov	024(sp), r0		/ фон
	mov	r0, r1
	bic	$0177776, r1
	add	$_bounce, r1
	mov	026(sp), r4
	mov	4(sp), r5
	mov	030(sp), r3
	jsr	pc, rcopy
	mov	r0, 024(sp)		/ следующая строка фона
	mov	$PG_MASK, r0
	jsr	pc, mapr0
	mov	024(sp), r0
	mov	026(sp), r4
	mov	(sp), r2
5:	sub	r4, r0			/ начало пачки фона — его чётность
	sob	r2, 5b
	bic	$0177776, r0
	add	$_bounce, r0
	mov	020(sp), r1		/ буфер
	mov	(sp)+, r2
	mov	2(sp), r4		/ шаг в _bounce
	mov	020(sp), r5		/ шаг буфера
	mov	026(sp), r3
	jsr	pc, rcopy
	mov	r1, 016(sp)		/ следующая строка буфера
	tst	030(sp)
	bne	3b
9:	cmp	(sp)+, (sp)+
	jmp	cvend

/ rcopy: r2 (> 0) строк по r3 (1..64) байт из r0 (шаг строк r4) в r1 (шаг r5); на
/ выходе r0, r1 — начала следующих строк. Строка — развёрнутой цепочкой, как в
/ bm_blit: при чётных адресах — словами (нечётный хвост — байтом), при обоих
/ нечётных — байт и словами, при разной чётности — байтами. Портит r2..r5
rcopy:	tst	r2
	bne	10f
	rts	pc
10:	sub	r3, r4			/ шаги — за вычетом строки
	sub	r3, r5
	mov	r3, -(sp)		/ 010(sp): вход в цепочку байтов
	asl	(sp)
	neg	(sp)
	add	$rbend, (sp)
	mov	r3, -(sp)		/ 6(sp): байт в конце после нечётного начала
	dec	(sp)
	bic	$0177776, (sp)
	mov	r3, -(sp)		/ 4(sp): вход в слова после нечётного начала
	dec	(sp)
	bic	$1, (sp)
	neg	(sp)
	add	$rwend, (sp)
	mov	r3, -(sp)		/ 2(sp): байт в конце при чётном начале
	bic	$0177776, (sp)
	bic	$1, r3			/ (sp): вход в слова при чётном начале
	neg	r3
	add	$rwend, r3
	mov	r3, -(sp)
1:	mov	r0, r3
	xor	r1, r3
	ror	r3
	bcs	3f			/ разная чётность
	bit	$1, r0
	bne	2f
	mov	2(sp), r3
	jmp	@0(sp)
2:	movb	(r0)+, (r1)+
	mov	6(sp), r3
	jmp	@4(sp)
3:	jmp	@010(sp)
	.rept	32
	mov	(r0)+, (r1)+
	.endr
rwend:	tst	r3
	beq	rbend
	movb	(r0)+, (r1)+
	br	rbend
	.rept	64
	movb	(r0)+, (r1)+
	.endr
rbend:	add	r4, r0
	add	r5, r1
	dec	r2
	beq	5f
	jmp	1b			/ BR не дотянется через цепочки
5:	add	$012, sp
	rts	pc
