/**
 * @file Leveler.cpp
 * @brief Every track as loud as its style means (Leveler.h).
 * @note Copied from Ephemeris `Core/src/Leveler.cpp` (namespace eph, prefix EPH_) at d047d79 (27.09.2026).
 */
#include "tot/Leveler.h"
#include "tot/Engine.h"
#include "tot/Loudness.h"
#include "tot/compose/Style.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>

namespace tot {

namespace {
constexpr double kRate = 48000.0;
constexpr int kBlock = 512;
constexpr double kWarm = 4.0;       ///< seconds before the part: the rooms fill, the notes sounding on are found again
constexpr float kMostDb = 4.0f;     ///< the largest correction either way

/** @brief The loudness of @p seconds from @p beat of the score @p e plays (after kWarm seconds before it); nothing if
 *         @p stop said so on the way. */
std::optional<float> measure(Engine& e, const Score& s, double beat, double seconds, const std::function<bool()>& stop)
{
    const double at = s.tempo.secondsAt(beat);
    e.seek(s.tempo.beatAt(std::max(0.0, at - kWarm)));
    std::vector<float> l(kBlock), r(kBlock);
    const double warm = std::min(kWarm, at);
    for (int done = 0; done < static_cast<int>(warm * kRate); done += kBlock) {
        if (stop && stop()) return std::nullopt;
        e.process(l.data(), r.data(), kBlock);
    }
    LoudnessMeter meter;
    meter.prepare(kRate);
    for (int done = 0; done < static_cast<int>(seconds * kRate); done += kBlock) {
        if (stop && stop()) return std::nullopt;
        e.process(l.data(), r.data(), kBlock);
        meter.process(l.data(), r.data(), kBlock);
    }
    return static_cast<float>(meter.report().integrated);
}
} // namespace

std::vector<LevelReading> levelSet(SetScore& set, const ParamStore& params, double seconds, const std::function<bool()>& stop)
{
    std::vector<LevelReading> out;
    for (int d = 0; d < 2; ++d) {
        if (set.decks[d].levels.empty()) continue;
        const std::vector<LevelReading> r = levelScore(set.decks[d], params, seconds, stop);
        if (r.empty() && stop && stop()) return {};
        out.insert(out.end(), r.begin(), r.end());
    }
    // The loops: the correction of their source.
    for (LevelMark& m : set.decks[2].levels) {
        if (!std::isnan(m.targetLufs)) continue;
        for (int d = 0; d < 2; ++d)
            for (const LevelMark& src : set.decks[d].levels)
                if (src.peakBeat == m.peakBeat) m.trimDb = src.trimDb;
    }
    return out;
}

float styleTargetLufs(int style)
{
    // The references' loudest 20 seconds per profile, the median (Tools/ref_stats.json, loud20: Hypnotic -10.0, Ostgut
    // -9.5, Dub -11.5, Raw -9.4), rounded to half dB; kept in the profiles (Style.h).
    return styleProfile(static_cast<Style>(style)).peakLufs;
}

std::vector<LevelReading> levelScore(Score& score, const ParamStore& params, double seconds, const std::function<bool()>& stop)
{
    std::vector<LevelReading> out;
    if (score.levels.empty()) return out;
    auto e = std::make_unique<Engine>();
    ParamStore& p = e->params();
    p.copyValuesFrom(params);
    // The player's master level stays out of it: the correction is the track's, the master level comes on top.
    const int master = p.id(Module::Master, 0, master::Level);
    p.set(master, p.defaultValue(master));
    e->prepare(kRate, kBlock);
    // First as composed, then with the corrections found (the master compresses, clips and limits after the gain).
    Score s = score;
    for (LevelMark& m : s.levels) if (!std::isnan(m.targetLufs)) m.trimDb = 0.0f;
    e->load(s);
    for (const LevelMark& m : s.levels) {
        if (std::isnan(m.targetLufs)) { out.push_back({ m.beat, 0.0f, m.targetLufs, m.trimDb, 0.0f }); continue; }   // not measured
        const std::optional<float> got = measure(*e, s, m.peakBeat, seconds, stop);
        if (!got) return {};
        const float measured = *got;
        const float trim = measured > -70.0f ? std::clamp(m.targetLufs - measured, -kMostDb, kMostDb) : 0.0f;
        out.push_back({ m.beat, measured, m.targetLufs, trim, 0.0f });
    }
    for (size_t i = 0; i < s.levels.size(); ++i) s.levels[i].trimDb = out[i].trim;
    e->load(s);
    for (size_t i = 0; i < s.levels.size(); ++i) {
        if (std::isnan(s.levels[i].targetLufs)) continue;
        const std::optional<float> got = measure(*e, s, s.levels[i].peakBeat, seconds, stop);
        if (!got) return {};
        const float again = *got;
        if (again > -70.0f) out[i].trim = std::clamp(out[i].trim + (out[i].target - again), -kMostDb, kMostDb);
        out[i].after = again;
    }
    for (size_t i = 0; i < score.levels.size() && i < out.size(); ++i) score.levels[i].trimDb = out[i].trim;
    return out;
}

} // namespace tot
