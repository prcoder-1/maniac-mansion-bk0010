/ Звуковой драйвер (страница 1 ОЗУ БК-0011М с 040000, snddrv.c): таблица входов,
/ такты секвенсора Player_V2 из кадрового прерывания, запись в AY.
	.globl _drv_init, _drv_start, _drv_stop, _drv_stopall, _drv_mode, _drv_out
	.globl _hulls, _freqmod_table, _ch, _cur_nr, _cur_data, _retaddr, _chainNextSound
	.globl _note_lengths, _hull_offsets, _freqmod_offsets, _freqmod_lengths
	.globl _spk_freq_table, _pcjr_freq_table, _pcjr, _pj_tones, _att_level, _tpc_n, _tpc_v, _pj_tp
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
	beq	9f
	mov	r4, r2			/ шаг: не больше оставшихся тактов
	mov	$_ch, r5
	mov	$4, r3
3:	mov	(r5), r0		/ time_left
	beq	5f
	cmp	r0, r2
	bhis	31f
	mov	r0, r2
31:	mov	NL(r5), r0
	beq	32f
	cmp	r0, r2
	bhis	32f
	mov	r0, r2
32:	mov	HCNT(r5), r0
	beq	5f
	cmp	r0, r2
	bhis	5f
	mov	r0, r2
5:	add	$0100, r5
	sob	r3, 3b
	sub	r2, r4			/ остаток кадра
	dec	r2			/ тихих тактов
	beq	7f
	mov	$_ch, r5
	mov	$4, r3
6:	tst	(r5)
	beq	61f
	jsr	pc, quiet
61:	add	$0100, r5
	sob	r3, 6b
7:	jsr	pc, tick
	tst	r4
	bne	2b
9:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	jmp	_drv_out

/ r2 тактов канала r5 без событий: громкость, база, freqmod, счётчики; портит r0, r1
quiet:	mov	r2, r1
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
8:	rts	pc

/ Player_V2Base::nextTick: каналы 0..3
tick:	mov	r5, -(sp)
	mov	r3, -(sp)
	mov	$_ch, r5
	mov	$4, r3
1:	tst	(r5)
	beq	2f
	jsr	pc, nf
2:	add	$0100, r5
	sob	r3, 1b
	mov	(sp)+, r3
	mov	(sp)+, r5
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
/ усечение к нулю)
_ch_freq:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r1
	mov	FR(r1), r0
	mov	FM_M(r1), r3
	beq	6f
	mov	FM_T(r1), r2
	beq	6f
	clr	r4			/ знак
	tst	r2
	bpl	4f
	neg	r2
	inc	r4
4:	mov	r0, -(sp)
	clr	r0			/ (r0:r3) — m << k
	clr	r1			/ (r5:r1) — произведение
	clr	r5
5:	asr	r2			/ по битам |t| с младшего, пока есть
	bcc	51f
	add	r3, r1
	adc	r5
	add	r0, r5
51:	asl	r3
	rol	r0
	tst	r2
	bne	5b
	mov	(sp)+, r0
	swab	r1			/ >> 8
	bic	$0177400, r1
	swab	r5
	clrb	r5
	bis	r5, r1
	tst	r4
	beq	52f
	neg	r1
52:	add	r1, r0
6:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
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

/ uint8_t pj_tones(void): PCjr, каналы 0..2 -> громкости ay_want[8..10] и периоды
/ ay_want[0..5] звучащих; возвращает смеситель R7 (тоны звучащих включены)
_pj_tones:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r5, -(sp)
	mov	$077, r2
	mov	$_ch, r5
	clr	r3
1:	mov	VOL(r5), r1
	beq	5f
	tst	(r5)
	beq	5f
	com	r1			/ ослабление (65535 - vol) >> 12 -> уровень AY
	swab	r1
	bic	$0177400, r1
	asr	r1
	asr	r1
	asr	r1
	asr	r1
	movb	_att_level(r1), r1
	beq	5f
	movb	r1, _ay_want+010(r3)
	mov	FR(r5), r0		/ делитель: freq (с модуляцией) >> 6
	tst	FM_M(r5)
	beq	2f
	mov	r5, -(sp)
	jsr	pc, _ch_freq
	tst	(sp)+
2:	clc
	ror	r0
	asr	r0
	asr	r0
	asr	r0
	asr	r0
	asr	r0
	mov	r3, r1
	asl	r1
	cmp	r0, _tpc_n(r1)
	beq	3f
	mov	r0, _tpc_n(r1)
	mov	r1, -(sp)
	jsr	pc, pjtp
	mov	(sp)+, r1
	mov	r0, _tpc_v(r1)
3:	mov	_tpc_v(r1), r0
	movb	r0, _ay_want(r1)
	swab	r0
	movb	r0, _ay_want+1(r1)
	bicb	mbit(r3), r2
	br	6f
5:	clrb	_ay_want+010(r3)
6:	add	$0100, r5
	inc	r3
	cmp	r3, $3
	blo	1b
	mov	r2, r0
	mov	(sp)+, r5
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
mbit:	.byte	1, 2, 4
	.even

/ void ay_out(void): регистры R0..R10 из _ay_want, отличные от записанных (или все при
/ _ay_dirty)
_ay_out:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	$_ay_want, r1
	mov	$ay_regs, r2
	mov	$013, r3
	tst	_ay_dirty
	bne	5f
1:	cmpb	(r1)+, (r2)+
	bne	2f
	sob	r3, 1b
	br	9f
2:	dec	r1
	dec	r2
	jsr	pc, wr1
	sob	r3, 1b
	br	9f
5:	jsr	pc, wr1
	sob	r3, 5b
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
3:	mov	$6, @$0177130
	mov	_cur_page, @$0177130
	mov	$0, @$0177130
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

	.data
acc:	.word	0
ay_regs: .byte	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0	/ записанные в AY R0..R10
	.even
