#!/usr/bin/env python3
"""a.out (pdp11) -> .BIN БК: заголовок (адрес, длина) + text + data; bss не пишется.
mkbin.py --check file.out START LIMIT — проверить, что образ с bss кончается не дальше LIMIT."""
import struct, sys
if sys.argv[1] == '--check':
    # mkbin.py --check file.out START LIMIT: образ с bss от START кончается не дальше LIMIT
    d = open(sys.argv[2], 'rb').read()
    _, t, dd, b = struct.unpack('<4H', d[:8])
    end = int(sys.argv[3], 0) + t + dd + b
    lim = int(sys.argv[4], 0)
    if end > lim:
        sys.exit('ПРЕДЕЛ: %s кончается на %06o > %06o' % (sys.argv[2], end, lim))
    sys.exit(0)
src, dst = sys.argv[1], sys.argv[2]
d = open(src, 'rb').read()
magic, tsize, dsize, bsize, ssize, entry = struct.unpack('<6H', d[:12])
body = d[16:16 + tsize + dsize]
addr = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0o1000
open(dst, 'wb').write(struct.pack('<HH', addr, len(body)) + body)
print('%s: %06o..%06o (%d байт), bss %d, конец %06o' % (dst, addr, addr + len(body), len(body), bsize, addr + len(body) + bsize))
