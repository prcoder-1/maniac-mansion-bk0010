/* Звук (банк C): очередь Sound (sound.cpp) и Player_V1 для PC-спикера
 * (players/player_v1.cpp, без PCjr). Звук v1 — куски: ноты (0), свип частоты (1),
 * тон (2) и шум (3) переключением динамика на каждом такте плеера; такт плеера —
 * _mplex отсчётов PIT (1193180 Гц). У БК нет генератора тона: секвенсор идёт
 * вперёд времени тактами кольца snd.s (236 Гц, 5040 отсчётов PIT) и кладёт в
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

/* состояние Player_V1 (смещения — от cur_addr) */
static uint16_t next_chunk, repeat_chunk;   /* 0 — нет */
static uint16_t chunk_type, time_left, repeat_ctr;
static uint16_t start, end, freq, random_lsr;
static int16_t delta;
static int8_t forced_level;
static uint16_t mplex2;    /* такт плеера в парах отсчётов PIT (_mplex / 2) */
static uint16_t acc2;      /* непрожитое время, пары отсчётов PIT */

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

/* Player_V1::nextSpeakerCmd */
static void nextSpeakerCmd(void)
{
    switch (chunk_type)
    {
    case 0:
        if (--time_left) return;
        time_left = chunk_word();
        if (time_left == 0xfffb) time_left = chunk_word();
        if (time_left == 0) parseSpeakerChunk();
        else freq = chunk_word();
        break;
    case 1:
        freq += (uint16_t)delta;
        if (freq == end)
        {
            if (!--repeat_ctr)
            {
                parseSpeakerChunk();
                return;
            }
            freq = start;
        }
        break;
    case 2:
        start += (uint16_t)delta;
        if (start == end)
        {
            parseSpeakerChunk();
            return;
        }
        set_mplex(start);
        forced_level = (int8_t)-forced_level;
        break;
    case 3:
    {
        start += (uint16_t)delta;
        if (start == end)
        {
            parseSpeakerChunk();
            return;
        }
        uint16_t lsr = random_lsr + 0x9248;
        lsr = (uint16_t)((lsr >> 3) | (lsr << 13));
        random_lsr = lsr;
        set_mplex((start & lsr) | 0x180);
        forced_level = (int8_t)-forced_level;
        break;
    }
    }
}

/* Player_V1::chainSound (спикер: куски — с _current_data + 4) */
static void chainSound(uint16_t nr, uint8_t pg, uint16_t addr)
{
    trace_sound("chain", nr);
    cur_nr = nr;
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

/* время плеера вперёд на n пар отсчётов PIT (такты по _mplex) */
static void advance(uint16_t n)
{
    acc2 += n;
    for (uint8_t guard = 255; cur_nr && acc2 >= mplex2; guard--)
    {
        if (!guard)   /* больше 255 тактов за раз — не догоняем */
        {
            acc2 = 0;
            return;
        }
        acc2 -= mplex2;
        nextSpeakerCmd();
    }
    if (!cur_nr) acc2 = 0;
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
enum { S_ON = 0, S_RT = 6, S_RH, S_RW };
#define AHEAD 32   /* кольцо впереди времени не больше чем на 32 такта */

/* делитель PIT -> полупериод 8.8 в отсчётах ВИ1: d * 2.5156 */
static uint16_t tone_hp(uint16_t d)
{
    if (!d) return 0;
    if (d > 26000) return 0xffff;
    return (uint16_t)((d << 1) + (d >> 1) + (d >> 6));
}

/* до 4 тактов кольца вперёд (sound_fill) */
static void seq_fill(void)
{
    for (uint8_t k = 0; k < 4 && cur_nr; k++)
    {
        int16_t ahead = (int16_t)(sndst[S_RW] - sndst[S_RH]);
        if (ahead >= AHEAD) break;
        advance(2520);   /* такт кольца: 99 отсчётов ВИ1 = 5040 отсчётов PIT */
        if (ahead >= 0) snd_ring[sndst[S_RW] & 63] = tone_hp(tone_divisor());   /* отстали — не пишем */
        sndst[S_RW]++;
    }
    snd_play = cur_nr != 0;
}

/* звук сменился (старт, стоп): кольцо — с текущего такта заново; молчало — время
 * тактов с текущего момента */
static void ring_restart(void)
{
    if (!snd_active)
    {
        sndst[S_ON] = 0;
        sndst[S_RT] = TMR_COUNT;
        sndst[S_RH] = 0;
        sndst[S_RW] = 0;
        snd_active = 1;
    }
    else if ((int16_t)(sndst[S_RW] - sndst[S_RH]) > 1)
        sndst[S_RW] = sndst[S_RH] + 1;
    snd_play = cur_nr != 0;
}
#endif

/* Player_V1::stopAllSounds */
static void player_stopAll(void)
{
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

/* Player_V1::getMusicTimer: в v1 таймера музыки нет */
uint16_t sound_music_timer(void)
{
    return 0;
}

#ifdef HOST
uint16_t snd_elapsed(void);

/* продвинуть плеер до текущего момента (хост: по джиффи, 19886 отсчётов PIT) */
void sound_update(void)
{
    uint16_t n = snd_elapsed();
    while (n--) advance(9943);
    tone_div = tone_divisor();
}
#else
/* такты вперёд в кольцо snd.s (зовёт snd_tick через snd_seq, когда в кольце
 * мало, и sound_process) */
void sound_fill(void)
{
    LOCK();
    seq_fill();
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
