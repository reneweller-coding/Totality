/**
 * @file Style.h
 * @brief The four style profiles (PLAN 2.8, 7.6) as the numbers the composer draws from.
 *
 * A profile is a vector of ranges and chances: tempo, the weights of the three forms, who owns the low end, the layer
 * pool, the rack's restlessness and cycles, the swing, the density cap and the rate of events, the hypnosis corridor,
 * a sound recipe and the knobs drawn anew for every track, and the loudness target. Between two profiles the numbers
 * are interpolated (morphProfile, compose.morph_to); the two axes of Dok. 8.0 -- the Dub share and the Hypnotic share --
 * pull any profile towards those two (axisProfile).
 *
 * **Sources of the numbers.** Tempo, forms, low owner and the layer chances are PLAN 2.8's (itself [I] from Dok. 1, 3,
 * 8.0); the rack's restlessness and the corridor are calibrated against the references' bar similarity (PLAN 13.4); the
 * loudness targets are the references' loudest 20 seconds; the recipes put right what the Phase 3 fit over all four
 * styles left per style (PLAN, Stand der Umsetzung).
 *
 * @note The shape (designated initialisers, morph) follows Ephemeris `Core/include/eph/compose/Style.h` at d047d79
 *       (27.09.2026); the fields are Umbra's.
 */
#pragma once
#include "umb/Params.h"
#include "umb/pattern/Rack.h"
#include <vector>

namespace umb {

/** @brief A knob a style sets at a track's start (real units). */
struct SoundValue {
    const char* key;   ///< the parameter's key ("master.tilt")
    float value;       ///< its value
};

/** @brief A knob drawn anew for every track, uniformly in [low, high] (real units; Log knobs drawn on their curve). */
struct SoundRange {
    const char* key;   ///< the parameter's key
    float low;         ///< lowest value
    float high;        ///< highest value
};

/** @brief A layer's chance to be in a track's pool (the rest of the pool is fixed: kick, offbeat and rolling hat). */
struct LayerChance {
    LayerId layer;     ///< which layer
    float chance;      ///< 0..1
};

/** @brief One style profile. */
struct StyleProfile {
    const char* name;                    ///< the profile's name, as in compose.style
    float bpmLow;                        ///< lowest tempo
    float bpmHigh;                       ///< highest tempo
    float arcWeight;                     ///< the weight of the Arc (the tool: intro, body, outro)
    float peakWeight;                    ///< of the Peak (a long reduction and the densest block after it)
    float endlessWeight;                 ///< of the Endless (Erg. 8: full from bar 1, no intro, no outro)
    float subChance;                     ///< the sub bass owns the low end (else the rumble)
    float intro64Chance;                 ///< an intro (and outro) of 64 bars instead of 32
    int blocksLow;                       ///< shortest track in 32-bar blocks
    int blocksHigh;                      ///< longest
    std::vector<LayerChance> pool;       ///< the layers a track may have, with their chances
    float mutation;                      ///< RackPlan::mutation
    float motionScale;                   ///< RackPlan::motionScale
    float reroll;                        ///< RackPlan::reroll
    float polymeterChance;               ///< one or two cyclic percussion layers in a track
    float swingLow;                      ///< MPC swing range, per cent
    float swingHigh;
    float fillChance;                    ///< a fill at the end of an eight-bar phrase
    float toolReductionChance;           ///< an Arc has a short kick-out at all (Dok. 8.5: 0.5)
    float edgeChance;                    ///< the intro and outro filtered by the group high pass (their loudness moving)
    int maxReduction;                    ///< the longest kick-out in bars (Dok. 8.5's Peak: 8, 16 or 32)
    float eventRate;                     ///< chance of an event on each 8-bar line of the body
    float throwShare;                    ///< share of the events that are delay throws
    int densityCap;                      ///< at most this many layers at once beside kick and rumble
    float simTarget;                     ///< the corridor: the symbolic bar similarity a block aims at
    float microTarget;                   ///< the corridor: the symbolic micro-change, dB per bar and band
    std::vector<SoundValue> recipe;      ///< knobs set at every track's start
    std::vector<SoundRange> sounds;      ///< knobs drawn per track
    float peakLufs;                      ///< what a track's loudest part should measure (Leveler.h)
    float styleMix[4];                   ///< how much of Hypnotic, Ostgut, Dub and Raw it is (the presets' fit, Presets.h)
};

/** @brief The profile of a style. */
const StyleProfile& styleProfile(Style style);

/**
 * @brief A profile between @p a and @p b: at @p t = 0 exactly @p a, at 1 exactly @p b. Numbers are interpolated (counts
 *        rounded), the pool's chances per layer (a layer only one has counts 0 in the other), the recipes per knob (a
 *        knob only one sets counts its default in the other), the sound ranges per knob; the name is the nearer one's.
 */
StyleProfile morphProfile(const StyleProfile& a, const StyleProfile& b, float t, const ParamStore& p);

/** @brief @p base pulled towards Dub by @p dubShare and then towards Hypnotic by @p hypnoticShare (Dok. 8.0's axes). */
StyleProfile axisProfile(const StyleProfile& base, float dubShare, float hypnoticShare, const ParamStore& p);

/** @brief The profile the knobs describe: compose.style, morphed towards compose.morph_to by compose.morph, and the axes. */
StyleProfile profileOf(const ParamStore& p);

} // namespace umb
