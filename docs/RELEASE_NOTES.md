# Umbra release notes

## 1.0.0 (28.09.2026)

The first release: a generator of hypnotic Berlin techno, as a VST3 plugin, a standalone application for Windows, a
command-line renderer and an app for Meta Quest.

**Composing.** Tracks in three forms -- the Arc (the DJ tool: intro, a body that adds one layer a block, outro), the
Peak (a long kick-out and the densest block after it), the Endless (full from the first bar, changing by exchange) --
from four style profiles, Hypnotic, Ostgut, Dub and Raw Peak, which morph into each other and move along a Dub and a
Hypnotic axis; or a style of your own (the Style page). One operation per 32-bar block, events on the 8-bar lines,
automation by two hands, eight candidates per block chosen against a corridor of bar similarity fitted to thirty
reference tracks. Every part on its own seed stream: form, harmony, rack, layers, blocks, events, hands, sounds can be
rerolled alone.

**Sets.** Up to four hours on two decks and a third for loops: five dramaturgies of tempo and energy, tracks in
neighbouring Camelot keys, blends of 16 or 32 bars with the bass swapped on a 32-bar line through the isolator, loops of
the outgoing track under the next, breaks from the mixer's tape echo and hall.

**Sound.** A kick of three engines and a 909 top layer; the rumble, which continues the kick's phase under its hall;
a sub locked to the kick; twelve kit lanes with the 909's metal oscillators; the ping; a bass synth and a 303 through ten
circuit-modelled filters; the dub chord with tape echo, springs and plate; drone, texture, a grain cloud. Multiband
ducking, a track bus with tilt and parallel glue, a DJ mixer, a master with a 4x clipper and a true-peak limiter at
-1 dBTP, and a leveler that brings each track's loudest part to its style's loudness (-9.4 to -11.5 LUFS).

**Playing.** In a DAW Umbra follows the host's transport and tempo. The Perform page is a mixer: seven mutes (their
tails ring out), a master filter, an echo throw, isolator kills and faders per deck, every control learnable from a
MIDI controller; the keys C3 to F#3 toggle the mutes. The Patterns page shows the Eclipse: the kick a dark disc, every
part a ring of beads around it, polymeters precessing, conjunctions lighting the corona.

**Export.** A 24-bit WAV with cue markers and the cues as JSON, MIDI with the tempo map, stems whose sum is exactly the
mix before the master, seamless 4- and 8-bar DJ loops, `.umbset` files; OSC cues for a visualiser while it plays
(`/umb/beat`, `/umb/bar`, `/umb/block`, `/umb/op`, `/umb/key`).

**Meta Quest.** The whole generator on the headset, played with the hands (pinches for play, kick out and next track,
the hands' height for the filter and the throw), the Eclipse turning above the player. Built, not yet run on a device.

**Tested.** 29 self tests and vector tests, the VST3 loaded as a host loads it (30 checks), pluginval at strictness 10.

**Known.** The name "Umbra" is used by other audio products (PLAN, risk 7). Whether Rekordbox and Traktor read the WAV's
cue markers is untested (the JSON cues are the fallback). The Quest's CPU load is estimated, not measured. Endless forms
keep their loudness flat (a loudness range under 1 LU); the Dub profile sits at the lower edge of the references'
spectral centroid.
