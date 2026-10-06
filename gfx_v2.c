/* Декодер графики SCUMM v2 в формат БК (обёртка; разбор RLE — gfxv2.s, его
 * Си-модель для хоста — test/host/host_gfx.c).
 * gfx.cpp: GdiV2::prepareDrawBitmap — RLE по колонкам сверху вниз; байт с битом 7 —
 * «повтор»: точки берутся из таблицы предыдущей колонки (dither_table) на той же
 * строке; длина 0 — длина в следующем байте. За картинкой — маска (z-plane):
 * байт на полосу 8 точек на строку, RLE по полосам.
 *
 * Сжатие 4:5: колонка мира x с x%5==4 отбрасывается, остальные встают в
 * bx = x - x/5; 5 колонок дают ровно байт БК (4 точки по 2 бита, точка 0 — младшие
 * биты). Цвет EGA -> цвет БК матрицей 2x2 по (bx&1, y&1) — узор не плывёт при
 * сжатии и прокрутке. Картинка пишется в PG_BITMAP (строки по bm_stride байт),
 * маска — в PG_MASK (строки по bm_strips байт), как getMaskBuffer в ScummVM. */
#include "scumm.h"
#include "gfx.h"

/* Цвет EGA -> 2x2 цветов БК [TL, TR, BL, BR]: 0 чёрный, 1 синий, 2 зелёный, 3 красный
 * (копия — в tools/v2gfx.py) */
static const uint8_t bg_pat[16][4] =
{
    {0, 0, 0, 0}, {1, 1, 1, 1}, {2, 2, 2, 2}, {1, 2, 2, 1},
    {3, 3, 3, 3}, {3, 1, 1, 3}, {3, 2, 0, 3}, {1, 2, 2, 1},
    {1, 0, 0, 2}, {1, 1, 1, 1}, {2, 2, 2, 2}, {1, 2, 2, 1},
    {3, 3, 3, 3}, {3, 1, 1, 3}, {3, 2, 2, 3}, {1, 2, 2, 1},
};

/* v2_pat[y&1][k][цвет]: биты точки k (0..3) байта БК; чётность bx = k&1, так как
 * байт начинается с чётной bx. Замена цветов комнаты (_roomPalette) — здесь же. */
void gfx_set_pal(void)
{
    for (uint8_t yp = 0; yp < 2; ++yp)
        for (uint8_t k = 0; k < 4; ++k)
            for (uint8_t c = 0; c < 16; ++c)
                v2_pat[yp][k][c] = (uint8_t)(bg_pat[roomPalette[c] & 15][(yp << 1) | (k & 1)] << (k * 2));
}

void gfx_init_pal(void)
{
    for (uint8_t c = 0; c < 16; ++c) roomPalette[c] = c;
    gfx_set_pal();
}

/* Декодировать картинку v2 (страница/адрес src) размером w x h в мир с точки
 * (x0, y0): картинка комнаты (0, 0, ширина комнаты) или объекта. x0 кратно 8.
 * Возвращает адрес за маской. */
uint16_t v2_decode(uint8_t spage, uint16_t src, uint16_t x0, uint8_t y0, uint16_t w, uint8_t h)
{
    struct v2p p;
    p.spage = spage;
    p.src = src;
    p.w = w;
    p.h = h;
    p.m5 = x0 % 5;
    p.dest = PAGE_ADDR + (uint16_t)y0 * bm_stride + ((x0 - x0 / 5) >> 2);
    p.stride = bm_stride;
    p.yodd = y0 & 1;
    struct v2m m;
    m.spage = spage;
    m.src = v2_pix(&p);
    m.nstrips = w >> 3;
    m.h = h;
    m.dest = PAGE_ADDR + (uint16_t)y0 * bm_strips + (x0 >> 3);
    m.stride = bm_strips;
    return v2_mask(&m);
}

/* Фон комнаты целиком (GdiV2::roomChanged + полная перерисовка): размеры, очистка
 * хвостов строк, картинка и маска. room — страница/адрес блока комнаты. */
void room_decode(uint8_t rpage, uint16_t room)
{
    uint16_t w = far_rw(rpage, room + 4);
    uint8_t h = (uint8_t)far_rw(rpage, room + 6);
    bm_stride = (uint16_t)((BK_W(w) + 3) >> 2);
    bm_strips = (uint8_t)(w >> 3);
    far_fill(PG_BITMAP, PAGE_ADDR, 0, (uint16_t)(bm_stride * h));
    v2_decode(rpage, room + far_rw(rpage, room + 0x0A), 0, 0, w, h);
}
