#!/usr/bin/env python3
"""Межбанковые вызовы из far.def.

    gen_far.py far.def OUTDIR

far.def: строки «bank ИМЯ СТРАНИЦА», за ними имена функций банка (# — комментарий).
Пишет:
  far_stubs.s   — заглушки всех функций (в общую копию кода, 0160000);
  tab_ИМЯ.s     — таблица переходов банка (первый объект банка, 0100000);
  far_ИМЯ.strip — ключи objcopy, убирающие из символов общей копии заглушки
                  собственных функций банка (при сборке банка они его же).
"""
import os, sys

src, out = sys.argv[1], sys.argv[2]
banks = []
for line in open(src):
    line = line.split('#')[0].split()
    if not line:
        continue
    if line[0] == 'bank':
        banks.append((line[1], int(line[2], 0), []))
    else:
        banks[-1][2].append(line[0])

stubs = ['/ Сгенерировано tools/gen_far.py из far.def — не править', '\t.text']
for name, page, fns in banks:
    tab = ['/ Сгенерировано tools/gen_far.py: таблица переходов банка %s' % name, '\t.text']
    for i, f in enumerate(fns):
        stubs += ['\t.globl _%s' % f, '_%s:\tmov\t$0%o, r1' % (f, (page << 8) | (i * 2)), '\tjmp\tfarcall']
        tab.append('\t.word\t_%s' % f)
    open(os.path.join(out, 'tab_%s.s' % name), 'w').write('\n'.join(tab) + '\n')
    open(os.path.join(out, 'far_%s.strip' % name), 'w').write(
        ' '.join('--strip-symbol=_%s' % f for f in fns) + '\n')
stubs.append('\t.globl farcall')
open(os.path.join(out, 'far_stubs.s'), 'w').write('\n'.join(stubs) + '\n')
