/ proc3 ClassicCostumeRenderer (costume.cpp) для БК (общая копия кода): точки
/ кадра актёра по колонкам прямо в видеопамять. Колонка мира x%5 == 4 выброшена
/ (сжатие 4:5), z-маска читается в её странице (PG_MASK подключается на время
/ работы). Параметры — struct P3 в _bounce (costume_r.c).
	.globl _proc3_run, _bounce, _cur_page, _div5, pcode
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
	POLL = B+032
	CPAT = B+034
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

	OUT_W = 320
	OUT_H = 128
	VIEW_ROW = 44
	PG_MASK = 5

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
	mov	@$VY, r3
	mov	@$H, r5
	jsr	pc, colset
	mov	@$B+2, r4		/ незаконченная серия: вход в StartPos
	bne	cont
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
	tst	@$MSTEP
	bne	mloop
uloop:	cmp	r3, $OUT_H		/ y без знака: < 0 тоже мимо
	bhis	1f
	movb	(r1), r2		/ видеопамять: одно чтение, одна запись
	bicb	@$NK, r2
	bisb	r0, r2
	movb	r2, (r1)
1:	add	$0100, r1
	inc	r3
	swab	r0
	dec	r5
	beq	colend
	dec	r4
	bne	uloop
	br	newrun
mloop:	cmp	r3, $OUT_H
	bhis	1f
	bitb	@$MBIT, @MPTR
	bne	1f
	movb	(r1), r2
	bicb	@$NK, r2
	bisb	r0, r2
	movb	r2, (r1)
1:	add	$0100, r1
	add	@$MSTEP, @$MPTR
	inc	r3
	swab	r0
	dec	r5
	beq	colend
	dec	r4
	bne	mloop
	br	newrun
/ прозрачная серия или невидимая колонка — целиком
skip:	cmp	r4, r5
	bhis	2f
	sub	r4, r5			/ серия кончается в колонке
	add	r4, r3
	mov	r4, r0
	asl	r0
	asl	r0
	asl	r0
	asl	r0
	asl	r0
	asl	r0
	add	r0, r1
	tst	@$MSTEP
	beq	newrun
	mov	r4, r0
3:	add	@$MSTEP, @$MPTR
	sob	r0, 3b
	br	newrun
2:	sub	r5, r4			/ до конца колонки: как r5 шагов цикла
	inc	r4
colend:	dec	@$SKIP			/ конец колонки
	beq	done
	tst	@$POLL			/ тон между колонками (snd_poll зовёт банк C —
	beq	3f			/ потом снова страница маски)
	jsr	pc, @POLL
	jsr	pc, mapmask
3:	mov	@$H, r5
	mov	@$VY, r3
	add	@$STEP, @$X
	cmp	@$X, $OUT_W
	bhis	done
	jsr	pc, colset
cont:	dec	r4
	bne	dorun
	jmp	newrun
done:	mov	$6, @$0177130
	mov	_cur_page, @$0177130
	mov	$0, @$0177130
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ подключить страницу маски; портит r0
mapmask:
	mov	@$pcode+2*PG_MASK, r0
	bis	$0120, r0
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
	cmp	r2, $0400
	bhis	8f
	mov	r2, r1
	bic	$0177774, r1		/ k
	movb	pixmask(r1), @$NK
	asl	r1
	asl	r1
	mov	r1, @$K4
	movb	r2, r1
	bic	$0177776, r1
	movb	r1, @$ODD
	asr	r2
	asr	r2			/ bx >> 2
	mov	@$VY, r1
	add	$VIEW_ROW, r1
	asl	r1
	asl	r1
	asl	r1
	asl	r1
	asl	r1
	asl	r1
	add	$040000, r1
	add	r2, r1			/ байт видеопамяти
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
