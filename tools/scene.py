#!/usr/bin/env python3
"""Сцены игры в эмуляторе (БК-0011М + СМК-512 + AY) для замеров и снимков.

    scene.py КОМАНДА [КОМАНДА ...]

Команды выполняются по порядку в одном запуске эмулятора (--server):
    boot            сброс, ANDOS, запуск MANIAC до экрана выбора героев
    load ФАЙЛ       состояние эмулятора из файла (save ФАЙЛ — записать)
    kids            выбрать двух героев, START, пропустить заставки (КТ)
    room N          перейти в комнату N (трамплин на startScene из scummLoop)
    run N           прогнать N кадров
    steps N         шаги цикла (loop_count) за N кадров -> шагов/с
    key КОД         нажать клавишу (восьмеричный код, как в bk_key)
    click X Y       щелчок мышью в точке экрана MM (320x200)
    shot ФАЙЛ       снимок экрана
    prof ФАЙЛ N     профиль (folded stacks) за N кадров
    mem АДРЕС|СИМВОЛ [ДЛИНА]
    tool ИМЯ 'JSON'  любой инструмент эмулятора (bk_io_log, bk_io_state, ...)
Адреса переменных — из kernel.map/REPL.map текущей сборки.
"""
import base64, json, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EMU = os.path.expanduser('~/claude-experiments/bk0010-emulator/build/bk0010-emulator')
FPS = 3125 / 64   # кадровая частота БК-0011М
KIDS_ROOM = 45    # экран выбора героев


def image():
    m = re.search(r'^VERSION=(\S+)', open(os.path.join(ROOT, 'Makefile')).read(), re.M)
    return os.path.join(ROOT, 'bin', 'MANIAC_%s.IMG' % m.group(1))


def syms():
    s = {}
    for f in ('kernel.out', 'REPL.out'):
        out = subprocess.run(['pdp11-aout-nm', os.path.join(ROOT, f)], capture_output=True, text=True).stdout
        for line in out.split('\n'):
            p = line.split()
            if len(p) == 3:
                s.setdefault(p[2], int(p[0], 16))
    return s


class Emu:
    def __init__(self):
        self.p = subprocess.Popen([EMU, '--server'], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.DEVNULL, text=True,
                                  env=dict(os.environ, QT_QPA_PLATFORM='offscreen'))
        self.n = 0
        self.rpc('initialize', {'protocolVersion': '2024-11-05', 'capabilities': {},
                                'clientInfo': {'name': 'scene.py', 'version': '1'}})
        self.s = syms()
        self.load_syms()

    def rpc(self, method, params):
        self.n += 1
        self.p.stdin.write(json.dumps({'jsonrpc': '2.0', 'id': self.n, 'method': method, 'params': params}) + '\n')
        self.p.stdin.flush()
        while True:
            r = json.loads(self.p.stdout.readline())
            if r.get('id') == self.n:
                if 'error' in r:
                    raise RuntimeError(r['error'])
                return r

    def tool(self, tool_name, **args):
        r = self.rpc('tools/call', {'name': tool_name, 'arguments': args})
        return r.get('result', {}).get('content', [])

    def text(self, tool_name, **args):
        return '\n'.join(c['text'] for c in self.tool(tool_name, **args) if c.get('type') == 'text')

    def load_syms(self):
        # bk_symbols заменяет таблицу: ядро и REPL — одним файлом
        path = os.path.join(ROOT, 'test', 'out', 'syms.map')
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            for m in ('kernel.map', 'REPL.map'):
                f.write(open(os.path.join(ROOT, m)).read())
        self.text('bk_symbols', path=path)

    def addr(self, a):
        return self.s['_' + a] if ('_' + a) in self.s else int(a, 8)

    def word(self, a):
        t = self.text('bk_read_mem', addr='0%o' % a, len=2)
        return int(t.split(':')[1].split()[0], 8)

    def byte(self, a):
        t = self.text('bk_read_mem', addr='0%o' % a, len=1, format='bytes')
        return int(t.split(':')[1].split()[0], 8)

    def wr(self, a, words):
        self.text('bk_write_mem', addr='0%o' % a, words=words)

    def run(self, n):
        return self.text('bk_run', max_frames=n)

    def key(self, code, hold=3, after=10):
        # нажатие расписанием ввода bk_run (bk_key ведёт себя иначе: ANDOS перезагружалась)
        self.text('bk_run', max_frames=hold + after, input=[
            {'frame': 0, 'code': code}, {'frame': hold, 'release_key': True}])

    def wait_room(self, room, limit=6000, step=100):
        a = self.s['_currentRoom']
        for _ in range(0, limit, step):
            if self.byte(a) == room:
                return True
            self.run(step)
        return False

    def boot(self):
        self.text('bk_reset', machine=os.environ.get('BK_MACHINE', 'bk11m'), smk=True, ay=True, ay2=False, disk=image(), frames=3000)
        self.key('012', 5, 400)       # заставка ANDOS -> панель
        # курсор на MANIAC и ВВОД — одним прогоном (через bk_key ANDOS уходит в перезагрузку)
        self.text('bk_run', max_frames=100, input=[
            {'frame': 0, 'code': '033'}, {'frame': 4, 'release_key': True},
            {'frame': 15, 'code': '012'}, {'frame': 19, 'release_key': True}])
        if not self.wait_room(KIDS_ROOM, 8000):
            print('boot: нет экрана выбора героев')
        self.run(1500)                # титры; затем «Please select two other kids»

    def click(self, x, y):
        self.wr(self.s['_mouse_x'], [x])
        self.wr(self.s['_mouse_y'], [y])
        self.run(10)
        self.key('012', 3, 60)

    def kids(self):
        self.click(78, 124)
        self.click(118, 124)
        self.click(290, 75)
        for _ in range(4):            # заставки: КТ
            self.run(400)
            self.key('003', 3, 10)

    def room(self, n):
        stub, scene = self.s['_scummLoop'], self.s['_startScene']
        self.text('bk_break', addr='0%o' % stub)
        self.run(200)
        self.text('bk_unbreak', addr='0%o' % stub)
        t = 0o035400   # свободное базовое ОЗУ (sys.ld 035100..036177)
        code = [0o005046, 0o005046, 0o012746, n, 0o004737, scene, 0o062706, 6, 0o000137, stub]
        self.wr(t, code)
        self.text('bk_set_reg', name='PC', value='0%o' % t)
        self.wait_room(n, 3000, 50)

    def steps(self, n):
        a = self.s['_loop_count']
        c0 = self.word(a)
        self.run(n)
        c1 = self.word(a)
        d = (c1 - c0) & 0xFFFF
        print('steps: %d за %d кадров = %.2f шагов/с' % (d, n, d * FPS / n))

    def shot(self, path):
        for c in self.tool('bk_screenshot'):
            if c.get('type') == 'image':
                open(path, 'wb').write(base64.b64decode(c['data']))
                print('shot -> %s' % path)

    def close(self):
        self.p.stdin.close()
        self.p.wait()


def main(argv):
    e = Emu()
    i = 0
    while i < len(argv):
        c = argv[i]
        a = argv[i + 1:]
        if c == 'boot':
            e.boot()
        elif c == 'load':   # состояние не хранит дискету: сначала вставить образ
            e.text('bk_reset', machine=os.environ.get('BK_MACHINE', 'bk11m'), smk=True, ay=True, ay2=False,
                   disk=image(), frames=0)
            print(e.text('bk_state_load', path=os.path.abspath(a[0]))); i += 1
        elif c == 'save':
            print(e.text('bk_state_save', path=os.path.abspath(a[0]))); i += 1
        elif c == 'kids':
            e.kids()
        elif c == 'room':
            e.room(int(a[0])); i += 1
        elif c == 'run':
            e.run(int(a[0])); i += 1
        elif c == 'steps':
            e.steps(int(a[0])); i += 1
        elif c == 'key':
            e.key(a[0]); i += 1
        elif c == 'click':
            e.click(int(a[0]), int(a[1])); i += 2
        elif c == 'shot':
            e.shot(a[0]); i += 1
        elif c == 'prof':
            print(e.text('bk_profile', path=os.path.abspath(a[0]), frames=int(a[1]))); i += 2
        elif c == 'tool':   # tool ИМЯ 'JSON'
            print(e.text(a[0], **json.loads(a[1]))); i += 2
        elif c == 'mem':
            n = int(a[1]) if len(a) > 1 and a[1].isdigit() else 16
            print(e.text('bk_read_mem', addr='0%o' % e.addr(a[0]), len=n))
            i += 2 if len(a) > 1 and a[1].isdigit() else 1
        else:
            sys.exit('неизвестная команда %s' % c)
        i += 1
    e.close()


if __name__ == '__main__':
    main(sys.argv[1:])
