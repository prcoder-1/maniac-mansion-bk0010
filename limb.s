/ Путь «конечность -> спрайт» (банк B): drawLimb + mainRoutine по конечностям
/ актёра, поиск в кэше спрайтов и вывод из него, cpat_build. Модель на Си и
/ медленные ветки — в costume_r.c: заголовок кадра мимо кэша (limb_hdr), промах
/ кэша спрайтов (spr_draw), рисование proc3 (limb_p3). Сборка с -DLIMB_C — без
/ этого файла (сверка). Страница кэша спрайтов (040000) подключена, если lh_on
	.globl	_limbs_draw, _cpat_build
	.text

	A_STOP = 0110			/ struct Actor: cost_stopped
	A_POS = 0114			/ cost_curpos[16]
	OUT_W = 0500
	OUT_H = 0200
	SPR_TOP = 040000		/ SprHdr: вершина кольца (слово), круг (байт +2)
	SPR_SLOT = 041100		/ 64 x {res, foff, off, ps, pg, cost, key, pal, w, h, lap}
	SPR_LH = 043500			/ 128 x {res, pos, foff, hdr[6], pg, cost, limb, skip}
	P3_CPAT = _bounce+032		/ struct P3 (costume_r.c)
	P3_NEWPAL = _bounce+0146

/ uint8_t limbs_draw(struct Actor *a): drawLimb конечностей 0..15 -> ИЛИ результатов
_limbs_draw:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r0
	mov	A_STOP(r0), lb_stop
	add	$A_POS, r0
	mov	r0, lb_pp
	clr	lb_res
	clr	lb_limb
	clr	lb_l8
1:	mov	@lb_pp, r3		/ cost_curpos[limb]
	asr	lb_stop
	bcs	9f
	cmp	r3, $-1
	beq	9f
	bic	$0100000, r3		/ i
	tstb	_lh_on
	beq	3f
	mov	r3, r0			/ кэш заголовков: (i ^ i >> 5 ^ limb << 3 ^ c_cost) & 127
	asr	r0
	asr	r0
	asr	r0
	asr	r0
	asr	r0
	xor	r3, r0
	mov	lb_l8, r1
	xor	r1, r0
	movb	_c_cost, r1
	xor	r1, r0
	bic	$0177600, r0
	asl	r0
	asl	r0
	asl	r0
	mov	r0, r4
	asl	r0
	add	r0, r4
	add	$SPR_LH, r4		/ e
	cmp	2(r4), r3
	bne	4f
	cmpb	024(r4), lb_limb
	bne	4f
	cmpb	023(r4), _c_cost
	bne	4f
	cmp	(r4), _c_res
	bne	4f
	cmpb	022(r4), _c_page
	bne	4f
	tstb	025(r4)
	bne	9f
	mov	_c_res, r0
	add	4(r4), r0
	mov	r0, _srcptr
	add	$6, r4			/ hdr
	br	5f
3:	clr	r4
4:	mov	r4, -(sp)
	mov	r3, -(sp)
	mov	lb_limb, -(sp)
	jsr	pc, _limb_hdr
	add	$6, sp
	mov	r0, r4
	beq	9f
5:	mov	(r4)+, _width
	mov	(r4)+, _height
	mov	_xmove, r2
	add	(r4)+, r2		/ xmoveCur
	mov	_ymove, r3
	add	(r4)+, r3		/ ymoveCur
	add	(r4)+, _xmove
	sub	(r4), _ymove
	add	$014, _srcptr
	movb	lb_limb, _cur_limb
	jsr	pc, mainr
9:	add	$2, lb_pp
	add	$010, lb_l8
	inc	lb_limb
	cmp	lb_limb, $020
	blt	1b
	mov	lb_res, r0
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
mr0n:	rts	pc

/ mainRoutine: r2 — xmoveCur, r3 — ymoveCur; drawFlag — в lb_res
mainr:	tstb	_mirror
	bne	1f
	neg	r2
1:	add	_actorX, r2		/ vx
	add	_actorY, r3		/ vy (t)
	mov	r2, r0			/ l
	mov	r2, r1			/ r
	tstb	_mirror
	beq	2f
	add	_width, r1
	br	3f
2:	sub	_width, r0
3:	cmp	r3, $OUT_H
	bge	mr0n
	mov	r3, r4
	add	_height, r4		/ b
	ble	mr0n
	cmp	r0, $OUT_W
	bge	mr0n
	tst	r1
	ble	mr0n
	mov	_width, r5		/ skip_width
	clr	lb_pre
	mov	$1, lb_flag
	tstb	_mirror
	beq	5f
	tst	r2			/ skip = -vx
	bge	4f
	add	r2, r5
	mov	r2, lb_pre
	neg	lb_pre
	clr	r2
	br	7f
4:	cmp	r1, $OUT_W		/ skip = r - OUT_W
	bgt	41f
	mov	$2, lb_flag
	br	7f
41:	sub	r1, r5
	add	$OUT_W, r5
	br	7f
5:	cmp	r1, $OUT_W		/ skip = r - OUT_W
	ble	6f
	sub	r1, r5
	add	$OUT_W, r5
	mov	r1, lb_pre
	sub	$OUT_W, lb_pre
	mov	$OUT_W-1, r2
	br	7f
6:	cmp	r0, $-1			/ skip = -1 - l
	blt	61f
	mov	$2, lb_flag
	br	7f
61:	add	r0, r5
	inc	r5
7:	tst	r5
	ble	mr0
	mov	r5, lb_sw
	tst	r0
	bge	1f
	clr	r0
1:	mov	r3, r5			/ t
	bge	2f
	clr	r5
2:	cmp	r4, $OUT_H
	ble	3f
	mov	$OUT_H, r4
3:	cmp	r5, _draw_top
	bge	4f
	mov	r5, _draw_top
4:	cmp	r4, _draw_bottom
	ble	5f
	mov	r4, _draw_bottom
5:	cmp	r0, _draw_left
	bge	6f
	mov	r0, _draw_left
6:	inc	r1
	cmp	r1, _draw_right
	ble	7f
	mov	r1, _draw_right
7:	dec	r1
	tst	_meas_rec
	beq	8f
	mov	r4, -(sp)
	mov	lb_l8, r4
	add	_meas_rec, r4
	mov	r0, (r4)+
	mov	r5, (r4)+
	mov	r1, (r4)+
	mov	(sp)+, (r4)
8:	add	_height, r5
	cmp	r5, $0400
	blt	9f
	bisb	$2, lb_res
	rts	pc
9:	tstb	_measuring
	bne	mrf
	mov	_tg+014, r0		/ строки цели
	mov	r0, r1
	add	_tg+6, r1
	cmp	r3, r1
	bge	mrf
	mov	r3, r1
	add	_height, r1
	cmp	r1, r0
	ble	mrf
	mov	$-1, r4			/ step
	tstb	_mirror
	beq	1f
	neg	r4
1:	tstb	_spr_can
	beq	3f
	cmp	lb_flag, $2
	bne	3f
	jsr	pc, sprh
	tst	r0
	bne	mrf
3:	mov	lb_flag, -(sp)
	mov	lb_pre, -(sp)
	mov	r4, -(sp)
	mov	lb_sw, -(sp)
	mov	r3, -(sp)
	mov	r2, -(sp)
	jsr	pc, _limb_p3
	add	$014, sp
mrf:	bisb	lb_flag, lb_res
mr0:	rts	pc

/ Конечность из кэша спрайтов (spr_draw без постройки): r2 — vx, r3 — vy;
/ промах — постройка (spr_build); -> r0: 0 — рисовать proc3, 1 — нарисована.
/ r2..r4 сохраняет
sp0n:	mov	(sp)+, r4
sprn:	clr	r0
	rts	pc
sprh:	cmp	_width, $0376
	bhi	sprn
	cmp	_height, $0377
	bhi	sprn
	mov	r4, -(sp)
	mov	r2, r0
	tstb	_mirror
	bne	1f
	sub	_width, r0
	inc	r0
1:	add	_xstart, r0
	sub	_v_D, r0		/ wv: левая колонка в сетке
	clr	r1
	bisb	_div5(r0), r1
	mov	r1, r5
	asl	r5
	asl	r5
	add	r1, r5
	neg	r5
	add	r0, r5			/ wv mod 5
	mov	r0, r4
	sub	r1, r4			/ b0
	add	_width, r0
	dec	r0
	clr	r1
	bisb	_div5(r0), r1
	sub	r1, r0			/ b1
	sub	r4, r0
	mov	r4, r1
	bic	$0177774, r1		/ b0 & 3
	add	r1, r0
	add	$4, r0
	asr	r0
	asr	r0			/ w
	mov	r0, lb_w
	asl	r1
	asl	r1
	asl	r1
	bis	r1, r5			/ key
	bit	$1, r3
	beq	2f
	bis	$040, r5
2:	tstb	_mirror
	beq	3f
	bis	$0100, r5
3:	mov	r5, lb_key
	mov	r4, lb_b0
	bic	$3, r4
	add	_v_sh, r4
	sub	_tg+010, r4		/ vb: точка холста байта V
	bmi	sp0n
	mov	r4, r1
	bic	$0177774, r1
	mov	r1, lb_sh
	asr	r4
	asr	r4
	mov	r4, lb_col
	add	r0, r4
	tst	r1
	beq	4f
	inc	r4
4:	mov	_tg+4, r1
	asr	r1
	asr	r1
	cmp	r4, r1
	bhi	sp0
	mov	_srcptr, r4
	sub	_c_res, r4		/ foff
	mov	r4, r0			/ h = foff ^ foff >> 5 ^ c_cost ^ key << 2 ^ spr_pl << 5
	clc
	ror	r0
	asr	r0
	asr	r0
	asr	r0
	asr	r0
	xor	r4, r0
	movb	_c_cost, r1
	xor	r1, r0
	asl	r5
	asl	r5
	xor	r5, r0
	movb	_spr_pl, r1
	asl	r1
	asl	r1
	asl	r1
	asl	r1
	asl	r1
	xor	r1, r0
	movb	@$SPR_TOP+2, r1
	dec	r1
	mov	r1, lb_lapm1
	mov	r0, lb_h
	clr	lb_fr
	mov	$010, r5			/ SPR_WIN
5:	mov	r0, r1
	bic	$0177700, r1
	asl	r1
	asl	r1
	asl	r1
	asl	r1
	add	$SPR_SLOT, r1
	tstb	015(r1)			/ годен: h и (круг этот или прошлый за вершиной)
	beq	8f
	cmpb	016(r1), @$SPR_TOP+2
	beq	6f
	cmpb	016(r1), lb_lapm1
	bne	8f
	cmp	4(r1), @$SPR_TOP
	bhis	6f
8:	tst	lb_fr			/ негодный
	bne	7f
	mov	r1, lb_fr
	br	7f
6:	cmp	2(r1), r4
	bne	7f
	cmpb	012(r1), lb_key
	bne	7f
	cmp	(r1), _c_res
	bne	7f
	cmpb	010(r1), _c_page
	bne	7f
	cmpb	011(r1), _c_cost
	bne	7f
	cmpb	013(r1), _spr_pl
	beq	sphit
7:	inc	r0
	sob	r5, 5b
	mov	lb_fr, -(sp)		/ spr_build(vx, vy, step, b0, w, key, h, fr)
	mov	lb_h, -(sp)
	mov	lb_key, -(sp)
	mov	lb_w, -(sp)
	mov	lb_b0, -(sp)
	mov	012(sp), -(sp)		/ step (сохранённый r4)
	mov	r3, -(sp)
	mov	r2, -(sp)
	jsr	pc, _spr_build
	add	$020, sp
	mov	r0, r1
	bne	sphit
	br	spx
sp0:	clr	r0
spx:	mov	(sp)+, r4
	rts	pc

/ r1 — слот: строки спрайта в строках цели, вывод в холст
sphit:	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	_tg+014, r4
	sub	r3, r4			/ первая строка спрайта
	bge	1f
	clr	r4
1:	mov	_tg+014, r5
	add	_tg+6, r5
	sub	r3, r5			/ за последней
	cmp	r5, _height
	ble	2f
	mov	_height, r5
2:	sub	r4, r5			/ строк
	ble	spok
	add	r4, r3			/ строка MM первой
	mov	lb_w, r2
	inc	r2			/ w1
	mov	6(r1), lb_ps
	mov	4(r1), r0
	inc	r0			/ src
	tst	r4
	beq	4f
3:	add	r2, r0
	sob	r4, 3b
4:	mov	r3, r4
	sub	_tg+014, r4
	mov	_tg+2, r1
	jsr	pc, umul
	add	_tg, r4
	add	lb_col, r4		/ dst
	tstb	_zbuf
	bne	spz
	mov	lb_sh, r1
	bne	5f
	mov	$1, -(sp)		/ spr_blit(src, src + ps, w, h, dst, stride - w, 1)
	mov	_tg+2, -(sp)
	sub	lb_w, (sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	lb_w, -(sp)
	mov	r0, -(sp)
	add	lb_ps, (sp)
	mov	r0, -(sp)
	jsr	pc, _spr_blit
	br	6f
5:	mov	r1, -(sp)		/ spr_blits(src, ps, w1, h, dst, stride - w1, sh)
	mov	_tg+2, -(sp)
	sub	r2, (sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	r2, -(sp)
	mov	lb_ps, -(sp)
	mov	r0, -(sp)
	jsr	pc, _spr_blits
6:	add	$016, sp
spok:	mov	(sp)+, r3
	mov	(sp)+, r2
	mov	$1, r0
	br	spx
/ spr_blitm(src, ps, w1, nb, h, dst, stride - nb, sh, PAGE_ADDR + строка * bm_strips,
/ 5 * ((view0 >> 2) + col), bm_strips)
spz:	mov	r4, lb_dst
	mov	_bm_strips, -(sp)
	mov	_tg+010, r1
	asr	r1
	asr	r1
	add	lb_col, r1
	mov	r1, -(sp)
	asl	r1
	asl	r1
	add	r1, (sp)
	mov	r3, r4
	mov	_bm_strips, r1
	jsr	pc, umul
	add	$0100000, r4
	mov	r4, -(sp)
	mov	lb_sh, r1
	mov	r1, -(sp)
	mov	lb_w, r4		/ nb
	tst	r1
	beq	7f
	mov	r2, r4
7:	mov	_tg+2, -(sp)
	sub	r4, (sp)
	mov	lb_dst, -(sp)
	mov	r5, -(sp)
	mov	r4, -(sp)
	mov	r2, -(sp)
	mov	lb_ps, -(sp)
	mov	r0, -(sp)
	jsr	pc, _spr_blitm
	add	$026, sp
	br	spok

/ r4 = r4 * r1 (r4 — не меньше 0); портит r1
umul:	mov	r0, -(sp)
	clr	r0
1:	tst	r4
	beq	3f
	asr	r4
	bcc	2f
	add	r1, r0
2:	asl	r1
	br	1b
3:	mov	r0, r4
	mov	(sp)+, r0
	rts	pc

/ void cpat_build(void): cpal -> узоры палитры pal_want в P3.cpat, P3.newpal = 1;
/ cpat_sum — сумма её слов с циклическим сдвигом (spr_pal)
_cpat_build:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	clr	r0
	bisb	_pal_want, r0
	asl	r0
	asl	r0
	asl	r0
	asl	r0
	add	$_pal_apat, r0
	mov	$_cpal, r1
	mov	$P3_CPAT, r2
	clr	r3
	mov	$020, -(sp)
1:	movb	(r1)+, r4
	bic	$0177760, r4
	add	r0, r4
	clr	r5
	bisb	(r4), r5
	asl	r5
	asl	r5
	add	$_apat_pats, r5
	mov	(r5)+, (r2)
	asl	r3
	adc	r3
	add	(r2)+, r3
	mov	(r5), (r2)
	asl	r3
	adc	r3
	add	(r2)+, r3
	dec	(sp)
	bne	1b
	tst	(sp)+
	mov	r3, _cpat_sum
	mov	$1, P3_NEWPAL
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

	.data
lb_pp:	.word	0			/ &cost_curpos[limb]
lb_stop: .word	0			/ cost_stopped, сдвигается
lb_res:	.word	0
lb_limb: .word	0
lb_l8:	.word	0			/ limb * 8
lb_flag: .word	0			/ drawFlag
lb_pre:	.word	0			/ колонок пропустить (codec1_ignorePakCols)
lb_sw:	.word	0			/ skip_width
lb_w:	.word	0			/ байт в строке спрайта
lb_key:	.word	0
lb_sh:	.word	0			/ сдвиг в точках
lb_col:	.word	0			/ байт холста
lb_lapm1: .word	0			/ прошлый круг
lb_ps:	.word	0			/ плоскость спрайта
lb_dst:	.word	0
lb_b0:	.word	0			/ точка БК левой колонки в сетке
lb_h:	.word	0			/ хеш слота
lb_fr:	.word	0			/ негодный слот окна поиска
