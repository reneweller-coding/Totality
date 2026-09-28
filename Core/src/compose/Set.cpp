/**
 * @file Set.cpp
 * @brief The set composer (Set.h).
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

const char* const kDramaturgyNames[] = { "Warm-up", "Peak", "Closing", "Sunday", "Flat", "Cruise", "Marathon" };
const char* const kLoopKindNames[] = { "carry", "tease", "layer" };
const char* const kMoveNames[] = { "low kill", "high swell", "mid dip", "filter build", "echo throw" };

namespace {

constexpr double kBar = 4.0;

uint64_t hashName(const std::string& s)
{
    uint64_t h = 1469598103934665603ull;   // FNV-1a
    for (char c : s) { h ^= static_cast<unsigned char>(c); h *= 1099511628211ull; }
    return h;
}

/**
 * @brief The profile at energy @p e on the ladder Dub, Hypnotic, Ostgut, Raw (morphed between neighbours), centred on the
 *        knobs' style: energy 0.5 is that style, the set's energy moves it up to about a rung and a fifth either way.
 */
StyleProfile ladderProfile(float e, const ParamStore& p)
{
    static const Style kLadder[4] = { Style::Dub, Style::Hypnotic, Style::Ostgut, Style::RawPeak };
    const int base = p.getInt(p.id(Module::Compose, 0, compose::Style));
    const int rung = base == static_cast<int>(Style::Dub) ? 0 : base == static_cast<int>(Style::Hypnotic) ? 1
                   : base == static_cast<int>(Style::Ostgut) ? 2 : 3;
    const float x = std::clamp(static_cast<float>(rung) + (e - 0.5f) * 2.4f, 0.0f, 3.0f);
    const int i = std::min(2, static_cast<int>(x));
    StyleProfile s = morphProfile(styleProfile(kLadder[i]), styleProfile(kLadder[i + 1]), x - static_cast<float>(i), p);
    return axisProfile(s, p.get(p.id(Module::Compose, 0, compose::DubShare)), p.get(p.id(Module::Compose, 0, compose::HypnoticShare)), p);
}

/** @brief Appends @p src, moved @p offset beats later, to @p dst (tempo and length are the caller's). */
void appendShifted(Score& dst, const Score& src, double offset)
{
    for (NoteEvent n : src.notes) { n.beat += offset; dst.notes.push_back(n); }
    for (Gesture g : src.gestures) { g.beat += offset; dst.gestures.push_back(g); }
    for (BlockOp o : src.ops) { o.beat += offset; dst.ops.push_back(o); }
    for (Marker m : src.markers) { m.beat += offset; dst.markers.push_back(m); }
    for (LevelMark l : src.levels) { l.beat += offset; l.peakBeat += offset; dst.levels.push_back(l); }
    for (KnobSet k : src.knobs) { k.beat += offset; dst.knobs.push_back(k); }
    for (SoundPick s : src.sounds) { s.beat += offset; dst.sounds.push_back(s); }
}

Gesture stepOf(const ParamStore& p, int id, double beat, float value)
{
    Gesture g;
    g.param = id;
    g.beat = beat;
    g.length = 0.0;
    g.from = g.to = p.toNormalised(id, value) - p.toNormalised(id, p.get(id));
    g.shape = GestureShape::Step;
    return g;
}

Gesture rampOf(const ParamStore& p, int id, double beat, double length, float from, float to, GestureShape shape)
{
    Gesture g;
    g.param = id;
    g.beat = beat;
    g.length = length;
    g.from = p.toNormalised(id, from) - p.toNormalised(id, p.get(id));
    g.to = p.toNormalised(id, to) - p.toNormalised(id, p.get(id));
    g.shape = shape;
    return g;
}

/**
 * @brief Before each piece on a deck, every knob any piece on that deck moves goes back to the knob (a Step to offset 0
 *        at the piece's start, written before the piece's own), so no piece inherits the last one's sounds.
 */
void homeBetween(Score& deck, const std::vector<std::pair<double, std::vector<Gesture>>>& pieces, int mixFirst, int mixEnd)
{
    std::set<int> moved;
    for (const auto& piece : pieces) for (const Gesture& g : piece.second) if (g.param < mixFirst || g.param >= mixEnd) moved.insert(g.param);
    for (const auto& piece : pieces) {
        std::vector<Gesture> homes;
        for (int id : moved) {
            Gesture g;
            g.param = id;
            g.beat = piece.first;
            g.length = 0.0;
            g.shape = GestureShape::Step;
            homes.push_back(g);
        }
        deck.gestures.insert(deck.gestures.end(), homes.begin(), homes.end());
        for (Gesture g : piece.second) deck.gestures.push_back(g);
    }
}

} // namespace

float setEnergy(Dramaturgy d, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    switch (d) {
    case Dramaturgy::WarmUp: return 0.3f + 0.45f * t;
    case Dramaturgy::Closing: return 0.8f - 0.5f * t;
    case Dramaturgy::Sunday: return 0.45f + 0.1f * static_cast<float>(std::sin(2.0 * 3.141592653589793 * 2.0 * t));
    case Dramaturgy::Flat: return 0.6f;
    case Dramaturgy::Cruise:
        // Klock (Mix-Dok. 6): hard from the start, easing into the cruise within a tenth, building in the last quarter.
        if (t < 0.1f) return 0.85f - 1.5f * t;
        if (t < 0.75f) return 0.7f + 0.05f * static_cast<float>(std::sin(2.0 * 3.141592653589793 * 3.0 * (t - 0.1f) / 0.65f));
        return 0.7f + 0.3f * (t - 0.75f) / 0.25f;
    case Dramaturgy::Marathon:
        // A night of many hours: a warm-up to a fifth, two peaks with a valley between, a long closing.
        if (t < 0.2f) return 0.35f + 1.0f * t;
        if (t < 0.8f) return 0.75f + 0.2f * static_cast<float>(std::sin(2.0 * 3.141592653589793 * 2.0 * (t - 0.2f) / 0.6f - 1.5707963));
        return 0.55f - 0.2f * (t - 0.8f) / 0.2f;
    default: return t < 0.7f ? 0.5f + 0.5f * t / 0.7f : 1.0f - 0.3f * (t - 0.7f) / 0.3f;   // Peak
    }
}

float setTempo(Dramaturgy d, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    switch (d) {
    case Dramaturgy::WarmUp: return 125.0f + 5.0f * t;
    case Dramaturgy::Closing: return 132.0f - 5.0f * t;
    case Dramaturgy::Sunday: return 126.0f + 2.0f * t;
    case Dramaturgy::Flat: return 130.0f;
    case Dramaturgy::Cruise: return t < 0.75f ? 129.0f + 2.0f * t : 130.5f + 2.5f * (t - 0.75f) / 0.25f;
    case Dramaturgy::Marathon: return t < 0.6f ? 126.0f + 8.0f * t / 0.6f : 134.0f - 6.0f * (t - 0.6f) / 0.4f;   // Nodge's climb, a glide down
    default: return 128.0f + 6.0f * t;   // Peak (Dok. 6: Berghain 06 climbs to 134)
    }
}

SetScore composeSet(const ParamStore& p, uint64_t seed, double minutes, const Curation* cur, SetInfo* info)
{
    const Dramaturgy dram = static_cast<Dramaturgy>(p.getInt(p.id(Module::Set, 0, set::Dramaturgy)));
    const bool wander = p.getInt(p.id(Module::Set, 0, set::Journey)) != 0;
    const float loopChance = p.get(p.id(Module::Set, 0, set::Loops));
    const float breakChance = p.get(p.id(Module::Set, 0, set::FxBreaks));
    const int blendBars = p.getInt(p.id(Module::Set, 0, set::BlendBars)) == 0 ? 16 : 32;
    const float trackMinutes = p.get(p.id(Module::Set, 0, set::TrackMinutes));
    const float djHand = p.get(p.id(Module::Set, 0, set::DjHand));
    const double totalSeconds = std::max(1.0, minutes) * 60.0;
    Rng sr;
    sr.seed(mixSeed(mixSeed(seed, hashName("set")), static_cast<uint64_t>(cur != nullptr ? cur->count("set") : 0)));
    const StyleProfile fixed = profileOf(p);

    SetInfo si;
    si.dramaturgy = dram;
    std::vector<Score> scores;   // each track's own score (its beats from 0)
    TempoMap tempo;
    double seconds = 0.0;        // the set's length so far, estimated at the tracks' tempos
    float bpmPrev = 0.0f;
    int keyPrev = -1;
    for (int i = 0; i < 256; ++i) {
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
        // (the fraction drawn), and an intro and an outro of a block each.
        const double want = static_cast<double>(trackMinutes) * bpm / 4.0 / 32.0;
        const int body = std::clamp(static_cast<int>(std::floor(want)) + (sr.uniform() < want - std::floor(want) ? 1 : 0), 2, 8);
        const int blocks = body + 2;
        const std::string unit = "track" + std::to_string(i + 1) + ".";
        const uint64_t trackSeed = mixSeed(mixSeed(seed, 0x545241434Bull + static_cast<uint64_t>(i)),
                                           static_cast<uint64_t>(cur != nullptr ? cur->count("track" + std::to_string(i + 1)) : 0));
        TrackRequest req;
        req.profile = &prof;
        req.bpm = bpm;
        req.key = key;
        req.blocks = blocks;
        req.energy = e;
        req.mixable = true;
        req.quickStart = i == 0;   // (Phase 11: nothing lies under the first one's intro)
        SetTrack st;
        st.energy = e;
        st.seed = trackSeed;
        Score sc = composeTrack(p, trackSeed, req, cur, unit, &st.info);
        if (i == 0) {
            st.deck = 0;
            st.start = 0.0;
            st.swapIn = 0.0;
            tempo.setConstant(bpm);
        } else {
            SetTrack& prev = si.tracks.back();
            const double swap = prev.start + prev.info.outroBar * kBar;
            prev.swapOut = swap;
            st.deck = 1 - prev.deck;
            st.start = swap - st.info.introBars * kBar;
            st.swapIn = swap;
            // The tempo ramps through the blend, to the swap.
            tempo.add(swap - blendBars * kBar, bpmPrev, true);
            tempo.add(swap, bpm, false);
        }
        st.end = st.start + st.info.bars * kBar;
        st.swapOut = st.end;
        seconds = tempo.secondsAt(st.end);
        si.tracks.push_back(st);
        scores.push_back(std::move(sc));
        bpmPrev = bpm;
        keyPrev = key;
        if (tempo.secondsAt(st.start + st.info.outroBar * kBar) >= totalSeconds) break;   // the last one ends the set
    }
    const size_t n = si.tracks.size();

    SetScore out;
    out.lengthBeats = si.tracks.back().end;
    for (Score& d : out.decks) { d.clear(130.0); d.tempo = tempo; d.lengthBeats = out.lengthBeats; }
    // The tracks onto their decks; the mixer's moves at every swap.
    std::vector<std::pair<double, std::vector<Gesture>>> pieces[kDecks];
    std::vector<size_t> pieceOf(n);
    for (size_t i = 0; i < n; ++i) {
        const SetTrack& st = si.tracks[i];
        Score& deck = out.decks[st.deck];
        Score shifted;
        appendShifted(shifted, scores[i], st.start);
        deck.notes.insert(deck.notes.end(), shifted.notes.begin(), shifted.notes.end());
        deck.ops.insert(deck.ops.end(), shifted.ops.begin(), shifted.ops.end());
        deck.markers.insert(deck.markers.end(), shifted.markers.begin(), shifted.markers.end());
        deck.levels.insert(deck.levels.end(), shifted.levels.begin(), shifted.levels.end());
        deck.knobs.insert(deck.knobs.end(), shifted.knobs.begin(), shifted.knobs.end());
        deck.sounds.insert(deck.sounds.end(), shifted.sounds.begin(), shifted.sounds.end());
        std::vector<Gesture> g = shifted.gestures;
        const int d = st.deck;
        const int fader = p.id(Module::Deck, d, deck::Fader), low = p.id(Module::Deck, d, deck::Low);
        const int mid = p.id(Module::Deck, d, deck::Mid), high = p.id(Module::Deck, d, deck::High);
        const int filter = p.id(Module::Deck, d, deck::Filter), send = p.id(Module::Deck, d, deck::FxSend);
        // Every channel starts neutral with its track (the last one on the deck left it faded).
        g.push_back(stepOf(p, filter, st.start, 0.0f));
        g.push_back(stepOf(p, send, st.start, 0.0f));
        if (i > 0) {
            // In (Mix-Dok. 7): the low band killed until the swap; the fader opens blend bars before it with the highs
            // 8 dB down (back over a quarter of the blend) and the mids 10 dB down (back over its last 16 bars). (Phase
            // 10: 18 dB took the incoming track's percussion with it.)
            const double open = std::max(st.start, st.swapIn - blendBars * kBar);
            const double midFrom = std::max(open, st.swapIn - 16.0 * kBar);
            g.push_back(stepOf(p, low, st.start, -60.0f));
            g.push_back(stepOf(p, fader, st.start, -60.0f));
            g.push_back(stepOf(p, high, st.start, -8.0f));
            g.push_back(stepOf(p, mid, st.start, -10.0f));
            g.push_back(rampOf(p, fader, open, 8.0 * kBar, -60.0f, 0.0f, GestureShape::EaseOut));
            g.push_back(rampOf(p, high, open, blendBars / 4.0 * kBar, -8.0f, 0.0f, GestureShape::Linear));
            g.push_back(rampOf(p, mid, midFrom, st.swapIn - midFrom, -10.0f, 0.0f, GestureShape::EaseIn));
            g.push_back(stepOf(p, low, st.swapIn, 0.0f));
        } else {
            g.push_back(stepOf(p, low, st.start, 0.0f));
            g.push_back(stepOf(p, fader, st.start, 0.0f));
            g.push_back(stepOf(p, mid, st.start, 0.0f));
            g.push_back(stepOf(p, high, st.start, 0.0f));
        }
        if (i + 1 < n) {
            // Out: the low band 6 dB down over the 16 bars before the swap ("A Low auf ~11 Uhr"), closed on it; the
            // outgoing deck keeps its hats and its percussion ("A nur noch Hats/Perc") -- its mids 6 dB down in 8 bars,
            // its highs out over 16 -- and its fader falls through the 16 bars after eight more.
            g.push_back(rampOf(p, low, st.swapOut - 16.0 * kBar, 16.0 * kBar, 0.0f, -6.0f, GestureShape::Linear));
            g.push_back(stepOf(p, low, st.swapOut, -60.0f));
            g.push_back(rampOf(p, mid, st.swapOut, 8.0 * kBar, 0.0f, -6.0f, GestureShape::EaseOut));
            g.push_back(rampOf(p, high, st.swapOut, 16.0 * kBar, 0.0f, -12.0f, GestureShape::Linear));
            const double from = st.swapOut + 8.0 * kBar, to = std::min(st.end, st.swapOut + 24.0 * kBar);
            if (to - from > kBar) g.push_back(rampOf(p, fader, from, to - from, 0.0f, -60.0f, GestureShape::EaseIn));
            // A break from the mixer's effects: the outgoing deck thrown into the echo and the hall for a beat.
            if (sr.uniform() < breakChance) {
                g.push_back(stepOf(p, send, st.swapOut - 2.0, 0.7f));
                g.push_back(stepOf(p, send, st.swapOut, 0.0f));
                si.breaks.push_back(st.swapOut - 2.0);
            }
            // The rest into the echo as the fader closes ("Effekte (Delay/Reverb) auf A-Rest").
            if (sr.uniform() < breakChance && to - from > 4.0 * kBar) {
                g.push_back(rampOf(p, send, to - 4.0 * kBar, 4.0 * kBar, 0.0f, 0.6f, GestureShape::EaseIn));
                g.push_back(stepOf(p, send, to + 2.0 * kBar, 0.0f));
            }
        }
        pieceOf[i] = pieces[d].size();
        pieces[d].push_back({ st.start, g });
        out.markers.push_back(Marker{ st.start, "T" + std::to_string(i + 1) + " " + st.info.style + " " + kFormNames[static_cast<int>(st.info.form)]
                                                + " " + st.info.camelot });
        if (i > 0) out.markers.push_back(Marker{ st.swapIn, "Swap T" + std::to_string(i) + " > T" + std::to_string(i + 1) });
    }

    // Phase 9: the DJ's hand between the blends -- on a track's 16-bar lines, never within four bars of its own moments.
    for (size_t i = 0; i < n; ++i) {
        const SetTrack& st = si.tracks[i];
        const int d = st.deck;
        const int low = p.id(Module::Deck, d, deck::Low), mid = p.id(Module::Deck, d, deck::Mid), high = p.id(Module::Deck, d, deck::High);
        const int filter = p.id(Module::Deck, d, deck::Filter), send = p.id(Module::Deck, d, deck::FxSend);
        const double from = (i == 0 ? st.start + st.info.introBars * kBar : st.swapIn) + 8.0 * kBar;
        const double until = i + 1 < n ? si.tracks[i + 1].swapIn - blendBars * kBar - 8.0 * kBar : st.start + st.info.outroBar * kBar;
        std::vector<double> busy;
        for (const auto& mo : st.info.moments) busy.push_back(st.start + mo.first * kBar);
        for (int l : st.info.landings) busy.push_back(st.start + l * kBar);
        for (int r : st.info.reductions) busy.push_back(st.start + r * kBar);
        for (int r : st.info.returns) busy.push_back(st.start + r * kBar);
        std::vector<Gesture>& g = pieces[d][pieceOf[i]].second;
        double free = from;
        for (double line = st.start + std::ceil((from - st.start) / 64.0) * 64.0; line <= until; line += 64.0) {
            const float u = sr.uniform(), k = sr.uniform(), v = sr.uniform();
            if (u >= djHand * 0.9f) continue;   // (the default 0.5: a move on nearly every second line)
            bool near = false;
            for (double b : busy) near = near || std::fabs(b - line) < 4.0 * kBar;
            if (near) continue;
            static const float kW[5] = { 0.30f, 0.20f, 0.15f, 0.20f, 0.15f };
            int kind = 0;
            for (float x = k; kind < 4 && x >= kW[kind]; ++kind) x -= kW[kind];
            const double lead = kind == 3 ? (v < 0.5f ? 8.0 : 16.0) * kBar : kind == 2 ? 8.0 * kBar : kind == 0 ? 2.0 * kBar : 0.0;
            if (line - lead < free) continue;
            switch (static_cast<MoveKind>(kind)) {
            case MoveKind::LowKill: {
                const double bars = v < 0.5f ? 1.0 : 2.0;
                g.push_back(stepOf(p, low, line - bars * kBar, -60.0f));
                g.push_back(stepOf(p, low, line, 0.0f));
                break;
            }
            case MoveKind::HighSwell: {
                const double len = (v < 0.5f ? 8.0 : 16.0) * kBar;
                g.push_back(stepOf(p, high, line, -10.0f));
                g.push_back(rampOf(p, high, line, len, -10.0f, 0.0f, GestureShape::EaseIn));
                free = line + len;
                break;
            }
            case MoveKind::MidDip:
                g.push_back(rampOf(p, mid, line - 8.0 * kBar, 4.0 * kBar, 0.0f, -8.0f, GestureShape::EaseOut));
                g.push_back(stepOf(p, mid, line, 0.0f));
                break;
            case MoveKind::FilterBuild:
                g.push_back(rampOf(p, filter, line - lead, lead, 0.0f, 0.45f, GestureShape::EaseIn));
                g.push_back(stepOf(p, filter, line, 0.0f));
                break;
            default:
                g.push_back(stepOf(p, send, line - 1.0, 0.6f));
                g.push_back(stepOf(p, send, line, 0.0f));
                break;
            }
            free = std::max(free, line);
            si.moves.push_back(SetMove{ line, d, static_cast<MoveKind>(kind) });
        }
    }

    // Phase 9: the third deck, what is borrowed -- the carry, the tease, the layer (Set.h). A track's stretch (its swap to
    // the next) has room for one (p set.loops x 1.4): the carry of the track before (0.4), the tease of the next (0.35,
    // where the keys agree and it has a figure), the layer of the track before last (0.25); one at a time on the deck,
    // each with its tails ringing out for 16 bars before the next.
    struct Want { double start, end; int from; LoopKind kind; int bars; };
    std::vector<Want> wants;
    for (size_t i = 1; i < n; ++i) {
        const SetTrack& st = si.tracks[i];
        const float u = sr.uniform(), uk = sr.uniform(), ub = sr.uniform(), ulen = sr.uniform();
        const int bars = ub < 0.5f ? 4 : 8;
        bool tease = false;
        if (i + 1 < n) {
            const SetTrack& nx = si.tracks[i + 1];
            const int dk = ((nx.info.key - st.info.key) % 12 + 12) % 12;
            const int fig = nx.info.figure;
            tease = (dk == 0 || dk == 5 || dk == 7) && fig >= 0 && fig != static_cast<int>(LayerId::Bass) && nx.info.figureBar >= 0;
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
            wants.push_back(Want{ st.swapIn + 8.0 * kBar, st.swapIn + (ulen < 0.5f ? 40.0 : 56.0) * kBar, static_cast<int>(i - 1), LoopKind::Carry, bars });
        } else if (kind == 1) {
            const double open = si.tracks[i + 1].swapIn - blendBars * kBar;
            const double len = (ulen < 0.5f ? 16.0 : 32.0) * kBar;
            wants.push_back(Want{ open - len, open + 8.0 * kBar, static_cast<int>(i + 1), LoopKind::Tease, 4 });
        } else {
            const double at = st.swapIn + 16.0 * kBar;
            wants.push_back(Want{ at, at + (ulen < 0.5f ? 16.0 : 32.0) * kBar, static_cast<int>(i - 2), LoopKind::Layer, bars });
        }
    }
    std::stable_sort(wants.begin(), wants.end(), [](const Want& a, const Want& b) { return a.start < b.start; });
    double deckCFree = 0.0;
    for (const Want& w : wants) {
        if (w.start < deckCFree || w.end > out.lengthBeats) continue;
        const SetTrack& from = si.tracks[static_cast<size_t>(w.from)];
        const Score& src = scores[static_cast<size_t>(w.from)];
        SetLoop lp;
        lp.from = w.from;
        lp.bars = w.bars;
        lp.start = w.start;
        lp.end = w.end;
        lp.kind = w.kind;
        // The source: the figure's first bars (the tease), else the loudest block's first bars of hats and percussion.
        Part figurePart = Part::Ping;
        if (from.info.figure == static_cast<int>(LayerId::Chord)) figurePart = Part::Chord;
        if (from.info.figure == static_cast<int>(LayerId::Acid)) figurePart = Part::Acid;
        const double srcBeat = (w.kind == LoopKind::Tease ? from.info.figureBar : from.info.peakBar) * kBar, len = lp.bars * kBar;
        std::vector<NoteEvent> notes;
        for (const NoteEvent& x : src.notes) {
            const bool kept = w.kind == LoopKind::Tease ? x.part == figurePart : laneOf(x.part) >= 0;
            if (kept && x.beat >= srcBeat - 0.05 && x.beat < srcBeat + len - 0.05) notes.push_back(x);
        }
        if (notes.empty()) continue;
        Score& c = out.decks[2];
        for (double at = lp.start; at < lp.end - 1e-9; at += len)
            for (NoteEvent x : notes) { x.beat = x.beat - srcBeat + at; if (x.beat < lp.end) c.notes.push_back(x); }
        // Its sounds: the source track's own, set at the loop's start; the low band always killed.
        for (KnobSet k : src.knobs) { k.beat = lp.start; c.knobs.push_back(k); }
        for (SoundPick s : src.sounds) { s.beat = lp.start; c.sounds.push_back(s); }
        std::vector<Gesture> g;
        for (const Gesture& x : src.gestures)
            if (x.beat == 0.0 && x.length == 0.0) { Gesture y = x; y.beat = lp.start; g.push_back(y); }
        const int fader = p.id(Module::Deck, 2, deck::Fader), low = p.id(Module::Deck, 2, deck::Low);
        const int filter = p.id(Module::Deck, 2, deck::Filter);
        g.push_back(stepOf(p, low, lp.start, -60.0f));
        for (int k : { deck::Mid, deck::High, deck::FxSend }) g.push_back(stepOf(p, p.id(Module::Deck, 2, k), lp.start, 0.0f));
        const double span = lp.end - lp.start;
        if (w.kind == LoopKind::Tease) {
            // High-passed and opening, rising to hand over to the track itself.
            g.push_back(rampOf(p, filter, lp.start, span - 8.0 * kBar, 0.5f, 0.15f, GestureShape::EaseOut));
            g.push_back(rampOf(p, fader, lp.start, 8.0 * kBar, -60.0f, -8.0f, GestureShape::EaseOut));
            g.push_back(rampOf(p, fader, lp.end - 8.0 * kBar, 8.0 * kBar, -8.0f, -60.0f, GestureShape::EaseIn));
        } else {
            const float level = w.kind == LoopKind::Carry ? -4.0f : -6.0f;
            g.push_back(stepOf(p, filter, lp.start, w.kind == LoopKind::Layer ? 0.2f : 0.0f));
            g.push_back(rampOf(p, fader, lp.start, 4.0 * kBar, -60.0f, level, GestureShape::EaseOut));
            const double fade = std::min(16.0 * kBar, span / 2.0);
            g.push_back(rampOf(p, fader, lp.end - fade, fade, level, -60.0f, GestureShape::EaseIn));
        }
        pieces[2].push_back({ lp.start, g });
        // Its level: the source track's correction (levelSet copies it; not measured on its own).
        c.levels.push_back(LevelMark{ lp.start, from.start + from.info.peakBar * kBar, std::numeric_limits<float>::quiet_NaN(), 0.0f });
        std::string name = kLoopKindNames[static_cast<int>(w.kind)];
        name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
        out.markers.push_back(Marker{ lp.start, name + " of T" + std::to_string(w.from + 1) });
        si.loops.push_back(lp);
        deckCFree = lp.end + 16.0 * kBar;
    }
    for (int d = 0; d < kDecks; ++d) {
        homeBetween(out.decks[d], pieces[d], p.base(Module::Deck, d), p.base(Module::Deck, d) + deck::Count);
        out.decks[d].sort();
    }
    std::stable_sort(out.markers.begin(), out.markers.end(), [](const Marker& a, const Marker& b) { return a.beat < b.beat; });
    std::sort(si.loops.begin(), si.loops.end(), [](const SetLoop& a, const SetLoop& b) { return a.start < b.start; });
    if (info != nullptr) *info = si;
    return out;
}

Score flattenSet(const SetScore& set)
{
    Score s;
    s.clear(130.0);
    s.tempo = set.decks[0].tempo;
    s.lengthBeats = set.lengthBeats;
    for (const Score& d : set.decks) {
        s.notes.insert(s.notes.end(), d.notes.begin(), d.notes.end());
        s.ops.insert(s.ops.end(), d.ops.begin(), d.ops.end());
    }
    s.markers = set.markers;
    if (!set.decks[0].levels.empty()) { s.keyRoot = set.decks[0].keyRoot; s.scale = set.decks[0].scale; }
    s.sort();
    return s;
}

} // namespace tot
