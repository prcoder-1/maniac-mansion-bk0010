#!/usr/bin/env python3
"""Правки в файлах игры (на месте, повторный запуск ничего не меняет).

    lfl_patch.py КАТАЛОГ

Конец игры: «Press f4 to play again.» -> «Press KT to play again.» (30.LFL, 47.LFL; на БК —
клавиша КТ, буквы шрифта игры — латинские, с виду те же).

Титры заставки (скрипт 120 в 33.LFL): после «Programmed by ...» — ещё надпись
«Ported to BK 0011M by» / «Prcoder» (print и delay, как у соседних). Скрипт удлиняется: его длина,
переход пропуска заставки (beginOverride в начале) и смещения ресурсов за ним в
индексе 00.LFL сдвигаются. Как tools/lfl_patch.py в maniac-v1-bk0010, но индекс v2:
счётчики списков — в самом файле.
Текст упакован: бит 7 байта — пробел после символа, код 1 — новая строка;
файл — XOR 0xFF.
"""
import os, struct, sys

D = sys.argv[1]


def load(name):
    return bytearray(b ^ 0xff for b in open(os.path.join(D, name), 'rb').read())


def save(name, d):
    path = os.path.join(D, name)
    st = os.stat(path)
    open(path, 'wb').write(bytes(b ^ 0xff for b in d))
    os.utime(path, (st.st_atime, st.st_mtime))   # дата файла игры — прежняя (mkandos пишет её в каталог)
    print('lfl_patch: %s исправлен' % path)


def fail(msg):
    sys.exit('lfl_patch: ' + msg)


PATCHES = {   # f4 -> KT (бит 7 последней буквы слова — пробел за ней)
    '30.LFL': [(b'Pres\xf3f\xb4t\xefpla', b'Pres\xf3K\xd4t\xefpla')],
    '47.LFL': [(b'Pres\xf3f\xb4t\xefpla', b'Pres\xf3K\xd4t\xefpla')],
}
for name, subs in PATCHES.items():
    d = load(name)
    new = bytes(d)
    for old, rep in subs:
        if new.count(old) == 1:
            new = new.replace(old, rep)
        elif new.count(rep) != 1:
            fail('%s: нет текста для правки' % name)
    if new != bytes(d):
        save(name, bytearray(new))

# --- титры: надпись после программистов ---
ROOM, SCRIPT = 33, 120
AFTER = b'Wilmunde\xf2an\xe4Ro\xeeGilbert\x00\x2e\x0f\xff\xff'   # «Programmed by»: её print и delay
# по центру 40 знаков, как соседние: 9 пробелов, «Ported to BK 0011M by», новая строка
# (и пробел), ещё 15, «Prcoder»
TEXT = b'\xa0' * 4 + b' Porte\xe4t\xefB\xcb0011\xcdby\x81' + b'\xa0' * 7 + b' Prcoder'
INSERT = b'\x14\xff' + TEXT + b'\x00' + b'\x2e\x0f\xff\xff'   # print(255, ...); delay

idx = load('00.LFL')
if struct.unpack_from('<H', idx, 0)[0] != 0x0100:
    fail('00.LFL — не MM v2')
# resource_v2.cpp readIndexFile: объекты (слово-счётчик и байты), комнаты (байт-счётчик и
# по 3 байта), затем костюмы, скрипты, звуки: байт-счётчик, байты комнат, слова смещений
p = 2
p += 2 + struct.unpack_from('<H', idx, p)[0]
p += 1 + 3 * idx[p]
lists = []
for _ in range(3):
    n = idx[p]
    lists.append((p + 1, p + 1 + n, n))
    p += 1 + 3 * n
sp = lists[1]
if idx[sp[0] + SCRIPT] != ROOM:
    fail('скрипт %d не в %02d.LFL' % (SCRIPT, ROOM))
off = struct.unpack_from('<H', idx, sp[1] + 2 * SCRIPT)[0]
lfl = load('%02d.LFL' % ROOM)
if struct.unpack_from('<H', lfl, 0)[0] > off:
    fail('скрипт %d — внутри блока комнаты' % SCRIPT)
size = struct.unpack_from('<H', lfl, off)[0]
body = bytes(lfl[off:off + size])
if body.count(INSERT) == 1:
    sys.exit(0)   # уже
if b'Prcoder' in body:
    fail('%02d.LFL: надпись другой версии — нужны исходные 00.LFL и %02d.LFL' % (ROOM, ROOM))
at = body.find(AFTER)
if at < 0 or body.count(AFTER) != 1:
    fail('%02d.LFL: нет титров для правки' % ROOM)
at += len(AFTER)
n = len(INSERT)
# cutscene; beginOverride; jumpRelative (пропуск заставки) — за место вставки
if body[4:7] != b'\x40\x58\x18':
    fail('скрипт %d: нет перехода пропуска заставки' % SCRIPT)
jmp = struct.unpack_from('<h', body, 7)[0]
if not 9 + jmp > at:
    fail('скрипт %d: переход пропуска не за местом вставки' % SCRIPT)
struct.pack_into('<h', lfl, off + 7, jmp + n)
struct.pack_into('<H', lfl, off, size + n)
lfl[off + at:off + at] = INSERT
# ресурсы файла за скриптом — дальше на n
for rooms, offs, cnt in lists:
    for i in range(cnt):
        o = struct.unpack_from('<H', idx, offs + 2 * i)[0]
        if idx[rooms + i] == ROOM and o != 0xFFFF and o > off:
            struct.pack_into('<H', idx, offs + 2 * i, o + n)
save('%02d.LFL' % ROOM, lfl)
save('00.LFL', idx)
