/* Хостовые заглушки M3: графика, звук, ввод; трасса в формате ScummVM
 * (--debugflags=opcodes,vars) и случайные числа из журнала ScummVM. */
#include <stdio.h>
#include <stdlib.h>
#include "scumm.h"
#include "gfx.h"
#include "opcodes_v2.h"

FILE *host_trace;
static FILE *rnd_in;

void host_rnd_open(const char *path) { rnd_in = fopen(path, "r"); }

/* ScummVM: Common::RandomSource; в режиме сверки — значения из журнала эталона */
uint16_t getRandomNumber(uint16_t max)
{
    int v;
    if (rnd_in && fscanf(rnd_in, "%d", &v) == 1) return (uint16_t)v;
    static uint32_t seed = 12345;
    seed = 0xDEADBF03u * (seed + 1);
    seed = (seed >> 13) | (seed << 19);
    return (uint16_t)(seed % (max + 1u));
}

void trace_opcode(uint16_t script, uint16_t offs, uint8_t op)
{
    if (host_trace)
        fprintf(host_trace, "Script %u, offset 0x%x: [%X] %s()\n", script, offs + 1, op, opcode_name[opcode_handler[op]]);
}

void trace_write_var(uint16_t var, int16_t value)
{
    if (host_trace) fprintf(host_trace, "writeVar(%u) = %d\n", var, value);
}

/* звук — пока без вывода */
void addSoundToQueue(uint8_t s)
{
    VAR(VAR_LAST_SOUND) = s;
    if (s && s <= res_num(rtSound)) res_get(rtSound, s);
}
void stopSound(uint8_t s) { (void)s; }
void stopAllSounds(void) {}
uint8_t isSoundRunning(uint8_t s) { (void)s; return 0; }
void sound_process(void) {}
uint16_t sound_music_timer(void) { return 0; }

/* ввод: сценарий «кадр клавиша» */
unsigned host_frame;
static unsigned in_frame[256];
static int in_key[256];
static int in_n, in_pos;
void host_input_load(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return;
    while (in_n < 256 && fscanf(f, "%u %i", &in_frame[in_n], &in_key[in_n]) == 2) in_n++;
    fclose(f);
}
uint16_t input_poll(int16_t *mx, int16_t *my, uint8_t *click)
{
    (void)mx; (void)my; (void)click;
    if (in_pos < in_n && in_frame[in_pos] <= host_frame) return (uint16_t)in_key[in_pos++];
    return 0;
}
