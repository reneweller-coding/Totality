"""Totality's factory sets (17.10.2026): 256 DJ sets to begin a night with -> Core/src/FactorySetsData.inc.

Each of the four styles goes through the seven arcs (set.dramaturgy), nine sets an arc, and every style has one night of
twelve hours more: 4 x 7 x 9 + 4 = 256. A set is what a player would save after an evening of curating -- a seed and the
set's knobs, the rest at its defaults (the composer picks every track's sounds as always):

- its length, from the lengths the arc is played at (a warm-up an hour or two, a Sunday three or four, a marathon
  a night);
- the styles: two sets in three follow the set's energy along the ladder Dub, Hypnotic, Ostgut, Raw Peak
  (set.journey), the third keeps its style; a Flat set always keeps it (its energy hardly moves);
- a track's time, the blend (16 or 32 bars), the live loops, the FX breaks, the DJ's hand -- drawn within what fits the
  arc (a peak blends short and changes often, a Sunday lets a track run);
- now and then the pull towards Dub or Hypnotic (compose.dub_share, compose.hypnotic_share).

Every set a name of its own (two words of the night and the machine), drawn without repeats. The draws are seeded, so
the bank is the same every time the script runs.

How long each set plays is measured by composing it, which the render app does for every row of its --catalog
(half a second a set): build, ask it, and run the script again with its answer -- the lengths go into the table, so
the app and Heliacal list the bank at once instead of composing 256 sets first.

    python Tools/sets/make_sets.py
    bin/msvc/TotalitySets.exe --catalog > work/sets_catalog.json
    python Tools/sets/make_sets.py --lengths work/sets_catalog.json
"""
import json
import sys
import pathlib
import random

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "Core" / "src" / "FactorySetsData.inc"

STYLES = ["Hypnotic", "Ostgut", "Dub", "Raw Peak"]
# The arcs (DramaturgyNames) with the lengths they are played at, minutes, and their knobs' ranges: a track's time,
# the share of 32-bar blends, the DJ's hand.
ARCS = {
    "Warm-up": dict(lengths=[60, 90, 120], track=(3.5, 5.0), long_blend=0.7, hand=(0.2, 0.5)),
    "Peak": dict(lengths=[60, 90, 120], track=(2.8, 3.8), long_blend=0.3, hand=(0.5, 0.9)),
    "Closing": dict(lengths=[60, 90, 120], track=(3.2, 4.5), long_blend=0.6, hand=(0.4, 0.7)),
    "Sunday": dict(lengths=[120, 180, 240], track=(3.8, 5.5), long_blend=0.8, hand=(0.3, 0.6)),
    "Flat": dict(lengths=[60, 90, 120], track=(3.0, 4.5), long_blend=0.5, hand=(0.3, 0.6)),
    "Cruise": dict(lengths=[90, 120, 180], track=(3.5, 5.0), long_blend=0.6, hand=(0.3, 0.6)),
    "Marathon": dict(lengths=[360, 480, 600], track=(3.0, 4.5), long_blend=0.5, hand=(0.4, 0.8)),
}
PER_ARC = 9

FIRST = ["Concrete", "Iron", "Sodium", "Basement", "Hollow", "Static", "Velvet", "Midnight", "Furnace", "Tunnel",
         "Steel", "Chrome", "Shadow", "Grey", "Signal", "Ritual", "Pressure", "Low", "Smoke", "Ashen", "Cobalt", "Neon",
         "Rust", "Graphite", "Turbine", "Vault", "Bunker", "Warehouse", "Black", "Silent", "Endless", "Deep", "Raw",
         "Cold", "Electric", "Mercury", "Carbon", "Tungsten", "Cellar", "Strobe", "Fog", "Ozone", "Granite", "Copper",
         "Fluorescent", "Sublevel", "Night", "Distant"]
SECOND = ["Hours", "Hall", "Liturgy", "Machine", "Vigil", "Floor", "Current", "Corridor", "Shift", "Sermon", "Circuit",
          "Procession", "Drift", "Rite", "Session", "Chamber", "Sequence", "Heartbeat", "Dawn", "Descent",
          "Transmission", "Loop", "Mass", "Spiral", "System", "Motion", "Gravity", "Frequency", "Orbit", "Momentum",
          "Undertow", "Assembly", "Pulse", "Tide", "Cathedral", "Doctrine", "Discipline", "Patrol", "Hymn",
          "Mechanism", "Horizon", "Labyrinth", "Engine", "Furnace", "Signal", "Pressure", "Static", "Ritual"]


def length_text(minutes):
    """A set's length as people say it."""
    if minutes < 60:
        return f"{minutes} min"
    if minutes % 60:
        return f"{minutes} min"
    h = minutes // 60
    return "1 hour" if h == 1 else f"{h} hours"


def names(rng, count):
    """Two words each, never the same word twice in a name, never a name twice."""
    out, seen = [], set()
    while len(out) < count:
        a, b = rng.choice(FIRST), rng.choice(SECOND)
        if a == b or a + " " + b in seen:
            continue
        seen.add(a + " " + b)
        out.append(a + " " + b)
    return out


def sets():
    """The 256 sets: (name, style, arc, note, text)."""
    rng = random.Random(17102026)
    plan = []
    for style in STYLES:
        for arc, a in ARCS.items():
            for k in range(PER_ARC):
                plan.append((style, arc, a["lengths"][k % 3], k % 3 != 2 and arc != "Flat"))
        plan.append((style, "Marathon", 720, True))   # a night of twelve hours
    out = []
    for (style, arc, minutes, wander), name in zip(plan, names(rng, len(plan))):
        a = ARCS[arc]
        knobs = [
            ("compose.style", style),
            ("set.dramaturgy", arc),
            ("set.journey", "Wander" if wander else "Stay"),
            ("set.minutes", f"{minutes}"),
            ("set.track_minutes", f"{rng.uniform(*a['track']):.2f}"),
            ("set.blend", "32 bars" if rng.random() < a["long_blend"] else "16 bars"),
            ("set.loops", f"{rng.uniform(0.25, 0.8):.2f}"),
            ("set.fx_breaks", f"{rng.uniform(0.1, 0.55):.2f}"),
            ("set.dj_hand", f"{rng.uniform(*a['hand']):.2f}"),
        ]
        if rng.random() < 0.3 and style != "Dub":
            knobs.append(("compose.dub_share", f"{rng.uniform(0.1, 0.45):.2f}"))
        if rng.random() < 0.3 and style != "Hypnotic":
            knobs.append(("compose.hypnotic_share", f"{rng.uniform(0.1, 0.45):.2f}"))
        seed = rng.randrange(1, 2 ** 31)
        text = ["# Totality set", f"seed={seed}", "minutes=0", f"set={minutes}"]
        text += [f"param {k}={v}" for k, v in knobs]
        note = length_text(minutes) + (", the styles following the energy" if wander else f", {style} all night")
        out.append((name, style, arc, note, "\n".join(text) + "\n"))
    return out


def c_string(s):
    """A C string literal, a line of the set to a line of the source."""
    lines = s.split("\n")
    parts = [line.replace("\\", "\\\\").replace('"', '\\"') + "\\n" for line in lines[:-1]]
    return "\n".join(f'      "{p}"' for p in parts)


def main():
    rows = sets()
    lengths = {}
    if "--lengths" in sys.argv:   # the render app's catalog: the factory sets' rows with their lengths
        catalog = json.load(open(sys.argv[sys.argv.index("--lengths") + 1], encoding="utf-8"))
        lengths = {i["name"]: i["length"] for i in catalog["items"] if i["kind"] == "Factory set"}
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("// The factory's DJ sets (FactorySets.h), written by Tools/sets/make_sets.py -- not by hand.\n")
        for name, style, arc, note, text in rows:
            f.write(f'    {{ "{name}", "{style}", "{arc}", "{note}",\n{c_string(text)},\n'
                    f'      {lengths.get(name, 0.0):.1f} }},\n')
    measured = sum(1 for row in rows if row[0] in lengths)
    print(f"{len(rows)} sets, {measured} with their lengths -> {OUT}")


if __name__ == "__main__":
    main()
