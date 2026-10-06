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
    C_FREQMOD_MODULO, C_UNKNOWN, C_MUSIC_TIMER = 23, C_MUSIC_SCRIPT_NR, C_WORDS
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

static const int16_t hulls[] = {
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
/* 5-й канал — для «очистки» каналов вне 0..3 (см. 0xfd) */
static uint16_t ch[5][C_WORDS];

static uint8_t rd(uint16_t off)
{
    return far_rb(cur_pg, cur_addr + off);
}

static uint16_t rdw(uint16_t off)
{
    return far_rw(cur_pg, cur_addr + off);
}

static void clear_channel(uint8_t i)
{
    for (uint8_t k = 0; k < C_WORDS; k++) ch[i][k] = 0;
}

/* Player_V2Base::chainSound (на спикере смещение — header_len + 2) */
static void chainSound(uint16_t nr, uint8_t pg, uint16_t addr)
{
    trace_sound("chain", nr);
    cur_nr = nr;
    cur_pg = pg;
    cur_addr = addr;
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
static void execute_cmd(uint16_t *channel)
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
                /* в ScummVM индекс до 127 выходит за канал; дальше 5-го канала не пишем */
                if ((uint16_t)(channel - ch[0]) + opcode / 2 < 5 * C_WORDS) channel[opcode / 2] = value;
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
                octave = note / 12;
                note = note % 12;
                dest_channel[C_HULL_OFFSET] = 0;
                dest_channel[C_HULL_COUNTER] = 1;
                dest_channel[C_FREQ] = dest_channel[C_BASE_FREQ] = spk_freq_table[note] >> octave;
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

/* int8 * uint16 / 256 с усечением к нулю — без 32-битной арифметики */
static int16_t fm_scale(int8_t t, uint16_t m)
{
    int16_t hi = (int16_t)(t * (int16_t)(m >> 8));
    int16_t lo = (int16_t)(t * (int16_t)(m & 0xff));
    return (int16_t)(hi + lo / 256);
}

/* Player_V2Base::next_freqs */
static void next_freqs(uint16_t *channel)
{
    channel[C_VOLUME] += channel[C_VOLUME_DELTA];
    channel[C_BASE_FREQ] += channel[C_FREQ_DELTA];
    channel[C_FREQMOD_OFFSET] += channel[C_FREQMOD_INCR];
    if (channel[C_FREQMOD_OFFSET] > channel[C_FREQMOD_MODULO])
        channel[C_FREQMOD_OFFSET] -= channel[C_FREQMOD_MODULO];
    channel[C_FREQ] = channel[C_BASE_FREQ];
    if (channel[C_FREQMOD_MULTIPLIER])
        channel[C_FREQ] += (uint16_t)fm_scale(freqmod_table[channel[C_FREQMOD_TABLE] + (channel[C_FREQMOD_OFFSET] >> 4)],
                                              channel[C_FREQMOD_MULTIPLIER]);
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

#ifndef HOST
#define TMR_COUNT (*(volatile uint16_t *)0177710)
#endif

/* Player_V2::generateSpkSamples: звучит первый канал с громкостью;
 * snd_active — играет ли звук (ядро тогда зовёт snd_poll в работе цикла) */
static void update_tone(void)
{
    if (!cur_nr) snd_active = 0;
    else if (!snd_active)
    {
        snd_active = 1;
#ifndef HOST
        snd_due = TMR_COUNT;
#endif
    }
    for (uint8_t i = 0; i < 4; i++)
        if (ch[i][C_VOLUME] && ch[i][C_TIME_LEFT])
        {
            tone_div = ch[i][C_FREQ] ? ch[i][C_FREQ] : 1;
            return;
        }
    tone_div = 0;
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
    player_stop(sound);
    for (uint8_t i = 0; i < sizeof que2; i++)
        if (que2[i] == sound) que2[i] = 0;
    update_tone();
}

/* Sound::stopAllSounds */
void stopAllSounds(void)
{
    que2pos = 0;
    for (uint8_t i = 0; i < sizeof que2; i++) que2[i] = 0;
    player_stopAll();
    update_tone();
}

uint16_t sound_music_timer(void)
{
    return ch[0][C_MUSIC_TIMER];
}

/* такты секвенсора 236 Гц по таймеру ВИ1 (23437.5 Гц): 1 такт = 1589/16 отсчёта */
#ifndef HOST
static uint16_t tm_prev, tm_frac;
static uint16_t snd_elapsed(void)
{
    uint16_t cur = TMR_COUNT, d = tm_prev - cur, n = 0;
    tm_prev = cur;
    while (d >= 256)
    {
        d -= 256;
        tm_frac += 4096;
        while (tm_frac >= 1589) { tm_frac -= 1589; n++; }
    }
    tm_frac += (uint16_t)(d << 4);
    while (tm_frac >= 1589) { tm_frac -= 1589; n++; }
    return n;
}
#else
uint16_t snd_elapsed(void);
#endif

/* продвинуть секвенсор до текущего момента (зовётся и из простоя ядра) */
void sound_update(void)
{
    uint16_t n = snd_elapsed();
    if (n > 236) n = 236;   /* после чтения с диска: не дольше секунды */
    if (cur_nr)
        while (n--) nextTick();
    update_tone();
}

/* Sound::processSoundQueues (очередь — с конца) */
void sound_process(void)
{
    sound_update();
    while (que2pos)
    {
        uint8_t snd = que2[--que2pos];
        if (snd) player_start(snd);
    }
    update_tone();
}
