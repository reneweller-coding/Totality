# Umbra

A generator for hypnotic Berlin techno: tracks of 32-bar plateaus and whole DJ sets, composed from a seed and
synthesised in real time. The sibling of [Noctuary](../AmbientSynth) (ambient), Phosphene (psytrance) and Ephemeris
(Berlin School).

**State (27.09.2026): Phases 1 to 4 done.** A composer writes tracks in three forms (Arc, Peak, Endless) from four
style profiles (Hypnotic, Ostgut, Dub, Raw Peak) that morph into each other: one operation per 32-bar block, events on
the 8-bar lines, automation by two "hands" on a bar grid, harmony by the research document's rules, eight candidates per
block judged against a hypnosis corridor. A set composer mixes them on two decks as a Berlin DJ would -- five
dramaturgies, the tempo drifting by at most 1 BPM a track, blends of 16 or 32 bars with a hard bass swap on a 32-bar
line through the isolator, live loops of the last track on a third deck, breaks from the mixer's effects. Every part is
drawn on its own seed stream and can be rerolled alone (`.umbset`). Cue markers, DJ loops, MIDI and stems come out with
the audio. The sound: the kick (three engines, a 909 top layer), the rumble that continues the
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
build/Tools/render/Release/umb_render --seed 7 --out track.wav --midi track.mid --stems stems --loops loops
build/Tools/render/Release/umb_render --seed 7 --set "compose.style=Dub" --form peak --out dub.wav
build/Tools/render/Release/umb_render --seed 2026 --set-minutes 120 --set "set.dramaturgy=Peak" --out set.wav
build/Tools/render/Release/umb_render --seed 7 --reroll block4 --reroll rack.ch --save-set track.umbset --plan
```

A track's tempo, key, scale and low end are drawn from its style (`compose.auto`); `--bpm`, `--low` and `--form` fix
them. Before it renders, `umb_render` measures the loudest part of every track and sets its level to the style's target
(the references' loudest 20 seconds); `--bench` skips that and the loudness meter, `--plan` renders nothing. The WAV
carries cue markers (bass in, kick-outs, returns, outro; in a set every track, swap and loop), also written beside it as
JSON. `--loops dir` writes seamless 4- and 8-bar loops of the loudest block with kick, hats and perc alone;
`--decks dir` a set's decks after their channels; `--score-json` the score for the evaluation. `--reroll unit` draws one
part again (`form`, `harmony`, `rack`, `rack.<layer>`, `layers`, `blocks`, `block<n>`, `events`, `hands`, `sounds`; in a
set `track<n>` and `track<n>.<unit>`), leaving everything else as it was; `--save-set` and `--load-set` keep it.

`--list` prints every parameter; `--set "key=value; ..."` changes them; `--patterns` prints the layers in Tidal
mini-notation, `--stats` how often each part repeats its bar.

## Measure

```
python Tools/fetch_refs.py                 # the reference audio, outside the repository
python Tools/analyze_ref.py                # statistics of the references into Tools/ref_stats.json
python Tools/analyze_ref.py track.wav      # a render, measured the same way
python Tools/eval_report.py --score track.json --wav track.wav --out report.md   # the evaluation (PLAN 13.5)
```

## Licence

AGPL-3.0, like the siblings (see LICENSE).
