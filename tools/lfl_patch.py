#!/usr/bin/env python3
"""Правки текстов в файлах игры (на месте, повторный запуск ничего не меняет).

    lfl_patch.py КАТАЛОГ

Конец игры: «Press f4 to play again.» -> «Press KT to play again.» (на БК — клавиша КТ).
Текст v1 упакован: бит 7 байта — пробел после символа; файл — XOR 0xFF.
"""
import os, sys

PATCHES = {
    '30.LFL': [(b'Pres\xf3f\xb4t\xefpla', b'Pres\xf3K\xd4t\xefpla')],
    '47.LFL': [(b'Pres\xf3f\xb4t\xefpla', b'Pres\xf3K\xd4t\xefpla')],
}
for name, subs in PATCHES.items():
    path = os.path.join(sys.argv[1], name)
    d = bytes(b ^ 0xff for b in open(path, 'rb').read())
    new = d
    for old, rep in subs:
        if new.count(old) == 1:
            new = new.replace(old, rep)
        elif new.count(rep) != 1:
            sys.exit('lfl_patch: %s: нет текста для правки' % path)
    if new != d:
        open(path, 'wb').write(bytes(b ^ 0xff for b in new))
        print('lfl_patch: %s исправлен' % path)
