"""Totality -- the logo: the corona of a total eclipse round the dark disc, drawn per size.

    python Deploy/make_icon.py        -> Deploy/totality.ico (16 .. 256 px), totality.png, totality_512.png, totality.svg,
                                         Quest/res/mipmap-*/ic_launcher.png

The moon's disc, black, with the thin bright rim of the chromosphere, and the corona in streamers: long along the
equator, short at the poles, as the corona stands at a solar minimum (logo proposal A, chosen 28.09.2026). At 32
pixels and below half the streamers, thicker and shorter, since 48 lines would run into a grey ring. The same drawing
is the plugin's header (PluginEditor.cpp, drawLogo) and, in points of light, the Quest's panel (Quest/src/main.cpp,
addLogo). After Ephemeris' Deploy/make_icon.py; the .ico stays committed so the release build needs no Python.
"""
import math
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
BACK = (12, 12, 15, 255)
CORONA = (238, 214, 168)
DISC = (5, 5, 7, 255)


def streamers(disc, count, small=False):
    """The corona as (angle, inner radius, outer radius, opacity); the same numbers as drawLogo and addLogo."""
    out = []
    for i in range(count):
        a = 2.0 * math.pi * i / count
        w = 0.55 + 0.45 * math.sin(7.0 * a + 1.3) * math.sin(3.0 * a)
        e = abs(math.cos(a)) ** 3
        length = disc * (0.18 + 0.95 * e * (0.6 + 0.4 * w) + 0.12 * w) * (0.8 if small else 1.0)
        r0 = disc * 1.08
        out.append((a, r0, r0 + length, 0.45 + 0.5 * abs(math.cos(a))))
    return out


def layout(size):
    """small, disc radius / size, streamers, stroke width / disc radius."""
    small = size <= 32
    return small, (0.22 if small else 0.2), (24 if small else 48), (0.09 if small else 0.055)


def draw(size):
    s = size * 4                                   # drawn large, scaled down smooth
    small, frac, count, wfrac = layout(size)
    im = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    ImageDraw.Draw(im).rounded_rectangle((0, 0, s - 1, s - 1), radius=s // 5, fill=BACK)
    c = s / 2
    disc = s * frac
    width = max(6, round(disc * wfrac))           # never under 1.5 pixels in the icon
    for a, r0, r1, alpha in streamers(disc, count, small):
        ray = Image.new("RGBA", (s, s), (0, 0, 0, 0))
        ImageDraw.Draw(ray).line((c + r0 * math.cos(a), c + r0 * math.sin(a), c + r1 * math.cos(a), c + r1 * math.sin(a)),
                                 fill=CORONA + (int(255 * alpha),), width=width)
        im.alpha_composite(ray)
    d = ImageDraw.Draw(im)
    rim = disc * 1.06
    d.ellipse((c - rim, c - rim, c + rim, c + rim), fill=CORONA + (255,))
    d.ellipse((c - disc, c - disc, c + disc, c + disc), fill=DISC)
    return im.resize((size, size), Image.LANCZOS)


def svg(size=256):
    """The large drawing as vectors."""
    s, c = float(size), size / 2.0
    hexa = lambda col: "#%02x%02x%02x" % col[:3]
    small, frac, count, wfrac = layout(size)
    disc = s * frac
    out = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d">' % (size, size, size, size),
           '<rect x="0" y="0" width="%g" height="%g" rx="%g" fill="%s"/>' % (s, s, s / 5, hexa(BACK))]
    for a, r0, r1, alpha in streamers(disc, count, small):
        out.append('<line x1="%.2f" y1="%.2f" x2="%.2f" y2="%.2f" stroke="%s" stroke-width="%.2f" stroke-linecap="round" '
                   'stroke-opacity="%.3f"/>' % (c + r0 * math.cos(a), c + r0 * math.sin(a), c + r1 * math.cos(a),
                                                c + r1 * math.sin(a), hexa(CORONA), disc * wfrac, alpha))
    out.append('<circle cx="%g" cy="%g" r="%.3f" fill="%s"/>' % (c, c, disc * 1.06, hexa(CORONA)))
    out.append('<circle cx="%g" cy="%g" r="%.3f" fill="%s"/>' % (c, c, disc, hexa(DISC)))
    out.append('</svg>')
    return "\n".join(out) + "\n"


def main():
    sizes = [16, 24, 32, 48, 64, 128, 256]
    images = [draw(n) for n in sizes]
    images[-1].save(ROOT / "totality.ico", sizes=[(n, n) for n in sizes], append_images=images[:-1])
    images[-1].save(ROOT / "totality.png")
    draw(512).save(ROOT / "totality_512.png")
    (ROOT / "totality.svg").write_text(svg(), encoding="utf-8")
    # The Quest's launcher icon at Android's five densities.
    for density, n in (("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192)):
        draw(n).save(ROOT.parent / "Quest" / "res" / ("mipmap-" + density) / "ic_launcher.png")
    print("wrote", ROOT / "totality.ico", "the png and svg, and the Quest's launcher icons")


if __name__ == "__main__":
    main()
