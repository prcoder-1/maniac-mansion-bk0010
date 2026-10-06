/* Звук (банк C): заготовка до порта Player_V2 (M6) — очередь и состояние без вывода */
#include "scumm.h"

/* sound.cpp: Sound::addSoundToQueue */
void addSoundToQueue(uint8_t sound)
{
    VAR(VAR_LAST_SOUND) = sound;
    if (sound && sound <= res_num(rtSound)) res_get(rtSound, sound);   /* ensureResourceLoaded */
}
void stopSound(uint8_t sound) { (void)sound; }
void stopAllSounds(void) {}
uint8_t isSoundRunning(uint8_t sound) { (void)sound; return 0; }
void sound_process(void) {}
uint16_t sound_music_timer(void) { return 0; }
