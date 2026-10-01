# Totality release notes

## 1.0.0 (28.09.2026)

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
