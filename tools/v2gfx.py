#!/usr/bin/env python3
"""Эталон графики комнат SCUMM v2 для БК: независимая реализация на Python.

GdiV2::prepareDrawBitmap (весь кадр от x=0, таблица повторов обнулена) ->
EGA-картинка; сжатие 4:5 (колонки x%5==4 отбрасываются, bx = x - x/5);
дизеринг EGA16 -> 4 цвета БК матрицей 2x2 по (bx&1, y&1); упаковка 2 бита на
точку (точка 0 — младшие биты). Маска (z-plane) — байт на полосу 8 точек на
строку, строки подряд по W/8 байт.

    v2gfx.py КАТАЛОГ КОМНАТА [out.png]   — печать сумм, при out.png — картинка
    v2gfx.py КАТАЛОГ all                 — суммы всех комнат (эталон для тестов)
"""
import os, struct, sys, zlib

# Цвет EGA -> 2x2 цветов БК [TL, TR, BL, BR]: 0 чёрный, 1 синий, 2 зелёный, 3 красный.
# Источник истины — gfx_pal.h; здесь копия для независимой проверки.
BG_PAT = [
    [0, 0, 0, 0], [1, 1, 1, 1], [2, 2, 2, 2], [1, 2, 2, 1],
    [3, 3, 3, 3], [3, 1, 1, 3], [3, 2, 0, 3], [1, 2, 2, 1],
    [1, 0, 0, 2], [1, 1, 1, 1], [2, 2, 2, 2], [1, 2, 2, 1],
    [3, 3, 3, 3], [3, 1, 1, 3], [3, 2, 2, 3], [1, 2, 2, 1],
]
BK_RGB = [(0, 0, 0), (0, 0, 255), (0, 255, 0), (255, 0, 0)]


def load(d, n):
    return bytes(b ^ 0xFF for b in open(os.path.join(d, '%02d.LFL' % n), 'rb').read())


def decode(src, p, w, h):
    """-> (картинка EGA [y][x], маска bytes, смещение после маски)"""
    img = [[0] * w for _ in range(h)]
    dt = [0] * 128
    run, color, dither = 1, 0, False
    for x in range(w):
        for y in range(h):
            run -= 1
            if run == 0:
                data = src[p]; p += 1
                if data & 0x80:
                    run, dither = data & 0x7F, True
                else:
                    run, dither = data >> 4, False
                color = data & 0x0F
                if run == 0:
                    run = src[p]; p += 1
            if not dither:
                dt[y] = color
            img[y][x] = dt[y]
    ns = w // 8
    mask = bytearray(ns * h)
    x, y = 0, 0
    while x < ns:
        run = src[p]; p += 1
        flag = run & 0x80
        if flag:
            run &= 0x7F
            data = src[p]; p += 1
        while True:
            if not flag:
                data = src[p]; p += 1
            mask[y * ns + x] = data
            y += 1
            if y >= h:
                y, x = 0, x + 1
                if x >= ns:
                    break
            run -= 1
            if run == 0:
                break
    return img, bytes(mask), p


def to_bk(img, w, h):
    bw = w - w // 5 if w % 5 == 0 else len([x for x in range(w) if x % 5 != 4])
    stride = (bw + 3) // 4
    out = bytearray(stride * h)
    for y in range(h):
        for x in range(w):
            if x % 5 == 4:
                continue
            bx = x - x // 5
            c = BG_PAT[img[y][x]][((y & 1) << 1) | (bx & 1)]
            out[y * stride + (bx >> 2)] |= c << ((bx & 3) * 2)
    return bytes(out), stride, bw


def room(d, n):
    f = load(d, n)
    w, h = struct.unpack_from('<HH', f, 4)
    io = struct.unpack_from('<H', f, 0x0A)[0]
    img, mask, _ = decode(f, io, w, h)
    bk, stride, bw = to_bk(img, w, h)
    return w, h, bk, stride, bw, mask


def png(path, pix, w, h):
    raw = b''.join(b'\0' + bytes(c for px in row for c in px) for row in pix)
    def chunk(t, data):
        return struct.pack('>I', len(data)) + t + data + struct.pack('>I', zlib.crc32(t + data))
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                          + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def main():
    d = sys.argv[1]
    rooms = range(1, 61) if sys.argv[2] == 'all' else [int(sys.argv[2])]
    for n in rooms:
        if not os.path.exists(os.path.join(d, '%02d.LFL' % n)):
            continue
        w, h, bk, stride, bw, mask = room(d, n)
        print('room %d %d %d %d %d %d' % (n, w, h, stride, sum(bk) & 0xFFFF, sum(mask) & 0xFFFF))
        if len(sys.argv) > 3:
            pix = [[BK_RGB[(bk[y * stride + (x >> 2)] >> ((x & 3) * 2)) & 3] for x in range(bw)] for y in range(h)]
            png(sys.argv[3], pix, bw, h)


main()
