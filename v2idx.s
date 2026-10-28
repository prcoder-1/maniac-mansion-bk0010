/ Таблицы полос картинки комнаты и маски без декодирования точек — для ленивого
/ декодирования фона (gfx.c room_index). Код банка C, но выполняется копией в _bounce
/ (базовое ОЗУ): на время разбора в окне — страница комнаты. Не длиннее 256 байт;
/ адреса — только абсолютные (общая копия кода, базовое ОЗУ), переходы — короткие.
	.globl	_v2idx_code, _v2idx_end
	.text

/ void v2idx(struct v2p *p): _v2_strips {src, серия, цвет} и _v2_zstrips {src,
/ серия | повтор<<7, байт} — те же, что пишут v2_pix и v2_mask (gfxv2.s) с tab, но
/ разбором по сериям. Из struct v2p — 0 spage, 2 src, 4 w, 6 h
_v2idx_code:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r1
	mov	(r1)+, r0
	jsr	pc, @$mapr0		/ страница комнаты (общая копия кода, smk.s)
	mov	(r1)+, r0		/ src
	mov	(r1)+, r3		/ колонок
	mov	(r1), -(sp)		/ 2(sp) — h
	mov	r0, r1
	clr	r4			/ осталось точек серии
	clr	r5			/ цвет; -1 — повтор
	mov	$_v2_strips, r0
	clr	-(sp)			/ (sp) — x & 7
1:	tst	(sp)			/ колонка с x % 8 == 0: запись в таблицу
	bne	2f
	mov	r1, (r0)+
	movb	r4, (r0)+
	movb	r5, (r0)+
2:	inc	(sp)
	bic	$0177770, (sp)
	mov	2(sp), r2		/ строк колонки
3:	tst	r4			/ байт RLE — как в v2_pix (seg)
	bne	5f
	movb	(r1)+, r4
	bmi	4f
	movb	rl_col(r4), r5
	movb	rl_len(r4), r4
	bne	5f
	br	6f
4:	bic	$0177600, r4
	mov	$-1, r5
	tst	r4
	bne	5f
6:	clr	r4
	bisb	(r1)+, r4
5:	cmp	r4, r2			/ серия дойдёт до конца колонки
	bhis	7f
	sub	r4, r2
	clr	r4
	br	3b
7:	sub	r2, r4
	sob	r3, 1b
	mov	$_v2_zstrips, r0	/ маска — сразу за картинкой (как v2_mask)
	mov	016(sp), r3
	mov	4(r3), r3
	asr	r3
	asr	r3
	asr	r3			/ полос
	clr	r4
	clr	r2			/ 1 — серия повтора
	clr	r5
1:	mov	r1, (r0)+
	movb	r4, (r0)
	tst	r2
	beq	2f
	bisb	$0200, (r0)
2:	inc	r0
	movb	r5, (r0)+
	mov	2(sp), (sp)		/ строк полосы
3:	tst	r4
	bne	4f
	movb	(r1)+, r4
	bic	$0177400, r4
	clr	r2
	bit	$0200, r4
	beq	4f
	bic	$0200, r4
	inc	r2
	movb	(r1)+, r5
4:	mov	(sp), -(sp)		/ n = min(серия, строк)
	cmp	r4, (sp)
	bhis	5f
	mov	r4, (sp)
5:	sub	(sp), r4
	sub	(sp), 2(sp)
	tst	r2
	bne	6f
	add	(sp), r1		/ байты подряд — мимо; последний — в r5, как v2_mask
	movb	-1(r1), r5
6:	tst	(sp)+
	tst	(sp)
	bne	3b
	sob	r3, 1b
	cmp	(sp)+, (sp)+
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	jmp	@$back			/ страница вызывающего (smk.s)
_v2idx_end:
