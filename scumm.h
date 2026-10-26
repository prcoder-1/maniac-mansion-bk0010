/* Общие определения порта SCUMM v2 (подмножество engines/scumm ScummVM).
 *
 * Правило памяти: код банка не держит указателей в страницы СМК — данные ресурсов
 * читаются функциями far_* по паре (страница, адрес окна 0100000..0157777).
 * Указатели, передаваемые между банками, должны указывать в базовое ОЗУ (стек,
 * состояние ядра), но не в bss банка.
 *
 * Имена функций и полей — как в ScummVM (без префикса «_»), у каждой функции —
 * ссылка на исходник, который она переводит. */
#pragma once
#include <stdint.h>
#include "smk.h"

typedef uint8_t byte;

/* scumm.h: типы ресурсов */
enum
{
    rtRoom = 1, rtScript = 2, rtCostume = 3, rtSound = 4, rtInventory = 5,
    rtVerb = 8, rtActorName = 9, rtObjectName = 16,
};

/* resource_v4.cpp readMAXS / scumm.cpp: размеры таблиц MM v2 */
#define NUM_VARIABLES 800
#define NUM_GLOBAL_OBJECTS 800
#define NUM_ROOMS 61
#define NUM_COSTUMES 40
#define NUM_SCRIPTS 200
#define NUM_SOUNDS 120
#define NUM_ACTORS 25
#define NUM_LOCAL_OBJECTS 32      /* ScummVM: 200; в комнатах MM не больше 27 */
#define NUM_INVENTORY 80
#define NUM_VERBS 100
#define NUM_NEW_NAMES 50
#define NUM_SENTENCE 6
#define NUM_SCRIPT_SLOT 32        /* ScummVM: 80 */
#define MAX_SCRIPT_NESTING 15
#define SENTENCE_SCRIPT 2

/* vars.cpp: ScummEngine_v2::setupScummVars */
enum
{
    VAR_EGO = 0, VAR_CAMERA_POS_X = 2, VAR_HAVE_MSG = 3, VAR_ROOM = 4, VAR_OVERRIDE = 5,
    VAR_MACHINE_SPEED = 6, VAR_CHARCOUNT = 7, VAR_ACTIVE_VERB = 8, VAR_ACTIVE_OBJECT1 = 9,
    VAR_ACTIVE_OBJECT2 = 10, VAR_NUM_ACTOR = 11, VAR_CURRENT_LIGHTS = 12, VAR_CURRENTDRIVE = 13,
    VAR_MUSIC_TIMER = 17, VAR_VERB_ALLOWED = 18, VAR_ACTOR_RANGE_MIN = 19, VAR_ACTOR_RANGE_MAX = 20,
    VAR_CURSORSTATE = 21, VAR_CAMERA_MIN_X = 23, VAR_CAMERA_MAX_X = 24, VAR_TIMER_NEXT = 25,
    VAR_SENTENCE_VERB = 26, VAR_SENTENCE_OBJECT1 = 27, VAR_SENTENCE_OBJECT2 = 28,
    VAR_SENTENCE_PREPOSITION = 29, VAR_VIRT_MOUSE_X = 30, VAR_VIRT_MOUSE_Y = 31,
    VAR_CLICK_AREA = 32, VAR_CLICK_VERB = 33, VAR_CLICK_OBJECT = 35, VAR_ROOM_RESOURCE = 36,
    VAR_LAST_SOUND = 37, VAR_BACKUP_VERB = 38, VAR_KEYPRESS = 39, VAR_CUTSCENEEXIT_KEY = 40,
    VAR_TALK_ACTOR = 41,
};
#define VAR(x) scummVars[x]

/* object.h */
enum { kObjectStatePickupable = 1, kObjectStateUntouchable = 2, kObjectStateLocked = 4, kObjectState_08 = 8 };
enum { OF_OWNER_ROOM = 0x0F };
enum { WIO_NOT_FOUND = 0xFF, WIO_INVENTORY = 0, WIO_ROOM = 1, WIO_GLOBAL = 2, WIO_LOCAL = 3, WIO_FLOBJECT = 4 };

/* script.h */
enum { ssDead = 0, ssPaused = 1, ssRunning = 2 };

/* scumm.h: setUserState */
enum
{
    USERSTATE_SET_FREEZE = 0x01, USERSTATE_SET_CURSOR = 0x02, USERSTATE_SET_IFACE = 0x04,
    USERSTATE_FREEZE_ON = 0x08, USERSTATE_CURSOR_ON = 0x10, USERSTATE_IFACE_SENTENCE = 0x20,
    USERSTATE_IFACE_INVENTORY = 0x40, USERSTATE_IFACE_VERBS = 0x80, USERSTATE_IFACE_ALL = 0xE0,
};

/* scumm.h: области щелчка */
enum { kVerbClickArea = 1, kSceneClickArea = 2, kInventoryClickArea = 3, kKeyClickArea = 4, kSentenceClickArea = 5 };

/* actor.h */
enum { MF_NEW_LEG = 1, MF_IN_LEG = 2, MF_TURN = 4, MF_LAST_LEG = 8 };
#define kInvalidBox 0xFF          /* kOldInvalidBox: игры с малым заголовком */
/* boxes.h */
enum { kBoxXFlip = 0x08, kBoxYFlip = 0x10, kBoxPlayerOnly = 0x20, kBoxLocked = 0x40, kBoxInvisible = 0x80 };
/* camera.h */
enum { kNormalCameraMode = 1, kFollowActorCameraMode = 2, kPanningCameraMode = 3 };

/* Actor_v2::initActor: кадры разговора (для банка A) */
#define ACTOR_TALKSTART 5
#define ACTOR_TALKSTOP 4

#define V12_X_MULTIPLIER 8
#define V12_Y_MULTIPLIER 2

/* object.h: ObjectData (32 байта — индекс массива сдвигом) */
struct ObjectData
{
    uint16_t OBIMoffset, OBCDoffset;
    int16_t walk_x, walk_y;
    uint16_t obj_nr;
    int16_t x_pos, y_pos;
    uint16_t width, height;
    byte actordir, parent, parentstate, state, fl_object_index, flags;
    byte pad[8];
};

/* far_*: smk.s (общая копия кода) */
uint8_t far_rb(uint16_t page, uint16_t addr);
uint16_t far_rw(uint16_t page, uint16_t addr);
void far_wb(uint16_t page, uint16_t addr, uint16_t v);
void far_rd(void *dst, uint16_t page, uint16_t addr, uint16_t n);
void far_wr(uint16_t page, uint16_t addr, const void *src, uint16_t n);
void far_cp(uint16_t dpage, uint16_t daddr, uint16_t spage, uint16_t saddr, uint16_t n);
void far_fill(uint16_t page, uint16_t addr, uint16_t v, uint16_t n);

/* ядро */
void fatal(const char *msg, uint16_t v) __attribute__((noreturn));

extern uint8_t screen_trashed;   /* буфер чтения файла затёр экран */
extern uint16_t dbg_t[4];         /* отладка: отметки таймера */

/* ---- состояние в базовом ОЗУ (state.c): видно из обоих банков ---- */
extern int16_t scummVars[NUM_VARIABLES];
extern uint8_t objectOwnerTable[NUM_GLOBAL_OBJECTS];
extern uint8_t objectStateTable[NUM_GLOBAL_OBJECTS];
extern uint16_t numGlobalObjects;
extern struct ObjectData objs[NUM_LOCAL_OBJECTS];
extern uint8_t numObjectsInRoom;
extern uint8_t currentRoom, roomResource;
extern uint16_t roomWidth, roomHeight;
extern uint8_t room_page;             /* страница блока текущей комнаты */
extern uint16_t room_addr;            /* его адрес в окне */
extern uint16_t ENCD_offs, EXCD_offs, IM00_offs;
extern uint8_t currentScript;         /* _currentScript, 0xFF — нет */
extern uint8_t userState, userPut, cursorState;
extern uint8_t haveMsg;
extern int16_t talkDelay;
extern uint8_t fullRedraw, bgNeedsRedraw;
extern int16_t screenStartStrip, screenEndStrip;
extern uint8_t egoPositioned;
extern uint8_t sentenceNum;
struct Camera { int16_t cur_x, dest_x, last_x; uint8_t mode, follows, movingToActor, pad; };
extern struct Camera camera;
extern uint8_t roomPalette[16];
extern int16_t mouse_x, mouse_y, virtualMouse_x, virtualMouse_y;
extern uint16_t mouseAndKeyboardStat;
extern uint8_t resourceMapper[128];
extern uint8_t flashlight_xStrips, flashlight_yStrips;
extern uint8_t vm_c_page;             /* кэш байт-кода VM: страница (0 — пуст) и адрес */
extern uint16_t vm_c_addr;
#define vm_flush() (vm_c_page = 0)

/* ---- script.c (VM, банк A) ---- */
void runScript(uint16_t script, uint8_t freezeResistant, uint8_t recursive);
void runObjectScript(uint16_t object, uint8_t entry, uint8_t freezeResistant, uint8_t recursive, int8_t slot);
void stopScript(uint16_t script);
void stopObjectScript(uint16_t script);
void stopObjectCode(void);
void runAllScripts(void);
void decreaseScriptDelay(int16_t amount);
void freezeScripts(void);
void unfreezeScripts(void);
uint8_t isScriptRunning(uint16_t script);
uint8_t isScriptInUse(uint16_t script);
uint8_t getVerbEntrypoint(uint16_t obj, uint8_t entry);
void runExitScript(void);
void runEntryScript(void);
void killScriptsAndResources(void);
void checkAndRunSentenceScript(void);
void runInputScript(uint8_t clickArea, uint16_t val, uint8_t mode);
void abortCutscene(void);
void runBootscript(void);
void vm_reset(void);

/* ---- script_v2.c (опкоды, банк A) ---- */
void executeOpcode(uint8_t opcode);
void setUserState(uint8_t state);
void resetSentence(void);

/* ---- object.c (банк A) ---- */
uint8_t getState(uint16_t obj);
void putState(uint16_t obj, uint8_t state);
uint8_t getOwner(uint16_t obj);
void putOwner(uint16_t obj, uint8_t owner);
int8_t getObjectIndex(uint16_t object);
uint8_t whereIsObject(uint16_t object);
uint8_t getOBCDFromObject(uint16_t obj, uint16_t *addr);
uint8_t getObjOrActorName(uint16_t obj, uint16_t *addr);
void setOwnerOf(uint16_t obj, uint8_t owner);
void addObjectToInventory(uint16_t obj, uint8_t room);
uint16_t findInventory(uint8_t owner, uint8_t idx);
uint8_t getInventoryCount(uint8_t owner);
int8_t getObjectOrActorXY(uint16_t object, int16_t *x, int16_t *y);
void getObjectXYPos(uint16_t object, int16_t *x, int16_t *y, int16_t *dir);
int16_t getObjX(uint16_t obj);
int16_t getObjY(uint16_t obj);
uint8_t getObjActToObjActDist(uint16_t a, uint16_t b);
uint16_t findObject(int16_t x, int16_t y);
void setObjectName(uint16_t obj);
void markObjectRectAsDirty(uint16_t obj);
void addObjectToDrawQue(uint8_t object);
void clearDrawObjectQueue(void);
void processDrawQue(void);
void updateObjectStates(void);
void drawRoomObjects(void);
void drawRoomObjectsRange(uint8_t s0, uint8_t s1);
void resetRoomObjects(void);
uint8_t inventory_res(uint8_t idx, uint16_t *addr);
extern uint16_t inventory[NUM_INVENTORY];

/* ---- room.c (банк B) ---- */
void startScene(uint8_t room, uint8_t actor, uint16_t objectNr);
uint8_t getNumBoxes(void);
uint8_t getBoxFlags(uint8_t box);
void setBoxFlags(uint8_t box, uint8_t val);
uint8_t getMaskFromBox(uint8_t box);
int8_t getNextBox(uint8_t from, uint8_t to);
struct BoxCoords { int16_t ul_x, ul_y, ur_x, ur_y, ll_x, ll_y, lr_x, lr_y; };
void getBoxCoordinates(uint8_t boxnum, struct BoxCoords *box);
uint8_t checkXYInBoxBounds(uint8_t boxnum, int16_t x, int16_t y);

/* ---- actor.c (банк B) ---- */
struct AdjustBoxResult { int16_t x, y; uint8_t box, pad; };
void actors_init(void);
void putActor(uint8_t act, int16_t x, int16_t y, uint8_t room);
void startWalkActor(uint8_t act, int16_t x, int16_t y, int16_t dir);
void adjustXYToBeInBox(uint8_t act, int16_t x, int16_t y, struct AdjustBoxResult *r);
void setActorCostume(uint8_t act, uint8_t c);
void setActorPalette(uint8_t act, uint8_t i, uint8_t c);
void setActorTalkColor(uint8_t act, uint8_t c);
void setActorSound(uint8_t act, uint8_t s);
void setActorElevation(uint8_t act, int16_t e);
void setActorDirection(uint8_t act, int16_t dir);
void turnToDirection(uint8_t act, int16_t dir);
void faceToObject(uint8_t act, uint16_t obj);
void animateActor(uint8_t act, uint8_t anim);
void startAnimActor(uint8_t act, uint8_t f);
void stopActorMoving(uint8_t act);
void hideActor(uint8_t act);
void showActors(void);
void walkActors(void);
void walkActorToActor(uint8_t act, uint8_t toActor, uint8_t dist);
uint8_t getActorFromPos(int16_t x, int16_t y);
void runActorTalkScript(uint8_t act, uint8_t f);
void actor_set_room(uint8_t act, uint8_t room);
uint8_t actor_name(uint8_t act, uint16_t *addr);
void actor_set_name(uint8_t act, uint8_t page, uint16_t addr);
/* поля актёра для опкодов (Actor::_room и т.д.) */
enum { AF_ROOM, AF_X, AF_Y, AF_COSTUME, AF_MOVING, AF_FACING, AF_ELEVATION, AF_WALKBOX,
       AF_TALKCOLOR, AF_VISIBLE, AF_FRAME, AF_TALKSTOP, AF_TALKSTART };
int16_t actor_get(uint8_t act, uint8_t field);
void processActors(void);
void playActorSounds(void);

/* ---- camera.c (банк B) ---- */
void setCameraAtEx(int16_t at);
void setCameraAt(int16_t pos_x);
void setCameraFollows(uint8_t act, uint8_t setCamera);
void moveCamera(void);
void cameraMoved(void);
void panCameraTo(int16_t x);
void actorFollowCamera(uint8_t act);

/* ---- string.c (банк A) ---- */
void decodeParseString(uint8_t actor);
void stopTalk(void);
void CHARSET_1(void);

/* ---- verbs.c (банк A) ---- */
void verbOps_new(uint8_t slot, uint8_t verbid, uint8_t x, uint8_t y, uint8_t prep);
void killVerb(uint8_t slot);
uint8_t getVerbSlot(uint8_t id, uint8_t mode);
void verb_set_mode(uint8_t slot, uint8_t mode);
void drawVerb(uint8_t slot, uint8_t mode);
void redrawVerbs(void);
void redrawV2Inventory(void);
void drawSentence(void);
uint8_t verb_name(uint8_t slot, uint16_t *addr);
void verbs_reset(void);
void verbMouseOver(uint8_t verb);
void checkExecVerbs(void);
void handleMouseOver(uint8_t updateInventory);
void clearVerbArea(void);

/* ---- scumm.c (банк B) ---- */
void scumm_init(void);
void scummLoop(int16_t delta);
int16_t scumm_delta(void);

/* ---- графика (банк B, gfx.c) ---- */
void drawObject(uint8_t idx);
void markStripsDirty(int16_t minStrip, int16_t maxStrip);
void gfx_room_changed(void);
void redrawBGAreas(void);
void drawDirtyScreenParts(void);
uint8_t gfx_view_flush(void);
uint8_t actor_in_strip(int16_t strip, uint8_t act);
#ifdef HOST
extern uint8_t div5[1024];
#else
extern const uint8_t div5[1024];
#endif   /* div5.s: x/5 (REPL) */
#define BK_X(x) ((uint16_t)((x) - div5[(x)]))
void bm_blit(uint16_t src, uint16_t stride, uint16_t dst, uint16_t ncols, uint16_t rows);
void charset_printChar(int16_t x, int16_t y, uint8_t c, uint8_t color);
void charset_clear(void);
int16_t charset_drawString(int16_t x, int16_t y, uint8_t color, const uint8_t *s);
void screen_clear_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void cursor_hide(void);
void cursor_show(void);
void gfx_clear_all(void);
void screen_recolor(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t from, uint8_t to);

/* ---- звук (банк C; драйвер AY — snddrv.h) ---- */
void sound_process(void);
void sound_mode(uint8_t pcjr);
#define BOXES_SIZE 512   /* боксы и матрица комнаты (room.c): у MM до 361 байта (комната 10) */
extern uint8_t draw_frame;   /* 0 — scummLoop без отрисовки (логика догоняет время) */
void addSoundToQueue(uint8_t sound);
void stopSound(uint8_t sound);
void stopAllSounds(void);
uint8_t isSoundRunning(uint8_t sound);

/* ---- resource.c (банк B) ---- */
void res_init(void);
/* адрес найденного/выделенного — в res_addr (базовое ОЗУ: функции зовутся из других банков) */
uint8_t res_get(uint8_t type, uint16_t idx);
uint16_t res_alloc(uint16_t size);
extern uint16_t res_addr;
void res_free(uint16_t page, uint16_t addr);
void res_lock_room(uint8_t room);
/* сохранение игры: курсор образа и блоки кучи (main.c), хуки модулей */
extern uint8_t sv_page;
extern uint16_t sv_off;
void sv_heap(uint8_t *pg, uint16_t *addr, uint8_t valid, uint8_t mode);
uint16_t save_dataA(void);
uint16_t save_dataB(void);
void heapA(uint8_t mode);
void heapV(uint8_t mode);
void heapB(uint8_t mode);
void actors_redraw_all(void);
void cost_info_reset(void);   /* costume.c: забыть заголовки костюмов */
uint8_t saveload_menu(void);
uint8_t res_num(uint8_t type);

/* ---- платформа ---- */
uint16_t getRandomNumber(uint16_t max);
void trace_opcode(uint16_t script, uint16_t offs, uint8_t op);
void trace_write_var(uint16_t var, int16_t value);
