#!/usr/bin/env python3
"""Файл LFL, сжатый ZX0 (формат v2) — для дискеты; распаковывает zx0.s в загрузчике.

    zx0lfl.py SALVADOR ВХОД.LFL ВЫХОД.LFL

Сжимает salvador'ом и проверяет распаковку так, как её делает загрузчик (loader.s,
zx0.s): сжатое (до чётной длины) — в конце буфера 03200..067777, распакованное — с его
начала; запись не должна догнать ещё не прочитанное. Окно — не больше WINDOW: dzx0
собирает старший байт смещения в знаковый байт (swab), дальше 16 Кбайт оно неверно.
"""
import os, subprocess, sys

BUF_SIZE = 0o070000 - 0o3200   # буфер загрузчика
WINDOW = 16256                  # предел смещения для dzx0 (zx0.s)


def unzx0_inplace(z, size):
    """распаковка ZX0 v2 в буфере size байт: сжатое — в конце; -> распакованное"""
    zl = len(z) + (len(z) & 1)
    mem = bytearray(size)
    rp = size - zl
    if rp < 0:
        raise ValueError('сжатый файл длиннее буфера')
    mem[rp:rp + len(z)] = z
    st = {'rp': rp, 'wp': 0, 'mask': 0, 'val': 0, 'back': False, 'last': 0}

    def rbyte():
        st['last'] = mem[st['rp']]
        st['rp'] += 1
        return st['last']

    def rbit():
        if st['back']:
            st['back'] = False
            return st['last'] & 1
        st['mask'] >>= 1
        if not st['mask']:
            st['mask'] = 128
            st['val'] = rbyte()
        return 1 if st['val'] & st['mask'] else 0

    def gamma(inv):
        v = 1
        while not rbit():
            v = v << 1 | (rbit() ^ inv)
        return v

    def wbyte(b):
        if st['wp'] >= st['rp']:
            raise ValueError('распаковка на месте догнала сжатое (%d)' % st['wp'])
        mem[st['wp']] = b
        st['wp'] += 1

    def wcopy(off, n):
        for _ in range(n):
            wbyte(mem[st['wp'] - off])

    last = 1
    state = 'lit'
    while True:
        if state == 'lit':
            for _ in range(gamma(0)):
                wbyte(rbyte())
            if rbit():
                state = 'new'
                continue
            wcopy(last, gamma(0))
            if not rbit():
                continue
            state = 'new'
        off = gamma(1)
        if off == 256:
            return bytes(mem[:st['wp']])
        last = off * 128 - (rbyte() >> 1)
        if last > WINDOW:
            raise ValueError('смещение %d дальше окна %d' % (last, WINDOW))
        st['back'] = True
        wcopy(last, gamma(0) + 1)
        state = 'new' if rbit() else 'lit'


def main():
    salvador, src, dst = sys.argv[1:4]
    raw = open(src, 'rb').read()
    if len(raw) > BUF_SIZE:
        sys.exit('zx0lfl: %s длиннее буфера загрузчика' % src)
    tmp = dst + '.tmp'
    subprocess.run([salvador, '-w', str(WINDOW), src, tmp], check=True, stdout=subprocess.DEVNULL)
    z = open(tmp, 'rb').read()
    os.remove(tmp)
    try:
        out = unzx0_inplace(z, BUF_SIZE)
    except ValueError as e:
        sys.exit('zx0lfl: %s: %s' % (src, e))
    if out != raw:
        sys.exit('zx0lfl: %s: распакованное не совпадает с исходным' % src)
    open(dst, 'wb').write(z)
    st = os.stat(src)
    os.utime(dst, (st.st_atime, st.st_mtime))   # дата файла игры — прежняя (mkandos пишет её в каталог)


main()
