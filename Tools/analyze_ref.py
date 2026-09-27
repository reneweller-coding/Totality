"""The reference measurement (PLAN 13.4): what Berlin techno measures, and what Umbra's renders measure, the same way.

For every recording of Tools/ref_sets.txt (fetched by Tools/fetch_refs.py) and for any file given on the command line:

  tempo       from the autocorrelation of the kick band's energy (118 .. 142 BPM), refined on the kicks' grid; checked
              against the Beatport tempo the research document gives, which is also the check of this tool
  grid        where the kick sits: the offset of the bar and of every beat
  onsets      positive spectral flux in three bands (low 40-200 Hz, mid 200-2000 Hz, high 5-16 kHz), folded into the
              sixteen steps of the bar: where the percussion sits (Phosphene's ref_perc_profile, adapted)
  balance     power in 20-60, 60-250, 250-1000, 1-5 k, 5-16 kHz of the middle minute; the sub's share of the low end,
              the share above 5 kHz, the power centroid (Dok. 8.7's targets: sub ~40 % of the low end, >5 kHz ~7 %,
              centroid 1.5 .. 3.5 kHz)
  width       side over mid above 200 Hz and under 120 Hz, the correlation
  loudness    integrated LUFS, LRA and true peak from ffmpeg's ebur128 (BS.1770-4); the loudest 20 s (the energy mean
              of the short-term loudness over the loudest window of 20 s: what Umbra's Leveler measures, PLAN 8.5)
  form        the loudness of every bar; reductions (runs of bars at least 6 dB under the body's median) and their
              lengths in bars; the strongest novelty boundaries (Foote 2000 on per-bar band energies) and where they fall
              against the 16- and 32-bar lines
  kick        the fundamental of the kick's tail (the peak of 30 .. 100 Hz, 80 .. 250 ms after the kicks)
  hypnosis    PLAN 2.9: the onset states of the three bands per sixteenth (eight states) over the body as a first-order
              Markov chain -- its entropy rate h = H(a) and the predictive information rate b = H(a^2) - H(a)
              (Abdallah and Plumbley 2009); since a first-order chain also reads density (a carpet of onsets on every
              sixteenth is one state), repetition itself too: the entropy of the state at each of the bar's sixteen
              places (0 for an exact loop), the share of bars equal to the bar before, and the best share of bars equal
              to the bar 1, 2 or 4 before (a loop of two or four bars); the same without a threshold, the median
              correlation of the bars' continuous onset profiles (3 bands x 16 steps) at the best of those lags (the
              binary states flip where a quiet sixteenth sits near the threshold, 27.09.2026); and the micro-change rate, the
              median change of the band energies from one bar to the next, in dB

Only statistics leave this tool: Tools/ref_stats.json holds a row per recording and the medians per profile.

    python Tools/analyze_ref.py                      # every recording of ref_sets.txt
    python Tools/analyze_ref.py --only hypnotic
    python Tools/analyze_ref.py out/study.wav        # a render, printed the same way
"""
from __future__ import annotations

import argparse
import json
import math
import re
import subprocess
import sys
from pathlib import Path

import numpy as np
from scipy import signal

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from fetch_refs import default_dir, local_file, read_sets  # noqa: E402

SR = 22050          # tempo, onsets, form, kick
SR_FULL = 44100     # balance and width
KEYS = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
FLATS = {"Db": "C#", "Eb": "D#", "Gb": "F#", "Ab": "G#", "Bb": "A#"}


# ------------------------------------------------------------------------------------------------ decoding

def decode(path, sr, mono=True, start=0.0, dur=None):
    cmd = ["ffmpeg", "-v", "quiet"]
    if start > 0:
        cmd += ["-ss", str(start)]
    if dur is not None:
        cmd += ["-t", str(dur)]
    cmd += ["-i", str(path), "-ac", "1" if mono else "2", "-ar", str(sr), "-f", "f32le", "-"]
    raw = subprocess.run(cmd, capture_output=True, check=True).stdout
    x = np.frombuffer(raw, dtype=np.float32).astype(np.float64)
    if mono:
        return x
    n = len(x) // 2
    return x[:2 * n].reshape(-1, 2).T.copy()


def bandpass(x, lo, hi, sr):
    sos = signal.butter(4, [lo, hi], btype="bandpass", fs=sr, output="sos")
    return signal.sosfiltfilt(sos, x)


def loudness(path):
    full = subprocess.run(["ffmpeg", "-v", "info", "-i", str(path), "-af", "ebur128=peak=true", "-f", "null", "-"],
                          capture_output=True, text=True).stderr
    out = full[-4000:]

    def grab(label):
        m = re.findall(label + r":\s*(-?\d+\.\d+)", out)
        return float(m[-1]) if m else float("nan")
    # The per-frame log: a short-term loudness every 100 ms.
    st = np.array([float(v) for v in re.findall(r"\bS:\s*(-?\d+\.\d+)", full)], dtype=np.float64)
    loud20 = float("nan")
    if len(st) > 200:
        e = np.power(10.0, st / 10.0)
        w = np.convolve(e, np.ones(200) / 200.0, mode="valid")
        loud20 = round(10.0 * math.log10(max(float(w.max()), 1e-30)), 2)
    return grab("I"), grab("LRA"), grab("Peak"), loud20


# ------------------------------------------------------------------------------------------------ tempo and grid

def tempo(low, lo=118.0, hi=142.0):
    """Tempo from the autocorrelation of the kick band's 5 ms energy envelope at 1, 2 and 4 beats."""
    hop = int(0.005 * SR)
    n = len(low) // hop
    env = (low[:n * hop] ** 2).reshape(n, hop).mean(axis=1)
    env = env - env.mean()

    def score(bpm):
        v = 0.0
        for m in (1, 2, 4, 8):
            L = 60.0 / bpm * SR / hop * m
            i0 = int(L)
            fr = L - i0
            if i0 + 2 >= len(env):
                continue
            c = np.dot(env[:-i0 - 1], env[i0:len(env) - 1]) * (1 - fr) + np.dot(env[:-i0 - 1], env[i0 + 1:]) * fr
            v += c / (len(env) - i0 - 1)
        return v
    coarse = np.arange(lo, hi + 1e-9, 0.1)
    best = coarse[int(np.argmax([score(b) for b in coarse]))]
    fine = np.arange(best - 0.12, best + 0.12, 0.01)
    return float(fine[int(np.argmax([score(b) for b in fine]))])


def grid(times, low_flux, bpm):
    """The offset (samples) of the beat grid: where the kick band's onsets (its spectral flux) fold strongest onto
    the beat, within +-12 ms. Aligning on the onsets rather than on the energy puts the kick on step 0 and not on the
    sixteenth before it (the first version aligned on the energy's maximum, after the attack)."""
    beat = 60.0 / bpm * SR
    tol = 0.012 * SR
    best, best_score = 0.0, -1e30
    for off in np.linspace(0, beat, 384, endpoint=False):
        ph = (times - off) % beat
        near = (ph < tol) | (ph > beat - tol)
        score = low_flux[near].sum()
        if score > best_score:
            best_score, best = score, off
    return best


# ------------------------------------------------------------------------------------------------ onsets per band

BANDS3 = [("low", 40.0, 200.0), ("mid", 200.0, 2000.0), ("high", 5000.0, 10500.0)]


def flux(x, lo, hi, n=1024, hop=110):
    """Positive spectral flux of the log magnitude in [lo, hi], hop 5 ms at 22.05 kHz."""
    f, t, Z = signal.stft(x, fs=SR, nperseg=n, noverlap=n - hop, boundary=None, padded=False)
    sel = (f >= lo) & (f <= hi)
    m = np.log1p(1000.0 * np.abs(Z[sel]))
    d = np.maximum(np.diff(m, axis=1), 0.0).sum(axis=0)
    times = t[1:] * SR   # frame centres, samples
    return times, d


def step_series(times, d, bpm, offset, total_steps):
    """The flux summed per sixteenth of the grid: a series of total_steps values (each step from half a step before
    its grid line to half a step after, so an onset a few ms early or late counts for its own step)."""
    step = 60.0 / bpm * SR / 4.0
    k = np.floor((times - offset + 0.5 * step) / step).astype(int)
    ok = (k >= 0) & (k < total_steps)
    s = np.zeros(total_steps)
    np.add.at(s, k[ok], d[ok])
    return s


def binarize(s, bar_steps=16, q=0.75):
    """Onsets: a step whose flux is over the local level -- above the q quantile of its surrounding 8 bars times 0.5."""
    out = np.zeros(len(s), dtype=int)
    win = 8 * bar_steps
    for i in range(0, len(s), bar_steps):
        a, z = max(0, i - win // 2), min(len(s), i + win // 2)
        ref = np.quantile(s[a:z], q) if z > a else 0.0
        out[i:i + bar_steps] = (s[i:i + bar_steps] > 0.5 * ref).astype(int) if ref > 0 else 0
    return out


def entropy(p):
    p = p[p > 0]
    return float(-(p * np.log2(p)).sum())


def markov_rates(states, n_states):
    """Entropy rate H(a) and predictive information rate H(a^2) - H(a) of the first-order chain fitted to `states`."""
    c = np.zeros((n_states, n_states))
    for a, b in zip(states[:-1], states[1:]):
        c[a, b] += 1
    c += 1e-3   # a trace of smoothing so unseen rows are defined
    a = c / c.sum(axis=1, keepdims=True)
    # Stationary distribution: the counts of the states.
    pi = np.bincount(states, minlength=n_states).astype(float) + 1e-9
    pi /= pi.sum()
    h1 = sum(pi[i] * entropy(a[i]) for i in range(n_states))
    a2 = a @ a
    h2 = sum(pi[i] * entropy(a2[i]) for i in range(n_states))
    return h1, h2 - h1


# ------------------------------------------------------------------------------------------------ the measurement

def measure(path, bpm_hint=None):
    x = decode(path, SR)
    dur = len(x) / SR
    low = bandpass(x, 40.0, 200.0, SR)
    bpm = tempo(low)
    if bpm_hint is not None:
        # The half/double ambiguity of an autocorrelation: take the candidate nearest the hint's octave.
        for cand in (bpm, bpm * 2, bpm / 2):
            if abs(cand - bpm_hint) < abs(bpm - bpm_hint):
                bpm = cand
    fluxes = {name: flux(x, lo, hi) for name, lo, hi in BANDS3}
    off = grid(fluxes["low"][0], fluxes["low"][1], bpm)
    beat = 60.0 / bpm * SR
    bar = 4 * beat
    bars = int((len(x) - off) // bar)
    steps = bars * 16

    # Onset profiles and the state sequence.
    series = {}
    for name, _, _ in BANDS3:
        t, d = fluxes[name]
        series[name] = step_series(t, d, bpm, off, steps)
    prof = {k: (v.reshape(bars, 16).mean(axis=0) / max(v.reshape(bars, 16).mean(axis=0).max(), 1e-12)).round(3).tolist()
            for k, v in series.items()}

    # Per-bar loudness and band energies (the form and the micro-change).
    bands5 = [(20, 60), (60, 250), (250, 1000), (1000, 5000), (5000, 10500)]
    per_bar = np.zeros((bars, len(bands5)))
    f, t, Z = signal.stft(x, fs=SR, nperseg=2048, noverlap=2048 - 551, boundary=None, padded=False)
    P = np.abs(Z) ** 2
    frame_bar = np.floor((t * SR - off) / bar).astype(int)
    for j, (lo, hi) in enumerate(bands5):
        sel = (f >= lo) & (f < hi)
        e = P[sel].sum(axis=0)
        for b in range(bars):
            m = frame_bar == b
            per_bar[b, j] = e[m].mean() if m.any() else 0.0
    bar_db = 10 * np.log10(per_bar.sum(axis=1) + 1e-20)
    body = slice(bars // 5, bars - bars // 5)
    med = np.median(bar_db[body])
    quiet = bar_db < med - 6.0
    reductions = []
    b = 0
    while b < bars:
        if quiet[b] and bars // 8 <= b < bars - bars // 8:
            e = b
            while e < bars and quiet[e]:
                e += 1
            reductions.append((b, e - b))
            b = e
        else:
            b += 1
    # Novelty (Foote): a checkerboard kernel on the self-similarity of per-bar band energies in dB.
    feats = 10 * np.log10(per_bar + 1e-20)
    feats = (feats - feats.mean(axis=0)) / (feats.std(axis=0) + 1e-9)
    S = -np.sqrt(((feats[:, None, :] - feats[None, :, :]) ** 2).sum(axis=2))
    L = 8
    kern = np.kron(np.array([[1, -1], [-1, 1]]), np.ones((L, L)))
    nov = np.zeros(bars)
    for i in range(L, bars - L):
        nov[i] = (S[i - L:i + L, i - L:i + L] * kern).sum()
    peaks, _ = signal.find_peaks(nov, distance=8, prominence=np.std(nov[L:bars - L]) if bars > 2 * L else 1.0)
    peaks = sorted(peaks, key=lambda i: -nov[i])[:8]
    bounds = sorted(int(p) for p in peaks)
    # The file's first bar need not be a phrase's first bar, so what counts is the spacing of the boundaries: the
    # share of neighbouring boundaries 8, 16, 24 ... bars apart (one bar of slack).
    gaps = np.diff(bounds)
    on8 = float(np.mean([min(g % 8, 8 - g % 8) <= 1 for g in gaps])) if len(gaps) else float("nan")
    # The micro-change rate: the change of every band's energy from one bar to the next, in dB, averaged over the
    # bands, the median over the body.
    dbs = 10 * np.log10(per_bar[body] + 1e-20)
    micro = float(np.median(np.abs(np.diff(dbs, axis=0)).mean(axis=1))) if dbs.shape[0] > 2 else float("nan")

    # Bar similarity without a threshold: every bar's flux in the three bands on the sixteen steps (48 values, each band
    # divided by its median over the body), and the median correlation of a bar with the bar 1, 2 or 4 before -- the best
    # lag. A loop that repeats reads near 1 however its levels wander; a pattern rolled anew each bar reads lower.
    sim = float("nan")
    b0, b1 = bars // 5, bars - bars // 5
    if b1 - b0 > 8:
        mats = []
        for name, _, _ in BANDS3:
            v = series[name][b0 * 16:b1 * 16].reshape(b1 - b0, 16)
            mats.append(v / (np.median(v) + 1e-12))
        M = np.log1p(np.concatenate(mats, axis=1))
        best = []
        for lag in (1, 2, 4):
            c = [np.corrcoef(M[i], M[i - lag])[0, 1] for i in range(lag, M.shape[0]) if M[i].std() > 0 and M[i - lag].std() > 0]
            best.append(float(np.median(c)) if c else float("nan"))
        sim = max(best)

    # Hypnosis: eight onset states per sixteenth over the body.
    s0, s1 = (bars // 5) * 16, (bars - bars // 5) * 16
    onoff = [binarize(series[name])[s0:s1] for name, _, _ in BANDS3]
    states = onoff[0] * 4 + onoff[1] * 2 + onoff[2]
    h, pir = markov_rates(states, 8) if len(states) > 64 else (float("nan"), float("nan"))
    # Repetition directly: the entropy of the state at each of the bar's sixteen places over the body (0 for a loop that
    # repeats exactly, whatever its density), and the share of bars whose sixteen states equal the bar before's.
    nb = len(states) // 16
    loop_h, same, loop_rep = float("nan"), float("nan"), float("nan")
    if nb > 4:
        grid16 = states[:nb * 16].reshape(nb, 16)
        loop_h = float(np.mean([entropy(np.bincount(grid16[:, k], minlength=8) / nb) for k in range(16)]))
        same = float(np.mean([np.array_equal(grid16[i], grid16[i - 1]) for i in range(1, nb)]))
        # A loop of two or four bars repeats at its own length, not bar by bar: the best of the lags 1, 2 and 4.
        loop_rep = max(float(np.mean([np.array_equal(grid16[i], grid16[i - lag]) for i in range(lag, nb)])) for lag in (1, 2, 4))

    # The kick's fundamental: the average spectrum 80 .. 250 ms after the kicks, 30 .. 100 Hz.
    n = 4096
    acc = np.zeros(n // 2 + 1)
    for k in range(bars // 5 * 4, (bars - bars // 5) * 4, 2):
        a = int(off + k * beat + 0.08 * SR)
        seg = x[a:a + n]
        if len(seg) < n:
            break
        acc += np.abs(np.fft.rfft(seg * np.hanning(n))) ** 2
    ff = np.fft.rfftfreq(n, 1.0 / SR)
    sel = np.nonzero((ff >= 30) & (ff <= 100))[0]
    kick_hz = float("nan")
    if acc[sel].any():
        # The peak bin and a parabola through it and its neighbours in dB (the window is 186 ms, a bin 5.4 Hz).
        i = sel[np.argmax(acc[sel])]
        a, b, c = (10 * np.log10(acc[j] + 1e-30) for j in (i - 1, i, i + 1))
        d = 0.5 * (a - c) / (a - 2 * b + c) if (a - 2 * b + c) != 0 else 0.0
        kick_hz = float(ff[i] + d * (ff[1] - ff[0]))

    # Balance and width, the middle minute at full rate.
    start = max(0.0, dur / 2 - 30.0)
    xs = decode(path, SR_FULL, mono=False, start=start, dur=60.0)
    mid, side = 0.5 * (xs[0] + xs[1]), 0.5 * (xs[0] - xs[1])
    fw, pm = signal.welch(mid, fs=SR_FULL, nperseg=1 << 15)
    _, ps = signal.welch(side, fs=SR_FULL, nperseg=1 << 15)

    def bp(p, lo, hi):
        return float(p[(fw >= lo) & (fw < hi)].sum())
    lowE = bp(pm, 20, 250)
    total = bp(pm, 20, 16000)
    sel = (fw >= 40) & (fw < 16000)
    cen = float((fw[sel] * pm[sel]).sum() / max(pm[sel].sum(), 1e-30))
    corr = float(np.corrcoef(xs[0], xs[1])[0, 1])
    lufs, lra, tp, loud20 = loudness(path)
    return {
        "bpm": round(bpm, 2), "seconds": round(dur, 1), "bars": bars,
        "onsets": prof,
        "sub_share": round(bp(pm, 20, 60) / max(lowE, 1e-30), 3),
        "low_share": round(lowE / max(total, 1e-30), 3),
        "high_share": round(bp(pm, 5000, 16000) / max(total, 1e-30), 4),
        "bands_db": [round(10 * math.log10(max(bp(pm, lo, hi), 1e-30) / max(bp(pm, 40, 140), 1e-30)), 1)
                     for lo, hi in [(20, 60), (60, 250), (250, 1000), (1000, 5000), (5000, 16000)]],
        "centroid": round(cen),
        "width_db": round(10 * math.log10(max(bp(ps, 200, 16000), 1e-30) / max(bp(pm, 200, 16000), 1e-30)), 1),
        "low_side_db": round(10 * math.log10(max(bp(ps, 20, 120), 1e-30) / max(bp(pm, 20, 120), 1e-30)), 1),
        "correlation": round(corr, 3),
        "lufs": lufs, "lra": lra, "true_peak": tp, "loud20": loud20,
        "reductions": reductions,
        "boundaries": bounds,
        "boundary_spacing_on_8": round(on8, 2),
        "kick_hz": round(kick_hz, 1),
        "entropy_rate": round(h, 3), "pir": round(pir, 3),
        "loop_entropy": round(loop_h, 3), "bars_repeated": round(same, 3), "loop_repeated": round(loop_rep, 3),
        "bar_similarity": round(sim, 3),
        "micro_change_db": round(micro, 2),
    }


# ------------------------------------------------------------------------------------------------ output

PRINT = [("bpm", "BPM", "{:6.2f}"), ("kick_hz", "kick", "{:5.1f}"), ("sub_share", "sub/low", "{:5.2f}"),
         ("high_share", ">5k", "{:6.3f}"), ("centroid", "centr", "{:5d}"), ("width_db", "S/M", "{:5.1f}"),
         ("lufs", "LUFS", "{:5.1f}"), ("loud20", "L20", "{:5.1f}"), ("lra", "LRA", "{:4.1f}"), ("entropy_rate", "h", "{:4.2f}"), ("pir", "PIR", "{:4.2f}"),
         ("loop_entropy", "hloop", "{:5.2f}"), ("bars_repeated", "rep", "{:4.2f}"), ("loop_repeated", "lrep", "{:4.2f}"),
         ("bar_similarity", "sim", "{:4.2f}"),
         ("micro_change_db", "micro", "{:4.2f}")]


def row_text(name, r):
    cells = []
    for k, _, f in PRINT:
        v = r.get(k)
        try:
            cells.append(f.format(v))
        except (ValueError, TypeError):
            cells.append(str(v))
    red = ",".join(f"{b}+{l}" for b, l in r["reductions"][:4])
    return f"{name[:44]:44s} " + " ".join(cells) + f"  red {red}"


def header():
    widths = [6, 5, 5, 6, 5, 5, 5, 4, 4, 4, 5, 4, 4, 4, 4]
    return f"{'':44s} " + " ".join(f"{h:>{w}s}" for (_, h, _), w in zip(PRINT, widths))


def median_of(rows, key):
    v = [r[key] for r in rows if isinstance(r.get(key), (int, float)) and not math.isnan(r[key])]
    return round(float(np.median(v)), 3) if v else None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="*", type=Path)
    ap.add_argument("--dir", type=Path, default=default_dir())
    ap.add_argument("--only", default="")
    ap.add_argument("--out", type=Path, default=HERE / "ref_stats.json")
    args = ap.parse_args()

    print(header())
    if args.files:
        for p in args.files:
            print(row_text(p.name, measure(p)))
        return 0

    rows = []
    for ref in read_sets(HERE / "ref_sets.txt"):
        if args.only and ref["profile"] != args.only:
            continue
        path = local_file(args.dir, ref["id"])
        if path is None:
            print(f"missing {ref['name']} (run Tools/fetch_refs.py)")
            continue
        r = measure(path, ref["bpm"])
        r.update({"profile": ref["profile"], "name": ref["name"], "beatport_bpm": ref["bpm"], "beatport_key": ref["key"]})
        rows.append(r)
        flag = ""
        if ref["bpm"] is not None and abs(r["bpm"] - ref["bpm"]) > 0.5:
            flag = f"  (Beatport {ref['bpm']:.0f})"
        print(row_text(f"{ref['profile'][:3]} {ref['name']}", r) + flag, flush=True)

    keys = ["bpm", "kick_hz", "sub_share", "low_share", "high_share", "centroid", "width_db", "low_side_db", "correlation",
            "lufs", "loud20", "lra", "true_peak", "entropy_rate", "pir", "loop_entropy", "bars_repeated", "loop_repeated", "bar_similarity",
            "micro_change_db",
            "boundary_spacing_on_8"]
    profiles = {}
    for prof in sorted({r["profile"] for r in rows}) + ["all"]:
        sub = [r for r in rows if prof == "all" or r["profile"] == prof]
        med = {k: median_of(sub, k) for k in keys}
        med["n"] = len(sub)
        med["onsets"] = {b: np.median(np.array([r["onsets"][b] for r in sub]), axis=0).round(3).tolist() for b, _, _ in BANDS3}
        red = [l for r in sub for _, l in r["reductions"]]
        med["reductions_per_track"] = round(len(red) / max(len(sub), 1), 2)
        med["reduction_bars_median"] = float(np.median(red)) if red else None
        profiles[prof] = med
        print(f"median {prof:9s} n={len(sub):2d}  " + "  ".join(f"{k}={med[k]}" for k in keys))
    # Tempo check against Beatport: the check of the tool itself.
    tagged = [r for r in rows if r["beatport_bpm"] is not None]
    off = [abs(r["bpm"] - r["beatport_bpm"]) for r in tagged]
    if tagged:
        print(f"tempo against Beatport: {sum(1 for o in off if o <= 0.5)} of {len(tagged)} within 0.5 BPM, worst {max(off):.2f}")
    args.out.write_text(json.dumps({"tracks": rows, "profiles": profiles}, indent=1), encoding="utf-8")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
