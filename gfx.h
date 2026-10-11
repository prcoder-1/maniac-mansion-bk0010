/* Графика: фон комнаты в PG_BITMAP (БК 2bpp, сжатие 4:5), маска в PG_MASK */
#pragma once
#include <stdint.h>

/* состояние (state.c, базовое ОЗУ) */
extern uint8_t roomPalette[16];     /* _roomPalette: замена цветов EGA комнаты */
extern uint16_t bm_stride;          /* байт в строке фона комнаты */
#define BM_ROWS 136
extern uint16_t bm_row[BM_ROWS], bm_mrow[BM_ROWS];   /* адреса строк фона и маски (gfx_v1.c): без умножения на шаг */
extern uint8_t bm_strips;           /* полос по 8 точек (байт в строке маски) */
extern uint8_t bm_ring;             /* широкая комната: фон — кольцо групп (gfx_v1.c) */
#define RING_G 22                   /* групп по 5 полос в кольце: 110 полос, 176 байт строки */
#define RING_PX 880                 /* точка мира x — в точке кольца x mod RING_PX */
#define PHYS_X(x) ((x) >= RING_PX ? (x) - RING_PX : (x))   /* x < 2 * RING_PX */
uint8_t view_ensure(void);          /* видимая часть — в кольце (дорисовать) */
uint8_t gfx_prefetch(void);         /* одна группа рядом с видимой частью */
void bm_blit_ring(uint16_t col, uint16_t y, uint16_t vaddr, uint16_t ncols, uint16_t nrows);

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
void room_decode(uint8_t rpage, uint16_t room);
uint16_t v2_decode(uint8_t spage, uint16_t src, uint16_t x0, uint8_t y0, uint16_t w, uint8_t h);

/* ширина фона комнаты в точках БК для ширины w точек мира */
#define BK_W(w) ((w) - (w) / 5)
