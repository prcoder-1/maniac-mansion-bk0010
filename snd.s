/ Звук на динамике (общая копия кода). У БК нет генератора тона: бит 6 регистра
/ 0177716 переключает сам процессор, по таймеру ВИ1 (23437.5 Гц, считает вниз).
/
/ Секвенсор Player_V2 (sound.c, банк D) идёт вперёд времени и кладёт в кольцо
/ _snd_ring точный полупериод тона каждого такта 236 Гц (8.8 отсчёта, 0 —
/ тишина). snd_tick, который зовут горячие циклы и межбанковые вызовы, сам
/ переключает такты по таймеру, берёт тон такта из кольца и ставит фронты:
/ фронт ближе SNAP отсчётов — дождавшись срока, точно (высокие ноты — октавой-
/ двумя ниже: так часто вызовы не успевают). До срока DL вызов почти ничего не
/ стоит. Когда в кольце меньше LOW тактов, зовётся секвенсор (snd_seq в main.c
/ по вектору _snd_seqv) — откуда угодно: _cur_page на это время = _map_now, и
/ его farret вернёт подключённую страницу.
	.globl _snd_tick, _snd_spin, _sndst, _map_now, _cur_page
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
	DL = _sndst+022			/ срок следующей работы snd_tick (отсчёт таймера)
	TICK = 0143			/ такт 236 Гц = 99 отсчётов
	LOW = 020			/ дозаполнять кольцо, если в нём меньше 16 тактов
	BGMIN = 014000			/ полупериод фона не меньше 24 отсчётов (8.8)
	SNAP = 030			/ фронт ближе SNAPV (до 24 отсчётов, 1 мс) — дождаться
	SNAPV = 023720			/ окно: четверть полупериода, не больше SNAP (sethp)
	SPINV = 023722			/ ждать за вызов не дольше SNAPV + 8

/ void snd_tick(void): все регистры сохраняются. До срока DL (конец такта или
/ фронт минус SNAP) — сразу выход
_snd_tick:
.ifdef GAPLOG
/ отладка: промежутки между вызовами длиннее GLIM отсчётов — в журнал GBUF:
/ слово — байт записей, записи {возврат прошлого вызова, его страница, его r1
/ (в farcall — номер функции), слово под возвратом (возврат в Си из far_*), этот
/ возврат, 6(sp), 14(sp), промежуток от конца прошлого вызова}; 023706.. — прошлый вызов
	GBUF = 035352		/ ниже дна стека Си (036200): только для отладки
	GLR = 023706
	GLM = 023710
	GLX = 023712
	GLY = 023714
	GLT = 023716
	GLIM = 0200
	mov	r0, -(sp)
	mov	r1, -(sp)
	mov	@$GLT, r1
	sub	@$0177710, r1		/ прошло (таймер считает вниз)
	cmp	r1, $GLIM
	blo	1f
	mov	@$GBUF, r0
	cmp	r0, $0520		/ 21 запись (до 036072)
	bhis	1f
	add	$GBUF+2, r0
	mov	@$GLR, (r0)+
	mov	@$GLM, (r0)+
	mov	@$GLX, (r0)+
	mov	@$GLY, (r0)+
	mov	4(sp), (r0)+
	mov	6(sp), (r0)+
	mov	014(sp), (r0)+
	mov	r1, (r0)+
	add	$020, @$GBUF
1:	mov	4(sp), @$GLR
	mov	014(sp), @$GLY		/ far_rd/far_wr: возврат в Си
	mov	_cur_page, @$GLM		/ банк вызывающего Си-кода
	mov	(sp)+, r1
	mov	r1, @$GLX
	mov	(sp)+, r0
.endif
	tstb	@$_snd_active
	beq	9f
	mov	r0, -(sp)
	mov	@$0177710, r0
	sub	@$DL, r0
	dec	r0			/ до срока 1..TICK отсчётов — делать нечего
	cmp	r0, $TICK
	blo	8f
	mov	@$RT, r0
	sub	@$0177710, r0
	cmp	r0, $TICK
	bhis	5f
1:	tst	@$ON			/ фронт, если срок близко
	beq	7f
	mov	@$0177710, r0
	sub	@$NEXT, r0
	cmp	r0, @$SNAPV
	bgt	7f
	jsr	pc, edge
7:	jsr	pc, setdl
8:	mov	(sp)+, r0
9:
.ifdef GAPLOG
	mov	@$0177710, @$GLT	/ промежуток — от конца вызова: ожидание фронта не в счёт
.endif
	rts	pc
5:	tst	@$ON			/ ближний фронт — до работы такта
	beq	51f
	jsr	pc, edge
51:	jsr	pc, ntick		/ новый такт; дозаполнить кольцо
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

/ срок DL: что раньше — конец такта или фронт минус SNAP; портит r0
setdl:	mov	@$RT, r0
	sub	$TICK, r0		/ конец такта
	tst	@$ON
	beq	2f
	mov	r0, -(sp)
	mov	@$NEXT, r0
	add	@$SNAPV, r0
	sub	(sp), r0		/ таймер считает вниз: раньше — больше
	tst	r0			/ знак разности по модулю 2^16: V после sub не нужен
	ble	1f
	add	r0, (sp)
1:	mov	(sp)+, r0
2:	mov	r0, @$DL
	rts	pc

/ фронты, срок которых прошёл или наступит не позже чем через SNAP отсчётов:
/ ближние — дождаться и поставить точно (ждать за вызов не больше SPIN); портит r0
edge:	mov	r1, -(sp)
	mov	@$0177710, r1		/ начало ожидания
1:	mov	r1, r0
	sub	@$0177710, r0
	cmp	r0, @$SPINV
	bhis	9f
	mov	@$0177710, r0
	sub	@$NEXT, r0		/ до срока (знак — по модулю 2^16)
	tst	r0			/ без этого ble по N^V: у границы 0100000 таймера
	ble	2f			/ фронт «в прошлом» виделся будущим — тишина до 1.4 с
	cmp	r0, @$SNAPV
	ble	1b
	br	9f
2:	mov	@$LVL, r0
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
	bgt	1b
	mov	@$0177710, r0		/ опоздали больше чем на полупериод — с текущего
	sub	@$HPI, r0
	mov	r0, @$NEXT
	br	1b
9:	mov	(sp)+, r1
	rts	pc

/ следующий такт: тон из кольца (высокий — октавой-двумя ниже); портит r0
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
2:	clr	r1
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
4:	cmp	r1, @$_snd_cur		/ тон тот же и звучит — полупериод уже стоит
	bne	41f
	tst	r1
	beq	41f
	tst	@$ON
	bne	9f
41:	mov	r1, @$_snd_cur
	bne	5f
	clr	@$ON
	br	9f
5:	mov	r1, r0			/ полупериод меньше BGMIN — октавой-двумя ниже:
6:	cmp	r0, $BGMIN		/ чаще фронтов вызовы не успевают
	bhis	7f
	asl	r0
	br	6b
7:	jsr	pc, sethp
9:	mov	(sp)+, r1
	rts	pc

/ полупериод r0 (8.8) -> HPI, HPF; динамик молчал — запуск; портит r0
sethp:	mov	r0, -(sp)
	clrb	(sp)
	swab	(sp)
	mov	(sp), @$HPI
	asr	(sp)			/ окно ожидания: высокий тон — меньше (процессор)
	asr	(sp)
	cmp	(sp), $SNAP
	blos	2f
	mov	$SNAP, (sp)
2:	mov	(sp), @$SNAPV
	add	$010, (sp)
	mov	(sp)+, @$SPINV
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
