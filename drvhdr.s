/ Звуковой драйвер (страница 1 ОЗУ БК-0011М с 040000, snddrv.c): таблица входов,
/ такты секвенсора Player_V2 из кадрового прерывания, запись в AY.
	.globl _drv_init, _drv_start, _drv_stop, _drv_stopall, _drv_mode, _drv_out
	.globl _hulls, _freqmod_table, _ch, _cur_nr, _cur_data, _retaddr, _chainNextSound
	.globl _note_lengths, _hull_offsets, _freqmod_offsets, _freqmod_lengths
	.globl _spk_freq_table, _pcjr_freq_table, _pcjr, _pj_tp, _next_nr, _drv_out_spk, _cval
	.globl _snd_cur, _snd_next, _snd_mtimer
	.globl _ay_w, _ch_freq, _smk_rd, _page_code, _cur_page, _ay_out, _ay_want, _ay_dirty
	.text

/ входы (snddrv.h: DRV_*), по 4 байта
	jmp	@$drv_irq
	jmp	@$_drv_init
	jmp	@$_drv_start
	jmp	@$_drv_stop
	jmp	@$_drv_stopall
	jmp	@$_drv_mode

/ слова канала (enum в snddrv.c), шаг канала 32 слова
	TL = 0
	NC = 2
	BF = 4
	FD = 6
	FR = 010
	VOL = 012
	VD = 014
	TEMPO = 016
	INP = 020
	TR = 022
	NL = 024
	HC = 026
	HO = 030
	HCNT = 032
	FMT = 034
	FMO = 036
	FMI = 040
	FMM = 042
	FMMOD = 044
	FM_T = 062
	FM_M = 064

/ кадровое прерывание (irq.s, r0 и r1 сохранены): такты 236 Гц — 236*64 = 15104 на
/ 3125 за кадр (4 или 5), затем регистры AY.
/ Такты — шагами до ближайшего события (конец ноты, note_length, счётчик огибающей)
/ любого канала: такты шага до последнего событий не дают и идут оптом (quiet), на
/ последнем — полный nextTick в прежнем порядке каналов; итог тот же, что потактно.
drv_irq:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	clr	r4			/ тактов за кадр
	add	$035400, acc
1:	cmp	acc, $06065
	blo	2f
	sub	$06065, acc
	inc	r4
	br	1b
2:	tst	_cur_nr
	bne	20f
	jmp	9f
20:	mov	r4, r2			/ шаг: не больше оставшихся тактов
	.irp	o, 0, 0100, 0200, 0300
	mov	_ch+\o, r0		/ time_left
	beq	5f
	cmp	r0, r2
	bhis	3f
	mov	r0, r2
3:	mov	_ch+NL+\o, r0
	beq	4f
	cmp	r0, r2
	bhis	4f
	mov	r0, r2
4:	mov	_ch+HCNT+\o, r0
	beq	5f
	cmp	r0, r2
	bhis	5f
	mov	r0, r2
5:
	.endr
	sub	r2, r4			/ остаток кадра
	dec	r2			/ тихих тактов
	beq	7f
	.irp	o, 0, 0100, 0200, 0300
	tst	_ch+\o
	beq	6f
	mov	$_ch+\o, r5
	jsr	pc, quiet
6:
	.endr
7:					/ nextTick: каналы 0..3 по порядку (time_left — на момент
	.irp	o, 0, 0100, 0200, 0300	/ обработки: нота от раннего канала будит поздний)
	tst	_ch+\o
	beq	8f
	mov	$_ch+\o, r5
	jsr	pc, nf
8:
	.endr
	tst	r4
	beq	9f
	jmp	2b
9:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	jmp	_drv_out

/ r2 тактов канала r5 без событий: громкость, база, freqmod, счётчики; портит r0, r1
quiet:	mov	r2, -(sp)
	mov	r2, r1
	mov	VD(r5), r0
	beq	2f
1:	add	r0, VOL(r5)
	sob	r1, 1b
	mov	r2, r1
2:	mov	FD(r5), r0
	beq	4f
3:	add	r0, BF(r5)
	sob	r1, 3b
	mov	r2, r1
4:	mov	BF(r5), FR(r5)
	mov	FMI(r5), r0
	beq	6f
	cmp	r0, FMMOD(r5)		/ шаг freqmod не больше модуля: оптом — сумма, затем
	bhi	5f			/ вычитание модуля (тот же итог, что потактно)
	mov	r0, r1
	dec	r2
	beq	42f
41:	add	r0, r1
	sob	r2, 41b
42:	mov	(sp), r2		/ r2 — из стека: число тихих тактов (sob его обнулил)
	add	r1, FMO(r5)
43:	cmp	FMO(r5), FMMOD(r5)
	blos	6f
	sub	FMMOD(r5), FMO(r5)
	br	43b
5:	add	r0, FMO(r5)
	cmp	FMO(r5), FMMOD(r5)
	blos	51f
	sub	FMMOD(r5), FMO(r5)
51:	sob	r1, 5b
6:	sub	r2, (r5)		/ time_left
	tst	NL(r5)
	beq	7f
	sub	r2, NL(r5)
7:	tst	HCNT(r5)
	beq	8f
	sub	r2, HCNT(r5)
8:	tst	(sp)+
	rts	pc

/ Player_V2Base::next_freqs, канал — r5; freq = база, модуляция — в ch_freq; портит r0, r1
nf:	mov	VD(r5), r0
	beq	1f
	add	r0, VOL(r5)
1:	add	FD(r5), BF(r5)
	mov	BF(r5), FR(r5)
	mov	FMO(r5), r0
	add	FMI(r5), r0
	cmp	r0, FMMOD(r5)
	blos	2f
	sub	FMMOD(r5), r0
2:	mov	r0, FMO(r5)
	mov	FMM(r5), FM_M(r5)	/ без модуляции FM_T не нужен
	beq	21f
	clc
	ror	r0
	asr	r0
	asr	r0
	asr	r0
	add	FMT(r5), r0
	movb	_freqmod_table(r0), r0	/ int8 со знаком
	mov	r0, FM_T(r5)
21:	tst	NL(r5)
	beq	3f
	dec	NL(r5)
	bne	3f
	mov	$020, HO(r5)
	mov	$1, HCNT(r5)
3:	dec	(r5)			/ time_left
	bne	4f
	jsr	pc, xcmd
4:	tst	HCNT(r5)
	beq	9f
	dec	HCNT(r5)
	bne	9f
5:	mov	HC(r5), r0		/ &hulls[curve + offset / 2]
	asl	r0
	add	HO(r5), r0
	add	$_hulls, r0
	add	$4, HO(r5)
	cmp	2(r0), $-1
	bne	6f
	mov	(r0), VOL(r5)
	bne	5b
	clr	VD(r5)
	br	5b
6:	mov	(r0)+, VD(r5)
	mov	(r0), HCNT(r5)
9:	rts	pc

/ слово LE скрипта (r4)+ -> r0; портит r1
rdw:	movb	1(r4), r0
	swab	r0
	clrb	r0
	movb	(r4)+, r1
	bic	$0177400, r1
	bis	r1, r0
	inc	r4
	rts	pc

/ Player_V2Base::execute_cmd, канал — r5 (r2..r5 сохраняются); портит r0, r1.
/ r4 — указатель скрипта, r2 — канал ноты, r3 — признак последней ноты
xcmd:	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)		/ current_channel
	mov	NC(r5), r4
	bne	1f
	jmp	stopped
1:	add	_cur_data, r4
cmd:	movb	(r4)+, r0
	bic	$0177400, r0
	cmp	r0, $0370
	blo	notes
	sub	$0370, r0
	asl	r0
	jmp	@ctab(r0)
ctab:	.word	op_f8, op_f9, op_fa, op_fb, op_fc, op_fd, op_fe, op_ff

op_f8:	movb	(r4)+, r0		/ hull curve: hull_offsets[b / 2]
	bic	$0177401, r0
	mov	_hull_offsets(r0), HC(r5)
	br	cmd
op_f9:	movb	(r4)+, r0		/ freqmod curve: [b / 4]
	bic	$0177403, r0
	asr	r0
	mov	_freqmod_offsets(r0), FMT(r5)
	mov	_freqmod_lengths(r0), FMMOD(r5)
	br	cmd
op_fd:	jsr	pc, rdw			/ clear other channel: слово / sizeof(ChannelInfo) = 50
	clr	r1
1:	cmp	r0, $062
	blo	2f
	sub	$062, r0
	inc	r1
	cmp	r1, $4
	blo	1b
2:	swab	r1			/ * 0100 (шаг канала)
	clc
	ror	r1
	ror	r1
	mov	r1, r5
	add	$_ch, r5
op_fa:	mov	r5, r0			/ clear channel: слова 1..18, кроме tempo
	tst	(r0)+
	mov	$6, r1
1:	clr	(r0)+
	sob	r1, 1b
	tst	(r0)+
	mov	$013, r1
1:	clr	(r0)+
	sob	r1, 1b
	clr	FM_M(r5)
	br	cmd
op_fb:	mov	_retaddr, r4		/ ret
	add	_cur_data, r4
	br	cmd
op_fc:	jsr	pc, rdw			/ call
	mov	r4, r1
	sub	_cur_data, r1
	mov	r1, _retaddr
	mov	r0, r4
	add	_cur_data, r4
	br	cmd
op_fe:	movb	(r4)+, r2		/ loop: счётчик — слово канала b / 2
	bic	$0177401, r2
	add	r5, r2
	jsr	pc, rdw
	tst	(r2)
	beq	1f
	dec	(r2)
	beq	cmd
1:	add	r0, r4
	br	cmd
op_ff:	movb	(r4)+, r2		/ set parameter b / 2 (только внутри канала)
	bic	$0177400, r2
	jsr	pc, rdw
	cmp	r2, $062
	bhis	1f
	mov	r2, r1
	bic	$1, r1
	add	r5, r1
	mov	r0, (r1)
	sub	r5, r1
	cmp	r1, $FR
	bne	1f
	clr	FM_M(r5)
1:	tst	r2
	bne	cmd
	br	xend

/ ноты: канал (op >> 5) & 3; без 0200 — длина tempo * note_lengths[op & 037],
/ с 0200 — 11-битная длина, 020 — пауза
notes:	mov	r0, r2
	bic	$0177637, r2
	asl	r2
	add	$_ch, r2
	bit	$0200, r0
	bne	3f
	mov	TEMPO(r5), r1
	bne	1f
	inc	r1
1:	bic	$0177740, r0
	movb	_note_lengths(r0), r0
	clr	(r5)
2:	asr	r0			/ tempo * длина по битам длины
	bcc	21f
	add	r1, (r5)
21:	asl	r1
	tst	r0
	bne	2b
	movb	(r4)+, r0
	mov	r0, r3			/ бит 0200 — последняя нота
	bic	$0177600, r0
	cmp	r0, $0177
	beq	xend
	br	4f
3:	mov	r0, r3			/ опкод
	bic	$0177770, r0
	swab	r0
	clrb	r0
	movb	(r4)+, r1
	bic	$0177400, r1
	bis	r1, r0
	mov	r0, (r5)
	bit	$020, r3
	bne	xend
	clr	r3			/ не последняя
	movb	(r4)+, r0
	bic	$0177600, r0
4:	mov	(r5), TL(r2)		/ нота в канал r2
	mov	(r5), r1
	sub	INP(r2), r1
	mov	r1, NL(r2)
	add	TR(r2), r0
5:	tst	r0			/ note < 0: += 12
	bge	6f
	add	$014, r0
	br	5b
6:	clr	r1			/ октава
7:	cmp	r0, $014
	blt	8f
	sub	$014, r0
	inc	r1
	br	7b
8:	clr	HO(r2)
	mov	$1, HCNT(r2)
	tstb	_pcjr
	beq	10f
	cmp	r2, $_ch+0300		/ PCjr, канал 3 — шум
	bne	9f
	mov	r0, -(sp)		/ hull = 196 + note * 12
	asl	r0
	add	(sp)+, r0
	asl	r0
	asl	r0
	add	$0304, r0
	mov	r0, HC(r2)
	swab	r1			/ freq = 384 - 64 * octave
	clc
	ror	r1
	ror	r1
	mov	$0600, r0
	sub	r1, r0
	br	12f
9:	asl	r0
	mov	_pcjr_freq_table(r0), r0
	br	11f
10:	asl	r0
	mov	_spk_freq_table(r0), r0
11:	tst	r1			/ >> octave (без знака)
	beq	12f
	clc
	ror	r0
	dec	r1
	br	11b
12:	mov	r0, FR(r2)
	mov	r0, BF(r2)
	clr	FM_M(r2)
	tstb	r3
	bmi	xend
	movb	(r4)+, r0		/ следующая нота — без проверки на команду (как в ScummVM)
	bic	$0177400, r0
	jmp	notes

xend:	mov	(sp), r5		/ current_channel
	tst	(r5)
	beq	1f
	sub	_cur_data, r4
	mov	r4, NC(r5)
	br	xret
1:	clr	NC(r5)
stopped:
	mov	$_ch, r0		/ все 4 канала молчат — звук кончился
	mov	$4, r1
1:	tst	(r0)
	bne	xret
	add	$0100, r0
	sob	r1, 1b
	clr	_cur_nr
	clr	_cur_data
	jsr	pc, _chainNextSound
xret:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ uint16_t ch_freq(const uint16_t *c): freq + freqmod * multiplier / 256 (int8 * uint16,
/ усечение к нулю). |t| * m >> 8 = |t| * mh + (|t| * ml >> 8) (m = mh * 256 + ml),
/ произведения — четвертями квадратов: a * b = qsq[a + b] - qsq[|a - b|]
_ch_freq:
	mov	2(sp), r1
	mov	FR(r1), r0
	tst	FM_M(r1)
	beq	9f
	mov	FM_T(r1), r1
	beq	8f
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r0, -(sp)		/ freq
	clr	r4			/ знак t
	tst	r1
	bpl	1f
	neg	r1
	inc	r4
1:	mov	012(sp), r3		/ c
	mov	FM_M(r3), r3		/ m
	mov	r3, r2
	bic	$0177400, r2		/ ml
	clrb	r3
	swab	r3			/ mh
	jsr	pc, qmul		/ |t| * ml
	clrb	r0
	swab	r0			/ >> 8
	mov	r0, -(sp)
	mov	r3, r2
	jsr	pc, qmul		/ |t| * mh
	add	(sp)+, r0
	tst	r4
	beq	2f
	neg	r0
2:	add	(sp)+, r0		/ + freq
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
8:	mov	2(sp), r1
9:	rts	pc

/ r1 (0..128) * r2 (0..255) -> r0 (16 бит); портит r0, r2
qmul:	mov	r1, r0
	add	r2, r0
	asl	r0
	mov	qsq(r0), r0
	sub	r1, r2
	bpl	1f
	neg	r2
1:	asl	r2
	sub	qsq(r2), r0
	rts	pc

/ uint16_t pj_tp(uint16_t n): делитель SN76489 N (111860 / N Гц) -> период тона AY
/ (1714286 / 16 / TP): N * 0.957826 по таблицам младшего и старшего байта N
_pj_tp:	mov	2(sp), r0
/ то же: r0 -> r0, портит r1
pjtp:	cmp	r0, $01777
	blos	1f
	mov	$01777, r0
1:	mov	r0, r1
	bic	$0177400, r1
	clrb	r0
	swab	r0
	asl	r0
	mov	pjh(r0), r0
	movb	pjl(r1), r1
	bic	$0177400, r1
	add	r1, r0
	rts	pc
pjh:	.word	0, 245, 490, 736
pjl:
	.byte	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 11, 12, 13, 14
	.byte	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30
	.byte	31, 32, 33, 34, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45
	.byte	46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 57, 58, 59, 60
	.byte	61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76
	.byte	77, 78, 79, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91
	.byte	92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 102, 103, 104, 105, 106
	.byte	107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122
	.byte	123, 124, 125, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137
	.byte	138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 148, 149, 150, 151, 152
	.byte	153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168
	.byte	169, 170, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183
	.byte	184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 193, 194, 195, 196, 197, 198
	.byte	199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214
	.byte	215, 216, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229
	.byte	230, 231, 232, 233, 234, 235, 236, 237, 238, 238, 239, 240, 241, 242, 243, 244
	.even

/ Player_V2::generatePCjrSamples / generateSpkSamples -> регистры AY (раз в кадр).
/ PCjr: каналы 0..2 — тоны (громкость — по старшему байту ~volume: таблица lv256;
/ период — pjtp от freq >> 6, пересчёт только при смене freq канала: кэш cfr/cval),
/ канал 3 — шум в свободный канал AY (белый — генератором шума, периодический — тоном
/ с периодом 16 сдвигов). Спикер — drv_out_spk (snddrv.c).
_drv_out:
	tst	_cur_nr
	bne	1f
	tstb	silent			/ тишина уже в AY
	beq	10f
	tst	_ay_dirty
	bne	10f
	rts	pc
10:	clr	_ay_want+010		/ R8, R9
	clrb	_ay_want+012		/ R10
	mov	$077, r0
	jmp	out
1:	tstb	_pcjr
	bne	2f
	clr	_ay_want+010
	clrb	_ay_want+012
	jsr	pc, _drv_out_spk	/ Си: смеситель — в r0
	jmp	out
2:	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	$077, r2		/ смеситель R7: 1 — выключено
	mov	$_ch, r5
	clr	r3			/ канал
3:	mov	VOL(r5), r1
	beq	7f
	tst	(r5)
	beq	7f
	com	r1			/ ослабление (65535 - vol) >> 12 -> уровень AY
	clrb	r1
	swab	r1
	movb	lv256(r1), r1
	beq	7f
	movb	r1, _ay_want+010(r3)
	mov	FR(r5), r0		/ freq (с модуляцией)
	tst	FM_M(r5)
	beq	4f
	mov	r5, -(sp)
	jsr	pc, _ch_freq
	tst	(sp)+
4:	mov	r3, r1
	asl	r1
	tstb	cval(r3)
	beq	5f
	cmp	r0, cfr(r1)
	beq	6f			/ freq та же — период в ay_want тот же
5:	mov	r0, cfr(r1)
	movb	$1, cval(r3)
	jsr	pc, nsh6
	mov	r1, -(sp)
	jsr	pc, pjtp
	mov	(sp)+, r1
	movb	r0, _ay_want(r1)
	swab	r0
	movb	r0, _ay_want+1(r1)
6:	bicb	mbit(r3), r2
	br	8f
7:	clrb	_ay_want+010(r3)
8:	add	$0100, r5
	inc	r3
	cmp	r3, $3
	blo	3b
/ шум: уровень r4, f = freq >> 6 в r3, период тона от канала 2 (n = 3) — в r5
	mov	VOL(r5), r0		/ r5 — канал 3
	beq	nz0
	tst	(r5)
	beq	nz0
	com	r0
	clrb	r0
	swab	r0
	movb	lv256(r0), r4
	bne	nz1
nz0:	jmp	nz9
nz1:
	mov	FR(r5), r0
	tst	FM_M(r5)
	beq	1f
	mov	r5, -(sp)
	jsr	pc, _ch_freq
	tst	(sp)+
1:	jsr	pc, nsh6
	mov	r0, r3
	bic	$0177774, r0
	clr	r5
	cmp	r0, $3			/ сдвиг от канала 2: его период
	bne	2f
	mov	$_ch+0200, r5
	mov	FR(r5), r0
	tst	FM_M(r5)
	beq	3f
	mov	r5, -(sp)
	jsr	pc, _ch_freq
	tst	(sp)+
3:	jsr	pc, nsh6
	jsr	pc, pjtp
	mov	r0, r5
2:	clr	r1			/ первый свободный канал AY
4:	tstb	_ay_want+010(r1)
	beq	5f
	inc	r1
	cmp	r1, $3
	blo	4b
5:	bit	$4, r3
	beq	nper
	mov	r3, r0			/ белый: период шума AY
	bic	$0177774, r0
	beq	51f
	cmp	r0, $3
	bne	52f
	cmp	r5, $017
	bhi	52f
	asl	r5
	movb	r5, _ay_want+6
	br	53f
51:	movb	$017, _ay_want+6
	br	53f
52:	movb	$037, _ay_want+6
53:	cmp	r1, $3
	blo	56f
	clr	r1			/ все заняты — в самый тихий
	cmpb	_ay_want+011, _ay_want+010
	bhis	54f
	inc	r1
54:	cmpb	_ay_want+012, _ay_want+010(r1)
	bhis	55f
	mov	$2, r1
55:	cmpb	r4, _ay_want+010(r1)
	blos	57f
56:	movb	r4, _ay_want+010(r1)
57:	bicb	nbit(r1), r2
	br	nz9
nper:	cmp	r1, $3			/ периодический: тон в свободном канале
	bhis	nz9
	mov	r3, r0
	bic	$0177774, r0
	cmp	r0, $3
	beq	1f
	asl	r0
	mov	ptp(r0), r0
	br	2f
1:	mov	r5, r0			/ от канала 2: период * 16, не больше 07777
	asl	r0
	asl	r0
	asl	r0
	asl	r0
	cmp	r0, $07777
	blos	2f
	mov	$07777, r0
2:	mov	r1, r5
	asl	r5
	movb	r0, _ay_want(r5)
	swab	r0
	movb	r0, _ay_want+1(r5)
	clrb	cval(r1)		/ период канала занят шумом — кэш тона негоден
	movb	r4, _ay_want+010(r1)
	bicb	mbit(r1), r2
nz9:	mov	r2, r0
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
out:	movb	r0, _ay_want+7
	jsr	pc, _ay_out
	clr	_ay_dirty
	clrb	silent
	tst	_cur_nr
	bne	1f
	incb	silent
1:	mov	_cur_nr, @$_snd_cur	/ почтовый ящик ядра
	mov	_next_nr, @$_snd_next
	mov	_ch+056, @$_snd_mtimer	/ music_timer канала 0
	rts	pc

/ r0 >> 6 без знака
nsh6:	clc
	ror	r0
	asr	r0
	asr	r0
	asr	r0
	asr	r0
	asr	r0
	rts	pc

mbit:	.byte	1, 2, 4
nbit:	.byte	010, 020, 040
_cval:
cval:	.byte	0, 0, 0			/ cfr канала годен
	.even
cfr:	.word	0, 0, 0			/ freq, по которой посчитан период канала
ptp:	.word	245, 490, 980	/ pj_tp(256 << n), 1024 -> 1023
silent:	.byte	0
	.even
qsq:
	.word	0, 0, 1, 2, 4, 6, 9, 12, 16, 20, 25, 30
	.word	36, 42, 49, 56, 64, 72, 81, 90, 100, 110, 121, 132
	.word	144, 156, 169, 182, 196, 210, 225, 240, 256, 272, 289, 306
	.word	324, 342, 361, 380, 400, 420, 441, 462, 484, 506, 529, 552
	.word	576, 600, 625, 650, 676, 702, 729, 756, 784, 812, 841, 870
	.word	900, 930, 961, 992, 1024, 1056, 1089, 1122, 1156, 1190, 1225, 1260
	.word	1296, 1332, 1369, 1406, 1444, 1482, 1521, 1560, 1600, 1640, 1681, 1722
	.word	1764, 1806, 1849, 1892, 1936, 1980, 2025, 2070, 2116, 2162, 2209, 2256
	.word	2304, 2352, 2401, 2450, 2500, 2550, 2601, 2652, 2704, 2756, 2809, 2862
	.word	2916, 2970, 3025, 3080, 3136, 3192, 3249, 3306, 3364, 3422, 3481, 3540
	.word	3600, 3660, 3721, 3782, 3844, 3906, 3969, 4032, 4096, 4160, 4225, 4290
	.word	4356, 4422, 4489, 4556, 4624, 4692, 4761, 4830, 4900, 4970, 5041, 5112
	.word	5184, 5256, 5329, 5402, 5476, 5550, 5625, 5700, 5776, 5852, 5929, 6006
	.word	6084, 6162, 6241, 6320, 6400, 6480, 6561, 6642, 6724, 6806, 6889, 6972
	.word	7056, 7140, 7225, 7310, 7396, 7482, 7569, 7656, 7744, 7832, 7921, 8010
	.word	8100, 8190, 8281, 8372, 8464, 8556, 8649, 8742, 8836, 8930, 9025, 9120
	.word	9216, 9312, 9409, 9506, 9604, 9702, 9801, 9900, 10000, 10100, 10201, 10302
	.word	10404, 10506, 10609, 10712, 10816, 10920, 11025, 11130, 11236, 11342, 11449, 11556
	.word	11664, 11772, 11881, 11990, 12100, 12210, 12321, 12432, 12544, 12656, 12769, 12882
	.word	12996, 13110, 13225, 13340, 13456, 13572, 13689, 13806, 13924, 14042, 14161, 14280
	.word	14400, 14520, 14641, 14762, 14884, 15006, 15129, 15252, 15376, 15500, 15625, 15750
	.word	15876, 16002, 16129, 16256, 16384, 16512, 16641, 16770, 16900, 17030, 17161, 17292
	.word	17424, 17556, 17689, 17822, 17956, 18090, 18225, 18360, 18496, 18632, 18769, 18906
	.word	19044, 19182, 19321, 19460, 19600, 19740, 19881, 20022, 20164, 20306, 20449, 20592
	.word	20736, 20880, 21025, 21170, 21316, 21462, 21609, 21756, 21904, 22052, 22201, 22350
	.word	22500, 22650, 22801, 22952, 23104, 23256, 23409, 23562, 23716, 23870, 24025, 24180
	.word	24336, 24492, 24649, 24806, 24964, 25122, 25281, 25440, 25600, 25760, 25921, 26082
	.word	26244, 26406, 26569, 26732, 26896, 27060, 27225, 27390, 27556, 27722, 27889, 28056
	.word	28224, 28392, 28561, 28730, 28900, 29070, 29241, 29412, 29584, 29756, 29929, 30102
	.word	30276, 30450, 30625, 30800, 30976, 31152, 31329, 31506, 31684, 31862, 32041, 32220
	.word	32400, 32580, 32761, 32942, 33124, 33306, 33489, 33672, 33856, 34040, 34225, 34410
	.word	34596, 34782, 34969, 35156, 35344, 35532, 35721, 35910, 36100, 36290, 36481, 36672
lv256:
	.rept	16
	.byte	15
	.endr
	.rept	16
	.byte	14
	.endr
	.rept	16
	.byte	13
	.endr
	.rept	16
	.byte	12
	.endr
	.rept	16
	.byte	12
	.endr
	.rept	16
	.byte	11
	.endr
	.rept	16
	.byte	10
	.endr
	.rept	16
	.byte	9
	.endr
	.rept	16
	.byte	8
	.endr
	.rept	16
	.byte	8
	.endr
	.rept	16
	.byte	7
	.endr
	.rept	16
	.byte	6
	.endr
	.rept	16
	.byte	6
	.endr
	.rept	16
	.byte	5
	.endr
	.rept	16
	.byte	5
	.endr
	.rept	16
	.byte	0
	.endr
	.even

/ void ay_out(void): регистры R0..R10 из _ay_want, отличные от записанных (или все при
/ _ay_dirty)
_ay_out:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	$_ay_want, r1
	mov	$ay_regs, r2
	tst	_ay_dirty
	bne	5f
	mov	$5, r3			/ R0..R9 словами, R10 — отдельно
1:	cmp	(r1)+, (r2)+
	bne	2f
	sob	r3, 1b
	br	4f
2:	sub	$2, r1			/ слово другое: его байты по одному
	sub	$2, r2
	cmpb	(r1), (r2)
	beq	21f
	jsr	pc, wr1
	br	22f
21:	inc	r1
	inc	r2
22:	cmpb	(r1), (r2)
	beq	23f
	jsr	pc, wr1
	br	24f
23:	inc	r1
	inc	r2
24:	sob	r3, 1b
4:	cmpb	(r1), (r2)
	beq	9f
	jsr	pc, wr1
	br	9f
5:	mov	$013, r3
6:	jsr	pc, wr1
	sob	r3, 6b
9:	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
/ регистр (r1 - _ay_want) из (r1)+ в AY и в ay_regs (r2)+; портит r0
wr1:	mov	r1, r0
	sub	$_ay_want, r0
	movb	(r1)+, (r2)+
	clr	@$0177714		/ чип 1 TurboSound
	com	r0
	mov	r0, @$0177714		/ выбор регистра (инверсия)
	movb	-1(r1), r0
	comb	r0
	movb	r0, @$0177714		/ данные (инверсия)
	rts	pc

/ void ay_w(uint8_t r, uint8_t v): регистр AY на 0177714 — слово: выбор (сначала
/ чип 1 TurboSound — 0377 в инверсии), байт: данные; линии инверсные
_ay_w:
	clr	@$0177714
	mov	2(sp), r0
	com	r0
	mov	r0, @$0177714
	mov	4(sp), r0
	com	r0
	movb	r0, @$0177714
	rts	pc

/ void smk_rd(void *dst, uint16_t page, uint16_t addr, uint16_t n): из страницы СМК
/ (окно 0100000) в страницу драйвера или базовое ОЗУ; затем страница вызывающего
_smk_rd:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	010(sp), r0
	asl	r0
	mov	_page_code(r0), r0
	mov	r0, @$_smk_cur		/ тень (обработчик «СТОП», crt0.s)
	mov	$6, @$0177130
	mov	r0, @$0177130
	mov	$0, @$0177130
	mov	012(sp), r1		/ откуда
	mov	6(sp), r2		/ куда
	mov	014(sp), r3		/ сколько
	beq	3f
	mov	r1, r0
	bis	r2, r0
	asr	r0
	bcs	2f			/ нечётный адрес — байтами
	inc	r3
	asr	r3
1:	mov	(r1)+, (r2)+
	sob	r3, 1b
	br	3f
2:	movb	(r1)+, (r2)+
	sob	r3, 2b
3:	mov	_cur_page, @$_smk_cur
	mov	$6, @$0177130
	mov	_cur_page, @$0177130
	mov	$0, @$0177130
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

	.data
acc:	.word	0
ay_regs: .byte	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0	/ записанные в AY R0..R10
	.even
