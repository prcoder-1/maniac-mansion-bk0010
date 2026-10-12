#!/usr/bin/env python3
"""far.def по объектникам: функции, вызываемые из чужого банка или из ядра.

    far_auto.py far.def --bank A 2 a.o ... --bank B 3 ... --kernel k.o ...

Ссылка на данные (не код) другого банка — ошибка: общие данные живут в базовом ОЗУ.
Дальняя функция с 32-битным результатом — ошибка: farret (farcall.s) портит r1.
"""
import os, re, subprocess, sys

out = sys.argv[1]
banks, kernel, cur = [], [], None
for a in sys.argv[2:]:
    if a == '--bank':
        cur = 'bank'
        banks.append([None, None, []])
    elif a == '--kernel':
        cur = 'kernel'
    elif cur == 'bank' and banks[-1][0] is None:
        banks[-1][0] = a
    elif cur == 'bank' and banks[-1][1] is None:
        banks[-1][1] = a
    elif cur == 'bank':
        banks[-1][2].append(a)
    else:
        kernel.append(a)


def syms(objs):
    defs, undef = {}, set()
    for o in objs:
        for line in subprocess.run(['pdp11-aout-nm', o], capture_output=True, text=True).stdout.split('\n'):
            p = line.split()
            if len(p) == 3 and p[1] in 'TDBC':
                defs[p[2]] = p[1]
            elif len(p) == 2 and p[0] == 'U':
                undef.add(p[1])
    return defs, undef


info = [(name, page, *syms(objs)) for name, page, objs in banks]
kdefs, kundef = syms(kernel)
exports = {name: set() for name, _, _, _ in info}
bad = []
for i, (name, page, defs, undef) in enumerate(info + [('K', None, kdefs, kundef)]):
    for s in undef:
        if s in defs or s in kdefs and name != 'K':
            continue
        for oname, _, odefs, _ in info:
            if oname != name and s in odefs:
                if odefs[s] == 'T':
                    exports[oname].add(s)
                else:
                    bad.append('%s: данные %s банка %s' % (name, s, oname))
wide = re.compile(r'^(?:static\s+)?(?:inline\s+)?(?:u?int32_t|u?int64_t|(?:unsigned\s+)?long)\s+(\w+)\s*\(', re.M)
for name, _, objs in banks:
    for o in objs:
        c = o[:-2] + '.c'
        if os.path.exists(c):
            for f in wide.findall(open(c).read()):
                if '_' + f in exports[name]:
                    bad.append('%s: 32-битный результат дальней функции %s' % (name, f))
if bad:
    sys.exit('far_auto: ' + '; '.join(sorted(bad)))
with open(out, 'w') as f:
    f.write('# Сгенерировано tools/far_auto.py: функции банков, вызываемые снаружи\n')
    for name, page, _, _ in info:
        f.write('bank %s %s\n' % (name, page))
        for s in sorted(exports[name]):
            f.write(s[1:] + '\n')
print('far.def: ' + ', '.join('%s %d' % (n, len(exports[n])) for n, _, _, _ in info))
