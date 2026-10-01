/**
 * @file Rack.h
 * @brief The pattern rack (PLAN 6): layers with sixteen-step onset matrices, anchor, loop and motion, velocity and
 *        timing, turned into the notes of a bar.
 *
 * **The matrices** are Dok. 8.2's: the probability of an onset on each sixteenth in the full groove. Values below one
 * are rolled; the layer's kind says when:
 *  - **Anchor** (kick, offbeat hat): every bar the same inside a block (Myloops: "identical every bar, changes only at
 *    32 bars and more").
 *  - **Loop** (open hat, ride, claps, toms, rim, bass): a loop of 1, 2 or 4 bars (p 0.10 / 0.55 / 0.35), rolled once
 *    per block; bar 3 repeats bar 1, and only bars 2 and 4 mutate ("Mutation nur in Takt 2 bzw. 2/4", Dok. 8.2).
 *  - **Motion** (ghost kick, rolling hat, clap ghosts, shaker): rolled anew every bar -- ghosts at 40 to 70 %, the
 *    spine at 100 %, velocity +-15 to 20 (Myloops, Dok. 5).
 *
 * **Rules** (Dok. 8.2, hard): the quarters 1/5/9/13 belong to the kick (clap and rim may share them; the bass never);
 * one hat per step (the open hat wins over the offbeat hat, which wins over the rolling hat); the collision dip
 * (percussion on a kick or hat step loses 25 % of its velocity; the rim is played 4 ms early instead).
 *
 * **Velocity and timing** (Dok. 8.3): each layer's velocity range, accents and random spread; a feel offset drawn per
 * track in the layer's range; MPC swing (Linn: every even sixteenth late by (S - 50) % of an eighth) on the layers
 * with a swing share, half of it on the bass and the ride; a random late jitter (half-normal, compose.humanize) --
 * never on the kick, never early (Fruehauf et al.: early is worse than late).
 *
 * **Determinism.** Every roll comes from a seed mixed from the track's seed, the layer and the bar (or the block), so
 * bar 37 realised alone equals bar 37 realised in sequence.
 *
 * **Phase 2** (27.09.2026). Cyclic layers: a pattern of @c period sixteenths that runs against the bar -- a polymeter of
 * 3, 5, 6, 7 or 12 steps, reset every 16 bars (Dok. 8.2), or a slipping loop of 15 or 17 steps that moves a sixteenth
 * against the bar every bar and meets it again after 15 or 17 bars (Erg. 5), reset at the block. Euclidean patterns
 * E(3,8), E(5,16), E(7,16) rotated off the quarters (Toussaint). Displacement: a whole percussion layer moved by one to
 * three sixteenths (Butler). Ghost chains (Erg. 7): a motion layer's probabilities multiplied by what its own last two
 * steps did (a ghost halves the next step's chance, three quarters the one after), by what the other ghosts already
 * took (halved), and raised by half where the same place half a bar before stayed empty (the push before beat 4 when
 * beat 2 had none). Trig conditions (Elektron's A:B): an extra open hat in the second of four bars, a single shaker in
 * the third. Fills: a sixteenth roll of the tom or conga through the sixteenths after the last quarter of an
 * eight-bar phrase, rising, p 0.25, never a snare roll. The ping (Ping.h) plays a cyclic figure of one or two pitch classes.
 */
#pragma once
#include "tot/Params.h"
#include "tot/Score.h"
#include <cstdint>
#include <string>
#include <vector>

namespace tot {

constexpr int kSteps = 16;   ///< sixteenths per bar

/** @brief The layers of the rack (Dok. 8.2). Appended to, never reordered. */
enum class LayerId : int { Kick = 0, GhostKick, ClosedHat, RollingHat, OpenHat, Ride, ClapA, ClapB, ClapGhost, Shaker,
                           TomConga, Rim, Bass,
                           /** Phase 2: the ping's figure (always cyclic). */
                           Ping,
                           /** Phase 3: the dub chord's stabs, the drone (a note every eight bars), the 303 line, the
                            *  texture (a note every bar). */
                           Chord, Drone, Acid, Texture, Count };
constexpr int kNumLayers = static_cast<int>(LayerId::Count);   ///< number of layers
extern const char* const kLayerNames[kNumLayers];             ///< "kick", "ghost kick", ...

/** @brief When a layer's probabilities are rolled. */
enum class LayerKind : uint8_t { Anchor, Loop, Motion };

/** @brief The fixed description of a layer. */
struct LayerDef {
    LayerKind kind;          ///< when its values below one are rolled
    float p[kSteps];         ///< onset probability per step in the full groove
    float vel[kSteps];       ///< velocity per step (0..1; 0 where p is 0)
    float velRandom;         ///< +- random velocity (0..1 units)
    float offsetLoMs;        ///< the feel offset's range, ms (drawn per track)
    float offsetHiMs;
    float swing;             ///< share of the swing: 0, 0.5 or 1
    double length;           ///< note length in beats
    bool perc;               ///< dips when it meets a kick or a hat
};
/** @brief The description of layer @p id. */
const LayerDef& layerDef(LayerId id);

/** @brief An Elektron-style trig condition: an extra onset of @c layer on @c step in the a-th of every b bars. */
struct TrigCond {
    LayerId layer = LayerId::OpenHat;   ///< which layer
    int step = 0;                       ///< on which sixteenth
    int a = 1, b = 4;                   ///< plays in bar a of every b (1-based)
};

/** @brief What a track decides once about its layers. */
struct RackPlan {
    uint64_t seed = 1;                  ///< the track's pattern seed
    int loopBars[kNumLayers] = {};      ///< loop length of each Loop layer: 1, 2 or 4
    float offsetMs[kNumLayers] = {};    ///< each layer's feel offset
    bool clapB = false;                 ///< the displaced clap (Dok. 8.2, variant B) instead of 2 and 4
    int tomLane = 9;                    ///< which kit lane plays the tom/conga layer (9 tom, 10 conga)
    int keyRoot = 9;                    ///< pitch class of the key
    int scale = 0;                      ///< compose.scale
    int bassRoot = 45;                  ///< the bass line's root as a MIDI note (41 .. 82 Hz)
    int bassSet[4] = { 0, 0, 0, 0 };    ///< the bass alphabet (semitones above the root, Dok. 4); bassSize of them
    int bassSize = 1;
    float swing = 53.0f;                ///< MPC swing, per cent
    float humanizeMs = 3.0f;            ///< jitter's standard deviation
    // Phase 2.
    int period[kNumLayers] = {};        ///< a cyclic layer's period in sixteenths (0: the sixteen-step matrix)
    int resetBars[kNumLayers] = {};     ///< where a cyclic layer starts over: every 16 bars (polymeter) or 32 (slipping)
    uint64_t cycle[kNumLayers] = {};    ///< a cyclic layer's onsets, bit k for position k of its period
    uint16_t euclid[kNumLayers] = {};   ///< a Euclidean pattern replacing the matrix (0: none), bit s for step s
    int displace[kNumLayers] = {};      ///< a whole layer moved later by this many sixteenths
    int pingRoot = 57;                  ///< the ping's root note (220 .. 415 Hz)
    int pingNote[64] = {};              ///< the ping's pitch at each position of its period
    TrigCond conds[4];                  ///< trig conditions
    int nConds = 0;
    float fillChance = 0.25f;           ///< a fill at the end of an eight-bar phrase (Dok. 8.2)
    /**
     * @name The style's restlessness (27.09.2026, the reference measurement, PLAN 13.4)
     * The references' bar similarity (the median correlation of a bar's onset profile with the bar one, two or four
     * before) is 0.87 for the hypnotic records, 0.92 to 0.93 for Raw and Dub, 0.955 for Ostgut: the hypnotic school is
     * the restless one. Dok. 8.2's motion, rolled every bar, gives the study 0.87; rolled once per block, 0.98-0.99. The
     * share of the motion steps rolled anew every bar sets it in between (seeds 7 and 8, seven minutes: Hypnotic 1.0 ->
     * 0.87, Raw 0.3 -> 0.93, Dub 0.25 -> 0.94, Ostgut 0.1 -> 0.97).
     * @{ */
    float mutation = 0.25f;             ///< share of a loop's open steps re-rolled in its bars 2 and 4
    float motionScale = 1.0f;           ///< factor on the motion layers' probabilities
    float reroll = 1.0f;                ///< share of the motion layers' steps rolled anew every bar (the rest once per block)
    float oneBarLoops = 0.10f;          ///< chance of a one-bar loop (the rest two and four bars, 0.55 : 0.35)
    /** @} */
    /**
     * @name Phase 3: the harmony (Dok. 8.6, PLAN 7.5)
     * One chord a track (p 0.8): i 0.5, i7 0.3, i add9 0.1, i with the fourth in the bass 0.1, in close position from the
     * root in 220 .. 415 Hz (thirds over 200 Hz); with p 0.2 a second chord a shuttle away -- bII 0.3, bVII 0.25,
     * iv 0.25, bIII 0.2 -- every second or fourth bar, a triad stacked in the scale's thirds (so it stays in the key).
     * Never V-i, never the leading tone, never a major seventh; add9 only where the scale has the ninth.
     * @{ */
    int chordRoot = 57;                 ///< the chord's root note
    int chordTones[5] = { 0, 3, 7 };    ///< semitones above the root (the fourth in the bass below it: -7)
    int nChordTones = 3;
    int shuttle = 0;                    ///< the second chord's root, semitones from the first (0: none)
    int shuttleTones[3] = {};           ///< the second chord, semitones above chordRoot
    int shuttleBars = 0;                ///< it plays every shuttleBars-th bar (2 or 4), 0: never
    int droneNote = 57;                 ///< the drone's note (tonic or fifth; the voice keeps it above 150 Hz)
    int acidRoot = 45;                  ///< the 303 line's root, an octave over the bass's
    uint16_t acidMask = 0x0FFF;         ///< Phase 4: the pitch classes above the root the 303 may play (bit k: k semitones)
    /** @} */
    /**
     * @brief Phase 4: each layer's own seed for its rolls (PLAN 7.8). All start as the track's seed; the composer
     *        draws one layer anew by giving it another, so a layer is locked while the rest is rerolled and back.
     */
    uint64_t layerSeed[kNumLayers] = {};
    /**
     * @name Phase 8: the figure (28.09.2026)
     * The track's signature voice (Composer.h) -- what the canon's tracks are remembered by: the motif of "The Bells",
     * the bassline of "Energy Flash", the dub chords of "Dawning". The 303, the chord's stabs or the bass play a motif of
     * one or two bars that stays the same from its entry to the track's end (Myloops' anchor: "identical every bar") and
     * moves only by its knobs, in the composer's waves; the ping's figure is its cycle, made richer (makeFigure).
     * @{ */
    LayerId figure = LayerId::Count;    ///< the signature voice (Count: none)
    int figBars = 1;                    ///< the motif's length: 1 or 2 bars
    uint32_t figOn = 0;                 ///< its onsets, bit bar * 16 + step
    int8_t figPitch[32] = {};           ///< semitones above the voice's root at each place
    uint32_t figAccent = 0;             ///< accented places
    uint32_t figSlide = 0;              ///< places that glide into the next one (the 303)
    double figLength = 0.5;             ///< a stab's length in beats
    /** @} */
};

/** @brief What a track decides about its rack before the rack draws its plan (Phase 4: from the style's profile). */
struct RackSettings {
    int keyRoot = 9;                    ///< pitch class of the key
    int scale = 0;                      ///< compose.scale
    float swing = 53.0f;                ///< MPC swing, per cent
    float humanizeMs = 3.0f;            ///< jitter's standard deviation
    float mutation = 0.25f;             ///< RackPlan::mutation
    float motionScale = 1.0f;           ///< RackPlan::motionScale
    float reroll = 0.15f;               ///< RackPlan::reroll
    float polymeterChance = 0.5f;       ///< one or two cyclic percussion layers
    float fillChance = 0.25f;           ///< RackPlan::fillChance
};
/** @brief The settings the knobs describe: compose.key, scale, swing, humanize, and compose.style's profile. */
RackSettings rackSettings(const ParamStore& p);
/**
 * @brief Makes the plan of a track.
 * @param s    what the track decided
 * @param seed the track's pattern seed
 */
RackPlan makeRackPlan(const RackSettings& s, uint64_t seed);
/** @brief makeRackPlan(rackSettings(p), seed). */
RackPlan makeRackPlan(const ParamStore& p, uint64_t seed);
/**
 * @brief Phase 8: makes @p voice the plan's figure and writes its motif (RackPlan::figure).
 *
 * - **The 303**: a line of one bar (p 0.45) or two, 50 to 75 % of the off-quarter sixteenths sounding, pitches from
 *   its alphabet (root 0.45, octave 0.2, fifth, flat seventh and minor third 0.1 each, fourth 0.05, in the scale),
 *   accents 0.3, slides 0.2 into a sounding sixteenth; the second bar answers the first with two to four places changed.
 * - **The chord's stabs**: a rhythm of the dub-techno idiom -- the offbeat eighths (Basic Channel), the downbeat and the
 *   third beat's offbeat (Dok. 8.2's stab), the tresillo 3-3-2, a late pair, one long chord a bar ("Dawning"), a call of
 *   three and an answer of two over two bars, E(5,16) off the downbeat.
 * - **The bass**: a riff off the quarters -- the offbeat bass, the gallop, a rolling line, a syncopated or a sparse
 *   one; the octave on one or two places (p 0.3 each), the alphabet's second tone at the end of the motif.
 * - **The ping**: a cycle of 16 (0.35), 12 (0.2), 7 (0.25) or 5 (0.2) sixteenths, Euclidean and rotated, of up to three
 *   tones -- the root on its first onset, the others root 0.45, a second tone 0.35 (fifth, flat seventh, octave, minor
 *   third, fourth), a third 0.2.
 *
 * LayerId::Count clears the figure. The motif comes from @p seed alone (the composer's stream "figure").
 */
void makeFigure(RackPlan& plan, LayerId voice, uint64_t seed);

/** @brief What the form asks of the rack in one bar. */
struct BarSpec {
    int bar = 0;                            ///< absolute bar index (0-based) of the track
    int block = 0;                          ///< its 32-bar block
    int barInBlock = 0;                     ///< 0 .. 31
    double beat = 0.0;                      ///< the bar's first beat in the score
    double bpm = 130.0;                     ///< the tempo there (for the ms offsets)
    bool active[kNumLayers] = {};           ///< which layers play
    float density[kNumLayers] = {};         ///< a factor on the probabilities below one (1 = the full groove)
    bool muteStep[kSteps] = {};             ///< steps where nothing but the kick plays ("mute one expected hit", Dok. 8.5)
    bool fills = true;                      ///< fills allowed in this bar
    uint32_t variant = 0;                   ///< Phase 4: the block's candidate (PLAN 7.9); 0 the plain rolls
};
/** @brief A BarSpec with every layer off and every density at one. */
BarSpec emptyBar(int bar, double beat, double bpm);

/**
 * @brief The notes of one bar.
 * @param plan the track's plan
 * @param spec what the form asks
 * @param out  receives the notes (appended; beats absolute, with offsets, swing and jitter)
 */
void realizeBar(const RackPlan& plan, const BarSpec& spec, std::vector<NoteEvent>& out);

/**
 * @brief Which steps of layer @p id sound in a bar, before velocity and timing: the rolls, the cycle, the ghost chain,
 *        the displacement -- without the rules that involve the other layers (for the tests and the displays).
 * @param busy steps the other ghosts and percussion already took (the ghost chain's cross factor), or null
 */
void rollSteps(const RackPlan& plan, LayerId id, int bar, int block, float density, bool* on, const bool* busy = nullptr,
               uint32_t variant = 0);

/** @brief k onsets spread as evenly as possible over n steps, the first on step 0 (Toussaint's Euclidean rhythm). */
uint64_t euclidMask(int k, int n);
/** @brief @p mask of @p n steps rotated later by @p r. */
uint64_t rotateMask(uint64_t mask, int n, int r);
/**
 * @brief @p k onsets spread as evenly as possible over the twelve sixteenths of a bar that are no quarter (1, 2, 3, 5,
 *        ... 15) -- E(k,12) there, rotated by @p r of them; a bar's mask, bit s for step s.
 */
uint16_t offQuarterMask(int k, int r);

/**
 * @brief A layer's bar in Tidal/Strudel mini-notation (PLAN 6.7): sixteen steps as "bd ~ ~ ~ ...", a cyclic layer as a
 *        polymetric "{x ~ x ~ ~}%16".
 * @param name the sound's name in the notation ("bd", "hh" ...)
 */
std::string miniNotation(const RackPlan& plan, LayerId id, int bar, const char* name);

/** @brief The kit lane a layer plays on, or -1 (the kick and the bass are no lanes). */
int layerLane(const RackPlan& plan, LayerId id);

} // namespace tot
