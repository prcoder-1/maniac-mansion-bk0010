#!/usr/bin/env python3
"""Инструкции звукового драйвера по функциям из вывода bk_hotspots.

    drvcount.py HOT.txt КАДРЫ
"""
import collections, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
syms = []
for l in subprocess.run(['pdp11-aout-nm', '-n', os.path.join(ROOT, 'SNDDRV.out')],
                        capture_output=True, text=True).stdout.split('\n'):
    p = l.split()
    if len(p) == 3 and p[1] in 'tT' and 0o40000 <= int(p[0], 16) < 0o60000:
        syms.append((int(p[0], 16), p[2]))
syms.sort()
frames = int(sys.argv[2])
c, tot = collections.Counter(), 0
for line in open(sys.argv[1]):
    m = re.match(r'\s*>?([0-7]{6})\s.*x(\d+)\s*$', line)
    if not m:
        continue
    a, n = int(m.group(1), 8), int(m.group(2))
    tot += n
    if 0o40000 <= a < 0o60000:
        c[max((s for s in syms if s[0] <= a), default=(0, '?'))[1]] += n
drv = sum(c.values())
print('всего %d инструкций, драйвер %d (%.1f%%), %.0f на кадр' % (tot, drv, 100 * drv / tot, drv / frames))
for k, v in c.most_common():
    print('%6.0f/кадр %s' % (v / frames, k))
