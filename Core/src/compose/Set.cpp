/**
 * @file Set.cpp
 * @brief The set composer (Set.h).
 */
#include "umb/compose/Set.h"
#include "umb/Dsp.h"
#include "umb/compose/Style.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace umb {

const char* const kDramaturgyNames[] = { "Warm-up", "Peak", "Closing", "Sunday", "Flat" };

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
    for (int i = 0; i < 64; ++i) {
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
        const int blocks = prof.blocksLow + sr.below(prof.blocksHigh - prof.blocksLow + 1);
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

    SetScore out;
    out.lengthBeats = si.tracks.back().end;
    for (Score& d : out.decks) { d.clear(130.0); d.tempo = tempo; d.lengthBeats = out.lengthBeats; }
    // The tracks onto their decks; the mixer's moves at every swap.
    std::vector<std::pair<double, std::vector<Gesture>>> pieces[kDecks];
    for (size_t i = 0; i < si.tracks.size(); ++i) {
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
        if (i > 0) {
            // In: the low band killed and the fader closed from the start; the fader opens over 8 bars, blend bars before
            // the swap; the low band opens on the swap.
            const double open = std::max(st.start, st.swapIn - blendBars * kBar);
            g.push_back(stepOf(p, low, st.start, -60.0f));
            g.push_back(stepOf(p, fader, st.start, -60.0f));
            g.push_back(rampOf(p, fader, open, 8.0 * kBar, -60.0f, 0.0f, GestureShape::EaseOut));
            g.push_back(stepOf(p, low, st.swapIn, 0.0f));
        } else {
            g.push_back(stepOf(p, low, st.start, 0.0f));
            g.push_back(stepOf(p, fader, st.start, 0.0f));
        }
        if (i + 1 < si.tracks.size()) {
            // Out: the low band closes on the swap; after eight more bars the fader falls through the outro.
            g.push_back(stepOf(p, low, st.swapOut, -60.0f));
            const double from = st.swapOut + 8.0 * kBar;
            if (st.end - from > kBar) g.push_back(rampOf(p, fader, from, st.end - from, 0.0f, -60.0f, GestureShape::EaseIn));
            // A break from the mixer's effects: the outgoing deck thrown into the echo and the hall for a beat.
            if (sr.uniform() < breakChance) {
                const int send = p.id(Module::Deck, d, deck::FxSend);
                g.push_back(stepOf(p, send, st.swapOut - 2.0, 0.7f));
                g.push_back(stepOf(p, send, st.swapOut, 0.0f));
                si.breaks.push_back(st.swapOut - 2.0);
            }
        }
        pieces[d].push_back({ st.start, g });
        out.markers.push_back(Marker{ st.start, "T" + std::to_string(i + 1) + " " + st.info.style + " " + kFormNames[static_cast<int>(st.info.form)]
                                                + " " + st.info.camelot });
        if (i > 0) out.markers.push_back(Marker{ st.swapIn, "Swap T" + std::to_string(i) + " > T" + std::to_string(i + 1) });
    }
    // The live loops: from the second swap on, a loop of the outgoing track on the third deck.
    double deckCFree = 0.0;
    for (size_t i = 1; i + 1 < si.tracks.size(); ++i) {
        const float u = sr.uniform(), ub = sr.uniform(), ul = sr.uniform();
        const SetTrack& from = si.tracks[i];
        const double start = from.swapOut;
        if (u >= loopChance || start < deckCFree) continue;
        SetLoop lp;
        lp.from = static_cast<int>(i);
        lp.bars = ub < 0.5f ? 4 : 8;
        lp.start = start;
        lp.end = start + (ul < 0.5f ? 32.0 : 64.0) * kBar;
        // The source: the loudest block's first bars; hats, percussion and ping only.
        const double srcBeat = from.info.peakBar * kBar, len = lp.bars * kBar;
        std::vector<NoteEvent> src;
        for (const NoteEvent& n : scores[i].notes) {
            const bool kept = laneOf(n.part) >= 0 || n.part == Part::Ping;
            if (kept && n.beat >= srcBeat - 0.05 && n.beat < srcBeat + len - 0.05) src.push_back(n);
        }
        Score& c = out.decks[2];
        for (double at = lp.start; at < lp.end - 1e-9; at += len)
            for (NoteEvent n : src) { n.beat = n.beat - srcBeat + at; c.notes.push_back(n); }
        // Its sounds: the source track's own, set at the loop's start; the low band always killed.
        for (KnobSet k : scores[i].knobs) { k.beat = lp.start; c.knobs.push_back(k); }
        for (SoundPick s : scores[i].sounds) { s.beat = lp.start; c.sounds.push_back(s); }
        std::vector<Gesture> g;
        for (const Gesture& x : scores[i].gestures)
            if (x.beat == 0.0 && x.length == 0.0) { Gesture y = x; y.beat = lp.start; g.push_back(y); }
        const int fader = p.id(Module::Deck, 2, deck::Fader), low = p.id(Module::Deck, 2, deck::Low);
        g.push_back(stepOf(p, low, lp.start, -60.0f));
        g.push_back(rampOf(p, fader, lp.start, 4.0 * kBar, -60.0f, -4.0f, GestureShape::EaseOut));
        g.push_back(rampOf(p, fader, lp.end - 16.0 * kBar, 16.0 * kBar, -4.0f, -60.0f, GestureShape::EaseIn));
        pieces[2].push_back({ lp.start, g });
        // Its level: the source track's correction (levelSet copies it; not measured on its own).
        c.levels.push_back(LevelMark{ lp.start, from.start + from.info.peakBar * kBar, std::numeric_limits<float>::quiet_NaN(), 0.0f });
        out.markers.push_back(Marker{ lp.start, "Loop of T" + std::to_string(i + 1) });
        si.loops.push_back(lp);
        deckCFree = lp.end + 32.0 * kBar;   // its tails ring out before the next
    }
    for (int d = 0; d < kDecks; ++d) {
        homeBetween(out.decks[d], pieces[d], p.base(Module::Deck, d), p.base(Module::Deck, d) + deck::Count);
        out.decks[d].sort();
    }
    std::stable_sort(out.markers.begin(), out.markers.end(), [](const Marker& a, const Marker& b) { return a.beat < b.beat; });
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

} // namespace umb
