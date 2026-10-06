/* M3: прогон движка на хосте с трассой опкодов — для сверки со ScummVM
 *   vmtest КАТАЛОГ КАДРОВ трасса.txt [случайные.txt] [ввод.txt] */
#include <stdio.h>
#include <stdlib.h>
#include "scumm.h"

extern const char *host_data_dir;
extern FILE *host_trace;
extern unsigned host_frame;
void host_rnd_open(const char *path);
void host_input_load(const char *path);

int main(int argc, char **argv)
{
    if (argc < 4) return 1;
    host_data_dir = argv[1];
    unsigned frames = (unsigned)atoi(argv[2]);
    host_trace = fopen(argv[3], "w");
    if (argc > 4) host_rnd_open(argv[4]);
    if (argc > 5) host_input_load(argv[5]);
    scumm_init();
    runBootscript();
    for (host_frame = 0; host_frame < frames; host_frame++)
    {
        int16_t delta = scumm_delta();
        fprintf(host_trace, "--- frame %u delta %d room %u\n", host_frame, delta, currentRoom);
        scummLoop(delta);
    }
    fclose(host_trace);
    extern uint8_t host_vram[];
    void png_bk(const char *path, const uint8_t *bm, uint16_t stride, uint16_t w, uint16_t h);
    if (getenv("SHOT")) png_bk(getenv("SHOT"), host_vram, 64, 256, 256);
    return 0;
}
