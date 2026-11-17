#!/usr/bin/env python3
"""Шрифт 8x8 для AZБК (модуль azmod) из встроенного шрифта SCUMM v2 (ScummVM:
englishCharsetDataV2 в engines/scumm/charset-fontdata.cpp): как есть, бит 7 — левая точка.

    mkfont8.py charset-fontdata.cpp font8.s
"""
import re, sys

src = open(sys.argv[1]).read()
body = src[src.index('englishCharsetDataV2[] = {'):]
body = body[:body.index('};')]
data = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', body)]
with open(sys.argv[2], 'w') as f:
    f.write('/ Шрифт 8x8 (tools/mkfont8.py из englishCharsetDataV2 ScummVM): %d знаков по 8 байт,\n' % (len(data) // 8))
    f.write('/ бит 7 — левая точка\n\t.globl _font8\n\t.text\n_font8:\n')
    for g in range(0, len(data), 8):
        f.write('\t.byte ' + ', '.join('0%o' % v for v in data[g:g + 8]) + '\n')
