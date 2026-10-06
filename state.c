/* Состояние движка в базовом ОЗУ: видно из обоих банков */
#include "scumm.h"

int16_t scummVars[NUM_VARIABLES];
uint8_t objectOwnerTable[NUM_GLOBAL_OBJECTS];
uint8_t objectStateTable[NUM_GLOBAL_OBJECTS];
uint16_t numGlobalObjects;
uint8_t screen_trashed;

#include "gfx.h"
uint8_t roomPalette[16];
uint16_t bm_stride;
uint8_t bm_strips;
uint16_t dbg_t[4];
