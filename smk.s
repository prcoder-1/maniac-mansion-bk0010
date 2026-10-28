/ Доступ к чужим страницам СМК (общая копия кода, видна в любой странице).
/ Каждая функция включает страницу данных и перед возвратом — страницу вызывающего
/ (_cur_page). Адреса — адреса окна 0100000..0157777. Перенос между страницами —
/ через буфер _bounce в базовом ОЗУ (sys.ld).

	.include "smk.inc"
	.globl _far_rb, _far_rw, _far_rd, _far_wr, _far_cp, _far_fill, _cur_page, _bounce
	.globl _far_wb, pcode, mapr0, back
	.text

/ включить страницу r0 (0..15), не трогая _cur_page; портит r0
mapr0:
	bic	$0177760, r0
	asl	r0
	mov	pcode(r0), r0
	SMKSET	r0
	rts	pc

/ вернуть страницу вызывающего
back:
	SMKSET	_cur_page
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

/ void far_fill(uint16_t page, uint16_t addr, uint16_t byte, uint16_t n)
_far_fill:
	mov	2(sp), r0
	jsr	pc, mapr0
	mov	4(sp), r1
	mov	010(sp), r0
	beq	2f
1:	movb	6(sp), (r1)+
	sob	r0, 1b
2:	jmp	back			/ BR сюда не достаёт

/ коды страниц 0..15 (своя копия: общая копия не ссылается на ядро)
pcode:	PCODES

	.text
/ void snap_save(void *beg, void *end): данные ядра — снимком в страницу SWAP (SNAP: адрес,
/ число слов, слова), метка SNAPM — последней; для перезапуска по «СТОП» (_rst, loader.s)
	.globl	_snap_save, _snap_rst
_snap_save:
	mov	$SWAP, r0
	jsr	pc, mapr0
	mov	2(sp), r0
	mov	4(sp), r1
	sub	r0, r1
	asr	r1
	mov	r0, @$SNAP
	mov	r1, @$SNAP+2
	mov	r2, -(sp)
	mov	$SNAP+4, r2
1:	mov	(r0)+, (r2)+
	sob	r1, 1b
	mov	(sp)+, r2
	mov	$SNAPMAGIC, @$SNAPM
	jmp	back

/ данные ядра — из снимка (страница SWAP подключена); портит r0..r2
_snap_rst:
	mov	@$SNAP, r1
	mov	@$SNAP+2, r2
	mov	$SNAP+4, r0
1:	mov	(r0)+, (r1)+
	sob	r2, 1b
	rts	pc
