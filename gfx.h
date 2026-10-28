/* Графика: фон комнаты в PG_BITMAP (БК 2bpp, сжатие 4:5), маска в PG_MASK */
#pragma once
#include <stdint.h>

/* состояние (state.c, базовое ОЗУ) */
extern uint8_t roomPalette[16];     /* _roomPalette: замена цветов EGA комнаты */
extern uint16_t bm_stride;          /* байт в строке фона комнаты */
extern uint8_t bm_strips;           /* полос по 8 точек (байт в строке маски) */

/* буферы декодера по постоянным адресам (sys.ld) */
extern uint8_t v2_pat[2][4][32];    /* [чётность строки][точка k][цвет] -> биты БК */
extern uint8_t v2_dt[128];          /* dither_table / полоса маски */
extern uint8_t v2_col[4][128];      /* цвета EGA колонок собираемого байта */

/* состояние RLE на входе (run0/color0, run0/flag0/data0) и StripTable: запись
 * {адрес, серия, цвет|байт} на каждую полосу 8 точек (tab = 0 — не писать) */
struct v2p { uint16_t spage, src, w, h, m5, dest, stride, yodd, run0, color0, tab, m8; };
struct v2m { uint16_t spage, src, nstrips, h, dest, stride, run0, flag0, data0, tab; };
uint16_t v2_pix(struct v2p *p);   /* gfxv2.s */
uint16_t v2_mask(struct v2m *p);
extern uint8_t v2_strips[120][4], v2_zstrips[120][4];
void room_redraw_strips(uint8_t s0, uint8_t s1);

void gfx_init_pal(void);
void gfx_set_pal(void);
uint8_t room_decode(uint8_t rpage, uint16_t room);   /* 1 — полосы не декодированы (bg_ensure) */
uint16_t v2_decode(uint8_t spage, uint16_t src, uint16_t x0, uint8_t y0, uint16_t w, uint8_t h);

/* ширина фона комнаты в точках БК для ширины w точек мира */
#define BK_W(w) ((w) - (w) / 5)
