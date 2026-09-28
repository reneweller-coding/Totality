/**
 * @file Corridor.h
 * @brief The hypnosis corridor on the score (PLAN 2.9, 7.9): the symbolic twins of the reference measurement's audio
 *        measures, so that what the composer aims at and what the references measure share a unit.
 *
 * A bar becomes an onset profile -- the velocities summed per band (low: kick, ghost kick, bass; mid: claps, toms, rim,
 * ping, chord, 303; high: hats, ride, shaker) and sixteenth -- as analyze_ref.py turns a bar of audio into the flux of
 * three bands per sixteenth. From the profiles:
 *  - **similarity**: a bar's correlation with the bar one, two or four before, the best of the three (a loop of one, two
 *    or four bars counts as repetition), the median over a stretch -- the threshold-free measure that separates the
 *    schools (PLAN 2.9 Nachtrag: Hypnotic 0.87, Ostgut 0.955);
 *  - **micro-change**: the mean absolute change of each band's energy from bar to bar, dB;
 *  - **density**: onsets per bar.
 */
#pragma once
#include "tot/Score.h"
#include "tot/pattern/Rack.h"
#include <vector>

namespace tot {

/** @brief One bar's onset profile: velocity per band (0 low, 1 mid, 2 high) and sixteenth, and its onset count. */
struct BarProfile {
    float v[3][kSteps] = {};
    int onsets = 0;
};

/** @brief The band of every part for a plan's lanes (-1: not counted: the drone, the texture, the noise lane). */
void partBands(const RackPlan& plan, int* bandOfPart);

/**
 * @brief The profiles of bars @p firstBar .. @p firstBar + @p count - 1 (4 beats each, from beat 0) of @p notes.
 * @param bands partBands()'s table
 */
std::vector<BarProfile> barProfiles(const std::vector<NoteEvent>& notes, int firstBar, int count, const int* bands);

/** @brief The measures of a stretch. */
struct CorridorStats {
    float similarity = 0.0f;   ///< median best correlation with the bar 1, 2 or 4 before
    float micro = 0.0f;        ///< mean |change| of the bands' energy from bar to bar, dB
    float density = 0.0f;      ///< mean onsets per bar
};

/**
 * @brief The measures of bars @p from .. @p to - 1 of @p bars (the bars before @p from serve as the lags' context; a bar
 *        without four predecessors counts only the lags it has).
 */
CorridorStats corridorOf(const std::vector<BarProfile>& bars, int from, int to);

} // namespace tot
