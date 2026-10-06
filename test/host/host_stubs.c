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

/* такты секвенсора звука (236 Гц) по джиффи цикла (vmtest: host_jiffies) */
unsigned host_jiffies;
void trace_sound(const char *ev, uint16_t nr)
{
    if (host_trace) fprintf(host_trace, "sound %s %u (jiffy %u)\n", ev, nr, host_jiffies);
}
unsigned host_snd_step;   /* != 0: тактов на вызов (sndtest) */
uint16_t snd_elapsed(void)
{
    if (host_snd_step) return (uint16_t)host_snd_step;
    static unsigned last, frac;
    unsigned n = (host_jiffies - last) * 236 + frac;
    last = host_jiffies;
    frac = n % 60;
    return (uint16_t)(n / 60);
}

/* ввод: сценарий строк «кадр клавиша x y щелчок» (x, y — экран MM; -1 — не трогать) */
unsigned host_frame;
static unsigned in_frame[256];
static int in_key[256], in_x[256], in_y[256], in_click[256];
static int in_n, in_pos;
void host_input_load(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[128];
    while (in_n < 256 && fgets(line, sizeof line, f))
    {
        if (line[0] == '#') continue;
        if (sscanf(line, "%u %i %i %i %i", &in_frame[in_n], &in_key[in_n], &in_x[in_n], &in_y[in_n], &in_click[in_n]) >= 2)
            in_n++;
    }
    fclose(f);
}
uint16_t input_poll(int16_t *mx, int16_t *my, uint8_t *click)
{
    *click = 0;
    if (in_pos < in_n && in_frame[in_pos] <= host_frame)
    {
        if (in_x[in_pos] >= 0) *mx = (int16_t)in_x[in_pos];
        if (in_y[in_pos] >= 0) *my = (int16_t)in_y[in_pos];
        *click = (uint8_t)in_click[in_pos];
        return (uint16_t)in_key[in_pos++];
    }
    return 0;
}
