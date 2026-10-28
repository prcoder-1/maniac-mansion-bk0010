/* Актёр (банк B): поля Actor / Actor_v2 / CostumeData из ScummVM actor.h.
 * Размер — 256 байт: индекс массива — сдвигом. */
#pragma once
#include "scumm.h"

/* Actor_v2::initActor */
#define ACTOR_INIT_FRAME 2
#define ACTOR_WALK_FRAME 0
#define ACTOR_STAND_FRAME 1
#define ACTOR_TALKSTART_FRAME 5
#define ACTOR_TALKSTOP_FRAME 4

struct Actor
{
    union
    {
    struct
    {
    int16_t x, y, elevation, facing, targetFacing;
    int16_t top, bottom;
    uint8_t number, room, costume, moving, walkbox, visible, frame, talkColor;
    uint8_t sound, needRedraw, needBgReset, animProgress, animSpeed, name_page;
    uint16_t name_addr;
    uint8_t palette[16];
    /* ActorWalkData */
    int16_t dest_x, dest_y, cur_x, cur_y, next_x, next_y;
    int32_t deltaXFactor, deltaYFactor;
    uint8_t destbox, curbox;
    int16_t destdir;
    /* CostumeData */
    uint16_t cost_animCounter, cost_stopped;
    uint8_t cost_soundCounter, cost_soundPos;
    uint16_t cost_curpos[16], cost_start[16], cost_end[16], cost_frame[16];
    uint16_t cost_still;   /* конечности, которым increaseAnim ничего не изменит (costume.c) */
    /* последняя отрисовка (costume_r.c): на месте — только сменившиеся конечности;
     * dr_cost 0 — рисовать целиком */
    uint16_t dr_pos[16], dr_stopped, dr_pal;
    int16_t dr_x, dr_y, dr_facing, dr_sss;
    uint8_t dr_cost;
    };
    uint8_t raw[256];
    };
};
_Static_assert(sizeof(struct Actor) == 256, "struct Actor: 256 байт");

extern struct Actor actors[NUM_ACTORS];
/* актёры текущей комнаты по возрастанию номеров (actor.c); ract_build — перед обходом */
extern uint8_t ract[NUM_ACTORS], nract;
void ract_build(void);

/* заголовок костюма (costume.c) */
struct CostInfo { uint16_t res, ptr, animCmds; uint8_t page, numAnim, fmt, palette0; uint16_t pad[3]; };   /* 16 байт: индекс — сдвигом */
const struct CostInfo *cost_info(uint8_t id);


/* costume.c: ClassicCostumeLoader */
void costume_reset(struct Actor *a);
void costumeDecodeData(struct Actor *a, uint16_t frame);
uint8_t increaseAnims(struct Actor *a);
void animateCostume(struct Actor *a);
void animate_only(void);
