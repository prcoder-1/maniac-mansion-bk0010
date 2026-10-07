/* Состояние движка в базовом ОЗУ: видно из обоих банков */
#include "scumm.h"

int16_t scummVars[NUM_VARIABLES];
uint8_t objectOwnerTable[NUM_GLOBAL_OBJECTS];
uint8_t objectStateTable[NUM_GLOBAL_OBJECTS];
uint16_t numGlobalObjects;
uint8_t screen_trashed;
uint16_t res_addr;
uint8_t render_cga;   /* 1 — цвета как CGA ScummVM (ditherCGA), 0 — дизеринг EGA */
uint16_t tone_div;   /* звучащий тон: делитель PIT, 0 — тишина (sound.c) */
#ifdef HOST
uint8_t snd_active;   /* на БК — sys.ld: звук играет (секвенсор продвигает snd_tick) */
#endif

#include "gfx.h"
uint8_t roomPalette[16];
uint16_t bm_stride;
uint8_t bm_strips;
uint16_t dbg_t[4];

struct ObjectData objs[NUM_LOCAL_OBJECTS];
uint8_t numObjectsInRoom;
uint8_t currentRoom, roomResource;
uint16_t roomWidth, roomHeight;
uint8_t room_page;
uint16_t room_addr;
uint16_t ENCD_offs, EXCD_offs, IM00_offs;
uint8_t currentScript = 0xFF;
uint8_t userState, userPut, cursorState;
uint8_t haveMsg;
int16_t talkDelay;
uint8_t fullRedraw, bgNeedsRedraw;
int16_t screenStartStrip, screenEndStrip;
uint8_t egoPositioned;
uint8_t sentenceNum;
struct Camera camera;
int16_t mouse_x, mouse_y, virtualMouse_x, virtualMouse_y;
uint16_t mouseAndKeyboardStat;
uint8_t resourceMapper[128];
uint8_t flashlight_xStrips, flashlight_yStrips;
uint8_t vm_c_page;
uint16_t vm_c_addr;
uint8_t vm_cache[64];
uint16_t dbg_acc[8];
