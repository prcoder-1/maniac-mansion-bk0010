#!/usr/bin/env python3
"""Указатели в дальних вызовах: адрес из окна банка 0100000..0157777 (данные, строки,
функции банка), переданный аргументом в функцию другого банка, — ошибка: на время
вызова в окне чужая страница (общие данные — в стеке или базовом ОЗУ).

    far_ptr.py BANK.out far.def ИМЯ_БАНКА REPL.out

Эвристика по дизассемблеру слинкованного банка: до каждого вызова — что лежит в
регистрах (адрес окна и арифметика от него) и что кладётся в стек. Указатель,
сохранённый в структуре или пришедший через память, не виден.
"""
import re, subprocess, sys

out, fdef, me, repl = sys.argv[1:5]
other, bank = set(), None
for line in open(fdef):
    w = line.split('#')[0].split()
    if not w:
        continue
    if w[0] == 'bank':
        bank = w[1]
    elif bank != me:
        other.add('_' + w[0])
stubs = {}
for line in subprocess.run(['pdp11-aout-nm', repl], capture_output=True, text=True).stdout.split('\n'):
    p = line.split()
    if len(p) == 3 and p[2] in other:
        stubs[int(p[0], 16)] = p[2]

WIN = range(0o100000, 0o160000)
REGS = ('r0', 'r1', 'r2', 'r3', 'r4', 'r5')
ins = re.compile(r'^\s*([0-9a-f]+):\t[0-9a-f ]+\t(\S+)\s*(.*)$')
fn = re.compile(r'^[0-9a-f]+ <(\S+)>:$')


def imm(op):
    m = re.fullmatch(r'\$(-?[0-7]+)', op)
    return int(m.group(1), 8) & 0xffff if m else None


bad = []
dis = subprocess.run(['pdp11-aout-objdump', '-d', out], capture_output=True, text=True).stdout
func = '?'
taint, val, args, pend = {}, {}, [], {}
for line in dis.split('\n'):
    m = fn.match(line)
    if m:
        func, taint, val, args, pend = m.group(1), {}, {}, [], {}
        continue
    m = ins.match(line)
    if not m:
        continue
    addr, op, ops = int(m.group(1), 16), m.group(2), [o.strip() for o in m.group(3).split(', ')] if m.group(3) else []
    args += pend.pop(addr, [])
    if op in ('br', 'jmp', 'rts'):   # что положено в стек до перехода — аргументы вызова там
        t = re.match(r'([0-9a-f]+) <', ops[0]) if op == 'br' else None
        if t and int(t.group(1), 16) > addr:
            pend.setdefault(int(t.group(1), 16), []).extend(args)
        args = []
        continue
    if op == 'jsr':
        t = ops[1].split()[0]
        if t.startswith('(') and t[1:-1] in REGS:
            dest = val.get(t[1:-1])
        elif t.startswith('@$'):
            dest = imm(t[1:])
        else:
            dest = int(t, 16) if re.fullmatch(r'[0-9a-f]+', t) else None
        if dest in stubs and any(args):
            bad.append('%s (%06o): %s' % (func, addr, stubs[dest][1:]))
        args = []
        for r in ('r0', 'r1'):
            taint.pop(r, None)
            val.pop(r, None)
        continue
    if len(ops) != 2:
        if len(ops) == 1 and ops[0] in REGS and op == 'clr':
            taint.pop(ops[0], None)
            val.pop(ops[0], None)
        continue
    src, dst = ops
    v = imm(src)
    t = op != 'movb' and ((v in WIN) if v is not None else taint.get(src, False))   # байт — не адрес
    if op in ('mov', 'movb'):
        if dst == '-(sp)':
            args.append(t)
        elif dst in REGS:
            taint[dst] = t
            if v is not None and op == 'mov':
                val[dst] = v
            else:
                val.pop(dst, None)
    elif dst in REGS:
        if op in ('add', 'sub'):
            taint[dst] = taint.get(dst, False) or t
        val.pop(dst, None)
if bad:
    sys.exit('far_ptr: %s: адрес окна банка в дальний вызов: %s' % (out, '; '.join(bad)))
