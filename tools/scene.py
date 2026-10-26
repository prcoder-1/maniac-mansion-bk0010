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

    def trampoline(self, code):
        """код перед очередным шагом цикла: остановка на заглушке scummLoop, код в
        свободном базовом ОЗУ, затем — в scummLoop с тем же стеком"""
        stub = self.s['_scummLoop']
        self.text('bk_break', addr='0%o' % stub)
        self.run(200)
        self.text('bk_unbreak', addr='0%o' % stub)
        t = 0o035400   # свободное базовое ОЗУ (sys.ld 035100..036177)
        self.wr(t, code + [0o000137, stub])
        self.text('bk_set_reg', name='PC', value='0%o' % t)

    def sound(self, n):
        # stopAllSounds(); addSoundToQueue(n) — звук стартует в конце шага (sound_process)
        self.trampoline([0o004737, self.s['_stopAllSounds'],
                         0o012746, n, 0o004737, self.s['_addSoundToQueue'], 0o005726])

    def ay_log(self, frames):
        """записи в AY за frames кадров -> состояния R0..R10 по кадрам"""
        self.wr(self.s['_ay_dirty'], [1])   # драйвер перепишет все регистры
        self.text('bk_io_log', enable=True, addr='0177714', cap=65536)
        self.run(frames)
        t = self.text('bk_io_log', count=65536)
        self.text('bk_io_log', enable=False)
        regs, sel, out, last, line0 = [0] * 16, 0, [], None, -1
        for line in t.split('\n'):
            m = re.search(r'W(B?)\s+177714 =\s+(\d+)\s.*стр\s+(\d+):', line)
            if not m:
                continue
            ln = int(m.group(3))
            if ln < line0 or ln > line0 + 40:   # новая пачка (кадровое прерывание)
                st = tuple(regs[:11])
                if line0 >= 0 and st != last:
                    out.append(st)
                    last = st
            line0 = ln
            v = int(m.group(2), 8)
            if m.group(1) == 'B':
                regs[sel] = (~v) & 0o377
            elif v != 0:
                sel = (~v) & 0o17
        st = tuple(regs[:11])
        if st != last:
            out.append(st)
        return out

    def idle(self):
        a = self.s['_idle_t']   # uint32 gcc pdp11: старшее слово первым
        return (self.word(a) << 16 | self.word(a + 2))

    def steps(self, n):
        a = self.s['_loop_count']
        c0, i0 = self.word(a), self.idle()
        self.run(n)
        c1, i1 = self.word(a), self.idle()
        d = (c1 - c0) & 0xFFFF
        idle = ((i1 - i0) & 0xFFFFFFFF) / (n * 640.0)
        print('steps: %d за %d кадров = %.2f шагов/с, работа %.1f%%' % (d, n, d * FPS / n, 100 * (1 - idle)))

    def shot(self, path):
        for c in self.tool('bk_screenshot'):
            if c.get('type') == 'image':
                open(path, 'wb').write(base64.b64decode(c['data']))
                print('shot -> %s' % path)

    def close(self):
        self.p.stdin.close()
        self.p.wait()


def drv_syms():
    s = {}
    out = subprocess.run(['pdp11-aout-nm', os.path.join(ROOT, 'SNDDRV.out')], capture_output=True, text=True).stdout
    for line in out.split('\n'):
        p = line.split()
        if len(p) == 3:
            s[p[2]] = int(p[0], 16)
    return s


def words(text):
    w = []
    for line in text.split('\n'):
        if ':' in line:
            w += [int(x, 8) for x in line.split(':', 1)[1].split()]
    return w


def bytes_of(text):
    b = []
    for line in text.split('\n'):
        if ':' in line:
            b += [int(x, 8) for x in line.split(':', 1)[1].split()]
    return b


def out_of(ch, nr):
    """регистры AY по каналам драйвера (FR + модуляция FM_T * FM_M / 256) — v2snd.out"""
    import v2snd

    class P:
        pass
    p = P()
    p.cur, p.pcjr = bool(nr), True
    p.ch = []
    for c in range(4):
        w = ch[c * 32:c * 32 + 32]
        t, m = w[25], w[26]
        t = t - 0x10000 if t >= 0x8000 else t
        if m:
            w[4] = (w[4] + int(t * m / 256)) & 0xFFFF
        p.ch.append(w)
    tp, np, mix, lv = v2snd.out(p)
    return [tp[0], tp[1], tp[2], np, mix & 0o77, lv[0], lv[1], lv[2]]


def drvcheck(e, n, frames):
    """на входе drv_out каждого кадра — каналы драйвера; эталон v2snd.py после того же
    числа тактов от старта звука"""
    sys.path.insert(0, os.path.join(ROOT, 'tools'))
    import v2snd
    d = drv_syms()
    e.sound(n)
    e.text('bk_break', addr='0%o' % d['_drv_out'])
    ref = v2snd.Player(v2snd.sound_data(n), True)
    ticks = 0
    acc_prev = None
    fields = [('TL', 0), ('NC', 1), ('BF', 2), ('VOL', 5), ('VD', 6), ('NL', 10), ('HC', 11), ('HO', 12),
              ('HCNT', 13), ('FMO', 15)]
    bad = checked = obad = 0
    prev_out = None
    cands = None
    for f in range(frames):
        r = e.text('bk_run', max_frames=5)
        if 'breakpoint' not in r:
            continue
        nr = words(e.text('bk_read_mem', addr='0%o' % d['_cur_nr'], len=2))[0]
        acc = words(e.text('bk_read_mem', addr='0%o' % d['acc'], len=2))[0]
        if acc_prev is None:
            acc_prev = acc
            if nr != n:
                continue
        k = (acc_prev + 15104) // 3125 if acc_prev is not None else 0
        acc_prev = acc
        if nr != n:
            if ticks:
                print('drvcheck: звук кончился на такте %d (эталон: %s)' % (ticks, 'идёт' if ref.cur else 'кончился'))
                break
            continue
        ch = words(e.text('bk_read_mem', addr='0%o' % d['_ch'], len=4 * 64))
        regs = [b for b in bytes_of(e.text('bk_read_mem', addr='0%o' % d['ay_regs'], len=12, format='bytes'))][:11]
        if prev_out is not None and nr == n:   # регистры AY прошлого кадра против out() по его каналам
            want = prev_out
            got = [regs[0] | regs[1] << 8, regs[2] | regs[3] << 8, regs[4] | regs[5] << 8, regs[6], regs[7], regs[8], regs[9], regs[10]]
            # периоды молчащих каналов и шум без включённого шума не сравниваются
            for q in range(3):
                if not want[5 + q] or (want[4] >> q) & 1:   # молчит или тон выключен в R7
                    got[q] = want[q]
            if not any(not (want[4] >> (3 + q)) & 1 for q in range(3)):
                got[3] = want[3]
            if got != want:
                obad += 1
                if obad <= 5:
                    print('вывод: ждали %s, в AY %s' % (want, got))
                    if obad == 1:
                        print('  каналы прошлого кадра:', prev_ch)
        prev_out = out_of(ch, nr) if nr == n else None
        prev_ch = [ch[c * 32:c * 32 + 27] for c in range(4)]
        if not ticks:   # такты от старта звука — подбором (до 20000), с подтверждением вторым кадром
            import copy
            match = lambda pl: all(ch[c * 32 + ix] == pl.ch[c][ix] & 0xFFFF for c in range(4) for _, ix in fields)
            if cands is None:
                cands = []
                t = copy.deepcopy(ref)
                for kk in range(1, 20001):
                    t.tick()
                    if match(t):
                        cands.append(copy.deepcopy(t))
                continue
            for c0 in cands:
                for _ in range(k):
                    c0.tick()
            ok = [c0 for c0 in cands if match(c0)]
            if not ok:
                print('drvcheck: выравнивание не найдено')
                break
            ref = ok[0]
            ticks = 1
            print('drvcheck: выровнено (кандидатов %d -> %d)' % (len(cands), len(ok)))
            continue
        for _ in range(k):
            if ref.cur:
                ref.tick()
        ticks += k
        checked += 1
        for c in range(4):
            got = ch[c * 32:c * 32 + 25]
            want = ref.ch[c]
            diff = [(nm, got[ix], want[ix]) for nm, ix in fields if got[ix] != want[ix] & 0xFFFF]
            if diff:
                bad += 1
                if bad <= 5:
                    print('такт %d канал %d: %s' % (ticks, c, diff))
    e.text('bk_unbreak', addr='0%o' % d['_drv_out'])
    print('drvcheck %d: кадров %d, тактов %d, расхождений %d, вывод AY: %d' % (n, checked, ticks, bad, obad))


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
        elif c == 'sound':
            e.sound(int(a[0])); i += 1
        elif c == 'aycmp':   # aycmp N КАДРЫ [--spk]: записи AY против tools/v2snd.py
            n, fr = int(a[0]), int(a[1]); i += 2
            spk = len(a) > 2 and a[2] == '--spk'
            if spk:
                i += 1
            e.sound(n)
            got = e.ay_log(fr)
            ref = subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'v2snd.py'), str(n), str(fr)]
                                 + (['--spk'] if spk else []), capture_output=True, text=True).stdout
            want, prev = [], None
            for line in ref.split('\n'):
                m = re.match(r'\d+: A (\d+) B (\d+) C (\d+) N (\d+) M (\d+) V (\d+) (\d+) (\d+)', line)
                if m:
                    v = [int(x) for x in m.groups()]
                    v[4] = int(m.group(5), 8)
                    st = v
                    if st != prev:
                        want.append(st)
                        prev = st
            def norm(r):
                tp = [r[0] | r[1] << 8, r[2] | r[3] << 8, r[4] | r[5] << 8]
                lv = [r[8], r[9], r[10]]
                tp = [tp[k] if lv[k] and not (r[7] >> k) & 1 else 0 for k in range(3)]
                return tp + [r[6] if any(not (r[7] >> (3 + k)) & 1 for k in range(3)) else 0, r[7]] + lv
            gotn = []
            for r in got:
                x = norm(list(r))
                if not gotn or gotn[-1] != x:
                    gotn.append(x)
            wantn = [w[:3] if False else [w[0] if w[5] else 0, w[1] if w[6] else 0, w[2] if w[7] else 0] + w[3:] for w in want]
            k = 0
            for x in gotn:
                if k < len(wantn) and x == wantn[k]:
                    k += 1
            print('aycmp %d: эталон %d состояний, драйвер %d, совпало подряд %d' % (n, len(wantn), len(gotn), k))
            if k < len(wantn):
                print('  ждали', wantn[k], '\n  драйвер', gotn[:k + 3][-3:])
            if os.environ.get('AYDUMP'):
                for j in range(max(len(gotn), len(wantn))):
                    print('  %-40s %s' % (gotn[j] if j < len(gotn) else '', wantn[j] if j < len(wantn) else ''))
        elif c == 'drvcheck':   # drvcheck N КАДРЫ: каналы драйвера против tools/v2snd.py по тактам
            n, fr = int(a[0]), int(a[1]); i += 2
            drvcheck(e, n, fr)
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
