/ Межбанковый вызов. Заглушка функции чужого банка (far_stubs.s, генерирует
/ tools/gen_far.py): jsr r1, farcall1; .word (страница<<8)|(номер*2) — 6 байт вместо 8
/ у mov/jmp. Адрес возврата и страница вызывающего уходят на дальний стек в базовом ОЗУ,
/ стек Си не меняется: вызываемая функция видит свои аргументы на обычных местах. r1 в
/ ABI gcc — рабочий.
	.include "smk.inc"
	.globl farcall, farcall1, _far_sp, _cur_page
	.text
farcall1:
	mov	(r1), r1		/ код из заглушки; в стеке — r1, положенный jsr
	tst	(sp)+
farcall:
	mov	_far_sp, r0
	mov	(sp), -(r0)
	mov	_cur_page, -(r0)
	mov	r0, _far_sp
	mov	$farret, (sp)
	mov	r1, r0
	clrb	r0
	swab	r0
	asl	r0
	mov	pcode(r0), r0		/ режим СМК — в таблице (smk.inc)
	mov	r0, _cur_page
	SMKSET	r0
	bic	$0177400, r1
	jmp	@0100000(r1)		/ таблица переходов в начале банка

farret:					/ r0 — результат, r1 свободен
	mov	_far_sp, r1
	mov	(r1)+, _cur_page
	SMKSET	_cur_page
	mov	(r1)+, -(sp)
	mov	r1, _far_sp
	rts	pc

/ uint16_t azc(uint16_t fn, ...): функция fn модуля AZБК (az.h, таблица azhdr.s с
/ 040000). Модуль — в странице 6 ОЗУ 11М (код 7 в 0177716): она в окне 040000 на время
/ вызова, СМК не меняется. Адрес возврата и прежняя тень _mem11 — на дальнем стеке,
/ стек Си не меняется: модуль видит fn и аргументы на обычных местах. Тень _mem11 (её
/ адрес — в слове _p_mem11, sys.ld: общая копия линкуется раньше ядра) — первой:
/ кадровое прерывание возвращает окно по ней.
	.globl	_azc, _p_mem11
_azc:	mov	_far_sp, r0
	mov	(sp), -(r0)
	mov	@$_p_mem11, r1
	mov	(r1), -(r0)
	mov	r0, _far_sp
	mov	$azret, (sp)
	mov	(r1), r0
	bis	$070000, r0
	mov	r0, (r1)
	mov	r0, @$0177716
	mov	2(sp), r0
	asl	r0
	jmp	@040000(r0)
azret:	mov	r0, -(sp)		/ результат
	mov	_far_sp, r0
	mov	@$_p_mem11, r1
	mov	(r0)+, (r1)
	mov	(r1), @$0177716
	mov	(r0)+, r1		/ адрес возврата
	mov	r0, _far_sp
	mov	(sp)+, r0
	jmp	(r1)

