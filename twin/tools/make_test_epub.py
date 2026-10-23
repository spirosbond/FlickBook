#!/usr/bin/env python3
"""Generate the small, original-content EPUB fixtures used by the twin's tests.

The books are deterministic (fixed seeds, fixed zip timestamps), so re-running
this script reproduces byte-identical files:

    python3 twin/tools/make_test_epub.py twin/fixtures/books

Books:
  twin_test_book.epub   - 4 chapters of plain prose with headings (pagination, swipes)
  twin_gallery.epub     - cover image + an inline JPEG (ImageScaler / JPEGDEC path)
  twin_edge_cases.epub  - typographic quotes, accents, nested inline tags, a very long word,
                          an empty chapter, OEBPS/ sub-directory layout
  twin_progressive.epub - 1400x2128 progressive cover (JPEGDEC progressive path)
"""
import math
import random
import struct
import sys
import zipfile
from pathlib import Path

FIXED_DATE = (2024, 1, 1, 0, 0, 0)

# --------------------------------------------------------------- prose -----
WORDS = (
    "lamp harbor tide quiet keeper window ledger morning salt rope lantern gull cliff "
    "compass winter letter beacon kettle stairs evening signal storm wick brass glass "
    "island ferry chart pencil whistle drift anchor shore ember clock garden path"
).split()
VERBS = "watched carried counted mended wrote polished followed climbed opened folded".split()
ADJ = "small patient careful bright grey narrow steady old warm distant".split()


def sentence(rng: random.Random) -> str:
    s = "The {a} {n1} {v} the {b} {n2}".format(
        a=rng.choice(ADJ), n1=rng.choice(WORDS), v=rng.choice(VERBS),
        b=rng.choice(ADJ), n2=rng.choice(WORDS))
    if rng.random() < 0.4:
        s += " before the {} {}".format(rng.choice(ADJ), rng.choice(WORDS))
    return s + "."


def paragraph(rng: random.Random, n: int) -> str:
    return " ".join(sentence(rng) for _ in range(n))


# ------------------------------------------------- tiny baseline JPEG ------
ZIGZAG = [0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7,
          14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46,
          53, 60, 61, 54, 47, 55, 62, 63]
LUMA_Q = [16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55, 14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22,
          29, 51, 87, 80, 62, 18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92, 49, 64, 78, 87, 103,
          121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99]
DC_BITS = [0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0]
DC_VALS = list(range(12))
AC_BITS = [0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7D]
AC_VALS = [
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14,
    0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09,
    0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A,
    0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65,
    0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
    0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9,
    0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA,
    0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
    0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA]


def huff_codes(bits, vals):
    codes, code, k = {}, 0, 0
    for length in range(1, 17):
        for _ in range(bits[length - 1]):
            codes[vals[k]] = (code, length)
            code += 1
            k += 1
        code <<= 1
    return codes


class BitWriter:
    def __init__(self):
        self.out = bytearray()
        self.acc = 0
        self.n = 0

    def write(self, code, length):
        for i in range(length - 1, -1, -1):
            self.acc = (self.acc << 1) | ((code >> i) & 1)
            self.n += 1
            if self.n == 8:
                self.out.append(self.acc)
                if self.acc == 0xFF:
                    self.out.append(0)
                self.acc = 0
                self.n = 0

    def flush(self):
        while self.n:
            self.write(1, 1)


def encode_value(v):
    if v == 0:
        return 0, 0
    size = abs(v).bit_length()
    return size, (v if v > 0 else v + (1 << size) - 1)


def jpeg_gray(width, height, pixel, progressive=False):
    """Encode a 4:4:4 YCbCr JPEG with neutral chroma; pixel(x, y) -> luma 0..255.

    Three components are used (rather than a 1-component grayscale JPEG) because the
    Inkplate library's TJpgDec decoder only supports Y/Cb/Cr JPEGs - like most EPUBs ship.
    Progressive fixtures contain a DC-only scan, matching JPEGDEC's supported output."""
    dc_codes, ac_codes = huff_codes(DC_BITS, DC_VALS), huff_codes(AC_BITS, AC_VALS)
    cos = [[math.cos((2 * x + 1) * u * math.pi / 16) for x in range(8)] for u in range(8)]
    bw, prev_dc = BitWriter(), [0, 0, 0]

    def encode_block(zz, comp):
        size, bits = encode_value(zz[0] - prev_dc[comp])
        prev_dc[comp] = zz[0]
        bw.write(*dc_codes[size])
        bw.write(bits, size)
        if progressive:
            return
        run = 0
        for i in range(1, 64):
            if zz[i] == 0:
                run += 1
                continue
            while run > 15:
                bw.write(*ac_codes[0xF0])
                run -= 16
            size, bits = encode_value(zz[i])
            bw.write(*ac_codes[(run << 4) | size])
            bw.write(bits, size)
            run = 0
        if run:
            bw.write(*ac_codes[0x00])

    neutral = [0] * 64  # Cb = Cr = 128 -> all coefficients zero
    for by in range(0, height, 8):
        for bx in range(0, width, 8):
            block = [[pixel(min(bx + x, width - 1), min(by + y, height - 1)) - 128 for x in range(8)] for y in range(8)]
            coeffs = [0] * 64
            for v in range(1 if progressive else 8):
                for u in range(1 if progressive else 8):
                    s = sum(block[y][x] * cos[u][x] * cos[v][y] for y in range(8) for x in range(8))
                    cu = 1 / math.sqrt(2) if u == 0 else 1
                    cv = 1 / math.sqrt(2) if v == 0 else 1
                    coeffs[v * 8 + u] = int(round(0.25 * cu * cv * s / LUMA_Q[v * 8 + u]))
            encode_block([coeffs[ZIGZAG[i]] for i in range(64)], 0)
            encode_block(neutral, 1)
            encode_block(neutral, 2)
    bw.flush()

    def seg(marker, payload):
        return struct.pack(">BBH", 0xFF, marker, len(payload) + 2) + payload

    out = b"\xFF\xD8"
    out += seg(0xE0, b"JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00")
    out += seg(0xDB, b"\x00" + bytes(LUMA_Q[ZIGZAG[i]] for i in range(64)))
    out += seg(0xC2 if progressive else 0xC0,
               struct.pack(">BHHB", 8, height, width, 3) + b"\x01\x11\x00\x02\x11\x00\x03\x11\x00")
    for tid in (0, 1):  # identical luma/chroma tables (ids 0 and 1)
        out += seg(0xC4, bytes([0x00 | tid]) + bytes(DC_BITS) + bytes(DC_VALS))
        out += seg(0xC4, bytes([0x10 | tid]) + bytes(AC_BITS) + bytes(AC_VALS))
    out += seg(0xDA, b"\x03\x01\x00\x02\x11\x03\x11" +
               (b"\x00\x00\x00" if progressive else b"\x00\x3F\x00"))
    return out + bytes(bw.out) + b"\xFF\xD9"


def cover_pixel(w, h):
    def px(x, y):
        # Concentric rings with a diagonal gradient - easy to recognise when dithered.
        r = math.hypot(x - w / 2, y - h / 2)
        ring = 255 if int(r / 18) % 2 == 0 else 60
        return int(0.6 * ring + 0.4 * (255 * (x + y) / (w + h)))
    return px


# ----------------------------------------------------------------- EPUB ----
CONTAINER = """<?xml version="1.0" encoding="UTF-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
<rootfiles><rootfile full-path="{opf}" media-type="application/oebps-package+xml"/></rootfiles>
</container>
"""


def xhtml(title, body):
    return ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>{}</title></head>"
            "<body>{}</body></html>\n").format(title, body)


def write_epub(path: Path, title, author, chapters, images=(), cover_id=None, root="OEBPS/"):
    """chapters: list of (id, filename, xhtml); images: list of (id, filename, bytes, media)."""
    items, spine = [], []
    for cid, fname, _ in chapters:
        items.append('<item id="{}" href="{}" media-type="application/xhtml+xml"/>'.format(cid, fname))
        spine.append('<itemref idref="{}"/>'.format(cid))
    for iid, fname, _, media in images:
        items.append('<item id="{}" href="{}" media-type="{}"/>'.format(iid, fname, media))
    meta_cover = '<meta name="cover" content="{}"/>'.format(cover_id) if cover_id else ""
    opf = ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<package xmlns=\"http://www.idpf.org/2007/opf\" version=\"2.0\" unique-identifier=\"uid\">"
           "<metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
           "<dc:title>{t}</dc:title><dc:creator>{a}</dc:creator><dc:language>en</dc:language>"
           "<dc:identifier id=\"uid\">flicktwin-{slug}</dc:identifier>{c}</metadata>"
           "<manifest>{items}</manifest><spine>{spine}</spine></package>\n").format(
        t=title, a=author, slug=path.stem, c=meta_cover, items="".join(items), spine="".join(spine))

    def add(zf, name, data, compress=zipfile.ZIP_DEFLATED):
        info = zipfile.ZipInfo(name, FIXED_DATE)
        info.compress_type = compress
        info.external_attr = 0o644 << 16
        zf.writestr(info, data)

    with zipfile.ZipFile(path, "w") as zf:
        add(zf, "mimetype", "application/epub+zip", zipfile.ZIP_STORED)
        add(zf, "META-INF/container.xml", CONTAINER.format(opf=root + "content.opf"))
        add(zf, root + "content.opf", opf)
        for _, fname, html in chapters:
            add(zf, root + fname, html)
        for _, fname, data, _ in images:
            add(zf, root + fname, data)


def main(out_dir):
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)

    rng = random.Random(1234)
    chapters = []
    for i in range(1, 5):
        body = "<h1>Chapter {}</h1>".format(i)
        body += "".join("<p>{}</p>".format(paragraph(rng, rng.randint(4, 8))) for _ in range(10 + 2 * i))
        chapters.append(("ch{}".format(i), "chapter{}.xhtml".format(i), xhtml("Chapter {}".format(i), body)))
    write_epub(out / "twin_test_book.epub", "The Keeper's Almanac", "FlickBook Twin", chapters)

    cw, ch = 160, 240
    cover = jpeg_gray(cw, ch, cover_pixel(cw, ch))
    inline = jpeg_gray(240, 120, lambda x, y: 255 if (x // 20 + y // 20) % 2 else 30)
    rng = random.Random(99)
    gallery = [
        ("cov", "cover.xhtml", xhtml("Cover", '<div><img src="images/cover.jpg" alt="cover"/></div>')),
        ("g1", "gallery1.xhtml", xhtml("Plates", "<h2>Plate One</h2><p>{}</p><img src=\"images/checker.jpg\"/>"
                                       "<p>{}</p>".format(paragraph(rng, 3), paragraph(rng, 5)))),
    ]
    write_epub(out / "twin_gallery.epub", "Twin Gallery", "FlickBook Twin", gallery,
               images=[("cover-image", "images/cover.jpg", cover, "image/jpeg"),
                       ("checker", "images/checker.jpg", inline, "image/jpeg")],
               cover_id="cover-image")

    progressive = jpeg_gray(1400, 2128, lambda x, y: 240 if (x // 160 + y // 240) % 2 else 30,
                            progressive=True)
    write_epub(out / "twin_progressive.epub", "Progressive Cover", "FlickBook Twin",
               [("cov", "cover.xhtml", xhtml("Cover", '<img src="images/cover.jpg"/>'))],
               images=[("cover-image", "images/cover.jpg", progressive, "image/jpeg")],
               cover_id="cover-image")

    edge = [
        ("e1", "edge1.xhtml", xhtml("Typography",
                                    "<h1>Typography &amp; Punctuation</h1>"
                                    "<p>\u201cCurly quotes,\u201d she said \u2014 and \u2018single ones\u2019 too\u2026</p>"
                                    "<p>Caf\u00e9, na\u00efve, fa\u00e7ade, \u00fcber, \u00e5ngstr\u00f6m.</p>"
                                    "<p>Some <b>bold</b>, <i>italic</i> and <span><em>nested <strong>inline</strong>"
                                    " tags</em></span> in one line.</p>"
                                    "<p>Supercalifragilisticexpialidocious" + "x" * 60 + " is a very long word.</p>")),
        ("e2", "edge2.xhtml", xhtml("Empty", "")),
        ("e3", "edge3.xhtml", xhtml("After", "<h2>After the empty chapter</h2><p>{}</p>".format(
            paragraph(random.Random(5), 6)))),
    ]
    write_epub(out / "twin_edge_cases.epub", "Edge Cases", "FlickBook Twin", edge)
    for p in sorted(out.glob("*.epub")):
        print(p, p.stat().st_size, "bytes")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else str(Path(__file__).resolve().parent.parent / "fixtures" / "books"))
