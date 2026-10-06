#!/usr/bin/env python3
"""Проверка дальности переходов: pdp11-aout-as МОЛЧА обрезает смещение BR/Bxx
(±128 слов) и SOB (0..63 слова назад) — переход уходит не туда.

    chkbr.py листинг.lst   (листинг: pdp11-aout-as -a=листинг.lst)

Метки — именованные и локальные N: (ссылки Nb/Nf); при выходе за диапазон —
сообщение и код 1.
"""
import re, sys

BR = {'br', 'bne', 'beq', 'bpl', 'bmi', 'bvc', 'bvs', 'bcc', 'bcs', 'bge', 'blt',
      'bgt', 'ble', 'bhi', 'blos', 'bhis', 'blo', 'bec', 'bes'}
line_re = re.compile(r'^\s*(\d+)\s+([0-9a-fA-F]{4})?\s*(?:[0-9a-fA-F]{4}\s*)*\t?(.*)$')

rows = []          # (номер строки, адрес, исходник); строка без кода — адрес следующей
for raw in open(sys.argv[1], encoding='utf-8', errors='replace'):
    if raw.startswith('DEFINED SYMBOLS') or raw.startswith('NO DEFINED'):
        break
    m = re.match(r'^\s*(\d+)\s+([0-9a-fA-F]{4})\s', raw)
    src = raw.split('\t', 1)[1] if '\t' in raw else ''
    if m:
        rows.append([int(m.group(1)), int(m.group(2), 16), src])
    elif re.match(r'^\s*(\d+)\s', raw):
        rows.append([int(raw.split()[0]), None, src])
nxt = 0
for r in reversed(rows):
    if r[1] is None:
        r[1] = nxt
    else:
        nxt = r[1]

labels = {}        # имя -> адрес
local = []         # (индекс строки, N, адрес)
for i, (_, a, src) in enumerate(rows):
    s = src.split('/')[0]
    while True:
        m = re.match(r'\s*([A-Za-z_.$][\w.$]*|\d+)\s*:', s)
        if not m:
            break
        if m.group(1).isdigit():
            local.append((i, m.group(1), a))
        else:
            labels[m.group(1)] = a
        s = s[m.end():]

bad = 0
for i, (n, a, src) in enumerate(rows):
    s = re.sub(r'^\s*(?:([A-Za-z_.$][\w.$]*|\d+)\s*:\s*)*', '', src.split('/')[0]).strip()
    m = re.match(r'(\w+)\s+(.*)$', s)
    if not m:
        continue
    op, args = m.group(1).lower(), m.group(2).strip()
    if op not in BR and op != 'sob':
        continue
    tgt = args.split(',')[-1].strip()
    t = None
    lm = re.match(r'^(\d+)([bf])$', tgt)
    if lm:
        cands = [x for x in local if x[1] == lm.group(1)]
        if lm.group(2) == 'b':
            cands = [x for x in cands if x[0] <= i]
            t = cands[-1][2] if cands else None
        else:
            cands = [x for x in cands if x[0] > i]
            t = cands[0][2] if cands else None
    elif tgt in labels:
        t = labels[tgt]
    elif tgt == '.':
        t = a
    if t is None:
        continue
    d = (t - (a + 2)) // 2
    ok = (-128 <= d <= 127) if op != 'sob' else (-63 <= d <= 0)
    if not ok:
        print('%s:%d: %s %s — смещение %d слов вне диапазона' % (sys.argv[1], n, op, tgt, d))
        bad = 1
sys.exit(bad)
