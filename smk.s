/ Доступ к чужим страницам СМК (общая копия кода, видна в любой странице).
/ Каждая функция включает страницу данных и перед возвратом — страницу вызывающего
/ (_cur_page). Адреса — адреса окна 0100000..0157777. Перенос между страницами —
/ через буфер _bounce в базовом ОЗУ (sys.ld).

	.globl _far_rb, _far_rw, _far_rd, _far_wr, _far_cp, _far_fill, _cur_page, _bounce
	.globl _far_wb, pcode, _snd_tick, _far_wr2d, _far_rd2d, _sndst
	.text

/ включить страницу r0 (0..15) в ОЗУ10, не трогая _cur_page; портит r0
mapr0:
	bic	$0177760, r0
	asl	r0
	mov	pcode(r0), r0
	bis	$0120, r0
	mov	$6, @$0177130
	mov	r0, @$0177130
	mov	$0, @$0177130
	mov	r0, @$_map_now
	rts	pc

/ вернуть страницу вызывающего
back:					/ r0 — результат, r1 свободен
	mov	@$0177710, r1		/ срок snd_tick (DL) не наступил — без вызова
	sub	@$_sndst+022, r1
	dec	r1
	cmp	r1, @$_snd_tk
	blo	1f
	jsr	pc, _snd_tick
1:
	mov	$6, @$0177130
	mov	_cur_page, @$0177130
	mov	$0, @$0177130
	mov	_cur_page, @$_map_now
	rts	pc

/ uint8_t far_rb(uint16_t page, uint16_t addr)
_far_rb:
	mov	2(sp), r0
	jsr	pc, mapr0
	clr	r0
	bisb	@4(sp), r0
	br	back

/ uint16_t far_rw(uint16_t page, uint16_t addr): слово LE с любого адреса
_far_rw:
	mov	2(sp), r0
	jsr	pc, mapr0
	mov	4(sp), r1
	movb	1(r1), r0
	swab	r0
	clrb	r0
	bisb	(r1), r0
	br	back

/ void far_wb(uint16_t page, uint16_t addr, uint16_t v)
_far_wb:
	mov	2(sp), r0
	jsr	pc, mapr0
	movb	6(sp), @4(sp)
	br	back

/ копия r2 байт из r1 в r3 (словами, если оба адреса чётные); портит r0..r3
cpy:
	tst	r2
	beq	3f
	mov	r1, r0
	bis	r3, r0
	bit	$1, r0
	bne	2f
	mov	r2, r0
	asr	r0
	beq	2f
1:	mov	(r1)+, (r3)+
	sob	r0, 1b
	bit	$1, r2
	beq	3f
	movb	(r1)+, (r3)+
	rts	pc
2:	movb	(r1)+, (r3)+
	sob	r2, 2b
3:	rts	pc

/ void far_rd(void *dst, uint16_t page, uint16_t addr, uint16_t n): страница -> базовое ОЗУ
_far_rd:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	010(sp), r0
	jsr	pc, mapr0
	mov	6(sp), r3
	mov	012(sp), r1
	mov	014(sp), r2
	jsr	pc, cpy
	jsr	pc, back
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ void far_wr(uint16_t page, uint16_t addr, const void *src, uint16_t n): базовое ОЗУ -> страница
_far_wr:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	6(sp), r0
	jsr	pc, mapr0
	mov	010(sp), r3
	mov	012(sp), r1
	mov	014(sp), r2
	jsr	pc, cpy
	jsr	pc, back
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ void far_cp(uint16_t dpage, uint16_t daddr, uint16_t spage, uint16_t saddr, uint16_t n):
/ страница -> страница кусками через _bounce (BOUNCE_N байт)
BOUNCE_N = 0400
_far_cp:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	014(sp), r4		/ daddr
	mov	020(sp), r5		/ saddr
1:	mov	022(sp), r2		/ осталось
	beq	9f
	cmp	r2, $BOUNCE_N
	blos	2f
	mov	$BOUNCE_N, r2
2:	sub	r2, 022(sp)
	mov	r2, -(sp)		/ длина куска
	mov	020(sp), r0		/ spage (+2: длина на стеке)
	jsr	pc, mapr0
	mov	r5, r1
	mov	$_bounce, r3
	mov	(sp), r2
	jsr	pc, cpy
	mov	r1, r5
	mov	014(sp), r0		/ dpage
	jsr	pc, mapr0
	mov	$_bounce, r1
	mov	r4, r3
	mov	(sp)+, r2
	jsr	pc, cpy
	mov	r3, r4
	br	1b
9:	jsr	pc, back
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ прямоугольник w x h байт: базовое ОЗУ (buf, строки по bstride) <-> страница
/ (addr, строки по pstride); страница включается один раз
/ void far_wr2d(page, addr, pstride, const void *buf, bstride, w, h)
/ void far_rd2d(page, addr, pstride, void *buf, bstride, w, h)
_far_wr2d:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r0
	jsr	pc, mapr0
	mov	020(sp), r1		/ buf
	mov	014(sp), r3		/ addr
1:	mov	r1, r4
	mov	r3, r5
	mov	024(sp), r2		/ w
2:	movb	(r4)+, (r5)+
	sob	r2, 2b
	add	022(sp), r1
	add	016(sp), r3
	dec	026(sp)
	bne	1b
	br	9f
_far_rd2d:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r0
	jsr	pc, mapr0
	mov	020(sp), r1		/ buf
	mov	014(sp), r3		/ addr
1:	mov	r3, r4
	mov	r1, r5
	mov	024(sp), r2		/ w
2:	movb	(r4)+, (r5)+
	sob	r2, 2b
	add	022(sp), r1
	add	016(sp), r3
	dec	026(sp)
	bne	1b
9:	jsr	pc, back
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ void far_fill(uint16_t page, uint16_t addr, uint16_t byte, uint16_t n)
_far_fill:
	mov	2(sp), r0
	jsr	pc, mapr0
	mov	4(sp), r1
	mov	010(sp), r0
	beq	2f
1:	movb	6(sp), (r1)+
	bit	$0377, r0		/ фронт тона каждые 256 байт
	bne	3f
	jsr	pc, _snd_tick
3:	sob	r0, 1b
2:	jmp	back			/ BR сюда не достаёт

/ Поток RLE v1 (gfx_v1.c struct rle: 0 page, 2 p, 4 common[4], 010 n, 011 rep,
/ 012 val): 1ccnnnnn — общий цвет cc n+1 раз, 01nnnnnn b — b n+1 раз, 00nnnnnn —
/ n+1 байт как есть. Страница потока включена на весь вызов; серия — кусками до
/ 32 байт, между ними — проверка срока snd_tick (он сохраняет регистры и раскладку).
/ void rle_fill(struct rle *r, uint8_t *dst, uint16_t n, uint16_t stride): n байт
/ в базовое ОЗУ через stride; dst == 0 — пропуск (серии повтора — без перебора)
	.globl	_rle_fill
RLE_N = 010
RLE_REP = 011
RLE_VAL = 012
_rle_fill:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r5		/ r
	mov	014(sp), r3		/ dst
	mov	016(sp), r2		/ n
	mov	020(sp), r4		/ stride
	clr	r0
	bisb	(r5), r0
	jsr	pc, mapr0
	mov	2(r5), r1		/ p
	clr	-(sp)			/ остаток серии
	bisb	RLE_N(r5), (sp)
	tst	r2
	beq	9f
1:	mov	@$0177710, r0		/ срок snd_tick
	sub	@$_sndst+022, r0
	dec	r0
	cmp	r0, @$_snd_tk
	blo	2f
	jsr	pc, _snd_tick
2:	tst	(sp)
	bne	5f
	clr	r0			/ заголовок серии
	bisb	(r1)+, r0
	bit	$0200, r0
	beq	3f
	mov	r0, (sp)		/ общий цвет
	bic	$0177740, (sp)
	asl	r0
	asl	r0
	asl	r0
	swab	r0
	bic	$0177774, r0
	add	r5, r0
	movb	4(r0), RLE_VAL(r5)
	br	4f
3:	bit	$0100, r0
	beq	31f
	bic	$0177700, r0		/ байт повтора
	mov	r0, (sp)
	movb	(r1)+, RLE_VAL(r5)
4:	movb	$1, RLE_REP(r5)
	inc	(sp)
	br	5f
31:	inc	r0			/ байты как есть
	mov	r0, (sp)
	clrb	RLE_REP(r5)
5:	mov	(sp), r0		/ кусок: min(серия, n, 32)
	cmp	r0, r2
	blos	6f
	mov	r2, r0
6:	cmp	r0, $040
	blos	7f
	mov	$040, r0
7:	sub	r0, (sp)
	sub	r0, r2
	tstb	RLE_REP(r5)
	beq	71f
	tst	r3
	beq	8f
72:	movb	RLE_VAL(r5), (r3)
	add	r4, r3
	sob	r0, 72b
	br	8f
71:	tst	r3
	bne	73f
	add	r0, r1			/ пропуск байтов как есть
	br	8f
73:	movb	(r1)+, (r3)
	add	r4, r3
	sob	r0, 73b
8:	tst	r2
	bne	1b
9:	movb	(sp)+, RLE_N(r5)
	mov	r1, 2(r5)
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	jmp	back

/ коды страниц 0..15 (своя копия: общая копия не ссылается на ядро)
pcode:
	.word	0, 02000, 04, 02004, 010, 02010, 014, 02014
	.word	01, 02001, 05, 02005, 011, 02011, 015, 02015
