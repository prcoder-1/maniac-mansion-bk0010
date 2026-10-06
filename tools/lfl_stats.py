#!/usr/bin/env python3
"""Справочник ресурсов LFL (эталон для тестов): по индексу 00.LFL — для каждого
костюма/скрипта/звука файл, смещение, длину и сумму байтов (или BAD), для комнат —
длину блока комнаты и сумму. Формат строк совпадает с выводом test/host/restest.

    lfl_stats.py [КАТАЛОГ]
"""
import os, struct, sys

d = sys.argv[1] if len(sys.argv) > 1 else '.'


def rd(n):
    p = os.path.join(d, '%02d.LFL' % n)
    return bytes(b ^ 0xFF for b in open(p, 'rb').read()) if os.path.exists(p) else None


ix = rd(0)
p = 2
nobj = struct.unpack_from('<H', ix, p)[0]
p += 2 + nobj
nrooms = ix[p]
p += 1 + nrooms * 3
lists = []
for name in ('costume', 'script', 'sound'):
    n = ix[p]
    p += 1
    rooms = ix[p:p + n]
    p += n
    offs = struct.unpack_from('<%dH' % n, ix, p)
    p += 2 * n
    lists.append((name, rooms, offs))

for r in range(1, nrooms):
    f = rd(r)
    if f is None:
        continue
    size = struct.unpack_from('<H', f, 0)[0]
    print('room %d %d %d' % (r, size, sum(f[:size]) & 0xFFFF))
for name, rooms, offs in lists:
    for i in range(1, len(rooms)):
        r, o = rooms[i], offs[i]
        f = rd(r) if r else None
        if f is None or o == 0xFFFF or o + 2 > len(f):
            print('%s %d BAD' % (name, i))
            continue
        size = struct.unpack_from('<H', f, o)[0]
        if size < 4 or o + size > len(f):
            print('%s %d BAD' % (name, i))
            continue
        print('%s %d %d %d' % (name, i, size, sum(f[o:o + size]) & 0xFFFF))
