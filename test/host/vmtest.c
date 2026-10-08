/* M3: прогон движка на хосте с трассой опкодов — для сверки со ScummVM
 *   vmtest КАТАЛОГ КАДРОВ трасса.txt [случайные.txt] [ввод.txt] */
#include <stdio.h>
#include <stdlib.h>
#include "scumm.h"
#include "vm.h"
#include "actor.h"
#include "gfx.h"

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
        if (getenv("ACTROOM"))   /* ACTROOM=actor=room@frame */
        {
            extern struct Actor actors[];
            unsigned act, room, fr;
            if (sscanf(getenv("ACTROOM"), "%u=%u@%u", &act, &room, &fr) == 3 && fr == host_frame) actors[act].room = (uint8_t)room;
        }
        if (getenv("SETVAR"))   /* SETVAR=var=value@frame */
        {
            unsigned v, val, fr;
            if (sscanf(getenv("SETVAR"), "%u=%u@%u", &v, &val, &fr) == 3 && fr == host_frame) VAR(v) = (int16_t)val;
        }
        scummLoop(delta);
        if (getenv("ACTDUMP"))   /* актёры комнаты по кадрам */
        {
            extern struct Actor actors[];
            for (int i = 1; i < NUM_ACTORS; i++)
                if (actors[i].room == currentRoom && currentRoom)
                    fprintf(host_trace, "ACT f%u r%u a%d c%u (%d,%d) el%d top%d bot%d\n", host_frame, currentRoom, i, actors[i].costume, actors[i].x, actors[i].y, actors[i].elevation, actors[i].top, actors[i].bottom);
            fprintf(host_trace, "CAM f%u strip %u\n", host_frame, screenStartStrip);
        }
        if (getenv("BMSHOT") && host_frame == (unsigned)atoi(getenv("BMSHOT")))   /* фон в PG_BITMAP */
        {
            extern uint8_t smk_mem[16][0100000];
            void png_bk(const char *path, const uint8_t *bm, uint16_t stride, uint16_t w, uint16_t h);
            png_bk(getenv("BMPATH"), smk_mem[PG_BITMAP], bm_stride, (uint16_t)(bm_stride * 4), roomHeight);
            fprintf(host_trace, "BM ring %u strips %u\n", bm_ring, bm_strips);
        }
        if (getenv("SLOTDUMP") && host_frame % 50 == 0)
        {
            extern struct ScriptSlot slots[];
            for (int i = 0; i < NUM_SCRIPT_SLOT; i++)
                if (slots[i].number || slots[i].status)
                    fprintf(host_trace, "SLOT f%u %d: n%u st%u wh%u fr%u fc%u offs%x\n", host_frame, i, slots[i].number, slots[i].status, slots[i].where, slots[i].freezeResistant, slots[i].freezeCount, slots[i].offs);
        }
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
