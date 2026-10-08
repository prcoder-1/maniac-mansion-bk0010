/* Комната (банк B): room.cpp (startScene, ScummEngine_v3old::setupRoomSubBlocks /
 * resetRoomSubBlocks) и боксы boxes.cpp для v2. */
#include "scumm.h"
#include "gfx.h"

/* боксы текущей комнаты: копия из блока комнаты (setBoxFlags их меняет), затем
 * матрица переходов numBoxes x (numBoxes + 1) — rtMatrix 2 и 1 в ScummVM */
#ifdef HOST
uint8_t boxes[BOXES_SIZE];
#else
extern uint8_t boxes[BOXES_SIZE];   /* sys.ld: базовое ОЗУ (в банке B нет места) */
#endif
static uint16_t matrix_off;

extern void vm_leave_room_script(void);

/* room.cpp: startScene */
void startScene(uint8_t room, uint8_t actor, uint16_t objectNr)
{
    (void)actor;
    (void)objectNr;
    stopTalk();
    vm_leave_room_script();
    runExitScript();
    killScriptsAndResources();
    clearDrawObjectQueue();
    for (uint8_t i = 1; i < NUM_ACTORS; i++) hideActor(i);
    for (uint8_t i = 0; i < 16; i++) roomPalette[i] = i;
    gfx_set_pal();
    VAR(VAR_ROOM) = room;
    fullRedraw = 1;
    currentRoom = room;
    roomResource = room >= 0x80 ? resourceMapper[room & 0x7F] : room;
    VAR(VAR_ROOM_RESOURCE) = roomResource;
    if (room != 0)
    {
        room_page = res_get(rtRoom, roomResource);
        room_addr = res_addr;
        res_lock_room(roomResource);
    }
    for (uint8_t i = 0; i < NUM_LOCAL_OBJECTS; i++) objs[i].obj_nr = 0;   /* clearRoomObjects */
    if (currentRoom == 0)
    {
        ENCD_offs = EXCD_offs = 0;
        numObjectsInRoom = 0;
        return;
    }
    /* setupRoomSubBlocks (v3old) */
    roomWidth = far_rw(room_page, room_addr + 4);
    roomHeight = far_rw(room_page, room_addr + 6);
    numObjectsInRoom = far_rb(room_page, room_addr + 20);
    IM00_offs = far_rw(room_page, room_addr + 0x0A);
    EXCD_offs = far_rw(room_page, room_addr + 0x18);
    ENCD_offs = far_rw(room_page, room_addr + 0x1A);
    /* resetRoomSubBlocks: боксы и матрица */
    uint16_t b = room_addr + far_rb(room_page, room_addr + 0x15);
    uint8_t n = far_rb(room_page, b);
    uint16_t size = (uint16_t)(n * 8 + 1 + n * (n + 1));
    if (size > sizeof boxes) fatal("BOXES", n);
    far_rd(boxes, room_page, b, size);
    matrix_off = (uint16_t)(n * 8 + 1);
    gfx_room_changed();   /* initBGBuffers + GdiV2::roomChanged */
    resetRoomObjects();
    VAR(VAR_CAMERA_MIN_X) = 160;
    VAR(VAR_CAMERA_MAX_X) = (int16_t)roomWidth - 160;
    camera.mode = kNormalCameraMode;
    if (roomResource == 0) return;
    showActors();
    egoPositioned = 0;
    runEntryScript();
    runScript(5, 0, 0);
}

/* --- боксы (boxes.cpp) --- */

uint8_t getNumBoxes(void)
{
    return boxes[0];
}

/* getBoxBaseAddr: смещение бокса в boxes[] или 0 */
static uint16_t boxBase(uint8_t box)
{
    if (box == 255) return 0;
    if (boxes[0] == box) box--;
    if (box >= boxes[0]) fatal("BOX", box);
    return (uint16_t)(1 + box * 8);
}

uint8_t getBoxFlags(uint8_t box)
{
    uint16_t p = boxBase(box);
    return p ? boxes[p + 7] : 0;
}

void setBoxFlags(uint8_t box, uint8_t val)
{
    uint16_t p = boxBase(box);
    if (p) boxes[p + 7] = val;
}

uint8_t getMaskFromBox(uint8_t box)
{
    if (box == 255) return 1;   /* WORKAROUND ScummVM #740244 */
    uint16_t p = boxBase(box);
    return p ? boxes[p + 6] : 0;
}

/* Box.v2 = { uy, ly, ulx, urx, llx, lrx, mask, flags } */
void getBoxCoordinates(uint8_t boxnum, struct BoxCoords *box)
{
    uint16_t p = boxBase(boxnum);
    if (!p) fatal("BOX COORDS", boxnum);
    const uint8_t *bp = boxes + p;
    box->ul_x = bp[2];
    box->ul_y = bp[0];
    box->ur_x = bp[3];
    box->ur_y = bp[0];
    box->ll_x = bp[4];
    box->ll_y = bp[1];
    box->lr_x = bp[5];
    box->lr_y = bp[1];
}

int8_t getNextBox(uint8_t from, uint8_t to)
{
    uint8_t num = boxes[0];
    if (from == to) return (int8_t)to;
    if (to == kInvalidBox) return -1;
    if (from == kInvalidBox) return (int8_t)to;
    const uint8_t *boxm = boxes + matrix_off;   /* getBoxMatrixBaseAddr */
    if (*boxm == 0xFF) boxm++;
    boxm += num + boxm[from];
    return (int8_t)boxm[to];
}

/* boxes.cpp: closestPtOnLine */
void closestPtOnLine(int16_t sx, int16_t sy, int16_t ex, int16_t ey, int16_t px, int16_t py, int16_t *rx, int16_t *ry)
{
    int16_t lxdiff = ex - sx, lydiff = ey - sy;
    int16_t x, y;
    if (ex == sx)
    {
        x = sx;
        y = py;
    }
    else if (ey == sy)
    {
        x = px;
        y = sy;
    }
    else
    {
        int32_t dist = (int32_t)lxdiff * lxdiff + (int32_t)lydiff * lydiff;
        int32_t a, b, c;
        int16_t alx = lxdiff < 0 ? -lxdiff : lxdiff, aly = lydiff < 0 ? -lydiff : lydiff;
        if (alx > aly)
        {
            a = (int32_t)sx * lydiff / lxdiff;
            b = (int32_t)px * lxdiff / lydiff;
            c = (a + b - sy + py) * lydiff * lxdiff / dist;
            x = (int16_t)c;
            y = (int16_t)(c * lydiff / lxdiff - a + sy);
        }
        else
        {
            a = (int32_t)sy * lxdiff / lydiff;
            b = (int32_t)py * lydiff / lxdiff;
            c = (a + b - sx + px) * lydiff * lxdiff / dist;
            x = (int16_t)(c * lxdiff / lydiff - a + sx);
            y = (int16_t)c;
        }
    }
    int16_t alx = lxdiff < 0 ? -lxdiff : lxdiff, aly = lydiff < 0 ? -lydiff : lydiff;
    if (aly < alx)
    {
        if (lxdiff > 0)
        {
            if (x < sx) { x = sx; y = sy; }
            else if (x > ex) { x = ex; y = ey; }
        }
        else
        {
            if (x > sx) { x = sx; y = sy; }
            else if (x < ex) { x = ex; y = ey; }
        }
    }
    else
    {
        if (lydiff > 0)
        {
            if (y < sy) { x = sx; y = sy; }
            else if (y > ey) { x = ex; y = ey; }
        }
        else
        {
            if (y > sy) { x = sx; y = sy; }
            else if (y < ey) { x = ex; y = ey; }
        }
    }
    *rx = x;
    *ry = y;
}

static int32_t sqrDist(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    int32_t dx = x1 - x2, dy = y1 - y2;
    return dx * dx + dy * dy;
}

/* boxes.cpp: compareSlope */
static uint8_t compareSlope(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3)
{
    int32_t l = (int32_t)(y2 - y1) * (x3 - x1), r = (int32_t)(y3 - y1) * (x2 - x1);
    if (l <= r) return 1;
    return 0;
}

/* boxes.cpp: checkXYInBoxBounds */
uint8_t checkXYInBoxBounds(uint8_t boxnum, int16_t x, int16_t y)
{
    if (boxnum == kInvalidBox) return 0;
    struct BoxCoords b;
    getBoxCoordinates(boxnum, &b);
    if (x < b.ul_x && x < b.ur_x && x < b.lr_x && x < b.ll_x) return 0;
    if (x > b.ul_x && x > b.ur_x && x > b.lr_x && x > b.ll_x) return 0;
    if (y < b.ul_y && y < b.ur_y && y < b.lr_y && y < b.ll_y) return 0;
    if (y > b.ul_y && y > b.ur_y && y > b.lr_y && y > b.ll_y) return 0;
    if ((b.ul_x == b.ur_x && b.ul_y == b.ur_y && b.lr_x == b.ll_x && b.lr_y == b.ll_y) ||
        (b.ul_x == b.ll_x && b.ul_y == b.ll_y && b.ur_x == b.lr_x && b.ur_y == b.lr_y))
    {
        int16_t tx, ty;
        closestPtOnLine(b.ul_x, b.ul_y, b.lr_x, b.lr_y, x, y, &tx, &ty);
        if (sqrDist(x, y, tx, ty) <= 4) return 1;
    }
    if (!compareSlope(b.ul_x, b.ul_y, b.ur_x, b.ur_y, x, y)) return 0;
    if (!compareSlope(b.ur_x, b.ur_y, b.lr_x, b.lr_y, x, y)) return 0;
    if (!compareSlope(b.lr_x, b.lr_y, b.ll_x, b.ll_y, x, y)) return 0;
    if (!compareSlope(b.ll_x, b.ll_y, b.ul_x, b.ul_y, x, y)) return 0;
    return 1;
}

/* boxes.cpp: getClosestPtOnBox */
int32_t getClosestPtOnBox(const struct BoxCoords *b, int16_t x, int16_t y, int16_t *outX, int16_t *outY)
{
    int32_t best = 0xFFFFFFL, d;
    int16_t tx, ty;
    closestPtOnLine(b->ul_x, b->ul_y, b->ur_x, b->ur_y, x, y, &tx, &ty);
    d = sqrDist(x, y, tx, ty);
    if (d < best) { best = d; *outX = tx; *outY = ty; }
    closestPtOnLine(b->ur_x, b->ur_y, b->lr_x, b->lr_y, x, y, &tx, &ty);
    d = sqrDist(x, y, tx, ty);
    if (d < best) { best = d; *outX = tx; *outY = ty; }
    closestPtOnLine(b->lr_x, b->lr_y, b->ll_x, b->ll_y, x, y, &tx, &ty);
    d = sqrDist(x, y, tx, ty);
    if (d < best) { best = d; *outX = tx; *outY = ty; }
    closestPtOnLine(b->ll_x, b->ll_y, b->ul_x, b->ul_y, x, y, &tx, &ty);
    d = sqrDist(x, y, tx, ty);
    if (d < best) { best = d; *outX = tx; *outY = ty; }
    return best;
}
