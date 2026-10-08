/ Межбанковый вызов. Заглушка функции чужого банка (far_stubs.s, генерирует
/ tools/gen_far.py): mov $(страница<<8)|(номер*2), r1; jmp farcall. Адрес возврата
/ и страница вызывающего уходят на дальний стек в базовом ОЗУ, стек Си не меняется:
/ вызываемая функция видит свои аргументы на обычных местах. r1 в ABI gcc — рабочий.
	.globl farcall, _far_sp, _cur_page, _snd_tick, _sndst
	.text
farcall:				/ фронт фонового тона: Си-код между ассемблерными циклами
	mov	@$0177710, r0		/ срок snd_tick (DL) не наступил — без вызова
	sub	@$_sndst+022, r0
	dec	r0
	cmp	r0, $0143
	blo	1f
	jsr	pc, _snd_tick
1:	mov	_far_sp, r0
	mov	(sp), -(r0)
	mov	_cur_page, -(r0)
	mov	r0, _far_sp
	mov	$farret, (sp)
	mov	r1, r0
	clrb	r0
	swab	r0
	asl	r0
	mov	pcode(r0), r0
	bis	$0120, r0		/ ОЗУ10
	mov	r0, _cur_page
	mov	$6, @$0177130
	mov	r0, @$0177130
	mov	$0, @$0177130
	mov	r0, @$_map_now
	bic	$0177400, r1
	jmp	@0100000(r1)		/ таблица переходов в начале банка

farret:					/ r0 — результат, r1 свободен
	mov	@$0177710, r1		/ срок snd_tick (DL) не наступил — без вызова
	sub	@$_sndst+022, r1
	dec	r1
	cmp	r1, $0143
	blo	1f
	jsr	pc, _snd_tick
1:
	mov	_far_sp, r1
	mov	(r1)+, _cur_page
	mov	$6, @$0177130
	mov	_cur_page, @$0177130
	mov	$0, @$0177130
	mov	_cur_page, @$_map_now
	mov	(r1)+, -(sp)
	mov	r1, _far_sp
	rts	pc

/ коды страниц 0..15 (своя копия: общая копия не ссылается на ядро)
pcode:
	.word	0, 02000, 04, 02004, 010, 02010, 014, 02014
	.word	01, 02001, 05, 02005, 011, 02011, 015, 02015
