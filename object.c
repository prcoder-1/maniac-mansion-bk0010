/* Объекты (банк A): перевод engines/scumm/object.cpp для v2.
 * Объекты комнаты — objs[] в базовом ОЗУ; код объектов (OBCD) лежит в блоке
 * комнаты, у предметов инвентаря — копия OBCD в куче (rtInventory). */
#include "vm.h"

uint16_t inventory[NUM_INVENTORY];
static uint8_t inv_page[NUM_INVENTORY];
static uint16_t inv_addr[NUM_INVENTORY];

static uint16_t newNames[NUM_NEW_NAMES];
static uint8_t name_page[NUM_NEW_NAMES];
static uint16_t name_addr[NUM_NEW_NAMES];

static uint8_t drawObjectQue[32];
static uint8_t drawObjectQueNr;

uint8_t inventory_res(uint8_t idx, uint16_t *addr)
{
    *addr = inv_addr[idx];
    return inv_page[idx];
}

/* --- владельцы и состояния --- */

uint8_t getOwner(uint16_t obj)
{
    if (obj >= numGlobalObjects) fatal("OBJ OWNER", obj);
    return objectOwnerTable[obj];
}

void putOwner(uint16_t obj, uint8_t owner)
{
    if (obj >= numGlobalObjects) fatal("OBJ PUTOWNER", obj);
    objectOwnerTable[obj] = owner;
}

uint8_t getState(uint16_t obj)
{
    if (obj >= numGlobalObjects) fatal("OBJ STATE", obj);
    /* ScummVM без защиты от копирования: дверь лаборатории всегда открыта */
    if (obj == 182 || obj == 193) objectStateTable[obj] |= kObjectState_08;
    return objectStateTable[obj];
}

/* состояния объектов комнаты (objs[].state) отстают от таблицы: updateObjectStates
 * переписывает их, только если таблица или объекты комнаты менялись */
static uint8_t states_dirty = 1;

void putState(uint16_t obj, uint8_t state)
{
    if (obj >= numGlobalObjects) fatal("OBJ PUTSTATE", obj);
    objectStateTable[obj] = state;
    states_dirty = 1;
}

int8_t getObjectIndex(uint16_t object)
{
    if (object < 1) return -1;
    for (int8_t i = NUM_LOCAL_OBJECTS - 1; i > 0; i--)
        if (objs[i].obj_nr == object) return i;
    return -1;
}

uint8_t whereIsObject(uint16_t object)
{
    if (object >= numGlobalObjects || object < 1) return WIO_NOT_FOUND;
    if (objectOwnerTable[object] != OF_OWNER_ROOM)
    {
        for (uint8_t i = 0; i < NUM_INVENTORY; i++)
            if (inventory[i] == object) return WIO_INVENTORY;
        return WIO_NOT_FOUND;
    }
    for (int8_t i = NUM_LOCAL_OBJECTS - 1; i > 0; i--)
        if (objs[i].obj_nr == object) return WIO_ROOM;
    return WIO_NOT_FOUND;
}

/* object.cpp: getOBCDFromObject -> страница (0 — нет), адрес в *addr */
uint8_t getOBCDFromObject(uint16_t obj, uint16_t *addr)
{
    if (objectOwnerTable[obj] != OF_OWNER_ROOM)
    {
        for (uint8_t i = 0; i < NUM_INVENTORY; i++)
            if (inventory[i] == obj) return inventory_res(i, addr);
        return 0;
    }
    for (int8_t i = NUM_LOCAL_OBJECTS - 1; i > 0; --i)
        if (objs[i].obj_nr == obj)
        {
            *addr = room_addr + objs[i].OBCDoffset;
            return room_page;
        }
    return 0;
}

/* object.cpp: getObjOrActorName */
uint8_t getObjOrActorName(uint16_t obj, uint16_t *addr)
{
    if (obj < NUM_ACTORS) return actor_name((uint8_t)obj, addr);
    for (uint8_t i = 0; i < NUM_NEW_NAMES; i++)
        if (newNames[i] == obj)
        {
            *addr = name_addr[i];
            return name_page[i];
        }
    uint16_t p;
    uint8_t pg = getOBCDFromObject(obj, &p);
    if (!pg) return 0;
    *addr = p + far_rb(pg, p + 14);
    return pg;
}

/* object.cpp: setObjectName */
void setObjectName(uint16_t obj)
{
    if (obj < NUM_ACTORS) fatal("ACTOR NAME", obj);
    for (uint8_t i = 0; i < NUM_NEW_NAMES; i++)
        if (newNames[i] == obj)
        {
            res_free(name_page[i], name_addr[i]);
            newNames[i] = 0;
            break;
        }
    for (uint8_t i = 0; i < NUM_NEW_NAMES; i++)
        if (newNames[i] == 0)
        {
            name_page[i] = loadPtrToResource();
            name_addr[i] = res_addr;
            newNames[i] = obj;
            redrawV2Inventory();   /* runInventoryScript(0) */
            return;
        }
    fatal("NEW NAMES", obj);
}

/* script.cpp: killScriptsAndResources — имена объектов, принадлежащих комнате */
void nukeRoomObjectNames(void)
{
    for (uint8_t i = 0; i < NUM_NEW_NAMES; i++)
    {
        uint16_t obj = newNames[i];
        if (!obj) continue;
        uint8_t owner = getOwner(obj);
        if (owner == 0 || owner == OF_OWNER_ROOM)
        {
            newNames[i] = 0;
            res_free(name_page[i], name_addr[i]);
        }
    }
}

/* --- инвентарь --- */

/* object.cpp: addObjectToInventory + findObjectInRoom(foCodeHeader) для v2 */
void addObjectToInventory(uint16_t obj, uint8_t room)
{
    uint8_t rp = res_get(rtRoom, room);
    uint16_t r = res_addr;
    uint8_t numobj = far_rb(rp, r + 20);
    uint16_t search = r + 28, obcd = 0;
    for (uint8_t i = 0; i < numobj; i++, search += 2)
    {
        uint16_t o = r + far_rw(rp, search + 2 * numobj);
        if (far_rw(rp, o + 4) == obj)
        {
            obcd = o;
            break;
        }
    }
    if (!obcd) fatal("INV OBJ", obj);
    uint16_t size = far_rw(rp, obcd);
    uint8_t slot;
    for (slot = 0; slot < NUM_INVENTORY; slot++)
        if (inventory[slot] == 0) break;
    if (slot == NUM_INVENTORY) fatal("INVENTORY FULL", obj);
    inventory[slot] = obj;
    inv_page[slot] = (uint8_t)res_alloc(size);
    inv_addr[slot] = res_addr;
    far_cp(inv_page[slot], inv_addr[slot], rp, obcd, size);
}

uint16_t findInventory(uint8_t owner, uint8_t idx)
{
    uint8_t count = 1;
    for (uint8_t i = 0; i < NUM_INVENTORY; i++)
    {
        uint16_t obj = inventory[i];
        if (obj && getOwner(obj) == owner && count++ == idx) return obj;
    }
    return 0;
}

uint8_t getInventoryCount(uint8_t owner)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < NUM_INVENTORY; i++)
    {
        uint16_t obj = inventory[i];
        if (obj && getOwner(obj) == owner) count++;
    }
    return count;
}

/* object.cpp: clearOwnerOf */
static void clearOwnerOf(uint16_t obj)
{
    stopObjectScript(obj);
    if (getOwner(obj) == OF_OWNER_ROOM) return;   /* плавающих объектов в v2 нет */
    for (uint8_t i = 0; i < NUM_INVENTORY; i++)
        if (inventory[i] == obj)
        {
            res_free(inv_page[i], inv_addr[i]);
            inventory[i] = 0;
            for (i = 0; i < NUM_INVENTORY - 1; i++)
                if (!inventory[i] && inventory[i + 1])
                {
                    inventory[i] = inventory[i + 1];
                    inventory[i + 1] = 0;
                    inv_page[i] = inv_page[i + 1];
                    inv_addr[i] = inv_addr[i + 1];
                }
            break;
        }
}

/* object.cpp: setOwnerOf */
void setOwnerOf(uint16_t obj, uint8_t owner)
{
    if (!obj) fatal("SETOWNER 0", 0);
    if (owner == 0) clearOwnerOf(obj);
    putOwner(obj, owner);
    redrawV2Inventory();   /* runInventoryScript(0) */
}

/* --- координаты --- */

/* object.cpp: getObjectXYPos (v2): точка подхода, в единицах актёра */
void getObjectXYPos(uint16_t object, int16_t *x, int16_t *y, int16_t *dir)
{
    int8_t idx = getObjectIndex(object);
    if (idx < 0) fatal("OBJ XY", object);
    struct ObjectData *od = &objs[idx];
    *x = od->walk_x >> 3;
    *y = od->walk_y >> 1;
    static const int16_t new_dir_table[4] = { 270, 90, 180, 0 };   /* oldDirToNewDir */
    *dir = new_dir_table[od->actordir & 3];
}

/* object.cpp: getObjectOrActorXY */
int8_t getObjectOrActorXY(uint16_t object, int16_t *x, int16_t *y)
{
    if (object < NUM_ACTORS) return actor_xy((uint8_t)object, x, y) == currentRoom ? 0 : -1;
    switch (whereIsObject(object))
    {
    case WIO_NOT_FOUND:
        return -1;
    case WIO_INVENTORY:
    {
        uint8_t owner = objectOwnerTable[object];
        return owner < NUM_ACTORS && actor_xy(owner, x, y) == currentRoom ? 0 : -1;
    }
    }
    int16_t dir;
    getObjectXYPos(object, x, y, &dir);
    return 0;
}

int16_t getObjX(uint16_t obj)
{
    if (obj < 1) return 0;
    if (obj < NUM_ACTORS) return actor_get((uint8_t)obj, AF_X);
    if (whereIsObject(obj) == WIO_NOT_FOUND) return -1;
    int16_t x, y;
    getObjectOrActorXY(obj, &x, &y);
    return x;
}

int16_t getObjY(uint16_t obj)
{
    if (obj < 1) return 0;
    if (obj < NUM_ACTORS) return actor_get((uint8_t)obj, AF_Y);
    if (whereIsObject(obj) == WIO_NOT_FOUND) return -1;
    int16_t x, y;
    getObjectOrActorXY(obj, &x, &y);
    return y;
}

/* object.cpp: getObjActToObjActDist */
uint8_t getObjActToObjActDist(uint16_t a, uint16_t b)
{
    int16_t x, y, x2, y2;
    uint8_t acta = a < NUM_ACTORS, actb = b < NUM_ACTORS;
    if (acta && actb)   /* два актёра: место и комната — одним дальним вызовом на каждого */
    {
        uint8_t ra = actor_xy((uint8_t)a, &x, &y), rb = actor_xy((uint8_t)b, &x2, &y2);
        if (ra == rb && ra && ra != currentRoom) return 0;
        if (ra != currentRoom || rb != currentRoom) return 0xFF;
    }
    else
    {
        if (getObjectOrActorXY(a, &x, &y) == -1) return 0xFF;
        if (getObjectOrActorXY(b, &x2, &y2) == -1) return 0xFF;
    }
    if (acta && !actb)
    {
        struct AdjustBoxResult r;
        adjustXYToBeInBox((uint8_t)a, x2, y2, &r);
        x2 = r.x;
        y2 = r.y;
    }
    int16_t dy = y > y2 ? y - y2 : y2 - y;   /* getDist */
    int16_t dx = x > x2 ? x - x2 : x2 - x;
    int16_t d = dy > dx ? dy : dx;
    return d > 0xFF ? 0xFF : (uint8_t)d;
}

/* object.cpp: findObject (v2: маска состояния 08, класс «неприкасаемый» не задаётся) */
uint16_t findObject(int16_t x, int16_t y)
{
    for (uint8_t i = 1; i < NUM_LOCAL_OBJECTS; i++)
    {
        struct ObjectData *o = &objs[i];
        if (o->obj_nr < 1) continue;
        if (o->state & kObjectStateUntouchable) continue;
        uint8_t b = i;
        for (;;)
        {
            uint8_t a = objs[b].parentstate;
            b = objs[b].parent;
            if (b == 0)
            {
                if (o->x_pos <= x && (int16_t)o->width + o->x_pos > x && o->y_pos <= y && (int16_t)o->height + o->y_pos > y)
                    return o->obj_nr;
                break;
            }
            if ((objs[b].state & kObjectState_08) != a) break;
        }
    }
    return 0;
}

/* --- отрисовка (сама картинка — gfx.c, банк C) --- */


void markObjectRectAsDirty(uint16_t obj)
{
    for (uint8_t i = 1; i < NUM_LOCAL_OBJECTS; i++)
        if (objs[i].obj_nr == obj)
        {
            /* фон мира хранится целиком: полосы вне экрана тоже (в ScummVM — только видимые) */
            if (objs[i].width != 0)
                markStripsDirty(objs[i].x_pos / 8, objs[i].x_pos / 8 + objs[i].width / 8, objs[i].y_pos,
                                objs[i].y_pos + (int16_t)objs[i].height);
            bgNeedsRedraw = 1;
            return;
        }
}

void addObjectToDrawQue(uint8_t object)
{
    if (drawObjectQueNr >= sizeof drawObjectQue) fatal("DRAW QUE", object);
    drawObjectQue[drawObjectQueNr++] = object;
}

void clearDrawObjectQueue(void)
{
    drawObjectQueNr = 0;
}

void processDrawQue(void)
{
    for (uint8_t i = 0; i < drawObjectQueNr; i++)
    {
        uint8_t j = drawObjectQue[i];
        if (j) drawObject(j);
    }
    drawObjectQueNr = 0;
}

/* object.cpp: updateObjectStates */
void updateObjectStates(void)
{
    if (!states_dirty) return;
    states_dirty = 0;
    /* объекты комнаты — в местах 1..numObjectsInRoom (resetRoomObjects) */
    struct ObjectData *od = &objs[1];
    for (uint8_t i = 1; i <= numObjectsInRoom; i++, od++)
    {
        uint16_t n = od->obj_nr;
        if (n)
        {
            if (n == 182 || n == 193) objectStateTable[n] |= kObjectState_08;   /* как getState */
            od->state = objectStateTable[n];
        }
    }
}

/* object.cpp: drawRoomObject / drawRoomObjects (v2: маска состояния 08) */
static void drawRoomObject(uint8_t i)
{
    struct ObjectData *od = &objs[i];
    if (i < 1 || od->obj_nr < 1 || !od->state) return;
    for (;;)
    {
        uint8_t a = od->parentstate;
        if (!od->parent)
        {
            drawObject(i);
            break;
        }
        od = &objs[od->parent];
        if ((od->state & kObjectState_08) != a) break;
    }
}

void drawRoomObjects(void)
{
    for (int8_t i = NUM_LOCAL_OBJECTS - 1; i > 0; i--)
        if (objs[i].obj_nr > 0 && (objs[i].state & kObjectState_08)) drawRoomObject((uint8_t)i);
}

/* то же для объектов, задевающих полосы [s0, s1) (перерисовка части фона) */
void drawRoomObjectsRange(uint8_t s0, uint8_t s1, uint8_t y0, uint8_t y1)
{
    for (int8_t i = NUM_LOCAL_OBJECTS - 1; i > 0; i--)
    {
        struct ObjectData *o = &objs[i];
        if (o->obj_nr == 0 || !(o->state & kObjectState_08)) continue;
        int16_t a = o->x_pos / 8, b = a + (int16_t)(o->width / 8);
        if (b <= s0 || a >= s1 || o->y_pos >= y1 || o->y_pos + (int16_t)o->height <= y0) continue;
        drawRoomObject((uint8_t)i);
        cursor_poll();
    }
}

/* object.cpp: ScummEngine_v3old::resetRoomObjects + ScummEngine_v4::resetRoomObject */
void resetRoomObjects(void)
{
    for (uint8_t i = 0; i < NUM_LOCAL_OBJECTS; i++) objs[i].obj_nr = 0;   /* clearRoomObjects */
    states_dirty = 1;
    if (numObjectsInRoom == 0) return;
    if (numObjectsInRoom >= NUM_LOCAL_OBJECTS) fatal("ROOM OBJECTS", numObjectsInRoom);
    uint16_t ptr = room_addr + 28;
    for (uint8_t i = 0; i < numObjectsInRoom; i++, ptr += 2)
    {
        uint8_t slot;   /* findLocalObjectSlot */
        for (slot = 1; slot < NUM_LOCAL_OBJECTS; slot++)
            if (!objs[slot].obj_nr) break;
        struct ObjectData *od = &objs[slot];
        od->OBIMoffset = far_rw(room_page, ptr);
        od->OBCDoffset = far_rw(room_page, ptr + 2 * numObjectsInRoom);
        uint8_t b[10];
        far_rd(b, room_page, room_addr + od->OBCDoffset + 4, 10);   /* ptr - 2 + 6 ... + 15 */
        od->obj_nr = (uint16_t)(b[0] | (b[1] << 8));
        od->x_pos = (int16_t)(b[3] * 8);
        od->y_pos = (int16_t)((b[4] & 0x7F) * 8);
        od->parentstate = (b[4] & 0x80) ? 8 : 0;
        od->width = (uint16_t)(b[5] * 8);
        od->parent = b[6];
        od->walk_x = (int16_t)(b[7] * 8);
        od->walk_y = (int16_t)((b[8] & 0x1f) * 8);
        od->actordir = b[9] & 7;
        od->height = b[9] & 0xf8;
        od->fl_object_index = 0;
        od->state = 0;
    }
}

/* сохранение игры (saveload.c): данные банка A — адрес (длина — в res_addr) */
extern uint8_t bdata_start[], bdata_end[];
uint16_t save_dataA(void)
{
    res_addr = (uint16_t)(bdata_end - bdata_start);
    return (uint16_t)bdata_start;
}

/* сохранение игры: блок кучи слота (page, addr) — освободить (0), дописать в
 * образ sv_page:sv_off (1: [длина][байты], пустой — длина 0) или выделить
 * заново из образа (2); копия в actor.c (банк B) */
void sv_heap(uint8_t *pg, uint16_t *addr, uint8_t valid, uint8_t mode)
{
    uint16_t len = 0;
    if (mode && !sv_off) return;   /* образ уже переполнен */
    if (mode == 0)
    {
        if (valid && *pg) res_free(*pg, *addr);
        return;
    }
    if (mode == 1)
    {
        if (valid && *pg) len = (far_rw(*pg, *addr - 2) & ~1u) - 2;
        if (sv_off + 2 + len > PAGE_ADDR + PAGE_DATA_SIZE)
        {
            sv_off = 0;   /* не помещается в страницу */
            return;
        }
        far_wr(sv_page, sv_off, &len, 2);
        if (len) far_cp(sv_page, sv_off + 2, *pg, *addr, len);
    }
    else
    {
        len = far_rw(sv_page, sv_off);
        *pg = 0;
        if (len)
        {
            uint8_t p = (uint8_t)res_alloc(len);
            uint16_t a = res_addr;
            far_cp(p, a, sv_page, sv_off + 2, len);
            *pg = p;
            *addr = a;
        }
    }
    sv_off += 2 + len;
}

/* блоки кучи инвентаря и новых имён объектов (sv_heap) */
void heapA(uint8_t mode)
{
    for (uint8_t i = 0; i < NUM_INVENTORY; i++) sv_heap(&inv_page[i], &inv_addr[i], inventory[i] != 0, mode);
    for (uint8_t i = 0; i < NUM_NEW_NAMES; i++) sv_heap(&name_page[i], &name_addr[i], newNames[i] != 0, mode);
}
