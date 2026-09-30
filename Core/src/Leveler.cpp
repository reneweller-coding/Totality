/**
 * @file Leveler.cpp
 * @brief Every track as loud as its style means (Leveler.h).
 * @note Copied from Ephemeris `Core/src/Leveler.cpp` (namespace eph, prefix EPH_) at d047d79 (27.09.2026).
 */
#include "tot/Leveler.h"
#include "tot/Dsp.h"
#include "tot/Engine.h"
#include "tot/Loudness.h"
#include "tot/compose/Style.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>

namespace tot {

namespace {
constexpr double kRate = 48000.0;
constexpr int kBlock = 512;
constexpr double kWarm = 4.0;       ///< seconds before the part: the rooms fill, the notes sounding on are found again
constexpr float kMostDb = 4.0f;     ///< the largest correction either way
constexpr double kBalSeconds = 12.0;   ///< Phase 18: how much the balance reads in each of its places (6 bars at 128)
constexpr double kBalWarm = 2.0;       ///< and before it (the notes sounding on found again)
constexpr float kBalDown = -8.0f, kBalUp = 15.0f;   ///< the largest corrections of a part
constexpr float kRoomDown = -12.0f;                 ///< the guard takes the room's return down at most this far

/** @brief Phase 18: a band of the guard -- two second-order Butterworth high passes and two low passes, as the
 *         references were measured (Tools, 28.09.2026) -- and the energy through it. */
struct BandMeter {
    Svf hp[2], lp[2];
    double sum = 0.0;
    void prepare(float lo, float hi)
    {
        for (Svf& f : hp) { f.setQ(lo, 0.70710678f, static_cast<float>(kRate)); f.ic1 = f.ic2 = 0.0f; }
        for (Svf& f : lp) { f.setQ(hi, 0.70710678f, static_cast<float>(kRate)); f.ic1 = f.ic2 = 0.0f; }
        sum = 0.0;
    }
    void run(float x, bool count)
    {
        float l, b, h;
        for (Svf& f : hp) { f.tick(x, l, b, h); x = h; }
        for (Svf& f : lp) { f.tick(x, l, b, h); x = l; }
        if (count) sum += static_cast<double>(x) * x;
    }
};

/** @brief What the balance reads in its places: the loudest samples (the larger of what they held and what is found). */
struct PartPeaks {
    float kick = 0.0f;
    float part[kBalParts] = {};
};

/**
 * @brief Phase 18: plays @p seconds from @p beat (after kBalWarm seconds before it; the score on deck A) and keeps the
 *        loudest samples of the kick and of every part in @p peaks; with @p bands, the energies of the mix in the guard's
 *        bands (40-140 Hz, 250-1000, 1000-5000). False if @p stop said so.
 */
bool readParts(Engine& e, const Score& s, double beat, double seconds, PartPeaks& peaks, double* bands, const std::function<bool()>& stop)
{
    const double at = s.tempo.secondsAt(beat);
    e.seek(s.tempo.beatAt(std::max(0.0, at - kBalWarm)));
    std::vector<float> l(kBlock), r(kBlock);
    BandMeter meters[3];
    meters[0].prepare(40.0f, 140.0f);
    meters[1].prepare(250.0f, 1000.0f);
    meters[2].prepare(1000.0f, 5000.0f);
    const auto feed = [&](bool count) {
        if (bands == nullptr) return;
        for (int i = 0; i < kBlock; ++i) for (BandMeter& m : meters) m.run(0.5f * (l[static_cast<size_t>(i)] + r[static_cast<size_t>(i)]), count);
    };
    const double warm = std::min(kBalWarm, at);
    for (int done = 0; done < static_cast<int>(warm * kRate); done += kBlock) {
        if (stop && stop()) return false;
        e.process(l.data(), r.data(), kBlock);
        feed(false);
    }
    e.watchPeaks(true);
    for (int done = 0; done < static_cast<int>(seconds * kRate); done += kBlock) {
        if (stop && stop()) { e.watchPeaks(false); return false; }
        e.process(l.data(), r.data(), kBlock);
        feed(true);
    }
    e.watchPeaks(false);
    const Deck& d = e.deck(0);
    peaks.kick = std::max(peaks.kick, d.kickPeak());
    for (int p = 0; p < kBalParts; ++p) peaks.part[p] = std::max(peaks.part[p], d.partPeak(p));
    if (bands != nullptr) for (int k = 0; k < 3; ++k) bands[k] = meters[k].sum;
    return true;
}

/** @brief Every part's loudest sample against the loudest kick of all places, dB (NaN: more than 60 dB under it). */
BalanceDb against(const PartPeaks& peaks)
{
    BalanceDb out;
    for (int p = 0; p < kBalParts; ++p) {
        const float v = peaks.part[p];
        out[static_cast<size_t>(p)] = peaks.kick > 1e-4f && v > peaks.kick * 1e-3f ? 20.0f * std::log10(v / peaks.kick)
                                                                                   : std::numeric_limits<float>::quiet_NaN();
    }
    return out;
}

/**
 * @brief The corrections that bring @p found into the windows (0 inside, or for a silent part); the lanes' roles as deck A
 *        of @p e plays them. A rank within each kind (28.09.2026: ten parts pushed up at once flooded the mids): of the
 *        hat-like lanes and of the percussion lanes the two loudest as found keep their windows, the third's lies 3 dB
 *        lower, the others' 6; of the lead voices (ping, bass, 303, chord) the loudest keeps its window, the others' lie
 *        4 dB lower -- one or two voices up front, the rest behind them, as a record has it.
 */
BalanceDb corrections(const Engine& e, const LevelMark& m, const BalanceDb& found, BalanceDb* los = nullptr, BalanceDb* his = nullptr)
{
    const ParamStore& ps = e.params();
    int role[kBalLanes] = {};
    for (int l = 0; l < kBalLanes; ++l) role[l] = static_cast<int>(std::lround(e.deck(0).played(ps.id(Module::Perc, l, perc::Role))));
    const auto kindOf = [&](int p) {
        if (p >= kBalLanes) {
            const BalPart b = static_cast<BalPart>(p);
            return b == BalPart::Ping || b == BalPart::Bass || b == BalPart::Acid || b == BalPart::Chord ? 2 : b == BalPart::Room ? 4 : 3;
        }
        const PercRole r = static_cast<PercRole>(std::clamp(role[p], 0, kNumPercRoles - 1));
        return r == PercRole::ClosedHat || r == PercRole::RollingHat || r == PercRole::OpenHat || r == PercRole::Ride || r == PercRole::Shaker ? 0 : 1;
    };
    float lower[kBalParts] = {};
    for (int kind = 0; kind < 3; ++kind) {
        std::vector<std::pair<float, int>> heard;
        for (int p = 0; p < kBalParts; ++p)
            if (kindOf(p) == kind && !std::isnan(found[static_cast<size_t>(p)])) heard.emplace_back(found[static_cast<size_t>(p)], p);
        std::stable_sort(heard.begin(), heard.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        for (size_t k = 0; k < heard.size(); ++k)
            lower[heard[k].second] = kind == 2 ? (k == 0 ? 0.0f : 4.0f) : (k < 2 ? 0.0f : k == 2 ? 3.0f : 6.0f);
    }
    // (Phase 20: the lane that opens the track keeps its window -- ranked fourth, a Tribal track's conga lay 28 dB under the
    // kick through the intro it carries.)
    if (m.front >= 0 && m.front < kBalLanes) lower[m.front] = 0.0f;
    BalanceDb out{};
    for (int p = 0; p < kBalParts; ++p) {
        const float f = found[static_cast<size_t>(p)];
        if (std::isnan(f) || p == static_cast<int>(BalPart::Room)) continue;   // (the room: the guard's alone)
        float lo, hi;
        balanceWindow(m.styleMix, p, p < kBalLanes ? role[p] : -1, lo, hi);
        lo -= lower[p];
        hi -= lower[p];
        if (los != nullptr) (*los)[static_cast<size_t>(p)] = lo;
        if (his != nullptr) (*his)[static_cast<size_t>(p)] = hi;
        out[static_cast<size_t>(p)] = std::clamp(f < lo ? lo - f : f > hi ? hi - f : 0.0f, kBalDown, kBalUp);
    }
    return out;
}

/**
 * @brief The guard's ceilings for a track of @p styleMix: the mix's 250-1000 Hz and 1-5 kHz bands against its 40-140 Hz,
 *        dB -- the references' 90th percentile per style (the edge of the band every calibration of the project uses),
 *        measured with the guard's filters in the loudest of three 12-second windows (Tools, 28.09.2026: Hypnotic -2.6
 *        and -4.8, Ostgut -9.9 and -6.9, Dub -9.4 and -18.2, Raw -9.2 and -11.5). The 75th took the percussion's boosts
 *        back in a third of the tracks.
 */
void guardCeilings(const std::array<float, 4>& styleMix, float& mid, float& highMid)
{
    static const float kMid[4] = { -2.6f, -9.9f, -9.4f, -9.2f }, kHighMid[4] = { -4.8f, -6.9f, -18.2f, -11.5f };
    float w[4] = { styleMix[0], styleMix[1], styleMix[2], styleMix[3] };
    float sum = w[0] + w[1] + w[2] + w[3];
    if (sum <= 0.0f) { w[0] = 1.0f; sum = 1.0f; }
    mid = highMid = 0.0f;
    for (int s = 0; s < 4; ++s) { mid += w[s] / sum * kMid[s]; highMid += w[s] / sum * kHighMid[s]; }
}

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
    // The loops: the corrections of their source.
    for (LevelMark& m : set.decks[2].levels) {
        if (!std::isnan(m.targetLufs)) continue;
        for (int d = 0; d < 2; ++d)
            for (const LevelMark& src : set.decks[d].levels)
                if (src.peakBeat == m.peakBeat) { m.trimDb = src.trimDb; m.balDb = src.balDb; }
    }
    return out;
}

void balanceWindow(const std::array<float, 4>& styleMix, int part, int role, float& lo, float& hi)
{
    // The loudest sample against the kick's, dB. The research's fader levels: closed hat -8, open hat -10, conga -15, FX
    // -10 (Analyse 2026-09-27, Attack "Dark Cinematic"). The lanes by their role, a window of 5 or 6 dB around those;
    // the claps and snares with the closed hat, the rim and the toms between it and the conga, the shaker and the ghost
    // clap under the conga. Dub's hats 6 dB lower and its other percussion 3 (measured apart from the rest by
    // harmonic-percussive separation, the Dub records' hats lie 8 dB under the others'; our Hypnotic hats already over
    // the records', so no window pushes hats up past the closed hat's -8). The tonal voices per style: the lead voices
    // at the FX's -10, Dub's chord in front, the sustained voices lower, their peak being nearly their level. [I], to be
    // heard.
    float w[4] = { styleMix[0], styleMix[1], styleMix[2], styleMix[3] };
    float sum = w[0] + w[1] + w[2] + w[3];
    if (sum <= 0.0f) { w[0] = 1.0f; sum = 1.0f; }
    const float dub = w[2] / sum;
    if (part < kBalLanes) {
        //                                 closed rolling open  ride  clap  ghost snare  rim  shaker  tom  conga noise
        static const float kLaneLo[kNumPercRoles] = { -14, -16, -16, -16, -12, -18, -12, -14, -18, -15, -17, -20 };
        static const float kLaneHi[kNumPercRoles] = {  -8, -10, -10, -10,  -7, -12,  -7,  -9, -12, -10, -12, -12 };
        const int r = std::clamp(role, 0, kNumPercRoles - 1);
        const PercRole pr = static_cast<PercRole>(r);
        const bool hatLike = pr == PercRole::ClosedHat || pr == PercRole::RollingHat || pr == PercRole::OpenHat || pr == PercRole::Ride
                          || pr == PercRole::Shaker;
        const float shift = dub * (hatLike ? -6.0f : -3.0f);
        lo = kLaneLo[r] + shift;
        hi = kLaneHi[r] + shift;
        return;
    }
    static const float kLo[kBalParts - kBalLanes][4] = {
        { -11.0f, -11.0f, -12.0f, -11.0f },   // ping
        {  -9.0f,  -9.0f,  -8.0f,  -9.0f },   // bass
        { -11.0f, -11.0f, -12.0f, -10.0f },   // acid
        { -12.0f, -11.0f, -10.0f, -12.0f },   // chord
        { -18.0f, -18.0f, -16.0f, -18.0f },   // drone
        { -22.0f, -22.0f, -20.0f, -22.0f },   // texture
    };
    static const float kHi[kBalParts - kBalLanes][4] = {
        {  -8.0f,  -8.0f,  -8.0f,  -8.0f },
        {  -5.0f,  -5.0f,  -4.0f,  -5.0f },
        {  -8.0f,  -8.0f,  -8.0f,  -7.0f },
        {  -8.0f,  -8.0f,  -6.0f,  -8.0f },
        { -12.0f, -12.0f, -10.0f, -12.0f },
        { -16.0f, -16.0f, -14.0f, -16.0f },
    };
    const int k = part - kBalLanes;
    lo = hi = 0.0f;
    for (int s = 0; s < 4; ++s) { lo += w[s] / sum * kLo[k][s]; hi += w[s] / sum * kHi[k][s]; }
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
    // Phase 18: the balance first -- every part's loudest sample against the loudest kick, read in three places, and the
    // parts outside their windows moved to the edge (ranked, corrections()). The parts scale with their gains (set at the
    // sources), so one reading does. Then the guard: the mix's mids against its kick band in the loudest part, and where
    // they stand over the references' (guardCeilings), every boost comes down by the excess, at most three times. The
    // loudness is measured after, with the balance in.
    Score s = score;
    for (LevelMark& m : s.levels) if (!std::isnan(m.targetLufs)) { m.trimDb = 0.0f; m.balDb = BalanceDb{}; }
    e->load(s);
    std::vector<BalanceDb> found(s.levels.size()), bal(s.levels.size()), los(s.levels.size()), his(s.levels.size());
    std::vector<bool> guarded(s.levels.size(), false);
    for (size_t i = 0; i < s.levels.size(); ++i) {
        const LevelMark& m = s.levels[i];
        found[i].fill(std::numeric_limits<float>::quiet_NaN());
        bal[i] = m.balDb;
        if (std::isnan(m.targetLufs)) continue;
        // Three places: the loudest part, 16 and 40 bars before it (not before the body's bar 33) -- a part is read where
        // it plays; one place alone had a figure breathing out and read its echo (28.09.2026: -53 dB, +12). All against
        // the loudest kick of the three (a kick-out in one of them read the hats 60 dB over the kick).
        PartPeaks peaks;
        for (const double back : { 0.0, 64.0, 160.0 }) {
            const double at = m.peakBeat - back;
            if (back > 0.0 && at < m.beat + 128.0) continue;
            if (!readParts(*e, s, at, kBalSeconds, peaks, nullptr, stop)) return {};
        }
        found[i] = against(peaks);
        los[i].fill(std::numeric_limits<float>::quiet_NaN());
        his[i].fill(std::numeric_limits<float>::quiet_NaN());
        bal[i] = corrections(*e, m, found[i], &los[i], &his[i]);
    }
    for (size_t i = 0; i < s.levels.size(); ++i) s.levels[i].balDb = bal[i];
    for (size_t i = 0; i < s.levels.size(); ++i) {
        const LevelMark& m = s.levels[i];
        if (std::isnan(m.targetLufs)) continue;
        float ceilMid, ceilHigh;
        guardCeilings(m.styleMix, ceilMid, ceilHigh);
        for (int round = 0; round < 4; ++round) {
            e->load(s);
            // In the balance's three places, the largest excess (the loudest part had the figure breathing out, and
            // the mids were full where it played: 28.09.2026, -4.3 dB against a ceiling of -9.8) -- of the places
            // where the kick band stands (within 10 dB of the fullest): in a set, 40 bars before the loudest part can
            // lie before the bass swap, its kick band empty and its mids read 30 dB over it.
            float excess = -100.0f;
            double read[3][3] = {};
            int places = 0;
            double fullest = 0.0;
            for (const double back : { 0.0, 64.0, 160.0 }) {
                const double at = m.peakBeat - back;
                if (back > 0.0 && at < m.beat + 128.0) continue;
                PartPeaks unused;
                if (!readParts(*e, s, at, kBalSeconds, unused, read[places], stop)) return {};
                fullest = std::max(fullest, read[places][0]);
                ++places;
            }
            for (int k = 0; k < places; ++k) {
                const double* bands = read[k];
                if (bands[0] <= 1e-12 || bands[0] < 0.1 * fullest) continue;
                const float mid = static_cast<float>(10.0 * std::log10(std::max(bands[1], 1e-30) / bands[0]));
                const float high = static_cast<float>(10.0 * std::log10(std::max(bands[2], 1e-30) / bands[0]));
                excess = std::max(excess, std::max(mid - ceilMid, high - ceilHigh));
            }
            if (excess <= 0.25f) break;
            // The room's return first, to 12 dB down (the spectral fit of 27.09.2026 had filled the records' mids with
            // it: Raw's room +10 dB read -5.8 dB in the mids, over every instrument), then the boosts.
            bool any = false;
            float& room = bal[i][static_cast<size_t>(BalPart::Room)];
            if (room > kRoomDown) {
                room = std::max(kRoomDown, room - excess - 0.5f);
                any = true;
            } else {
                for (float& b : bal[i]) if (b > 0.0f) { b = std::max(0.0f, b - excess - 0.5f); any = true; }
            }
            s.levels[i].balDb = bal[i];
            guarded[i] = guarded[i] || any;
            if (!any) break;
        }
    }
    // Then the loudness: as composed (with the balance), then with the corrections found (the master compresses, clips
    // and limits after the gain).
    e->load(s);
    for (const LevelMark& m : s.levels) {
        if (std::isnan(m.targetLufs)) { out.push_back({ m.beat, 0.0f, m.targetLufs, m.trimDb, 0.0f }); continue; }   // not measured
        const std::optional<float> got = measure(*e, s, m.peakBeat, seconds, stop);
        if (!got) return {};
        const float measured = *got;
        const float trim = measured > -70.0f ? std::clamp(m.targetLufs - measured, -kMostDb, kMostDb) : 0.0f;
        out.push_back({ m.beat, measured, m.targetLufs, trim, 0.0f });
    }
    for (size_t i = 0; i < out.size() && i < found.size(); ++i) {
        out[i].found = found[i];
        out[i].bal = bal[i];
        out[i].lo = los[i];
        out[i].hi = his[i];
        out[i].guarded = guarded[i];
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
    for (size_t i = 0; i < score.levels.size() && i < out.size(); ++i) {
        score.levels[i].trimDb = out[i].trim;
        score.levels[i].balDb = out[i].bal;
    }
    return out;
}

} // namespace tot
