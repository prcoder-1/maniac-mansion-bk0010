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
        { extern unsigned host_jiffies; host_jiffies += (unsigned)delta; }
        scummLoop(delta);
        if (getenv("SHOTEVERY") && host_frame % (unsigned)atoi(getenv("SHOTEVERY")) == 0)
        {
            extern uint8_t host_vram[];
            void png_bk(const char *path, const uint8_t *bm, uint16_t stride, uint16_t w, uint16_t h);
            char path[256];
            snprintf(path, sizeof path, "%s/f%05u.png", getenv("SHOTDIR"), host_frame);
            png_bk(path, host_vram, 64, 256, 256);
        }
        if (getenv("VRAMSUM"))
        {
            extern uint8_t host_vram[];
            uint32_t h = 0;
            for (unsigned i = 0; i < 040000; i++) h = h * 31 + host_vram[i];
            fprintf(host_trace, "vram %08x\n", (unsigned)h);
        }
    }
    { extern uint8_t host_vram[]; unsigned nz = 0; for (unsigned i = 172 * 64; i < 228 * 64; i++) nz += host_vram[i] != 0;
      fprintf(host_trace, "verb area nonzero %u\n", nz); }
    fprintf(host_trace, "END userState %02x cursorState %d room %u ego %d\n", userState, cursorState, currentRoom, VAR(VAR_EGO));
    fclose(host_trace);
    extern uint8_t host_vram[];
    void png_bk(const char *path, const uint8_t *bm, uint16_t stride, uint16_t w, uint16_t h);
    if (getenv("SHOT")) png_bk(getenv("SHOT"), host_vram, 64, 256, 256);
    return 0;
}
