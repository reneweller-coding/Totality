"""
eval_report.py -- Totality's evaluation (PLAN 13.5, Dok. 8.9): what the composer aimed at and what came out, per track,
against the references.

usage:
  python Tools/eval_report.py --score track.json --wav track.wav [--score ... --wav ...] --out docs/eval/report.md
  python Tools/eval_report.py --score set.json --wav set.wav --out docs/eval/set.md      (a set: cut at its swaps)

The score JSON is tot_render's --score-json (tracks, notes, operations, tempo). Per track:

  symbolic   syncopation after Longuet-Higgins and Lee (1984) per voice, polyphonic syncopation (after Witek et al.
             2014, simplified: a note in one stream left for a stronger place that another stream takes), Gomez-Marin's
             descriptors (density, syncopation, balance: 1 - |mean of the onsets on the 16-step circle|), periodicity
             (the onset autocorrelation at one, two and four bars, after Panteli), Euclidean evenness (|F_k| / k of a
             voice's k onsets on the 16-step circle: 1 for the maximally even), the density cap (layers at once), the
             form rules (one operation per block, four-bar lines), the harmony rules (Dok. 8.9: at most four pitch
             classes, the bass at most two, no leading tone, all in the scale), the hypnosis corridor (the composer's
             own bar similarity and micro-change)
  audio      analyze_ref.py's measures of the rendered track, against the references of its profile: median and the
             10th to 90th percentile; a value outside that band is marked. Balance and width are measured on the
             track's loudest minute (a reference's on its middle minute, which is mostly body; the middle of a Peak
             track lies in its kick-out by design)

The report is Markdown (German, like the PLAN).
"""
from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import analyze_ref as ar  # noqa: E402

KEYS = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
SCALES = [[0, 2, 3, 5, 7, 8, 10], [0, 2, 3, 5, 7, 9, 10], [0, 1, 3, 5, 7, 8, 10], [0, 2, 3, 5, 7, 10], [0, 3, 5, 7, 10]]
PROFILE_OF = {"Hypnotic": "hypnotic", "Ostgut": "ostgut", "Dub": "dub", "Raw Peak": "raw"}
# Metrical weights of the sixteenths in 4/4 (Longuet-Higgins and Lee): the downbeat 0, the half bar -1, the quarters -2,
# the eighths -3, the sixteenths -4.
LHL = [0, -4, -3, -4, -2, -4, -3, -4, -1, -4, -3, -4, -2, -4, -3, -4]
# The voices the symbolic measures look at, by part name (perc lanes by their GM role in the default kit).
VOICES = {"kick": ["kick"], "hats": ["perc1", "perc2", "perc3", "perc4", "perc10"], "clap/perc": ["perc5", "perc6", "perc7",
          "perc8", "perc9", "perc11"], "bass": ["bass", "sub", "acid"], "ping/chord": ["ping", "chord"]}
AUDIO_KEYS = [("centroid", "Schwerpunkt Hz", "{:.0f}"), ("width_db", "Breite S/M dB", "{:.1f}"), ("lufs", "LUFS", "{:.1f}"),
              ("loud20", "lautestes 20 s", "{:.1f}"), ("lra", "LRA", "{:.1f}"), ("bar_similarity", "Takt-Ähnl.", "{:.3f}"),
              ("micro_change_db", "Mikro dB", "{:.2f}"), ("sub_share", "Sub-Anteil", "{:.2f}"),
              ("offq_mid", "Mitten neben den Vierteln", "{:.2f}")]


# ------------------------------------------------------------------------------------------------ symbolic measures

def bar_patterns(notes, parts, names, first_bar, bars):
    """Binary onset patterns per bar (16 steps) of the parts named in @p names."""
    ids = {i for i, p in enumerate(parts) if p in names}
    pat = np.zeros((bars, 16), dtype=bool)
    for beat, part, pitch, vel, *_ in notes:
        if part not in ids or vel <= 0.0:
            continue
        step = int(math.floor(beat * 4.0 + 0.25))
        b = step // 16 - first_bar
        if 0 <= b < bars:
            pat[b, step % 16] = True
    return pat


def lhl(pattern):
    """Longuet-Higgins and Lee's syncopation of one bar: a note followed by a silence on a stronger place."""
    on = [s for s in range(16) if pattern[s]]
    if not on:
        return 0.0
    total = 0.0
    for i, s in enumerate(on):
        nxt = on[i + 1] if i + 1 < len(on) else on[0] + 16
        rests = [r % 16 for r in range(s + 1, nxt)]
        if rests:
            strongest = max(LHL[r] for r in rests)
            if strongest > LHL[s]:
                total += strongest - LHL[s]
    return total


def poly_sync(pats):
    """A note of one stream left for a stronger place (before its next note) that another stream takes."""
    total = 0.0
    for a, pa in pats.items():
        on = [s for s in range(16) if pa[s]]
        for i, s in enumerate(on):
            nxt = on[i + 1] if i + 1 < len(on) else on[0] + 16
            for r in range(s + 1, nxt):
                rr = r % 16
                if LHL[rr] > LHL[s] and any(pb[rr] for b, pb in pats.items() if b != a):
                    total += LHL[rr] - LHL[s]
                    break
    return total


def balance(pattern):
    k = int(pattern.sum())
    if k == 0:
        return float("nan")
    z = sum(np.exp(2j * np.pi * s / 16.0) for s in range(16) if pattern[s])
    return 1.0 - abs(z) / k


def evenness(pattern):
    k = int(pattern.sum())
    if k < 2:
        return float("nan")
    z = sum(np.exp(2j * np.pi * k * s / 16.0) for s in range(16) if pattern[s])
    return abs(z) / k


def periodicity(notes, first_bar, bars):
    """The normalised autocorrelation of all onsets (velocity-weighted, sixteenths) at one, two and four bars."""
    n = bars * 16
    x = np.zeros(n)
    for beat, part, pitch, vel, *_ in notes:
        step = int(math.floor(beat * 4.0 + 0.25)) - first_bar * 16
        if 0 <= step < n:
            x[step] += vel
    x = x - x.mean()
    denom = float(np.dot(x, x)) + 1e-12
    return {lag // 16: float(np.dot(x[:-lag], x[lag:]) / denom) for lag in (16, 32, 64) if lag < n}


def harmony(notes, parts, key, scale):
    tonal = {i for i, p in enumerate(parts) if p in ("bass", "sub", "acid", "chord", "drone", "ping")}
    bass = {i for i, p in enumerate(parts) if p in ("bass", "sub")}
    pcs, bpcs = set(), set()
    off = 0
    for beat, part, pitch, vel, *_ in notes:
        if part not in tonal:
            continue
        pc = (pitch - key) % 12
        pcs.add(pc)
        if part in bass:
            bpcs.add(pc)
        if pc not in SCALES[scale]:
            off += 1
    return {"pcs": len(pcs), "bass_pcs": len(bpcs), "off_scale": off, "leading": 11 in pcs}


def symbolic(track, notes, parts, ops):
    info = track["info"]
    deck = track.get("deck", 0)
    notes = [n for n in notes if (n[4] if len(n) > 4 else 0) == deck]   # a set's other decks are other tracks
    start = track["start"]
    first_bar = int(round(start / 4.0))
    body0, body1 = first_bar + info["intro"], first_bar + info["outro"]
    bars = max(1, body1 - body0)
    out = {"voices": {}}
    pats = {}
    for v, names in VOICES.items():
        p = bar_patterns(notes, parts, names, body0, bars)
        pats[v] = p
        active = p.any(axis=1)
        if not active.any():
            continue
        pb = p[active]
        out["voices"][v] = {
            "lhl": float(np.mean([lhl(b) for b in pb])),
            "density": float(pb.sum() / (16.0 * len(pb))),
            "balance": float(np.nanmean([balance(b) for b in pb])),
            "evenness": float(np.nanmean([evenness(b) for b in pb])) if any(b.sum() >= 2 for b in pb) else float("nan"),
        }
    out["poly_sync"] = float(np.mean([poly_sync({v: pats[v][b] for v in pats}) for b in range(bars)]))
    body_notes = [n for n in notes if body0 * 4.0 - 0.05 <= n[0] < body1 * 4.0 - 0.05]
    out["periodicity"] = periodicity(body_notes, body0, bars)
    out["harmony"] = harmony([n for n in notes if start - 0.05 <= n[0] < start + info["bars"] * 4.0 - 0.05], parts, info["key"], info["scale"])
    # Form: one staircase operation at every block boundary, all on four-bar lines.
    t_ops = [o for o in ops if start - 1e-6 <= o[0] < start + info["bars"] * 4.0 + 1e-6 and o[3] == track.get("deck", 0)]
    blocks = info["bars"] // 32
    ok_blocks = 0
    for b in range(blocks):
        n = sum(1 for o in t_ops if abs(o[0] - start - 128.0 * b) < 1e-6 and o[1] not in ("kick out", "return", "end"))
        ok_blocks += n == 1
    out["form"] = {"blocks_with_one": ok_blocks, "blocks": blocks,
                   "off_lines": sum(1 for o in t_ops if abs(math.fmod(o[0] - start, 16.0)) > 1e-6)}
    # The density cap: the most layers at once (added minus removed).
    level, most = 0, 0
    for o in sorted(t_ops, key=lambda o: o[0]):
        if o[1] == "add":
            level += 1
        elif o[1] == "remove":
            level -= 1
        most = max(most, level)
    out["layers_at_once"] = most
    return out


# ------------------------------------------------------------------------------------------------ audio

def ref_bands(profile):
    refs = json.loads((HERE / "ref_stats.json").read_text(encoding="utf-8"))
    rows = [t for t in refs["tracks"] if t["profile"] == profile]
    return {k: (float(np.median([r[k] for r in rows])), float(np.percentile([r[k] for r in rows], 10)),
                float(np.percentile([r[k] for r in rows], 90))) for k, _, _ in AUDIO_KEYS}


def cut(wav, start_s, dur_s):
    tmp = Path(tempfile.gettempdir()) / f"tot_eval_{int(start_s * 1000)}.wav"
    subprocess.run(["ffmpeg", "-v", "quiet", "-y", "-ss", f"{start_s:.3f}", "-t", f"{dur_s:.3f}", "-i", str(wav), str(tmp)], check=True)
    return tmp


# ------------------------------------------------------------------------------------------------ report

def fmt(v, f="{:.2f}"):
    return "–" if v is None or (isinstance(v, float) and math.isnan(v)) else f.format(v)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--score", action="append", required=True, type=Path)
    ap.add_argument("--wav", action="append", default=[], type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--title", default="Totality: Evaluation")
    args = ap.parse_args()

    lines = [f"# {args.title}", "", "Erzeugt von `Tools/eval_report.py` (PLAN 13.5).", ""]
    audio_rows = []
    sym_rows = []
    in_set = False
    for si, spath in enumerate(args.score):
        doc = json.loads(spath.read_text(encoding="utf-8"))
        parts = doc["parts"]
        notes = doc["notes"]
        ops = [(o[0], o[1], o[2], o[3] if len(o) > 3 else 0) for o in doc["ops"]]
        wav = args.wav[si] if si < len(args.wav) else None
        tracks = doc["tracks"]
        in_set = in_set or len(tracks) > 1
        for ti, t in enumerate(tracks):
            info = t["info"]
            name = f"{spath.stem} T{ti + 1}" if len(tracks) > 1 else spath.stem
            sym = symbolic(t, notes, parts, ops)
            sym_rows.append((name, info, sym))
            if wav is not None:
                # A set's track is heard from its swap to the next one's; a track alone, whole.
                if len(tracks) > 1:
                    s0 = t["bass_s"] if ti > 0 else t["start_s"]
                    s1 = tracks[ti + 1]["bass_s"] if ti + 1 < len(tracks) else t["end_s"]
                    path = cut(wav, s0, s1 - s0)
                else:
                    path = wav
                loud = t["start_s"] + info["peak"] * 4.0 * 60.0 / info["bpm"] - (s0 if len(tracks) > 1 else 0.0)
                m = ar.measure(path, balance_start=loud)
                audio_rows.append((name, info, m))

    lines += ["## Symbolisch", "", "Je Track über den Body. Synkopen nach Longuet-Higgins und Lee (je Takt, gemittelt über die Takte, "
              "in denen die Stimme spielt), Dichte (Anteil belegter 16tel), Balance (1 − |Mittel auf dem 16er-Kreis|), "
              "Gleichmäßigkeit (|F_k|/k, 1 = maximal gleichmäßig).", ""]
    lines += ["| Track | Stil | Form | Stimme | LHL | Dichte | Balance | Gleichm. |", "|---|---|---|---|---|---|---|---|"]
    for name, info, sym in sym_rows:
        for v, d in sym["voices"].items():
            lines.append(f"| {name} | {info['style']} | {info['form']} | {v} | {fmt(d['lhl'])} | {fmt(d['density'])} | "
                         f"{fmt(d['balance'])} | {fmt(d['evenness'])} |")
    lines += ["", "| Track | poly. Synkope | Periodizität 1/2/4 Takte | Lagen zugleich | Blöcke mit einer Op | Ops neben 4-Takt-Linien | "
              "Tonhöhenklassen / Bass | außerhalb der Skala | Leitton | Korridor: Ähnl. / Mikro dB |",
              "|---|---|---|---|---|---|---|---|---|---|"]
    for name, info, sym in sym_rows:
        per = sym["periodicity"]
        h = sym["harmony"]
        f = sym["form"]
        lines.append(f"| {name} | {fmt(sym['poly_sync'])} | {fmt(per.get(1))} / {fmt(per.get(2))} / {fmt(per.get(4))} | "
                     f"{sym['layers_at_once']} | {f['blocks_with_one']} von {f['blocks']} | {f['off_lines']} | "
                     f"{h['pcs']} / {h['bass_pcs']} | {h['off_scale']} | {'ja' if h['leading'] else 'nein'} | "
                     f"{fmt(info['similarity'], '{:.3f}')} / {fmt(info['micro'])} |")
    if audio_rows:
        lines += ["", "## Audio gegen die Referenzen", "",
                  "Wert (Referenz-Median, 10. bis 90. Perzentil des Profils); **fett** außerhalb dieses Bands.", ""]
        head = "| Track | Profil | " + " | ".join(h for _, h, _ in AUDIO_KEYS) + " |"
        lines += [head, "|" + "---|" * (len(AUDIO_KEYS) + 2)]
        outside = {k: 0 for k, _, _ in AUDIO_KEYS}
        # A set's track is heard from its swap to the next swap: its body, its intro and outro under the neighbours'
        # blends -- its loudness range is not a whole track's and is shown, not judged.
        judged = [k for k, _, _ in AUDIO_KEYS if not (in_set and k == "lra")]
        for name, info, m in audio_rows:
            prof = PROFILE_OF.get(info["style"], "hypnotic")
            band = ref_bands(prof)
            cells = []
            for k, _, f in AUDIO_KEYS:
                v = m.get(k)
                med, lo, hi = band[k]
                bad = (v is None or (isinstance(v, float) and math.isnan(v)) or not (lo <= v <= hi)) and k in judged
                outside[k] += bad
                cell = f"{fmt(v, f)} ({fmt(med, f)}, {fmt(lo, f)}..{fmt(hi, f)})"
                cells.append(f"**{cell}**" if bad else cell)
            lines.append(f"| {name} | {prof} | " + " | ".join(cells) + " |")
        lines += ["", "Außerhalb des Bands: " + ", ".join(f"{h} {outside[k]} von {len(audio_rows)}" for k, h, _ in AUDIO_KEYS if k in judged) + "."]
        if in_set:
            lines += ["", "Im Set wird jeder Track von seinem Swap bis zum nächsten gemessen (sein Body; Intro und Outro liegen unter "
                      "den Blends der Nachbarn): sein LRA ist nicht der eines ganzen Tracks und wird gezeigt, nicht gewertet."]
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
