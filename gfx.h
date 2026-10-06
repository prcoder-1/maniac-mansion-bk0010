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

struct v2p { uint16_t spage, src, w, h, m5, dest, stride, yodd; };
struct v2m { uint16_t spage, src, nstrips, h, dest, stride; };
uint16_t v2_pix(const struct v2p *p);   /* gfxv2.s */
uint16_t v2_mask(struct v2m *p);

void gfx_init_pal(void);
void gfx_set_pal(void);
void room_decode(uint8_t rpage, uint16_t room);
uint16_t v2_decode(uint8_t spage, uint16_t src, uint16_t x0, uint8_t y0, uint16_t w, uint8_t h);

/* ширина фона комнаты в точках БК для ширины w точек мира */
#define BK_W(w) ((w) - (w) / 5)
