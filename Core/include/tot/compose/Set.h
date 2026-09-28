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
 * **A track's time** (Phase 9, Mix-Dok. 6 and 8: "Klocks Berghain 04: jeder Track 'a good two or three minutes'",
 * Fabric 66: 24 tracks in 73 minutes). A set's track is as long as its own time asks (set.track_minutes, three minutes
 * by default: its body, swap to swap, in whole blocks at its tempo, the fraction drawn), with an intro and an outro of 32
 * bars each that lie under its neighbours -- about twenty tracks an hour, each heard for four to five minutes, a third of
 * it under another.
 *
 * **The blend** (Dok. 6, 8.8; Mix-Dok. 7, "Ablauf Takt für Takt"). The outgoing track's outro begins on a 32-bar line; that
 * line is the **bass swap**: the incoming track, on the other deck, is placed so its body -- its bass's entry -- begins
 * exactly there. Its intro runs under the outgoing track with its low band killed at the isolator; its fader opens 16 or
 * 32 bars before the swap (set.blend) with its highs a little down and its mids well down, the highs coming up over the
 * first quarter of the blend and the mids over its last 16 bars; the outgoing deck's low band comes down 6 dB over those
 * 16 bars. At the swap the outgoing deck's low band closes and the incoming one's opens in the same sample; the outgoing
 * deck keeps its hats (its mids fall in 8 bars, its highs over 16) and its fader falls through the next 16 bars. Only
 * one deck ever owns the band under 200 Hz.
 *
 * **Live recomposition** (Rodhad, DVS1; Mix-Dok. 6: "Zwei bis drei Tracks laufen ständig, einzelne Elemente werden aus
 * einer Platte geborgt"). The third deck plays what is borrowed, in the source track's own sounds and with its low band
 * always killed, one thing at a time (p set.loops each):
 *  - **the carry**: a loop of four or eight bars of the outgoing track's hats and percussion (from its loudest block) plays
 *    on for 32 or 64 bars as its own fader falls;
 *  - **the tease**: the incoming track's figure (its first bars) comes in 16 or 32 bars before its blend, high-passed and
 *    opening, and hands over to the track itself -- only where the keys agree (the same or a fifth apart);
 *  - **the layer**: a percussion loop of the track before last under a track's core for 16 or 32 bars ("ein dritter
 *    Layer, Hat- oder Perc-Loop eines weiteren Tracks").
 *
 * **The DJ's hand** (Phase 9, set.dj_hand; Mix-Dok. 6: "working the EQs, the effects, all precision"). Between the blends,
 * on a track's 16-bar lines, the channel moves: the low band killed for the last one or two bars before the line (the
 * kick slams back), the highs taken down 10 dB and brought back over 8 or 16 bars, a dip of the mids, the filter's high
 * pass drawn up into the line, an echo throw on the phrase's last beat. Never within four bars of the track's own
 * moments (its downs and breaths, Composer.h), so the two hands do not double.
 *
 * **Breaks from the mixer** (p set.fx_breaks): at a swap the outgoing deck is thrown into the mixer's echo and hall for a
 * beat, a break where the track has none; and the rest of a fading track goes into the echo as its fader closes.
 *
 * **Streams.** The set's own choices (keys, lengths, loops) on the stream `set`; each track on its own seed (reroll
 * `track<n>`) and its units as `track<n>.form` and so on (SetFile.h).
 */
#pragma once
#include "tot/Params.h"
#include "tot/Score.h"
#include "tot/SetFile.h"
#include "tot/compose/Composer.h"
#include <cstdint>
#include <string>
#include <vector>

namespace tot {

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

/** @brief What the third deck borrows (Phase 9). */
enum class LoopKind : int { Carry = 0, Tease, Layer, Count };
extern const char* const kLoopKindNames[];     ///< "carry", "tease", "layer"

/** @brief A loop on the third deck. */
struct SetLoop {
    int from = 0;              ///< the track it comes from
    double start = 0.0;        ///< set beat
    double end = 0.0;
    int bars = 4;              ///< the loop's length
    LoopKind kind = LoopKind::Carry;
};

/** @brief A move of the DJ's hand on a channel (Phase 9). */
enum class MoveKind : int { LowKill = 0, HighSwell, MidDip, FilterBuild, EchoThrow, Count };
extern const char* const kMoveNames[];         ///< "low kill", "high swell", "mid dip", "filter build", "echo throw"

struct SetMove {
    double beat = 0.0;         ///< the line it leads into (or starts on)
    int deck = 0;
    MoveKind kind = MoveKind::LowKill;
};

/** @brief What a set tells. */
struct SetInfo {
    Dramaturgy dramaturgy = Dramaturgy::Peak;
    std::vector<SetTrack> tracks;
    std::vector<SetLoop> loops;
    std::vector<double> breaks;   ///< set beats of the mixer's breaks
    std::vector<SetMove> moves;   ///< Phase 9: the DJ's hand
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

} // namespace tot
