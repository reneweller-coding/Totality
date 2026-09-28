# Totality

A generator for hypnotic Berlin techno: tracks of 32-bar plateaus and whole DJ sets, composed from a seed and
synthesised in real time -- as a VST3 plugin, a standalone application, a command-line renderer and an app for Meta
Quest. The sibling of [Noctuary](../AmbientSynth) (ambient), Phosphene (psytrance) and Ephemeris (Berlin School).

![Totality, the Patterns page](docs/screenshot.png)

**Version 1.0.0 (28.09.2026).** A composer writes tracks in three forms (Arc, Peak, Endless) from four style profiles
(Hypnotic, Ostgut, Dub, Raw Peak) that morph into each other: one operation per 32-bar block, events on the 8-bar
lines, automation by two "hands", harmony by the research document's rules, eight candidates per block judged against a
hypnosis corridor. Every track has a figure -- a ping motif, a stab, a bass riff or a 303 line it is remembered by --
and its body runs in waves of 32 or 64 bars that build towards a landing, drop something away just before it and
breathe in between, as often as the reference records change (`Tools/analyze_ref.py`, meso). A set composer mixes them on two decks as a Berlin DJ would -- five dramaturgies, the tempo drifting
by at most 1 BPM a track, blends of 16 or 32 bars with a hard bass swap on a 32-bar line through the isolator, live
loops of the last track on a third deck, breaks from the mixer's effects. Every part is drawn on its own seed stream and
can be rerolled alone (`.totset`).

The sound: the kick (three engines, a 909 top layer), the rumble that continues the kick's phase under its hall, the
sub bass locked to the kick, a twelve-lane kit with the 909's metal oscillators, the ping (FM through a low-pass gate),
a bass synth and a 303 line through ten circuit-modelled filters, the dub chord with its chain (tape echo, springs,
plate), drone, texture and a granular cloud; multiband ducking, a track bus with tilt and parallel glue, a DJ mixer, a
master with clipper and true-peak limiter, and a leveler that brings every track's loudest part to its style's level.
Every synth has 1024 factory presets in sixteen groups; the composer chooses one per synth and per kit lane for every
track, by its style, and the knobs show them while the track plays.
Balance, width and loudness are fitted to 30 reference recordings (`Tools/analyze_ref.py`; only statistics are kept).

What comes out: the WAV with cue markers (and the same cues as JSON), MIDI with the tempo map, stems that sum exactly to
the mix, seamless DJ loops, and OSC cues for a visualiser such as Kaleidoscope while it plays. Live, the Perform page is
a mixer: mutes, a master filter, an echo throw, isolator kills, every control learnable from MIDI.

The manual: [docs/manual/Totality-Manual.pdf](docs/manual/Totality-Manual.pdf). The plan, in German, with the state of every
phase: [docs/PLAN.md](docs/PLAN.md). The research it rests on: [docs/research](docs/research).

## Build

```
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
cd build && ctest -C Release
```

JUCE comes from `ThirdParty/JUCE`, else from the sibling Phosphene's checkout, else it is fetched (tag 9.0.1).
`TOT_BUILD_PLUGIN=OFF` builds the core, the renderer and the tests alone. The release (tests, pluginval, manual,
installer): `powershell -ExecutionPolicy Bypass -File Deploy\build_release.ps1`. The Quest app: see
[Quest/README.md](Quest/README.md).

## Render

```
build/Tools/render/Release/tot_render --seed 7 --out track.wav --midi track.mid --stems stems --loops loops
build/Tools/render/Release/tot_render --seed 7 --set "compose.style=Dub" --form peak --out dub.wav
build/Tools/render/Release/tot_render --seed 2026 --set-minutes 120 --set "set.dramaturgy=Peak" --out set.wav
build/Tools/render/Release/tot_render --seed 7 --reroll block4 --reroll rack.ch --save-set track.totset --plan
```

A track's tempo, key, scale and low end are drawn from its style (`compose.auto`); `--bpm`, `--low` and `--form` fix
them. Before it renders, `tot_render` measures the loudest part of every track and sets its level to the style's target;
`--bench` skips that and the loudness meter (`--quality quest`: the headset's level), `--plan` renders nothing. The WAV
carries cue markers (bass in, kick-outs, returns, outro; in a set every track, swap and loop), also written beside it as
JSON. `--loops dir` writes seamless 4- and 8-bar loops of the loudest block with kick, hats and perc alone; `--stems dir`
a WAV per element, their sum the mix before the master; `--decks dir` a set's decks after their channels;
`--score-json` the score for the evaluation. `--reroll unit` draws one part again (`form`, `harmony`, `rack`,
`rack.<layer>`, `layers`, `blocks`, `block<n>`, `events`, `hands`, `sounds`, `figure`; in a set `track<n>` and
`track<n>.<unit>`), leaving everything else as it was; `--save-set` and `--load-set` keep it.

`--list` prints every parameter, `--dump-params f.json` describes them; `--set "key=value; ..."` changes them;
`--patterns` prints the layers in Tidal mini-notation, `--stats` how often each part repeats its bar.

## Measure

```
python Tools/fetch_refs.py                 # the reference audio, outside the repository
python Tools/analyze_ref.py                # statistics of the references into Tools/ref_stats.json
python Tools/analyze_ref.py track.wav      # a render, measured the same way
python Tools/eval_report.py --score track.json --wav track.wav --out report.md   # the evaluation (PLAN 13.5)
```

## Licence

AGPL-3.0, like the siblings (see LICENSE).
