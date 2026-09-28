/**
 * @file Composer.h
 * @brief The composer (PLAN 7): one track from a seed and a style profile.
 *
 * **Form** (PLAN 7.2, Dok. 8.5, Erg. 8). Three types: the **Arc** (the tool: intro, body, outro; no kick-out or a short
 * one of 4 or 8 bars), the **Peak** (a kick-out of 8, 16 or 32 bars at 50 to 65 % of the body, a swell, a cut, the
 * densest block after the return) and the **Endless** (full from bar 1, no intro, no outro, no mute, no reduction: all
 * dynamics in the automation and the cycles). Intro and outro are 32 or 64 bars of beat -- kick, hats, one perc, no bass,
 * no stab, and with p 0.7 no rumble either (it comes with the body, where a set swaps the low end) -- the outro the
 * intro's mirror, subtractive. Every block has exactly one operation (Add, Remove, Swap, Hold),
 * chosen by a density profile over the track (Tool 0.3 -> 0.6 -> 0.85 -> 1.0 -> 0.6 -> 0.3; the Peak cut in its
 * reduction and full after it); the layers enter in Dok. 8.5's order (hats and ride, clap and perc, bass, stab, pad and
 * texture), at least two of them only in the second half; a sub bass enters with the body. Big changes (bass,
 * reduction, return) fall on 16 or 32-bar lines, everything on 4-bar lines; no tonal material in the first and last 32
 * bars (except the Endless).
 *
 * **Events** on the body's 8-bar lines at the profile's rate: a mute of one expected hit, a one-bar dropout of the kick,
 * a delay throw (a ping, a stab or the hats into the echo, the feedback to the edge and back), a ghost more, a nudge of
 * a hat's decay; a sweep of the group high pass before a 32-bar line or a return.
 *
 * **Automation** on three time scales: micro (the hats' decays, every 16 bars), meso (two hands on a 16-beat grid moving
 * the filters and sends of what plays, their centre following the density; GestureEngine.h), macro (the rumble's hall and
 * the stab's brightness growing by a fifth over the track).
 *
 * **Harmony** (PLAN 7.5, Dok. 8.6): the key drawn (Aeolian 0.6, Dorian 0.15, the hexachord 0.1, Phrygian 0.1, the minor
 * pentatonic 0.05) or monotonic (0.08: no pitched layer but the kick's and the bass's roots); Dok. 8.9's check as a
 * filter -- at most four pitch classes in a track (the shuttle, the ping's second tone and the 303's alphabet give way in
 * that order), the bass at most two; its Camelot label.
 *
 * **Candidates and the hypnosis corridor** (PLAN 7.9): each block is realised eight times, each with its own variant of
 * the rack's rolls (the motion seeds); the one whose corridor measures (Corridor.h) lie nearest the profile's targets is
 * kept. Blocks are judged on their own bars, and the operations and events come from their own streams (not, as PLAN 7.9
 * first had it, from the candidates), so one block drawn again changes its own bars and nothing that follows.
 *
 * **Streams.** Every part is drawn on its own stream of the seed: `form`, `harmony`, `rack` (and `rack.<layer>`),
 * `layers`, `blocks` (and `block<n>`), `events`, `hands`, `sounds` (SetFile.h). A set asks for a track with its own
 * tempo, key, form, owner and energy (TrackRequest) and names its units `track<n>.`.
 */
#pragma once
#include "tot/Params.h"
#include "tot/Score.h"
#include "tot/SetFile.h"
#include "tot/compose/Style.h"
#include <cstdint>
#include <string>
#include <vector>

namespace tot {

/** @brief The three forms (PLAN 7.2). */
enum class FormType : int { Arc = 0, Peak, Endless, Count };
extern const char* const kFormNames[];   ///< "Arc", "Peak", "Endless"

/** @brief What a set asks of a track; every field left at its default is the composer's (or the knobs') to decide. */
struct TrackRequest {
    const StyleProfile* profile = nullptr;   ///< the profile (null: profileOf(the knobs))
    FormType form = FormType::Count;         ///< Count: compose.form, else drawn from the profile
    float bpm = 0.0f;                        ///< 0: drawn (compose.auto) or compose.bpm
    int key = -1;                            ///< -1: drawn (compose.auto) or compose.key
    int scale = -1;                          ///< -1: drawn or compose.scale
    int lowOwner = -1;                       ///< -1: drawn or compose.low_owner (0 rumble, 1 sub)
    int blocks = 0;                          ///< 0: from compose.minutes
    float energy = -1.0f;                    ///< the set's energy here, 0..1: towards the Peak and a higher density
    bool mixable = false;                    ///< a set's track: never Endless
};

/** @brief What a track tells the set, the cues and the displays. */
struct TrackInfo {
    FormType form = FormType::Arc;
    std::string style;           ///< the profile's name
    float bpm = 130.0f;
    int key = 9, scale = 0;
    bool monotonic = false;      ///< no pitched layer
    bool subOwns = false;        ///< the sub bass owns the low end
    int bars = 0;                ///< length
    int introBars = 0;           ///< the intro's length (0 for the Endless)
    int outroBar = 0;            ///< where the outro begins (bars for the Endless)
    int bassBar = 0;             ///< where the body begins: the bass's entry, a set's bass swap
    std::vector<int> reductions; ///< the first bar of each kick-out
    std::vector<int> returns;    ///< the bar of each return
    int peakBar = 0;             ///< where the loudest part begins (the LevelMark)
    std::string camelot;         ///< "8A", or "monotonic"
    std::vector<int> layers;     ///< the pool, in order of entry (LayerId)
    float similarity = 0.0f;     ///< the corridor over the body: bar similarity
    float micro = 0.0f;          ///< micro-change, dB
    float density = 0.0f;        ///< onsets per bar
};

/**
 * @brief Writes one track, beat 0 its first bar.
 * @param p        the knobs
 * @param seed     the track's seed
 * @param req      what a set asks (all defaults: a track alone)
 * @param curation rerolls, or null
 * @param unit     prefix of this track's units in @p curation ("" alone, "track3." in a set)
 * @param info     receives what the track tells, or null
 */
Score composeTrack(const ParamStore& p, uint64_t seed, const TrackRequest& req = TrackRequest{}, const Curation* curation = nullptr,
                   const std::string& unit = std::string(), TrackInfo* info = nullptr);

/** @brief The names of a track's units, in stream order. */
extern const char* const kUnitNames[8];

/** @brief The Camelot label of a minor key on pitch class @p key ("8A" for A). */
std::string camelotOf(int key);

} // namespace tot
