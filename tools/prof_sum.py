#!/usr/bin/env python3
"""Сводка профиля bk_profile (folded stacks): доля простоя и самые дорогие функции.

    prof_sum.py ФАЙЛ.folded [N]

Простой — стеки с ожиданием шага в главном цикле (snd_spin / wait_frame).
"""
import collections, sys

IDLE = ('snd_spin', 'wait_frame')

path = sys.argv[1]
top = int(sys.argv[2]) if len(sys.argv) > 2 else 15
self_w, incl_w = collections.Counter(), collections.Counter()
total = idle = 0
for line in open(path):
    stack, _, w = line.rstrip().rpartition(' ')
    if not w.isdigit():
        continue
    w = int(w)
    frames = stack.split(';')
    total += w
    if any(f.split('+')[0] in IDLE for f in frames):
        idle += w
    self_w[frames[-1]] += w
    for f in set(frames):
        incl_w[f] += w
print('всего %d тактов, простой %.1f%%, работа %.1f%%' % (total, 100 * idle / total, 100 - 100 * idle / total))
print('собственное время:')
for f, w in self_w.most_common(top):
    print('  %5.1f%%  %s' % (100 * w / total, f))
