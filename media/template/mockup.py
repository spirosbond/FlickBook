#!/usr/bin/env python3
"""Put FlickBook screenshots inside the 3D-printed case.

    python media/template/mockup.py SCREEN.png [-o OUT.png] [--eink] [--bg "#ffffff"] [--width 900]
    python media/template/mockup.py SCREEN.png --photo [-o OUT.jpg]
    python media/template/mockup.py --build      # regenerate flickbook-frame.png (needs opencv)

flickbook-frame.png is a front-on, transparent-background image of the case
with a transparent screen window at SCREEN_BOX. The window is exactly 2x the
758x1024 portrait panel, so any screenshot can also be placed by hand in an
image editor on a layer beneath the frame.
"""
import argparse
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

HERE = Path(__file__).resolve().parent
FRAME_PATH = HERE / "flickbook-frame.png"
PHOTO_PATH = HERE.parent / "home-frontview.jpg"  # in-context shot used by --photo
SOURCE_PATH = HERE / "PXL_20261025_191946010.jpg"  # top-down shot used by --build

SCREEN_BOX = (211, 304, 1516, 2048)  # x, y, w, h inside flickbook-frame.png
FRAME_SIZE = (1937, 2770)
# Screen window corners (TL, TR, BR, BL) in home-frontview.jpg.
PHOTO_SCREEN_QUAD = [(982.5, 1113.2), (2674.1, 1109.7), (2484.5, 3288.6), (1022.3, 3089.4)]

INK, PAPER = (38, 38, 40), (226, 226, 220)


def load_screen(path, size, eink):
    img = Image.open(path)
    if eink:
        lut = [round(INK[c] + (PAPER[c] - INK[c]) * v / 255) for c in range(3) for v in range(256)]
        img = img.convert("L").convert("RGB").point(lut)
    else:
        img = img.convert("RGB")
    if img.size == size:
        return img
    scale = min(size[0] / img.width, size[1] / img.height)
    fitted = img.resize(
        (round(img.width * scale), round(img.height * scale)),
        Image.NEAREST if scale == int(scale) else Image.LANCZOS,
    )
    out = Image.new("RGB", size, PAPER if eink else (255, 255, 255))
    out.paste(fitted, ((size[0] - fitted.width) // 2, (size[1] - fitted.height) // 2))
    return out


def compose_flat(screen_path, eink, bg, shadow):
    frame = Image.open(FRAME_PATH).convert("RGBA")
    x, y, w, h = SCREEN_BOX
    canvas = Image.new("RGBA", frame.size, bg or (0, 0, 0, 0))
    canvas.paste(load_screen(screen_path, (w, h), eink), (x, y))

    if shadow:
        # Soft shadow cast by the bezel lip onto the recessed screen.
        a = frame.getchannel("A")
        a = ImageChops.offset(a, 4, 6).filter(ImageFilter.GaussianBlur(9))
        a = a.point(lambda v: int(v * 0.55))
        clip = Image.new("L", frame.size, 0)
        clip.paste(255, (x, y, x + w, y + h))
        canvas.paste((0, 0, 0, 255), (0, 0), ImageChops.multiply(a, clip))

    canvas.alpha_composite(frame)
    return canvas


def _perspective_coeffs(dst_quad, src_quad):
    """Coefficients for Image.transform mapping output (dst) pixels to input (src)."""
    import numpy as np

    rows, rhs = [], []
    for (X, Y), (u, v) in zip(dst_quad, src_quad):
        rows.append([X, Y, 1, 0, 0, 0, -u * X, -u * Y]); rhs.append(u)
        rows.append([0, 0, 0, X, Y, 1, -v * X, -v * Y]); rhs.append(v)
    return np.linalg.solve(np.array(rows, float), np.array(rhs, float)).tolist()


def compose_photo(screen_path):
    photo = Image.open(PHOTO_PATH).convert("RGB")
    w, h = SCREEN_BOX[2:]
    screen = load_screen(screen_path, (w, h), eink=True)
    coeffs = _perspective_coeffs(PHOTO_SCREEN_QUAD, [(0, 0), (w, 0), (w, h), (0, h)])
    warped = screen.transform(photo.size, Image.PERSPECTIVE, coeffs, Image.BICUBIC)
    mask = Image.new("L", (w, h), 255).transform(photo.size, Image.PERSPECTIVE, coeffs, Image.BILINEAR)
    # Match the dim, slightly soft lighting of the photo.
    warped = warped.point(lambda v: int(v * 0.78)).filter(ImageFilter.GaussianBlur(0.8))
    photo.paste(warped, (0, 0), mask.filter(ImageFilter.GaussianBlur(1)))
    return photo


def _find_quads(im):
    """Corners (TL, TR, BR, BL) of the case outline and of its screen window."""
    import cv2
    import numpy as np

    hsv = cv2.cvtColor(im, cv2.COLOR_BGR2HSV)
    g = cv2.inRange(hsv, (35, 45, 60), (95, 255, 255))
    g = cv2.morphologyEx(g, cv2.MORPH_CLOSE, np.ones((25, 25), np.uint8))
    g = cv2.morphologyEx(g, cv2.MORPH_OPEN, np.ones((9, 9), np.uint8))
    _, lab, st, _ = cv2.connectedComponentsWithStats(g)
    case = ((lab == 1 + np.argmax(st[1:, 4])) * 255).astype(np.uint8)
    contours, _ = cv2.findContours(case, cv2.RETR_CCOMP, cv2.CHAIN_APPROX_NONE)

    quads = []
    for cnt in sorted(contours, key=cv2.contourArea, reverse=True)[:2]:
        # Fit a line to the middle of each side and intersect neighbours.
        pts = cnt.reshape(-1, 2).astype(float)
        approx = cv2.approxPolyDP(cnt, 0.02 * cv2.arcLength(cnt, True), True).reshape(-1, 2).astype(float)
        lines = []
        for k in range(4):
            a, b = approx[k], approx[(k + 1) % 4]
            L = np.linalg.norm(b - a); u = (b - a) / L; n = np.array([-u[1], u[0]])
            t, d = (pts - a) @ u, np.abs((pts - a) @ n)
            sel = pts[(t > 0.15 * L) & (t < 0.85 * L) & (d < 40)].astype(np.float32)
            vx, vy, x0, y0 = cv2.fitLine(sel, cv2.DIST_HUBER, 0, 0.01, 0.01).ravel()
            lines.append((np.array([x0, y0]), np.array([vx, vy])))
        c = []
        for k in range(4):
            (p1, d1), (p2, d2) = lines[k - 1], lines[k]
            c.append(p1 + np.linalg.solve(np.array([d1, -d2]).T, p2 - p1)[0] * d1)
        c = np.array(c)
        quads.append([c[np.argmin(c.sum(1))], c[np.argmin(c[:, 1] - c[:, 0])],
                      c[np.argmax(c.sum(1))], c[np.argmax(c[:, 1] - c[:, 0])]])
    return np.array(quads, np.float32)


def build_template():
    """Rectify the top-down source photo into flickbook-frame.png."""
    import cv2
    import numpy as np

    im = cv2.imread(str(SOURCE_PATH))
    outer, inner = _find_quads(im)
    print("case quad:  ", outer.round(1).tolist())
    print("screen quad:", inner.round(1).tolist())

    # One homography for both outlines, so the case and the window both come
    # out as true rectangles (the panel sits very slightly skewed in the case).
    x, y, w, h = SCREEN_BOX
    W, H = FRAME_SIZE
    dst = np.float32([[0, 0], [W, 0], [W, H], [0, H],
                      [x, y], [x + w, y], [x + w, y + h], [x, y + h]])
    M, _ = cv2.findHomography(np.vstack([outer, inner]), dst, 0)
    rect = cv2.warpPerspective(im, M, FRAME_SIZE, flags=cv2.INTER_LANCZOS4, borderMode=cv2.BORDER_REPLICATE)
    rgb = Image.fromarray(cv2.cvtColor(rect, cv2.COLOR_BGR2RGB))

    ss = 4  # supersampled, anti-aliased alpha
    alpha = Image.new("L", (W * ss, H * ss), 0)
    d = ImageDraw.Draw(alpha)
    # Small inset/radius measured from the case silhouette so no background bleeds in.
    i = 4
    d.rounded_rectangle((i * ss, i * ss, (W - i) * ss - 1, (H - i) * ss - 1), radius=70 * ss, fill=255)
    d.rounded_rectangle((x * ss, y * ss, (x + w) * ss - 1, (y + h) * ss - 1), radius=8 * ss, fill=0)
    rgb.putalpha(alpha.resize(FRAME_SIZE, Image.LANCZOS))
    rgb.save(FRAME_PATH, optimize=True)
    print("wrote", FRAME_PATH)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("screen", nargs="?", help="screenshot (ideally 758x1024 portrait)")
    p.add_argument("-o", "--out", help="output file (default: <screen>-mockup.png/.jpg)")
    p.add_argument("--eink", action="store_true", help="render with a grey e-paper tone")
    p.add_argument("--bg", help="background colour instead of transparent, e.g. '#ffffff'")
    p.add_argument("--no-shadow", action="store_true", help="skip the bezel shadow on the screen")
    p.add_argument("--width", type=int, help="resize the result to this width")
    p.add_argument("--photo", action="store_true", help="place the screen into the original photo")
    p.add_argument("--build", action="store_true", help="regenerate flickbook-frame.png")
    a = p.parse_args()

    if a.build:
        return build_template()
    if not a.screen:
        p.error("screen image required")

    img = compose_photo(a.screen) if a.photo else compose_flat(a.screen, a.eink, a.bg, not a.no_shadow)
    if a.width:
        img = img.resize((a.width, round(img.height * a.width / img.width)), Image.LANCZOS)
    ext = ".jpg" if a.photo else ".png"
    out = Path(a.out) if a.out else Path(a.screen).with_name(Path(a.screen).stem + "-mockup" + ext)
    if out.suffix.lower() in (".jpg", ".jpeg"):
        img.convert("RGB").save(out, quality=92)
    else:
        img.save(out, optimize=True)
    print("wrote", out)


if __name__ == "__main__":
    main()
