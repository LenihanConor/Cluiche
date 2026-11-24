"""Minimal pure-Python PNG reader/writer (RGBA8, non-interlaced) for capture checks.

No third-party dependencies: the E2E environment ships neither Pillow nor numpy.
"""
import struct
import zlib
from itertools import accumulate


class Image:
    def __init__(self, width, height, rows):
        self.width = width
        self.height = height
        self.rows = rows  # list of bytearray, each width*4 (RGBA)

    def px(self, x, y):
        x = int(x)
        y = int(y)
        if x < 0 or y < 0 or x >= self.width or y >= self.height:
            return None
        o = x * 4
        r = self.rows[y]
        return (r[o], r[o + 1], r[o + 2], r[o + 3])


def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def read_png(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG"
    pos = 8
    idat = []
    width = height = depth = ctype = interlace = None
    while pos < len(data):
        n, = struct.unpack(">I", data[pos:pos + 4])
        t = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + n]
        pos += 12 + n
        if t == b"IHDR":
            width, height, depth, ctype, _c, _f, interlace = struct.unpack(">IIBBBBB", body)
        elif t == b"IDAT":
            idat.append(body)
        elif t == b"IEND":
            break
    assert depth == 8 and ctype in (2, 6) and interlace == 0, f"unsupported PNG (depth={depth} ctype={ctype})"
    bpp = 4 if ctype == 6 else 3
    raw = zlib.decompress(b"".join(idat))
    stride = width * bpp
    rows = []
    prev = bytearray(stride)
    p = 0
    for _ in range(height):
        ft = raw[p]
        cur = bytearray(raw[p + 1:p + 1 + stride])
        p += 1 + stride
        if ft == 1:
            for c in range(bpp):
                cur[c::bpp] = bytearray(accumulate(cur[c::bpp], lambda a, b: (a + b) & 255))
        elif ft == 2:
            cur = bytearray((a + b) & 255 for a, b in zip(cur, prev))
        elif ft == 3:
            for i in range(stride):
                left = cur[i - bpp] if i >= bpp else 0
                cur[i] = (cur[i] + ((left + prev[i]) >> 1)) & 255
        elif ft == 4:
            for i in range(stride):
                a = cur[i - bpp] if i >= bpp else 0
                c = prev[i - bpp] if i >= bpp else 0
                cur[i] = (cur[i] + _paeth(a, prev[i], c)) & 255
        rows.append(cur)
        prev = cur
    if bpp == 3:
        out = []
        for r in rows:
            n = bytearray(width * 4)
            n[0::4] = r[0::3]
            n[1::4] = r[1::3]
            n[2::4] = r[2::3]
            n[3::4] = b"\xff" * width
            out.append(n)
        rows = out
    return Image(width, height, rows)


def crop_centre(img, w, h):
    """Return a centred w x h crop (rows only) as a list of bytes per row."""
    x0 = (img.width - w) // 2
    y0 = (img.height - h) // 2
    return [bytes(img.rows[y][x0 * 4:(x0 + w) * 4]) for y in range(y0, y0 + h)]


def max_channel_delta(rows_a, rows_b):
    worst = 0
    count = 0
    for ra, rb in zip(rows_a, rows_b):
        if ra == rb:
            continue
        for a, b in zip(ra, rb):
            d = a - b if a > b else b - a
            if d > worst:
                worst = d
            if d:
                count += 1
    return worst, count


def write_png(path, width, height, rows):
    raw = b"".join(b"\x00" + bytes(r) for r in rows)

    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)
