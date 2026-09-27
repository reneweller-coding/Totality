# Umbra

A generator for hypnotic Berlin techno: tracks of 32-bar plateaus and, later, whole DJ sets, composed from a seed and
synthesised in real time. The sibling of [Noctuary](../AmbientSynth) (ambient), Phosphene (psytrance) and Ephemeris
(Berlin School).

**State (27.09.2026): Phase 1.** The kick (three engines, a 909 top layer), the rumble that continues the kick's phase
under its hall, the sub bass locked to the kick, a twelve-lane kit with a six-bit 909 metal table, the pattern rack
with the onset matrices of the research document, and a fixed study of one track. No plugin yet.

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

`--list` prints every parameter; `--set "key=value; ..."` changes them.

## Licence

AGPL-3.0, like the siblings (see LICENSE).
