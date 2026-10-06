/* PNG без сжатия (deflate «stored») для хостовых снимков в палитре БК */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint32_t crc_tab[256];

static uint32_t crc(uint32_t c, const uint8_t *p, size_t n)
{
    if (!crc_tab[1])
        for (uint32_t i = 0; i < 256; ++i)
        {
            uint32_t v = i;
            for (int k = 0; k < 8; ++k) v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            crc_tab[i] = v;
        }
    c = ~c;
    while (n--) c = crc_tab[(c ^ *p++) & 0xFF] ^ (c >> 8);
    return ~c;
}

static void be32(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }

static void chunk(FILE *f, const char *t, const uint8_t *d, uint32_t n)
{
    uint8_t h[8];
    be32(h, n);
    memcpy(h + 4, t, 4);
    fwrite(h, 1, 8, f);
    fwrite(d, 1, n, f);
    uint32_t c = crc(crc(0, (const uint8_t *)t, 4), d, n);
    be32(h, c);
    fwrite(h, 1, 4, f);
}

/* RGB-картинка w x h */
void png_rgb(const char *path, const uint8_t *rgb, uint32_t w, uint32_t h)
{
    uint32_t row = w * 3 + 1, raw_n = row * h;
    uint8_t *raw = malloc(raw_n);
    for (uint32_t y = 0; y < h; ++y)
    {
        raw[y * row] = 0;
        memcpy(raw + y * row + 1, rgb + y * w * 3, w * 3);
    }
    uint32_t nblk = (raw_n + 65534) / 65535, zn = 2 + raw_n + nblk * 5 + 4;
    uint8_t *z = malloc(zn), *q = z;
    *q++ = 0x78; *q++ = 0x01;
    uint32_t a = 1, b = 0;
    for (uint32_t i = 0; i < raw_n; ++i) { a = (a + raw[i]) % 65521; b = (b + a) % 65521; }
    for (uint32_t off = 0; off < raw_n; off += 65535)
    {
        uint32_t n = raw_n - off < 65535 ? raw_n - off : 65535;
        *q++ = off + n >= raw_n;
        *q++ = n; *q++ = n >> 8; *q++ = ~n; *q++ = (~n) >> 8;
        memcpy(q, raw + off, n);
        q += n;
    }
    be32(q, (b << 16) | a);
    FILE *f = fopen(path, "wb");
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    uint8_t ih[13];
    be32(ih, w); be32(ih + 4, h);
    ih[8] = 8; ih[9] = 2; ih[10] = ih[11] = ih[12] = 0;
    chunk(f, "IHDR", ih, 13);
    chunk(f, "IDAT", z, zn);
    chunk(f, "IEND", 0, 0);
    fclose(f);
    free(raw);
    free(z);
}

static const uint8_t bk_rgb[4][3] = { {0, 0, 0}, {0, 0, 255}, {0, 255, 0}, {255, 0, 0} };

/* картинка БК 2bpp (точка 0 — младшие биты) */
void png_bk(const char *path, const uint8_t *bm, uint16_t stride, uint16_t w, uint16_t h)
{
    uint8_t *rgb = malloc((size_t)w * h * 3);
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x)
            memcpy(rgb + (y * w + x) * 3, bk_rgb[(bm[y * stride + (x >> 2)] >> ((x & 3) * 2)) & 3], 3);
    png_rgb(path, rgb, w, h);
    free(rgb);
}
