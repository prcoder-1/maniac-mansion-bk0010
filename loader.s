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
BUF       = 03200
BUF_END   = 070000
SAVE_END  = 040000

	.globl _ld_file0, _smk_map, _smk_code, _page_code, _smk_home, _boot_psw
	.globl _cur_page, _vctl, _mem11, _ld_q, _ld_blk, _ld_fast, smkset, mappg, setcode
	.globl _rst, _snap_rst, restart, _kbd_irq_on, _zx0_ld
	.text

/ void smk_map(uint16_t page): страница page (0..15)
_smk_map:
	mov	2(sp), r0
	asl	r0
	mov	_page_code(r0), r0
setmap:
/ включить код r0 (режим|страница) и запомнить его в _cur_page; портит r0
setcode:
	mov	r0, _cur_page
/ включить код r0 в СМК; портит r0
smkset:
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

/ int16_t ld_file0(void): прочитать файл и разложить по страницам; имя и поля
/ struct ld_req { name; flags; page; dest; split; ovf_page; ovf_dest; fblk; flen;
/ fchk } (smk.h) уже в _ld_blk и _ld_q (ld_file — irq.s). Возвращает длину файла
/ или -1 (ответ драйвера в ld_resp). fblk != 0 — файл лежит подряд с блока fblk
/ (таблицы ftab.py): читается одним вызовом драйвера ПЗУ в обход ДОС, в окне
/ 040000 — скрытая страница 3 ОЗУ 11М (экран цел, _ld_fast = 1); первое слово не
/ fchk или ошибка — через ДОС (EMT 36 сама подключает в окно экран — он в мусоре).
_ld_file0:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	clr	_ld_fast
	mov	sp, q_sp
	mov	_cur_page, q_caller

	mtps	$0340
	mov	_vctl, r0		/ кадровое прерывание — выкл.: обработчик в затираемом ОЗУ
	bis	$040000, r0
	mov	r0, @$0177662
	mov	_mem11, r0		/ окно 040000 — страница 3 (буфер файла не в экране)
	bic	$070000, r0
	bis	$030000, r0
	mov	r0, @$0177716
	mov	$SWAP, r0		/ базовое ОЗУ -> SWAP
	jsr	pc, mappg
	mov	$BUF, r1
	mov	$0100000, r2
	mov	$016300, r3		/ (SAVE_END-BUF)/2 слов
4:	mov	(r1)+, (r2)+
	sob	r3, 4b
	mov	$01000, sp		/ стек, как при запуске из ДОС (до копирования в BUF: старый — внутри него)
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
	mov	_smk_home, r0
	jsr	pc, setcode
	clr	blk			/ ответ 0
	mov	q_fblk, r0
	beq	82f
	mov	q_flen, r1
	inc	r1
	clc
	ror	r1			/ слов
	mov	$BUF, r2
	mov	$0120020, r3		/ рабочая область драйвера — та же, что у ДОС
	jsr	pc, @$0160004
	bcs	85f
	cmp	@$BUF, q_fchk
	bne	82f
	mov	q_flen, blk+030
	inc	_ld_fast
	br	84f
85:	cmpb	@$052, $7		/ ошибка 7: «СТОП» во время чтения — игра заново
	bne	82f
	jmp	_rst
82:	mov	$0230, @$0177664	/ ДОС: малый экран
	mov	$3, blk			/ чтение; ответ 0
	mov	$BUF, blk+2
	clr	blk+4
	clr	blk+030
	mtps	_boot_psw
	mov	$blk, r1
	emt	036
84:	mtps	$0340
	mov	$010200, @$0177716	/ «СТОП» снова заблокирована (crt0.s): ДОС могла писать в регистр
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
5:	cmp	r5, $064600		/ BUF_END-BUF
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
	bit	$4, q_flags		/ LD_ZX0: распаковать в буфере (zx0.s — общая копия кода,
	beq	70f			/ она в странице)
	jsr	pc, _zx0_ld
70:	mov	r5, r3			/ до split байт — в страницу
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
	mov	_mem11, @$0177716	/ окно 040000 — снова экран
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

/ «СТОП» (вектор 4; и ошибка 7 драйвера ПЗУ — «СТОП» во время его операции): игра
/ заново, как после загрузки. Линия «СТОП» — та же, что ОСТ разъёма МПИ (IRQ1): по ней
/ СМК сбрасывается в SYS, страница 0, и держит сброс, пока клавиша нажата. Здесь — то,
/ что работает и при затёртом чтением файла базовом ОЗУ (с 03200): ждём, пока
/ подключится страница SWAP с меткой снимка (до ~2 с; держат дольше — вход повторится);
/ стек загрузчика (идёт чтение) — базовое ОЗУ из его копии
_rst:	mtps	$0340
	clr	r1
	cmp	sp, $01000
	bhi	1f
	inc	r1			/ стек загрузчика: идёт чтение, базовое ОЗУ затёрто
1:	mov	$040000, sp
	clr	_kbd_irq_on		/ клавиатура — опросом, как при пуске
	clr	r2
2:	mov	$SWAP, r0
	jsr	pc, mappg
	cmp	@$SNAPM, $SNAPMAGIC
	beq	3f
	sob	r2, 2b
	jmp	restart			/ снимка ещё нет (самое начало пуска)
3:	tst	r1
	beq	4f
	mov	$0100000, r3
	mov	$BUF, r4
	mov	$016300, r2		/ (SAVE_END-BUF)/2 слов
5:	mov	(r3)+, (r4)+
	sob	r2, 5b
4:	jsr	pc, _snap_rst		/ данные ядра — из снимка (общая копия кода видна в SWAP)
	mov	_mem11, @$0177716	/ регистры 11М — по теням, СМК — в раскладке запуска
	mov	_vctl, @$0177662
	mov	_smk_home, r0
	jsr	pc, setcode
	jmp	restart

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
	.globl	_kbd_irq_on
_kbd_irq_on:	.word	0	/ 1 — клавиатура по прерыванию (platform.c)
	.globl	_boot_v60, _boot_v274, _boot_v100
/ векторы, отдаваемые ДОС на время EMT 36: адрес, затем значение при запуске (вектор 4
/ ДОС и драйвер ПЗУ на время операции ставят себе сами; вне её он наш — _rst)
vecs:		.word	060
_boot_v60:	.word	0, 0
		.word	0274
_boot_v274:	.word	0, 0
		.word	0100
_boot_v100:	.word	0, 0
		.word	0
q_vec:		.word	0, 0, 0, 0, 0, 0
	.globl	_boot_tmr
_boot_tmr:	.word	0, 0	/ предел и управление таймера при запуске
q_tmr:		.word	0, 0
	.globl	_ld_resp
_ld_resp:	.word	0
_ld_q:					/ поля struct ld_req после имени — подряд (ld_file, irq.s)
q_flags:	.word	0
q_page:		.word	0
q_dest:		.word	0
q_split:	.word	0
q_opage:	.word	0
q_odest:	.word	0
q_fblk:		.word	0
q_flen:		.word	0
q_fchk:		.word	0
_ld_fast:	.word	0		/ 1 — прочитан драйвером ПЗУ, экран цел
q_sp:		.word	0
q_caller:	.word	0
	.even
_ld_blk:
blk:		.space	062	/ блок параметров EMT 36 (с полями текущего файла)
