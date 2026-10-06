/* Глаголы, инвентарь и строка предложения (банк A): перевод engines/scumm/verbs.cpp
 * (v2: initV2MouseOver, checkV2MouseOver, checkV2Inventory, redrawV2Inventory,
 * checkExecVerbs, handleMouseOver) и o2_drawSentence из script_v2.cpp.
 * Координаты — точки экрана MM 320x200; область глаголов начинается со строки 144. */
#include "vm.h"

#define VERB_TOP 144
#define MAX_VERBS 32          /* ScummVM: 100; MM занимает слоты до ~22 */

/* VerbSlot: 32 байта */
struct VerbSlot
{
    int16_t left, top, right, bottom;
    int16_t old_left, old_top, old_right, old_bottom;
    uint8_t verbid, color, hicolor, dimcolor, curmode, key, prep, saveid;
    uint8_t name_page, pad;
    uint16_t name_addr;
    uint8_t pad2[4];
};

static struct VerbSlot verbs[MAX_VERBS];
static uint8_t verbMouseOverSlot;
static int8_t mouseOverBoxV2 = -1;
static uint8_t inventoryOffset;


/* verbs.cpp: initV2MouseOver — прямоугольники относительно области глаголов */
struct Box8 { int16_t l, t, r, b; };
static const struct Box8 mouseOverBoxes[7] =
{
    { 0, 32, 144, 40 }, { 176, 32, 320, 40 }, { 0, 40, 144, 48 }, { 176, 40, 320, 48 },
    { 144, 32, 176, 40 },   /* kInventoryUpArrow */
    { 144, 40, 176, 48 },   /* kInventoryDownArrow */
    { 0, 0, 320, 8 },       /* kSentenceLine */
};
static const uint8_t mo_color[7] = { 13, 13, 13, 13, 1, 1, 13 };
#define HI_COLOR 14
enum { kInventoryUpArrow = 4, kInventoryDownArrow = 5, kSentenceLine = 6 };

void verbs_reset(void)
{
    for (uint8_t i = 0; i < MAX_VERBS; i++)
    {
        verbs[i].verbid = 0;
        verbs[i].right = 319;
        verbs[i].old_left = -1;
        verbs[i].color = 2;
        verbs[i].hicolor = 0;
        verbs[i].curmode = 0;
        verbs[i].saveid = 0;
        verbs[i].key = 0;
        verbs[i].name_page = 0;
    }
    mouseOverBoxV2 = -1;
}

static struct VerbSlot *V(uint8_t slot)
{
    if (slot >= MAX_VERBS) fatal("VERB SLOT", slot);
    return &verbs[slot];
}

uint8_t verb_name(uint8_t slot, uint16_t *addr)
{
    *addr = V(slot)->name_addr;
    return V(slot)->name_page;
}

/* script_v2.cpp: o2_verbOps — новый глагол */
void verbOps_new(uint8_t slot, uint8_t verbid, uint8_t x, uint8_t y, uint8_t prep)
{
    static const char keyboard[] = "qwertasdfgzxcvb";
    struct VerbSlot *vs = V(slot);
    if (slot == 0) fatal("VERB 0", 0);
    vs->verbid = verbid;
    vs->color = 2;
    vs->hicolor = 14;
    vs->dimcolor = 8;
    vs->curmode = 1;
    vs->saveid = 0;
    vs->key = 0;
    vs->prep = prep;
    vs->left = (int16_t)(x * 8);
    vs->top = (int16_t)(y * 8);
    if (slot >= 1 && slot <= 15) vs->key = (uint8_t)keyboard[slot - 1];
    if (vs->name_page) res_free(vs->name_page, vs->name_addr);
    vs->name_page = loadPtrToResource();
    vs->name_addr = res_addr;
}

void verb_set_mode(uint8_t slot, uint8_t mode)
{
    V(slot)->curmode = mode;
}

/* verbs.cpp: getVerbSlot */
uint8_t getVerbSlot(uint8_t id, uint8_t mode)
{
    for (uint8_t i = 1; i < MAX_VERBS; i++)
        if (verbs[i].verbid == id && verbs[i].saveid == mode) return i;
    return 0;
}

/* verbs.cpp: restoreVerbBG */
static void restoreVerbBG(struct VerbSlot *vs)
{
    if (vs->old_left != -1)
    {
        screen_clear_rect(vs->old_left, vs->old_top + VERB_TOP, vs->old_right, vs->old_bottom + VERB_TOP);
        vs->old_left = -1;
    }
}

static void drawFarString(int16_t x, int16_t y, uint8_t color, uint8_t page, uint16_t addr, uint8_t max, int16_t *right)
{
    uint8_t buf[41];
    uint8_t n = 0;
    while (n < max && (buf[n] = far_rb(page, addr + n)) != 0) n++;
    buf[n] = 0;
    *right = charset_drawString(x, y, color, buf);
}

/* verbs.cpp: drawVerb */
void drawVerb(uint8_t verb, uint8_t mode)
{
    if (!verb) return;
    struct VerbSlot *vs = V(verb);
    if (!vs->saveid && vs->curmode && vs->verbid)
    {
        restoreVerbBG(vs);
        uint8_t color;
        if (vs->curmode == 2) color = vs->dimcolor;
        else if (mode && vs->hicolor) color = vs->hicolor;
        else color = vs->color;
        if (!vs->name_page) return;
        int16_t right;
        drawFarString(vs->left, vs->top + VERB_TOP, color, vs->name_page, vs->name_addr, 40, &right);
        vs->right = right;
        vs->bottom = vs->top + 8;
        vs->old_left = vs->left;
        vs->old_top = vs->top;
        vs->old_right = right;
        vs->old_bottom = vs->bottom;
    }
    else
        restoreVerbBG(vs);
}

/* verbs.cpp: killVerb */
void killVerb(uint8_t slot)
{
    if (slot == 0) return;
    struct VerbSlot *vs = V(slot);
    vs->verbid = 0;
    vs->curmode = 0;
    if (vs->name_page)
    {
        res_free(vs->name_page, vs->name_addr);
        vs->name_page = 0;
    }
    if (vs->saveid == 0)
    {
        drawVerb(slot, 0);
        verbMouseOver(0);
    }
    vs->saveid = 0;
}

/* verbs.cpp: findVerbAtPos */
static uint8_t findVerbAtPos(int16_t x, int16_t y)
{
    y -= VERB_TOP;
    for (uint8_t i = MAX_VERBS - 1; i > 0; i--)
    {
        struct VerbSlot *vs = &verbs[i];
        if (vs->curmode != 1 || !vs->verbid || vs->saveid || y < vs->top || y >= vs->bottom) continue;
        if (x < vs->left || x >= vs->right) continue;
        return i;
    }
    return 0;
}

/* verbs.cpp: verbMouseOver */
void verbMouseOver(uint8_t verb)
{
    if (!(userState & USERSTATE_IFACE_VERBS)) return;
    if (verbMouseOverSlot != verb)
    {
        drawVerb(verbMouseOverSlot, 0);
        verbMouseOverSlot = verb;
        if (verbs[verb].hicolor)
        {
            drawVerb(verb, 1);
            verbMouseOverSlot = verb;
        }
    }
}

/* verbs.cpp: redrawVerbs */
void redrawVerbs(void)
{
    if (!(userState & USERSTATE_IFACE_VERBS)) return;
    uint8_t verb = 0;
    if (cursorState > 0) verb = findVerbAtPos(mouse_x, mouse_y);
    for (uint8_t i = 0; i < MAX_VERBS; i++)
    {
        if (i == verb && verbs[verb].hicolor) drawVerb(i, 1);
        else drawVerb(i, 0);
    }
    verbMouseOverSlot = verb;
}

/* setUserState: очистка всей области глаголов и инвентаря */
void clearVerbArea(void)
{
    screen_clear_rect(0, VERB_TOP, 320, VERB_TOP + 8 * 88 > 200 ? 200 : VERB_TOP + 8 * 88);
}

/* verbs.cpp: ScummEngine_v2::redrawV2Inventory */
void redrawV2Inventory(void)
{
    mouseOverBoxV2 = -1;
    if (!(userState & USERSTATE_IFACE_INVENTORY)) return;
    screen_clear_rect(0, VERB_TOP + 32, 320, 200);
    uint8_t ego = (uint8_t)VAR(VAR_EGO);
    int16_t max_inv = (int16_t)getInventoryCount(ego) - inventoryOffset;
    if (max_inv > 4) max_inv = 4;
    for (uint8_t i = 0; i < max_inv; i++)
    {
        uint16_t obj = findInventory(ego, (uint8_t)(i + 1 + inventoryOffset));
        if (obj == 0) break;
        uint16_t a;
        uint8_t pg = getObjOrActorName(obj, &a);
        if (!pg) fatal("INV NAME", obj);
        int16_t right;
        drawFarString(mouseOverBoxes[i].l, mouseOverBoxes[i].t + VERB_TOP, mo_color[i], pg, a, 18, &right);
    }
    uint8_t arrow[4] = { ' ', 1, 2, 0 };   /* на стеке: строку читает банк C */
    if (inventoryOffset > 0)
        charset_drawString(mouseOverBoxes[kInventoryUpArrow].l, mouseOverBoxes[kInventoryUpArrow].t + VERB_TOP,
                           mo_color[kInventoryUpArrow], arrow);
    arrow[1] = 3;
    arrow[2] = 4;
    if (inventoryOffset + 4 < getInventoryCount(ego))
        charset_drawString(mouseOverBoxes[kInventoryDownArrow].l, mouseOverBoxes[kInventoryDownArrow].t + VERB_TOP,
                           mo_color[kInventoryDownArrow], arrow);
}

/* verbs.cpp: checkV2MouseOver — подсветка перерисовкой текста цветом выделения */
static void checkV2MouseOver(void)
{
    if (!(userState & USERSTATE_IFACE_INVENTORY))
    {
        mouseOverBoxV2 = -1;
        return;
    }
    int8_t new_box = -1;
    if (cursorState > 0)
        for (uint8_t i = 0; i < 7; i++)
        {
            const struct Box8 *b = &mouseOverBoxes[i];
            int16_t y = mouse_y - VERB_TOP;
            if (mouse_x >= b->l && mouse_x < b->r && y >= b->t && y < b->b)
            {
                new_box = (int8_t)i;
                break;
            }
        }
    if (new_box != mouseOverBoxV2)
    {
        if (mouseOverBoxV2 != -1)
        {
            const struct Box8 *b = &mouseOverBoxes[mouseOverBoxV2];
            screen_recolor(b->l, b->t + VERB_TOP, b->r, b->b + VERB_TOP, HI_COLOR, mo_color[mouseOverBoxV2]);
        }
        if (new_box != -1)
        {
            const struct Box8 *b = &mouseOverBoxes[new_box];
            screen_recolor(b->l, b->t + VERB_TOP, b->r, b->b + VERB_TOP, mo_color[new_box], HI_COLOR);
        }
        mouseOverBoxV2 = new_box;
    }
}

/* verbs.cpp: ScummEngine_v2::handleMouseOver */
void handleMouseOver(uint8_t updateInventory)
{
    if (cursorState > 0) verbMouseOver(findVerbAtPos(mouse_x, mouse_y));
    if (updateInventory) inventoryOffset = 0;
    if (updateInventory) redrawV2Inventory();
    checkV2MouseOver();
}

/* verbs.cpp: checkV2Inventory */
static uint16_t checkV2Inventory(int16_t x, int16_t y)
{
    y -= VERB_TOP;
    if (y < 32) return 0;
    uint8_t ego = (uint8_t)VAR(VAR_EGO);
    const struct Box8 *b = &mouseOverBoxes[kInventoryUpArrow];
    if (x >= b->l && x < b->r && y >= b->t && y < b->b)
    {
        if (inventoryOffset >= 2)
        {
            inventoryOffset -= 2;
            redrawV2Inventory();
        }
    }
    b = &mouseOverBoxes[kInventoryDownArrow];
    if (x >= b->l && x < b->r && y >= b->t && y < b->b)
        if (inventoryOffset + 4 < getInventoryCount(ego))
        {
            inventoryOffset += 2;
            redrawV2Inventory();
        }
    uint8_t object;
    for (object = 0; object < 4; object++)
    {
        b = &mouseOverBoxes[object];
        if (x >= b->l && x < b->r && y >= b->t && y < b->b) break;
    }
    if (object >= 4) return 0;
    return findInventory(ego, (uint8_t)(object + 1 + inventoryOffset));
}

/* verbs.cpp: ScummEngine_v2::checkExecVerbs. mouseAndKeyboardStat: клавиша (<0x200),
 * 0x8000 — щелчок левой, 0x4000 — правой (MBS_LEFT_CLICK / MBS_RIGHT_CLICK) */
void checkExecVerbs(void)
{
    uint16_t stat = mouseAndKeyboardStat;
    if (userPut <= 0 || stat == 0) return;
    if (stat < 0x200)
    {
        for (uint8_t i = 1; i < MAX_VERBS; i++)
        {
            struct VerbSlot *vs = &verbs[i];
            if (vs->verbid && vs->saveid == 0 && vs->curmode == 1 && stat == vs->key)
            {
                runInputScript(kVerbClickArea, vs->verbid, 1);
                return;
            }
        }
        int8_t object = -1;
        uint8_t ego = (uint8_t)VAR(VAR_EGO);
        switch (stat)
        {
        case 'u':
            if (inventoryOffset >= 2)
            {
                inventoryOffset -= 2;
                redrawV2Inventory();
            }
            return;
        case 'j':
            if (inventoryOffset + 4 < getInventoryCount(ego))
            {
                inventoryOffset += 2;
                redrawV2Inventory();
            }
            return;
        case 'i': object = 0; break;
        case 'o': object = 1; break;
        case 'k': object = 2; break;
        case 'l': object = 3; break;
        }
        if (object != -1)
        {
            uint16_t obj = findInventory(ego, (uint8_t)(object + 1 + inventoryOffset));
            if (obj > 0) runInputScript(kInventoryClickArea, obj, 0);
            return;
        }
        runInputScript(kKeyClickArea, stat, 1);
        return;
    }
    uint8_t code = (stat & 0x8000) ? 1 : 2;
    if (mouse_y >= VERB_TOP && mouse_y <= VERB_TOP + 8)
        runInputScript(kSentenceClickArea, 0, 0);
    else if (mouse_y > VERB_TOP + 32)
    {
        uint16_t object = checkV2Inventory(mouse_x, mouse_y);
        if (object > 0) runInputScript(kInventoryClickArea, object, 0);
    }
    else
    {
        uint8_t over = findVerbAtPos(mouse_x, mouse_y);
        if (over != 0)
            runInputScript(kVerbClickArea, verbs[over].verbid, code);
        else if (mouse_y >= 16 && mouse_y < VERB_TOP)
            runInputScript(kSceneClickArea, 0, code);
        else if (mouse_y >= VERB_TOP)
            runInputScript(kVerbClickArea, 0, code);
    }
}

/* script_v2.cpp: o2_drawSentence */
void drawSentence(void)
{
    if (!(userState & USERSTATE_IFACE_SENTENCE)) return;
    uint8_t slot = getVerbSlot((uint8_t)VAR(VAR_SENTENCE_VERB), 0);
    if (!slot || !verbs[slot].name_page) return;
    uint8_t buf[80];
    uint8_t n = 0;
    uint16_t a;
    uint8_t pg = verbs[slot].name_page;
    a = verbs[slot].name_addr;
    while (n < 40 && (buf[n] = far_rb(pg, a + n)) != 0) n++;
    if (VAR(VAR_SENTENCE_OBJECT1) > 0)
    {
        pg = getObjOrActorName((uint16_t)VAR(VAR_SENTENCE_OBJECT1), &a);
        if (pg)
        {
            buf[n++] = ' ';
            for (uint8_t k = 0; n < 78 && (buf[n] = far_rb(pg, a + k)) != 0; k++) n++;
        }
    }
    int16_t prep = VAR(VAR_SENTENCE_PREPOSITION);
    if (prep > 0 && prep <= 4)
    {
        static const char *const prepositions[5] = { " ", " in", " with", " on", " to" };
        for (const char *p = prepositions[prep]; *p && n < 78; p++) buf[n++] = (uint8_t)*p;
    }
    if (VAR(VAR_SENTENCE_OBJECT2) > 0)
    {
        pg = getObjOrActorName((uint16_t)VAR(VAR_SENTENCE_OBJECT2), &a);
        if (pg)
        {
            buf[n++] = ' ';
            for (uint8_t k = 0; n < 78 && (buf[n] = far_rb(pg, a + k)) != 0; k++) n++;
        }
    }
    buf[n] = 0;
    /* не больше 40 печатных знаков ('@' не считается) */
    uint8_t len = 0, i = 0, out[80], j = 0;
    while (buf[i])
    {
        if (buf[i] != '@') len++;
        if (len > 40) break;
        out[j++] = buf[i++];
    }
    out[j] = 0;
    screen_clear_rect(0, VERB_TOP, 320, VERB_TOP + 8);
    charset_drawString(0, VERB_TOP, 13, out);
}
