"""Umbra -- the picture GitHub shows when the project is linked: docs/social-preview.png (1280 x 640).

    python Tools/manual/make_preview.py

The logo, the name, one sentence, and the Patterns tab (docs/screenshot.png, from make_shots.py). GitHub takes it by
hand: Settings, Social preview. After Ephemeris' make_preview.py.
"""
import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
W, H = 1280, 640


def font(name, size):
    try:
        return ImageFont.truetype(os.path.join("C:/Windows/Fonts", name), size)
    except OSError:
        return ImageFont.load_default()


img = Image.new("RGB", (W, H), (11, 11, 14))
d = ImageDraw.Draw(img)
shot = Image.open(os.path.join(ROOT, "docs", "screenshot.png")).convert("RGB")
crop = shot.crop((0, 0, 1180, 760)).resize((780, 502), Image.LANCZOS)
x0, y0 = W - 780 - 30, (H - 502) // 2
img.paste(crop, (x0, y0))
d.rectangle([x0 - 1, y0 - 1, x0 + 780, y0 + 502], outline=(43, 45, 54), width=2)
logo = Image.open(os.path.join(ROOT, "Deploy", "umbra_512.png")).convert("RGBA").resize((150, 150), Image.LANCZOS)
img.paste(logo, (60, 80), logo)
d.text((60, 252), "UMBRA", font=font("segoeuib.ttf", 52), fill=(238, 214, 168))
y = 330
for line in ("A generator of hypnotic Berlin", "techno: tracks and DJ sets,", "composed, mixed and played", "by the program."):
    d.text((62, y), line, font=font("segoeui.ttf", 26), fill=(238, 228, 208))
    y += 36
for line in ("VST3 and standalone for Windows,", "and an app for Meta Quest"):
    d.text((62, y + 18), line, font=font("segoeui.ttf", 19), fill=(143, 143, 152))
    y += 26
out = os.path.join(ROOT, "docs", "social-preview.png")
img.save(out)
print("wrote", os.path.relpath(out, ROOT), img.size)
