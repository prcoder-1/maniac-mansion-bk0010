/ Такты секвенсора Player_V2 (банк C): nextTick/next_freqs из sound.c (там же
/ Си-образец для хоста) — горячая часть звука, 236 раз в секунду на 4 канала.
/ Команды звука (execute_cmd) — на Си.
	.globl _seq_ticks, _execute_cmd, _hulls, _freqmod_table, _snd_tick
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

/ void seq_ticks(uint16_t *ch, uint16_t n): n тактов каналов 0..3
_seq_ticks:
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	012(sp), r4		/ n
	beq	9f
1:	mov	010(sp), r5		/ канал 0
	mov	$4, r3
2:	tst	(r5)
	beq	3f
	jsr	pc, _snd_tick
	jsr	pc, nf
3:	add	$0100, r5
	sob	r3, 2b
	sob	r4, 1b
9:	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	rts	pc

/ Player_V2Base::next_freqs, канал — r5; портит r0, r1
nf:	mov	VD(r5), r0
	beq	1f
	add	r0, VOL(r5)
1:	mov	BF(r5), r0
	add	FD(r5), r0
	mov	r0, BF(r5)
	mov	r0, FR(r5)		/ freq = база; модуляция — в update_tone
	mov	FMO(r5), r0
	add	FMI(r5), r0
	cmp	r0, FMMOD(r5)
	blos	2f
	sub	FMMOD(r5), r0
2:	mov	r0, FMO(r5)
	clc
	ror	r0
	asr	r0
	asr	r0
	asr	r0
	add	FMT(r5), r0
	movb	_freqmod_table(r0), r0	/ int8 со знаком
	mov	r0, FM_T(r5)
	mov	FMM(r5), FM_M(r5)
	mov	NL(r5), r0
	beq	3f
	dec	r0
	mov	r0, NL(r5)
	bne	3f
	mov	$020, HO(r5)
	mov	$1, HCNT(r5)
3:	dec	(r5)			/ time_left
	bne	4f
	mov	r5, -(sp)
	jsr	pc, _execute_cmd	/ Си: r2..r5 сохраняет
	tst	(sp)+
4:	mov	HCNT(r5), r0
	beq	9f
	dec	r0
	mov	r0, HCNT(r5)
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
