/ Межбанковый вызов. Заглушка функции чужого банка (far_stubs.s, генерирует
/ tools/gen_far.py): mov $(страница<<8)|(номер*2), r1; jmp farcall. Адрес возврата
/ и страница вызывающего уходят на дальний стек в базовом ОЗУ, стек Си не меняется:
/ вызываемая функция видит свои аргументы на обычных местах. r1 в ABI gcc — рабочий.
	.include "smk.inc"
	.globl farcall, _far_sp, _cur_page
	.text
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

