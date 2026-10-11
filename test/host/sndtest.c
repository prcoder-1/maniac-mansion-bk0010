/* M6: проигрывание каждого звука секвенсором Player_V2 — длительность и тоны
 *   sndtest КАТАЛОГ */
#include <stdio.h>
#include <stdlib.h>
#include "scumm.h"

extern const char *host_data_dir;
extern unsigned host_jiffies;

int main(int argc, char **argv)
{
    if (argc < 2) return 1;
    host_data_dir = argv[1];
    res_init();
    if (argc > 2)
    {
        /* по тактам: такт тон */
        extern unsigned host_snd_step;
        uint8_t s = (uint8_t)atoi(argv[2]);
        host_snd_step = 1;
        addSoundToQueue(s);
        sound_process();
        for (unsigned t = 1; isSoundRunning(s) && t < 20000; t++)
        {
            sound_update();
            printf("%u %u\n", t, tone_div);
        }
        return 0;
    }
    for (uint8_t s = 1; s < res_num(rtSound); s++)
    {
        if (!res_get(rtSound, s)) { printf("%2u: -\n", s); continue; }
        stopAllSounds();
        addSoundToQueue(s);
        sound_process();
        unsigned t = 0, changes = 0, sounding = 0;
        uint16_t last = 0xFFFF, lo = 0xFFFF, hi = 0;
        while (isSoundRunning(s) && t < 118 * 60)
        {
            host_jiffies++;   /* 1/60 с = ~4 такта */
            sound_update();
            t++;
            if (tone_div != last) { changes++; last = tone_div; }
            if (tone_div) { sounding++; if (tone_div < lo) lo = tone_div; if (tone_div > hi) hi = tone_div; }
        }
        printf("%2u: %5u jiffies%s, звучит %5u, смен %4u, делитель %u..%u\n", s, t, t >= 118 * 60 ? " (loop)" : "",
               sounding, changes, lo == 0xFFFF ? 0 : lo, hi);
    }
    return 0;
}
