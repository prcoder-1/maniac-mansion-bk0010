/* Декодер графики SCUMM v2 в формат БК (обёртка; разбор RLE — gfxv2.s, его
 * Си-модель для хоста — test/host/host_gfx.c).
 * gfx.cpp: GdiV2::prepareDrawBitmap — RLE по колонкам сверху вниз; байт с битом 7 —
 * «повтор»: точки берутся из таблицы предыдущей колонки (dither_table) на той же
 * строке; длина 0 — длина в следующем байте. За картинкой — маска (z-plane):
 * байт на полосу 8 точек на строку, RLE по полосам.
 *
 * Сжатие 4:5: колонка мира x с x%5==4 отбрасывается, остальные встают в
 * bx = x - x/5; 5 колонок дают ровно байт БК (4 точки по 2 бита, точка 0 — младшие
 * биты). Точка 3 берёт цвет колонки 4, если в колонке 3 он тот же, что в колонке 2:
 * вертикальная линия в одну колонку не пропадает. Цвет EGA -> цвет БК матрицей 2x2 по (bx&1, y&1) — узор не плывёт при
 * сжатии и прокрутке. Картинка пишется в PG_BITMAP (строки по bm_stride байт),
 * маска — в PG_MASK (строки по bm_strips байт), как getMaskBuffer в ScummVM. */
#include "scumm.h"
#include "gfx.h"

/* Узоры 2x2 цветов EGA на БК — по палитре комнаты (pal11.c, tools/pal11.py; палитра 0 —
 * ручные узоры прежней таблицы) */
extern const uint8_t room_pal[64], pal_pat[16][16][4], pal_text[16][16];

/* v2_pat[y&1][k][цвет]: биты точки k (0..3) байта БК; чётность bx = k&1, так как
 * байт начинается с чётной bx. Замена цветов комнаты (_roomPalette) — здесь же. */
void gfx_set_pal(void)
{
#ifdef HOST
    uint8_t p = 0;   /* хостовый эталон (v2gfx.py) — палитра 0 */
#else
    uint8_t p = room_pal[currentRoom & 63];
#endif
    pal_want = p;
    for (uint8_t c = 0; c < 16; ++c)
        for (uint8_t k = 0; k < 4; ++k) cur_pat[c][k] = pal_pat[p][c][k];
    for (uint8_t yp = 0; yp < 2; ++yp)
        for (uint8_t k = 0; k < 4; ++k)
            for (uint8_t c = 0; c < 16; ++c)
            {
                uint8_t e = roomPalette[c] & 15;
                uint8_t b = cur_pat[e][(yp << 1) | (k & 1)];
                v2_pat[yp][k][c] = (uint8_t)(b << (k * 2));
            }
}

void gfx_init_pal(void)
{
    for (uint8_t c = 0; c < 16; ++c)
    {
        roomPalette[c] = c;
        cur_tpal[c] = pal_text[0][c];   /* включена палитра 0 (main.c vctl_set) */
    }
    pal_cur = 0;
    gfx_set_pal();
}

/* Декодировать картинку v2 (страница/адрес src) размером w x h в мир с точки
 * (x0, y0): картинка комнаты (0, 0, ширина комнаты) или объекта. x0 кратно 8.
 * Возвращает адрес за маской. */
static uint16_t decode_ex(uint8_t spage, uint16_t src, uint16_t x0, uint8_t y0, uint16_t w, uint8_t h, uint8_t tables)
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
    p.run0 = 0;
    p.color0 = 0;
    p.tab = tables ? (uint16_t)(uintptr_t)v2_strips : 0;
    p.m8 = 0;
    struct v2m m;
    m.spage = spage;
    m.src = v2_pix(&p);
    m.nstrips = w >> 3;
    m.h = h;
    m.dest = PAGE_ADDR + (uint16_t)y0 * bm_strips + (x0 >> 3);
    m.stride = bm_strips;
    m.run0 = m.flag0 = m.data0 = 0;
    m.tab = tables ? (uint16_t)(uintptr_t)v2_zstrips : 0;
    return v2_mask(&m);
}

uint16_t v2_decode(uint8_t spage, uint16_t src, uint16_t x0, uint8_t y0, uint16_t w, uint8_t h)
{
    return decode_ex(spage, src, x0, y0, w, h, 0);
}

/* gfx.cpp: GdiV2::prepareDrawBitmap по таблице (_roomStrips) — полосы [s0, s1) фона
 * и маски заново из RLE комнаты. Дизеринг на границе полосы данных не встречается
 * (проверено по всем комнатам), так что пустая dither_table перезапуск не портит. */
void room_redraw_strips(uint8_t s0, uint8_t s1)
{
    if (s1 > bm_strips) s1 = bm_strips;
    if (s0 >= s1) return;
    struct v2p p;
    /* группа 5 колонок на краях — целиком: точка 3 зависит от колонок 2..4 (gfxv2.s) */
    uint8_t ps0 = s0 && (s0 * 8) % 5 == 3 ? s0 - 1 : s0;
    uint16_t x0 = (uint16_t)ps0 * 8;
    p.spage = room_page;
    p.src = (uint16_t)(v2_strips[ps0][0] | (v2_strips[ps0][1] << 8));
    p.w = (uint16_t)(s1 - ps0) * 8;
    if ((x0 + p.w) % 5 == 4 && x0 + p.w < roomWidth) p.w++;
    p.h = roomHeight;
    p.m5 = x0 % 5;
    p.dest = PAGE_ADDR + ((x0 - x0 / 5) >> 2);
    p.stride = bm_stride;
    p.yodd = 0;
    p.run0 = v2_strips[ps0][2];
    p.color0 = (uint16_t)(int16_t)(int8_t)v2_strips[ps0][3];
    p.tab = 0;
    p.m8 = 0;
    v2_pix(&p);
    struct v2m m;
    m.spage = room_page;
    m.src = (uint16_t)(v2_zstrips[s0][0] | (v2_zstrips[s0][1] << 8));
    m.nstrips = (uint16_t)(s1 - s0);
    m.h = roomHeight;
    m.dest = PAGE_ADDR + s0;
    m.stride = bm_strips;
    m.run0 = v2_zstrips[s0][2] & 0x7F;
    m.flag0 = v2_zstrips[s0][2] >> 7;
    m.data0 = v2_zstrips[s0][3];
    m.tab = 0;
    v2_mask(&m);
}

/* Фон комнаты целиком (GdiV2::roomChanged + полная перерисовка): размеры, очистка
 * хвостов строк, картинка и маска. room — страница/адрес блока комнаты. */
void room_decode(uint8_t rpage, uint16_t room)
{
    uint16_t w = far_rw(rpage, room + 4);
    uint8_t h = (uint8_t)far_rw(rpage, room + 6);
    bm_stride = (uint16_t)((BK_W(w) + 3) >> 2);
    bm_strips = (uint8_t)(w >> 3);
    if (w % 5)   /* иначе декодер пишет каждый байт сам (все комнаты MM: 320/640/960) */
        far_fill(PG_BITMAP, PAGE_ADDR, 0, (uint16_t)(bm_stride * h));
    decode_ex(rpage, room + far_rw(rpage, room + 0x0A), 0, 0, w, h, 1);   /* + StripTable */
}
