/**
 * @file Export.h
 * @brief What an export writes beside the mix (PLAN 9): the cues of a track or a set (for a WAV's cue chunk and as JSON)
 *        and the seamless DJ loops. Shared by tot_render and the plugin.
 */
#pragma once
#include "tot/Params.h"
#include "tot/Score.h"
#include "tot/compose/Composer.h"
#include "tot/compose/Set.h"
#include <string>
#include <vector>

namespace tot {

/** @brief A cue: seconds and a label. */
struct CueAt {
    double seconds;
    std::string label;
};

/** @brief The cues of a track (PLAN 9): the bass's entry, the kick-outs and returns, the outro, at set beat @p at. */
void trackCues(const TrackInfo& t, double at, const TempoMap& tempo, const std::string& prefix, std::vector<CueAt>& out);

/** @brief The cues of a set: every track (its style, form and Camelot key), every swap, every kick-out, every loop; in time order. */
std::vector<CueAt> setCues(const SetInfo& si, const TempoMap& tempo);

/** @brief The cues as JSON (seconds, sample at @p rate, label), beside the WAV for DJ software that does not read its chunk. */
bool writeCuesJson(const std::string& path, const std::vector<CueAt>& cues, double rate);

/**
 * @brief DJ loops (PLAN 9): 4 and 8 bars of a track's loudest block, seamless -- the bars rendered three times over, the
 *        last pass kept, so its start carries the tails of the pass before it as a loop played round does -- as the mix
 *        and as kick, hats and perc alone (the stems), into @p dir as loop4.wav, loop4_kick.wav, ... loop8_perc.wav.
 */
bool renderLoops(const ParamStore& knobs, const Score& track, const TrackInfo& info, const std::string& dir, double rate);

} // namespace tot
