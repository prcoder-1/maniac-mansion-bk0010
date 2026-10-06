#!/usr/bin/env python3
"""Сценарий для MCP-сервера эмулятора БК (отладка без перезапуска сессии).

    mcp.py 'bk_reset {"smk":true,"disk":"bin/MANIAC.IMG","frames":800}' 'bk_regs {}' ...

Каждый аргумент — «имя_инструмента JSON-аргументы». Текстовые ответы печатаются,
картинки (bk_screenshot) пишутся в shotN.png в текущий каталог.
"""
import base64, json, os, subprocess, sys

EMU = os.path.expanduser('~/claude-experiments/bk0010-emulator/build/bk0010-emulator')
p = subprocess.Popen([EMU, '--server'], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                     stderr=subprocess.DEVNULL, text=True, env=dict(os.environ, QT_QPA_PLATFORM='offscreen'))


def call(i, method, params):
    p.stdin.write(json.dumps({'jsonrpc': '2.0', 'id': i, 'method': method, 'params': params}) + '\n')
    p.stdin.flush()
    while True:
        r = json.loads(p.stdout.readline())
        if r.get('id') == i:
            return r


call(0, 'initialize', {'protocolVersion': '2024-11-05', 'capabilities': {},
                        'clientInfo': {'name': 'mcp.py', 'version': '1'}})
shot = 0
for k, a in enumerate(sys.argv[1:], 1):
    name, _, js = a.partition(' ')
    args = json.loads(js) if js.strip() else {}
    if 'disk' in args and args['disk']:
        args['disk'] = os.path.abspath(args['disk'])
    if 'path' in args:
        args['path'] = os.path.abspath(args['path'])
    r = call(k, 'tools/call', {'name': name, 'arguments': args})
    print('=== %s' % a)
    for c in r.get('result', {}).get('content', []):
        if c.get('type') == 'text':
            print(c['text'])
        elif c.get('type') == 'image':
            fn = 'shot%d.png' % shot
            shot += 1
            open(fn, 'wb').write(base64.b64decode(c['data']))
            print('[картинка -> %s]' % fn)
    if 'error' in r:
        print(r['error'])
p.stdin.close()
p.wait()
