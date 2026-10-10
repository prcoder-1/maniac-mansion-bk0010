/* Отрисовка актёра: банк B (costume_r.c — какие актёры, где, в каком порядке)
 * и банк C (limb.c — конечности кадра, кэш спрайтов, холст). Общее состояние —
 * в базовом ОЗУ (sys.ld _rd): массив actors[] из банка C не виден. */
#pragma once
#include "scumm.h"

#define VIEW_ROW 44                 /* строка БК начала комнаты: 28 + 16 */
#define OUT_W 320
#define OUT_H 136   /* v1: комната 136 строк */

/* прямоугольник точек MM, занятый кадром актёра */
struct DrawnRect { int16_t l, t, r, b; };

/* цель отрисовки: видеопамять или буфер в PG_MASK (холст) */
struct Target { uint16_t base, stride, bxlim, ylim, view0, page; int16_t y0; };

struct Rend
{
    struct Target tg;
    int16_t xstart;         /* _virtscr[kMainVirtScreen].xstart */
    uint16_t view0;         /* точка БК мира в левом крае экрана */
    /* актёр (ставит costume_r.c перед rend_limbs) */
    int16_t actorX, actorY;
    uint16_t c_res, c_animCmds;
    uint8_t c_page, mirror, zbuf, measuring;
    uint8_t spr_ok, spr_act, spr_budget, pad;
    uint8_t cpal[4];
    uint8_t *meas_rec;      /* измерение: место конечностей {t, b, l} по 4 байта */
    uint16_t pos[16], stopped;   /* cost_curpos, cost_stopped */
    /* итог rend_limbs */
    int16_t draw_top, draw_bottom, draw_left, draw_right;
};
extern struct Rend rd;

uint8_t rend_limbs(void);   /* 16 конечностей; бит 0 — _needRedraw */
void target_screen(void);
void restoreRect(int16_t l, int16_t t, int16_t r, int16_t b);
uint8_t cv_open(int16_t l, int16_t t, int16_t r, int16_t b, uint8_t load);
void cv_bgrect(const struct DrawnRect *o);
void cv_view_excl(void);
void cv_close(void);
