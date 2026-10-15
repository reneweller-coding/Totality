/**
 * @file Leveler.cpp
 * @brief Every track as loud as its style means (Leveler.h).
 * @note Copied from Ephemeris `Core/src/Leveler.cpp` (namespace eph, prefix EPH_) at d047d79 (27.09.2026).
 * @author Rene Weller
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
constexpr double Rate = 48000.0;   ///< the rate the parts are rendered at to be measured, Hz
constexpr int Block = 512;   ///< the block they are rendered in
constexpr double Warm = 4.0;       ///< seconds before the part: the rooms fill, the notes sounding on are found again
constexpr float MostDb = 4.0f;     ///< the largest correction either way
constexpr double BalSeconds = 12.0;   ///< Phase 18: how much the balance reads in each of its places (6 bars at 128)
constexpr double BalWarm = 2.0;       ///< and before it (the notes sounding on found again)
constexpr float BalDown = -8.0f;   ///< the largest correction of a part down, dB
constexpr float BalUp = 15.0f;   ///< the largest correction of a part up, dB
constexpr float RoomDown = -12.0f;                 ///< the guard takes the room's return down at most this far

/** @brief Phase 18: a band of the guard -- two second-order Butterworth high passes and two low passes, as the
 *         references were measured (Tools, 28.09.2026) -- and the energy through it. */
struct BandMeter {
    Svf m_hp[2];   ///< the two high passes
    Svf m_lp[2];   ///< the two low passes
    double m_sum = 0.0;   ///< the energy through the band
    /**
     * @brief Sets the band from @p lo to @p hi Hz and clears it.
     * @param[in] lo  the band's lower edge in Hz
     * @param[in] hi  its upper edge in Hz
     */
    void prepare(float lo, float hi)
    {
        for (Svf& f : m_hp) { f.setQ(lo, 0.70710678f, static_cast<float>(Rate)); f.m_ic1 = f.m_ic2 = 0.0f; }
        for (Svf& f : m_lp) { f.setQ(hi, 0.70710678f, static_cast<float>(Rate)); f.m_ic1 = f.m_ic2 = 0.0f; }
        m_sum = 0.0;
    }
    /**
     * @brief One sample @p x through the band; with @p count its energy is summed.
     * @param[in] x      the sample
     * @param[in] count  true to sum its energy
     */
    void run(float x, bool count)
    {
        float l, b, h;
        for (Svf& f : m_hp) { f.tick(x, l, b, h); x = h; }
        for (Svf& f : m_lp) { f.tick(x, l, b, h); x = l; }
        if (count) m_sum += static_cast<double>(x) * x;
    }
};

/** @brief What the balance reads in its places: the loudest samples (the larger of what they held and what is found). */
struct PartPeaks {
    float m_kick = 0.0f;   ///< the kick's loudest sample
    float m_part[BalParts] = {};   ///< every part's loudest sample
};

/**
 * @brief Phase 18: plays @p seconds from @p beat (after BalWarm seconds before it; the score on deck A) and keeps the
 *        loudest samples of the kick and of every part in @p peaks; with @p bands, the energies of the mix in the guard's
 *        bands (40-140 Hz, 250-1000, 1000-5000). False if @p stop said so.
 * @param[in,out] e        the engine that plays the score
 * @param[in]     s        the score
 * @param[in]     beat     where the stretch begins
 * @param[in]     seconds  how long it lasts
 * @param[out]    peaks    receives the parts' loudest samples
 * @param[out]    bands    receives the mix's band energies
 * @param[in]     stop     asked now and then; true stops the reading
 * @return false when stopped
 */
bool readParts(Engine& e, const Score& s, double beat, double seconds, PartPeaks& peaks, double* bands, const std::function<bool()>& stop)
{
    const double at = s.m_tempo.secondsAt(beat);
    e.seek(s.m_tempo.beatAt(std::max(0.0, at - BalWarm)));
    std::vector<float> l(Block), r(Block);
    BandMeter meters[3];
    meters[0].prepare(40.0f, 140.0f);
    meters[1].prepare(250.0f, 1000.0f);
    meters[2].prepare(1000.0f, 5000.0f);
    const auto feed = [&](bool count) {
        if (bands == nullptr) return;
        for (int i = 0; i < Block; ++i) for (BandMeter& m : meters) m.run(0.5f * (l[static_cast<size_t>(i)] + r[static_cast<size_t>(i)]), count);
    };
    const double warm = std::min(BalWarm, at);
    for (int done = 0; done < static_cast<int>(warm * Rate); done += Block) {
        if (stop && stop()) return false;
        e.process(l.data(), r.data(), Block);
        feed(false);
    }
    e.watchPeaks(true);
    for (int done = 0; done < static_cast<int>(seconds * Rate); done += Block) {
        if (stop && stop()) { e.watchPeaks(false); return false; }
        e.process(l.data(), r.data(), Block);
        feed(true);
    }
    e.watchPeaks(false);
    const Deck& d = e.deck(0);
    peaks.m_kick = std::max(peaks.m_kick, d.kickPeak());
    for (int p = 0; p < BalParts; ++p) peaks.m_part[p] = std::max(peaks.m_part[p], d.partPeak(p));
    if (bands != nullptr) for (int k = 0; k < 3; ++k) bands[k] = meters[k].m_sum;
    return true;
}

/**
 * @brief Every part's loudest sample against the loudest kick of all places, dB (NaN: more than 60 dB under it).
 * @param[in] peaks  the parts' loudest samples
 * @return each part against the loudest kick in dB, NaN more than 60 dB under it
 */
BalanceDbT against(const PartPeaks& peaks)
{
    BalanceDbT out;
    for (int p = 0; p < BalParts; ++p) {
        const float v = peaks.m_part[p];
        out[static_cast<size_t>(p)] = peaks.m_kick > 1e-4f && v > peaks.m_kick * 1e-3f ? 20.0f * std::log10(v / peaks.m_kick)
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
 * @param[in]  e      the engine
 * @param[in]  m      the level mark
 * @param[in]  found  the parts as measured, dB against the kick
 * @param[out] los    receives the windows' lower edges, or null
 * @param[out] his    receives the windows' upper edges, or null
 * @return the corrections in dB
 */
BalanceDbT corrections(const Engine& e, const LevelMark& m, const BalanceDbT& found, BalanceDbT* los = nullptr, BalanceDbT* his = nullptr)
{
    const ParamStore& ps = e.params();
    int role[BalLanes] = {};
    for (int l = 0; l < BalLanes; ++l) role[l] = static_cast<int>(std::lround(e.deck(0).played(ps.id(ModuleE::PERC, l, perc::ROLE))));
    const auto kindOf = [&](int p) {
        if (p >= BalLanes) {
            const BalPartE b = static_cast<BalPartE>(p);
            return b == BalPartE::PING || b == BalPartE::BASS || b == BalPartE::ACID || b == BalPartE::CHORD ? 2 : b == BalPartE::ROOM ? 4 : 3;
        }
        const PercRoleE r = static_cast<PercRoleE>(std::clamp(role[p], 0, NumPercRoles - 1));
        return r == PercRoleE::CLOSED_HAT || r == PercRoleE::ROLLING_HAT || r == PercRoleE::OPEN_HAT || r == PercRoleE::RIDE || r == PercRoleE::SHAKER ? 0 : 1;
    };
    float lower[BalParts] = {};
    for (int kind = 0; kind < 3; ++kind) {
        std::vector<std::pair<float, int>> heard;
        for (int p = 0; p < BalParts; ++p)
            if (kindOf(p) == kind && !std::isnan(found[static_cast<size_t>(p)])) heard.emplace_back(found[static_cast<size_t>(p)], p);
        std::stable_sort(heard.begin(), heard.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        for (size_t k = 0; k < heard.size(); ++k)
            lower[heard[k].second] = kind == 2 ? (k == 0 ? 0.0f : 4.0f) : (k < 2 ? 0.0f : k == 2 ? 3.0f : 6.0f);
    }
    // (Phase 20: the lane that opens the track keeps its window -- ranked fourth, a Tribal track's conga lay 28 dB under the
    // kick through the intro it carries.)
    if (m.m_front >= 0 && m.m_front < BalLanes) lower[m.m_front] = 0.0f;
    BalanceDbT out{};
    for (int p = 0; p < BalParts; ++p) {
        const float f = found[static_cast<size_t>(p)];
        if (std::isnan(f) || p == static_cast<int>(BalPartE::ROOM)) continue;   // (the room: the guard's alone)
        float lo, hi;
        balanceWindow(m.m_styleMix, p, p < BalLanes ? role[p] : -1, lo, hi);
        lo -= lower[p];
        hi -= lower[p];
        // (Phase 21: the opening's lane at the top of its window -- at its bottom, a set's first track opened with its tom
        // 24 dB under the kick, and the mix began as the kick alone.)
        if (p == m.m_front) lo = std::max(lo, hi - 2.0f);
        if (los != nullptr) (*los)[static_cast<size_t>(p)] = lo;
        if (his != nullptr) (*his)[static_cast<size_t>(p)] = hi;
        out[static_cast<size_t>(p)] = std::clamp(f < lo ? lo - f : f > hi ? hi - f : 0.0f, BalDown, BalUp);
    }
    return out;
}

/**
 * @brief The guard's ceilings for a track of @p styleMix: the mix's 250-1000 Hz and 1-5 kHz bands against its 40-140 Hz,
 *        dB -- the references' 90th percentile per style (the edge of the band every calibration of the project uses),
 *        measured with the guard's filters in the loudest of three 12-second windows (Tools, 28.09.2026: Hypnotic -2.6
 *        and -4.8, Ostgut -9.9 and -6.9, Dub -9.4 and -18.2, Raw -9.2 and -11.5). The 75th took the percussion's boosts
 *        back in a third of the tracks.
 * @param[in]  styleMix  the track's style mix
 * @param[out] mid       receives the ceiling of the 250-1000 Hz band
 * @param[out] highMid   receives the ceiling of the 1-5 kHz band
 */
void guardCeilings(const std::array<float, 4>& styleMix, float& mid, float& highMid)
{
    static const float Mid[4] = { -2.6f, -9.9f, -9.4f, -9.2f }, HighMid[4] = { -4.8f, -6.9f, -18.2f, -11.5f };
    float w[4] = { styleMix[0], styleMix[1], styleMix[2], styleMix[3] };
    float sum = w[0] + w[1] + w[2] + w[3];
    if (sum <= 0.0f) { w[0] = 1.0f; sum = 1.0f; }
    mid = highMid = 0.0f;
    for (int s = 0; s < 4; ++s) { mid += w[s] / sum * Mid[s]; highMid += w[s] / sum * HighMid[s]; }
}

/**
 * @brief The loudness of @p seconds from @p beat of the score @p e plays (after Warm seconds before it); nothing if
 *         @p stop said so on the way.
 * @param[in,out] e        the engine that plays the score
 * @param[in]     s        the score
 * @param[in]     beat     where the stretch begins
 * @param[in]     seconds  how long it lasts
 * @param[in]     stop     asked now and then; true stops the measurement
 * @return the loudness in LUFS, empty when stopped
 */
std::optional<float> measure(Engine& e, const Score& s, double beat, double seconds, const std::function<bool()>& stop)
{
    const double at = s.m_tempo.secondsAt(beat);
    e.seek(s.m_tempo.beatAt(std::max(0.0, at - Warm)));
    std::vector<float> l(Block), r(Block);
    const double warm = std::min(Warm, at);
    for (int done = 0; done < static_cast<int>(warm * Rate); done += Block) {
        if (stop && stop()) return std::nullopt;
        e.process(l.data(), r.data(), Block);
    }
    LoudnessMeter meter;
    meter.prepare(Rate);
    for (int done = 0; done < static_cast<int>(seconds * Rate); done += Block) {
        if (stop && stop()) return std::nullopt;
        e.process(l.data(), r.data(), Block);
        meter.process(l.data(), r.data(), Block);
    }
    return static_cast<float>(meter.report().m_integrated);
}
} // namespace

std::vector<LevelReading> levelSet(SetScore& set, const ParamStore& params, double seconds, const std::function<bool()>& stop)
{
    std::vector<LevelReading> out;
    for (int d = 0; d < 2; ++d) {
        if (set.m_decks[d].m_levels.empty()) continue;
        const std::vector<LevelReading> r = levelScore(set.m_decks[d], params, seconds, stop);
        if (r.empty() && stop && stop()) return {};
        out.insert(out.end(), r.begin(), r.end());
    }
    // The loops: the corrections of their source.
    for (LevelMark& m : set.m_decks[2].m_levels) {
        if (!std::isnan(m.m_targetLufs)) continue;
        for (int d = 0; d < 2; ++d)
            for (const LevelMark& src : set.m_decks[d].m_levels)
                if (src.m_peakBeat == m.m_peakBeat) { m.m_trimDb = src.m_trimDb; m.m_balDb = src.m_balDb; }
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
    if (part < BalLanes) {
        //                                 closed rolling open  ride  clap  ghost snare  rim  shaker  tom  conga noise
        static const float LaneLo[NumPercRoles] = { -14, -16, -16, -16, -12, -18, -12, -14, -18, -15, -17, -20 };
        static const float LaneHi[NumPercRoles] = {  -8, -10, -10, -10,  -7, -12,  -7,  -9, -12, -10, -12, -12 };
        const int r = std::clamp(role, 0, NumPercRoles - 1);
        const PercRoleE pr = static_cast<PercRoleE>(r);
        const bool hatLike = pr == PercRoleE::CLOSED_HAT || pr == PercRoleE::ROLLING_HAT || pr == PercRoleE::OPEN_HAT || pr == PercRoleE::RIDE
                          || pr == PercRoleE::SHAKER;
        const float shift = dub * (hatLike ? -6.0f : -3.0f);
        lo = LaneLo[r] + shift;
        hi = LaneHi[r] + shift;
        return;
    }
    static const float Lo[BalParts - BalLanes][4] = {
        { -11.0f, -11.0f, -12.0f, -11.0f },   // ping
        {  -9.0f,  -9.0f,  -8.0f,  -9.0f },   // bass
        { -11.0f, -11.0f, -12.0f, -10.0f },   // acid
        { -12.0f, -11.0f, -10.0f, -12.0f },   // chord
        { -18.0f, -18.0f, -16.0f, -18.0f },   // drone
        { -22.0f, -22.0f, -20.0f, -22.0f },   // texture
    };
    static const float Hi[BalParts - BalLanes][4] = {
        {  -8.0f,  -8.0f,  -8.0f,  -8.0f },
        {  -5.0f,  -5.0f,  -4.0f,  -5.0f },
        {  -8.0f,  -8.0f,  -8.0f,  -7.0f },
        {  -8.0f,  -8.0f,  -6.0f,  -8.0f },
        { -12.0f, -12.0f, -10.0f, -12.0f },
        { -16.0f, -16.0f, -14.0f, -16.0f },
    };
    const int k = part - BalLanes;
    lo = hi = 0.0f;
    for (int s = 0; s < 4; ++s) { lo += w[s] / sum * Lo[k][s]; hi += w[s] / sum * Hi[k][s]; }
}

float styleTargetLufs(int style)
{
    // The references' loudest 20 seconds per profile, the median (Tools/ref_stats.json, loud20: Hypnotic -10.0, Ostgut
    // -9.5, Dub -11.5, Raw -9.4), rounded to half dB; kept in the profiles (Style.h).
    return styleProfile(static_cast<StyleE>(style)).m_peakLufs;
}

std::vector<LevelReading> levelScore(Score& score, const ParamStore& params, double seconds, const std::function<bool()>& stop)
{
    std::vector<LevelReading> out;
    if (score.m_levels.empty()) return out;
    auto e = std::make_unique<Engine>();
    ParamStore& p = e->params();
    p.copyValuesFrom(params);
    // The player's master level stays out of it: the correction is the track's, the master level comes on top.
    const int master = p.id(ModuleE::MASTER, 0, master::LEVEL);
    p.set(master, p.defaultValue(master));
    e->prepare(Rate, Block);
    // Phase 18: the balance first -- every part's loudest sample against the loudest kick, read in three places, and the
    // parts outside their windows moved to the edge (ranked, corrections()). The parts scale with their gains (set at the
    // sources), so one reading does. Then the guard: the mix's mids against its kick band in the loudest part, and where
    // they stand over the references' (guardCeilings), every boost comes down by the excess, at most three times. The
    // loudness is measured after, with the balance in.
    Score s = score;
    for (LevelMark& m : s.m_levels) if (!std::isnan(m.m_targetLufs)) { m.m_trimDb = 0.0f; m.m_balDb = BalanceDbT{}; }
    e->load(s);
    std::vector<BalanceDbT> found(s.m_levels.size()), bal(s.m_levels.size()), los(s.m_levels.size()), his(s.m_levels.size());
    std::vector<bool> guarded(s.m_levels.size(), false);
    for (size_t i = 0; i < s.m_levels.size(); ++i) {
        const LevelMark& m = s.m_levels[i];
        found[i].fill(std::numeric_limits<float>::quiet_NaN());
        bal[i] = m.m_balDb;
        if (std::isnan(m.m_targetLufs)) continue;
        // Three places: the loudest part, 16 and 40 bars before it (not before the body's bar 33) -- a part is read where
        // it plays; one place alone had a figure breathing out and read its echo (28.09.2026: -53 dB, +12). All against
        // the loudest kick of the three (a kick-out in one of them read the hats 60 dB over the kick).
        PartPeaks peaks;
        for (const double back : { 0.0, 64.0, 160.0 }) {
            const double at = m.m_peakBeat - back;
            if (back > 0.0 && at < m.m_beat + 128.0) continue;
            if (!readParts(*e, s, at, BalSeconds, peaks, nullptr, stop)) return {};
        }
        found[i] = against(peaks);
        los[i].fill(std::numeric_limits<float>::quiet_NaN());
        his[i].fill(std::numeric_limits<float>::quiet_NaN());
        bal[i] = corrections(*e, m, found[i], &los[i], &his[i]);
    }
    for (size_t i = 0; i < s.m_levels.size(); ++i) s.m_levels[i].m_balDb = bal[i];
    for (size_t i = 0; i < s.m_levels.size(); ++i) {
        const LevelMark& m = s.m_levels[i];
        if (std::isnan(m.m_targetLufs)) continue;
        float ceilMid, ceilHigh;
        guardCeilings(m.m_styleMix, ceilMid, ceilHigh);
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
                const double at = m.m_peakBeat - back;
                if (back > 0.0 && at < m.m_beat + 128.0) continue;
                PartPeaks unused;
                if (!readParts(*e, s, at, BalSeconds, unused, read[places], stop)) return {};
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
            float& room = bal[i][static_cast<size_t>(BalPartE::ROOM)];
            if (room > RoomDown) {
                room = std::max(RoomDown, room - excess - 0.5f);
                any = true;
            } else {
                for (float& b : bal[i]) if (b > 0.0f) { b = std::max(0.0f, b - excess - 0.5f); any = true; }
            }
            s.m_levels[i].m_balDb = bal[i];
            guarded[i] = guarded[i] || any;
            if (!any) break;
        }
    }
    // Then the loudness: as composed (with the balance), then with the corrections found (the master compresses, clips
    // and limits after the gain).
    e->load(s);
    for (const LevelMark& m : s.m_levels) {
        if (std::isnan(m.m_targetLufs)) { out.push_back({ m.m_beat, 0.0f, m.m_targetLufs, m.m_trimDb, 0.0f }); continue; }   // not measured
        const std::optional<float> got = measure(*e, s, m.m_peakBeat, seconds, stop);
        if (!got) return {};
        const float measured = *got;
        const float trim = measured > -70.0f ? std::clamp(m.m_targetLufs - measured, -MostDb, MostDb) : 0.0f;
        out.push_back({ m.m_beat, measured, m.m_targetLufs, trim, 0.0f });
    }
    for (size_t i = 0; i < out.size() && i < found.size(); ++i) {
        out[i].m_found = found[i];
        out[i].m_bal = bal[i];
        out[i].m_lo = los[i];
        out[i].m_hi = his[i];
        out[i].m_guarded = guarded[i];
    }
    for (size_t i = 0; i < s.m_levels.size(); ++i) s.m_levels[i].m_trimDb = out[i].m_trim;
    e->load(s);
    for (size_t i = 0; i < s.m_levels.size(); ++i) {
        if (std::isnan(s.m_levels[i].m_targetLufs)) continue;
        const std::optional<float> got = measure(*e, s, s.m_levels[i].m_peakBeat, seconds, stop);
        if (!got) return {};
        const float again = *got;
        if (again > -70.0f) out[i].m_trim = std::clamp(out[i].m_trim + (out[i].m_target - again), -MostDb, MostDb);
        out[i].m_after = again;
    }
    for (size_t i = 0; i < score.m_levels.size() && i < out.size(); ++i) {
        score.m_levels[i].m_trimDb = out[i].m_trim;
        score.m_levels[i].m_balDb = out[i].m_bal;
    }
    return out;
}

} // namespace tot
