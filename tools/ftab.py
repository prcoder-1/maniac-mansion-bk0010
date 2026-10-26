#!/usr/bin/env python3
"""Таблицы быстрого чтения в образе дискеты ANDOS.

    ftab.py ОБРАЗ.IMG kernel.map BANKC.map

Загрузчик (loader.s ld_file) читает файл, лежащий на диске подряд, одним вызовом
драйвера ПЗУ КНГМД вместо EMT 36. Где файл, он узнаёт из таблиц {блок, длина,
первое слово} — code_ft в ядре (REPL, BANKA..BANKD, SNDDRV) и lfl_ft в банке C
(NN.LFL). Скрипт находит файлы в каталоге и FAT образа и пишет таблицы прямо в
копии MANIAC (грузится с 0760) и BANKC (с 0100000) на образе. Файл не подряд —
нули: его читает ДОС. Первое слово загрузчик сверяет: файлы переложены (копия на
другой диск) — тоже ДОС.

Заодно — метка сборки сохранений sv_build в BANKC (saveload.c): CRC32 файлов
MANIAC.BIN, BANKA и BANKB рядом с образом (до правки таблиц) — сохранение другой
сборки не загружается.
"""
import os
import struct
import sys
import zlib


def sym(mapfile, name):
    for line in open(mapfile):
        p = line.split()
        if len(p) == 2 and p[1] == name and p[0].startswith('0x'):
            return int(p[0], 16)
    sys.exit('ftab: нет %s в %s' % (name, mapfile))


def main():
    path, kmap, cmap = sys.argv[1:4]
    img = bytearray(open(path, 'rb').read())
    bps, spc, res, nfat, nroot, total = struct.unpack('<HBHBHH', img[11:21])
    spf = struct.unpack('<H', img[22:24])[0]
    fat_off = res * bps
    root_off = (res + nfat * spf) * bps
    data_off = root_off + nroot * 32
    csize = spc * bps

    def fat_get(n):
        v = struct.unpack('<H', img[fat_off + n * 3 // 2:fat_off + n * 3 // 2 + 2])[0]
        return v >> 4 if n & 1 else v & 0xFFF

    def find(name):
        base, _, ext = name.partition('.')
        key = (base.ljust(8) + ext.ljust(3)).encode()
        for i in range(nroot):
            e = root_off + i * 32
            if img[e:e + 11] == key:
                clu = struct.unpack('<H', img[e + 26:e + 28])[0]
                size = struct.unpack('<I', img[e + 28:e + 32])[0]
                chain = [clu]
                while len(chain) * csize < size:
                    nxt = fat_get(chain[-1])
                    if nxt < 2 or nxt >= 0xFF8:
                        sys.exit('ftab: цепочка %s короче файла' % name)
                    chain.append(nxt)
                return chain, size
        return None, 0

    def entry(name):
        chain, size = find(name)
        if not chain or chain != list(range(chain[0], chain[0] + len(chain))) or size > 0xFFFF:
            return (0, 0, 0)
        off = data_off + (chain[0] - 2) * csize
        return ((off // bps), size, struct.unpack('<H', img[off:off + 2])[0])

    def patch(name, at, words):
        chain, size = find(name)
        data = struct.pack('<%dH' % len(words), *words)
        if not chain or at + len(data) > size:
            sys.exit('ftab: %s не вмещает таблицу (%#o)' % (name, at))
        for i, b in enumerate(data):
            k, r = divmod(at + i, csize)
            img[data_off + (chain[k] - 2) * csize + r] = b

    code = [w for n in ('REPL', 'BANKA', 'BANKB', 'BANKC', 'BANKD', 'SNDDRV') for w in entry(n)]
    patch('MANIAC', sym(kmap, 'code_ft') - 0o760, code)
    nrooms = next(int(l.split()[2]) for l in open('scumm.h') if l.startswith('#define NUM_ROOMS '))
    lfl = [w for r in range(nrooms) for w in entry('%02d.LFL' % r)]
    patch('BANKC', sym(cmap, 'lfl_ft') - 0o100000, lfl)
    crc = 0
    for n in ('MANIAC.BIN', 'BANKA', 'BANKB'):
        crc = zlib.crc32(open(os.path.join(os.path.dirname(path), n), 'rb').read(), crc)
    patch('BANKC', sym(cmap, 'sv_build') - 0o100000, [crc & 0xFFFF, crc >> 16])
    open(path, 'wb').write(img)
    fast = sum(1 for i in range(0, len(code + lfl), 3) if (code + lfl)[i])
    print('%s: таблицы быстрого чтения — %d файлов' % (path, fast))


main()
