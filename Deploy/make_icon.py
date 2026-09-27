"""Umbra -- the logo: the Eclipse view of the Patterns page, drawn per size.

    python Deploy/make_icon.py        -> Deploy/umbra.ico (16 .. 256 px), umbra.png, umbra_512.png, umbra.svg

A dark disc -- the kick, the umbra -- in front of its corona, a bright bead where the rim catches the light (the
diamond ring), and around it the rings of the layers with their onsets; at 16 and 24 pixels only the disc and its
corona, since the rings would turn into a speckle. The same drawing is the plugin's header (PluginEditor.cpp,
drawLogo). After Ephemeris' Deploy/make_icon.py; the .ico stays committed so the release build needs no Python.
"""
import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent
BACK = (12, 12, 15, 255)
CORONA = (238, 214, 168)
DISC = (5, 5, 7, 255)
RING = (120, 112, 104, 85)
ONSET = (214, 85, 63, 255)


def draw(size):
    s = size * 4                                   # drawn large, scaled down smooth
    im = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, s - 1, s - 1), radius=s // 5, fill=BACK)
    c = s / 2
    disc = s * (0.2 if size > 24 else 0.24)
    # The corona: a soft glow, drawn on its own layer and blurred.
    glow = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    g = ImageDraw.Draw(glow)
    for k in range(12, 0, -1):
        r = disc * (1.0 + 0.06 * k)
        a = int(200 * (1.0 - k / 13.0) ** 1.6)
        g.ellipse((c - r, c - r, c + r, c + r), fill=CORONA + (a,))
    glow = glow.filter(ImageFilter.GaussianBlur(s / 60))
    im.alpha_composite(glow)
    d = ImageDraw.Draw(im)
    if size > 24:
        rings = 2 if size <= 48 else 3
        width = max(3, s // 90)
        for k in range(rings):
            r = disc * 1.55 + (s * 0.44 - disc * 1.55) * k / max(1, rings - 1)
            d.ellipse((c - r, c - r, c + r, c + r), outline=RING, width=width)
            steps = (16, 12, 7)[k]                  # a sixteenth ring, a polymeter of twelve, a slipping seven
            for i in range(0, steps, 3 if k == 0 else 2):
                a = -math.pi / 2 + 2 * math.pi * i / steps + 0.4 * k
                pr = max(s * 0.018, width * 1.4)
                x, y = c + r * math.cos(a), c + r * math.sin(a)
                d.ellipse((x - pr, y - pr, x + pr, y + pr), fill=ONSET if i == 0 else CORONA + (200,))
    d.ellipse((c - disc, c - disc, c + disc, c + disc), fill=DISC)
    # The diamond ring: a bead of light on the rim, upper right.
    a = -math.pi / 4
    br = disc * 0.13
    bx, by = c + disc * math.cos(a), c + disc * math.sin(a)
    bead = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    ImageDraw.Draw(bead).ellipse((bx - br, by - br, bx + br, by + br), fill=(255, 246, 226, 255))
    im.alpha_composite(bead.filter(ImageFilter.GaussianBlur(br / 3)))
    return im.resize((size, size), Image.LANCZOS)


def svg(size=256):
    """The large drawing as vectors (the glow as a radial gradient)."""
    s, c = float(size), size / 2.0
    hexa = lambda col: "#%02x%02x%02x" % col[:3]
    disc = s * 0.2
    out = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d">' % (size, size, size, size),
           '<defs><radialGradient id="corona" cx="50%%" cy="50%%" r="50%%">'
           '<stop offset="%.3f" stop-color="%s" stop-opacity="0.9"/><stop offset="1" stop-color="%s" stop-opacity="0"/>'
           '</radialGradient></defs>' % (disc / (disc * 1.75), hexa(CORONA), hexa(CORONA)),
           '<rect x="0" y="0" width="%g" height="%g" rx="%g" fill="%s"/>' % (s, s, s / 5, hexa(BACK)),
           '<circle cx="%g" cy="%g" r="%.3f" fill="url(#corona)"/>' % (c, c, disc * 1.75)]
    width = max(1.0, s / 90.0)
    for k in range(3):
        r = disc * 1.55 + (s * 0.44 - disc * 1.55) * k / 2
        out.append('<circle cx="%g" cy="%g" r="%.3f" fill="none" stroke="%s" stroke-opacity="%.3f" stroke-width="%g"/>'
                   % (c, c, r, hexa(RING), RING[3] / 255.0, width))
        steps = (16, 12, 7)[k]
        for i in range(0, steps, 3 if k == 0 else 2):
            a = -math.pi / 2 + 2 * math.pi * i / steps + 0.4 * k
            pr = max(s * 0.018, width * 1.4)
            out.append('<circle cx="%.3f" cy="%.3f" r="%.3f" fill="%s"/>'
                       % (c + r * math.cos(a), c + r * math.sin(a), pr, hexa(ONSET) if i == 0 else hexa(CORONA)))
    out.append('<circle cx="%g" cy="%g" r="%.3f" fill="%s"/>' % (c, c, disc, hexa(DISC)))
    a = -math.pi / 4
    out.append('<circle cx="%.3f" cy="%.3f" r="%.3f" fill="#fff6e2"/>' % (c + disc * math.cos(a), c + disc * math.sin(a), disc * 0.11))
    out.append('</svg>')
    return "\n".join(out) + "\n"


def main():
    sizes = [16, 24, 32, 48, 64, 128, 256]
    images = [draw(n) for n in sizes]
    images[-1].save(ROOT / "umbra.ico", sizes=[(n, n) for n in sizes], append_images=images[:-1])
    images[-1].save(ROOT / "umbra.png")
    draw(512).save(ROOT / "umbra_512.png")
    (ROOT / "umbra.svg").write_text(svg(), encoding="utf-8")
    print("wrote", ROOT / "umbra.ico", "and the png and svg")


if __name__ == "__main__":
    main()
