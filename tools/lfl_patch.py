#!/usr/bin/env python3
"""Правки в файлах игры (на месте, повторный запуск ничего не меняет).

    lfl_patch.py КАТАЛОГ

Конец игры: «Press f4 to play again.» -> «Press KT to play again.» (на БК — клавиша КТ).
Титры заставки (скрипт 120 в 33.LFL): после «Programmed by ...» — ещё надпись
«Ported to BK 0010 by Prcoder» (print и delay, как у соседних). Скрипт удлиняется: его длина,
переход пропуска заставки (beginOverride в начале) и смещения ресурсов за ним в
индексе 00.LFL — сдвигаются.
Текст v1 упакован: бит 7 байта — пробел после символа, код 1 — новая строка;
файл — XOR 0xFF.
"""
import os, struct, sys

D = sys.argv[1]


def load(name):
    return bytearray(b ^ 0xff for b in open(os.path.join(D, name), 'rb').read())


def save(name, d):
    open(os.path.join(D, name), 'wb').write(bytes(b ^ 0xff for b in d))
    print('lfl_patch: %s исправлен' % os.path.join(D, name))


def fail(msg):
    sys.exit('lfl_patch: ' + msg)


PATCHES = {
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
AFTER = b'Wilmunde\xf2an\xe4Ro\xeeGilbert\x00\x2e\x0f\xff\xff'   # её print и delay
TEXT = b'\xa0' * 3 + b'Porte\xe4t\xefB\xcb001\xb0b\xf9Prcoder'   # 6 пробелов: по центру 40 знаков
INSERT = b'\x14\xff' + TEXT + b'\x00' + b'\x2e\x0f\xff\xff'   # print(255, ...); delay 240
LFL_MAX = 0o34600   # loader.s: файл короче — буфер не доходит до экрана (LD_LOW)

idx = load('00.LFL')
# readClassicIndexFile: объекты 800, комнаты 55 * 3, затем костюмы, скрипты, звуки:
# байты комнат, затем слова смещений
lists, p = [], 2 + 800 + 55 * 3
for n in (35, 200, 100):
    lists.append((p, p + n, n))
    p += 3 * n
sp = lists[1]
if idx[sp[0] + SCRIPT] != ROOM:
    fail('скрипт %d не в %02d.LFL' % (SCRIPT, ROOM))
off = struct.unpack_from('<H', idx, sp[1] + 2 * SCRIPT)[0]
lfl = load('%02d.LFL' % ROOM)
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
if len(lfl) + n >= LFL_MAX:
    fail('%02d.LFL длиннее %o байт' % (ROOM, LFL_MAX))
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
