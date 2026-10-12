/* Опкоды SCUMM v2 (банк A): перевод engines/scumm/script_v2.cpp и используемых
 * обработчиков script_v5.cpp / script_v4.cpp / script_v3.cpp. Таблица опкод ->
 * обработчик — opcodes_v2.h (генерируется из setupOpcodes()).
 */
#include "vm.h"
#include "opcodes_v2.h"

#define PARAM_1 0x80
#define PARAM_2 0x40
#define PARAM_3 0x20

struct SentenceTab { uint8_t verb, preposition; uint16_t objectA, objectB; uint8_t freezeCount, pad; };
struct SentenceTab sentence[NUM_SENTENCE];

/* --- переменные (script_v2.cpp: readVar / writeVar / getResultPos) --- */

static int16_t readVar(uint16_t var)
{
    if (var >= 14 && var <= 16) var = (uint16_t)scummVars[var];
    if (var >= NUM_VARIABLES) fatal("VAR", var);
    return scummVars[var];
}

static void writeVar(uint16_t var, int16_t value)
{
    if (var >= NUM_VARIABLES) fatal("VAR W", var);
#ifdef HOST
    trace_write_var(var, value);
#endif
    if (var == VAR_CUTSCENEEXIT_KEY)
        if (value == 4 || value == 13 || value == 64) value = 27;
    scummVars[var] = value;
}

static int16_t getVar(void)
{
    return readVar(fetchScriptByte());
}

static void getResultPos(void)
{
    resultVarNumber = fetchScriptByte();
}

static void getResultPosIndirect(void)
{
    resultVarNumber = (uint16_t)scummVars[fetchScriptByte()];
}

static void setResult(int16_t value)
{
    writeVar(resultVarNumber, value);
}

/* script_v5.cpp: getVarOrDirectByte / getVarOrDirectWord */
static uint8_t gvdb(uint8_t mask)
{
    if (opcode & mask) return (uint8_t)getVar();
    return fetchScriptByte();
}

static int16_t gvdw(uint8_t mask)
{
    if (opcode & mask) return getVar();
    return (int16_t)fetchScriptWord();
}

/* script_v5.cpp: jumpRelative */
static void jumpRelative(uint8_t cond)
{
    int16_t offset = (int16_t)fetchScriptWord();
    if (!cond) sp_pc += (uint16_t)offset;
}

/* script_v5.cpp: o5_breakHere */
static void breakHere(void)
{
    updateScriptPtr();
    currentScript = 0xFF;
}

/* --- состояния объектов (script_v2.cpp: *StateCommon) --- */

static void setStateCommon(uint8_t type)
{
    uint16_t obj = (uint16_t)gvdw(PARAM_1);
    putState(obj, getState(obj) | type);
}

static void clearStateCommon(uint8_t type)
{
    uint16_t obj = (uint16_t)gvdw(PARAM_1);
    putState(obj, getState(obj) & (uint8_t)~type);
}

static void ifStateCommon(uint8_t type)
{
    uint16_t obj = (uint16_t)gvdw(PARAM_1);
    uint8_t s = getState(obj) & type;
    jumpRelative(s != 0);
}

static void ifNotStateCommon(uint8_t type)
{
    uint16_t obj = (uint16_t)gvdw(PARAM_1);
    uint8_t s = getState(obj) & type;
    jumpRelative(s == 0);
}

/* resource.cpp: loadPtrToResource — строка из скрипта (до 0, с escape 0xFF) в кучу */
uint8_t loadPtrToResource(void)
{
    uint8_t buf[128];
    uint16_t n = 0;
    for (;;)
    {
        uint8_t c = fetchScriptByte();
        if (n < sizeof buf - 1) buf[n++] = c;
        if (!c) break;
        if (c == 0xFF)
        {
            c = fetchScriptByte();
            if (n < sizeof buf - 1) buf[n++] = c;
            if (c != 1 && c != 2 && c != 3 && c != 8)
                for (uint8_t k = 0; k < 2; k++)
                {
                    c = fetchScriptByte();
                    if (n < sizeof buf - 1) buf[n++] = c;
                }
        }
    }
    buf[n - 1] = 0;
    uint8_t pg = (uint8_t)res_alloc(n);
    far_wr(pg, res_addr, buf, n);
    return pg;
}

/* --- обработчики --- */

static void o2_putActor(void)
{
    uint8_t act = gvdb(PARAM_1);
    uint8_t x = gvdb(PARAM_2);
    uint8_t y = gvdb(PARAM_3);
    putActor(act, x, y, (uint8_t)actor_get(act, AF_ROOM));
}

static void o2_drawObject(void)
{
    uint16_t obj = (uint16_t)gvdw(PARAM_1);
    uint8_t xpos = gvdb(PARAM_2);
    uint8_t ypos = gvdb(PARAM_3);
    int8_t idx = getObjectIndex(obj);
    if (idx == -1) return;
    struct ObjectData *od = &objs[idx];
    if (xpos != 0xFF)
    {
        od->walk_x += (int16_t)(xpos * 8) - od->x_pos;
        od->x_pos = (int16_t)(xpos * 8);
        od->walk_y += (int16_t)(ypos * 8) - od->y_pos;
        od->y_pos = (int16_t)(ypos * 8);
    }
    addObjectToDrawQue((uint8_t)idx);
    int16_t x = od->x_pos, y = od->y_pos;
    uint16_t w = od->width, h = od->height;
    for (int8_t i = NUM_LOCAL_OBJECTS - 1; i >= 0; i--)
    {
        struct ObjectData *o = &objs[i];
        if (o->obj_nr && o->x_pos == x && o->y_pos == y && o->width == w && o->height == h)
            putState(o->obj_nr, getState(o->obj_nr) & (uint8_t)~kObjectState_08);
    }
    putState(obj, getState(od->obj_nr) | kObjectState_08);
}

static void o2_resourceRoutines(void)
{
    static const uint8_t resTypes[] = { 0, 0, rtCostume, rtRoom, 0, rtScript, rtSound };
    uint8_t resid = gvdb(PARAM_1);
    uint8_t op = fetchScriptByte();
    uint8_t type = (op >> 4) < sizeof resTypes ? resTypes[op >> 4] : 0;
    if ((op & 0x0f) == 0 || type == 0) return;
    /* HACK ScummVM: демо-скрипт 9 MM v2 грузит несуществующий звук 1 */
    if (slots[currentScript].number == 9 && type == rtSound && resid == 1) return;
    if ((op & 0x0f) == 1)
        res_get(type, resid);
    else
        res_lock(type, resid, op & 1);   /* запертый не освобождается при нехватке кучи */
}

static void o2_verbOps(void)
{
    uint8_t verb = fetchScriptByte();
    uint8_t slot;
    switch (verb)
    {
    case 0:   /* SO_DELETE_VERBS */
        slot = gvdb(PARAM_1) + 1;
        killVerb(slot);
        break;
    case 0xFF: /* Verb On/Off */
    {
        verb = fetchScriptByte();
        uint8_t state = fetchScriptByte();
        slot = getVerbSlot(verb, 0);
        verb_set_mode(slot, state);
        break;
    }
    default:   /* New Verb */
    {
        uint8_t x = fetchScriptByte();
        uint8_t y = fetchScriptByte();
        slot = gvdb(PARAM_1) + 1;
        uint8_t prep = fetchScriptByte();
        verbOps_new(slot, verb, x, y, prep);
        break;
    }
    }
    /* интерфейс выключен (катсцена): глаголы на экран не выводятся (redrawVerbs),
     * их нарисует setUserState — иначе скрипт начала игры выводит меню дважды */
    if (!(userState & USERSTATE_IFACE_VERBS)) return;
    drawVerb(slot, 0);
    verbMouseOver(0);
}

static void o2_doSentence(void)
{
    uint8_t a = gvdb(PARAM_1);
    if (a == 0xFC)
    {
        sentenceNum = 0;
        stopScript(SENTENCE_SCRIPT);
        return;
    }
    if (a == 0xFB)
    {
        resetSentence();
        return;
    }
    if (sentenceNum >= NUM_SENTENCE) fatal("SENTENCE", sentenceNum);
    struct SentenceTab *st = &sentence[sentenceNum++];
    st->verb = a;
    st->objectA = (uint16_t)gvdw(PARAM_2);
    st->objectB = (uint16_t)gvdw(PARAM_3);
    st->preposition = st->objectB != 0;
    st->freezeCount = 0;
    uint8_t op = fetchScriptByte();
    switch (op)
    {
    case 0:
        break;
    case 1:   /* выполнить */
        sentenceNum--;
        if (st->verb == 254)
            stopObjectScript(st->objectA);
        else
        {
            uint8_t isBackgroundScript, isSpecialVerb;
            if (st->verb != 253 && st->verb != 250)
            {
                VAR(VAR_ACTIVE_VERB) = st->verb;
                VAR(VAR_ACTIVE_OBJECT1) = (int16_t)st->objectA;
                VAR(VAR_ACTIVE_OBJECT2) = (int16_t)st->objectB;
                isBackgroundScript = 0;
                isSpecialVerb = 0;
            }
            else
            {
                isBackgroundScript = st->verb == 250;
                isSpecialVerb = 1;
                st->verb = 253;
            }
            int8_t slot = -1;
            for (uint8_t i = 0; i < NUM_SCRIPT_SLOT; i++)
            {
                struct ScriptSlot *ss = &slots[i];
                if (st->objectA == ss->number && ss->freezeResistant == isBackgroundScript &&
                    ss->recursive == isSpecialVerb &&
                    (ss->where == WIO_ROOM || ss->where == WIO_INVENTORY || ss->where == WIO_FLOBJECT))
                {
                    slot = (int8_t)i;
                    break;
                }
            }
            runObjectScript(st->objectA, st->verb, isBackgroundScript, isSpecialVerb, slot);
        }
        break;
    case 2:   /* показать предложение */
        sentenceNum--;
        VAR(VAR_SENTENCE_VERB) = st->verb;
        VAR(VAR_SENTENCE_OBJECT1) = (int16_t)st->objectA;
        VAR(VAR_SENTENCE_OBJECT2) = (int16_t)st->objectB;
        drawSentence();
        break;
    default:
        fatal("DOSENTENCE", op);
    }
}

static void o2_ifClassOfIs(void)
{
    uint16_t obj = (uint16_t)gvdw(PARAM_1);
    uint8_t clsop = gvdb(PARAM_2);
    uint16_t a;
    uint8_t pg = getOBCDFromObject(obj, &a);
    if (!pg)
    {
        jumpRelative(0);
        return;
    }
    uint8_t cls = far_rb(pg, a + 6) & clsop;
    jumpRelative(cls == clsop);
}

static void o2_startScript(void)
{
    uint8_t script = gvdb(PARAM_1);
    /* WORKAROUND ScummVM #1447058: звонок в дверь во время поимки Тедом */
    if (script == 82 + 5)
        if (isScriptRunning(83 + 5) || isScriptRunning(84 + 5)) return;
    /* WORKAROUND ScummVM #4556: Пурпурное щупальце в лаборатории */
    if (script == 155 && VAR(120) == 1) return;
    runScript(script, 0, 0);
}

/* script_v2.cpp: stopScriptCommon */
static void stopScriptCommon(uint16_t script)
{
    /* WORKAROUND ScummVM #4112: свет в лаборатории */
    if (roomResource == 4 && isScriptRunning(138 + 5) && slots[currentScript].number == 163 &&
        script == 138 + 5)
        putState(157, getState(157) & (uint8_t)~kObjectState_08);
    /* FIXME ScummVM #915575: выход из комнаты 26 не останавливает 116 при подземелье */
    if (roomResource == 26 && slots[currentScript].number == 10001)
        if (script == 111 + 5 && isScriptRunning(84 + 5)) return;
    if (script == 0) script = slots[currentScript].number;
    if (currentScript != 0 && slots[currentScript].number == script)
        stopObjectCode();
    else
        stopScript(script);
}

static void walkActorToObject(uint8_t actor, uint16_t obj)
{
    int16_t x, y, dir;
    struct AdjustBoxResult r;
    getObjectXYPos(obj, &x, &y, &dir);
    adjustXYToBeInBox(actor, x, y, &r);
    startWalkActor(actor, r.x, r.y, dir);
}

static void o2_putActorAtObject(void)
{
    uint8_t a = gvdb(PARAM_1);
    uint16_t obj = (uint16_t)gvdw(PARAM_2);
    int16_t x, y;
    if (whereIsObject(obj) != WIO_NOT_FOUND)
    {
        int16_t dir;
        struct AdjustBoxResult r;
        getObjectXYPos(obj, &x, &y, &dir);
        adjustXYToBeInBox(a, x, y, &r);
        x = r.x;
        y = r.y;
    }
    else
    {
        x = 30;
        y = 60;
    }
    putActor(a, x, y, (uint8_t)actor_get(a, AF_ROOM));
}

static void o2_putActorInRoom(void)
{
    uint8_t act = gvdb(PARAM_1);
    uint8_t room = gvdb(PARAM_2);
    actor_set_room(act, room);
    if (!room) putActor(act, 0, 0, 0);
}

static void o2_lights(void)
{
    uint8_t a = gvdb(PARAM_1);
    uint8_t b = fetchScriptByte();
    uint8_t c = fetchScriptByte();
    if (c == 0)   /* MM v1: 2 — свет, 1 — фонарик, иначе темно -> LIGHTMODE_* (11, 4, 0) */
        VAR(VAR_CURRENT_LIGHTS) = a == 2 ? 11 : a == 1 ? 4 : 0;
    else if (c == 1)
    {
        flashlight_xStrips = a;
        flashlight_yStrips = b;
    }
    fullRedraw = 1;
}

static void o2_loadRoomWithEgo(void)
{
    uint16_t obj = (uint16_t)gvdw(PARAM_1);
    uint8_t room = gvdb(PARAM_2);
    uint8_t a = (uint8_t)VAR(VAR_EGO);
    /* MM: исходный интерпретатор оставляет актёру прежние x/y (скрипт 161, «Oomph!»);
     * ScummVM передаёт сюда getPos() — уже в точках, как и здесь */
    putActor(a, (int16_t)(actor_get(a, AF_X) * V12_X_MULTIPLIER), (int16_t)(actor_get(a, AF_Y) * V12_Y_MULTIPLIER), room);
    egoPositioned = 0;
    int8_t x = (int8_t)fetchScriptByte();
    int8_t y = (int8_t)fetchScriptByte();
    startScene((uint8_t)actor_get(a, AF_ROOM), a, obj);
    int16_t x2, y2, dir;
    struct AdjustBoxResult r;
    getObjectXYPos(obj, &x2, &y2, &dir);
    adjustXYToBeInBox(a, x2, y2, &r);
    putActor(a, r.x, r.y, currentRoom);
    setActorDirection(a, dir + 180);
    camera.dest_x = camera.cur_x = (int16_t)(actor_get(a, AF_X) * V12_X_MULTIPLIER);
    setCameraAt(camera.cur_x);
    setCameraFollows(a, 0);
    fullRedraw = 1;
    resetSentence();
    if (x >= 0 && y >= 0) startWalkActor(a, x, y, -1);
    runScript(5, 0, 0);
}

static void o2_delay(void)
{
    int32_t d = fetchScriptByte();
    d |= (int32_t)fetchScriptByte() << 8;
    d |= (int32_t)fetchScriptByte() << 16;
    slots[currentScript].delay = 0xFFFFFFL - d;
    slots[currentScript].status = ssPaused;
    breakHere();
}

static void o2_roomOps(void)
{
    int16_t a = gvdb(PARAM_1);
    int16_t b = gvdb(PARAM_2);
    uint8_t op = fetchScriptByte();
    switch (op & 0x1F)
    {
    case 1:   /* SO_ROOM_SCROLL */
        a *= 8;
        b *= 8;
        if (a < 160) a = 160;
        if (b < 160) b = 160;
        if (a > (int16_t)roomWidth - 160) a = (int16_t)roomWidth - 160;
        if (b > (int16_t)roomWidth - 160) b = (int16_t)roomWidth - 160;
        VAR(VAR_CAMERA_MIN_X) = a;
        VAR(VAR_CAMERA_MAX_X) = b;
        break;
    case 2:   /* SO_ROOM_COLOR */
        roomPalette[b & 15] = (uint8_t)a;
        fullRedraw = 1;
        break;
    }
}

static void o2_cutscene(void)
{
    cutSceneData[0] = userState | (userPut ? 16 : 0);
    cutSceneData[1] = VAR(VAR_CURSORSTATE);
    cutSceneData[2] = currentRoom;
    cutSceneData[3] = camera.mode;
    VAR(VAR_CURSORSTATE) = 200;
    setUserState(USERSTATE_SET_IFACE | USERSTATE_SET_CURSOR | USERSTATE_SET_FREEZE | USERSTATE_FREEZE_ON);
    sentenceNum = 0;
    stopScript(SENTENCE_SCRIPT);
    resetSentence();
    cutScenePtr[0] = 0;
}

static void o2_endCutscene(void)
{
    cutSceneStackPointer = 0;
    VAR(VAR_OVERRIDE) = 0;
    cutSceneScript[0] = 0;
    cutScenePtr[0] = 0;
    VAR(VAR_CURSORSTATE) = cutSceneData[1];
    setUserState((uint8_t)(cutSceneData[0] | USERSTATE_SET_IFACE | USERSTATE_SET_CURSOR | USERSTATE_SET_FREEZE));
    camera.mode = (uint8_t)cutSceneData[3];
    if (camera.mode == kFollowActorCameraMode)
        actorFollowCamera((uint8_t)VAR(VAR_EGO));
    else if ((uint8_t)cutSceneData[2] != currentRoom)
        startScene((uint8_t)cutSceneData[2], 0, 0);
}

static void o2_pickupObject(void)
{
    uint16_t obj = (uint16_t)gvdw(PARAM_1);
    if (obj < 1) fatal("PICKUP", obj);
    if (getObjectIndex(obj) == -1) return;
    if (whereIsObject(obj) == WIO_INVENTORY) return;
    addObjectToInventory(obj, roomResource);
    markObjectRectAsDirty(obj);
    putOwner(obj, (uint8_t)VAR(VAR_EGO));
    putState(obj, getState(obj) | kObjectState_08 | kObjectStateUntouchable);
    clearDrawObjectQueue();
    redrawV2Inventory();   /* runInventoryScript(1) */
}

/* script_v2.cpp: setUserState */
void setUserState(uint8_t state)
{
    uint8_t old = userState;
    if (state & USERSTATE_SET_IFACE) userState = state & USERSTATE_IFACE_ALL;
    if (state & USERSTATE_SET_FREEZE)
    {
        if (state & USERSTATE_FREEZE_ON) freezeScripts();
        else unfreezeScripts();
    }
    if (state & USERSTATE_SET_CURSOR)
    {
        if (state & USERSTATE_CURSOR_ON)
        {
            userPut = 1;
            cursorState = 1;
        }
        else
        {
            userPut = 0;
            cursorState = 0;
        }
    }
    if (userState == old && verbs_same_state()) return;   /* стереть и нарисовать то же — мигание */
    clearVerbArea();   /* restoreBackground: вся область глаголов и инвентаря */
    redrawVerbs();
    redrawV2Inventory();
}

/* script_v2.cpp: resetSentence */
void resetSentence(void)
{
    VAR(VAR_SENTENCE_VERB) = VAR(VAR_BACKUP_VERB);
    VAR(VAR_SENTENCE_OBJECT1) = 0;
    VAR(VAR_SENTENCE_OBJECT2) = 0;
    VAR(VAR_SENTENCE_PREPOSITION) = 0;
}

static void o4_saveLoadGame(void)
{
    getResultPos();
    uint8_t a = gvdb(PARAM_1);
    uint8_t result = 0;
    switch (a & 0xE0)
    {
    case 0x00: result = 100; break;   /* число мест */
    case 0x20: result = 3; break;     /* диск: «жёсткий» */
    case 0x40: result = 5; break;     /* загрузка — пока не умеем */
    case 0x80: result = 2; break;     /* запись — пока не умеем */
    case 0xC0: result = 7; break;     /* записи нет */
    default: fatal("SAVELOAD", a);
    }
    setResult(result);
}

/* script_v2.cpp: decodeParseString — строка реплики в буфер string.c */
void executeOpcode(uint8_t op)
{
    int16_t a, b;
    uint16_t ua, ub;
    uint8_t act, x, y;
    switch (opcode_handler[op])
    {
    case OP_o5_stopObjectCode: stopObjectCode(); break;
    case OP_o2_putActor: o2_putActor(); break;
    case OP_o5_startMusic: addSoundToQueue(gvdb(PARAM_1)); break;
    case OP_o5_getActorRoom:
        getResultPos();
        act = gvdb(PARAM_1);
        setResult(actor_get(act, AF_ROOM));
        break;
    case OP_o2_isGreaterEqual:
        ua = (uint16_t)getVar();
        ub = (uint16_t)gvdw(PARAM_1);
        jumpRelative(ub >= ua);
        break;
    case OP_o2_drawObject: o2_drawObject(); break;
    case OP_o2_getActorElevation:
        getResultPos();
        act = gvdb(PARAM_1);
        setResult(actor_get(act, AF_ELEVATION));
        break;
    case OP_o2_setState08:
        ua = (uint16_t)gvdw(PARAM_1);
        putState(ua, getState(ua) | kObjectState_08);
        markObjectRectAsDirty(ua);
        clearDrawObjectQueue();
        break;
    case OP_o5_isNotEqual:
        a = getVar();
        b = gvdw(PARAM_1);
        jumpRelative(b != a);
        break;
    case OP_o5_faceActor:
        act = gvdb(PARAM_1);
        ua = (uint16_t)gvdw(PARAM_2);
        faceToObject(act, ua);
        break;
    case OP_o2_assignVarWordIndirect:
        getResultPosIndirect();
        setResult(gvdw(PARAM_1));
        break;
    case OP_o2_setObjPreposition:
    {
        ua = (uint16_t)gvdw(PARAM_1);
        uint8_t unk = fetchScriptByte();
        if (whereIsObject(ua) != WIO_NOT_FOUND)
        {
            uint16_t p;
            uint8_t pg = getOBCDFromObject(ua, &p);
            far_wb(pg, p + 12, (uint8_t)((far_rb(pg, p + 12) & 0x1F) | (unk << 5)));
            vm_flush();
        }
        break;
    }
    case OP_o2_resourceRoutines: o2_resourceRoutines(); break;
    case OP_o5_walkActorToActor:
    {
        act = gvdb(PARAM_1);
        uint8_t nr2 = gvdb(PARAM_2);
        uint8_t dist = fetchScriptByte();
        if ((uint8_t)actor_get(act, AF_ROOM) != currentRoom) break;
        if ((uint8_t)actor_get(nr2, AF_ROOM) != currentRoom) break;
        walkActorToActor(act, nr2, dist);
        break;
    }
    case OP_o2_putActorAtObject: o2_putActorAtObject(); break;
    case OP_o2_ifNotState08: ifNotStateCommon(kObjectState_08); break;
    case OP_o5_getObjectOwner:
        getResultPos();
        setResult(getOwner((uint16_t)gvdw(PARAM_1)));
        break;
    case OP_o5_animateActor:
        act = gvdb(PARAM_1);
        x = gvdb(PARAM_2);
        animateActor(act, x);
        break;
    case OP_o2_panCameraTo: panCameraTo((int16_t)(gvdb(PARAM_1) * V12_X_MULTIPLIER)); break;
    case OP_o2_actorOps:
    {
        act = gvdb(PARAM_1);
        x = gvdb(PARAM_2);
        uint8_t sub = fetchScriptByte();
        extern uint8_t string0_color;
        if (act == 0 && sub == 5)
        {
            string0_color = x;   /* цвет реплик по умолчанию из boot-скрипта */
            break;
        }
        switch (sub)
        {
        case 1: setActorSound(act, x); break;
        case 2: setActorPalette(act, act, x); break;   /* v1: индекс палитры — номер актёра */
        case 3:
        {
            uint8_t pg = loadPtrToResource();
            actor_set_name(act, pg, res_addr);
            break;
        }
        case 4: setActorCostume(act, x); break;
        case 5: setActorTalkColor(act, x); break;
        default: fatal("ACTOROPS", sub);
        }
        break;
    }
    case OP_o5_print: decodeParseString(gvdb(PARAM_1)); break;
    case OP_o2_actorFromPos:
        getResultPos();
        a = (int16_t)(gvdb(PARAM_1) * V12_X_MULTIPLIER);
        b = (int16_t)(gvdb(PARAM_2) * V12_Y_MULTIPLIER);
        setResult(getActorFromPos(a, b));
        break;
    case OP_o5_getRandomNr:
        getResultPos();
        setResult((int16_t)getRandomNumber(gvdb(PARAM_1)));
        break;
    case OP_o2_clearState02: clearStateCommon(kObjectStateUntouchable); break;
    case OP_o5_jumpRelative: jumpRelative(0); break;
    case OP_o2_doSentence: o2_doSentence(); break;
    case OP_o5_move:
        getResultPos();
        setResult(gvdw(PARAM_1));
        break;
    case OP_o2_setBitVar:
    {
        uint16_t var = fetchScriptWord() + gvdb(PARAM_1);
        uint16_t bit = 1u << (var & 0x0f);
        var >>= 4;
        if (gvdb(PARAM_2)) scummVars[var] |= (int16_t)bit;
        else scummVars[var] &= (int16_t)~bit;
        break;
    }
    case OP_o5_startSound:
        VAR(VAR_MUSIC_TIMER) = 0;
        addSoundToQueue(gvdb(PARAM_1));
        break;
    case OP_o2_ifClassOfIs: o2_ifClassOfIs(); break;
    case OP_o2_walkActorTo:
        act = gvdb(PARAM_1);
        x = gvdb(PARAM_2);
        y = gvdb(PARAM_3);
        startWalkActor(act, x, y, -1);
        break;
    case OP_o2_ifState02: ifStateCommon(kObjectStateUntouchable); break;
    case OP_o5_stopMusic: stopAllSounds(); break;
    case OP_o4_saveLoadGame: o4_saveLoadGame(); break;
    case OP_o2_getActorY:
        getResultPos();
        setResult(getObjY(gvdb(PARAM_1)));
        break;
    case OP_o2_loadRoomWithEgo: o2_loadRoomWithEgo(); break;
    case OP_o5_setVarRange:
    {
        getResultPos();
        uint8_t n = fetchScriptByte();
        do
        {
            if (opcode & 0x80) a = (int16_t)fetchScriptWord();
            else a = fetchScriptByte();
            setResult(a);
            resultVarNumber++;
        } while (--n);
        break;
    }
    case OP_o2_setState04: setStateCommon(kObjectStateLocked); break;
    case OP_o5_equalZero:
        a = getVar();
        jumpRelative(a == 0);
        break;
    case OP_o2_setOwnerOf:
        ua = (uint16_t)gvdw(PARAM_1);
        x = gvdb(PARAM_2);
        setOwnerOf(ua, x);
        break;
    case OP_o2_addIndirect:
        getResultPosIndirect();
        scummVars[resultVarNumber] += gvdw(PARAM_1);
        break;
    case OP_o5_delayVariable:
        slots[currentScript].delay = getVar();
        slots[currentScript].status = ssPaused;
        breakHere();
        break;
    case OP_o2_assignVarByte:
        getResultPos();
        setResult(fetchScriptByte());
        break;
    case OP_o2_putActorInRoom: o2_putActorInRoom(); break;
    case OP_o2_delay: o2_delay(); break;
    case OP_o2_ifNotState04: ifNotStateCommon(kObjectStateLocked); break;
    case OP_o3_setBoxFlags:
        x = gvdb(PARAM_1);
        y = fetchScriptByte();
        setBoxFlags(x, y);
        break;
    case OP_o2_getBitVar:
    {
        getResultPos();
        uint16_t var = fetchScriptWord() + gvdb(PARAM_1);
        uint16_t bit = 1u << (var & 0x0f);
        var >>= 4;
        setResult(((uint16_t)scummVars[var] & bit) ? 1 : 0);
        break;
    }
    case OP_o2_setCameraAt: setCameraAtEx((int16_t)(gvdb(PARAM_1) * V12_X_MULTIPLIER)); break;
    case OP_o2_roomOps: o2_roomOps(); break;
    case OP_o5_getDist:
        getResultPos();
        ua = (uint16_t)gvdw(PARAM_1);
        ub = (uint16_t)gvdw(PARAM_2);
        setResult(getObjActToObjActDist(ua, ub));
        break;
    case OP_o2_findObject:
        getResultPos();
        a = (int16_t)(gvdb(PARAM_1) * V12_X_MULTIPLIER);
        b = (int16_t)(gvdb(PARAM_2) * V12_Y_MULTIPLIER);
        setResult((int16_t)findObject(a, b));
        break;
    case OP_o2_walkActorToObject:
        act = gvdb(PARAM_1);
        ua = (uint16_t)gvdw(PARAM_2);
        if (whereIsObject(ua) != WIO_NOT_FOUND) walkActorToObject(act, ua);
        break;
    case OP_o2_setState01: setStateCommon(kObjectStatePickupable); break;
    case OP_o2_isLessEqual:
        ua = (uint16_t)getVar();
        ub = (uint16_t)gvdw(PARAM_1);
        jumpRelative(ub <= ua);
        break;
    case OP_o2_subtract:
        getResultPos();
        scummVars[resultVarNumber] -= gvdw(PARAM_1);
        break;
    case OP_o2_waitForActor:
        act = gvdb(PARAM_1);
        if (actor_get(act, AF_MOVING))
        {
            sp_pc -= 2;
            breakHere();
        }
        break;
    case OP_o5_stopSound: stopSound(gvdb(PARAM_1)); break;
    case OP_o2_setActorElevation:
        act = gvdb(PARAM_1);
        a = (int8_t)gvdb(PARAM_2);
        setActorElevation(act, a);
        break;
    case OP_o2_ifNotState01: ifNotStateCommon(kObjectStatePickupable); break;
    case OP_o2_cutscene: o2_cutscene(); break;
    case OP_o2_startScript: o2_startScript(); break;
    case OP_o2_getActorX:
        getResultPos();
        setResult(getObjX(gvdb(PARAM_1)));
        break;
    case OP_o2_isLess:
        ua = (uint16_t)getVar();
        ub = (uint16_t)gvdw(PARAM_1);
        jumpRelative(ub < ua);
        break;
    case OP_o5_increment:
        getResultPos();
        setResult(readVar(resultVarNumber) + 1);
        break;
    case OP_o2_clearState08:
        ua = (uint16_t)gvdw(PARAM_1);
        putState(ua, getState(ua) & (uint8_t)~kObjectState_08);
        markObjectRectAsDirty(ua);
        clearDrawObjectQueue();
        break;
    case OP_o5_isEqual:
        a = readVar(fetchScriptByte());
        b = gvdw(PARAM_1);
        jumpRelative(b == a);
        break;
    case OP_o2_chainScript:
        x = gvdb(PARAM_1);
        stopScript(slots[currentScript].number);
        currentScript = 0xFF;
        runScript(x, 0, 0);
        break;
    case OP_o2_waitForSentence:
        if (!sentenceNum && !isScriptInUse(SENTENCE_SCRIPT)) break;
        sp_pc--;
        breakHere();
        break;
    case OP_o2_ifState08: ifStateCommon(kObjectState_08); break;
    case OP_o2_pickupObject: o2_pickupObject(); break;
    case OP_o5_actorFollowCamera: actorFollowCamera(gvdb(0x80)); break;
    case OP_o5_setObjectName: setObjectName((uint16_t)gvdw(PARAM_1)); break;
    case OP_o5_getActorMoving:
        getResultPos();
        act = gvdb(PARAM_1);
        setResult(actor_get(act, AF_MOVING));
        break;
    case OP_o2_setState02: setStateCommon(kObjectStateUntouchable); break;
    case OP_o2_beginOverride:
        cutScenePtr[0] = sp_pc;
        cutSceneScript[0] = currentScript;
        fetchScriptByte();   /* пропустить следующий переход */
        fetchScriptWord();
        break;
    case OP_o2_add:
        getResultPos();
        scummVars[resultVarNumber] += gvdw(PARAM_1);
        break;
    case OP_o2_dummy: break;
    case OP_o2_ifNotState02: ifNotStateCommon(kObjectStateUntouchable); break;
    case OP_o2_cursorCommand:
    {
        uint16_t cmd = (uint16_t)gvdw(PARAM_1);
        if (cmd & 0xFF) VAR(VAR_CURSORSTATE) = cmd & 0xFF;
        setUserState((uint8_t)(cmd >> 8));
        break;
    }
    case OP_o2_stopScript: stopScriptCommon(gvdb(PARAM_1)); break;
    case OP_o5_getActorFacing:
    {
        getResultPos();
        act = gvdb(PARAM_1);
        int16_t f = actor_get(act, AF_FACING);   /* newDirToOldDir */
        uint8_t d = 3;
        if (f >= 71 && f <= 109) d = 1;
        else if (f >= 109 && f <= 251) d = 2;
        else if (f >= 251 && f <= 289) d = 0;
        setResult(d);
        break;
    }
    case OP_o5_getClosestObjActor:
    {
        uint8_t closest_obj = 0xFF, closest_dist = 0xFF;
        getResultPos();
        ua = (uint16_t)gvdw(PARAM_1);
        a = VAR(VAR_ACTOR_RANGE_MAX);
        do
        {
            uint8_t dist = getObjActToObjActDist(ua, (uint16_t)a);
            if (dist < closest_dist)
            {
                closest_dist = dist;
                closest_obj = (uint8_t)a;
            }
        } while (--a >= VAR(VAR_ACTOR_RANGE_MIN));
        setResult(closest_obj);
        break;
    }
    case OP_o2_clearState04: clearStateCommon(kObjectStateLocked); break;
    case OP_o5_isScriptRunning:
        getResultPos();
        setResult(isScriptRunning(gvdb(PARAM_1)));
        break;
    case OP_o2_subIndirect:
        getResultPosIndirect();
        scummVars[resultVarNumber] -= gvdw(PARAM_1);
        break;
    case OP_o2_getObjPreposition:
    {
        getResultPos();
        ua = (uint16_t)gvdw(PARAM_1);
        if (whereIsObject(ua) != WIO_NOT_FOUND)
        {
            uint16_t p;
            uint8_t pg = getOBCDFromObject(ua, &p);
            setResult(far_rb(pg, p + 12) >> 5);
        }
        else
            setResult(0xFF);
        break;
    }
    case OP_o2_ifState04: ifStateCommon(kObjectStateLocked); break;
    case OP_o2_lights: o2_lights(); break;
    case OP_o5_getActorCostume:
        getResultPos();
        act = gvdb(PARAM_1);
        setResult(actor_get(act, AF_COSTUME));
        break;
    case OP_o5_loadRoom:
        x = gvdb(PARAM_1);
        if (x != currentRoom) startScene(x, 0, 0);   /* GF_SMALL_HEADER */
        fullRedraw = 1;
        break;
    case OP_o2_clearState01: clearStateCommon(kObjectStatePickupable); break;
    case OP_o2_isGreater:
        ua = (uint16_t)getVar();
        ub = (uint16_t)gvdw(PARAM_1);
        jumpRelative(ub > ua);
        break;
    case OP_o2_verbOps: o2_verbOps(); break;
    case OP_o2_getActorWalkBox:
        getResultPos();
        act = gvdb(PARAM_1);
        if ((uint8_t)actor_get(act, AF_ROOM) == currentRoom) setResult(actor_get(act, AF_WALKBOX));
        else setResult(0xFF);
        break;
    case OP_o5_isSoundRunning:
        getResultPos();
        x = gvdb(PARAM_1);
        if (x) x = isSoundRunning(x);
        setResult(x);
        break;
    case OP_o2_ifState01: ifStateCommon(kObjectStatePickupable); break;
    case OP_o5_breakHere: breakHere(); break;
    case OP_o2_restart: restart();
    case OP_o5_notEqualZero:
        a = getVar();
        jumpRelative(a != 0);
        break;
    case OP_o2_switchCostumeSet: break;   /* o2_dummy на PC */
    case OP_o2_drawSentence: drawSentence(); break;
    case OP_o2_waitForMessage:
        if (VAR(VAR_HAVE_MSG))
        {
            sp_pc--;
            breakHere();
        }
        break;
    case OP_o5_decrement:
        getResultPos();
        setResult(readVar(resultVarNumber) - 1);
        break;
    case OP_o5_pseudoRoom:
    {
        uint8_t i = fetchScriptByte(), j;
        while ((j = fetchScriptByte()) != 0)
            if (j >= 0x80) resourceMapper[j & 0x7F] = i;
        break;
    }
    case OP_o5_printEgo: decodeParseString((uint8_t)VAR(VAR_EGO)); break;
    case OP_o2_endCutscene: o2_endCutscene(); break;
    default: fatal("OPCODE", op);
    }
}
