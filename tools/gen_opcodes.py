#!/usr/bin/env python3
"""Таблица опкодов v2 из ScummVM: engines/scumm/script_v2.cpp, setupOpcodes().

    gen_opcodes.py script_v2.cpp > opcodes_v2.h

Пишет перечисление обработчиков OP_<имя>, таблицу opcode -> обработчик и (для
хостовой трассы, под HOST) имена обработчиков, как их печатает ScummVM.
"""
import re, sys

src = open(sys.argv[1]).read()
body = src[src.index('void ScummEngine_v2::setupOpcodes()'):]
body = body[:body.index('\n}\n')]
ops = re.findall(r'OPCODE\((0x[0-9a-fA-F]+),\s*(\w+)\);', body)
table = {}
names = []
for code, fn in ops:
    table[int(code, 16)] = fn
    if fn not in names:
        names.append(fn)
assert len(table) == 256, len(table)
out = ['/* Сгенерировано tools/gen_opcodes.py из ScummVM script_v2.cpp (setupOpcodes) — не править */',
       '#pragma once', '#include <stdint.h>', '', 'enum', '{']
for n in names:
    out.append('    OP_%s,' % n)
out += ['};', '', 'static const uint8_t opcode_handler[256] =', '{']
for i in range(0, 256, 4):
    out.append('    ' + ' '.join('OP_%s,' % table[i + k] for k in range(4)) + '  /* %02X */' % i)
out += ['};', '', '#ifdef HOST', 'static const char *const opcode_name[] =', '{']
for n in names:
    out.append('    "%s",' % n)
out += ['};', '#endif', '']
print('\n'.join(out))
