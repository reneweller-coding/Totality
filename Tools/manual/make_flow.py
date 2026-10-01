"""Totality -- the signal flow as a picture: docs/flow.png, for the project's front page.

    python Tools/manual/make_flow.py

After Ephemeris' Tools/manual/make_flow.py (01.10.2026): every unit a box in the colour of its family (the panel's,
EditorTheme.cpp: gold sources, copper filters, sage envelopes and modulation, teal what moves by itself, steel blue the
rooms and the mix), the groups as frames, the buses as arrows. Drawn with Pillow at twice the size it is shown at, so
it stays sharp on a screen. What it shows is Deck.h's and Engine.h's signal flow.
"""
import math
import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
S = 2                                   # drawn at twice the size
W, H = 1400, 820                        # in the size it is shown at

BG = (13, 14, 18)
BOX = (22, 24, 30)
INK = (232, 226, 212)
DIM = (142, 146, 156)
SOURCE, FILTER, ENVELOPE, MOTION, SPACE = (230, 193, 120), (217, 130, 91), (159, 191, 111), (111, 184, 174), (111, 143, 184)
AMBER = (232, 169, 72)


def font(size, bold=False):
    names = ["segoeuib.ttf" if bold else "segoeui.ttf", "arialbd.ttf" if bold else "arial.ttf", "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"]
    for n in names:
        for d in ("C:/Windows/Fonts", "/usr/share/fonts/truetype/dejavu"):
            p = os.path.join(d, n)
            if os.path.exists(p):
                return ImageFont.truetype(p, size * S)
    return ImageFont.load_default()


img = Image.new("RGB", (W * S, H * S), BG)
d = ImageDraw.Draw(img)
F_BOX, F_SMALL, F_TITLE, F_NOTE = font(13), font(11), font(13, True), font(12)


def wrap(text, f, width):
    lines = []
    for para in text.split("\n"):
        line = ""
        for word in para.split(" "):
            test = (line + " " + word).strip()
            if d.textlength(test, font=f) <= width * S:
                line = test
            else:
                lines.append(line)
                line = word
        lines.append(line)
    return lines


def box(x, y, w, h, text, colour, f=None):
    f = f or F_BOX
    d.rounded_rectangle([x * S, y * S, (x + w) * S, (y + h) * S], radius=7 * S, fill=BOX, outline=colour, width=2 * S)
    lines = wrap(text, f, w - 16)
    lh = f.size * 1.3
    ty = y * S + (h * S - lh * len(lines)) / 2
    for ln in lines:
        tw = d.textlength(ln, font=f)
        d.text((x * S + (w * S - tw) / 2, ty), ln, font=f, fill=INK)
        ty += lh


def group(x, y, w, h, title, colour):
    d.rounded_rectangle([x * S, y * S, (x + w) * S, (y + h) * S], radius=10 * S, outline=colour, width=2 * S)
    d.text(((x + 14) * S, (y + 8) * S), title, font=F_TITLE, fill=colour)


def arrow(points, colour, dashed=False, head=True):
    pts = [(px * S, py * S) for px, py in points]
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        if dashed:
            n = max(1, int(((x1 - x0) ** 2 + (y1 - y0) ** 2) ** 0.5 / (12 * S)))
            for i in range(0, n, 2):
                a, b = i / n, min(1.0, (i + 1) / n)
                d.line([(x0 + (x1 - x0) * a, y0 + (y1 - y0) * a), (x0 + (x1 - x0) * b, y0 + (y1 - y0) * b)], fill=colour, width=2 * S)
        else:
            d.line([(x0, y0), (x1, y1)], fill=colour, width=2 * S)
    if not head:
        return
    (x0, y0), (x1, y1) = pts[-2], pts[-1]
    ang = math.atan2(y1 - y0, x1 - x0)
    size = 8 * S
    d.polygon([(x1, y1), (x1 - size * math.cos(ang - 0.45), y1 - size * math.sin(ang - 0.45)),
               (x1 - size * math.cos(ang + 0.45), y1 - size * math.sin(ang + 0.45))], fill=colour)


def note(x, y, text, colour=DIM, f=None):
    d.text((x * S, y * S), text, font=f or F_NOTE, fill=colour)


# The inputs: what plays the decks.
ins = [("Composer: Arc, Peak, Endless from four style profiles; a 32-bar block at a time, judged against a hypnosis corridor", MOTION),
       ("Rack: the kit's and the voices' patterns, euclidean figures, the track's figure", SOURCE),
       ("Two hands: the automation on the knobs; the DJ's hand on EQ, filter and echo", MOTION),
       ("Perform: mutes, master filter, echo throw, kills; a MIDI keyboard on a voice", AMBER),
       ("Cues out: WAV markers, JSON, OSC to a visualiser", SPACE)]
x = 20
for t, c in ins:
    box(x, 16, 262, 56, t, c, F_SMALL)
    x += 274

# A deck's voices.
group(20, 96, 660, 486, "DECK  x3  --  the whole instrument for one deck's tracks", SOURCE)
box(36, 128, 628, 52, "Kick: three engines (Sweep, Resonator, 909) and a 909 top layer; the rumble continues its phase "
    "under its own hall", SOURCE)
box(36, 190, 628, 36, "Sub bass, its notes started in the kick's phase; one owner of the band under the split", SOURCE)
box(36, 236, 628, 52, "Kit: twelve lanes -- noise, the 909's metal oscillators, modal, tone, FM -- on a hats bus and a "
    "perc bus", SOURCE)
box(36, 298, 628, 36, "Ping: FM through a low-pass gate", SOURCE)
box(36, 344, 628, 66, "Bass synth and 303 line through ten filters solved as their circuits -- Moog ladder, Prophet, Juno, "
    "Oberheim SEM, Xpander, diode ladder, Korg35, Polivoks, EDP Wasp, comb -- high-passed at 100 Hz", FILTER)
box(36, 420, 628, 52, "Dub chord with its chain (tape echo, springs, plate), drone, texture and a granular cloud", SOURCE)
box(36, 482, 628, 36, "The motion: slow free filter LFOs and a hat volume sine, after the research", ENVELOPE)
box(36, 528, 628, 40, "The sounds: 1024 factory presets per synth in sixteen groups, chosen for every track by its style", MOTION)

# More of a set.
group(20, 600, 660, 202, "A SET", MOTION)
box(36, 632, 628, 52, "Set composer: seven dramaturgies, up to twelve hours; neighbouring Camelot keys; never two tracks "
    "of a kind in a row; the tempo drifting at most 1 BPM a track", MOTION)
box(36, 694, 628, 52, "Blends with the highs first and the mids over the last 16 bars, a hard bass swap on a 32-bar line "
    "through the isolator; a third deck that borrows and teases", MOTION)
note(40, 760, "every part on its own seed stream: rerolled alone, saved as a .totset")

# Inputs into the deck.
arrow([(151, 72), (151, 96)], MOTION)
arrow([(425, 72), (425, 96)], SOURCE)
arrow([(699, 72), (699, 84), (560, 84), (560, 96)], MOTION, dashed=True)
arrow([(973, 72), (973, 90), (630, 90), (630, 96)], AMBER, dashed=True)

# The deck's buses.
group(720, 96, 660, 250, "A DECK'S BUSES", SPACE)
box(736, 128, 628, 36, "kick + rumble + hats bus + perc bus  ->  drum bus (three saturation stages)", SPACE)
box(736, 172, 628, 36, "chord bus + drone  ->  multiband duck, triggered by the kick's events", MOTION)
box(736, 216, 628, 52, "sends: room, dub echo + springs, plate  ->  returns (and the cloud)  ->  multiband duck", SPACE)
box(736, 278, 628, 52, "sum  ->  group high pass  ->  tilt  ->  glue (35 % parallel)  ->  the Leveler's trim", SPACE)
arrow([(680, 330), (700, 330), (700, 146), (736, 146)], SOURCE)

# The DJ mixer.
group(720, 364, 660, 126, "DJ MIXER", SPACE)
box(736, 396, 628, 36, "each deck: fader  ->  isolator (200 Hz / 2.5 kHz, a kill each)  ->  filter", SPACE)
box(736, 440, 628, 36, "fx send  ->  tempo echo + hall", SPACE)
arrow([(1050, 346), (1050, 364)], SPACE)
note(1062, 348, "three decks", SPACE, F_SMALL)

# The master.
group(720, 508, 660, 210, "MASTER", SPACE)
box(736, 540, 628, 52, "mono under Mono Below  ->  level  ->  [vinyl cut]  ->  clipper at four times the rate  ->  "
    "true-peak limiter", SPACE)
box(736, 602, 628, 100, "The Leveler: every kit lane and tonal voice set against the kick, in a window around the research's "
    "fader levels; the room taken back where the mids would stand over the references'; every track's loudest part "
    "brought to its style's level. Balance, width and loudness fitted to 30 measured reference recordings", AMBER)
arrow([(1050, 490), (1050, 508)], SPACE)
box(900, 736, 464, 48, "Out: stereo -- WAV with cue markers, stems that sum to the mix, MIDI with the tempo map, DJ loops",
    SPACE, F_SMALL)
arrow([(1132, 718), (1132, 736)], SPACE)
note(720, 792, "Deterministic: a track renders offline to the bit as it plays, for any block size.", DIM)

out = os.path.join(ROOT, "docs", "flow.png")
img.save(out)
print("wrote", os.path.relpath(out, ROOT), img.size)
