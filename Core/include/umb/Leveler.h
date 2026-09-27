/**
 * @file Leveler.h
 * @brief Every track as loud as its style means (PLAN 8.5): the loudest part of each track measured, and a correction of
 *        the master gain that brings it to its style's target.
 *
 * What plays -- which layers enter, whether a chord or a drone comes, which kick -- is drawn anew each time, so tracks of
 * one style come out dB apart. The composer marks where each track is at its loudest and what that part should measure
 * (LevelMark); levelScore renders that part -- a few seconds before it for the rooms and the echo to fill and the notes
 * that sound on across the jump to be found again, then the part itself -- measures it (BS.1770, gated) and sets the
 * correction (LevelMark::trimDb, at most 4 dB either way). The master compresses, clips and limits after the gain, so
 * the part is measured a second time with the correction and the rest put right. The player's master level is left out
 * of the measurement and comes on top.
 *
 * The correction is part of the score: live and offline play it alike, and a score saved or exported carries it. The
 * measuring renders about 50 seconds per track: umb_render does it before it renders, the plugin while the track already
 * plays -- the correction follows and glides in (Engine::setLevelTrims).
 *
 * @note Copied from Ephemeris `Core/include/eph/Leveler.h` (namespace eph, prefix EPH_) at d047d79 (27.09.2026); the
 *       pieces are tracks here, the targets the styles' (PLAN 8.5, measured against the references).
 */
#pragma once
#include "umb/Params.h"
#include "umb/Score.h"
#include <functional>
#include <vector>

namespace umb {

/** @brief What levelScore found for one track. */
struct LevelReading {
    double beat = 0.0;       ///< the track's start
    float measured = 0.0f;   ///< its loudest part as composed, LUFS
    float target = 0.0f;     ///< what it should measure
    float trim = 0.0f;       ///< the correction set, dB
    float after = 0.0f;      ///< the part with the correction, LUFS
};

/**
 * @brief Measures every track of @p score (its LevelMarks) with the knobs of @p params and sets their corrections.
 * @param seconds how much of each loudest part is measured
 * @param stop    asked between blocks: true breaks off, and @p score is left as it was
 * @return what was found, a reading per track (nothing if broken off)
 */
std::vector<LevelReading> levelScore(Score& score, const ParamStore& params, double seconds = 20.0,
                                     const std::function<bool()>& stop = {});

/**
 * @brief Levels every track of a set: each deck's tracks as levelScore does (a deck alone, without the mixer's moves); a
 *        mark without a target (a live loop on the third deck, Set.h) takes the correction of the track whose loudest
 *        part it names.
 * @return the readings of decks A and B, in deck order
 */
std::vector<LevelReading> levelSet(SetScore& set, const ParamStore& params, double seconds = 20.0,
                                   const std::function<bool()>& stop = {});

/** @brief The loudness the loudest part of a track of style @p style should measure, LUFS (PLAN 8.5). */
float styleTargetLufs(int style);

} // namespace umb
