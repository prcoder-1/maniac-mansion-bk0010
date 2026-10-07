/ Звук на динамике (общая копия кода). У БК нет генератора тона: бит 6 регистра
/ 0177716 переключает сам процессор, по таймеру ВИ1 (23437.5 Гц, считает вниз).
/
/ Секвенсор Player_V2 (sound.c, банк C) идёт вперёд времени и кладёт в кольцо
/ _snd_ring точный полупериод тона каждого такта 236 Гц (8.8 отсчёта, 0 —
/ тишина). snd_tick, который зовут горячие циклы и межбанковые вызовы, сам
/ переключает такты по таймеру, берёт тон такта из кольца и ставит фронты: в
/ фоне — с дрожанием до промежутка между вызовами (высокие ноты — октавой-двумя
/ ниже), а пока есть запас времени (половина) — нота такта целиком в плотном
/ цикле, точно. Когда в кольце меньше LOW тактов, зовётся секвенсор (snd_seq в
/ main.c по вектору _snd_seqv) — откуда угодно: _cur_page на это время =
/ _map_now, и его farret вернёт подключённую страницу.
	.globl _snd_tick, _snd_spin, _fm_scale, _sndst, _map_now, _cur_page
	.globl _snd_active, _snd_play, _snd_busy, _snd_cur, _snd_seqv, _snd_ring
	.text

	ON = _sndst+0			/ динамик тикает
	LVL = _sndst+2			/ уровень следующего фронта (0 / 0100)
	NEXT = _sndst+4			/ срок фронта
	NEXTF = _sndst+6		/ его дробь (старший байт)
	HPI = _sndst+010		/ полупериод: целые отсчёты
	HPF = _sndst+012		/ дробь (старший байт)
	RT = _sndst+014			/ отсчёт таймера начала текущего такта
	RH = _sndst+016			/ номер текущего такта
	RW = _sndst+020			/ номер такта, которого ещё нет в кольце
	TOK = _sndst+022		/ запас плотного цикла, отсчёты
	IDLE = _sndst+024		/ простой главного цикла: тон и так точный
	TICK = 0143			/ такт 236 Гц = 99 отсчётов
	LOW = 020			/ дозаполнять кольцо, если в нём меньше 16 тактов
	BGMIN = 014000			/ полупериод фона не меньше 24 отсчётов (8.8)
	TOKMAX = 01400

/ void snd_tick(void): все регистры сохраняются
_snd_tick:
	tstb	@$_snd_active
	beq	9f
	mov	r0, -(sp)
	mov	@$RT, r0
	sub	@$0177710, r0
	cmp	r0, $TICK
	bhis	5f
1:	tst	@$ON			/ фронт, если подошёл срок
	beq	8f
	mov	@$0177710, r0
	sub	@$NEXT, r0
	tst	r0
	bgt	8f
	jsr	pc, edge
8:	mov	(sp)+, r0
9:	rts	pc
5:	jsr	pc, ntick		/ новый такт; дозаполнить кольцо
	tst	@$_snd_busy
	bne	1b
	mov	@$RW, r0
	sub	@$RH, r0
	cmp	r0, $LOW
	bge	1b
	inc	@$_snd_busy
	mov	r1, -(sp)
	mov	_cur_page, -(sp)
	mov	@$_map_now, _cur_page
	mov	@$_snd_seqv, r0
	jsr	pc, (r0)
	mov	(sp)+, _cur_page
	mov	(sp)+, r1
	clr	@$_snd_busy
	br	1b

/ фронт, если подошёл срок; портит r0
edge:	mov	@$0177710, r0
	sub	@$NEXT, r0
	tst	r0			/ знак разности по модулю 2^16
	bgt	9f
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
	bgt	9f
	mov	@$0177710, r0		/ опоздали больше чем на полупериод — с текущего
	sub	@$HPI, r0
	mov	r0, @$NEXT
9:	rts	pc

/ следующий такт: тон из кольца, решение о плотном цикле; портит r0
ntick:	mov	r1, -(sp)
1:	sub	$TICK, @$RT
	inc	@$RH
	mov	@$RT, r0
	sub	@$0177710, r0
	cmp	r0, $TICK
	blo	2f
	cmp	r0, $01000		/ отстали больше чем на 5 тактов (чтение файла) —
	blo	1b			/ время с текущего
	mov	@$0177710, @$RT
2:	add	$0112, @$TOK		/ запас плотного цикла: 3/4 такта на такт
	cmp	@$TOK, $TOKMAX
	blos	1f
	mov	$TOKMAX, @$TOK
1:	clr	r1
	mov	@$RW, r0
	sub	@$RH, r0
	ble	3f			/ секвенсор не успел: тишина
	mov	@$RH, r0
	bic	$0177700, r0
	asl	r0
	mov	_snd_ring(r0), r1
	br	4f
3:	tstb	@$_snd_play
	bne	4f
	clrb	@$_snd_active		/ звук кончился, кольцо сыграно
4:	mov	r1, @$_snd_cur
	bne	5f
	clr	@$ON
	br	9f
5:	mov	r1, r0			/ фон: октавой-двумя ниже, пока полупериод мал
6:	cmp	r0, $BGMIN
	bhis	7f
	asl	r0
	br	6b
7:	jsr	pc, sethp
	tst	@$IDLE
	bne	9f
	cmp	@$TOK, $TICK
	blo	9f
	sub	$TICK, @$TOK		/ плотный цикл: нота такта точно, до конца такта
	mov	r1, r0
	jsr	pc, sethp
	jsr	pc, fgplay
9:	mov	(sp)+, r1
	rts	pc

/ фронты до конца такта на регистрах (до ~8 кГц): r2 — срок, r3 — его дробь,
/ r4, r5 — полупериод, r1 — уровень; конец такта — на стеке; портит r0
fgplay:	mov	r1, -(sp)
	mov	r2, -(sp)
	mov	r3, -(sp)
	mov	r4, -(sp)
	mov	r5, -(sp)
	mov	@$NEXT, r2
	mov	@$NEXTF, r3
	mov	@$HPI, r4
	mov	@$HPF, r5
	mov	@$LVL, r1
	mov	@$RT, r0
	sub	$TICK, r0
	mov	r0, -(sp)		/ отсчёт конца такта
1:	mov	@$0177710, r0
	sub	(sp), r0
	tst	r0
	ble	3f			/ такт кончился
	mov	@$0177710, r0
	sub	r2, r0
	tst	r0
	bgt	1b			/ срок фронта не подошёл
	mov	r1, @$0177716
	mov	$0100, r0
	xor	r0, r1
	sub	r5, r3			/ следующий срок (8.8)
	sbc	r2
	sub	r4, r2
	br	1b
3:	tst	(sp)+
	mov	r2, @$NEXT
	mov	r3, @$NEXTF
	mov	r1, @$LVL
	mov	(sp)+, r5
	mov	(sp)+, r4
	mov	(sp)+, r3
	mov	(sp)+, r2
	mov	(sp)+, r1
	rts	pc

/ полупериод r0 (8.8) -> HPI, HPF; динамик молчал — запуск; портит r0
sethp:	mov	r0, -(sp)
	clrb	(sp)
	swab	(sp)
	mov	(sp)+, @$HPI
	swab	r0
	clrb	r0
	mov	r0, @$HPF
	tst	@$ON
	bne	1f
	mov	$0100, @$LVL
	clr	@$NEXTF
	mov	@$0177710, r0
	sub	@$HPI, r0
	mov	r0, @$NEXT
	inc	@$ON
1:	rts	pc

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
