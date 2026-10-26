/* Звук (банк C): очередь Sound (sound.cpp). Player_V2 — драйвер AY (snddrv.c) в
 * странице ОЗУ БК-0011М: такты — в кадровом прерывании, команды — через snd_call. */
#include "scumm.h"
#include "snddrv.h"

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
    return sound && (snd_cur == sound || snd_next == sound);
}

/* Sound::stopSound */
void stopSound(uint8_t sound)
{
    snd_call(DRV_STOP, sound, 0, 0);
    for (uint8_t i = 0; i < sizeof que2; i++)
        if (que2[i] == sound) que2[i] = 0;
}

/* Sound::stopAllSounds */
void stopAllSounds(void)
{
    que2pos = 0;
    for (uint8_t i = 0; i < sizeof que2; i++) que2[i] = 0;
    snd_call(DRV_STOPALL, 0, 0, 0);
}

/* Sound::processSoundQueues (очередь — с конца); Player_V2::startSound — в драйвере */
void sound_process(void)
{
    while (que2pos)
    {
        uint8_t snd = que2[--que2pos];
        if (!snd) continue;
        uint8_t pg = res_get(rtSound, snd);
        if (pg) snd_call(DRV_START, snd, pg, res_addr);
    }
}

/* звук PCjr (1) или PC-спикера (0) — меню СБР */
void sound_mode(uint8_t pcjr)
{
    snd_call(DRV_MODE, pcjr, 0, 0);
}
