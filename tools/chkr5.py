#!/usr/bin/env python3
"""Проверка объектника: функция пишет r5, не сохранив его (mov r5, -(sp)).

gcc pdp11 с -fomit-frame-pointer, когда функции всё же нужен указатель кадра
(mov sp, r5), не сохраняет r5 — у вызывающего пропадает свой (TOOLCHAIN-BUGS.md).
Такой функции нужен FP_SAFE (scumm.h).

    chkr5.py файл.o
"""
import re
import subprocess
import sys

obj = sys.argv[1]
out = subprocess.run(['pdp11-aout-objdump', '-d', obj], capture_output=True, text=True).stdout
bad = []
for f in re.split(r'\n(?=[0-9a-f]+ <[^>]+>:)', out):
    m = re.match(r'([0-9a-f]+) <([^>]+)>:', f)
    if not m or not m.group(2).startswith('_'):
        continue
    saved = False
    for line in f.split('\n')[1:]:
        if line.count('\t') < 2:
            continue
        ins = ' '.join(line.split('\t')[2:]).strip()
        if re.match(r'mov\s+r5,\s*-\(sp\)', ins):
            saved = True
        if re.search(r',\s*r5$', ins) and not ins.startswith(('cmp', 'bit', 'tst')) and not saved:
            bad.append('%s: %s портит r5 (%s) — нужен FP_SAFE' % (obj, m.group(2)[1:], ins))
            break
if bad:
    sys.exit('\n'.join(bad))
