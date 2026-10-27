/ Распаковка файлов LFL, сжатых ZX0 (формат v2, tools/salvador) — в общей копии кода:
/ зовёт загрузчик (loader.s) после чтения, при подключённой странице назначения.
/ dzx0 — распаковщик reddie (dzx0_pdp11_upd.asm; ZX0 v2.2 (c) Einar Saukas & Urusergi).
	BUF = 03200			/ буфер загрузчика (loader.s)
	BUF_END = 070000
	.globl	_zx0_ld
	.text

/ r5 — длина сжатого файла в BUF. Сжатое — в конец буфера, распакованное — с BUF:
/ распаковка на месте (вход всегда впереди выхода: распакованный файл короче буфера
/ на запас, tools/zx0lfl.py). Смещения — не дальше 16 Кбайт: старший байт смещения
/ dzx0 собирает в знаковый байт (упаковка — с окном 16256, zx0lfl.py). Выход: r5 —
/ длина распакованного; портит r0..r4
_zx0_ld:
	mov	r5, r0
	inc	r0
	asr	r0			/ слов
	mov	r0, r2
	asl	r2
	mov	$BUF, r1
	add	r2, r1			/ конец сжатого
	mov	$BUF_END, r2
1:	mov	-(r1), -(r2)
	sob	r0, 1b
	mov	r2, r0
	mov	$BUF, r1
	jsr	pc, dzx0
	sub	$BUF, r1
	mov	r1, r5
	rts	pc

/ dzx0: r0 — сжатое, r1 — куда; выход: r1 — за распакованным; портит r0..r4.
/ Смещение — на стеке
dzx0:
	clr	r2
	movb	$0200, r3
	sxt	-(sp)			/ N от movb: смещение -1
lit:	jsr	pc, elias
1:	movb	(r0)+, (r1)+
	sob	r2, 1b
	aslb	r3
	bcs	newof
	jsr	pc, elias
copy:	mov	r1, r4
	add	(sp), r4
2:	movb	(r4)+, (r1)+
	sob	r2, 2b
	aslb	r3
	bcc	lit
newof:	tst	(sp)+
	mov	$-2, r2
	jsr	pc, elloop
	incb	r2
	bne	3f
exit:	rts	pc
3:	swab	r2
	mov	r2, -(sp)
	movb	(r0)+, (sp)
	asr	(sp)
	mov	$1, r2
	bcs	4f
	jsr	pc, elback
4:	inc	r2
	br	copy
elias:	incb	r2
elloop:	aslb	r3
	bne	5f
	movb	(r0)+, r3
	rolb	r3
5:	bcs	exit
elback:	aslb	r3
	rol	r2
	br	elloop
