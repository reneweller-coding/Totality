"""Totality -- the manual's screenshots: every tab as a whole page, from the standalone, muted.

    python Tools/manual/make_shots.py [path\\to\\Totality.exe]   -> docs/screenshots/tab_NN.png

Each tab is its own run of the standalone in the screenshot mode: the seed 4242, the playhead at beat 402 (bar 101),
and TOT_SHOT_FULL, which grows the window until nothing of the page in front scrolls, so every knob is in the picture.
The runs are muted (TOT_SHOT forces it). Besides: docs/screenshots/set.png, the Arrange tab of a 40-minute set, and
docs/screenshot.png, the Patterns tab at the window's usual size, the picture on the project's front page. After
Ephemeris' make_shots.py.
"""
import os
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
# (name, tab, small tab or None): the pages in the order of the tabs (01.10.2026: a tab of several modules a picture per
# small tab); chapters.txt numbers its pictures by this list.
PAGES = [("Set", 0, None), ("Arrange", 1, None), ("Patterns", 2, None),
         ("Kick", 3, 0), ("Rumble", 3, 1), ("Sub", 3, 2), ("Drums", 4, None),
         ("Ping", 5, 0), ("Bass", 5, 1), ("Acid", 5, 2), ("Chord", 5, 3), ("Drone", 5, 4), ("Texture", 5, 5),
         ("Dub", 6, 0), ("Space", 6, 1), ("Cloud", 6, 2),
         ("Console", 7, 0), ("Buses and Master", 7, 1), ("Decks", 7, 2),
         ("Perform", 8, None), ("Export", 9, None), ("Style", 10, None)]
ARRANGE, PATTERNS = 1, 2


def shot(exe, page, path, full, extra=None):
    name, tab, small = PAGES[page]
    env = dict(os.environ, TOT_MUTE="1", TOT_SEED="4242", TOT_PLAY="1", TOT_SHOT_AT="402", TOT_TAB=str(tab), TOT_SHOT=path)
    env.pop("TOT_SET", None)
    env.pop("TOT_SUBTAB", None)
    if small is not None:
        env["TOT_SUBTAB"] = str(small)
    if full:
        env["TOT_SHOT_FULL"] = "1"
        env["FAMILY_NO_SECTIONS"] = "1"   # every page whole in its picture, its sections at once (Frame.h, 01.10.2026)
    else:
        env.pop("TOT_SHOT_FULL", None)
        env.pop("FAMILY_NO_SECTIONS", None)
    env.update(extra or {})
    if os.path.exists(path):
        os.remove(path)
    subprocess.run([exe], env=env, timeout=600, check=False)
    if not os.path.exists(path):
        sys.exit("no picture of page %d (%s)" % (page, name))
    print("%-16s %s" % (name, os.path.relpath(path, ROOT)))


def check_distinct(out):
    """Exits when two small tabs of one tab came out as the same page (02.10.2026).

    A small tab the screenshot mode did not reach leaves the page before it in front, and its picture is that page
    again (01.10.2026: the Mixer's "Buses and Master" and "Decks" showed the Console). The pages of one synth module
    look alike by design, so the check reads the row of small tabs, where the lit one moves: measured in the band
    y 200..270, a missed small tab differs from the page before it in 0 to 109 pixels (by more than 24 levels), two
    different small tabs in 4091 to 6007. Under 1000 is the same small tab.
    """
    from PIL import Image, ImageChops
    same = []
    for i in range(len(PAGES)):
        for j in range(i + 1, len(PAGES)):
            if PAGES[i][2] is None or PAGES[i][1] != PAGES[j][1]:
                continue
            a, b = (Image.open(os.path.join(out, "tab_%02d.png" % k)).convert("L").crop((0, 200, 1280, 270)) for k in (i, j))
            if sum(ImageChops.difference(a, b).histogram()[25:]) < 1000:
                same.append("%s (tab_%02d) and %s (tab_%02d)" % (PAGES[i][0], i, PAGES[j][0], j))
    if same:
        sys.exit("the same small tab twice: " + "; ".join(same))
    print("every small tab its own picture")


def main():
    exe = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "bin", "msvc", "Totality.exe")
    out = os.path.join(ROOT, "docs", "screenshots")
    os.makedirs(out, exist_ok=True)
    for page in range(len(PAGES)):
        shot(exe, page, os.path.join(out, "tab_%02d.png" % page), True)
    shot(exe, ARRANGE, os.path.join(out, "set.png"), False, {"TOT_SET": "40", "TOT_SEED": "5", "TOT_SHOT_AT": "1400"})
    shot(exe, PATTERNS, os.path.join(ROOT, "docs", "screenshot.png"), False)
    check_distinct(out)


if __name__ == "__main__":
    main()
