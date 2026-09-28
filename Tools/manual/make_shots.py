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
TABS = ["Set", "Arrange", "Patterns", "Low End", "Drums", "Tones", "Dub", "Mixer", "Perform", "Export", "Style"]


def shot(exe, tab, path, full, extra=None):
    env = dict(os.environ, TOT_MUTE="1", TOT_SEED="4242", TOT_PLAY="1", TOT_SHOT_AT="402", TOT_TAB=str(tab), TOT_SHOT=path)
    env.pop("TOT_SET", None)
    if full:
        env["TOT_SHOT_FULL"] = "1"
    else:
        env.pop("TOT_SHOT_FULL", None)
    env.update(extra or {})
    if os.path.exists(path):
        os.remove(path)
    subprocess.run([exe], env=env, timeout=600, check=False)
    if not os.path.exists(path):
        sys.exit("no picture of tab %d (%s)" % (tab, TABS[tab]))
    print("%-10s %s" % (TABS[tab], os.path.relpath(path, ROOT)))


def main():
    exe = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build", "Plugin", "Totality_artefacts", "Release", "Standalone", "Totality.exe")
    out = os.path.join(ROOT, "docs", "screenshots")
    os.makedirs(out, exist_ok=True)
    for tab in range(len(TABS)):
        shot(exe, tab, os.path.join(out, "tab_%02d.png" % tab), True)
    shot(exe, 1, os.path.join(out, "set.png"), False, {"TOT_SET": "40", "TOT_SEED": "5", "TOT_SHOT_AT": "1400"})
    shot(exe, 2, os.path.join(ROOT, "docs", "screenshot.png"), False)


if __name__ == "__main__":
    main()
