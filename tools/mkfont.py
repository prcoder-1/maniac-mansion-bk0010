#!/usr/bin/env python3
"""Шрифт 6x8 для БК из встроенного шрифта SCUMM v2 (ScummVM: englishCharsetDataV2
в engines/scumm/charset-fontdata.cpp).

Колонка c строки текста MM (8 точек) встаёт на БК в x = c*32/5 (шаг 6.4), поэтому
знак — 6 точек: из 8 колонок глифа убираются две. Сначала — пустые (промежуток),
затем совпадающие с соседней (жирные штрихи), затем с наименьшим числом точек.
Ручные правки — словарь FIX (знак: 8 строк по 6 символов '#'/'.').

    mkfont.py charset-fontdata.cpp font6.s [preview.png]

Режим con — шрифт служебного текста ядра (con.c: LOADING, fatal) из того же
шрифта игры: 8x8 как есть, знаки 040..0137, младший бит — левая точка.

    mkfont.py con charset-fontdata.cpp font_pc.s
"""
import re, struct, sys, zlib

FIX = {}

con = sys.argv[1] == 'con'
if con:
    sys.argv.pop(1)
src = open(sys.argv[1]).read()
body = src[src.index('englishCharsetDataV2[] = {'):]
body = body[:body.index('};')]
data = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', body)]
nglyph = len(data) // 8

if con:
    with open(sys.argv[2], 'w') as f:
        f.write('/ Шрифт 8x8 служебного текста (con.c) — шрифт игры (tools/mkfont.py con из\n')
        f.write('/ englishCharsetDataV2 ScummVM): знаки 040..0137 по 8 байт, младший бит — левая точка\n')
        f.write('\t.globl _font\n\t.text\n_font:\n')
        for g in range(0o40, 0o140):
            rows = [int('{:08b}'.format(r)[::-1], 2) for r in data[g * 8:g * 8 + 8]]
            f.write('\t.byte ' + ', '.join('0%o' % v if v else '0' for v in rows) + '\n')
    sys.exit(0)


def squeeze(rows):
    cols = [[(r >> (7 - c)) & 1 for r in rows] for c in range(8)]
    while len(cols) > 6:
        cand = None
        for i, col in enumerate(cols):          # пустая
            if not any(col):
                cand = i
                break
        if cand is None:
            for i in range(len(cols) - 1):      # дубль соседней
                if cols[i] == cols[i + 1]:
                    cand = i + 1
                    break
        if cand is None:                         # наименее заметная (не крайние сначала)
            cand = min(range(len(cols)), key=lambda i: (sum(cols[i]), i in (0, len(cols) - 1)))
        del cols[cand]
    out = []
    for y in range(8):
        v = 0
        for c in range(6):
            v |= cols[c][y] << (5 - c)
        out.append(v)
    return out


glyphs = []
for g in range(nglyph):
    rows = data[g * 8:g * 8 + 8]
    if g in FIX:
        glyphs.append([int(r.replace('#', '1').replace('.', '0'), 2) for r in FIX[g]])
    else:
        glyphs.append(squeeze(rows))

with open(sys.argv[2].replace('.s', '.c'), 'w') as f:   # копия для хостовой сборки
    f.write('/* Шрифт 6x8 (tools/mkfont.py): копия font6.s для хостовой сборки */\n#include <stdint.h>\n')
    f.write('const uint8_t font6[] = {\n')
    for g in glyphs:
        f.write('    ' + ', '.join('0%o' % v for v in g) + ',\n')
    f.write('};\n')
with open(sys.argv[2], 'w') as f:
    f.write('/ Шрифт 6x8 (tools/mkfont.py из englishCharsetDataV2 ScummVM): %d знаков по 8 байт,\n' % nglyph)
    f.write('/ бит 5 — левая точка\n\t.globl _font6\n\t.text\n_font6:\n')
    for g in glyphs:
        f.write('\t.byte ' + ', '.join('0%o' % v for v in g) + '\n')

if len(sys.argv) > 3:
    W, H = 16 * 8, ((nglyph + 15) // 16) * 10
    pix = [[0] * W for _ in range(H)]
    for g, rows in enumerate(glyphs):
        gx, gy = (g % 16) * 8, (g // 16) * 10
        for y, v in enumerate(rows):
            for x in range(6):
                if v >> (5 - x) & 1:
                    pix[gy + y][gx + x] = 255
    S = 3
    raw = b''.join(b'\0' + bytes(c for x in range(W) for c in [pix[y // S][x]] * 3 * S)
                   for y in range(H * S) for _ in [0])
    raw = b''.join(b'\0' + bytes(v for x in range(W) for v in [pix[y][x]] * 3 * S) for y in range(H) for _ in range(S))
    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d))
    open(sys.argv[3], 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W * S, H * S, 8, 2, 0, 0, 0))
                                  + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
print('%d знаков' % nglyph)
