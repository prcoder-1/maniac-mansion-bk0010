#!/usr/bin/env python3
"""Палитры БК-0011М по комнатам: подбор и таблицы.

    pal11.py КАТАЛОГ pal11.c            — таблицы для игры (gfx_v2.c)
    pal11.py КАТАЛОГ --png КАТАЛОГ_PNG  — превью: комната в выбранной палитре и в палитре 0
    pal11.py КАТАЛОГ --report           — палитра и ошибка каждой комнаты

У БК-0011М 16 палитр по 4 цвета (цвет 0 — всегда чёрный), одна на кадр. Цвет EGA
(16) рисуется узором 2x2 из цветов палитры: сплошной, шахматка двух цветов, 3:1.
Для палитры и цвета EGA выбирается узор с наименьшей ошибкой: ΔE (OKLab) смеси
против цвета EGA плюс штраф за контраст цветов узора (рябь); разным цветам комнаты —
по возможности разные узоры. Палитра комнаты — с наименьшей суммарной ошибкой по
гистограмме фона, с весом цветов интерфейса (глаголы, предложение, инвентарь,
курсор) и цветов актёров. Ручные поправки — OVERRIDE.

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
OVERRIDE = {}   # комната -> палитра (ручные поправки после просмотра превью)

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


PATS = patterns()
RIPPLE = 0.12    # штраф за контраст цветов узора (на ΔE между ними)
QUARTER = 0.05   # 3:1 — рябь заметнее шахматки


def pat_cost(pal, p, target):
    cols = [pal[i] for i in p]
    e = de(mix_lab(cols), target)
    if len(set(p)) > 1:
        a, b = sorted(set(p))[:2]
        e += RIPPLE * de(lab(pal[a]), lab(pal[b]))
        if p.count(p[0]) == 3:
            e += QUARTER * de(lab(pal[a]), lab(pal[b]))
    return e


def assign(pal, weight):
    """узор каждому цвету EGA: лучший, но разным цветам с весом — разные узоры
    (жадно, от весомых к лёгким: занятый узор — штраф за совпадение)"""
    if pal is PALETTES[0]:
        total = sum(weight.get(c, 0) * pat_cost(pal, HAND0_PAT[c], lab(EGA[c])) ** 2 for c in range(16))
        return {c: HAND0_PAT[c] for c in range(16)}, total
    costs = {c: sorted((pat_cost(pal, p, lab(EGA[c])), p) for p in PATS) for c in range(16)}
    taken, res, total = {}, {}, 0.0
    for c in sorted(range(16), key=lambda c: -weight.get(c, 0)):
        w = weight.get(c, 0)
        best = None
        for e, p in costs[c]:
            clash = 0
            if p in taken and w > 0:
                o = taken[p]
                clash = 12.0 * min(w, weight.get(o, 0)) / max(w, 1)   # не различить — плохо
            score = e + clash
            if best is None or score < best[0]:
                best = (score, p, e)
        res[c] = best[1]
        if w > 0:
            taken.setdefault(best[1], c)
        total += w * best[0] * best[0]   # квадрат: большой сдвиг тона на большой площади — хуже
    return res, total


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


def room_hist(d, n):
    import v2gfx
    f = v2gfx.load(d, n)
    w, h = struct.unpack_from('<HH', f, 4)
    io = struct.unpack_from('<H', f, 0x0A)[0]
    img, _, _ = v2gfx.decode(f, io, w, h)
    hist = [0] * 16
    for row in img:
        for c in row:
            hist[c] += 1
    return hist, img, w, h


def choose(hist):
    weight = {c: hist[c] for c in range(16)}
    for c, v in UI_WEIGHT.items():
        weight[c] = weight.get(c, 0) + v
    for c, v in ACTOR_WEIGHT.items():
        weight[c] = weight.get(c, 0) + v
    best = None
    for p in CANDIDATES:
        res, total = assign(PALETTES[p], weight)
        if best is None or total < best[1]:
            best = (p, total, res)
    return best


def rooms(d):
    return [n for n in range(1, 61) if os.path.exists(os.path.join(d, '%02d.LFL' % n))]


def render(img, w, h, pal, pats):
    import v2gfx
    out = []
    for y in range(h):
        row = []
        for x in range(w):
            if x % 5 == 4:
                continue
            bx = x - x // 5
            row.append(rgb(pal[pats[img[y][x]][((y & 1) << 1) | (bx & 1)]]))
        out.append([c for px in row for c in [px, px]])   # точки БК — двойные по ширине
    return [[px for px in r] for r in out], len(out[0])


def main():
    d = sys.argv[1]
    if '--report' in sys.argv or '--png' in sys.argv:
        pdir = sys.argv[sys.argv.index('--png') + 1] if '--png' in sys.argv else None
        if pdir:
            os.makedirs(pdir, exist_ok=True)
        import v2gfx
        for n in rooms(d):
            hist, img, w, h = room_hist(d, n)
            p, total, pats = choose(hist)
            p = OVERRIDE.get(n, p)
            pats, _ = assign(PALETTES[p], {c: hist[c] + UI_WEIGHT.get(c, 0) + ACTOR_WEIGHT.get(c, 0) for c in range(16)})
            print('room %2d: палитра %2d, ошибка %.0f, цвета %s' % (n, p, (total / max(1, sum(hist))) ** 0.5,
                  ' '.join('%d:%d' % (c, hist[c]) for c in range(16) if hist[c])))
            if pdir:
                a, wa = render(img, w, h, PALETTES[p], pats)
                p0 = {c: HAND0_PAT[c] for c in range(16)}
                b, _ = render(img, w, h, PALETTES[0], p0)
                gap = [(40, 40, 40)] * wa
                pix = a + [gap] * 4 + b
                v2gfx.png(os.path.join(pdir, 'room%02d_p%d.png' % (n, p)), pix, wa, len(pix))
        return
    out = sys.argv[2]
    room_pal = [0] * 64
    used = {}
    for n in rooms(d):
        hist, _, _, _ = room_hist(d, n)
        p, total, pats = choose(hist)
        p = OVERRIDE.get(n, p)
        room_pal[n] = p
        used[p] = True
    # таблица узоров каждой палитры — без привязки к комнате (общий вес)
    lines = ['/* сгенерировано tools/pal11.py: палитры БК-0011М по комнатам */',
             '#include <stdint.h>', '',
             '/* комната -> палитра (0177662, разряды 8..11) */',
             'const uint8_t room_pal[64] = {']
    for i in range(0, 64, 16):
        lines.append('    ' + ', '.join(str(v) for v in room_pal[i:i + 16]) + ',')
    lines.append('};')
    lines.append('')
    lines.append('/* палитра -> узор 2x2 [TL, TR, BL, BR] цвета EGA и сплошной цвет текста */')
    lines.append('const uint8_t pal_pat[16][16][4] = {')
    base = {c: 1000 + UI_WEIGHT.get(c, 0) + ACTOR_WEIGHT.get(c, 0) for c in range(16)}
    for p in range(16):
        pats, _ = assign(PALETTES[p], base) if p in used or p == 0 else ({c: (0, 0, 0, 0) for c in range(16)}, 0)
        lines.append('    { ' + ', '.join('{%d, %d, %d, %d}' % pats[c] for c in range(16)) + ' },')
    lines.append('};')
    lines.append('const uint8_t pal_text[16][16] = {')
    for p in range(16):
        lines.append('    { ' + ', '.join(str(v) for v in text_colors(PALETTES[p])) + ' },')
    lines.append('};')
    open(out, 'w').write('\n'.join(lines) + '\n')
    print('%s: палитры %s' % (out, sorted(used)))


main()
