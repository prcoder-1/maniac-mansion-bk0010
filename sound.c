/* Звук (банк C): очередь Sound (sound.cpp) и Player_V2 для PC-спикера
 * (players/player_v2base.cpp, player_v2.cpp). Секвенсор 236 Гц идёт по таймеру;
 * результат — делитель PIT звучащего канала в tone_div (0 — тишина). Меандр
 * играет ядро в простое цикла: у БК нет генератора тона. */
#include "scumm.h"

#define HEADER_LEN 4   /* GF_OLD_BUNDLE */

#ifdef HOST
void trace_sound(const char *ev, uint16_t nr);
#else
#define trace_sound(ev, nr) do { } while (0)
#endif

/* player_v2base.h: channel_data, слова 0..24 */
enum
{
    C_TIME_LEFT, C_NEXT_CMD, C_BASE_FREQ, C_FREQ_DELTA, C_FREQ, C_VOLUME, C_VOLUME_DELTA,
    C_TEMPO, C_INTER_NOTE_PAUSE, C_TRANSPOSE, C_NOTE_LENGTH, C_HULL_CURVE, C_HULL_OFFSET,
    C_HULL_COUNTER, C_FREQMOD_TABLE, C_FREQMOD_OFFSET, C_FREQMOD_INCR, C_FREQMOD_MULTIPLIER,
    C_FREQMOD_MODULO, C_UNKNOWN, C_MUSIC_TIMER = 23, C_MUSIC_SCRIPT_NR, C_WORDS,
    C_FM_T = C_WORDS, C_FM_M   /* отложенная модуляция freq (next_freqs) */
};

static const uint8_t note_lengths[] = {
    0,
    0, 0, 2,
    0, 3, 4,
    5, 6, 8,
    9, 12, 16,
    18, 24, 32,
    36, 48, 64,
    72, 96
};

static const uint16_t hull_offsets[] = {
    0, 12, 24, 36, 48, 60,
    72, 88, 104, 120, 136, 256,
    152, 164, 180
};

const int16_t hulls[] = {   /* и sndseq.s */
    3, -1, 0, 0, 0, 0, 0, 0,
    0, -1, 0, 0,
    3, -1, 0, 32, 0, -1, 0, 0,
    0, -1, 0, 0,
    3, -1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0,
    3, -1, 0, 2, 0, -1, 0, 0,
    0, -1, 0, 0,
    3, -1, 0, 6, 0, -1, 0, 0,
    0, -1, 0, 0,
    3, -1, 0, 16, 0, -1, 0, 0,
    0, -1, 0, 0,
    (int16_t)60000, -1, -1000, 20, 0, 0, 0, 0,
    (int16_t)40000, -1, -5000, 5, 0, -1, 0, 0,
    (int16_t)50000, -1, 0, 8, 30000, -1, 0, 0,
    28000, -1, -5000, 5, 0, -1, 0, 0,
    (int16_t)60000, -1, -2000, 16, 0, 0, 0, 0,
    28000, -1, -6000, 5, 0, -1, 0, 0,
    (int16_t)55000, -1, 0, 8, (int16_t)35000, -1, 0, 0,
    (int16_t)40000, -1, -2000, 10, 0, -1, 0, 0,
    (int16_t)60000, -1, 0, 4, -2000, 8, 0, 0,
    (int16_t)40000, -1, -6000, 5, 0, -1, 0, 0,
    0, -1, 150, 340, -150, 340, 0, -1,
    0, -1, 0, 0,
    20000, -1, 4000, 7, 1000, 15, 0, 0,
    (int16_t)35000, -1, -2000, 15, 0, -1, 0, 0,
    (int16_t)35000, -1, 500, 20, 0, 0, 0, 0,
    (int16_t)45000, -1, -500, 60, 0, -1, 0, 0,
    (int16_t)44000, -1, -4400, 10, 0, -1, 0, 0,
    0, -1, 0, 0,
    (int16_t)53000, -1, -5300, 10, 0, -1, 0, 0,
    0, -1, 0, 0,
    (int16_t)63000, -1, -6300, 10, 0, -1, 0, 0,
    0, -1, 0, 0,
    (int16_t)44000, -1, -1375, 32, 0, -1, 0, 0,
    0, -1, 0, 0,
    (int16_t)53000, -1, -1656, 32, 0, -1, 0, 0,
    0, -1, 0, 0,
    (int16_t)63000, -1, -1968, 32, 0, -1, 0, 0,
    0, -1, 0, 0,
    (int16_t)44000, -1, -733, 60, 0, -1, 0, 0,
    0, -1, 0, 0,
    (int16_t)53000, -1, -883, 60, 0, -1, 0, 0,
    0, -1, 0, 0,
    (int16_t)63000, -1, -1050, 60, 0, -1, 0, 0,
    0, -1, 0, 0,
    (int16_t)44000, -1, -488, 90, 0, -1, 0, 0,
    0, -1, 0, 0,
    (int16_t)53000, -1, -588, 90, 0, -1, 0, 0,
    0, -1, 0, 0,
    (int16_t)63000, -1, -700, 90, 0, -1, 0, 0,
    0, -1, 0, 0
};

static const uint16_t freqmod_lengths[] = { 0x1000, 0x1000, 0x20, 0x2000, 0x1000 };
static const uint16_t freqmod_offsets[] = { 0, 0x100, 0x200, 0x302, 0x202 };

#include "freqmod.h"   /* freqmod_table[0x502] */

static const uint16_t spk_freq_table[12] = {
    36484, 34436, 32503, 30679, 28957, 27332,
    25798, 24350, 22983, 21693, 20476, 19326
};

/* данные звука — в куче страниц СМК: (страница, адрес); указатели скрипта — смещения */
static uint16_t cur_nr, next_nr;
static uint8_t cur_pg, next_pg;
static uint16_t cur_addr, next_addr;
static uint16_t retaddr;
/* 5-й канал — для «очистки» каналов вне 0..3 (см. 0xfd); шаг 32 слова, а не
 * 25 — индекс канала сдвигом, без программного умножения */
#define C_STRIDE 32
static uint16_t ch[5][C_STRIDE];

/* копия звучащего звука в банке (самый большой — 5876 байт): секвенсор читает
 * её, а не чужую страницу. Копируется в безопасной точке (sound_process): far_cp
 * идёт через _bounce, занятый proc3, а секвенсор зовётся и из него (snd_tick) */
#define SBUF_N 5888
static uint8_t sbuf[SBUF_N];
static uint8_t cur_buf;   /* sbuf — копия cur_nr */

static uint8_t rd(uint16_t off)
{
    if (cur_buf) return sbuf[off];
    return far_rb(cur_pg, cur_addr + off);
}

static uint16_t rdw(uint16_t off)
{
    if (cur_buf) return (uint16_t)(sbuf[off] | (sbuf[off + 1] << 8));
    return far_rw(cur_pg, cur_addr + off);
}

static void buffer_sound(void)
{
#ifndef HOST   /* на хосте страницы банков не моделируются */
    if (cur_buf || !cur_pg) return;
    uint16_t n = far_rw(cur_pg, cur_addr);
    if (n > SBUF_N) return;
    far_cp(PG_BANKC, (uint16_t)sbuf, cur_pg, cur_addr, n);
    cur_buf = 1;
#endif
}

static void clear_channel(uint8_t i)
{
    for (uint8_t k = 0; k < C_STRIDE; k++) ch[i][k] = 0;
}

/* Player_V2Base::chainSound (на спикере смещение — header_len + 2) */
static void chainSound(uint16_t nr, uint8_t pg, uint16_t addr)
{
    trace_sound("chain", nr);
    cur_nr = nr;
    cur_pg = pg;
    cur_addr = addr;
    cur_buf = 0;
    for (uint8_t i = 0; i < 4; i++)
    {
        clear_channel(i);
        ch[i][C_MUSIC_SCRIPT_NR] = nr;
        if (pg)
        {
            ch[i][C_NEXT_CMD] = rdw(HEADER_LEN + 2 + 2 * i);
            if (ch[i][C_NEXT_CMD]) ch[i][C_TIME_LEFT] = 1;
        }
    }
}

static void chainNextSound(void)
{
    if (next_nr)
    {
        chainSound(next_nr, next_pg, next_addr);
        next_nr = 0;
        next_pg = 0;
    }
}

/* Player_V2Base::execute_cmd */
void execute_cmd(uint16_t *channel)
{
    uint16_t value;
    int16_t offset;
    uint16_t sp;   /* script_ptr */
    uint16_t *current_channel = channel, *dest_channel;

    if (channel[C_NEXT_CMD] == 0) goto check_stopped;
    sp = channel[C_NEXT_CMD];

    for (;;)
    {
        uint8_t opcode = rd(sp++);
        if (opcode >= 0xf8)
        {
            switch (opcode)
            {
            case 0xf8:   /* set hull curve */
                channel[C_HULL_CURVE] = hull_offsets[rd(sp) / 2];
                sp++;
                break;
            case 0xf9:   /* set freqmod curve */
                channel[C_FREQMOD_TABLE] = freqmod_offsets[rd(sp) / 4];
                channel[C_FREQMOD_MODULO] = freqmod_lengths[rd(sp) / 4];
                sp++;
                break;
            case 0xfd:   /* clear other channel */
                value = rdw(sp) / (C_WORDS * 2);
                sp += 2;
                if (value >= 5) value = 4;
                channel = ch[value];
                /* fall through */
            case 0xfa:   /* clear current channel */
                for (uint8_t k = C_NEXT_CMD; k <= C_FREQMOD_MODULO; k++)
                    if (k != C_TEMPO) channel[k] = 0;
                channel[C_FM_M] = 0;
                break;
            case 0xfb:   /* ret from subroutine */
                sp = retaddr;
                break;
            case 0xfc:   /* call subroutine */
                offset = (int16_t)rdw(sp);
                sp += 2;
                retaddr = sp;
                sp = (uint16_t)offset;
                break;
            case 0xfe:   /* loop music */
                opcode = rd(sp++);
                offset = (int16_t)rdw(sp);
                sp += 2;
                if (!channel[opcode / 2] || --channel[opcode / 2]) sp += (uint16_t)offset;
                break;
            case 0xff:   /* set parameter */
                opcode = rd(sp++);
                value = rdw(sp);
                /* в ScummVM индекс до 127 выходит за канал (в соседние) — здесь не пишем */
                if (opcode / 2 < C_WORDS) channel[opcode / 2] = value;
                if (opcode / 2 == C_FREQ) channel[C_FM_M] = 0;
                sp += 2;
                if (opcode == 0) goto end;
                break;
            }
        }
        else
        {
            for (;;)
            {
                int16_t note, octave;
                uint8_t is_last_note;
                dest_channel = ch[(opcode >> 5) & 3];
                if (!(opcode & 0x80))
                {
                    uint16_t tempo = channel[C_TEMPO];
                    if (!tempo) tempo = 1;
                    channel[C_TIME_LEFT] = tempo * note_lengths[opcode & 0x1f];
                    note = rd(sp++);
                    is_last_note = note & 0x80;
                    note &= 0x7f;
                    if (note == 0x7f) goto end;
                }
                else
                {
                    channel[C_TIME_LEFT] = (uint16_t)(((opcode & 7) << 8) | rd(sp++));
                    if (opcode & 0x10) goto end;
                    is_last_note = 0;
                    note = rd(sp++) & 0x7f;
                }
                dest_channel[C_TIME_LEFT] = channel[C_TIME_LEFT];
                dest_channel[C_NOTE_LENGTH] = channel[C_TIME_LEFT] - dest_channel[C_INTER_NOTE_PAUSE];
                note = (int16_t)(note + dest_channel[C_TRANSPOSE]);
                while (note < 0) note += 12;
                for (octave = 0; note >= 12; octave++) note -= 12;   /* note / 12, note % 12 */
                dest_channel[C_HULL_OFFSET] = 0;
                dest_channel[C_HULL_COUNTER] = 1;
                dest_channel[C_FREQ] = dest_channel[C_BASE_FREQ] = spk_freq_table[note] >> octave;
                dest_channel[C_FM_M] = 0;
                if (is_last_note) goto end;
                opcode = rd(sp++);
            }
        }
    }

end:
    channel = current_channel;
    if (channel[C_TIME_LEFT])
    {
        channel[C_NEXT_CMD] = sp;
        return;
    }
    channel[C_NEXT_CMD] = 0;

check_stopped:
    for (uint8_t i = 0; i < 4; i++)
        if (ch[i][C_TIME_LEFT]) return;
    trace_sound("end", cur_nr);
    cur_nr = 0;
    cur_pg = 0;
    chainNextSound();
}

/* snd.s: int8 * uint16 / 256 с усечением к нулю */
int16_t fm_scale(int16_t t, uint16_t m);

#ifdef HOST   /* на БК — sndseq.s */
/* Player_V2Base::next_freqs */
static void next_freqs(uint16_t *channel)
{
    channel[C_VOLUME] += channel[C_VOLUME_DELTA];
    channel[C_BASE_FREQ] += channel[C_FREQ_DELTA];
    channel[C_FREQMOD_OFFSET] += channel[C_FREQMOD_INCR];
    if (channel[C_FREQMOD_OFFSET] > channel[C_FREQMOD_MODULO])
        channel[C_FREQMOD_OFFSET] -= channel[C_FREQMOD_MODULO];
    /* freq = freqmod * multiplier / 256 + base_freq нужна только звучащему
     * каналу: здесь — база и входы модуляции этого такта, умножение — в
     * update_tone (пока freq не перезапишет нота или параметр) */
    channel[C_FREQ] = channel[C_BASE_FREQ];
    channel[C_FM_T] = (uint16_t)freqmod_table[channel[C_FREQMOD_TABLE] + (channel[C_FREQMOD_OFFSET] >> 4)];
    channel[C_FM_M] = channel[C_FREQMOD_MULTIPLIER];
    if (channel[C_NOTE_LENGTH] && !--channel[C_NOTE_LENGTH])
    {
        channel[C_HULL_OFFSET] = 16;
        channel[C_HULL_COUNTER] = 1;
    }
    if (!--channel[C_TIME_LEFT]) execute_cmd(channel);
    if (channel[C_HULL_COUNTER] && !--channel[C_HULL_COUNTER])
    {
        for (;;)
        {
            const int16_t *hull_ptr = hulls + channel[C_HULL_CURVE] + channel[C_HULL_OFFSET] / 2;
            if (hull_ptr[1] == -1)
            {
                channel[C_VOLUME] = (uint16_t)hull_ptr[0];
                if (hull_ptr[0] == 0) channel[C_VOLUME_DELTA] = 0;
                channel[C_HULL_OFFSET] += 4;
            }
            else
            {
                channel[C_VOLUME_DELTA] = (uint16_t)hull_ptr[0];
                channel[C_HULL_COUNTER] = (uint16_t)hull_ptr[1];
                channel[C_HULL_OFFSET] += 4;
                break;
            }
        }
    }
}

/* Player_V2Base::nextTick (счётчик _music_timer — только для v3) */
static void nextTick(void)
{
    for (uint8_t i = 0; i < 4; i++)
        if (ch[i][C_TIME_LEFT]) next_freqs(ch[i]);
}
#else
void seq_ticks(uint16_t *ch0, uint16_t n);
#endif

#ifndef HOST
#define TMR_COUNT (*(volatile uint16_t *)0177710)
/* sys.ld: тон snd.s — ON, LVL, NEXT, NEXTF, HPI, HPF (звучащий полупериод), XI, XF
 * (точный), BI, BF (для фона), FG (main.c: плотный цикл — звучит точный) */
extern volatile uint16_t sndst[11];
#else
#define TMR_COUNT 0
static uint16_t sndst[11];
#endif

/* тон d (делитель PIT, 0 — тишина) -> полупериод в отсчётах таймера ВИ1 с дробью
 * 8.8 = d / 2386364 * 23437.5 * 256 = d * 2.5143. В фоне фронты запаздывают до
 * промежутка между snd_tick (~1 мс): ноты выше играются на октаву-две ниже */
#define BG_MIN 24
static uint16_t out_div;
static void tone_out(uint16_t d)
{
    if (d == out_div) return;
    out_div = d;
    if (!d)
    {
        sndst[0] = 0;
        return;
    }
    uint16_t hp = d > 26000 ? 0xFFFF : (uint16_t)(d * 2 + (d >> 1) + (d >> 6)), bg = hp;
    while (bg < (BG_MIN << 8)) bg <<= 1;
    sndst[6] = hp >> 8;
    sndst[7] = (uint16_t)(hp << 8);
    sndst[8] = bg >> 8;
    sndst[9] = (uint16_t)(bg << 8);
    if (sndst[10]) bg = hp;
    sndst[4] = bg >> 8;
    sndst[5] = (uint16_t)(bg << 8);
    if (!sndst[0])
    {
        sndst[1] = 0100;
        sndst[3] = 0;
        sndst[2] = (uint16_t)(TMR_COUNT - sndst[4]);
        sndst[0] = 1;
    }
}

/* Player_V2::generateSpkSamples: звучит первый канал с громкостью;
 * snd_active — играет ли звук (ядро тогда продвигает секвенсор в работе цикла) */
static void update_tone(void)
{
    snd_active = cur_nr != 0;
    for (uint8_t i = 0; i < 4; i++)
        if (ch[i][C_VOLUME] && ch[i][C_TIME_LEFT])
        {
            uint16_t *c = ch[i];
            uint16_t f = c[C_FREQ];
            if (c[C_FM_M]) f += (uint16_t)fm_scale((int16_t)c[C_FM_T], c[C_FM_M]);
            tone_div = f ? f : 1;
            tone_out(tone_div);
            return;
        }
    tone_div = 0;
    tone_out(0);
}

/* Player_V2::stopAllSounds */
static void player_stopAll(void)
{
    for (uint8_t i = 0; i < 4; i++) clear_channel(i);
    next_nr = cur_nr = 0;
    next_pg = cur_pg = 0;
}

/* Player_V2::stopSound */
static void player_stop(uint16_t nr)
{
    trace_sound("stop", nr);
    if (next_nr == nr)
    {
        next_nr = 0;
        next_pg = 0;
    }
    if (cur_nr == nr)
    {
        for (uint8_t i = 0; i < 4; i++) clear_channel(i);
        cur_nr = 0;
        cur_pg = 0;
        chainNextSound();
    }
}

/* Player_V2::startSound */
static void player_start(uint16_t nr)
{
    uint8_t pg = res_get(rtSound, nr);
    uint16_t addr = res_addr;
    trace_sound("start", nr);
    if (!pg) return;
    uint8_t cprio = cur_pg ? rd(HEADER_LEN) : 0;
    uint8_t prio = far_rb(pg, addr + HEADER_LEN);
    uint8_t nprio = next_pg ? far_rb(next_pg, next_addr + HEADER_LEN) : 0;
    uint8_t restartable = far_rb(pg, addr + HEADER_LEN + 1);

    if (!cur_nr || cprio <= prio)
    {
        uint16_t tnr = cur_nr;
        uint8_t tprio = cprio, tpg = cur_pg;
        uint16_t taddr = cur_addr;
        chainSound(nr, pg, addr);
        nr = tnr;
        prio = tprio;
        pg = tpg;
        addr = taddr;
        restartable = pg ? far_rb(pg, addr + HEADER_LEN + 1) : 0;
    }
    if (!cur_nr)
    {
        nr = 0;
        next_nr = 0;
        next_pg = 0;
    }
    if (nr != cur_nr && restartable && (!next_nr || nprio <= prio))
    {
        next_nr = nr;
        next_pg = pg;
        next_addr = addr;
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
    player_stop(sound);
    for (uint8_t i = 0; i < sizeof que2; i++)
        if (que2[i] == sound) que2[i] = 0;
    update_tone();
    UNLOCK();
}

/* Sound::stopAllSounds */
void stopAllSounds(void)
{
    LOCK();
    que2pos = 0;
    for (uint8_t i = 0; i < sizeof que2; i++) que2[i] = 0;
    player_stopAll();
    update_tone();
    UNLOCK();
}

uint16_t sound_music_timer(void)
{
    return ch[0][C_MUSIC_TIMER];
}

/* такты секвенсора по таймеру ВИ1 (23437.5 Гц): такт 236 Гц = 99.3 отсчёта,
 * берём 99 (темп быстрее на 0.3%) */
#ifndef HOST
static uint16_t tm_prev, tm_acc;
static uint16_t snd_elapsed(void)
{
    uint16_t cur = TMR_COUNT, d = tm_prev - cur, n = 0;
    tm_prev = cur;
    if (d > 23400) d = 23400;   /* не дольше секунды (после чтения с диска) */
    tm_acc += d;
    uint16_t a = tm_acc;
    while (a >= 99)
    {
        a -= 99;
        __asm__("" : "+r"(a));   /* иначе gcc считает a - 99 * n через __mulhi3 */
        n++;
    }
    tm_acc = a;
    return n;
}
#else
uint16_t snd_elapsed(void);
#endif

/* продвинуть секвенсор до текущего момента (зовётся и из простоя ядра) */
void sound_update(void)
{
    LOCK();
    uint16_t n = snd_elapsed();
    if (cur_nr)
#ifdef HOST
        while (n--) nextTick();
#else
        seq_ticks(ch[0], n);
#endif
    SND_TICK();
    update_tone();
    UNLOCK();
}

/* Sound::processSoundQueues (очередь — с конца) */
void sound_process(void)
{
    sound_update();
    LOCK();
    while (que2pos)
    {
        uint8_t snd = que2[--que2pos];
        if (snd) player_start(snd);
    }
    buffer_sound();
    update_tone();
    UNLOCK();
}
