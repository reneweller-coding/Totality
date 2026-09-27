# Umbra for Meta Quest

The whole generator on the headset: the composer writes a track or a set, the engine synthesizes it, and the hands
play it like hands at a mixer. Native OpenXR, no game engine — `NativeActivity` + `android_native_app_glue`, EGL,
GLES 3, the Khronos OpenXR loader, `XR_EXT_hand_tracking`, Oboe, and the unchanged core from `../Core`. The frame of
the app (session, swapchains, point renderer, font, hands, audio stream) is Ephemeris' Quest app, after Phosphene's;
the player, the gestures, the panel and the Eclipse in the room are Umbra's.

```
Quest/
  CMakeLists.txt        NDK build of libumbquest.so (links UmbraCore, oboe, openxr_loader)
  AndroidManifest.xml   NativeActivity, hasCode=false, hand-tracking permission/features, VR category
  src/main.cpp          the app: OpenXR session, composer thread, Oboe, hand controls, the panel and the Eclipse
  res/mipmap-*/         the launcher icon at five densities (Deploy/make_icon.py)
  fetch_thirdparty.ps1  OpenXR loader (prefab AAR) and Oboe into ../ThirdParty (junctions into Noctuary's)
  build_apk.ps1         CMake/NDK -> aapt2 -> jar -> zipalign -> apksigner (debug key)
```

## Build

```powershell
powershell -File Quest\fetch_thirdparty.ps1
powershell -File Quest\build_apk.ps1
adb install -r build-quest\UmbraQuest.apk
```

Needs NDK r27 (`C:\Android-Buildtools\sdk\ndk\27.2.12479018`), build-tools 34, platform android-34 and JDK 17 — the
parameters at the top of `build_apk.ps1`. No Gradle and no ninja. The APK is 4.8 MB: Umbra ships no data, every sound
is synthesised.

Built on 28.09.2026 without a headset attached: it compiles and links for arm64 without a warning and the APK is
signed, but it has not run on a device yet.

## Three threads

| Thread | Does |
|---|---|
| Audio (Oboe, low latency, exclusive) | `TrackPlayer::process`: `Engine::process` and the play/stop fade; publishes beat, seconds and level. One compare-and-exchange, no lock, no allocation. |
| Composer | composes a whole track (or set) and loads it before the stream starts; "next" composes the next one and swaps it in behind a fade while the audio thread writes silence; then measures its loudness while it plays. |
| Render (the glue thread) | OpenXR frame loop, hands, gestures, picture. |

## Playing it

The engine plays live (`Engine::setLive`): the perform module acts, as in the plugin.

| Gesture | Effect |
|---|---|
| left pinch | play / stop — a 15 ms fade, the music pauses where it is |
| right pinch | kick out / kick in (`perform.mute_kick`) |
| both hands pinched together | the next track (or set), from the next seed, swapped in behind a fade |
| left hand height | the master filter (`perform.filter`): low pass below mid height, high pass above, open in the middle |
| right hand height | the echo throw (`perform.throw`), from mid height up |

A pinch acts when it opens again, so a pinch of both hands never also counts as two single ones. A hand moves its
control only while it is **not** pinching. Height is measured against the head, so it works standing or sitting; both
controls are smoothed over 0.15 s, the filter has a dead zone round the middle, and nothing ever jumps. When a track has
ended and its rooms have rung out, the next one follows by itself.

The panel is head-locked (yaw only) and drawn as points: the logo, the track (or the set and its track), style and
form, the block, key, scale and Camelot label, the kick's and the ping's preset (the composer chooses one of 1024 per
synth for every track, as in the plugin), the time and the tempo, the level, the filter and the throw, KICK OUT
while the kick is out, four beat lamps.

**The Eclipse in the room** (PLAN 10.2): 3 m ahead and 1.3 m above the eyes where the session began, tilted towards the
player — the plugin's Patterns view at the size of a room. The kick is a dark disc, no light at all, in its corona,
which swells with every kick; every other part a ring of beads, a bar once round, the playhead's hand sweeping them; a
ray where three rings meet on a sixteenth. The next bar's beads fade in over the first eighth of the bar while the last
bar's fade out, so nothing pops: every brightness is a continuous function of the bar position (the Kaleidoscope
rules), and nothing about the camera moves with the audio.

## Config

`umb.cfg` in the app's external data folder, every key optional:

```
adb push umb.cfg /sdcard/Android/data/com.reneweller.umbra.quest/files/umb.cfg
```

```
mute=1                     start silent (the test rule); the engine still runs
seed=2026                  the first track's seed; the next takes the next seed
minutes=7                  length of a track
set_minutes=60             a set of so many minutes instead of single tracks
style=Hypnotic             Hypnotic, Ostgut, Dub or Raw
quality=desktop            everything (default here: quest)
osc_host=192.168.1.20      the score cues to a visualiser (Cue.h: /umb/beat, /umb/bar, /umb/block, /umb/op, /umb/key)
osc_port=9000
knobs=compose.key=D;mix.hats_level=-5      any knobs, repeatable
```

## CPU (PLAN 11)

The Quest's quality (`Engine::Quality::Quest`, the default here) lets the grain cloud rest and clips the rumble at the
rate instead of four times it. Measured on the desktop (`umb_render --bench`, and with the CMake option `UMB_PROFILE`
per stage), a 20-minute set of seed 2026 with its blends: 7.8 % of a core at the desktop quality, 7.5 % at the Quest's.
The largest stages: the rumble (its hall and clip) 12 %, the twelve kit lanes 13 %, the dub chain 11 %, the room 9 %,
the master's 4x clipper 7.5 %. What that is on the Quest 2 has to be measured there; with the usual factor of three
to four between a desktop core with AVX2 and the Quest's with NEON it would lie at 23 to 30 %, at the edge of the
plan's 30 %. Further levers, not built: the outgoing deck of a blend at a lower quality (PLAN 11), a smaller room.
