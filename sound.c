/* Звук (банк D): очередь Sound (sound.cpp) и Player_V1 для PC-спикера
 * (players/player_v1.cpp, без PCjr). Звук v1 — куски: ноты (0), свип частоты (1),
 * тон (2) и шум (3) переключением динамика на каждом такте плеера; такт плеера —
 * _mplex отсчётов PIT (1193180 Гц). У БК нет генератора тона: секвенсор идёт
 * вперёд времени тактами кольца snd.s (118 Гц, 10080 отсчётов PIT) и кладёт в
 * кольцо полупериод звучащего тона, играет его snd.s. */
#include "scumm.h"

#ifdef HOST
void trace_sound(const char *ev, uint16_t nr);
#else
#define trace_sound(ev, nr) do { } while (0)
#endif

/* данные звука — в куче страниц СМК: (страница, адрес) */
uint16_t cur_nr;
static uint16_t next_nr;
static uint8_t cur_pg, next_pg;
static uint16_t cur_addr, next_addr;   /* _current_data, _next_data: ресурс + 6 */

/* состояние Player_V1 (смещения — от cur_addr); struct v1st — и для v1sweep.s */
static uint16_t next_chunk, repeat_chunk;   /* 0 — нет */
static uint16_t time_left;
static int8_t forced_level;
struct v1st
{
    uint16_t a;      /* непрожитое время, пары отсчётов PIT */
    uint16_t m2;     /* такт плеера в парах отсчётов PIT (_mplex / 2) */
    uint16_t st, e, d, lsr, fq, rep, type;   /* _start, _end, _delta, _random_lsr, _freq, _repeat_ctr, chunk */
};
static struct v1st v1;
#define acc2 v1.a
#define mplex2 v1.m2
#define start v1.st
#define end v1.e
#define delta (*(int16_t *)&v1.d)
#define random_lsr v1.lsr
#define freq v1.fq
#define repeat_ctr v1.rep
#define chunk_type v1.type
static uint8_t hp_ok;      /* hp_v — тон нынешнего состояния плеера (seq_fill) */

static uint16_t chunk_word(void)
{
    uint16_t v = far_rw(cur_pg, cur_addr + next_chunk);
    next_chunk += 2;
    return v;
}

static void set_mplex(uint16_t m)
{
    mplex2 = m ? m >> 1 : 0x8000;   /* 0 — 65536 */
    if (!mplex2) mplex2 = 1;
}

static void chainNextSound(void);

static void sound_end(void)
{
    hp_ok = 0;
    cur_nr = 0;
    cur_pg = 0;
    freq = 0;
    forced_level = 0;
    repeat_chunk = next_chunk = 0;
    chainNextSound();
}

/* Player_V1::parseSpeakerChunk */
static void parseSpeakerChunk(void)
{
    set_mplex(3000);
    forced_level = 0;
    for (uint8_t guard = 16; guard; guard--)
    {
        chunk_type = chunk_word();
        switch (chunk_type)
        {
        case 0xfffe:
            repeat_chunk = next_chunk;
            continue;
        case 0xfffd:
            next_chunk = repeat_chunk;
            continue;
        case 0xfffc:
            continue;
        case 0:
            time_left = 1;
            set_mplex(chunk_word());
            return;
        case 1:
            set_mplex(chunk_word());
            start = chunk_word();
            end = chunk_word();
            delta = (int16_t)chunk_word();
            repeat_ctr = chunk_word();
            freq = start;
            return;
        case 2:
        case 3:
            start = chunk_word();
            end = chunk_word();
            delta = (int16_t)chunk_word();
            freq = 0;
            forced_level = -1;
            return;
        default:   /* 0xffff — конец; прочее — битые данные */
            sound_end();
            return;
        }
    }
    sound_end();   /* только метки повтора — зацикленный пустой звук */
}

/* Player_V1::nextSpeakerCmd, ноты (куски 1..3 — v1_fill) */
static void nextSpeakerCmd(void)
{
    if (--time_left) return;
    time_left = chunk_word();
    if (time_left == 0xfffb) time_left = chunk_word();
    if (time_left == 0) parseSpeakerChunk();
    else freq = chunk_word();
}

/* Player_V1::chainSound (спикер: куски — с _current_data + 4) */
static void chainSound(uint16_t nr, uint8_t pg, uint16_t addr)
{
    trace_sound("chain", nr);
    cur_nr = nr;
    hp_ok = 0;
    cur_pg = pg;
    cur_addr = addr;
    freq = 0;
    acc2 = 0;
    repeat_chunk = next_chunk = 4;
    parseSpeakerChunk();
}

static void chainNextSound(void)
{
    if (next_nr)
    {
        uint16_t nr = next_nr;
        uint8_t pg = next_pg;
        next_nr = 0;
        next_pg = 0;
        chainSound(nr, pg, next_addr);
    }
}

#ifdef HOST
/* v1sweep.s: такты плеера кусков 1..3 и n тактов кольца в snd_ring с rw (n == 0 —
 * только догнать такты плеера); -> записано | 0400, если кусок кончился. Шум —
 * средним тактом плеера ((st & ~0x180) / 2 + 0x180), в кольцо — граница st | 1 */
static uint16_t snd_ring[64];
static uint16_t v1_fill(struct v1st *p, uint16_t rw, uint16_t n)
{
    uint16_t k = 0;
    uint8_t ended = 0;
    if (n) p->a += 5040;
    for (;;)
    {
        if (p->type == 3) p->m2 = (uint16_t)(((p->st & ~0x180u) >> 2) + 0xc0);   /* средний такт шума */
        while (p->a >= p->m2)
        {
            p->a -= p->m2;
            if (p->type == 1)
            {
                p->fq += p->d;
                if (p->fq == p->e)
                {
                    if (!--p->rep)
                    {
                        ended = 1;
                        break;
                    }
                    p->fq = p->st;
                }
                continue;
            }
            p->st += p->d;
            if (p->st == p->e)
            {
                ended = 1;
                break;
            }
            if (p->type == 2) p->m2 = p->st ? (p->st >> 1 ? p->st >> 1 : 1) : 0x8000;   /* set_mplex(start) */
        }
        if (ended && (!n || k)) return k | 0400;   /* короче такта и ничего не записал — такт его тоном */
        if (!n) return k;
        uint16_t v, dv = p->type == 1 ? p->fq : p->m2 >= 0x4000 ? 0xffff : (uint16_t)(p->m2 << 2);
        if (p->type == 3) v = p->st | 1;
        else if (dv > 26000) v = 0xfffe | 2;
        else if (!dv) v = 0;
        else v = (uint16_t)((((dv << 1) + (dv >> 1) + (dv >> 6)) & ~1u) | 2);   /* бит 1: без ожидания фронта */
        snd_ring[rw++ & 63] = v;
        k++;
        if (ended) return k | 0400;
        if (!--n) return k;
        p->a += 5040;
    }
}
#else
uint16_t v1_fill(struct v1st *p, uint16_t rw, uint16_t n);
#endif

/* время плеера вперёд на n пар отсчётов PIT (такты по _mplex) */
static void advance(uint16_t n)
{
    acc2 += n;
    for (uint8_t guard = 255; cur_nr; guard--)
    {
        if (!guard)   /* больше 255 тактов за раз — не догоняем */
        {
            acc2 = 0;
            return;
        }
        if (chunk_type)
        {   /* свип, тон, шум: плотным циклом (на БК — v1sweep.s) */
            if (!(v1_fill(&v1, 0, 0) & 0400)) return;
            parseSpeakerChunk();   /* кусок кончился: следующий, остаток времени — ему */
        }
        else
        {
            if (acc2 < mplex2) return;
            acc2 -= mplex2;
            nextSpeakerCmd();
        }
        if (!(guard & 7)) SND_POLL();   /* до 255 тактов плеера на такт кольца */
    }
    acc2 = 0;
}

/* делитель PIT звучащего тона (полный период), 0 — тишина: тон канала 0 или
 * переключение динамика на каждом такте (период — два такта) */
static uint16_t tone_divisor(void)
{
    if (!cur_nr) return 0;
    if (freq) return freq;
    if (forced_level) return mplex2 >= 0x4000 ? 0xffff : (uint16_t)(mplex2 << 2);
    return 0;
}

#ifndef HOST
#define TMR_COUNT (*(volatile uint16_t *)0177710)
/* sys.ld: тон и такты snd.s — ON, LVL, NEXT, NEXTF, HPI, HPF, RT, RH, RW, DL;
 * кольцо полупериодов по тактам; флаги */
extern volatile uint16_t sndst[11];
extern volatile uint16_t snd_ring[64];
extern volatile uint8_t snd_play;
extern volatile uint16_t snd_cur;   /* sys.ld: значение такта, которое звучит (snd.s) */
extern uint16_t snd_ay;             /* sys.ld: не 0 — AY-3-8910 */
extern volatile uint16_t snd_busy;  /* sys.ld: секвенсор занят — snd_tick в него не входит */
enum { S_ON = 0, S_RT = 6, S_RH, S_RW, S_DL };
#define AHEAD 16   /* кольцо впереди времени не больше чем на 16 тактов */

/* AY-3-8910 на порту 0177714: слово — выбор регистра, байт — данные, линии
 * инверсные; первым — выбор чипа 1 TurboSound (как в Digger) */
static void ay_reg(uint8_t r, uint8_t v)
{
    volatile uint16_t *port = (volatile uint16_t *)0177714;
    *port = 0;
    *port = (uint8_t)~r;
    *(volatile uint8_t *)port = (uint8_t)~v;
}

/* делитель PIT -> период тона AY (такт 1714286 Гц, «Мультизвук»): d * 0.0898 =
 * 23/256 d, без 32-битного умножения: по старшему и младшему байту */
static uint16_t ay_tp(uint16_t d)
{
    uint16_t q = d >> 8, r = d & 0377;
    uint16_t tp = (uint16_t)((q << 4) + (q << 3) - q + ((((r << 4) + (r << 3) - r) + 0200) >> 8));
    return tp > 07777 ? 07777 : tp;
}

/* делитель PIT -> полупериод 8.8 в отсчётах ВИ1: d * 2.5156, на БК-0011М (таймер
 * 31250 Гц) d * 3.3516 — ниже 61 Гц не звучит (как ниже 46 Гц на БК-0010); тон
 * меняется редко — пересчёт только при смене делителя (на каждом такте кольца — дорого) */
static uint16_t hp_d, hp_v;
static uint16_t tone_hp(uint16_t d)
{
    if (d == hp_d) return hp_v;
    hp_d = d;
    if (!d) hp_v = 0;
    else if (snd_ay) hp_v = (uint16_t)(ay_tp(d) << 2);   /* бит 0 — признак шума */
    else if (d > (bk_11m ? 19550u : 26000u)) hp_v = 0xfffe;
    else if (bk_11m) hp_v = (uint16_t)(((d << 1) + d + (d >> 2) + (d >> 4) + (d >> 5) + (d >> 7)) & ~1u);
    else hp_v = (uint16_t)(((d << 1) + (d >> 1) + (d >> 6)) & ~1u);   /* бит 0 — признак шума */
    return hp_v;
}

/* шум на AY: период генератора шума по границе полупериодов динамика st (такты
 * PIT): та же средняя длина серии, st / 56 + 7, но не больше 31 */
static uint16_t ay_np(uint16_t st)
{
    uint16_t np = (uint16_t)((st >> 6) + (st >> 9) + 7);
    return (uint16_t)((np > 31 ? 31 : np) << 2 | 1);
}

/* такт кольца: полупериод тона 8.8 (0 — тишина; бит 1 — фронты без ожидания) или граница
 * шума | 1; на AY — период тона << 2 или период шума << 2 | 1 */
static uint16_t ring_value(void)
{
    if (cur_nr && chunk_type == 3) return snd_ay ? ay_np(start) : start | 1;
    uint16_t v = tone_hp(tone_divisor());
    return v && cur_nr && chunk_type ? v | 2 : v;   /* свип, тон: без ожидания фронта (бит 1) */
}

/* до max тактов кольца вперёд, пока впереди меньше upto (sound_fill) */
static void seq_fill(uint8_t max, int16_t upto)
{
    uint8_t resume = 0;   /* кусок кончился посреди такта: время такта уже в acc2 */
    while (max && cur_nr)
    {
        uint16_t rw = sndst[S_RW];
        int16_t ahead = (int16_t)(rw - sndst[S_RH]);
        if (ahead >= upto) break;
        uint16_t n = (uint16_t)(upto - ahead);
        if (n > max) n = max;
        if (hp_ok && ahead >= 0 && chunk_type == 0 && acc2 + 5040u < mplex2)
        {   /* такты без команды плеера: тон тот же — подряд, без обвязки */
            uint16_t a = acc2 + 5040, m = mplex2, v = hp_v;
            uint16_t e = rw + n;
            do
            {
                snd_ring[rw & 63] = v;
                a += 5040;
            } while (++rw != e && a < m);
            max -= (uint8_t)(rw - sndst[S_RW]);
            acc2 = a - 5040;
            sndst[S_RW] = rw;
        }
        else if (!resume && ahead >= 0 && chunk_type)
        {   /* свип, тон, шум: такты кольца подряд (v1sweep.s) */
            uint16_t r = v1_fill(&v1, rw, n);
            sndst[S_RW] = rw + (uint8_t)r;
            max -= (uint8_t)r;
            hp_ok = 0;
            if (r & 0400)
            {
                parseSpeakerChunk();
                resume = 1;
            }
        }
        else
        {
            advance(resume ? 0 : 5040);   /* такт кольца: 198 отсчётов ВИ1 = 10080 отсчётов PIT */
            resume = 0;
            uint16_t v = ring_value();
            hp_ok = 1;
            if (ahead >= 0) snd_ring[rw & 63] = v;   /* отстали — не пишем */
            sndst[S_RW] = rw + 1;
            max--;
        }
        SND_POLL();   /* фронты тона между тактами кольца; snd_busy не пустит в секвенсор повторно */
    }
    snd_play = cur_nr != 0;
}

/* звук сменился (старт, стоп): кольцо — со следующего такта заново; молчало —
 * время тактов с текущего момента. Тон такта snd.s берёт в его начале (ntick):
 * записанное в идущий такт RH не звучит — у звука пропадал первый такт, а щелчок
 * часов (один такт) — целиком */
static void ring_restart(void)
{
    if (!snd_active)
    {
        sndst[S_ON] = 0;
        sndst[S_RT] = sndst[S_DL] = TMR_COUNT;   /* срок — сейчас: пока звука не было, он отодвигался */
        sndst[S_RH] = 0;
        snd_active = 1;
    }
    sndst[S_RW] = sndst[S_RH] + 1;
    snd_play = cur_nr != 0;
}

/* звук на AY (on) или динамике: AY молчит (все каналы выключены), значения
 * кольца — в новом формате со следующего такта */
void sound_set_ay(uint8_t on)
{
    uint16_t lock_ = snd_busy;
    snd_busy = 1;
    if (on || snd_ay)   /* без AY порт не трогать: там может быть Covox или принтер */
    {
        ay_reg(7, 077);
        ay_reg(8, 0);
        ay_reg(9, 0);
        ay_reg(10, 0);
    }
    snd_ay = on;
    hp_d = hp_v = 0;
    hp_ok = 0;
    sndst[S_ON] = 0;
    snd_cur = 0;
    if (snd_active) ring_restart();
    snd_busy = lock_;
}

/* чтение диска, ожидание клавиши: snd_tick не зовут — AY замолкает (динамик и так
 * молчит), следующий такт снова запишет его регистры */
void sound_hold(void)
{
    if (!snd_ay) return;
    ay_reg(9, 0);
    snd_cur = 1;   /* шум с периодом 0 не бывает: такт после паузы отличается */
}
#endif

/* Player_V1::stopAllSounds */
static void player_stopAll(void)
{
    hp_ok = 0;
    next_nr = 0;
    next_pg = 0;
    cur_nr = 0;
    cur_pg = 0;
    freq = 0;
    forced_level = 0;
    repeat_chunk = next_chunk = 0;
}

/* Player_V1::stopSound */
static void player_stop(uint16_t nr)
{
    trace_sound("stop", nr);
    if (next_nr == nr)
    {
        next_nr = 0;
        next_pg = 0;
    }
    if (cur_nr == nr) sound_end();
}

/* Player_V1::startSound (спикер: данные — с ресурса + 6) */
static void player_start(uint16_t nr)
{
    uint8_t pg = res_get(rtSound, nr);
    uint16_t addr = res_addr + 6;
    trace_sound("start", nr);
    if (!pg) return;
    uint8_t cur = cur_nr ? far_rb(cur_pg, cur_addr) : 0;
    uint8_t prio = far_rb(pg, addr) & 0x7f;
    if (!cur_nr || (cur & 0x7f) <= prio)
    {
        if (cur & 0x80)   /* прерванный звук с флагом — доиграть после */
        {
            next_nr = cur_nr;
            next_pg = cur_pg;
            next_addr = cur_addr;
        }
        chainSound(nr, pg, addr);
    }
}

/* --- Sound (sound.cpp) --- */

/* состояние секвенсора меняется: snd_tick (snd.s) не должен звать его изнутри */
#ifndef HOST
extern volatile uint16_t snd_busy;   /* sys.ld */
#define LOCK() uint16_t lock_ = snd_busy; snd_busy = 1
#define UNLOCK() snd_busy = lock_
#else
#define LOCK() do { } while (0)
#define UNLOCK() do { } while (0)
#endif

static uint8_t que2[10];
static uint8_t que2pos;

/* Sound::addSoundToQueue + addSoundToQueue2 */
void addSoundToQueue(uint8_t sound)
{
    VAR(VAR_LAST_SOUND) = sound;
    if (sound && sound <= res_num(rtSound)) res_get(rtSound, sound);   /* ensureResourceLoaded */
    if (que2pos < sizeof que2) que2[que2pos++] = sound;
}

/* Sound::isSoundInQueue + Sound::isSoundRunning */
uint8_t isSoundRunning(uint8_t sound)
{
    for (uint8_t i = que2pos; i--;)
        if (que2[i] == sound) return 1;
    return sound && (cur_nr == sound || next_nr == sound);
}

/* Sound::stopSound */
void stopSound(uint8_t sound)
{
    LOCK();
    uint16_t was = cur_nr;
    player_stop(sound);
    for (uint8_t i = 0; i < sizeof que2; i++)
        if (que2[i] == sound) que2[i] = 0;
#ifndef HOST
    if (cur_nr != was) ring_restart();
#else
    (void)was;
#endif
    UNLOCK();
}

/* Sound::stopAllSounds */
void stopAllSounds(void)
{
    LOCK();
    que2pos = 0;
    for (uint8_t i = 0; i < sizeof que2; i++) que2[i] = 0;
    player_stopAll();
#ifndef HOST
    if (snd_active) ring_restart();
#endif
    UNLOCK();
}

#ifdef HOST
uint16_t snd_elapsed(void);

/* продвинуть плеер до текущего момента (хост: такты кольца 118 Гц, как на БК) */
void sound_update(void)
{
    uint16_t n = snd_elapsed();
    while (n--) advance(5040);
    tone_div = tone_divisor();
}
#else
/* такты вперёд в кольцо snd.s: из sound_process — до 4, из snd_tick (snd_seq,
 * когда в кольце мало) — до 4 */
void sound_fill(void)
{
    LOCK();
    seq_fill(4, AHEAD);
    UNLOCK();
}

void sound_fill1(void)
{
    LOCK();
    seq_fill(4, 10);   /* snd.s зовёт, когда впереди меньше 8: сразу до 10 — реже */
    UNLOCK();
}
#endif

/* Sound::processSoundQueues (очередь — с конца) */
void sound_process(void)
{
#ifdef HOST
    sound_update();
#endif
    LOCK();
    uint16_t was = cur_nr;
    while (que2pos)
    {
        uint8_t snd = que2[--que2pos];
        if (snd) player_start(snd);
    }
#ifndef HOST
    if (cur_nr != was) ring_restart();
#else
    (void)was;
#endif
    UNLOCK();
#ifndef HOST
    if (snd_active) sound_fill();
#endif
}
