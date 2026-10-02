# Totality release notes

## 1.2.0 (02.10.2026): with a DAW, on a Mac, and heard

**MIDI out.** In a DAW the plugin sends what it plays: every note of every part at the moment it sounds, a channel
per part as in the MIDI file of an export, the muted parts left out; a stop or a jump of the transport sends all notes
off. Record a part as notes, or let another instrument double it.

**Outputs of their own.** Besides the main output, a stereo output for each of its fifteen stems (the kick, the rumble, the sub, the hats, the percussion, the ping, the room, the bass, the 303, the chord, the drone, the texture, the dub, the cloud and the DJ effects), off until the
DAW switches them on; each carries its part before the master while the main output plays on.

**Ableton Link** in the standalone (Settings > Ableton Link, off to begin with): with other Link apps in the session
Totality takes their tempo, lines its bars up with theirs and starts and stops with them; alone it offers its own tempo.

**The keyboard's options**, each off until chosen (the Keyboard group): Lower Keys Play and Split At divide the keys
between two voices, Scale Lock keeps every key in the track's key, Velocity Curve (As Played, Soft, Hard, Fixed) shapes the
touch. A release always ends the note its press started.

**On a Mac.** Every release gets a build for Apple Silicon (macOS 12 or newer) -- the standalone and the VST3 --,
built and tested on GitHub's runners by the workflow `macos` and attached to the release as `Totality-<version>-macOS.zip`.
It is signed ad hoc, not notarized (that takes a paid Apple account; README-macOS.txt in the zip says how to open it),
and it has not yet been played on a real Mac.

**Demos.** The release "demos" holds a track per style as an MP3 and one of them as a video with pictures by
[KaleidoscopeEnhanced](https://github.com/reneweller-coding/KaleidoscopeEnhanced), its cuts placed by the track's own
score cues; `Tools/demo/make_demos.py` renders them all again.

**Behind the panel.** A sound check (`Tools/soundcheck.py`, `ctest -L sound`): four styles, a track each, and their stems, measured by loudness
(BS.1770) against `Tests/golden/soundcheck.json`, so a change that makes a style louder, quieter or emptier shows up
before anybody listens. A CI run on every push (GitHub Actions: the build and the tests on Windows, the documentation
check on Linux) that nobody waits for. One release script for the family (`Deploy/publish_release.ps1`: the notes from
this file, the checksums, the tag, the release).

## 1.1.0 (01.10.2026): the family's panel -- the first public release

**Play it yourself.** A Keyboard group on the Perform page: Keyboard Plays sends the keys of a MIDI keyboard to a voice
(the kit, the bass, the 303, the ping, the chord, the drone, or by channel), with the sound its page has; Replace leaves that voice's generated notes out, Layer plays over
them; Composer off leaves every generated note out, so only what is played sounds -- through the mix and the effects
as composed. The keys play on their samples; an export plays what was composed.

**One layout for the repositories.** Every instrument of the family builds the same way now: `build.ps1` (msvc, icx,
release, quest) on the presets of `CMakePresets.json`, the build trees under `build\<preset>`, everything that can be
started -- the standalone, the VST3, the renderer -- flat in `bin\msvc` and `bin\icx` (and the Quest APK in
`bin\quest`), the release in `dist\`, local data, renders and logs in `work\` (`cmake/Family.cmake`).

**Every line explained.** Every class, function, variable, macro and table of the sources -- the core, the plugin,
the Quest app, the tools and the tests -- has its Doxygen comment now, and the test `doccheck` (`cmake/Family.cmake`)
fails as soon as one is missing. The scripts that generate tables write the comments into what they generate.

**No page scrolls.** The window opens at 1280 x 860, as every generator of the family's. A tab of several modules has a
small tab for each, and a page that is still taller than the window shows its groups in sections, one at a time --
the sound and the modulation apart, cut further where needed (Filter, LFO, Matrix ...) --, switched at its top right.
The overview above the tabs can be folded away in the settings.

**One panel for the family.** Totality, Parhelion, Ephemeris and Phosphene share their panel now (Plugin/Frame.h,
the same file in each, and Noctuary its right-hand tools): the header's two rows -- the logo, the style, the key and
the scale, track or mix and its length, Compose, New seed, Play, Mute; the thumbs, the status, Undo, Redo, Help and the
settings --, the overview under it, the tabs in one order (Set, Arrange, the instrument's own pages, Mixer, Perform,
Export, Style), the same keys (Space, Ctrl+Z / Ctrl+Y, F1, F11, Esc, Ctrl+S / Ctrl+O / Ctrl+E) and the same right
click on every control (MIDI learn, forget, the default). Each keeps its own colours and letters and has a picture
behind its panel, generated for it and kept low in contrast (Settings > Picture behind the panel takes it away).

**Undo and Redo** of every change -- a knob, a new seed, a reroll, a preset, a loaded set --, step by step, the step
named in the tooltip. **Help** (F1): this manual inside the plugin, by topic, with the page in front, the keys and the
headset as topics of their own (a new chapter, The panel). **Settings**: the update check, the picture, the headset,
the window's size, full screen, the keys, About.

**A live ring** round every knob the composer moves shows where it stands at this moment.

**The mixer as a console.** The Mixer tab's first page is a channel strip per part -- level, mute, sound and meter --
and the output with its loudness; the buses and the master, and the decks, are pages of their own.

**The same controllers in every generator.** Controller 74 (brightness) moves the master filter, 11 the echo throw; the
mod wheel is no longer bound to the filter.

**The headset only when there is one.** The controls of a Meta Quest's hands are shown only while a headset sends them,
or when the settings say Always. The Quest app sends its hands to the plugin in bridge mode (bridge_host in tot.cfg,
port 9103) and the plugin plays with them in the grammar every generator's headset shares: left pinch play / stop,
both hands the next track, right pinch kick out / in, the left hand the master filter, the right the echo throw. audio=0
leaves the headset silent.

**Words.** The automation is "the moves" now, as in every generator of the family: "reroll moves", DJ Moves.

## 1.0.0 (28.09.2026, built, not published)

The first release: a generator of hypnotic Berlin techno, as a VST3 plugin, a standalone application for Windows, a
command-line renderer and an app for Meta Quest.

**Composing.** Tracks in three forms -- the Arc (the DJ tool: intro, a body that builds its full groove by about bar 97, outro), the
Peak (a long kick-out and the densest block after it), the Endless (full from the first bar, changing by exchange) --
from four style profiles, Hypnotic, Ostgut, Dub and Raw Peak, which morph into each other and move along a Dub and a
Hypnotic axis; or a style of your own (the Style page). A track never begins with the kick alone: it opens with kick,
hats and percussion, or with hats and percussion or an atmosphere and the kick on bar 9 or 17, by its style's weights
(measured on the reference records), and ends as it began; the percussion that opens it carries it (four to seven
hits a bar, up front in the balance), and a DJ mix's first track never begins with the kick. One operation per 32-bar block, events on the 8-bar lines,
automation by two hands, eight candidates per block chosen against a corridor of bar similarity fitted to thirty
reference tracks. Every track has a figure, the voice it is remembered by -- a ping motif, a dub stab, a bass riff or a
303 line --, and its body runs in waves: the figure's filters and the hats open towards a landing, something drops away
just before it (the centre, the kick and the claps, the figure), and an element breathes out between two landings and
comes back with a throw. Every part on its own seed stream: form, harmony, rack, layers, blocks, events, hands, sounds,
figure can be rerolled alone. Every track is also of an archetype -- Tool, Roller, Stab, Acid, Bleep, Dub Chord, Tribal
--, drawn by its style or fixed on the Set page; a set never plays two of a kind in a row. The + and - buttons rate the
track that plays; with Favor Ratings the liked kinds and sounds come more often.

**Sets.** Up to four hours, mixed as one long recomposition on two decks and a third that borrows: five dramaturgies
of tempo and energy, tracks in neighbouring Camelot keys, about twenty an hour with three minutes of their own each
(Track Time); blends of 16 or 32 bars led by the EQs -- the highs first, the mids over the last 16 bars -- with the bass
swapped on a 32-bar line through the isolator, where the incoming track's figure lands (its first body block runs under
the outgoing track's full groove, nearly full itself); the outgoing track's hats carried on, the next track's figure teased in,
a percussion loop of the one before layered under; the DJ's hand on the channels between the blends (low kills, high
swells, mid dips, filter builds, echo throws; DJ Hand); breaks from the mixer's tape echo and hall; seven dramaturgies (with Cruise and Marathon), up to twelve hours.

**Sound.** A kick of three engines and a 909 top layer; the rumble, which continues the kick's phase under its hall;
a sub locked to the kick; twelve kit lanes with the 909's metal oscillators; the ping; a bass synth and a 303 through ten
circuit-modelled filters; the dub chord with tape echo, springs and plate; drone, texture, a grain cloud. Multiband
ducking, a track bus with tilt and parallel glue, a DJ mixer, a master with a 4x clipper and a true-peak limiter at
-1 dBTP, and a leveler that sets every part against the kick -- each lane of the kit and every tonal voice in a window
around the fader levels of the research, one or two voices up front and the rest behind them, the room taken back
where the mids would stand over the reference records' -- and brings each track's loudest part to its style's
loudness (-9.4 to -11.5 LUFS). Every track has a tonal voice it is remembered by, in by the body's second block.

**Presets.** 1024 factory presets for each synth -- kick, rumble, sub, a kit lane, ping, bass, 303, dub chord, drone
and texture -- in sixteen named groups each. The composer chooses one per synth and per kit lane for every track, by how
well its group suits the track's style (a lane among those made for its role); every page names the preset of the track
that plays, and its values stand on the knobs, so a turn goes on from what you hear. In a set each deck keeps its own
track's sounds through the blend.

**The panel.** Track or DJ mix, chosen by two buttons beside the style, with one length for what is chosen; Compose
names what it writes, the status line what plays. The arrangement zooms with the mouse wheel around the pointer down
to four bars, a drag or Shift + wheel moves along, a double click shows everything; a ruler counts a track's bars and
a mix's minutes, and a zoomed view pages on with the playhead.

**Playing.** In a DAW Totality follows the host's transport and tempo. The Perform page is a mixer: seven mutes (their
tails ring out), a master filter, an echo throw, isolator kills and faders per deck, every control learnable from a
MIDI controller; the keys C3 to F#3 toggle the mutes. The Patterns page shows the Eclipse: the kick a dark disc, every
part a ring of beads around it, polymeters precessing, conjunctions lighting the corona.

**Export.** A 24-bit WAV with cue markers, the cues as JSON and a rekordbox collection (beat grid and cues), MIDI with the tempo map, stems whose sum is exactly the
mix before the master, seamless 4- and 8-bar DJ loops, `.totset` files; OSC cues for a visualiser while it plays
(`/tot/beat`, `/tot/bar`, `/tot/block`, `/tot/op`, `/tot/key`).

**Meta Quest.** The whole generator on the headset, played with the hands (pinches for play, kick out and next track,
the hands' height for the filter and the throw), the Eclipse turning above the player. Built, not yet run on a device.

**Tested.** 29 self tests and vector tests, the VST3 loaded as a host loads it (30 checks), pluginval at strictness 10.

**Known.** Whether Rekordbox and Traktor read the WAV's
cue markers is untested (the JSON cues are the fallback). The Quest's CPU load is estimated, not measured. Endless forms
keep their loudness flat (a loudness range under 1 LU); the Dub profile sits at the lower edge of the references'
spectral centroid.
