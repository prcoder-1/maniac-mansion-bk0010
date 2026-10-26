#!/usr/bin/env python3
"""Палитры и узоры БК-0011М по комнатам: подбор и таблицы.

    pal11.py КАТАЛОГ pal11.c pal11a.s   — таблицы для игры: фон (gfx_v2.c), актёры (costume_r.c)
    pal11.py КАТАЛОГ --png КАТАЛОГ_PNG  — превью: комната в выбранной палитре и в палитре 0
    pal11.py КАТАЛОГ --report           — палитра и ошибка каждой комнаты

У БК-0011М 16 палитр по 4 цвета (цвет 0 — всегда чёрный), одна на кадр. Цвет EGA
(16) рисуется узором 2x2 из цветов палитры: сплошной, шахматка двух цветов, 3:1.
Узоры цветов EGA подбираются для каждой комнаты вместе: ошибка цвета — ΔE (OKLab)
смеси против цвета EGA плюс штраф за контраст цветов узора (рябь), с весом по
гистограмме; плюс штраф за соседние цвета, ставшие на БК неразличимыми: иначе тёмное
на чёрном (особняк на ночном небе) или рот на лице пропадают. Два набора узоров в
одной палитре: фона (картинки комнаты и её объектов, цвета интерфейса) и актёров
(цвета и пары на лицах и фигурах: кожа, черты, волосы, глаза). Палитра комнаты — с
наименьшей суммой обоих. Ручные поправки — OVERRIDE.

Цвета палитр — как в эмуляторе (src/core/Screen.cpp, по GID), по индексу точки
0..3 (младший бит — первая точка пары).
"""
import os, struct, sys, zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

PALETTES = [
    [0x000000, 0x0000FF, 0x00FF00, 0xFF0000],
    [0x000000, 0xFFFF00, 0xFF00FF, 0xFF0000],
    [0x000000, 0x00FFFF, 0x0000FF, 0xFF00FF],
    [0x000000, 0x00FF00, 0x00FFFF, 0xFFFF00],
    [0x000000, 0xFF00FF, 0x00FFFF, 0xFFFFFF],
    [0x000000, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF],
    [0x000000, 0xAA0000, 0x700000, 0xFF0000],
    [0x000000, 0xAAFF00, 0x55FF00, 0xFFFF00],
    [0x000000, 0xAA00FF, 0x7000FF, 0xFF00FF],
    [0x000000, 0x55FF00, 0x7000FF, 0x700000],
    [0x000000, 0xAAFF00, 0xAA00FF, 0xAA0000],
    [0x000000, 0x00FFFF, 0xFFFF00, 0xFF0000],
    [0x000000, 0xFF0000, 0x00FF00, 0x00FFFF],
    [0x000000, 0x00FFFF, 0xFFFF00, 0xFFFFFF],
    [0x000000, 0xFFFF00, 0x00FF00, 0xFFFFFF],
    [0x000000, 0x00FFFF, 0x00FF00, 0xFFFFFF],
]
EGA = [0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
       0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF]
# палитра 0 — ручные узоры и цвета текста (gfx_v2.c bg_pat, gfx.c text_pal): цвет EGA ->
# [TL, TR, BL, BR]; подбор для неё — по ним же
HAND0_PAT = [(0, 0, 0, 0), (1, 1, 1, 1), (2, 2, 2, 2), (1, 2, 2, 1),
             (3, 3, 3, 3), (3, 1, 1, 3), (3, 0, 0, 3), (1, 1, 2, 2),
             (1, 0, 0, 1), (2, 1, 1, 1), (2, 2, 2, 2), (1, 2, 2, 2),
             (3, 3, 3, 3), (1, 3, 3, 3), (3, 2, 2, 3), (3, 2, 2, 2)]
HAND0_TEXT = [0, 1, 2, 1, 3, 3, 3, 2, 1, 1, 2, 2, 3, 1, 3, 2]
# палитры-кандидаты: 5 (белый трижды) и 6, 8 (оттенки одного цвета) для комнат MM не годятся
CANDIDATES = [0, 1, 2, 3, 4, 7, 9, 10, 11, 12, 13, 14, 15]
# палитры комнат — выбранные на этапе 4 (по цвету); узоры в них подбираются заново.
# Свободный выбор с учётом соседства (choose, --report) менял бы палитру многим
# комнатам не к лучшему (вода бассейна салатовая, облако взрыва фиолетовое)
ROOM_PAL = [0, 12, 0, 10, 0, 10, 0, 0, 10, 10, 11, 10, 0, 10, 11, 10,
            10, 10, 10, 1, 0, 10, 0, 1, 0, 0, 0, 1, 0, 10, 0, 0,
            0, 12, 10, 0, 10, 11, 0, 1, 12, 10, 10, 1, 12, 0, 1, 12,
            0, 1, 0, 0, 10, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
# ручные поправки: заставка (33) — в палитре 0 особняк (тёмно-серый и тёмно-синий на
# чёрном небе) виден синим, в 12 он чёрный
OVERRIDE = {33: 0}

# цвета интерфейса v2 (verbs.c, gfx.c): глаголы 2, подсветка 14, тусклые 8,
# предложение и инвентарь 13, стрелки 1, курсор 15/7/8 — веса в «точках»
UI_WEIGHT = {2: 3000, 14: 800, 8: 600, 13: 2500, 1: 400, 15: 800, 7: 400}
# актёры: кожа (светло-красный — лица на экране почти всегда), волосы, одежда героев
# и Эдисонов — коричневый, жёлтый, красный, синие, зелёные, серые, белый
ACTOR_WEIGHT = {12: 8000, 6: 3000, 14: 1500, 4: 1000, 1: 600, 9: 600, 2: 400, 10: 400, 7: 600, 8: 600, 15: 600}


def rgb(c):
    return ((c >> 16) & 255, (c >> 8) & 255, c & 255)


def lin(v):
    v /= 255.0
    return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4


def lab_of_lin(r, g, b):
    """OKLab (Björn Ottosson): в синих тонах вернее CIELAB; масштаб — как у L*"""
    l = 0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b
    m = 0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b
    s_ = 0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b
    l, m, s_ = l ** (1 / 3), m ** (1 / 3), s_ ** (1 / 3)
    return (100 * (0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s_),
            100 * (1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s_),
            100 * (0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s_))


def lab(c):
    r, g, b = rgb(c)
    return lab_of_lin(lin(r), lin(g), lin(b))


def mix_lab(cols):
    """средний цвет узора (смесь в линейном свете)"""
    rs = [lin(rgb(c)[0]) for c in cols]
    gs = [lin(rgb(c)[1]) for c in cols]
    bs = [lin(rgb(c)[2]) for c in cols]
    n = len(cols)
    return lab_of_lin(sum(rs) / n, sum(gs) / n, sum(bs) / n)


def de(a, b):
    return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2) ** 0.5


def patterns():
    """узоры 2x2 [TL, TR, BL, BR] из индексов 0..3: сплошные, шахматки, 3:1"""
    out = []
    for a in range(4):
        out.append((a, a, a, a))
    for a in range(4):
        for b in range(a + 1, 4):
            out.append((a, b, b, a))
    for a in range(4):
        for b in range(4):
            if a != b:
                out.append((a, a, a, b))
    return out


PATS = patterns() + sorted(set(HAND0_PAT) - set(patterns()))   # и ручные узоры палитры 0
RIPPLE = 0.12    # штраф за контраст цветов узора (на ΔE между ними)
QUARTER = 0.05   # 3:1 — рябь заметнее шахматки


def pat_cost(pal, p, target):
    cols = [pal[i] for i in p]
    e = de(mix_lab(cols), target)
    if len(set(p)) > 1:
        a, b = sorted(set(p))[:2]
        e += RIPPLE * de(lab(pal[a]), lab(pal[b]))
        if max(p.count(i) for i in p) == 3:
            e += QUARTER * de(lab(pal[a]), lab(pal[b]))
    return e


# соседство цветов: на БК пара должна различаться не меньше чем на
# PAIR_K * min(ΔE EGA, PAIR_CAP); штраф — PAIR_LAM * число соседств * недобор²
PAIR_K, PAIR_CAP, PAIR_LAM, PAIR_TEX = 0.6, 40.0, 2.0, 4.0
HAND_STICK = 10.0   # палитра 0: ручной узор (HAND0_PAT) дешевле на столько ΔE — отход только ради различимости
# пары на лицах и фигурах актёров (костюмы — не в комнате): кожа и черты лица,
# глаза и контуры, волосы
ACTOR_ADJ = {(12, 4): 20000, (12, 0): 20000, (12, 6): 20000, (12, 14): 10000, (6, 0): 10000, (4, 0): 10000,
             (4, 6): 5000}


class PalTab:
    """для палитры: смесь и ошибка каждого узора, различие узоров"""
    def __init__(self, pal):
        self.pal = pal
        mixes = [mix_lab([pal[i] for i in p]) for p in PATS]
        self.ecost = [[pat_cost(pal, p, lab(EGA[c])) for p in PATS] for c in range(16)]
        self.eopt = [r[:] for r in self.ecost]   # для подбора узоров (сумма — по ecost)
        if pal is PALETTES[0]:
            for c in range(16):
                k = PATS.index(HAND0_PAT[c])
                self.eopt[c][k] = max(0.0, self.eopt[c][k] - HAND_STICK)
        self.dpp = [[0.0 if i == j else de(mixes[i], mixes[j]) + PAIR_TEX for j in range(len(PATS))]
                    for i in range(len(PATS))]


TABS = {}


def tab(p):
    if p not in TABS:
        TABS[p] = PalTab(PALETTES[p])
    return TABS[p]


WANT = [[min(de(lab(EGA[a]), lab(EGA[b])), PAIR_CAP) * PAIR_K for b in range(16)] for a in range(16)]


def assign(p, weight, adj):
    """узоры цветам EGA (индексы PATS) для палитры p: спуск по цветам от лучших по
    отдельности -> (узоры, сумма)"""
    t = tab(p)
    n = len(PATS)
    nb = {c: [(b, adj[c][b]) for b in range(16) if b != c and adj[c][b]] for c in range(16)}
    cur = [min(range(n), key=lambda k: t.eopt[c][k]) for c in range(16)]

    def cost_c(c, k):
        e = weight.get(c, 0) * t.eopt[c][k] ** 2
        for b, a in nb[c]:
            u = WANT[c][b] - t.dpp[k][cur[b]]
            if u > 0:
                e += PAIR_LAM * a * u * u
        return e

    for _ in range(12):
        changed = False
        for c in range(16):
            if not weight.get(c, 0) and not nb[c]:
                continue
            best = min(range(n), key=lambda k: cost_c(c, k))
            if cost_c(c, best) < cost_c(c, cur[c]) - 1e-9:
                cur[c] = best
                changed = True
        if not changed:
            break
    total = sum(weight.get(c, 0) * t.ecost[c][cur[c]] ** 2 for c in range(16))
    for a in range(16):
        for b in range(a + 1, 16):
            if adj[a][b]:
                u = WANT[a][b] - t.dpp[cur[a]][cur[b]]
                if u > 0:
                    total += PAIR_LAM * adj[a][b] * u * u
    return cur, total


def text_colors(pal):
    """цвет EGA текста -> сплошной нечёрный цвет палитры"""
    if pal is PALETTES[0]:
        return HAND0_TEXT
    out = []
    for c in range(16):
        if c == 0:
            out.append(0)
            continue
        t = lab(EGA[c])
        out.append(min(range(1, 4), key=lambda i: de(lab(pal[i]), t)))
    return out


def add_img(img, w, h, hist, adj):
    for y in range(h):
        row = img[y]
        nxt = img[y + 1] if y + 1 < h else None
        for x in range(w):
            c = row[x]
            hist[c] += 1
            if x + 1 < w and row[x + 1] != c:
                adj[c][row[x + 1]] += 1
                adj[row[x + 1]][c] += 1
            if nxt is not None and nxt[x] != c:
                adj[c][nxt[x]] += 1
                adj[nxt[x]][c] += 1


def room_objects(f):
    """картинки объектов комнаты: [(x, y, img, w, h)]"""
    import v2gfx
    n = f[20]
    out, seen = [], set()
    for i in range(n):
        obim = struct.unpack_from('<H', f, 28 + 2 * i)[0]
        obcd = struct.unpack_from('<H', f, 28 + 2 * n + 2 * i)[0]
        b = f[obcd + 4:obcd + 14]
        x, y, w, h = b[3] * 8, (b[4] & 0x7F) * 8, b[5] * 8, b[9] & 0xF8
        if not obim or not w or not h or obim in seen:
            continue
        seen.add(obim)
        try:
            img, _, _ = v2gfx.decode(f, obim, w, h)
        except IndexError:
            continue
        out.append((x, y, img, w, h))
    return out


def room_stats(d, n):
    """гистограмма и соседство цветов фона и объектов комнаты"""
    import v2gfx
    f = v2gfx.load(d, n)
    w, h = struct.unpack_from('<HH', f, 4)
    io = struct.unpack_from('<H', f, 0x0A)[0]
    img, _, _ = v2gfx.decode(f, io, w, h)
    hist = [0] * 16
    adj = [[0] * 16 for _ in range(16)]
    add_img(img, w, h, hist, adj)
    objs = room_objects(f)
    for _, _, oi, ow, oh in objs:
        add_img(oi, ow, oh, hist, adj)
    return hist, adj, img, w, h, objs


def weights(hist, adj):
    """фон: гистограмма и соседство картинок комнаты, цвета интерфейса"""
    weight = {c: hist[c] for c in range(16)}
    for c, v in UI_WEIGHT.items():
        weight[c] = weight.get(c, 0) + v
    return weight, adj


def actor_weights():
    a = [[0] * 16 for _ in range(16)]
    for (x, y), v in ACTOR_ADJ.items():
        a[x][y] += v
        a[y][x] += v
    return dict(ACTOR_WEIGHT), a


def choose(hist, adj, room=None, free=False):
    """-> (палитра, сумма, узоры фона, узоры актёров); палитра комнаты — ROOM_PAL и
    OVERRIDE, free — лучшая из CANDIDATES"""
    weight, a = weights(hist, adj)
    aw, aa = actor_weights()
    best = None
    fixed = OVERRIDE.get(room, ROOM_PAL[room] if room is not None else None)
    for p in (CANDIDATES if free or fixed is None else [fixed]):
        bg, t1 = assign(p, weight, a)
        ac, t2 = assign(p, aw, aa)
        if best is None or t1 + t2 < best[1]:
            best = (p, t1 + t2, bg, ac)
    return best


def rooms(d):
    return [n for n in range(1, 61) if os.path.exists(os.path.join(d, '%02d.LFL' % n))]


def render(img, w, h, pal, pats, objs=()):
    """картинка (с объектами поверх) в узорах: pats — цвет EGA -> узор [TL, TR, BL, BR]"""
    img = [r[:] for r in img]
    for ox, oy, oi, ow, oh in objs:
        for y in range(oh):
            for x in range(ow):
                if 0 <= oy + y < h and 0 <= ox + x < w:
                    img[oy + y][ox + x] = oi[y][x]
    out = []
    for y in range(h):
        row = []
        for x in range(w):
            if x % 5 == 4:
                continue
            e = img[y][x]
            if x % 5 == 3 and x + 1 < w and e == img[y][x - 1]:
                e = img[y][x + 1]
            bx = x - x // 5
            row.append(rgb(pal[pats[e][((y & 1) << 1) | (bx & 1)]]))
        out.append([c for px in row for c in [px, px]])   # точки БК — двойные по ширине
    return out, len(out[0])


def main():
    d = sys.argv[1]
    if '--report' in sys.argv or '--png' in sys.argv:
        pdir = sys.argv[sys.argv.index('--png') + 1] if '--png' in sys.argv else None
        only = [int(a) for a in sys.argv[sys.argv.index('--rooms') + 1].split(',')] if '--rooms' in sys.argv else None
        if pdir:
            os.makedirs(pdir, exist_ok=True)
        import v2gfx
        for n in rooms(d):
            if only and n not in only:
                continue
            hist, adj, img, w, h, objs = room_stats(d, n)
            p, total, cur, acur = choose(hist, adj, n)
            pf = choose(hist, adj, n, True)[0]
            print('room %2d: палитра %2d (свободный выбор %2d), ошибка %.0f, узоры %s; актёры %s' % (n, p, pf, (total / max(1, sum(hist))) ** 0.5,
                  ' '.join('%d:%s' % (c, ''.join(map(str, PATS[cur[c]]))) for c in range(16) if hist[c]),
                  ' '.join('%d:%s' % (c, ''.join(map(str, PATS[acur[c]]))) for c in sorted(ACTOR_WEIGHT))))
            if pdir:
                a, wa = render(img, w, h, PALETTES[p], [PATS[k] for k in cur], objs)
                b, _ = render(img, w, h, PALETTES[0], HAND0_PAT, objs)
                gap = [(40, 40, 40)] * wa
                pix = a + [gap] * 4 + b
                v2gfx.png(os.path.join(pdir, 'room%02d_p%d.png' % (n, p)), pix, wa, len(pix))
        return
    out, outa = sys.argv[2], sys.argv[3]
    room_pal = [0] * 64
    room_pat = [None] * 64
    for n in rooms(d):
        hist, adj, _, _, _, _ = room_stats(d, n)
        p, _, cur, _ = choose(hist, adj, n)
        room_pal[n] = p
        room_pat[n] = cur
    # комнаты без файла (и 0 — до первой) — палитра 0, узоры по общим весам
    w0, a0 = weights([1000] * 16, [[0] * 16 for _ in range(16)])
    gen0, _ = assign(0, w0, a0)
    lines = ['/* сгенерировано tools/pal11.py: палитры и узоры БК-0011М по комнатам */',
             '#include <stdint.h>', '',
             '/* комната -> палитра (0177662, разряды 8..11) */',
             'const uint8_t room_pal[64] = {']
    for i in range(0, 64, 16):
        lines.append('    ' + ', '.join(str(v) for v in room_pal[i:i + 16]) + ',')
    lines.append('};')
    lines.append('')
    lines.append('/* узоры 2x2 [TL, TR, BL, BR] (индексы цветов палитры) */')
    lines.append('const uint8_t pal_pats[%d][4] = {' % len(PATS))
    lines.append('    ' + ', '.join('{%d, %d, %d, %d}' % q for q in PATS))
    lines.append('};')
    lines.append('/* комната -> цвет EGA -> узор (pal_pats) */')
    lines.append('const uint8_t room_pat[64][16] = {')
    for n in range(64):
        lines.append('    { ' + ', '.join(str(v) for v in (room_pat[n] or gen0)) + ' },')
    lines.append('};')
    lines.append('/* палитра -> сплошной цвет текста цвета EGA */')
    lines.append('const uint8_t pal_text[16][16] = {')
    for p in range(16):
        lines.append('    { ' + ', '.join(str(v) for v in text_colors(PALETTES[p])) + ' },')
    lines.append('};')
    lines.append('#ifdef HOST')
    lines.append('/* хостовый эталон (v2gfx.py BG_PAT): ручные узоры палитры 0 */')
    lines.append('const uint8_t pal_hand0[16][4] = {')
    lines.append('    ' + ', '.join('{%d, %d, %d, %d}' % q for q in HAND0_PAT))
    lines.append('};')
    lines.append('#endif')
    open(out, 'w').write('\n'.join(lines) + '\n')
    # узоры актёров — по палитре (подбор по одним весам актёров), в код банка B
    # (.text: данные банка B пишутся в сохранение)
    la = ['/ сгенерировано tools/pal11.py: узоры актёров по палитрам (costume_r.c)',
          '\t.globl\t_pal_apat, _apat_pats', '\t.text',
          '/ узоры 2x2 [TL, TR, BL, BR] (индексы цветов палитры)', '_apat_pats:']
    for q in PATS:
        la.append('\t.byte\t%d, %d, %d, %d' % q)
    la.append('/ палитра -> цвет EGA (палитра актёра) -> узор (_apat_pats)')
    la.append('_pal_apat:')
    for p in range(16):
        acur = assign(p, *actor_weights())[0] if p in room_pal else [0] * 16
        la.append('\t.byte\t' + ', '.join(str(v) for v in acur))
    la.append('\t.even')
    open(outa, 'w').write('\n'.join(la) + '\n')
    print('%s, %s: палитры %s' % (out, outa, sorted(set(room_pal))))


if __name__ == "__main__":
    main()
