/**
 * @file Set.h
 * @brief The set composer (PLAN 7.1, 7.7): hours of tracks on two decks, mixed as a Berlin DJ mixes them.
 *
 * **Dramaturgy.** A set follows one of five arcs of energy and tempo: Warm-up (125 -> 130 BPM, rising), Peak
 * (128 -> 134, highest at 70 %), Closing (132 -> 127, falling), Sunday (126 -> 128, a slow wave) and Flat (130). The tempo
 * moves monotonically, at most 1 BPM a track, and ramps inside the blend. With set.journey the styles wander with the
 * energy along Dub, Hypnotic, Ostgut, Raw (morphed between neighbours), centred on the knobs' style (energy 0.5 is it,
 * the extremes a rung and a fifth away); else the knobs' style plays throughout.
 *
 * **Keys.** No transposition (Dok. 6): each track draws its own key, a Camelot neighbour of the last (p 0.6: a fifth up or
 * down), the same (p 0.25) or any (p 0.15).
 *
 * **The blend** (Dok. 6, 8.8). The outgoing track's outro begins on a 32-bar line; that line is the **bass swap**: the
 * incoming track, on the other deck, is placed so its body -- its bass's entry -- begins exactly there. Its intro runs
 * under the outgoing track with its low band killed at the isolator, its fader opening 16 or 32 bars before the swap
 * (set.blend); at the swap the outgoing deck's low band closes and the incoming one's opens in the same sample, and the
 * outgoing deck fades through its outro. Only one deck ever owns the band under 200 Hz.
 *
 * **Live recomposition** (Rodhad, DVS1). At a swap (p set.loops) a loop of four or eight bars of the outgoing track --
 * its hats, its percussion, its ping, from its loudest block -- plays on under the new one on the third deck for 32 or
 * 64 bars, its low band always killed, in the outgoing track's own sounds.
 *
 * **Breaks from the mixer** (p set.fx_breaks): at a swap the outgoing deck is thrown into the mixer's echo and hall for a
 * beat, a break where the track has none.
 *
 * **Streams.** The set's own choices (keys, lengths, loops) on the stream `set`; each track on its own seed (reroll
 * `track<n>`) and its units as `track<n>.form` and so on (SetFile.h).
 */
#pragma once
#include "umb/Params.h"
#include "umb/Score.h"
#include "umb/SetFile.h"
#include "umb/compose/Composer.h"
#include <cstdint>
#include <string>
#include <vector>

namespace umb {

extern const char* const kDramaturgyNames[];   ///< "Warm-up", "Peak", "Closing", "Sunday", "Flat"

/** @brief One track of a set, where it lies. */
struct SetTrack {
    int deck = 0;              ///< 0 or 1
    double start = 0.0;        ///< set beat of its first bar
    double swapIn = 0.0;       ///< set beat where its low end opens (its body's first bar; the set's start for the first)
    double swapOut = 0.0;      ///< set beat where it gives the low end up (the next track's swap; its end for the last)
    double end = 0.0;          ///< set beat of its end
    float energy = 0.0f;       ///< the set's energy at its start
    uint64_t seed = 0;         ///< its seed
    TrackInfo info;            ///< what it told
};

/** @brief A loop on the third deck. */
struct SetLoop {
    int from = 0;              ///< the track it comes from
    double start = 0.0;        ///< set beat
    double end = 0.0;
    int bars = 4;              ///< the loop's length
};

/** @brief What a set tells. */
struct SetInfo {
    Dramaturgy dramaturgy = Dramaturgy::Peak;
    std::vector<SetTrack> tracks;
    std::vector<SetLoop> loops;
    std::vector<double> breaks;   ///< set beats of the mixer's breaks
};

/**
 * @brief Writes a set of about @p minutes.
 * @param p        the knobs (set.*, compose.*)
 * @param seed     the set's seed
 * @param minutes  its length; the last track ends at or after it
 * @param curation rerolls, or null
 * @param info     receives where everything lies, or null
 */
SetScore composeSet(const ParamStore& p, uint64_t seed, double minutes, const Curation* curation = nullptr, SetInfo* info = nullptr);

/** @brief The energy of dramaturgy @p d at @p t (0..1 of the set). */
float setEnergy(Dramaturgy d, float t);
/** @brief The tempo of dramaturgy @p d at @p t, before the 1-BPM rule. */
float setTempo(Dramaturgy d, float t);

/** @brief All of a set as one score (the decks' notes and markers on the set's tempo; for MIDI and the displays). */
Score flattenSet(const SetScore& set);

} // namespace umb
