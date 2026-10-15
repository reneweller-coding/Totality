/**
 * @file Set.cpp
 * @brief The set composer (Set.h).
 * @author Rene Weller
 */
#include "tot/compose/Set.h"
#include "tot/Dsp.h"
#include "tot/compose/Style.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <set>

namespace tot {

const char* const DramaturgyNames[] = { "Warm-up", "Peak", "Closing", "Sunday", "Flat", "Cruise", "Marathon" };
const char* const LoopKindNames[] = { "carry", "tease", "layer" };
const char* const MoveNames[] = { "low kill", "high swell", "mid dip", "filter build", "echo throw" };

namespace {

constexpr double Bar = 4.0;   ///< beats per bar

/**
 * @brief The FNV-1a hash of @p s.
 * @param[in] s  the text
 * @return its FNV-1a hash
 */
uint64_t hashName(const std::string& s)
{
    uint64_t h = 1469598103934665603ull;   // FNV-1a
    for (char c : s) { h ^= static_cast<unsigned char>(c); h *= 1099511628211ull; }
    return h;
}

/**
 * @brief The profile at energy @p e on the ladder Dub, Hypnotic, Ostgut, Raw (morphed between neighbours), centred on the
 *        knobs' style: energy 0.5 is that style, the set's energy moves it up to about a rung and a fifth either way.
 * @param[in] e  the energy, 0..1
 * @param[in] p  the knobs
 * @return the style profile at that energy
 */
StyleProfile ladderProfile(float e, const ParamStore& p)
{
    static const StyleE Ladder[4] = { StyleE::DUB, StyleE::HYPNOTIC, StyleE::OSTGUT, StyleE::RAW_PEAK };
    const int base = p.getInt(p.id(ModuleE::COMPOSE, 0, compose::STYLE));
    const int rung = base == static_cast<int>(StyleE::DUB) ? 0 : base == static_cast<int>(StyleE::HYPNOTIC) ? 1
                   : base == static_cast<int>(StyleE::OSTGUT) ? 2 : 3;
    const float x = std::clamp(static_cast<float>(rung) + (e - 0.5f) * 2.4f, 0.0f, 3.0f);
    const int i = std::min(2, static_cast<int>(x));
    StyleProfile s = morphProfile(styleProfile(Ladder[i]), styleProfile(Ladder[i + 1]), x - static_cast<float>(i), p);
    return axisProfile(s, p.get(p.id(ModuleE::COMPOSE, 0, compose::DUB_SHARE)), p.get(p.id(ModuleE::COMPOSE, 0, compose::HYPNOTIC_SHARE)), p);
}

/**
 * @brief Appends @p src, moved @p offset beats later, to @p dst (tempo and length are the caller's).
 * @param[in,out] dst     the score, appended to
 * @param[in]     src     the score appended
 * @param[in]     offset  how many beats later it goes
 */
void appendShifted(Score& dst, const Score& src, double offset)
{
    for (NoteEvent n : src.m_notes) { n.m_beat += offset; dst.m_notes.push_back(n); }
    for (Gesture g : src.m_gestures) { g.m_beat += offset; dst.m_gestures.push_back(g); }
    for (BlockOp o : src.m_ops) { o.m_beat += offset; dst.m_ops.push_back(o); }
    for (Marker m : src.m_markers) { m.m_beat += offset; dst.m_markers.push_back(m); }
    for (LevelMark l : src.m_levels) { l.m_beat += offset; l.m_peakBeat += offset; dst.m_levels.push_back(l); }
    for (KnobSet k : src.m_knobs) { k.m_beat += offset; dst.m_knobs.push_back(k); }
    for (SoundPick s : src.m_sounds) { s.m_beat += offset; dst.m_sounds.push_back(s); }
}

/**
 * @brief A step of knob @p id to @p value at @p beat, as an offset from the knob in @p p.
 * @param[in] p      the knobs
 * @param[in] id     the knob
 * @param[in] beat   where the step is
 * @param[in] value  the value it steps to
 * @return the gesture
 */
Gesture stepOf(const ParamStore& p, int id, double beat, float value)
{
    Gesture g;
    g.m_param = id;
    g.m_beat = beat;
    g.m_length = 0.0;
    g.m_from = g.m_to = p.toNormalised(id, value) - p.toNormalised(id, p.get(id));
    g.m_shape = GestureShapeE::STEP;
    return g;
}

/**
 * @brief A ramp of knob @p id from @p beat over @p length beats, @p from to @p to in @p shape, as offsets from the
 *        knob.
 * @param[in] p       the knobs
 * @param[in] id      the knob
 * @param[in] beat    where the ramp begins
 * @param[in] length  its length in beats
 * @param[in] from    the value it starts at
 * @param[in] to      the value it ends at
 * @param[in] shape   the curve's shape
 * @return the gesture
 */
Gesture rampOf(const ParamStore& p, int id, double beat, double length, float from, float to, GestureShapeE shape)
{
    Gesture g;
    g.m_param = id;
    g.m_beat = beat;
    g.m_length = length;
    g.m_from = p.toNormalised(id, from) - p.toNormalised(id, p.get(id));
    g.m_to = p.toNormalised(id, to) - p.toNormalised(id, p.get(id));
    g.m_shape = shape;
    return g;
}

/**
 * @brief Before each piece on a deck, every knob any piece on that deck moves goes back to the knob (a Step to offset 0
 *        at the piece's start, written before the piece's own), so no piece inherits the last one's sounds.
 * @param[in,out] deck      the deck's score, appended to
 * @param[in]     pieces    the pieces on the deck: their start and their gestures
 * @param[in]     mixFirst  the first mixer parameter
 * @param[in]     mixEnd    one past the last
 */
void homeBetween(Score& deck, const std::vector<std::pair<double, std::vector<Gesture>>>& pieces, int mixFirst, int mixEnd)
{
    std::set<int> moved;
    for (const auto& piece : pieces) for (const Gesture& g : piece.second) if (g.m_param < mixFirst || g.m_param >= mixEnd) moved.insert(g.m_param);
    for (const auto& piece : pieces) {
        std::vector<Gesture> homes;
        for (int id : moved) {
            Gesture g;
            g.m_param = id;
            g.m_beat = piece.first;
            g.m_length = 0.0;
            g.m_shape = GestureShapeE::STEP;
            homes.push_back(g);
        }
        deck.m_gestures.insert(deck.m_gestures.end(), homes.begin(), homes.end());
        for (Gesture g : piece.second) deck.m_gestures.push_back(g);
    }
}

} // namespace

float setEnergy(DramaturgyE d, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    switch (d) {
    case DramaturgyE::WARM_UP: return 0.3f + 0.45f * t;
    case DramaturgyE::CLOSING: return 0.8f - 0.5f * t;
    case DramaturgyE::SUNDAY: return 0.45f + 0.1f * static_cast<float>(std::sin(2.0 * 3.141592653589793 * 2.0 * t));
    case DramaturgyE::FLAT: return 0.6f;
    case DramaturgyE::CRUISE:
        // Klock (Mix-Dok. 6): hard from the start, easing into the cruise within a tenth, building in the last quarter.
        if (t < 0.1f) return 0.85f - 1.5f * t;
        if (t < 0.75f) return 0.7f + 0.05f * static_cast<float>(std::sin(2.0 * 3.141592653589793 * 3.0 * (t - 0.1f) / 0.65f));
        return 0.7f + 0.3f * (t - 0.75f) / 0.25f;
    case DramaturgyE::MARATHON:
        // A night of many hours: a warm-up to a fifth, two peaks with a valley between, a long closing.
        if (t < 0.2f) return 0.35f + 1.0f * t;
        if (t < 0.8f) return 0.75f + 0.2f * static_cast<float>(std::sin(2.0 * 3.141592653589793 * 2.0 * (t - 0.2f) / 0.6f - 1.5707963));
        return 0.55f - 0.2f * (t - 0.8f) / 0.2f;
    default: return t < 0.7f ? 0.5f + 0.5f * t / 0.7f : 1.0f - 0.3f * (t - 0.7f) / 0.3f;   // Peak
    }
}

float setTempo(DramaturgyE d, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    switch (d) {
    case DramaturgyE::WARM_UP: return 125.0f + 5.0f * t;
    case DramaturgyE::CLOSING: return 132.0f - 5.0f * t;
    case DramaturgyE::SUNDAY: return 126.0f + 2.0f * t;
    case DramaturgyE::FLAT: return 130.0f;
    case DramaturgyE::CRUISE: return t < 0.75f ? 129.0f + 2.0f * t : 130.5f + 2.5f * (t - 0.75f) / 0.25f;
    case DramaturgyE::MARATHON: return t < 0.6f ? 126.0f + 8.0f * t / 0.6f : 134.0f - 6.0f * (t - 0.6f) / 0.4f;   // Nodge's climb, a glide down
    default: return 128.0f + 6.0f * t;   // Peak (Dok. 6: Berghain 06 climbs to 134)
    }
}

SetScore composeSet(const ParamStore& p, uint64_t seed, double minutes, const Curation* cur, SetInfo* info, const Preferences* prefs,
                    const std::function<bool()>& stop)
{
    const DramaturgyE dram = static_cast<DramaturgyE>(p.getInt(p.id(ModuleE::SET, 0, set::DRAMATURGY)));
    const bool wander = p.getInt(p.id(ModuleE::SET, 0, set::JOURNEY)) != 0;
    const float loopChance = p.get(p.id(ModuleE::SET, 0, set::LOOPS));
    const float breakChance = p.get(p.id(ModuleE::SET, 0, set::FX_BREAKS));
    const int blendBars = p.getInt(p.id(ModuleE::SET, 0, set::BLEND_BARS)) == 0 ? 16 : 32;
    const float trackMinutes = p.get(p.id(ModuleE::SET, 0, set::TRACK_MINUTES));
    const float djHand = p.get(p.id(ModuleE::SET, 0, set::DJ_HAND));
    const double totalSeconds = std::max(1.0, minutes) * 60.0;
    Rng sr;
    sr.seed(mixSeed(mixSeed(seed, hashName("set")), static_cast<uint64_t>(cur != nullptr ? cur->count("set") : 0)));
    const StyleProfile fixed = profileOf(p);

    SetInfo si;
    si.m_dramaturgy = dram;
    std::vector<Score> scores;   // each track's own score (its beats from 0)
    TempoMap tempo;
    double seconds = 0.0;        // the set's length so far, estimated at the tracks' tempos
    float bpmPrev = 0.0f;
    int keyPrev = -1;
    for (int i = 0; i < 256; ++i) {
        if (i > 0 && stop && stop()) break;   // (a newer set is wanted)
        const float t = static_cast<float>(std::min(1.0, seconds / totalSeconds));
        const float e = setEnergy(dram, t);
        const StyleProfile prof = wander ? ladderProfile(e, p) : fixed;
        float bpm = std::round(2.0f * setTempo(dram, t)) * 0.5f;
        if (i > 0) bpm = std::clamp(bpm, bpmPrev - 1.0f, bpmPrev + 1.0f);
        // The key: a Camelot neighbour (a fifth either way), the same, or any; drawn every time so the stream stays put.
        const float uk = sr.uniform(), ud = sr.uniform();
        const int anyKey = sr.below(12);
        int key = anyKey;
        if (i > 0) key = uk < 0.6f ? (keyPrev + (ud < 0.5f ? 7 : 5)) % 12 : uk < 0.85f ? keyPrev : anyKey;
        // Phase 9: the track's own time -- its body, swap to swap -- in whole blocks near set.track_minutes at its tempo
        // (the fraction drawn), and an intro and an outro of a block each. Phase 19: from the second track on, one
        // more block -- the body's first, which runs under the outgoing track before the swap (Set.h); three at least,
        // or the track before last would still be in its outro on the deck.
        const double want = static_cast<double>(trackMinutes) * bpm / 4.0 / 32.0;
        const int body = std::clamp(static_cast<int>(std::floor(want)) + (sr.uniform() < want - std::floor(want) ? 1 : 0), 3, 8);
        const int blocks = body + (i == 0 ? 2 : 3);
        const std::string unit = "track" + std::to_string(i + 1) + ".";
        const uint64_t trackSeed = mixSeed(mixSeed(seed, 0x545241434Bull + static_cast<uint64_t>(i)),
                                           static_cast<uint64_t>(cur != nullptr ? cur->count("track" + std::to_string(i + 1)) : 0));
        TrackRequest req;
        req.m_profile = &prof;
        req.m_bpm = bpm;
        req.m_key = key;
        req.m_blocks = blocks;
        req.m_energy = e;
        req.m_mixable = true;
        req.m_quickStart = i == 0;   // (Phase 11: nothing lies under the first one's intro)
        // Phase 13: the track's archetype by its style, never its predecessor's (compose.archetype fixes one for all).
        const float ua = sr.uniform();
        if (p.getInt(p.id(ModuleE::COMPOSE, 0, compose::ARCHETYPE)) == 0)
            req.m_archetype = static_cast<int>(pickArchetype(prof, ua, i > 0 ? si.m_tracks.back().m_info.m_archetype : -1, prefs));
        req.m_prefs = prefs;
        SetTrack st;
        st.m_energy = e;
        st.m_seed = trackSeed;
        Score sc = composeTrack(p, trackSeed, req, cur, unit, &st.m_info);
        if (i == 0) {
            st.m_deck = 0;
            st.m_start = 0.0;
            st.m_swapIn = 0.0;
            tempo.setConstant(bpm);
        } else {
            SetTrack& prev = si.m_tracks.back();
            const double swap = prev.m_start + prev.m_info.m_outroBar * Bar;
            prev.m_swapOut = swap;
            st.m_deck = 1 - prev.m_deck;
            // Phase 19: the swap on the incoming track's figure -- the first bar of its body's second block (its bass,
            // in from the body's first bar, is killed at the isolator until then and lands with the figure).
            st.m_start = swap - (st.m_info.m_bassBar + 32) * Bar;
            st.m_swapIn = swap;
            // The tempo ramps through the blend, to the swap.
            tempo.add(swap - blendBars * Bar, bpmPrev, true);
            tempo.add(swap, bpm, false);
        }
        st.m_end = st.m_start + st.m_info.m_bars * Bar;
        st.m_swapOut = st.m_end;
        seconds = tempo.secondsAt(st.m_end);
        si.m_tracks.push_back(st);
        scores.push_back(std::move(sc));
        bpmPrev = bpm;
        keyPrev = key;
        if (tempo.secondsAt(st.m_start + st.m_info.m_outroBar * Bar) >= totalSeconds) break;   // the last one ends the set
    }
    const size_t n = si.m_tracks.size();

    SetScore out;
    out.m_lengthBeats = si.m_tracks.back().m_end;
    for (Score& d : out.m_decks) { d.clear(130.0); d.m_tempo = tempo; d.m_lengthBeats = out.m_lengthBeats; }
    // The tracks onto their decks; the mixer's moves at every swap.
    std::vector<std::pair<double, std::vector<Gesture>>> pieces[Decks];
    std::vector<size_t> pieceOf(n);
    for (size_t i = 0; i < n; ++i) {
        const SetTrack& st = si.m_tracks[i];
        Score& deck = out.m_decks[st.m_deck];
        Score shifted;
        appendShifted(shifted, scores[i], st.m_start);
        deck.m_notes.insert(deck.m_notes.end(), shifted.m_notes.begin(), shifted.m_notes.end());
        deck.m_ops.insert(deck.m_ops.end(), shifted.m_ops.begin(), shifted.m_ops.end());
        deck.m_markers.insert(deck.m_markers.end(), shifted.m_markers.begin(), shifted.m_markers.end());
        deck.m_levels.insert(deck.m_levels.end(), shifted.m_levels.begin(), shifted.m_levels.end());
        deck.m_knobs.insert(deck.m_knobs.end(), shifted.m_knobs.begin(), shifted.m_knobs.end());
        deck.m_sounds.insert(deck.m_sounds.end(), shifted.m_sounds.begin(), shifted.m_sounds.end());
        std::vector<Gesture> g = shifted.m_gestures;
        const int d = st.m_deck;
        const int fader = p.id(ModuleE::DECK, d, deck::FADER), low = p.id(ModuleE::DECK, d, deck::LOW);
        const int mid = p.id(ModuleE::DECK, d, deck::MID), high = p.id(ModuleE::DECK, d, deck::HIGH);
        const int filter = p.id(ModuleE::DECK, d, deck::FILTER), send = p.id(ModuleE::DECK, d, deck::FX_SEND);
        // Every channel starts neutral with its track (the last one on the deck left it faded).
        g.push_back(stepOf(p, filter, st.m_start, 0.0f));
        g.push_back(stepOf(p, send, st.m_start, 0.0f));
        if (i > 0) {
            // In (Mix-Dok. 7): the low band killed until the swap; the fader opens blend bars before it with the highs
            // 8 dB down (back over a quarter of the blend) and the mids 10 dB down (back over its last 16 bars). (Phase
            // 10: 18 dB took the incoming track's percussion with it.)
            const double open = std::max(st.m_start, st.m_swapIn - blendBars * Bar);
            const double midFrom = std::max(open, st.m_swapIn - 16.0 * Bar);
            g.push_back(stepOf(p, low, st.m_start, -60.0f));
            g.push_back(stepOf(p, fader, st.m_start, -60.0f));
            g.push_back(stepOf(p, high, st.m_start, -8.0f));
            g.push_back(stepOf(p, mid, st.m_start, -10.0f));
            g.push_back(rampOf(p, fader, open, 8.0 * Bar, -60.0f, 0.0f, GestureShapeE::EASE_OUT));
            g.push_back(rampOf(p, high, open, blendBars / 4.0 * Bar, -8.0f, 0.0f, GestureShapeE::LINEAR));
            g.push_back(rampOf(p, mid, midFrom, st.m_swapIn - midFrom, -10.0f, 0.0f, GestureShapeE::EASE_IN));
            g.push_back(stepOf(p, low, st.m_swapIn, 0.0f));
        } else {
            g.push_back(stepOf(p, low, st.m_start, 0.0f));
            g.push_back(stepOf(p, fader, st.m_start, 0.0f));
            g.push_back(stepOf(p, mid, st.m_start, 0.0f));
            g.push_back(stepOf(p, high, st.m_start, 0.0f));
        }
        if (i + 1 < n) {
            // Out: the low band 6 dB down over the 16 bars before the swap ("A Low auf ~11 Uhr"), closed on it; the
            // outgoing deck keeps its hats and its percussion ("A nur noch Hats/Perc") -- its mids 6 dB down in 8 bars,
            // its highs out over 16 -- and its fader falls through the 16 bars after eight more.
            g.push_back(rampOf(p, low, st.m_swapOut - 16.0 * Bar, 16.0 * Bar, 0.0f, -6.0f, GestureShapeE::LINEAR));
            g.push_back(stepOf(p, low, st.m_swapOut, -60.0f));
            g.push_back(rampOf(p, mid, st.m_swapOut, 8.0 * Bar, 0.0f, -6.0f, GestureShapeE::EASE_OUT));
            g.push_back(rampOf(p, high, st.m_swapOut, 16.0 * Bar, 0.0f, -12.0f, GestureShapeE::LINEAR));
            const double from = st.m_swapOut + 8.0 * Bar, to = std::min(st.m_end, st.m_swapOut + 24.0 * Bar);
            if (to - from > Bar) g.push_back(rampOf(p, fader, from, to - from, 0.0f, -60.0f, GestureShapeE::EASE_IN));
            // A break from the mixer's effects: the outgoing deck thrown into the echo and the hall for a beat.
            if (sr.uniform() < breakChance) {
                g.push_back(stepOf(p, send, st.m_swapOut - 2.0, 0.7f));
                g.push_back(stepOf(p, send, st.m_swapOut, 0.0f));
                si.m_breaks.push_back(st.m_swapOut - 2.0);
            }
            // The rest into the echo as the fader closes ("Effekte (Delay/Reverb) auf A-Rest").
            if (sr.uniform() < breakChance && to - from > 4.0 * Bar) {
                g.push_back(rampOf(p, send, to - 4.0 * Bar, 4.0 * Bar, 0.0f, 0.6f, GestureShapeE::EASE_IN));
                g.push_back(stepOf(p, send, to + 2.0 * Bar, 0.0f));
            }
        }
        pieceOf[i] = pieces[d].size();
        pieces[d].push_back({ st.m_start, g });
        out.m_markers.push_back(Marker{ st.m_start, "T" + std::to_string(i + 1) + " " + st.m_info.m_style + " " + FormNames[static_cast<int>(st.m_info.m_form)]
                                                + " " + st.m_info.m_camelot });
        if (i > 0) out.m_markers.push_back(Marker{ st.m_swapIn, "Swap T" + std::to_string(i) + " > T" + std::to_string(i + 1) });
    }

    // Phase 9: the DJ's hand between the blends -- on a track's 16-bar lines, never within four bars of its own moments.
    for (size_t i = 0; i < n; ++i) {
        const SetTrack& st = si.m_tracks[i];
        const int d = st.m_deck;
        const int low = p.id(ModuleE::DECK, d, deck::LOW), mid = p.id(ModuleE::DECK, d, deck::MID), high = p.id(ModuleE::DECK, d, deck::HIGH);
        const int filter = p.id(ModuleE::DECK, d, deck::FILTER), send = p.id(ModuleE::DECK, d, deck::FX_SEND);
        const double from = (i == 0 ? st.m_start + st.m_info.m_introBars * Bar : st.m_swapIn) + 8.0 * Bar;
        const double until = i + 1 < n ? si.m_tracks[i + 1].m_swapIn - blendBars * Bar - 8.0 * Bar : st.m_start + st.m_info.m_outroBar * Bar;
        std::vector<double> busy;
        for (const auto& mo : st.m_info.m_moments) busy.push_back(st.m_start + mo.first * Bar);
        for (int l : st.m_info.m_landings) busy.push_back(st.m_start + l * Bar);
        for (int r : st.m_info.m_reductions) busy.push_back(st.m_start + r * Bar);
        for (int r : st.m_info.m_returns) busy.push_back(st.m_start + r * Bar);
        std::vector<Gesture>& g = pieces[d][pieceOf[i]].second;
        double free = from;
        for (double line = st.m_start + std::ceil((from - st.m_start) / 64.0) * 64.0; line <= until; line += 64.0) {
            const float u = sr.uniform(), k = sr.uniform(), v = sr.uniform();
            if (u >= djHand * 0.9f) continue;   // (the default 0.5: a move on nearly every second line)
            bool near = false;
            for (double b : busy) near = near || std::fabs(b - line) < 4.0 * Bar;
            if (near) continue;
            static const float W[5] = { 0.30f, 0.20f, 0.15f, 0.20f, 0.15f };
            int kind = 0;
            for (float x = k; kind < 4 && x >= W[kind]; ++kind) x -= W[kind];
            const double lead = kind == 3 ? (v < 0.5f ? 8.0 : 16.0) * Bar : kind == 2 ? 8.0 * Bar : kind == 0 ? 2.0 * Bar : 0.0;
            if (line - lead < free) continue;
            switch (static_cast<MoveKindE>(kind)) {
            case MoveKindE::LOW_KILL: {
                const double bars = v < 0.5f ? 1.0 : 2.0;
                g.push_back(stepOf(p, low, line - bars * Bar, -60.0f));
                g.push_back(stepOf(p, low, line, 0.0f));
                break;
            }
            case MoveKindE::HIGH_SWELL: {
                const double len = (v < 0.5f ? 8.0 : 16.0) * Bar;
                g.push_back(stepOf(p, high, line, -10.0f));
                g.push_back(rampOf(p, high, line, len, -10.0f, 0.0f, GestureShapeE::EASE_IN));
                free = line + len;
                break;
            }
            case MoveKindE::MID_DIP:
                g.push_back(rampOf(p, mid, line - 8.0 * Bar, 4.0 * Bar, 0.0f, -8.0f, GestureShapeE::EASE_OUT));
                g.push_back(stepOf(p, mid, line, 0.0f));
                break;
            case MoveKindE::FILTER_BUILD:
                g.push_back(rampOf(p, filter, line - lead, lead, 0.0f, 0.45f, GestureShapeE::EASE_IN));
                g.push_back(stepOf(p, filter, line, 0.0f));
                break;
            default:
                g.push_back(stepOf(p, send, line - 1.0, 0.6f));
                g.push_back(stepOf(p, send, line, 0.0f));
                break;
            }
            free = std::max(free, line);
            si.m_moves.push_back(SetMove{ line, d, static_cast<MoveKindE>(kind) });
        }
    }

    // Phase 9: the third deck, what is borrowed -- the carry, the tease, the layer (Set.h). A track's stretch (its swap to
    // the next) has room for one (p set.loops x 1.4): the carry of the track before (0.4), the tease of the next (0.35,
    // where the keys agree and it has a figure), the layer of the track before last (0.25); one at a time on the deck,
    // each with its tails ringing out for 16 bars before the next.
    struct Want { double m_start, m_end; int m_from; LoopKindE m_kind; int m_bars; };
    std::vector<Want> wants;
    for (size_t i = 1; i < n; ++i) {
        const SetTrack& st = si.m_tracks[i];
        const float u = sr.uniform(), uk = sr.uniform(), ub = sr.uniform(), ulen = sr.uniform();
        const int bars = ub < 0.5f ? 4 : 8;
        bool tease = false;
        if (i + 1 < n) {
            const SetTrack& nx = si.m_tracks[i + 1];
            const int dk = ((nx.m_info.m_key - st.m_info.m_key) % 12 + 12) % 12;
            const int fig = nx.m_info.m_figure;
            tease = (dk == 0 || dk == 5 || dk == 7) && fig >= 0 && fig != static_cast<int>(LayerIdE::BASS) && nx.m_info.m_figureBar >= 0;
        }
        const bool carry = i >= 2, layer = i >= 2;
        const float w[3] = { carry ? 0.4f : 0.0f, tease ? 0.35f : 0.0f, layer ? 0.25f : 0.0f };
        const float sum = w[0] + w[1] + w[2];
        if (sum <= 0.0f || u >= std::min(1.0f, loopChance * 1.4f)) continue;
        float x = uk * sum;
        int kind = 0;
        while (kind < 2 && x >= w[kind]) x -= w[kind++];
        if (w[kind] <= 0.0f) continue;
        if (kind == 0) {
            wants.push_back(Want{ st.m_swapIn + 8.0 * Bar, st.m_swapIn + (ulen < 0.5f ? 40.0 : 56.0) * Bar, static_cast<int>(i - 1), LoopKindE::CARRY, bars });
        } else if (kind == 1) {
            const double open = si.m_tracks[i + 1].m_swapIn - blendBars * Bar;
            const double len = (ulen < 0.5f ? 16.0 : 32.0) * Bar;
            wants.push_back(Want{ open - len, open + 8.0 * Bar, static_cast<int>(i + 1), LoopKindE::TEASE, 4 });
        } else {
            const double at = st.m_swapIn + 16.0 * Bar;
            wants.push_back(Want{ at, at + (ulen < 0.5f ? 16.0 : 32.0) * Bar, static_cast<int>(i - 2), LoopKindE::LAYER, bars });
        }
    }
    std::stable_sort(wants.begin(), wants.end(), [](const Want& a, const Want& b) { return a.m_start < b.m_start; });
    double deckCFree = 0.0;
    for (const Want& w : wants) {
        if (w.m_start < deckCFree || w.m_end > out.m_lengthBeats) continue;
        const SetTrack& from = si.m_tracks[static_cast<size_t>(w.m_from)];
        const Score& src = scores[static_cast<size_t>(w.m_from)];
        SetLoop lp;
        lp.m_from = w.m_from;
        lp.m_bars = w.m_bars;
        lp.m_start = w.m_start;
        lp.m_end = w.m_end;
        lp.m_kind = w.m_kind;
        // The source: the figure's first bars (the tease), else the loudest block's first bars of hats and percussion.
        PartE figurePart = PartE::PING;
        if (from.m_info.m_figure == static_cast<int>(LayerIdE::CHORD)) figurePart = PartE::CHORD;
        if (from.m_info.m_figure == static_cast<int>(LayerIdE::ACID)) figurePart = PartE::ACID;
        const double srcBeat = (w.m_kind == LoopKindE::TEASE ? from.m_info.m_figureBar : from.m_info.m_peakBar) * Bar, len = lp.m_bars * Bar;
        std::vector<NoteEvent> notes;
        for (const NoteEvent& x : src.m_notes) {
            const bool kept = w.m_kind == LoopKindE::TEASE ? x.m_part == figurePart : laneOf(x.m_part) >= 0;
            if (kept && x.m_beat >= srcBeat - 0.05 && x.m_beat < srcBeat + len - 0.05) notes.push_back(x);
        }
        if (notes.empty()) continue;
        Score& c = out.m_decks[2];
        for (double at = lp.m_start; at < lp.m_end - 1e-9; at += len)
            for (NoteEvent x : notes) { x.m_beat = x.m_beat - srcBeat + at; if (x.m_beat < lp.m_end) c.m_notes.push_back(x); }
        // Its sounds: the source track's own, set at the loop's start; the low band always killed.
        for (KnobSet k : src.m_knobs) { k.m_beat = lp.m_start; c.m_knobs.push_back(k); }
        for (SoundPick s : src.m_sounds) { s.m_beat = lp.m_start; c.m_sounds.push_back(s); }
        std::vector<Gesture> g;
        for (const Gesture& x : src.m_gestures)
            if (x.m_beat == 0.0 && x.m_length == 0.0) { Gesture y = x; y.m_beat = lp.m_start; g.push_back(y); }
        const int fader = p.id(ModuleE::DECK, 2, deck::FADER), low = p.id(ModuleE::DECK, 2, deck::LOW);
        const int filter = p.id(ModuleE::DECK, 2, deck::FILTER);
        g.push_back(stepOf(p, low, lp.m_start, -60.0f));
        for (int k : { deck::MID, deck::HIGH, deck::FX_SEND }) g.push_back(stepOf(p, p.id(ModuleE::DECK, 2, k), lp.m_start, 0.0f));
        const double span = lp.m_end - lp.m_start;
        if (w.m_kind == LoopKindE::TEASE) {
            // High-passed and opening, rising to hand over to the track itself.
            g.push_back(rampOf(p, filter, lp.m_start, span - 8.0 * Bar, 0.5f, 0.15f, GestureShapeE::EASE_OUT));
            g.push_back(rampOf(p, fader, lp.m_start, 8.0 * Bar, -60.0f, -8.0f, GestureShapeE::EASE_OUT));
            g.push_back(rampOf(p, fader, lp.m_end - 8.0 * Bar, 8.0 * Bar, -8.0f, -60.0f, GestureShapeE::EASE_IN));
        } else {
            const float level = w.m_kind == LoopKindE::CARRY ? -4.0f : -6.0f;
            g.push_back(stepOf(p, filter, lp.m_start, w.m_kind == LoopKindE::LAYER ? 0.2f : 0.0f));
            g.push_back(rampOf(p, fader, lp.m_start, 4.0 * Bar, -60.0f, level, GestureShapeE::EASE_OUT));
            const double fade = std::min(16.0 * Bar, span / 2.0);
            g.push_back(rampOf(p, fader, lp.m_end - fade, fade, level, -60.0f, GestureShapeE::EASE_IN));
        }
        pieces[2].push_back({ lp.m_start, g });
        // Its level: the source track's correction (levelSet copies it; not measured on its own).
        c.m_levels.push_back(LevelMark{ lp.m_start, from.m_start + from.m_info.m_peakBar * Bar, std::numeric_limits<float>::quiet_NaN(), 0.0f });
        std::string name = LoopKindNames[static_cast<int>(w.m_kind)];
        name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
        out.m_markers.push_back(Marker{ lp.m_start, name + " of T" + std::to_string(w.m_from + 1) });
        si.m_loops.push_back(lp);
        deckCFree = lp.m_end + 16.0 * Bar;
    }
    for (int d = 0; d < Decks; ++d) {
        homeBetween(out.m_decks[d], pieces[d], p.base(ModuleE::DECK, d), p.base(ModuleE::DECK, d) + deck::COUNT);
        out.m_decks[d].sort();
    }
    std::stable_sort(out.m_markers.begin(), out.m_markers.end(), [](const Marker& a, const Marker& b) { return a.m_beat < b.m_beat; });
    std::sort(si.m_loops.begin(), si.m_loops.end(), [](const SetLoop& a, const SetLoop& b) { return a.m_start < b.m_start; });
    if (info != nullptr) *info = si;
    return out;
}

Score flattenSet(const SetScore& set)
{
    Score s;
    s.clear(130.0);
    s.m_tempo = set.m_decks[0].m_tempo;
    s.m_lengthBeats = set.m_lengthBeats;
    for (const Score& d : set.m_decks) {
        s.m_notes.insert(s.m_notes.end(), d.m_notes.begin(), d.m_notes.end());
        s.m_ops.insert(s.m_ops.end(), d.m_ops.begin(), d.m_ops.end());
    }
    s.m_markers = set.m_markers;
    if (!set.m_decks[0].m_levels.empty()) { s.m_keyRoot = set.m_decks[0].m_keyRoot; s.m_scale = set.m_decks[0].m_scale; }
    s.sort();
    return s;
}

} // namespace tot
