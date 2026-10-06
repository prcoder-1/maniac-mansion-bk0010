/* Актёры (банк B): перевод engines/scumm/actor.cpp (Actor, Actor_v2) для v2.
 * Координаты — в единицах v2: x — 8 точек, y — 2 точки (getPos умножает). */
#include "scumm.h"
#include "actor.h"

struct Actor actors[NUM_ACTORS];

static struct Actor *A(uint8_t act)
{
    if (act >= NUM_ACTORS) fatal("ACTOR", act);
    return &actors[act];
}

/* util.cpp */
static int16_t toSimpleDir(int16_t dir)
{
    static const int16_t directions[] = { 71, 109, 251, 289 };
    for (uint8_t i = 0; i < 3; i++)
        if (dir >= directions[i] && dir <= directions[i + 1]) return (int16_t)(i + 1);
    return 0;
}

static int16_t normalizeAngle(int16_t angle)
{
    static const int16_t directions[] = { 22, 72, 107, 157, 202, 252, 287, 337 };
    int16_t temp = (int16_t)((angle + 360) % 360);
    for (uint8_t i = 0; i < 7; i++)
        if (temp >= directions[i] && temp <= directions[i + 1]) return (int16_t)((i + 1) * 45);
    return 0;
}

/* actor.cpp: Actor::initActor + Actor_v2::initActor */
static void initActor(struct Actor *a, int8_t mode)
{
    if (mode == -1)
    {
        a->top = a->bottom = 0;
        a->needRedraw = 0;
        a->needBgReset = 0;
        a->visible = 0;
        a->frame = 0;
        a->walkbox = 0;
        a->animProgress = 0;
        for (uint8_t i = 0; i < 16; i++) a->palette[i] = 0;
        a->sound = 0;
        costume_reset(a);
        a->cost_animCounter = 0;
        a->cost_soundCounter = 0;
        a->cost_soundPos = 0;
        a->dest_x = a->dest_y = 0;
        a->destbox = a->curbox = 0;
        a->destdir = 0;
    }
    if (mode == 1 || mode == -1)
    {
        a->costume = 0;
        a->room = 0;
        a->x = 0;
        a->y = 0;
        a->facing = 180;
    }
    else if (mode == 2)
        a->facing = 180;
    a->elevation = 0;
    a->talkColor = 15;
    a->sound = 0;
    a->targetFacing = a->facing;
    a->moving = 0;   /* stopActorMoving */
    a->animSpeed = 0;
    a->name_page = 0;
}

void actors_init(void)
{
    for (uint8_t i = 0; i < NUM_ACTORS; ++i)
    {
        actors[i].number = i;
        initActor(&actors[i], -1);
    }
}

int16_t actor_get(uint8_t act, uint8_t field)
{
    struct Actor *a = A(act);
    switch (field)
    {
    case AF_ROOM: return a->room;
    case AF_X: return a->x;
    case AF_Y: return a->y;
    case AF_COSTUME: return a->costume;
    case AF_MOVING: return a->moving;
    case AF_FACING: return a->facing;
    case AF_ELEVATION: return a->elevation;
    case AF_WALKBOX: return a->walkbox;
    case AF_TALKCOLOR: return a->talkColor;
    case AF_VISIBLE: return a->visible;
    case AF_FRAME: return a->frame;
    }
    return 0;
}

void actor_set_room(uint8_t act, uint8_t room)
{
    A(act)->room = room;
}

uint8_t actor_name(uint8_t act, uint16_t *addr)
{
    struct Actor *a = A(act);
    *addr = a->name_addr;
    return a->name_page;
}

void actor_set_name(uint8_t act, uint8_t page, uint16_t addr)
{
    struct Actor *a = A(act);
    if (a->name_page) res_free(a->name_page, a->name_addr);
    a->name_page = page;
    a->name_addr = addr;
}

void setActorPalette(uint8_t act, uint8_t i, uint8_t c)
{
    A(act)->palette[i & 15] = c;
    A(act)->needRedraw = 1;
}

void setActorTalkColor(uint8_t act, uint8_t c)
{
    A(act)->talkColor = c;
}

void setActorSound(uint8_t act, uint8_t s)
{
    A(act)->sound = s;
}

void setActorElevation(uint8_t act, int16_t e)
{
    struct Actor *a = A(act);
    if (a->elevation != e)
    {
        a->elevation = e;
        a->needRedraw = 1;
    }
}

static uint8_t isInCurrentRoom(const struct Actor *a)
{
    return a->room == currentRoom;
}

/* actor.cpp: Actor_v2::isPlayer */
static uint8_t isPlayer(const struct Actor *a)
{
    return VAR(42) <= a->number && a->number <= VAR(43);
}

/* --- направления --- */

/* actor.cpp: Actor::setDirection */
void setActorDirection(uint8_t act, int16_t direction)
{
    struct Actor *a = A(act);
    if (a->facing == direction) return;
    a->facing = normalizeAngle(direction);
    if (a->costume == 0) return;
    for (uint8_t i = 0; i < 16; i++)
    {
        uint16_t vald = a->cost_frame[i];
        if (vald == 0xFFFF) continue;
        costumeDecodeData(a, vald);
    }
    a->needRedraw = 1;
}

/* actor.cpp: Actor::turnToDirection (v <= 6) */
void turnToDirection(uint8_t act, int16_t newdir)
{
    if (newdir == -1) return;
    struct Actor *a = A(act);
    a->targetFacing = newdir;
    a->moving = MF_TURN;
}

/* actor.cpp: Actor::remapDirection (v2: особых флагов направления нет) */
static int16_t remapDirection(struct Actor *a, int16_t dir, uint8_t is_walking)
{
    uint8_t flags = getBoxFlags(a->walkbox);
    uint8_t flipX = a->deltaXFactor > 0, flipY = a->deltaYFactor > 0;
    if (flags & kBoxXFlip)
    {
        dir = 360 - dir;
        flipX = !flipX;
    }
    if (flags & kBoxYFlip)
    {
        dir = 180 - dir;
        flipY = !flipY;
    }
    switch (flags & 7)
    {
    case 1:
        if (is_walking) return flipX ? 90 : 270;
        return dir == 90 ? 90 : 270;
    case 2:
        if (is_walking) return flipY ? 180 : 0;
        return dir == 0 ? 0 : 180;
    case 3: return 270;
    case 4: return 90;
    case 5: return 0;
    case 6: return 180;
    }
    return normalizeAngle(dir) | 1024;
}

/* actor.cpp: Actor::updateActorDirection */
static int16_t updateActorDirection(struct Actor *a, uint8_t is_walking)
{
    int16_t from = toSimpleDir(a->facing);
    int16_t dir = remapDirection(a, a->targetFacing, is_walking);
    uint8_t shouldInterpolate = (dir & 1024) != 0;
    dir &= 1023;
    if (shouldInterpolate)
    {
        int16_t to = toSimpleDir(dir);
        int16_t diff = to - from;
        if ((diff < 0 ? -diff : diff) > 2) diff = -diff;
        if (diff > 0) to = from + 1;
        else if (diff < 0) to = from - 1;
        dir = (int16_t)(((to + 4) % 4) * 90);
    }
    return dir;
}

void faceToObject(uint8_t act, uint16_t obj)
{
    struct Actor *a = A(act);
    int16_t x2, y2;
    if (!isInCurrentRoom(a)) return;
    if (getObjectOrActorXY(obj, &x2, &y2) == -1) return;
    turnToDirection(act, x2 > a->x ? 90 : 270);
}

/* --- анимация --- */

/* actor.cpp: Actor::startAnimActor (v <= 6) */
void startAnimActor(uint8_t act, uint8_t f)
{
    struct Actor *a = A(act);
    switch (f)
    {
    case 0x38: f = ACTOR_INIT_FRAME; break;
    case 0x39: f = ACTOR_WALK_FRAME; break;
    case 0x3A: f = ACTOR_STAND_FRAME; break;
    case 0x3B: f = ACTOR_TALKSTART_FRAME; break;
    case 0x3C: f = ACTOR_TALKSTOP_FRAME; break;
    }
    if (isInCurrentRoom(a) && a->costume != 0)
    {
        a->animProgress = 0;
        a->needRedraw = 1;
        a->cost_animCounter = 0;
        costumeDecodeData(a, f);
        a->frame = f;
    }
}

/* actor.cpp: Actor::animateActor (v2) */
void animateActor(uint8_t act, uint8_t anim)
{
    static const int16_t new_dir_table[4] = { 270, 90, 180, 0 };
    uint8_t cmd = (uint8_t)(0x3F - anim / 4 + 2);
    int16_t dir = new_dir_table[anim % 4];
    struct Actor *a = A(act);
    switch (cmd)
    {
    case 2:
        startAnimActor(act, ACTOR_STAND_FRAME);
        stopActorMoving(act);
        break;
    case 3:
        a->moving &= (uint8_t)~MF_TURN;
        setActorDirection(act, dir);
        break;
    case 4:
        turnToDirection(act, dir);
        break;
    default:
        startAnimActor(act, anim / 4);
    }
}

/* actor.cpp: Actor::startWalkAnim (сценария ходьбы в v2 нет) */
static void startWalkAnim(struct Actor *a, uint8_t cmd, int16_t angle)
{
    if (angle == -1) angle = a->facing;
    switch (cmd)
    {
    case 1:
        setActorDirection(a->number, angle);
        startAnimActor(a->number, ACTOR_WALK_FRAME);
        break;
    case 2:
        setActorDirection(a->number, angle);
        break;
    case 3:
        turnToDirection(a->number, angle);
        startAnimActor(a->number, ACTOR_STAND_FRAME);
        break;
    }
}

void runActorTalkScript(uint8_t act, uint8_t f)
{
    struct Actor *a = A(act);
    if (!VAR(VAR_TALK_ACTOR) || a->room != currentRoom || a->frame == f) return;
    startAnimActor(act, f);
}

/* actor.cpp: Actor::animateCostume */
void animateCostume(struct Actor *a)
{
    if (!a->costume) return;
    a->animProgress++;
    if (a->animProgress >= a->animSpeed)
    {
        a->animProgress = 0;
        if (increaseAnims(a)) a->needRedraw = 1;
    }
}

/* кадр без перерисовки: только Actor::animateCostume актёров комнаты */
void animate_only(void)
{
    for (uint8_t i = 1; i < NUM_ACTORS; i++)
        if (isInCurrentRoom(&actors[i])) animateCostume(&actors[i]);
}

/* actor.cpp: ScummEngine::playActorSounds */
void playActorSounds(void)
{
    for (uint8_t i = 1; i < NUM_ACTORS; i++)
        if (actors[i].cost_soundCounter && isInCurrentRoom(&actors[i]))
        {
            currentScript = 0xFF;
            addSoundToQueue(actors[i].sound);
            for (uint8_t j = 1; j < NUM_ACTORS; j++) actors[j].cost_soundCounter = 0;
            return;
        }
}

/* --- положение и боксы --- */

void stopActorMoving(uint8_t act)
{
    A(act)->moving = 0;
}

static void setBox(struct Actor *a, uint8_t box)
{
    a->walkbox = box;   /* setupActorScale (v3): масштаба нет */
}

/* actor.cpp: static checkXYInBoxBounds (v2) — ближайшая точка бокса и «расстояние» */
static int16_t boxDistance(uint8_t boxnum, int16_t x, int16_t y, int16_t *destX, int16_t *destY)
{
    struct BoxCoords box;
    getBoxCoordinates(boxnum, &box);
    int16_t xmin, xmax;
    if (y < box.ul_y)
    {
        *destY = box.ul_y;
        xmin = box.ul_x;
        xmax = box.ur_x;
    }
    else if (y >= box.ll_y)
    {
        *destY = box.ll_y;
        xmin = box.ll_x;
        xmax = box.lr_x;
    }
    else if (x >= box.ul_x && x >= box.ll_x && x < box.ur_x && x < box.lr_x)
    {
        *destX = x;
        *destY = y;
        xmin = xmax = x;
    }
    else
    {
        *destY = y;
        int16_t ul = box.ul_x, ll = box.ll_x, ur = box.ur_x, lr = box.lr_x;
        int16_t top = box.ul_y, bottom = box.ll_y, cury;
        do
        {
            xmin = (ul + ll) / 2;
            xmax = (ur + lr) / 2;
            cury = (top + bottom) / 2;
            if (cury < y)
            {
                top = cury;
                ul = xmin;
                ur = xmax;
            }
            else if (cury > y)
            {
                bottom = cury;
                ll = xmin;
                lr = xmax;
            }
        } while (cury != y);
    }
    if (x < xmin) *destX = xmin;
    else if (x > xmax) *destX = xmax;
    else *destX = x;
    int16_t xDist = x - *destX, yDist = y - *destY;
    if (xDist < 0) xDist = -xDist;
    if (yDist < 0) yDist = -yDist;
    yDist /= 4;
    if (xDist < yDist) return (int16_t)((xDist >> 1) + yDist);
    return (int16_t)((yDist >> 1) + xDist);
}

/* actor.cpp: Actor_v2::adjustXYToBeInBox */
void adjustXYToBeInBox(uint8_t act, int16_t dstX, int16_t dstY, struct AdjustBoxResult *abr)
{
    struct Actor *a = A(act);
    abr->x = dstX;
    abr->y = dstY;
    abr->box = kInvalidBox;
    int16_t numBoxes = (int16_t)getNumBoxes() - 1;
    int16_t bestDist = 0xFF;
    for (int16_t i = 0; i <= numBoxes; i++)
    {
        uint8_t box = (uint8_t)(numBoxes - i);
        int16_t foundX, foundY;
        uint8_t flags = getBoxFlags(box);
        if ((flags & kBoxInvisible) && !((flags & kBoxPlayerOnly) && !isPlayer(a))) continue;
        int16_t dist = boxDistance(box, dstX, dstY, &foundX, &foundY);
        if (dist == 0)
        {
            abr->x = foundX;
            abr->y = foundY;
            abr->box = box;
            break;
        }
        if (dist < bestDist)
        {
            bestDist = dist;
            abr->x = foundX;
            abr->y = foundY;
            abr->box = box;
        }
    }
}

/* actor.cpp: Actor::adjustActorPos */
static void adjustActorPos(struct Actor *a)
{
    struct AdjustBoxResult abr;
    adjustXYToBeInBox(a->number, a->x, a->y, &abr);
    a->x = abr.x;
    a->y = abr.y;
    a->destbox = abr.box;
    setBox(a, abr.box);
    a->dest_x = -1;
    a->moving = 0;
    a->cost_soundCounter = 0;
    a->cost_soundPos = 0;
    if (a->walkbox != kInvalidBox)
        if (getBoxFlags(a->walkbox) & 7) turnToDirection(a->number, a->facing);
}

void hideActor(uint8_t act)
{
    struct Actor *a = A(act);
    if (!a->visible) return;
    if (a->moving)
    {
        a->moving = 0;
        startAnimActor(act, ACTOR_STAND_FRAME);
    }
    a->visible = 0;
    a->cost_soundCounter = 0;
    a->cost_soundPos = 0;
    a->needRedraw = 0;
    a->needBgReset = 1;
}

/* actor.cpp: Actor::showActor (v2) */
static void showActor(struct Actor *a)
{
    if (currentRoom == 0 || a->visible) return;
    adjustActorPos(a);
    res_get(rtCostume, a->costume);   /* ensureResourceLoaded */
    costume_reset(a);
    startAnimActor(a->number, ACTOR_STAND_FRAME);
    startAnimActor(a->number, ACTOR_INIT_FRAME);
    startAnimActor(a->number, ACTOR_TALKSTOP_FRAME);
    a->moving = 0;
    a->visible = 1;
    a->needRedraw = 1;
}

void showActors(void)
{
    for (uint8_t i = 1; i < NUM_ACTORS; i++)
        if (isInCurrentRoom(&actors[i])) showActor(&actors[i]);
}

/* actor.cpp: Actor::putActor */
void putActor(uint8_t act, int16_t dstX, int16_t dstY, uint8_t newRoom)
{
    struct Actor *a = A(act);
    if (a->visible && currentRoom != newRoom && (uint8_t)VAR(VAR_TALK_ACTOR) == act) stopTalk();
    a->x = dstX;
    a->y = dstY;
    a->room = newRoom;
    a->needRedraw = 1;
    if (VAR(VAR_EGO) == act) egoPositioned = 1;
    if (a->visible)
    {
        if (isInCurrentRoom(a))
        {
            if (a->moving)
            {
                a->moving = 0;
                startAnimActor(act, ACTOR_STAND_FRAME);
            }
            adjustActorPos(a);
        }
        else
            hideActor(act);
    }
    else if (isInCurrentRoom(a))
        showActor(a);
}

/* actor.cpp: Actor::setActorCostume (GF_OLD_BUNDLE) */
void setActorCostume(uint8_t act, uint8_t c)
{
    struct Actor *a = A(act);
    if (a->visible)
    {
        hideActor(act);
        costume_reset(a);
        a->costume = c;
        showActor(a);
    }
    else
    {
        a->costume = c;
        costume_reset(a);
    }
    for (uint8_t i = 0; i < 16; i++) a->palette[i] = i;
}

/* --- ходьба --- */

/* actor.cpp: getAngleFromPos (без atan) */
static int16_t getAngleFromPos(int32_t x, int32_t y)
{
    int32_t ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
    if (ay * 2 < ax) return x > 0 ? 90 : 270;
    return y > 0 ? 180 : 0;
}

/* actor.cpp: Actor::actorWalkStep (v2: шаг на единицу по каждой оси) */
static uint8_t actorWalkStep(struct Actor *a)
{
    a->needRedraw = 1;
    int16_t nextFacing = updateActorDirection(a, 1);
    if (!(a->moving & MF_IN_LEG) || a->facing != nextFacing)
    {
        if (ACTOR_WALK_FRAME != a->frame || a->facing != nextFacing) startWalkAnim(a, 1, nextFacing);
        a->moving |= MF_IN_LEG;
    }
    if (a->walkbox != a->curbox && checkXYInBoxBounds(a->curbox, a->x, a->y)) setBox(a, a->curbox);
    int16_t distX = a->next_x - a->cur_x, distY = a->next_y - a->cur_y;
    if (distX < 0) distX = -distX;
    if (distY < 0) distY = -distY;
    int16_t dx = a->x - a->cur_x, dy = a->y - a->cur_y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    if (dx >= distX && dy >= distY)
    {
        a->moving &= (uint8_t)~MF_IN_LEG;
        return 0;
    }
    if (a->deltaXFactor != 0) a->x += a->deltaXFactor > 0 ? 1 : -1;
    if (a->deltaYFactor != 0) a->y += a->deltaYFactor > 0 ? 1 : -1;
    dx = a->x - a->cur_x;
    dy = a->y - a->cur_y;
    if ((dx < 0 ? -dx : dx) > distX) a->x = a->next_x;
    if ((dy < 0 ? -dy : dy) > distY) a->y = a->next_y;
    if (a->x == a->next_x && a->y == a->next_y)
    {
        a->moving &= (uint8_t)~MF_IN_LEG;
        return 0;
    }
    return 1;
}

/* actor.cpp: Actor::calcMovementFactor (скорость v2: 1 x 1) */
static uint8_t calcMovementFactor(struct Actor *a, int16_t nx, int16_t ny)
{
    if (a->x == nx && a->y == ny) return 0;
    int16_t diffX = nx - a->x, diffY = ny - a->y;
    int32_t deltaYFactor = 65536L;   /* _speedy << 16 */
    if (diffY < 0) deltaYFactor = -deltaYFactor;
    int32_t deltaXFactor = deltaYFactor * diffX;
    if (diffY != 0) deltaXFactor /= diffY;
    else deltaYFactor = 0;
    if ((deltaXFactor < 0 ? -deltaXFactor : deltaXFactor) > 65536L)
    {
        deltaXFactor = 65536L;
        if (diffX < 0) deltaXFactor = -deltaXFactor;
        deltaYFactor = deltaXFactor * diffY;
        if (diffX != 0) deltaYFactor /= diffX;
        else deltaXFactor = 0;
    }
    a->cur_x = a->x;
    a->cur_y = a->y;
    a->next_x = nx;
    a->next_y = ny;
    a->deltaXFactor = deltaXFactor;
    a->deltaYFactor = deltaYFactor;
    a->targetFacing = getAngleFromPos(V12_X_MULTIPLIER * deltaXFactor, V12_Y_MULTIPLIER * deltaYFactor);
    return actorWalkStep(a);
}

/* actor.cpp: Actor::startWalkActor (v2) */
void startWalkActor(uint8_t act, int16_t destX, int16_t destY, int16_t dir)
{
    struct Actor *a = A(act);
    struct AdjustBoxResult abr;
    abr.x = destX;
    abr.y = destY;
    if (!isInCurrentRoom(a))
    {
        a->x = abr.x;
        a->y = abr.y;
        if (dir != -1) a->facing = dir;
        return;
    }
    adjustXYToBeInBox(act, abr.x, abr.y, &abr);
    if (a->x == abr.x && a->y == abr.y && (dir == -1 || a->facing == dir)) return;
    a->dest_x = abr.x;
    a->dest_y = abr.y;
    a->destbox = abr.box;
    a->destdir = dir;
    a->curbox = a->walkbox;
    a->moving = (uint8_t)((a->moving & ~(MF_LAST_LEG | MF_IN_LEG)) | MF_NEW_LEG);
}

/* actor.cpp: Actor_v2::walkActor */
static void walkActor(struct Actor *a)
{
    if (a->moving & MF_TURN)
    {
        int16_t new_dir = updateActorDirection(a, 0);
        if (a->facing != new_dir) setActorDirection(a->number, new_dir);
        else a->moving = 0;
        return;
    }
    if (!a->moving) return;
    if (a->moving & MF_IN_LEG)
    {
        actorWalkStep(a);
        return;
    }
    if (a->moving & MF_LAST_LEG)
    {
        a->moving = 0;
        startAnimActor(a->number, ACTOR_STAND_FRAME);
        if (a->targetFacing != a->destdir) turnToDirection(a->number, a->destdir);
        return;
    }
    int16_t fx, fy;
    setBox(a, a->curbox);
    if (a->walkbox == a->destbox)
    {
        fx = a->dest_x;
        fy = a->dest_y;
        a->moving |= MF_LAST_LEG;
    }
    else
    {
        int8_t next_box = getNextBox(a->walkbox, a->destbox);
        if (next_box < 0)
        {
            a->moving |= MF_LAST_LEG;
            return;
        }
        uint8_t flags = getBoxFlags((uint8_t)next_box);
        if ((flags & kBoxLocked) && !((flags & kBoxPlayerOnly) && !isPlayer(a))) a->moving |= MF_LAST_LEG;
        a->curbox = (uint8_t)next_box;
        struct BoxCoords bc;
        int16_t tx = 0, ty = 0;
        extern int32_t getClosestPtOnBox(const struct BoxCoords *b, int16_t x, int16_t y, int16_t *outX, int16_t *outY);
        getBoxCoordinates(a->curbox, &bc);
        getClosestPtOnBox(&bc, a->x, a->y, &tx, &ty);
        getBoxCoordinates(a->walkbox, &bc);
        getClosestPtOnBox(&bc, tx, ty, &fx, &fy);
    }
    calcMovementFactor(a, fx, fy);
}

void walkActors(void)
{
    for (uint8_t i = 1; i < NUM_ACTORS; ++i)
        if (isInCurrentRoom(&actors[i])) walkActor(&actors[i]);
}

/* script_v5.cpp: walkActorToActor (v2) */
void walkActorToActor(uint8_t actor, uint8_t toActor, uint8_t dist)
{
    struct Actor *a = A(actor), *to = A(toActor);
    int16_t d = (int16_t)(dist * V12_X_MULTIPLIER);
    int16_t x = (int16_t)(to->x * V12_X_MULTIPLIER), y = (int16_t)(to->y * V12_Y_MULTIPLIER);
    if (x < a->x * V12_X_MULTIPLIER) x += d;
    else x -= d;
    x /= V12_X_MULTIPLIER;
    y /= V12_Y_MULTIPLIER;
    struct AdjustBoxResult abr;
    adjustXYToBeInBox(actor, x, y, &abr);
    startWalkActor(actor, abr.x, abr.y, -1);
}

/* actor.cpp: ScummEngine::getActorFromPos — по отрисованным полосам и top/bottom */
uint8_t getActorFromPos(int16_t x, int16_t y)
{
    for (uint8_t i = 1; i < NUM_ACTORS; i++)
        if (actor_in_strip(x / 8, i) && y >= actors[i].top && y <= actors[i].bottom)
            if (i != VAR(VAR_EGO)) return i;
    return 0;
}
