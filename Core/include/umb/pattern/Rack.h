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
 */
#pragma once
#include "umb/Params.h"
#include "umb/Score.h"
#include <cstdint>
#include <vector>

namespace umb {

constexpr int kSteps = 16;   ///< sixteenths per bar

/** @brief The layers of the rack (Dok. 8.2). Appended to, never reordered. */
enum class LayerId : int { Kick = 0, GhostKick, ClosedHat, RollingHat, OpenHat, Ride, ClapA, ClapB, ClapGhost, Shaker,
                           TomConga, Rim, Bass, Count };
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
};
/**
 * @brief Makes the plan of a track from the knobs (compose.*) and a seed.
 * @param p    the knobs
 * @param seed the track's pattern seed
 */
RackPlan makeRackPlan(const ParamStore& p, uint64_t seed);

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
 * @brief Which steps of layer @p id sound in a bar, before velocity and timing: the rolls and the rules without the
 *        other layers (for the tests and the displays).
 */
void rollSteps(const RackPlan& plan, LayerId id, int bar, int block, float density, bool* on);

/** @brief The kit lane a layer plays on, or -1 (the kick and the bass are no lanes). */
int layerLane(const RackPlan& plan, LayerId id);

} // namespace umb
