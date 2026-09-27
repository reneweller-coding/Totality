# Umbra

A generator for hypnotic Berlin techno: tracks of 32-bar plateaus and, later, whole DJ sets, composed from a seed and
synthesised in real time. The sibling of [Noctuary](../AmbientSynth) (ambient), Phosphene (psytrance) and Ephemeris
(Berlin School).

**State (27.09.2026): Phases 1 to 3 done.** The kick (three engines, a 909 top layer), the rumble that continues the
kick's phase under its hall, the sub bass locked to the kick, a twelve-lane kit with a six-bit 909 metal table, the ping
(FM through a low-pass gate), a bass synth and a 303 line through circuit-modelled filters, the dub chord with its chain
(tape echo, springs, plate), drone, texture and a granular cloud, the pattern rack with the onset matrices of the
research document plus polymeters, slipping loops, Euclidean patterns, ghost chains, trig conditions and fills,
free-running LFOs, multiband ducking, a master with parallel glue, tilt, clipper and true-peak limiter, a leveler, and a
fixed study of one track per style. Balance, width and loudness are fitted to 30 reference recordings
(`Tools/analyze_ref.py`; only statistics are kept). No plugin yet.

The plan, in German: [docs/PLAN.md](docs/PLAN.md). The research it rests on: [docs/research](docs/research).

## Build

```
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
cd build && ctest -C Release
```

## Render

```
build/Tools/render/Release/umb_render --seed 7 --minutes 7 --out track.wav --midi track.mid --stems stems
build/Tools/render/Release/umb_render --seed 7 --low sub --set "compose.style=Dub" --out dub.wav
```

Before it renders, `umb_render` measures the loudest part of the track and sets its level to the style's target (the
references' loudest 20 seconds); `--bench` skips that and the loudness meter.

`--list` prints every parameter; `--set "key=value; ..."` changes them; `--patterns` prints the layers in Tidal
mini-notation, `--stats` how often each part repeats its bar.

## Measure

```
python Tools/fetch_refs.py                 # the reference audio, outside the repository
python Tools/analyze_ref.py                # statistics of the references into Tools/ref_stats.json
python Tools/analyze_ref.py track.wav      # a render, measured the same way
```

## Licence

AGPL-3.0, like the siblings (see LICENSE).
