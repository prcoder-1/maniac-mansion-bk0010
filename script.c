/* Ядро интерпретатора SCUMM v2 (банк A): перевод engines/scumm/script.cpp.
 * Слоты скриптов, вложенный запуск, выборка байт-кода, планировщик, катсцены,
 * скрипты входа/выхода комнаты, предложения и ввода. */
#include "vm.h"

struct ScriptSlot slots[NUM_SCRIPT_SLOT];
uint8_t slot_hi = 1;   /* выше — места, ни разу не занятые (number 0, ssDead) */
static struct NestedScript nest[MAX_SCRIPT_NESTING];
static uint8_t numNestedScripts;
uint16_t cutScenePtr[5];
uint8_t cutSceneScript[5];
int16_t cutSceneData[5];
uint8_t cutSceneStackPointer;

uint8_t sp_page;
uint16_t sp_base, sp_pc;
uint8_t opcode;
uint16_t resultVarNumber;


/* --- выборка (script.cpp: fetchScriptByte / fetchScriptWord) --- */


/* промах текущей строки: строки — блоки по 32 байта, место — по адресу (прямое
 * отображение: проверка одной строки, без поиска) */
__attribute__((noinline)) static uint8_t vm_fetch_miss(uint16_t a)   /* иначе быстрый путь сохраняет r2..r5 */
{
    uint16_t b = a & ~(uint16_t)(VM_CACHE - 1);
    uint8_t i = (uint8_t)((b >> 5) & (VM_LINES - 1));
    if (vm_l_page[i] != sp_page || vm_l_addr[i] != b || vm_l_gen[i] != vm_c_gen)
    {
        far_rd(vm_lines[i], sp_page, b, VM_CACHE);   /* данные страницы кончаются на границе блока */
        vm_l_page[i] = sp_page;
        vm_l_addr[i] = b;
        vm_l_gen[i] = vm_c_gen;
    }
    vm_c_page = sp_page;
    vm_c_addr = b;
    vm_c_line = vm_lines[i];
    return vm_c_line[a - b];
}

uint8_t fetchScriptByte(void)
{
    uint16_t a = sp_base + sp_pc++;
    uint16_t d = a - vm_c_addr;
    if (sp_page == vm_c_page && d < VM_CACHE) return vm_c_line[d];
    return vm_fetch_miss(a);
}

uint16_t fetchScriptWord(void)
{
    uint16_t lo = fetchScriptByte();
    return (uint16_t)(lo | ((uint16_t)fetchScriptByte() << 8));
}

/* script.cpp: getScriptBaseAddress */
static void getScriptBaseAddress(void)
{
    if (currentScript == 0xFF) return;
    struct ScriptSlot *ss = &slots[currentScript];
    uint8_t pg = 0;
    switch (ss->where)
    {
    case WIO_INVENTORY:
    {
        uint8_t idx;
        for (idx = 0; idx < NUM_INVENTORY; idx++)
            if (inventory[idx] == ss->number) break;
        pg = idx < NUM_INVENTORY ? inventory_res(idx, &sp_base) : 0;
        break;
    }
    case WIO_LOCAL:
    case WIO_ROOM:
        pg = res_get(rtRoom, roomResource);
        sp_base = res_addr;
        break;
    case WIO_GLOBAL:
        pg = res_get(rtScript, ss->number);
        sp_base = res_addr;
        break;
    default:
        fatal("BAD WHERE", ss->where);
    }
    sp_page = pg;
    /* script.cpp: bug #1202487 — скрипта нет */
    if (!pg)
    {
        ss->status = ssDead;
        currentScript = 0xFF;
    }
}

static void resetScriptPointer(void)
{
    if (currentScript == 0xFF) return;
    sp_pc = slots[currentScript].offs;
}

void updateScriptPtr(void)
{
    if (currentScript == 0xFF) return;
    slots[currentScript].offs = sp_pc;
}

/* script.cpp: executeScript */
static void executeScript(void)
{
    while (currentScript != 0xFF)
    {
#ifdef HOST
        uint16_t at = sp_pc;
#endif
        opcode = fetchScriptByte();
#ifdef HOST
        trace_opcode(slots[currentScript].number, at, opcode);
#endif
        executeOpcode(opcode);
        SND_POLL();
    }
}

/* script.cpp: getScriptSlot */
static uint8_t getScriptSlot(void)
{
    for (uint8_t i = 1; i < NUM_SCRIPT_SLOT; i++)
        if (slots[i].status == ssDead) return i;
    fatal("TOO MANY SCRIPTS", NUM_SCRIPT_SLOT);
}

/* script.cpp: runScriptNested */
void runScriptNested(uint8_t script)
{
    updateScriptPtr();
    if (numNestedScripts >= MAX_SCRIPT_NESTING) fatal("NESTING", script);
    struct NestedScript *n = &nest[numNestedScripts];
    if (currentScript == 0xFF)
    {
        n->number = 0;
        n->where = 0xFF;
    }
    else
    {
        n->number = slots[currentScript].number;
        n->where = slots[currentScript].where;
        n->slot = currentScript;
    }
    numNestedScripts++;
    currentScript = script;
    getScriptBaseAddress();
    resetScriptPointer();
    executeScript();
    if (numNestedScripts) numNestedScripts--;
    if (n->number)
    {
        struct ScriptSlot *s = &slots[n->slot];
        if (s->number == n->number && s->where == n->where && s->status != ssDead && s->freezeCount == 0)
        {
            currentScript = n->slot;
            getScriptBaseAddress();
            resetScriptPointer();
            return;
        }
    }
    currentScript = 0xFF;
}

static void initSlot(uint8_t s, uint16_t number, uint16_t offs, uint8_t where, uint8_t fr, uint8_t rec)
{
    struct ScriptSlot *ss = &slots[s];
    if (s >= slot_hi) slot_hi = s + 1;
    ss->number = number;
    ss->offs = offs;
    ss->status = ssRunning;
    ss->where = where;
    ss->freezeResistant = fr;
    ss->recursive = rec;
    ss->freezeCount = 0;
}

/* script.cpp: runScript (в v2 нет локальных скриптов и переменных) */
void runScript(uint16_t script, uint8_t freezeResistant, uint8_t recursive)
{
    if (!script) return;
    if (!recursive) stopScript(script);
    res_get(rtScript, script);   /* getResourceAddress: загрузить */
    uint8_t s = getScriptSlot();
    initSlot(s, script, 4, WIO_GLOBAL, freezeResistant, recursive);   /* _resourceHeaderSize */
    runScriptNested(s);
}

/* script.cpp: runObjectScript */
void runObjectScript(uint16_t object, uint8_t entry, uint8_t freezeResistant, uint8_t recursive, int8_t slot)
{
    if (!object) return;
    uint8_t where = whereIsObject(object);
    if (where == WIO_NOT_FOUND) return;
    uint16_t obcd = 0;
    if (where == WIO_ROOM)
    {
        int8_t i = getObjectIndex(object);   /* getOBCDOffs */
        obcd = objs[i].OBCDoffset;
    }
    if (slot == -1) slot = (int8_t)getScriptSlot();
    uint8_t offs = getVerbEntrypoint(object, entry);
    if (offs == 0) return;
    initSlot((uint8_t)slot, object, obcd + offs, where, freezeResistant, recursive);
    runScriptNested((uint8_t)slot);
}

/* script.cpp: getVerbEntrypoint (v2: таблица пар (глагол, смещение) с OBCD+15) */
uint8_t getVerbEntrypoint(uint16_t obj, uint8_t entry)
{
    if (whereIsObject(obj) == WIO_NOT_FOUND) return 0;
    uint16_t a;
    uint8_t pg = getOBCDFromObject(obj, &a);
    if (!pg) return 0;
    a += 15;
    for (;;)
    {
        uint8_t v = far_rb(pg, a);
        if (!v) return 0;
        if (v == entry || v == 0xFF) break;
        a += 2;
    }
    return far_rb(pg, a + 1);
}

/* script.cpp: stopScript */
void stopScript(uint16_t script)
{
    if (script == 0) return;
    for (uint8_t i = 0; i < NUM_SCRIPT_SLOT; i++)
    {
        struct ScriptSlot *ss = &slots[i];
        if (script == ss->number && ss->status != ssDead && (ss->where == WIO_GLOBAL || ss->where == WIO_LOCAL))
        {
            ss->number = 0;
            ss->status = ssDead;
            if (currentScript == i) currentScript = 0xFF;
        }
    }
    for (uint8_t i = 0; i < numNestedScripts; ++i)
        if (nest[i].number == script && (nest[i].where == WIO_GLOBAL || nest[i].where == WIO_LOCAL))
        {
            nest[i].number = 0;
            nest[i].slot = 0xFF;
            nest[i].where = 0xFF;
        }
}

/* script.cpp: stopObjectScript */
void stopObjectScript(uint16_t script)
{
    if (script == 0) return;
    for (uint8_t i = 0; i < NUM_SCRIPT_SLOT; i++)
    {
        struct ScriptSlot *ss = &slots[i];
        if (script == ss->number && ss->status != ssDead &&
            (ss->where == WIO_ROOM || ss->where == WIO_INVENTORY || ss->where == WIO_FLOBJECT))
        {
            ss->number = 0;
            ss->status = ssDead;
            if (currentScript == i) currentScript = 0xFF;
        }
    }
    for (uint8_t i = 0; i < numNestedScripts; ++i)
        if (nest[i].number == script &&
            (nest[i].where == WIO_ROOM || nest[i].where == WIO_INVENTORY || nest[i].where == WIO_FLOBJECT))
        {
            nest[i].number = 0;
            nest[i].slot = 0xFF;
            nest[i].where = 0xFF;
        }
}

/* script.cpp: stopObjectCode (v2) */
void stopObjectCode(void)
{
    struct ScriptSlot *ss = &slots[currentScript];
    if (ss->where == WIO_GLOBAL || ss->where == WIO_LOCAL)
        stopScript(ss->number);
    else
    {
        ss->number = 0;
        ss->status = ssDead;
    }
    currentScript = 0xFF;
}

/* script.cpp: freezeScripts / unfreezeScripts (v2) */
void freezeScripts(void)
{
    for (uint8_t i = 0; i < NUM_SCRIPT_SLOT; i++)
        if (currentScript != i && slots[i].status != ssDead && !slots[i].freezeResistant)
            slots[i].status |= 0x80;
}

void unfreezeScripts(void)
{
    for (uint8_t i = 0; i < NUM_SCRIPT_SLOT; i++)
        slots[i].status &= 0x7F;
}

/* script.cpp: runAllScripts */
void runAllScripts(void)
{
    currentScript = 0xFF;
    for (uint8_t i = 0; i < slot_hi; i++)
    {
        if (slots[i].status == ssRunning)
        {
            currentScript = i;
            getScriptBaseAddress();
            resetScriptPointer();
            executeScript();
        }
        if (!(i & 7)) SND_POLL();
    }
}

/* script.cpp: decreaseScriptDelay */
void decreaseScriptDelay(int16_t amount)
{
    for (uint8_t i = 0; i < slot_hi; i++)
    {
        struct ScriptSlot *ss = &slots[i];
        if (ss->status == ssPaused)
        {
            ss->delay -= amount;
            if (ss->delay < 0)
            {
                ss->status = ssRunning;
                ss->delay = 0;
            }
        }
        if (!(i & 7)) SND_POLL();
    }
}

/* script.cpp: isScriptRunning / isScriptInUse */
uint8_t isScriptRunning(uint16_t script)
{
    for (uint8_t i = 0; i < NUM_SCRIPT_SLOT; i++)
        if (slots[i].number == script && (slots[i].where == WIO_GLOBAL || slots[i].where == WIO_LOCAL) &&
            slots[i].status != ssDead)
            return 1;
    return 0;
}

uint8_t isScriptInUse(uint16_t script)
{
    for (uint8_t i = 0; i < slot_hi; i++)
    {
        if (slots[i].number == script) return 1;
        if (!(i & 7)) SND_POLL();
    }
    return 0;
}

/* script.cpp: runExitScript / runEntryScript (в v2 — только EXCD/ENCD комнаты) */
static void runRoomScript(uint16_t number, uint16_t offs)
{
    uint8_t s = getScriptSlot();
    initSlot(s, number, offs, WIO_ROOM, 0, 0);
    runScriptNested(s);
}

void runExitScript(void)
{
    if (EXCD_offs) runRoomScript(10001, EXCD_offs);
}

void runEntryScript(void)
{
    if (ENCD_offs) runRoomScript(10002, ENCD_offs);
}

/* script.cpp: killScriptsAndResources */
void killScriptsAndResources(void)
{
    for (uint8_t i = 0; i < NUM_SCRIPT_SLOT; i++)
    {
        struct ScriptSlot *ss = &slots[i];
        if (ss->where == WIO_ROOM || ss->where == WIO_FLOBJECT || ss->where == WIO_LOCAL)
        {
            ss->cutsceneOverride = 0;
            ss->status = ssDead;
        }
    }
    /* новые имена объектов комнаты — object.c */
    extern void nukeRoomObjectNames(void);
    nukeRoomObjectNames();
}

/* script.cpp: checkAndRunSentenceScript (v2) */
struct SentenceTab { uint8_t verb, preposition; uint16_t objectA, objectB; uint8_t freezeCount, pad; };
extern struct SentenceTab sentence[NUM_SENTENCE];

void checkAndRunSentenceScript(void)
{
    for (uint8_t i = 0; i < slot_hi; i++)
        if (slots[i].number == SENTENCE_SCRIPT && slots[i].status != ssDead && slots[i].freezeCount == 0)
            return;
    if (!sentenceNum || sentence[sentenceNum - 1].freezeCount) return;
    sentenceNum--;
    struct SentenceTab *st = &sentence[sentenceNum];
    if (st->preposition && st->objectB == st->objectA) return;
    VAR(VAR_ACTIVE_VERB) = st->verb;
    VAR(VAR_ACTIVE_OBJECT1) = (int16_t)st->objectA;
    VAR(VAR_ACTIVE_OBJECT2) = (int16_t)st->objectB;
    VAR(VAR_VERB_ALLOWED) = getVerbEntrypoint(st->objectA, st->verb) != 0;
    currentScript = 0xFF;
    runScript(SENTENCE_SCRIPT, 0, 0);
}

/* script.cpp: ScummEngine_v2::runInputScript (аргументы скрипта в v2 не читаются) */
void runInputScript(uint8_t clickArea, uint16_t val, uint8_t mode)
{
    (void)mode;
    VAR(VAR_CLICK_AREA) = clickArea;
    if (clickArea == kVerbClickArea) VAR(VAR_CLICK_VERB) = (int16_t)val;
    else if (clickArea == kInventoryClickArea) VAR(VAR_CLICK_OBJECT) = (int16_t)val;
    runScript(4, 0, 0);
}

/* script.cpp: abortCutscene */
void abortCutscene(void)
{
    uint8_t idx = cutSceneStackPointer;
    uint16_t offs = cutScenePtr[idx];
    if (offs)
    {
        struct ScriptSlot *ss = &slots[cutSceneScript[idx]];
        ss->offs = offs;
        ss->status = ssRunning;
        ss->freezeCount = 0;
        if (ss->cutsceneOverride > 0) ss->cutsceneOverride--;
        VAR(VAR_OVERRIDE) = 1;
        cutScenePtr[idx] = 0;
    }
}

/* scumm.cpp: runBootscript */
void runBootscript(void)
{
    runScript(1, 0, 0);
}

void vm_reset(void)
{
    numNestedScripts = 0;
    cutSceneStackPointer = 0;
    for (uint8_t i = 0; i < 5; i++)
    {
        cutScenePtr[i] = 0;
        cutSceneData[i] = 0;
    }
    currentScript = 0xFF;
    vm_flush();
}

/* room.cpp: startScene — объектный скрипт текущей комнаты больше не продолжается */
void vm_leave_room_script(void)
{
    if (currentScript != 0xFF)
    {
        uint8_t w = slots[currentScript].where;
        if (w == WIO_ROOM || w == WIO_FLOBJECT || w == WIO_LOCAL) currentScript = 0xFF;
    }
}
