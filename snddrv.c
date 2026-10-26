/* Звуковой драйвер: Player_V2 (players/player_v2base.cpp, player_v2.cpp) для
 * AY-3-8910 на порту 0177714. Партии PCjr/Tandy (3 тона и шум) — на три канала AY
 * и его генератор шума; поток PC-спикера (пункт меню) — на канал B.
 *
 * Лежит в странице 1 ОЗУ БК-0011М с адреса 040000 (drv.ld) и работает только в
 * окне 040000..077777: такты секвенсора 236 Гц — из кадрового прерывания (irq.s,
 * вход drv_irq в drvhdr.s), команды игры — через snd_call ядра с закрытыми
 * прерываниями. Данные звучащего и следующего звука — копии в двух слотах этой же
 * страницы (snddrv.h); со страницами СМК драйвер работает только в командах. */
#include "snddrv.h"

#define HEADER_LEN 4   /* GF_OLD_BUNDLE */

/* player_v2base.h: channel_data, слова 0..24; шаг канала — 32 слова (drvhdr.s) */
enum
{
    C_TIME_LEFT, C_NEXT_CMD, C_BASE_FREQ, C_FREQ_DELTA, C_FREQ, C_VOLUME, C_VOLUME_DELTA,
    C_TEMPO, C_INTER_NOTE_PAUSE, C_TRANSPOSE, C_NOTE_LENGTH, C_HULL_CURVE, C_HULL_OFFSET,
    C_HULL_COUNTER, C_FREQMOD_TABLE, C_FREQMOD_OFFSET, C_FREQMOD_INCR, C_FREQMOD_MULTIPLIER,
    C_FREQMOD_MODULO, C_UNKNOWN, C_MUSIC_TIMER = 23, C_MUSIC_SCRIPT_NR, C_WORDS,
    C_FM_T = C_WORDS, C_FM_M   /* модуляция freq этого такта: считается при выводе */
};
#define C_STRIDE 32

const uint8_t note_lengths[] = {
    0,
    0, 0, 2,
    0, 3, 4,
    5, 6, 8,
    9, 12, 16,
    18, 24, 32,
    36, 48, 64,
    72, 96
};

const uint16_t hull_offsets[] = {
    0, 12, 24, 36, 48, 60,
    72, 88, 104, 120, 136, 256,
    152, 164, 180
};

const int16_t hulls[] = {   /* и drvhdr.s */
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

const uint16_t freqmod_lengths[] = { 0x1000, 0x1000, 0x20, 0x2000, 0x1000 };
const uint16_t freqmod_offsets[] = { 0, 0x100, 0x200, 0x302, 0x202 };

#include "freqmod.h"   /* freqmod_table[0x502] */

const uint16_t spk_freq_table[12] = {
    36484, 34436, 32503, 30679, 28957, 27332,
    25798, 24350, 22983, 21693, 20476, 19326
};

const uint16_t pcjr_freq_table[12] = {
    65472, 61760, 58304, 55040, 52032, 49024,
    46272, 43648, 41216, 38912, 36736, 34624
};

/* ослабление SN76489 (шаг 2 дБ, 15 — тишина) -> громкость AY по таблице ЦАП AY */
const uint8_t att_level[16] = { 15, 14, 13, 12, 12, 11, 10, 9, 8, 8, 7, 6, 6, 5, 5, 0 };

/* 5-й канал — для «очистки» каналов вне 0..3 (команда 0xfd) */
uint16_t ch[5][C_STRIDE];   /* и drvhdr.s */
uint16_t cur_nr;            /* и drvhdr.s */
uint16_t next_nr;   /* и drvhdr.s */
const uint8_t *cur_data;   /* и drvhdr.s */
static const uint8_t *next_data;
uint16_t retaddr;   /* и drvhdr.s */
uint8_t pcjr = 1;
extern uint8_t cval[3];   /* drvhdr.s: кэш периода тона каналов годен */

void ay_w(uint8_t r, uint8_t v);       /* drvhdr.s */
uint16_t ch_freq(const uint16_t *c);   /* drvhdr.s: freq с модуляцией */
void smk_rd(void *dst, uint16_t page, uint16_t addr, uint16_t n);   /* drvhdr.s */

static inline uint16_t rdw(uint16_t off)
{
    return (uint16_t)(cur_data[off] | (cur_data[off + 1] << 8));
}

static void clear_channel(uint8_t i)
{
    for (uint8_t k = 0; k < C_STRIDE; k++) ch[i][k] = 0;
}

static void post(void)
{
    snd_cur = cur_nr;
    snd_next = next_nr;
    snd_mtimer = ch[0][C_MUSIC_TIMER];
}

/* Player_V2Base::chainSound */
static void chainSound(uint16_t nr, const uint8_t *data)
{
    uint16_t offset = HEADER_LEN + (pcjr ? 10 : 2);
    cur_nr = nr;
    cur_data = data;
    for (uint8_t i = 0; i < 4; i++)
    {
        clear_channel(i);
        ch[i][C_MUSIC_SCRIPT_NR] = nr;
        if (data)
        {
            ch[i][C_NEXT_CMD] = rdw(offset + 2 * i);
            if (ch[i][C_NEXT_CMD]) ch[i][C_TIME_LEFT] = 1;
        }
    }
}

void chainNextSound(void)
{
    if (next_nr)
    {
        chainSound(next_nr, next_data);
        next_nr = 0;
        next_data = 0;
    }
}

/* Player_V2Base::execute_cmd — xcmd в drvhdr.s */

/* Player_V2::startSound. Данные нового звука копируются из страницы СМК в слот,
 * не занятый тем, что остаётся звучать или ждать (их не больше двух). */
uint16_t drv_start(uint16_t nr, uint16_t pg, uint16_t addr)
{
    uint8_t hdr[HEADER_LEN + 2];
    smk_rd(hdr, pg, addr, sizeof hdr);
    uint16_t size = (uint16_t)(hdr[0] | (hdr[1] << 8));
    if (size > DRV_SLOT0_N || size < sizeof hdr) return 0;
    uint8_t prio = hdr[HEADER_LEN], restartable = hdr[HEADER_LEN + 1];
    uint8_t cprio = cur_data ? cur_data[HEADER_LEN] : 0;
    uint8_t nprio = next_data ? next_data[HEADER_LEN] : 0;
    uint8_t *slot = 0;   /* новый звук остаётся — его слот */

    /* (cnr, cdata) — кандидат в следующие: новый или прежний текущий */
    uint16_t cnr = nr;
    const uint8_t *cdata = 0;   /* 0 — новый звук */
    uint8_t new_cur = !cur_nr || cprio <= prio;
    if (new_cur)
    {
        cnr = cur_nr;
        cdata = cur_data;
        prio = cprio;
        restartable = cur_data ? cur_data[HEADER_LEN + 1] : 0;
    }
    uint16_t res_cur = new_cur ? nr : cur_nr;
    uint8_t to_next = cnr != res_cur && restartable && (!next_nr || nprio <= prio);
    /* что из старого остаётся: текущий (если новый не стал текущим), кандидат в
     * следующие или прежний следующий */
    const uint8_t *k1 = new_cur ? 0 : cur_data;
    const uint8_t *k2 = to_next ? cdata : next_data;
    if (new_cur || (to_next && !cdata))
    {
        uint8_t *s0 = (uint8_t *)DRV_SLOT0, *s1 = (uint8_t *)DRV_SLOT1;
        uint8_t free0 = s0 != k1 && s0 != k2, free1 = s1 != k1 && s1 != k2;
        if (size <= DRV_SLOT1_N && free1) slot = s1;
        else if (free0) slot = s0;
        else
        {
            /* большой звук, а в большом слоте — остающийся малый: тот — в малый слот */
            uint16_t n = (uint16_t)(s0[0] | (s0[1] << 8));
            if (!free1 || n > DRV_SLOT1_N) return 0;
            for (uint16_t i = 0; i < n; i++) s1[i] = s0[i];
            if (cur_data == s0) cur_data = s1;
            if (next_data == s0) next_data = s1;
            if (cdata == s0) cdata = s1;
            slot = s0;
        }
        smk_rd(slot, pg, addr, size);
    }
    if (new_cur) chainSound(nr, slot);
    if (to_next)
    {
        next_nr = cnr;
        next_data = cdata ? cdata : slot;
    }
    post();
    return 1;
}

/* Player_V2::stopSound */
void drv_stop(uint16_t nr)
{
    if (next_nr == nr)
    {
        next_nr = 0;
        next_data = 0;
    }
    if (cur_nr == nr)
    {
        for (uint8_t i = 0; i < 4; i++) clear_channel(i);
        cur_nr = 0;
        cur_data = 0;
        chainNextSound();
    }
    post();
}

/* Player_V2::stopAllSounds */
void drv_stopall(void)
{
    for (uint8_t i = 0; i < 4; i++) clear_channel(i);
    next_nr = cur_nr = 0;
    next_data = cur_data = 0;
    post();
}

/* PCjr (1) или спикер (0): звучащий звук — заново в новом виде */
void drv_mode(uint16_t p)
{
    pcjr = (uint8_t)p;
    cval[0] = cval[1] = cval[2] = 0;   /* периоды в ay_want — от другого режима */
    snd_pcjr = p;
    if (cur_nr) chainSound(cur_nr, cur_data);
    ay_dirty = 1;
    post();
}

void drv_init(void)
{
    drv_stopall();
    drv_mode(1);
}

/* --- вывод в AY раз в кадр --- */

uint16_t pj_tp(uint16_t n);   /* drvhdr.s: делитель SN76489 -> период тона AY */

/* делитель PIT (1193182 / d Гц) -> период тона AY: d * 23 / 256, по байтам */
static uint16_t spk_tp(uint16_t d)
{
    uint16_t q = d >> 8, r = d & 0377;
    uint16_t tp = (uint16_t)((q << 4) + (q << 3) - q + ((((r << 4) + (r << 3) - r) + 0200) >> 8));
    return tp > 07777 ? 07777 : tp;
}

uint8_t ay_want[12] __attribute__((aligned(2)));   /* R0..R10 кадра; ay_out (drvhdr.s) пишет отличные от записанных */
void ay_out(void);

/* PC-спикер (drv_out, drvhdr.s): первый канал с громкостью — на канал B; смеситель */
uint8_t drv_out_spk(void)
{
    for (uint8_t i = 0; i < 4; i++)
        if (ch[i][C_VOLUME] && ch[i][C_TIME_LEFT])
        {
            uint16_t d = ch_freq(ch[i]), tp = spk_tp(d ? d : 1);
            ay_want[2] = (uint8_t)tp;
            ay_want[3] = (uint8_t)(tp >> 8);
            ay_want[9] = 13;
            return 075;
        }
    return 077;
}
