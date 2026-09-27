/**
 * @file Study.h
 * @brief The study of Phases 1 to 3 (PLAN 14): one track of 32-bar blocks with one operation each, the reduction, the
 *        micro-automation, delay throws and the loudness mark -- the form grammar of PLAN 7.2 in its first, fixed shape,
 *        so that the low end, the rack and the sound can be heard as a track before the composer of Phase 4 exists.
 *
 * The shape (for n blocks, n >= 4; the kick and the rumble play wherever the kick plays):
 * @code
 *   block 0        intro: kick; the offbeat hat from bar 8; the rolling hat at half density from bar 16;
 *                  the hats' bus opens from 1.5 kHz over bars 8 .. 32 (Dok. 8.5: "Perc-Bus LP 800 Hz -> offen")
 *   block 1        add: the ghost kicks -- or, where the sub owns the low end, the bass
 *   block 2 ..     one operation each, from the style's queue (compose.style), then hold:
 *                    Hypnotic  open hat, ping, drone, the rolling hat's other half, shaker/tom/rim/ride shuffled,
 *                              texture; a clap (p 0.3), a chord (p 0.2)
 *                    Ostgut    open hat, clap, the rolling hat's other half, ride, a chord (p 0.5) or a 303
 *                              (p 0.25), shaker/tom/rim shuffled
 *                    Dub       chord, open hat, texture, shaker/rim shuffled, a drone (p 0.3); the rolling hat stays at half
 *                    Raw/Peak  open hat, the rolling hat's other half, clap, tom, a 303 (p 0.4), rim/shaker/ride shuffled
 *   the reduction  in the block at about 55 % of the body: kick, bass and 303 out for 8 bars (bars 16 .. 23), a noise
 *                  swell over them, bar 23 a one-bar dropout, the return on bar 24 (Dok. 8.5); chord, drone and
 *                  ping carry on through it, and the granular cloud of their last seconds rises over the eight bars
 *                  and fades over the eight after the return
 *   the outro      subtractive, the last one or two blocks: the layers leave in the reverse order, the tonal ones
 *                  first, one every eight bars; the last block kick and hats, the last 16 bars kick and hat
 * @endcode
 * Micro-events and meso curves (PLAN 7.3, 7.4): every 16 bars the rolling hat's decay moves a few per cent; the open
 * hat's decay rises from 100 to 400 ms over the 16 bars after it enters; the rumble's hall grows by a fifth over the
 * track; a chord enters 9 dB under its knob and steps up 3 dB every four bars; its band wanders up to an octave either
 * way over 16 or 32 bars; the 303's cutoff makes its trips; at the end of a 16-bar phrase (p 0.3, Dub 0.6) a ping, a
 * stab or the hats are thrown into the echo, whose feedback climbs to 0.85 and falls back over two bars.
 *
 * The loudness mark: the track's loudest part is the block after the reduction; its target is the style's
 * (styleTargetLufs, Leveler.h). No tonal material in the first and last 32 bars (the ping, the texture included).
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
