/ Загрузчик файлов (EMT 36 под ANDOS) и переключение страниц СМК-512.
/
/ Лежит в 01000..02777 и не затирается: файл читается целиком в буфер 03200..067777
/ (ANDOS читает только в раскладке запуска, где 0120000..0157777 — его резидент),
/ а базовое ОЗУ 03200..037777 на это время уходит в страницу SWAP. Экран — в режиме
/ «малого экрана»: видна лишь 070000..077777, буфер не мелькает.
/
/ Игра идёт в режиме СМК SMK_MODE (smk.inc): страница видна целиком в 0100000..0176777.
/ Код режима+страницы для регистра 0177130 подключённой страницы — в _cur_page.

	.include "smk.inc"
SWAP      = 6			/ страница-хранилище базового ОЗУ
BUF       = 03200
BUF_END   = 070000
SAVE_END  = 040000

	.globl _ld_file, _smk_map, _smk_code, _page_code, _smk_home, _boot_psw
	.globl _cur_page, _vctl
	.text

/ void smk_map(uint16_t page): страница page (0..15)
_smk_map:
	mov	2(sp), r0
	asl	r0
	mov	_page_code(r0), r0
/ то же, что setcode, и отметка в _map_now (snd.s) — только вне чтения файла:
/ там _map_now лежит в буфере файла
setmap:
	mov	r0, @$_map_now
/ включить код r0 (режим|страница) и запомнить его в _cur_page; портит r0
setcode:
	mov	r0, _cur_page
	mov	$6, @$0177130		/ строб, код, снятие — только MOV
	mov	r0, @$0177130
	mov	$0, @$0177130
	rts	pc

/ void smk_code(uint16_t code): включить произвольный код раскладки
_smk_code:
	mov	2(sp), r0
	br	setmap

/ страница r0 (0..15); портит r0
mappg:
	asl	r0
	mov	_page_code(r0), r0
	br	setcode

/ int16_t ld_file(const struct ld_req *q): прочитать файл и разложить по страницам.
/ struct ld_req { name; flags; page; dest; split; ovf_page; ovf_dest } (smk.h).
/ Возвращает длину файла или -1 (ответ драйвера в ld_resp). Флаг 4 (LD_WRITE) —
/ запись: split байт из page:dest в файл.
_ld_file:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r1
	mov	(r1)+, r0		/ имя -> блок EMT 36, пробелами до 16 знаков
	mov	$blk+6, r2
	mov	$020, r3
1:	movb	(r0), r4
	beq	2f
	inc	r0
	br	3f
2:	mov	$040, r4
3:	movb	r4, (r2)+
	sob	r3, 1b
	mov	(r1)+, q_flags
	mov	(r1)+, q_page
	mov	(r1)+, q_dest
	mov	(r1)+, q_split
	mov	(r1)+, q_opage
	mov	(r1)+, q_odest
	mov	sp, q_sp
	mov	_cur_page, q_caller

	mtps	$0340
	mov	_vctl, r0		/ кадровое прерывание — выкл.: обработчик в затираемом ОЗУ
	bis	$040000, r0
	mov	r0, @$0177662
	mov	$SWAP, r0		/ базовое ОЗУ -> SWAP
	jsr	pc, mappg
	mov	$BUF, r1
	mov	$0100000, r2
	mov	$016300, r3		/ (SAVE_END-BUF)/2 слов
4:	mov	(r1)+, (r2)+
	sob	r3, 4b
	mov	$01000, sp		/ стек, как при запуске из ДОС (до копирования в BUF: старый — внутри него)
	bit	$4, q_flags		/ запись: split байт из page:dest -> BUF
	beq	81f
	mov	q_page, r0
	jsr	pc, mappg
	mov	q_dest, r1
	mov	$BUF, r2
	mov	q_split, r3
	jsr	pc, copy
81:
	mov	$q_vec, r0		/ векторы ДОС на время чтения: её драйвер НГМД сам
	mov	$vecs, r1		/ обрабатывает тайм-ауты шины по вектору 4, клавиатура —
1:	mov	(r1)+, r2		/ ПЗУ монитора
	beq	2f
	mov	(r2), (r0)+
	mov	2(r2), (r0)+
	mov	(r1)+, (r2)
	mov	(r1)+, 2(r2)
	br	1b
2:
	mov	@$0177706, q_tmr	/ таймер — ДОС (ей нужен для НГМД), наш — после
	mov	@$0177712, q_tmr+2
	mov	_boot_tmr, @$0177706
	mov	_boot_tmr+2, @$0177712
	mov	$0230, @$0177664	/ малый экран
	mov	_smk_home, r0
	jsr	pc, setcode
	mov	$3, blk			/ чтение; ответ 0
	mov	$BUF, blk+2
	clr	blk+4
	bit	$4, q_flags
	beq	82f
	mov	$2, blk			/ запись
	mov	q_split, blk+4
82:
	clr	blk+030
	mtps	_boot_psw
	mov	$blk, r1
	emt	036
	mtps	$0340
	mov	$q_vec, r0
	mov	$vecs, r1
1:	mov	(r1)+, r2
	beq	2f
	mov	(r0)+, (r2)
	mov	(r0)+, 2(r2)
	cmp	(r1)+, (r1)+
	br	1b
2:
	mov	q_tmr, @$0177706
	mov	q_tmr+2, @$0177712
	bis	$0100, @$0177660	/ ДОС могла разрешить прерывание клавиатуры
	tst	_kbd_irq_on		/ наше прерывание клавиатуры — снова
	beq	3f
	bic	$0100, @$0177660
3:
	movb	blk+1, _ld_resp
	mov	blk+030, r5		/ длина прочитанного
	tstb	_ld_resp
	beq	5f
	mov	$-1, r5
	br	9f
5:	bit	$4, q_flags
	beq	83f
	mov	q_split, r5		/ записано
	br	9f
83:	cmp	r5, $064600		/ BUF_END-BUF
	blos	6f
	mov	$-1, r5			/ не влез в буфер (затёрт экран)
	br	9f

6:	bit	$2, q_flags		/ во все страницы 2..15 (общая копия кода)
	beq	7f
	mov	$2, r4
61:	mov	r4, r0
	jsr	pc, mappg
	mov	r5, r3
	mov	$BUF, r1
	mov	q_dest, r2
	jsr	pc, copy
	inc	r4
	cmp	r4, $020
	blo	61b
	br	9f

7:	mov	q_page, r0
	jsr	pc, mappg
	mov	r5, r3			/ до split байт — в страницу
	cmp	r3, q_split
	blos	71f
	mov	q_split, r3
71:	mov	$BUF, r1
	mov	q_dest, r2
	mov	r3, r4
	jsr	pc, copy
	mov	r5, r3			/ остаток — в хвост (ovf_page:ovf_dest)
	sub	r4, r3
	beq	9f
	mov	q_opage, r0
	jsr	pc, mappg
	mov	$BUF, r1
	add	r4, r1
	mov	q_odest, r2
	jsr	pc, copy

9:	mov	$SWAP, r0		/ базовое ОЗУ обратно
	jsr	pc, mappg
	mov	$0100000, r1
	mov	$BUF, r2
	mov	$016300, r3		/ (SAVE_END-BUF)/2 слов
8:	mov	(r1)+, (r2)+
	sob	r3, 8b
	mov	q_sp, sp
	mov	q_caller, r0
	jsr	pc, setmap
	mov	_vctl, @$0177662
	mov	$01330, @$0177664
	mtps	$0340
	tst	_kbd_irq_on
	beq	4f
	mtps	$0
4:	mov	r5, r0
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ copy: r3 байт (округляется до слов) из r1 в r2; при флаге 1 — с инверсией (XOR 0xFF)
copy:
	inc	r3
	asr	r3
	beq	3f
	bit	$1, q_flags
	bne	2f
1:	mov	(r1)+, (r2)+
	sob	r3, 1b
	rts	pc
2:	mov	(r1)+, r0
	com	r0
	mov	r0, (r2)+
	sob	r3, 2b
3:	rts	pc

	.data
/ коды страниц 0..15 в регистре 0177130
_page_code:
	PCODES
_smk_home:	.word	0	/ раскладка запуска (там резидент ДОС с EMT 36)
_boot_psw:	.word	0
	.globl	_kbd_irq_on, _map_now
_kbd_irq_on:	.word	0	/ 1 — клавиатура по прерыванию (platform.c)
	.globl	_boot_v4, _boot_v60, _boot_v274, _boot_v100
/ векторы, отдаваемые ДОС на время EMT 36: адрес, затем значение при запуске
vecs:		.word	04
_boot_v4:	.word	0, 0
		.word	060
_boot_v60:	.word	0, 0
		.word	0274
_boot_v274:	.word	0, 0
		.word	0100
_boot_v100:	.word	0, 0
		.word	0
q_vec:		.word	0, 0, 0, 0, 0, 0, 0, 0
	.globl	_boot_tmr
_boot_tmr:	.word	0, 0	/ предел и управление таймера при запуске
q_tmr:		.word	0, 0
	.globl	_ld_resp
_ld_resp:	.word	0
q_flags:	.word	0
q_page:		.word	0
q_dest:		.word	0
q_split:	.word	0
q_opage:	.word	0
q_odest:	.word	0
q_sp:		.word	0
q_caller:	.word	0
	.even
blk:		.space	062	/ блок параметров EMT 36 (с полями текущего файла)
