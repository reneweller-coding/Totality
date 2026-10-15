/**
 * @file Export.h
 * @brief What an export writes beside the mix (PLAN 9): the cues of a track or a set (for a WAV's cue chunk and as JSON)
 *        and the seamless DJ loops. Shared by tot_render and the plugin.
 * @author Rene Weller
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
    double m_seconds;   ///< when
    std::string m_label;   ///< its text
};

/**
 * @brief The cues of a track (PLAN 9): the bass's entry, the kick-outs and returns, the outro, at set beat @p at.
 * @param[in]     t       the track's info
 * @param[in]     at      where the track begins, in beats
 * @param[in]     tempo   the tempo map
 * @param[in]     prefix  the cue names' prefix
 * @param[in,out] out     the cues, appended to
 */
void trackCues(const TrackInfo& t, double at, const TempoMap& tempo, const std::string& prefix, std::vector<CueAt>& out);

/**
 * @brief The cues of a set: every track (its style, form and Camelot key), every swap, every kick-out, every loop; in
 *        time order.
 * @param[in] si     the set's info
 * @param[in] tempo  the tempo map
 * @return the cues
 */
std::vector<CueAt> setCues(const SetInfo& si, const TempoMap& tempo);

/**
 * @brief The cues as JSON (seconds, sample at @p rate, label), beside the WAV for DJ software that does not read its
 *        chunk.
 * @param[in] path  the file
 * @param[in] cues  the cues
 * @param[in] rate  the sample rate in Hz
 * @return false when the file cannot be written
 */
bool writeCuesJson(const std::string& path, const std::vector<CueAt>& cues, double rate);

/**
 * @brief Phase 16: the WAV as a rekordbox collection (rekordbox's "rekordbox xml" import, Preferences > Advanced): one
 *        TRACK with its beat grid -- a TEMPO from every bar whose tempo changed, so a set's ramps are followed bar by
 *        bar -- every cue as a memory cue and the first eight also as hot cues, in a playlist "Totality". Written to the
 *        published format (DJ_PLAYLISTS 1.0.0); not tried in rekordbox itself.
 * @param[in] path         the XML file
 * @param[in] wavPath      the audio file
 * @param[in] title        the title
 * @param[in] tonality     the key as rekordbox shows it ("Am", "F#m"), or empty
 * @param[in] cues         the cues
 * @param[in] tempo        the tempo map
 * @param[in] lengthBeats  the length in beats
 * @param[in] rate         the sample rate in Hz
 * @return false when the file cannot be written
 */
bool writeRekordboxXml(const std::string& path, const std::string& wavPath, const std::string& title, const std::string& tonality,
                       const std::vector<CueAt>& cues, const TempoMap& tempo, double lengthBeats, double rate);

/**
 * @brief DJ loops (PLAN 9): 4 and 8 bars of a track's loudest block, seamless -- the bars rendered three times over, the
 *        last pass kept, so its start carries the tails of the pass before it as a loop played round does -- as the mix
 *        and as kick, hats and perc alone (the stems), into @p dir as loop4.wav, loop4_kick.wav, ... loop8_perc.wav.
 * @param[in] knobs  the knobs
 * @param[in] track  the track
 * @param[in] info   its info
 * @param[in] dir    the folder the loops go to
 * @param[in] rate   the sample rate in Hz
 * @return false when a file cannot be written
 */
bool renderLoops(const ParamStore& knobs, const Score& track, const TrackInfo& info, const std::string& dir, double rate);

} // namespace tot
