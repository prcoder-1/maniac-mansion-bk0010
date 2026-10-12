/* Костюмы (банк B): перевод ClassicCostumeLoader из engines/scumm/costume.cpp —
 * разбор заголовка и анимация (костюмы v2: формат 0x58, GF_OLD_BUNDLE). Отрисовка
 * кадров (proc3) — отдельно. */
#include "actor.h"

/* состояние loadCostume: страница/адрес ресурса и смещения таблиц */
static uint8_t c_page;
static uint16_t c_res, c_dataOffsets, c_animCmds;
static uint8_t c_numAnim;

/* заголовок костюма: глобальные ресурсы после загрузки не вытесняются
 * (resource.c), поэтому место и поля заголовка запоминаются по номеру */
static struct CostInfo cinfo[NUM_COSTUMES];

const struct CostInfo *cost_info(uint8_t id)
{
    struct CostInfo *ci = &cinfo[id < NUM_COSTUMES ? id : 0];
    if (id < NUM_COSTUMES && ci->page) return ci;
    uint8_t page = res_get(rtCostume, id);
    if (!page) return 0;
    /* v1 (формат 0x57): _baseptr = ресурс + 4, цветов нет; animCmds — слово в +6,
     * frameOffsets — +8, dataOffsets — +24 от ресурса; смещения — от _baseptr */
    ci->page = page;
    ci->res = res_addr + 4;
    ci->numAnim = far_rb(page, res_addr + 4);
    ci->fmt = far_rb(page, res_addr + 5);
    ci->animCmds = ci->res + far_rw(page, ci->res + 2);
    return ci;
}

/* после загрузки игры: снимок банка B — из другого сеанса, кэш ресурсов — свой */
void cost_info_reset(void)
{
    spr_cache_clear();
    c64_cache_clear();   /* перекодированные кадры — тоже по адресам костюмов */
    for (uint8_t i = 0; i < NUM_COSTUMES; i++) cinfo[i].page = 0;
}

/* costume.cpp: ClassicCostumeLoader::loadCostume (GF_OLD_BUNDLE: _baseptr = ресурс) */
static uint8_t loadCostume(uint8_t id)
{
    const struct CostInfo *ci = cost_info(id);
    if (!ci) return 0;
    c_page = ci->page;
    c_res = ci->res;
    c_numAnim = ci->numAnim;
    c_dataOffsets = ci->res + 20;
    c_animCmds = ci->animCmds;
    return 1;
}

void costume_reset(struct Actor *a)
{
    a->cost_stopped = 0;
    a->cost_still = 0;
    for (uint8_t i = 0; i < 16; i++)
        a->cost_curpos[i] = a->cost_start[i] = a->cost_end[i] = a->cost_frame[i] = 0xFFFF;
}

/* costume.cpp: ClassicCostumeLoader::costumeDecodeData (usemask — все конечности) */
void costumeDecodeData(struct Actor *a, uint16_t frame)
{
    if (!loadCostume(a->costume)) return;
    a->cost_still = 0;
    int16_t f = a->facing;   /* newDirToOldDir */
    uint8_t d = 3;
    if (f >= 71 && f <= 109) d = 1;
    else if (f >= 109 && f <= 251) d = 2;
    else if (f >= 251 && f <= 289) d = 0;
    uint16_t anim = d + frame * 4;
    if (anim > c_numAnim) return;
    uint16_t off = far_rw(c_page, c_dataOffsets + anim * 2);
    if (off == 0) return;
    uint16_t r = c_res + off;
    uint16_t mask = (uint16_t)far_rb(c_page, r++) << 8;   /* v1: маска конечностей — байт */
    uint8_t i = 0;
    do
    {
        if (mask & 0x8000)
        {
            uint16_t j = far_rb(c_page, r++);
            if (j == 0xFF) j = 0xFFFF;
            if (j == 0xFFFF)
            {
                a->cost_curpos[i] = 0xFFFF;
                a->cost_start[i] = 0;
                a->cost_frame[i] = frame;
            }
            else
            {
                uint8_t extra = far_rb(c_page, r++);
                uint8_t cmd = far_rb(c_page, c_animCmds + j);
                if (cmd == 0x7A)
                    a->cost_stopped &= (uint16_t)~(1u << i);
                else if (cmd == 0x79)
                    a->cost_stopped |= (uint16_t)(1u << i);
                else
                {
                    a->cost_curpos[i] = a->cost_start[i] = j;
                    a->cost_end[i] = j + (extra & 0x7F);
                    if (extra & 0x80) a->cost_curpos[i] |= 0x8000;
                    a->cost_frame[i] = frame;
                }
            }
        }
        i++;
        mask <<= 1;
    } while (mask);
}

/* costume.cpp: ClassicCostumeLoader::increaseAnim. Конечность осталась на той же
 * простой команде, счётчики не тронуты — дальше то же самое: бит в cost_still */
static uint8_t increaseAnim(struct Actor *a, uint8_t slot)
{
    uint8_t looped = 0;
    if (a->cost_curpos[slot] == 0xFFFF) return 0;
    uint16_t highflag = a->cost_curpos[slot] & 0x8000;
    uint16_t i = a->cost_curpos[slot] & 0x7FFF;
    uint16_t end = a->cost_end[slot], i0 = i;
    uint16_t w = far_rw(c_page, c_animCmds + i);   /* команда и следующая за ней */
    uint8_t c = (uint8_t)w;
    uint8_t code = c & 0x7F;
    if (c & 0x80) a->cost_soundCounter++;   /* v <= 3 */
    for (;;)
    {
        if (!highflag)
        {
            if (i++ >= end) i = a->cost_start[slot];
        }
        else if (i != end)
            i++;
        uint8_t nc = i == i0 ? c : i == i0 + 1 ? (uint8_t)(w >> 8) : far_rb(c_page, c_animCmds + i);
        if (nc == 0x7C)
        {
            a->cost_animCounter++;
            looped = 1;
            if (a->cost_start[slot] != end) continue;
        }
        else if (nc == 0x78)
        {
            a->cost_soundCounter++;
            looped = 1;
            if (a->cost_start[slot] != end) continue;
        }
        a->cost_curpos[slot] = i | highflag;
        if (i == i0 && c < 0x78 && !looped) a->cost_still |= 1u << slot;
        return (nc & 0x7F) != code;
    }
}

uint8_t increaseAnims(struct Actor *a)
{
    if (a->cost_still == 0xFFFF) return 0;   /* стоит: ни одна конечность не сменится */
    if (!loadCostume(a->costume)) return 0;
    uint8_t r = 0;
    uint16_t bit = 1;
    for (uint8_t i = 0; i != 16; i++, bit <<= 1)
    {
        if (a->cost_still & bit) continue;
        if (a->cost_curpos[i] == 0xFFFF) a->cost_still |= bit;
        else r += increaseAnim(a, i);
    }
    return r;
}
