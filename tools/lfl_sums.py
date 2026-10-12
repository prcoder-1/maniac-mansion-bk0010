#!/usr/bin/env python3
"""Длины и суммы байтов файлов игры (после XOR 0xFF) — эталон test/m0sum.h.

    lfl_sums.py КАТАЛОГ > test/m0sum.h

Файлы — уже с правками порта (tools/lfl_patch.py: KT, титры).
"""
import os, sys

d = sys.argv[1]
names = sorted(f for f in os.listdir(d) if len(f) == 6 and f.endswith('.LFL') and f[:2].isdigit())
lens, sums = [], []
for f in names:
    b = bytes(x ^ 0xFF for x in open(os.path.join(d, f), 'rb').read())
    lens.append(len(b))
    sums.append(sum(b) & 0xFFFF)
print('/* Сгенерировано tools/lfl_sums.py: длины и суммы байтов NN.LFL после XOR 0xFF')
print(' * (MM v1 DOS с правками порта, tools/lfl_patch.py) — test/host/lfltest.c */')
print('#pragma once')
print('#include <stdint.h>')
print('#define LFL_FILES %d' % len(names))
print('static const uint16_t lfl_len[LFL_FILES] = {%s};' % ','.join(map(str, lens)))
print('static const uint16_t lfl_sum[LFL_FILES] = {%s};' % ','.join(map(str, sums)))
