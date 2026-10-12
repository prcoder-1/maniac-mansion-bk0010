#!/usr/bin/env python3
"""Эталон графики комнат SCUMM v1 (MM DOS) для БК: независимая реализация на Python.

GdiV1 (ScummVM gfx.cpp: roomChanged, decodeV1Gfx, drawStripV1Background,
drawStripV1Mask) -> картинка комнаты в цветах v1 и маска; затем формат БК, как он
задуман в порте:
  * сжатие 4:5: колонка мира x % 5 == 4 отбрасывается, точка БК bx = x - x // 5;
  * цвет v1 -> 2x2 цветов БК по (bx & 1, y & 1), таблица V1_PAT (копия, для
    независимой проверки; источник истины — gfx_v1.c v1_pat);
  * 2 бита на точку, точка 0 — младшие биты байта; строки по stride байт;
  * маска — байт на полосу 8 точек мира, строки по strips байт;
  * комната шире 22 групп по 5 полос — кольцо: группа G — в слоте G mod 22
    (880 точек мира, 176 байт строки, 110 полос маски).

Сравнивается видимая часть (её строит room_decode): при x = 0 — строки 0..h-1, байты
фона [0, min(stride, 8 * групп)), полосы маски [0, min(ширина, 40)); у комнаты шире
экрана ещё вид у правого края — полные группы по их слотам кольца.

    v1gfx.py КАТАЛОГ КОМНАТА [out.png]   — суммы комнаты, при out.png — картинка БК
    v1gfx.py КАТАЛОГ all                 — суммы всех комнат (эталон для test/ gfx)
"""
import os, struct, sys, zlib

# цвет v1 -> 2x2 цветов БК [TL, TR, BL, BR]: 0 чёрный, 1 синий, 2 зелёный, 3 красный
V1_PAT = [
    [0, 0, 0, 0], [2, 2, 2, 3], [3, 0, 0, 3], [2, 1, 1, 2],
    [3, 1, 1, 3], [2, 2, 2, 2], [1, 0, 0, 1], [3, 2, 2, 3],
    [3, 3, 3, 3], [3, 0, 0, 3], [3, 3, 3, 3], [1, 1, 1, 1],
    [2, 1, 1, 2], [2, 2, 2, 2], [1, 1, 1, 1], [1, 1, 1, 1],
]
BK_RGB = [(0, 0, 0), (0, 0, 255), (0, 255, 0), (255, 0, 0)]
RING_G, RING_PX = 22, 880


def load(d, n):
    return bytes(b ^ 0xFF for b in open(os.path.join(d, '%02d.LFL' % n), 'rb').read())


def decode_v1(src, p, size):
    """GdiV1::decodeV1Gfx"""
    common = src[p:p + 4]
    p += 4
    out = bytearray()
    while len(out) < size:
        run = src[p]
        p += 1
        if run & 0x80:
            out += bytes([common[(run >> 5) & 3]]) * ((run & 0x1F) + 1)
        elif run & 0x40:
            out += bytes([src[p]]) * ((run & 0x3F) + 1)
            p += 1
        else:
            out += src[p:p + run + 1]
            p += run + 1
    return out[:size]


def room(d, n):
    r = load(d, n)
    ws, hc = r[4], r[5]
    w, h = ws * 8, hc * 8
    colors = list(r[6:9])
    w16 = lambda o: struct.unpack_from('<H', r, o)[0]
    char_map = decode_v1(r, w16(10), 2048)
    pic_map = decode_v1(r, w16(12), ws * hc)
    color_map = decode_v1(r, w16(14), ws * hc)
    mask_map = decode_v1(r, w16(16), ws * hc)
    mp = w16(18)
    mask_char = decode_v1(r, mp + 2, w16(mp) - 8)   # длина в данных на 8 больше (bug #1837375)
    mask_char += bytes(2048 - len(mask_char)) if len(mask_char) < 2048 else b''
    # картинка мира: цвет v1 точки (drawStripV1Background), маска (drawStripV1Mask)
    pix = [bytearray(w) for _ in range(h)]
    mask = [bytearray(ws) for _ in range(h)]
    for s in range(ws):
        for cy in range(hc):
            i = cy + s * hc
            c3 = color_map[i] & 7
            ch = pic_map[i] * 8
            mc = mask_map[i] * 8
            for r8 in range(8):
                y = cy * 8 + r8
                b = char_map[ch + r8]
                for t in range(4):
                    v = (b >> (6 - 2 * t)) & 3
                    pix[y][s * 8 + 2 * t] = pix[y][s * 8 + 2 * t + 1] = colors[v] if v < 3 else c3
                mask[y][s] = mask_char[mc + r8] ^ 0xFF
    return ws, hc, w, h, pix, mask


def to_bk(ws, h, pix, mask, g0, g1):
    """формат БК: фон и маска групп [g0, g1] (кольцо групп для широкой комнаты)"""
    ng = (ws + 4) // 5
    ring = ng > RING_G
    wr = RING_PX if ring else ws * 8
    stride = ((wr - wr // 5) + 3) // 4
    strips = wr // 8
    bm = [bytearray(stride) for _ in range(h)]
    mk = [bytearray(strips) for _ in range(h)]
    for y in range(h):
        for x in range(g0 * 40, min(ws, (g1 + 1) * 5) * 8):
            if x % 5 == 4:
                continue
            g, wx = divmod(x, 40)
            bx = (g % RING_G if ring else g) * 32 + wx - wx // 5
            c = V1_PAT[pix[y][x] & 15][((y & 1) << 1) | (bx & 1)]
            bm[y][bx >> 2] |= c << ((bx & 3) * 2)
        for s in range(g0 * 5, min(ws, (g1 + 1) * 5)):
            g, k = divmod(s, 5)
            mk[y][(g % RING_G if ring else g) * 5 + k] = mask[y][s]
    return stride, strips, bm, mk


def sums(seq):
    """сумма и Флетчер-16 (позиционная)"""
    s = f1 = f2 = 0
    for b in seq:
        s = (s + b) & 0xFFFF
        f1 = (f1 + b) % 255
        f2 = (f2 + f1) % 255
    return s, (f2 << 8) | f1


def view(ws, h, pix, mask, st):
    """вид с полосы st (40 полос): строятся только его группы. x = 0 — байты
    [0, min(stride, 8 * групп)) и полосы [0, min(ws, 40)); иначе — полные группы
    по их слотам (в неполной группе кольца — мусор прежних комнат)"""
    g0, g1 = st // 5, (min(st + 40, ws) - 1) // 5
    stride, strips, bm, mk = to_bk(ws, h, pix, mask, g0, g1)
    ring = (ws + 4) // 5 > RING_G
    if st == 0:
        vis = min(ws, 40)
        nb = min(stride, (g1 + 1) * 8)
        return stride, bm, sums(b for row in bm for b in row[:nb]) + sums(b for row in mk for b in row[:vis])
    gs = [g for g in range(g0, g1 + 1) if g * 5 + 5 <= ws]
    sl = [g % RING_G if ring else g for g in gs]
    return stride, bm, (sums(b for row in bm for q in sl for b in row[q * 8:q * 8 + 8]) +
                        sums(b for row in mk for q in sl for b in row[q * 5:q * 5 + 5]))


def lines(d, n):
    ws, hc, w, h, pix, mask = room(d, n)
    out = []
    for st in ([0, ws - 40] if ws > 40 else [0]):
        stride, bm, s = view(ws, h, pix, mask, st)
        out.append('room %u @%u %u %u %u %u %u %u %u' % ((n, st, w, h, stride) + s))
        if st == 0:
            pic = (stride, bm, w, h)
    return out, pic


def png(path, stride, bm, w, h):
    bw = min(w - w // 5, stride * 4)
    raw = b''.join(b'\0' + bytes(c for bx in range(bw) for c in BK_RGB[(bm[y][bx >> 2] >> ((bx & 3) * 2)) & 3])
                   for y in range(h))
    chunk = lambda t, data: struct.pack('>I', len(data)) + t + data + struct.pack('>I', zlib.crc32(t + data))
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', bw, h, 8, 2, 0, 0, 0)) +
                           chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def main():
    d = sys.argv[1]
    if sys.argv[2] == 'all':
        for n in range(1, 61):
            if os.path.exists(os.path.join(d, '%02d.LFL' % n)):
                print('\n'.join(lines(d, n)[0]))
        return
    n = int(sys.argv[2])
    text, (stride, bm, w, h) = lines(d, n)
    print('\n'.join(text))
    if len(sys.argv) > 3:
        png(sys.argv[3], stride, bm, w, h)


main()
