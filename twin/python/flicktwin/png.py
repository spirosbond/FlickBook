"""Dependency-free PNG helpers (8-bit gray / RGB / RGBA, non-interlaced) and
golden-image comparison used by the twin's tests."""
import struct
import zlib
from pathlib import Path
from typing import NamedTuple, Optional, Union


class Gray(NamedTuple):
    width: int
    height: int
    pixels: bytes  # width*height, 0 = black, 255 = white


def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def read_png(data: Union[bytes, str, Path]) -> Gray:
    if not isinstance(data, (bytes, bytearray)):
        data = Path(data).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    pos, idat, w = 8, b"", None
    while pos < len(data):
        length, ctype = struct.unpack(">I4s", data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if ctype == b"IHDR":
            w, h, depth, color, _, _, interlace = struct.unpack(">IIBBBBB", chunk)
            if depth != 8 or interlace or color not in (0, 2, 4, 6):
                raise ValueError("unsupported PNG format")
            bpp = {0: 1, 2: 3, 4: 2, 6: 4}[color]
        elif ctype == b"IDAT":
            idat += chunk
        elif ctype == b"IEND":
            break
    raw = zlib.decompress(idat)
    stride = w * bpp
    out = bytearray(w * h)
    prev = bytearray(stride)
    i = 0
    for y in range(h):
        ftype = raw[i]
        line = bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        if ftype == 1:
            for x in range(bpp, stride):
                line[x] = (line[x] + line[x - bpp]) & 255
        elif ftype == 2:
            for x in range(stride):
                line[x] = (line[x] + prev[x]) & 255
        elif ftype == 3:
            for x in range(stride):
                left = line[x - bpp] if x >= bpp else 0
                line[x] = (line[x] + ((left + prev[x]) >> 1)) & 255
        elif ftype == 4:
            for x in range(stride):
                left = line[x - bpp] if x >= bpp else 0
                upleft = prev[x - bpp] if x >= bpp else 0
                line[x] = (line[x] + _paeth(left, prev[x], upleft)) & 255
        if bpp == 1:
            out[y * w:(y + 1) * w] = line
        else:
            for x in range(w):
                p = x * bpp
                out[y * w + x] = line[p] if bpp == 2 else (line[p] * 54 + line[p + 1] * 183 + line[p + 2] * 19) >> 8
        prev = line
    return Gray(w, h, bytes(out))


def write_png(path: Union[str, Path], img: Gray):
    raw = b"".join(b"\x00" + img.pixels[y * img.width:(y + 1) * img.width] for y in range(img.height))

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", img.width, img.height, 8, 0, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b"")
    Path(path).write_bytes(data)


class Diff(NamedTuple):
    differing: int
    total: int
    fraction: float
    diff_image: Optional[Gray]


def compare(a: Gray, b: Gray, threshold: int = 32) -> Diff:
    """Count pixels whose gray value differs by more than `threshold`."""
    if (a.width, a.height) != (b.width, b.height):
        total = max(a.width * a.height, b.width * b.height)
        return Diff(total, total, 1.0, None)
    diff = bytearray(a.width * a.height)
    n = 0
    for i, (p, q) in enumerate(zip(a.pixels, b.pixels)):
        if abs(p - q) > threshold:
            n += 1
            diff[i] = 0
        else:
            diff[i] = 255 - (255 - p) // 4  # faint copy of the expected image
    total = a.width * a.height
    return Diff(n, total, n / total, Gray(a.width, a.height, bytes(diff)))
