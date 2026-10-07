/ Тон на динамике в фоне (общая копия кода). У БК нет генератора тона: бит 6
/ регистра 0177716 переключает сам процессор. Вместо того чтобы останавливать
/ игру ради тона, горячие циклы (колонки proc3, строки вывода, опкоды VM) зовут
/ snd_tick: если по таймеру ВИ1 подошёл срок фронта — переключить динамик и
/ назначить следующий срок. Полупериод — в отсчётах таймера с дробью 8.8, поэтому
/ средняя частота точна, а дрожание фронта — до промежутка между вызовами.
/ Состояние — _sndst (sys.ld): его пишет звук (sound.c), читает snd_tick.
	.globl _snd_tick, _snd_spin, _fm_scale, _sndst, _map_now, _cur_page
	.globl _snd_active, _snd_busy, _snd_seqt, _snd_seqv
	.text

	ON = _sndst+0			/ 0 — тишина
	LVL = _sndst+2			/ уровень следующего фронта (0 / 0100)
	NEXT = _sndst+4			/ срок фронта (таймер считает вниз)
	NEXTF = _sndst+6		/ его дробь (старший байт)
	HPI = _sndst+010		/ полупериод: целые отсчёты
	HPF = _sndst+012		/ дробь (старший байт)

/ void snd_tick(void): все регистры сохраняются. Кроме фронта — такт секвенсора:
/ если звук играет и прошло 198 отсчётов (два такта), зовётся snd_seq (main.c, по вектору
/ _snd_seqv) — откуда угодно, даже из циклов с чужой страницей: _cur_page на это
/ время = _map_now, и farret секвенсора вернёт именно её
_snd_tick:
	mov	r0, -(sp)
	tst	@$ON
	beq	5f
	mov	@$0177710, r0
	sub	@$NEXT, r0
	tst	r0			/ знак разности по модулю 2^16, не N^V
	bgt	5f			/ срок не подошёл
	mov	@$LVL, r0
	mov	r0, @$0177716
	neg	r0
	add	$0100, r0		/ 0 <-> 0100
	mov	r0, @$LVL
	sub	@$HPF, @$NEXTF		/ следующий срок: NEXT -= полупериод (8.8)
	sbc	@$NEXT
	sub	@$HPI, @$NEXT
	mov	@$0177710, r0
	sub	@$NEXT, r0
	tst	r0
	bgt	5f
	mov	@$0177710, r0		/ опоздали больше чем на полупериод — с текущего
	sub	@$HPI, r0
	mov	r0, @$NEXT
5:	tstb	@$_snd_active
	beq	8f
	tst	@$_snd_busy
	bne	8f
	mov	@$_snd_seqt, r0
	sub	@$0177710, r0
	cmp	r0, $0306		/ два такта 236 Гц = 198 отсчётов
	blo	8f
	inc	@$_snd_busy
	mov	r1, -(sp)
	mov	_cur_page, -(sp)
	mov	@$_map_now, _cur_page
	mov	@$_snd_seqv, r0
	jsr	pc, (r0)
	mov	(sp)+, _cur_page
	mov	(sp)+, r1
	clr	@$_snd_busy
8:	mov	(sp)+, r0
	rts	pc

/ void snd_spin(uint16_t counts): ждать counts отсчётов таймера, играя тон
_snd_spin:
	mov	@$0177710, r1
1:	jsr	pc, _snd_tick
	mov	r1, r0
	sub	@$0177710, r0
	cmp	r0, 2(sp)
	blo	1b
	rts	pc

/ int16_t fm_scale(int16_t t, uint16_t m): t * m / 256 с усечением к нулю,
/ |t| <= 128 (freqmod_table Player_V2) — 8 шагов сдвига вместо __mulhi3
_fm_scale:
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	010(sp), r2
	mov	012(sp), r3
	clr	r0
	clr	r1
	clr	r4			/ знак
	tst	r2
	bpl	1f
	neg	r2
	inc	r4
1:	swab	r2			/ |t| — в старший байт
	mov	$8, -(sp)
2:	asl	r1			/ (r0:r1) *= 2
	rol	r0
	asl	r2
	bcc	3f
	add	r3, r1
	adc	r0
3:	dec	(sp)
	bne	2b
	tst	(sp)+
	swab	r1			/ (r0:r1) >> 8
	bic	$0177400, r1
	swab	r0
	clrb	r0
	bis	r1, r0
	tst	r4
	beq	4f
	neg	r0
4:	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	rts	pc
