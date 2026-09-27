/**
 * @file Study.h
 * @brief The study of Phase 1 (PLAN 14): one track of 32-bar blocks with one operation each, the reduction, the
 *        micro-automation -- the form grammar of PLAN 7.2 in its first, fixed shape, so that the low end, the hats
 *        and the rack can be heard as a track before the composer of Phase 4 exists.
 *
 * The shape (for n blocks, n >= 4; the kick and the rumble play wherever the kick plays):
 * @code
 *   block 0        intro: kick; the offbeat hat from bar 8; the rolling hat at half density from bar 16;
 *                  the hats' bus opens from 1.5 kHz over bars 8 .. 32 (Dok. 8.5: "Perc-Bus LP 800 Hz -> offen")
 *   block 1        add: the ghost kicks -- or, where the sub owns the low end, the bass
 *   block 2 ..     one operation each: add the open hat, the rolling hat's other half, the shaker, the tom/conga,
 *                  the rim, the ride (these four shuffled), then hold
 *   the reduction  in the block at about 55 % of the body: kick out for 8 bars (bars 16 .. 23), a noise swell over
 *                  them, bar 23 a one-bar dropout, the return on bar 24 (Dok. 8.5)
 *   last 2 blocks  the outro, subtractive: the layers leave in the reverse order; the last 16 bars kick and hat
 * @endcode
 * Micro-events (PLAN 7.4): every 16 bars the rolling hat's decay moves a few per cent; the open hat's decay rises
 * from 100 to 400 ms over the 16 bars after it enters; the rumble's hall grows by a fifth over the track.
 */
#pragma once
#include "umb/Params.h"
#include "umb/Score.h"
#include "umb/pattern/Rack.h"
#include <cstdint>

namespace umb {

/**
 * @brief Composes the study.
 * @param p    the knobs (compose.bpm, compose.minutes, compose.low_owner, the key and scale, the swing)
 * @param seed the seed
 * @return the score, sorted
 */
Score composeStudy(const ParamStore& p, uint64_t seed);

} // namespace umb
