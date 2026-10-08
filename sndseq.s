/ Такты секвенсора Player_V2 (банк C): nextTick/next_freqs, update_tone и
/ перевод делителя в полупериод из sound.c (там же Си-образец для хоста) —
/ горячая часть звука, 236 раз в секунду. Команды звука (execute_cmd) — на Си.
	.globl _seq_fill, _execute_cmd, _hulls, _freqmod_table, _snd_tick
	.globl _ch, _cur_nr, _sndst, _snd_ring, _snd_play
	.text

/ слова канала (enum в sound.c), шаг канала 32 слова
	TL = 0
	BF = 4
	FD = 6
	FR = 010
	VOL = 012
	VD = 014
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

	RH = _sndst+016
	RW = _sndst+020
	AHEAD = 32			/ кольцо впереди времени не больше чем на 32 такта

/ void seq_fill(void): до 4 тактов секвенсора в кольцо snd.s (sound_fill)
_seq_fill:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	$4, -(sp)
1:	mov	@$RW, r0		/ впереди
	sub	@$RH, r0
	cmp	r0, $AHEAD
	bge	9f
	mov	r0, -(sp)
	tst	_cur_nr
	beq	3f
	mov	$_ch, r5		/ nextTick: каналы 0..3
	mov	$4, r3
2:	tst	(r5)
	beq	21f
	jsr	pc, _snd_tick
	jsr	pc, nf
21:	add	$0100, r5
	sob	r3, 2b
3:	jsr	pc, tone
	tst	(sp)+			/ впереди < 0: догоняем время, такт не пишем
	bmi	4f
	mov	@$RW, r1
	bic	$0177700, r1
	asl	r1
	mov	r0, _snd_ring(r1)
4:	inc	@$RW
	dec	(sp)
	bne	1b
9:	tst	(sp)+
	clrb	@$_snd_play
	tst	_cur_nr
	beq	5f
	incb	@$_snd_play
5:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc

/ Player_V2::generateSpkSamples + tone_hp: первый канал с громкостью ->
/ полупериод 8.8 в отсчётах ВИ1 в r0 (0 — тишина); портит r0..r5
tone:	mov	$_ch, r1
	mov	$4, r2
1:	tst	VOL(r1)
	beq	2f
	tst	(r1)
	bne	3f
2:	add	$0100, r1
	sob	r2, 1b
	clr	r0
	rts	pc
3:	mov	FR(r1), r0
	mov	FM_M(r1), r3		/ freq += t * m / 256 (с усечением к нулю)
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
6:	tst	r0			/ делитель 0 -> 1
	bne	7f
	inc	r0
7:	cmp	r0, $062620		/ 26000: делитель PIT -> полупериод d * 2.5143
	blos	8f
	mov	$-1, r0
	rts	pc
8:	mov	r0, r1
	clc
	ror	r1
	mov	r1, r2			/ d >> 1
	asr	r1
	asr	r1
	asr	r1
	asr	r1
	asr	r1			/ d >> 6
	asl	r0
	add	r2, r0
	add	r1, r0
	rts	pc

/ Player_V2Base::next_freqs, канал — r5; портит r0, r1
nf:	mov	VD(r5), r0
	beq	1f
	add	r0, VOL(r5)
1:	add	FD(r5), BF(r5)
	mov	BF(r5), FR(r5)		/ freq = база; модуляция — в tone
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
	mov	r5, -(sp)
	jsr	pc, _execute_cmd	/ Си: r2..r5 сохраняет
	tst	(sp)+
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
