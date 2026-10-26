#!/usr/bin/env python3
"""Эталон Player_V2 (ScummVM players/player_v2base.cpp) для проверки драйвера AY.

    v2snd.py N [КАДРЫ] [--spk]

Звук N из файлов LFL: такты секвенсора 236 Гц (PCjr или PC-спикер), по кадрам БК-0011М
(48.828125 Гц, 15104/3125 тактов) — регистры AY, как их считает snddrv.c drv_out:
строки «кадр: R0..R10» (только кадры, где звук идёт).
"""
import os, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = '/home/prcoder/scummvm/engines/scumm/players/player_v2base.cpp'


def c_array(name):
    t = open(SRC).read()
    a = t.index(name)
    body = t[t.index('{', a) + 1:t.index('};', a)]
    out = []
    for line in body.split('\n'):
        line = line.split('//')[0]
        for x in line.split(','):
            x = x.strip()
            if x:
                cast = x.startswith('(int16)')
                v = int(x.replace('(int16)', '').replace(' ', ''), 0)
                out.append(v - 0x10000 if cast and v >= 0x8000 else v)
    return out


note_lengths = c_array('note_lengths[]')
hull_offsets = c_array('hull_offsets[]')
hulls = c_array('static const int16 hulls[]')
freqmod_lengths = c_array('freqmod_lengths[]')
freqmod_offsets = c_array('freqmod_offsets[]')
freqmod_table = c_array('freqmod_table[0x502]')
spk_freq = c_array('spk_freq_table[12]')
pcjr_freq = c_array('pcjr_freq_table[12]')
ATT = [15, 14, 13, 12, 12, 11, 10, 9, 8, 8, 7, 6, 6, 5, 5, 0]
HEADER = 4
W = 25
(TL, NC, BF, FD, FR, VOL, VD, TEMPO, INP, TR, NL, HC, HO, HCNT, FMT, FMO, FMI, FMM, FMMOD) = range(19)
MT = 23
U16 = 0xFFFF


def rd_lfl(n):
    return bytes(b ^ 0xFF for b in open(os.path.join(ROOT, '%02d.LFL' % n), 'rb').read())


def sound_data(nr):
    ix = rd_lfl(0)
    p = 2
    nobj = struct.unpack_from('<H', ix, p)[0]
    p += 2 + nobj
    nrooms = ix[p]
    p += 1 + nrooms * 3
    lists = []
    for _ in range(3):
        n = ix[p]
        p += 1
        rooms = ix[p:p + n]
        p += n
        offs = struct.unpack_from('<%dH' % n, ix, p)
        p += 2 * n
        lists.append((rooms, offs))
    rooms, offs = lists[2]
    f = rd_lfl(rooms[nr])
    o = offs[nr]
    size = struct.unpack_from('<H', f, o)[0]
    return f[o:o + size]


class Player:
    def __init__(self, data, pcjr):
        self.d, self.pcjr = data, pcjr
        self.ch = [[0] * W for _ in range(5)]
        self.cur = True
        self.ret = 0
        off = HEADER + (10 if pcjr else 2)
        for i in range(4):
            self.ch[i][NC] = self.w(off + 2 * i)
            if self.ch[i][NC]:
                self.ch[i][TL] = 1

    def w(self, o):
        return self.d[o] | self.d[o + 1] << 8

    def execute_cmd(self, c):
        cur = c
        if c[NC] == 0:
            return self.check_stopped()
        sp = c[NC]
        d = self.d
        while True:
            op = d[sp]; sp += 1
            if op >= 0xf8:
                if op == 0xf8:
                    c[HC] = hull_offsets[d[sp] // 2]; sp += 1
                elif op == 0xf9:
                    c[FMT] = freqmod_offsets[d[sp] // 4]; c[FMMOD] = freqmod_lengths[d[sp] // 4]; sp += 1
                elif op in (0xfa, 0xfd):
                    if op == 0xfd:
                        v = self.w(sp) // (W * 2); sp += 2
                        c = self.ch[min(v, 4)]
                    for k in (NC, BF, FD, FR, VOL, VD, INP, TR, HC, HO, HCNT, FMT, FMO, FMI, FMM, FMMOD):
                        c[k] = 0
                elif op == 0xfb:
                    sp = self.ret
                elif op == 0xfc:
                    o = self.w(sp); sp += 2
                    self.ret = sp
                    sp = o
                elif op == 0xfe:
                    k = d[sp]; sp += 1
                    o = self.w(sp); sp += 2
                    if not c[k // 2]:
                        sp = (sp + o) & U16
                    else:
                        c[k // 2] = (c[k // 2] - 1) & U16
                        if c[k // 2]:
                            sp = (sp + o) & U16
                elif op == 0xff:
                    k = d[sp]; sp += 1
                    v = self.w(sp); sp += 2
                    if k // 2 < W:
                        c[k // 2] = v
                    if k == 0:
                        break
            else:
                while True:
                    dc = self.ch[(op >> 5) & 3]
                    if not op & 0x80:
                        c[TL] = (max(c[TEMPO], 1) * note_lengths[op & 0x1f]) & U16
                        note = d[sp]; sp += 1
                        last = note & 0x80
                        note &= 0x7f
                        if note == 0x7f:
                            break
                    else:
                        c[TL] = ((op & 7) << 8) | d[sp]; sp += 1
                        if op & 0x10:
                            break
                        last = 0
                        note = d[sp] & 0x7f; sp += 1
                    dc[TL] = c[TL]
                    dc[NL] = (c[TL] - dc[INP]) & U16
                    note += dc[TR] if dc[TR] < 0x8000 else dc[TR] - 0x10000
                    while note < 0:
                        note += 12
                    octave, note = note // 12, note % 12
                    dc[HO] = 0
                    dc[HCNT] = 1
                    if self.pcjr and dc is self.ch[3]:
                        dc[HC] = 196 + note * 12
                        f = (384 - 64 * octave) & U16
                    else:
                        f = (pcjr_freq if self.pcjr else spk_freq)[note] >> octave
                    dc[FR] = dc[BF] = f
                    if last:
                        break
                    op = d[sp]; sp += 1
                else:
                    continue
                break
        c = cur
        if c[TL]:
            c[NC] = sp
            return
        c[NC] = 0
        self.check_stopped()

    def check_stopped(self):
        if not any(self.ch[i][TL] for i in range(4)):
            self.cur = False

    def next_freqs(self, c):
        c[VOL] = (c[VOL] + c[VD]) & U16
        c[BF] = (c[BF] + c[FD]) & U16
        c[FMO] = (c[FMO] + c[FMI]) & U16
        if c[FMO] > c[FMMOD]:
            c[FMO] = (c[FMO] - c[FMMOD]) & U16
        t = freqmod_table[c[FMT] + (c[FMO] >> 4)]
        m = c[FMM]
        c[FR] = (int(t * m / 256) + c[BF]) & U16   # int: усечение к нулю
        if c[NL]:
            c[NL] = (c[NL] - 1) & U16
            if not c[NL]:
                c[HO] = 16
                c[HCNT] = 1
        c[TL] = (c[TL] - 1) & U16
        if not c[TL]:
            self.execute_cmd(c)
        if c[HCNT]:
            c[HCNT] = (c[HCNT] - 1) & U16
            if not c[HCNT]:
                while True:
                    h = c[HC] + c[HO] // 2
                    if hulls[h + 1] == -1:
                        c[VOL] = hulls[h] & U16
                        if hulls[h] == 0:
                            c[VD] = 0
                        c[HO] += 4
                    else:
                        c[VD] = hulls[h] & U16
                        c[HCNT] = hulls[h + 1] & U16
                        c[HO] += 4
                        break

    def tick(self):
        for i in range(4):
            if self.ch[i][TL]:
                self.next_freqs(self.ch[i])


_K = 1714286 / (16 * 111860.0)
_TL = [min(255, int(l * _K + 0.5)) for l in range(256)]
_TH = [int(h * 256 * _K + 0.5) for h in range(4)]


def pj_tp(n):
    """как drvhdr.s pjtp: N * 0.957826 по таблицам байтов N"""
    n = min(n, 1023)
    return _TL[n & 255] + _TH[n >> 8]


def spk_tp(d):
    q, r = d >> 8, d & 255
    tp = (q << 4) + (q << 3) - q + ((((r << 4) + (r << 3) - r) + 128) >> 8)
    return min(tp, 0o7777)


def level(c):
    if not c[VOL] or not c[TL]:
        return 0
    return ATT[((0xFFFF - c[VOL]) & U16) >> 12]


def out(p):
    """регистры R0..R10 (как drv_out): тон A,B,C; шум; смеситель; громкости"""
    tp, lv, mix, np = [0, 0, 0], [0, 0, 0], 0o77, 0
    ch = p.ch
    if p.cur and p.pcjr:
        for i in range(3):
            lv[i] = level(ch[i])
            if lv[i]:
                tp[i] = pj_tp(ch[i][FR] >> 6)
                mix &= ~(1 << i)
        ln = level(ch[3])
        if ln:
            f = ch[3][FR] >> 6
            n = f & 3
            n2 = ch[2][FR] >> 6
            j = 0
            while j < 3 and lv[j]:
                j += 1
            if f & 4:
                np = 15 if n == 0 else 31 if n < 3 else (31 if pj_tp(n2) > 15 else pj_tp(n2) << 1)
                if j == 3:
                    j = min(range(3), key=lambda i: lv[i])
                    lv[j] = max(lv[j], ln)
                else:
                    lv[j] = ln
                mix &= ~(0o10 << j)
            elif j < 3:
                tp[j] = min(pj_tp(256 << n) if n < 3 else pj_tp(n2) << 4, 0o7777)
                lv[j] = ln
                mix &= ~(1 << j)
    elif p.cur:
        for i in range(4):
            if ch[i][VOL] and ch[i][TL]:
                tp[1] = spk_tp(ch[i][FR] or 1)
                lv[1] = 13
                mix = 0o75
                break
    return tp, np, mix, lv


def main():
    nr = int(sys.argv[1])
    frames = int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2].isdigit() else 200
    p = Player(sound_data(nr), '--spk' not in sys.argv)
    acc = 0
    for fr in range(frames):
        acc += 15104
        while acc >= 3125:
            acc -= 3125
            if p.cur:
                p.tick()
        if not p.cur:
            print('%d: конец' % fr)
            break
        tp, np, mix, lv = out(p)
        print('%d: A %d B %d C %d N %d M %03o V %d %d %d' % (fr, tp[0], tp[1], tp[2], np, mix, lv[0], lv[1], lv[2]))


if __name__ == '__main__':
    main()
