/**
 * @file Composer.cpp
 * @brief One track: form, harmony, rack, layers, candidates, events, hands, sounds (Composer.h).
 */
#include "umb/compose/Composer.h"
#include "umb/Presets.h"
#include "umb/Dsp.h"
#include "umb/compose/Corridor.h"
#include "umb/compose/GestureEngine.h"
#include "umb/pattern/Rack.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace umb {

const char* const kFormNames[] = { "Arc", "Peak", "Endless" };
const char* const kUnitNames[8] = { "form", "harmony", "rack", "layers", "blocks", "events", "hands", "sounds" };

std::string camelotOf(int key)
{
    const int k = ((key % 12) + 12) % 12;
    return std::to_string(((k * 7) % 12 + 4) % 12 + 1) + "A";
}

namespace {

constexpr int kCandidates = 8;
constexpr int kBar = 4;   ///< beats per bar
/** The names of the layers in a unit ("rack.clap"). */
const char* const kLayerUnit[kNumLayers] = { "kick", "ghost", "ch", "rolling", "oh", "ride", "clap", "clapb", "clapghost",
                                             "shaker", "tom", "rim", "bass", "ping", "chord", "drone", "acid", "texture" };

uint64_t hashName(const std::string& s)
{
    uint64_t h = 1469598103934665603ull;   // FNV-1a
    for (char c : s) { h ^= static_cast<unsigned char>(c); h *= 1099511628211ull; }
    return h;
}

/** @brief The seed of a unit's stream: the track's seed, the unit's name, how often it was rerolled. */
uint64_t streamSeed(uint64_t seed, const Curation* cur, const std::string& unit, const std::string& name)
{
    const int n = cur != nullptr ? cur->count(unit + name) : 0;
    return mixSeed(mixSeed(seed, hashName(name)), static_cast<uint64_t>(n));
}

Rng streamOf(uint64_t seed, const Curation* cur, const std::string& unit, const std::string& name)
{
    Rng r;
    r.seed(streamSeed(seed, cur, unit, name));
    return r;
}

int L(LayerId id) { return static_cast<int>(id); }

bool isTonal(LayerId id)
{
    return id == LayerId::Bass || id == LayerId::Acid || id == LayerId::Chord || id == LayerId::Drone
        || id == LayerId::Texture || id == LayerId::Ping;
}

/** @brief Dok. 8.5's order of entry: 0 the sub bass (with the body), 1 hats and ride, 2 clap and perc, 3 the 303 and the
 *         ping, 4 the stab, 5 pad and texture. */
int groupOf(LayerId id)
{
    switch (id) {
    case LayerId::Bass: return 0;
    case LayerId::OpenHat: case LayerId::Ride: case LayerId::RollingHat: return 1;
    case LayerId::ClapA: case LayerId::Shaker: case LayerId::TomConga: case LayerId::Rim: case LayerId::GhostKick: return 2;
    case LayerId::Acid: case LayerId::Ping: return 3;
    case LayerId::Chord: return 4;
    default: return 5;
    }
}

/** @brief The knobs the composer sets on every track (its KnobSets): every synth's sound, every knob a style names. */
const std::vector<int>& managedKnobs(const ParamStore& p)
{
    static const std::vector<int> ids = [&p] {
        std::set<int> s;
        for (int id = 0; id < p.count(); ++id) {
            const Module m = p.moduleOf(id);
            if (hasPresets(m) && !presetLeaves(m, p.indexOf(id))) s.insert(id);
        }
        for (int st = 0; st < 4; ++st) {
            const StyleProfile& prof = styleProfile(static_cast<Style>(st));
            for (const SoundValue& v : prof.recipe) if (const int id = p.find(v.key); id >= 0) s.insert(id);
            for (const SoundRange& r : prof.sounds) if (const int id = p.find(r.key); id >= 0) s.insert(id);
        }
        return std::vector<int>(s.begin(), s.end());
    }();
    return ids;
}

/** @brief The values a track sets on the knobs, and gestures in those values (offsets from the knobs underneath). */
struct TrackKnobs {
    const ParamStore& p;
    std::map<int, float> value;   ///< real units
    float get(int id) const { const auto it = value.find(id); return it == value.end() ? p.get(id) : it->second; }
    /** @brief A gesture's offset: from the track's own value where it sets the knob (a KnobSet), else from the knob. */
    float offset(int id, float v) const { return p.toNormalised(id, v) - p.toNormalised(id, get(id)); }
    Gesture ramp(int id, double beat, double length, float from, float to, GestureShape shape, uint8_t hand) const
    {
        Gesture g;
        g.param = id;
        g.beat = beat;
        g.length = length;
        g.from = offset(id, from);
        g.to = offset(id, to);
        g.shape = shape;
        g.hand = hand;
        return g;
    }
    Gesture step(int id, double beat, float v) const { return ramp(id, beat, 0.0, v, v, GestureShape::Step, 0); }
    /** @brief Back to the track's own value at @p beat. */
    Gesture home(int id, double beat) const { return step(id, beat, get(id)); }
};

/** @brief Which layers play, and how densely. */
struct LayerState {
    bool active[kNumLayers] = {};
    float density[kNumLayers];
    LayerState() { for (float& d : density) d = 1.0f; }
    /** @brief Layers sounding beside the kick; the rolling hat counts once it plays in full. */
    int count() const
    {
        int n = 0;
        for (int i = 1; i < kNumLayers; ++i)
            if (active[i] && !(i == L(LayerId::RollingHat) && density[i] < 1.0f)) ++n;
        return n;
    }
};

/** @brief What an event does to one bar. */
struct BarMods {
    int muteStep = -1;        ///< a step where only the kick plays
    bool dropout = false;     ///< the kick (and ghost, bass, 303) out for the bar
    float ghostBoost = 1.0f;  ///< a ghost more
};

/** @brief Dok. 8.9's harmony check as a filter: at most four pitch classes over what plays, the bass at most two. */
void limitHarmony(RackPlan& plan, const std::vector<LayerId>& pool, bool subOwns)
{
    const auto has = [&](LayerId id) { return std::find(pool.begin(), pool.end(), id) != pool.end(); };
    if (plan.bassSize > 2) { plan.bassSize = 2; plan.bassSet[0] = 0; plan.bassSet[1] = 7; }
    const auto pcs = [&]() {
        std::set<int> s = { 0 };
        const auto add = [&](int semis) { s.insert(((semis % 12) + 12) % 12); };
        if (has(LayerId::Chord)) {
            for (int t = 0; t < plan.nChordTones; ++t) add(plan.chordTones[t]);
            if (plan.shuttleBars > 0) for (int t = 0; t < 3; ++t) add(plan.shuttleTones[t]);
        }
        if (subOwns) for (int t = 0; t < plan.bassSize; ++t) add(plan.bassSet[t]);
        if (has(LayerId::Ping)) for (int k = 0; k < std::max(1, plan.period[L(LayerId::Ping)]); ++k) add(plan.pingNote[k] - plan.pingRoot);
        if (has(LayerId::Drone)) add(plan.droneNote - plan.chordRoot);
        if (has(LayerId::Acid))
            for (int k = 0; k < 12; ++k) if ((plan.acidMask & (1u << k)) && inScale(plan.scale, k)) add(k);
        return s;
    };
    // The 303's alphabet (root, octave, fifth, flat seventh, minor third, fourth) is its own: narrowed to the pitch
    // classes the rest already plays, else to root and fifth.
    if (has(LayerId::Acid)) plan.acidMask = static_cast<uint16_t>((1u << 0) | (1u << 7) | (1u << 10) | (1u << 3) | (1u << 5));
    if (pcs().size() <= 4) return;
    plan.shuttleBars = 0;                                              // the second chord first
    if (pcs().size() <= 4) return;
    for (int k = 0; k < 64; ++k) plan.pingNote[k] = plan.pingRoot;    // then the ping's second tone
    if (pcs().size() <= 4) return;
    if (has(LayerId::Acid)) {                                          // then the 303's alphabet
        uint16_t mask = (1u << 0) | (1u << 7);
        std::set<int> others = { 0, 7 };
        if (has(LayerId::Chord)) for (int t = 0; t < plan.nChordTones; ++t) others.insert(((plan.chordTones[t] % 12) + 12) % 12);
        if (subOwns) for (int t = 0; t < plan.bassSize; ++t) others.insert(plan.bassSet[t] % 12);
        for (int k : others) if (inScale(plan.scale, k)) mask = static_cast<uint16_t>(mask | (1u << k));
        plan.acidMask = mask;
    }
    if (pcs().size() <= 4) return;
    plan.nChordTones = 3;                                              // at last the chord a triad
    plan.chordTones[0] = 0; plan.chordTones[1] = 3; plan.chordTones[2] = 7;
}

} // namespace

Score composeTrack(const ParamStore& p, uint64_t seed, const TrackRequest& req, const Curation* cur, const std::string& unit,
                   TrackInfo* info)
{
    const StyleProfile prof = req.profile != nullptr ? *req.profile : profileOf(p);
    const auto pid = [&](Module m, int i, int k) { return p.id(m, i, k); };
    const bool autoOn = p.getInt(pid(Module::Compose, 0, compose::Auto)) != 0;
    const float energy = req.energy;

    // ---------------------------------------------------------------- harmony: tempo, key, scale, the low end's owner
    Rng hr = streamOf(seed, cur, unit, "harmony");
    const float uBpm = hr.uniform(), uScale = hr.uniform(), uMono = hr.uniform(), uLow = hr.uniform();
    const int uKey = hr.below(12);
    float bpm = req.bpm > 0.0f ? req.bpm
              : autoOn ? std::round(2.0f * (prof.bpmLow + uBpm * (prof.bpmHigh - prof.bpmLow))) * 0.5f
                       : p.get(pid(Module::Compose, 0, compose::Bpm));
    const int key = req.key >= 0 ? req.key : autoOn ? uKey : p.getInt(pid(Module::Compose, 0, compose::Key));
    int scale = p.getInt(pid(Module::Compose, 0, compose::Scale));
    if (req.scale >= 0) scale = req.scale;
    else if (autoOn) {
        // Dok. 8.6: Aeolian 0.6, Dorian 0.15, the Aeolian-Dorian hexachord 0.1, Phrygian 0.1, the minor pentatonic 0.05.
        scale = uScale < 0.6f ? static_cast<int>(Scale::Aeolian) : uScale < 0.75f ? static_cast<int>(Scale::Dorian)
              : uScale < 0.85f ? static_cast<int>(Scale::Hexachord) : uScale < 0.95f ? static_cast<int>(Scale::Phrygian)
              : static_cast<int>(Scale::MinorPentatonic);
    }
    const bool monotonic = autoOn && req.key < 0 && uMono < 0.08f;
    const bool subOwns = req.lowOwner >= 0 ? req.lowOwner == 1
                       : autoOn ? uLow < prof.subChance
                                : p.getInt(pid(Module::Compose, 0, compose::LowOwner)) == static_cast<int>(LowOwner::Sub);

    // ---------------------------------------------------------------- form
    Rng fr = streamOf(seed, cur, unit, "form");
    const float uForm = fr.uniform(), uIntro = fr.uniform(), uRed = fr.uniform(), uRedLen = fr.uniform(), uRedPos = fr.uniform();
    const float uRumble = fr.uniform(), uEdges = fr.uniform();
    FormType form = req.form;
    if (form == FormType::Count) {
        const int knob = p.getInt(pid(Module::Compose, 0, compose::Form));
        if (knob > 0) form = static_cast<FormType>(knob - 1);
        else {
            // The set's energy leans towards the Peak at its height and away from it at the ends.
            float wA = prof.arcWeight, wP = prof.peakWeight, wE = prof.endlessWeight;
            if (energy >= 0.0f) wP *= 0.5f + energy;
            const float u = uForm * (wA + wP + wE);
            form = u < wA ? FormType::Arc : u < wA + wP ? FormType::Peak : FormType::Endless;
        }
    }
    if (req.mixable && form == FormType::Endless) form = FormType::Arc;
    const int blocks = req.blocks > 0 ? req.blocks
                     : std::clamp(static_cast<int>(std::lround(p.get(pid(Module::Compose, 0, compose::Minutes)) * bpm / 128.0)), 4, 12);
    const int bars = blocks * 32;
    int introBlocks = form == FormType::Endless ? 0 : (uIntro < prof.intro64Chance && blocks >= 8 ? 2 : 1);
    int outroBlocks = introBlocks;
    while (introBlocks > 1 && blocks - introBlocks - outroBlocks < 3) { --introBlocks; --outroBlocks; }
    const int bodyFirst = introBlocks, bodyEnd = blocks - outroBlocks;   // body blocks [bodyFirst, bodyEnd)
    const int bodyBars = (bodyEnd - bodyFirst) * 32;
    // The reduction: its length and its return on a 16- or 32-bar line at 50 .. 65 % of the body.
    int redLen = 0;
    if (form == FormType::Peak) redLen = uRedLen < 0.5f ? 8 : uRedLen < 0.85f ? 16 : 32;
    else if (form == FormType::Arc && uRed < prof.toolReductionChance) redLen = uRedLen < 0.5f ? 4 : 8;
    redLen = std::min(redLen, prof.maxReduction);
    int redReturn = -1, redStart = -1;
    if (redLen > 0 && bodyBars >= 64) {
        const double at = bodyFirst * 32 + (0.5 + 0.15 * uRedPos) * bodyBars;
        const int line = redLen >= 32 ? 32 : 16;
        redReturn = static_cast<int>(std::lround(at / line)) * line;
        redReturn = std::clamp(redReturn, bodyFirst * 32 + std::max(redLen, 32), bodyEnd * 32 - 32);
        redStart = redReturn - redLen;
    } else {
        redLen = 0;
    }
    // The density profile per block (PLAN 7.2).
    std::vector<float> density(static_cast<size_t>(blocks), 0.85f);
    for (int b = 0; b < blocks; ++b) {
        float d;
        if (form == FormType::Endless) d = (b % 3 == 2) ? 1.0f : 0.85f;
        else if (b < bodyFirst) d = b == 0 ? 0.3f : 0.45f;
        else if (b >= bodyEnd) d = (b == blocks - 1) ? 0.3f : (outroBlocks == 2 ? 0.6f : 0.45f);
        else {
            const int i = b - bodyFirst;
            d = std::min(1.0f, 0.6f + 0.25f * static_cast<float>(i));   // full to the body's end; the outro falls
            if (form == FormType::Peak && redReturn >= 0 && b * 32 >= redReturn && b * 32 < redReturn + 64) d = 1.0f;
        }
        if (energy >= 0.0f && b >= bodyFirst && b < bodyEnd) d = std::clamp(d * (0.85f + 0.3f * energy), 0.3f, 1.0f);
        density[static_cast<size_t>(b)] = d;
    }

    // ---------------------------------------------------------------- layers: the pool and the order of entry
    Rng lr = streamOf(seed, cur, unit, "layers");
    std::vector<LayerId> pool;
    std::vector<float> keep;   // how firmly a layer stays when the pool must shrink: its chance, scattered
    for (const LayerChance& c : prof.pool) {
        const float u = lr.uniform(), v = lr.uniform();   // drawn for every layer, so one chance moves no other draw
        if (u >= c.chance) continue;
        if (monotonic && (c.layer == LayerId::Ping || c.layer == LayerId::Chord || c.layer == LayerId::Drone || c.layer == LayerId::Acid)) continue;
        pool.push_back(c.layer);
        keep.push_back(c.chance + 0.25f * v);
    }
    if (subOwns) { pool.push_back(LayerId::Bass); keep.push_back(10.0f); }
    // One operation a block: the pool is as large as there are blocks to bring it in -- the intro's perc (and hat), one a
    // block of the body; the Endless starts with its tops and brings the rest one a block. The likeliest layers (a
    // style's signature) stay.
    {
        const int bodyBlocks = bodyEnd - bodyFirst;
        const int slots = form == FormType::Endless ? 3 + (subOwns ? 1 : 0) + blocks - 2
                                                    : 1 + (introBlocks == 2 ? 1 : 0) + bodyBlocks;
        const size_t maxPool = static_cast<size_t>(std::max(3, slots));
        while (pool.size() > maxPool) {
            size_t weakest = 0;
            for (size_t i = 1; i < pool.size(); ++i) if (keep[i] < keep[weakest]) weakest = i;
            pool.erase(pool.begin() + static_cast<long>(weakest));
            keep.erase(keep.begin() + static_cast<long>(weakest));
        }
    }
    // The order: by group, shuffled within it. (The rolling hat's other half comes with the body's first bar.)
    std::vector<LayerId> order = pool;
    for (int i = static_cast<int>(order.size()) - 1; i > 0; --i) std::swap(order[static_cast<size_t>(i)], order[static_cast<size_t>(lr.below(i + 1))]);
    std::stable_sort(order.begin(), order.end(), [](LayerId a, LayerId b) { return groupOf(a) < groupOf(b); });
    // The intro's one perc: the first of the clap-and-perc group that is no ghost.
    LayerId introPerc = LayerId::Count;
    for (LayerId id : order) if (groupOf(id) == 2 && id != LayerId::GhostKick) { introPerc = id; break; }

    // ---------------------------------------------------------------- rack
    Rng rr = streamOf(seed, cur, unit, "rack");
    RackSettings rs;
    rs.keyRoot = key;
    rs.scale = scale;
    rs.swing = prof.swingLow + rr.uniform() * (prof.swingHigh - prof.swingLow);
    rs.humanizeMs = p.get(pid(Module::Compose, 0, compose::Humanize));
    rs.mutation = prof.mutation;
    rs.motionScale = prof.motionScale;
    rs.reroll = prof.reroll;
    rs.polymeterChance = prof.polymeterChance;
    rs.fillChance = prof.fillChance;
    RackPlan plan = makeRackPlan(rs, streamSeed(seed, cur, unit, "rack"));
    for (int l = 0; l < kNumLayers; ++l) {
        const int n = cur != nullptr ? cur->count(unit + "rack." + kLayerUnit[l]) : 0;
        if (n > 0) plan.layerSeed[l] = mixSeed(mixSeed(plan.seed, 0x4C41594552ull + static_cast<uint64_t>(l)), static_cast<uint64_t>(n));
    }
    if (cur != nullptr && cur->count(unit + "rack.clap") > 0) plan.layerSeed[L(LayerId::ClapB)] = plan.layerSeed[L(LayerId::ClapA)];
    if (monotonic) plan.bassSize = 1;
    limitHarmony(plan, pool, subOwns);
    int bands[kNumParts];
    partBands(plan, bands);

    // ---------------------------------------------------------------- sounds
    // A factory preset per synth (Presets.h), drawn by the fit of its group to the profile's style mix -- a lane of the kit
    // among the presets made for its role -- then the style's mix (its recipe), then its ranges: a knob a preset sets is
    // held inside the style's range, a knob no preset sets is drawn in it. Every knob the composer manages is set, at the
    // track's start, as an absolute value (Score::knobs): the knobs show them while the track plays (Engine.h).
    Rng sr = streamOf(seed, cur, unit, "sounds");
    TrackKnobs knobs{ p, {} };
    std::vector<SoundPick> picks;
    const bool pickSounds = p.getBool(pid(Module::Compose, 0, compose::PickSounds));
    static const Module kSynths[] = { Module::Kick, Module::Rumble, Module::Sub, Module::Ping, Module::Bass, Module::Acid, Module::Chord,
                                      Module::Drone, Module::Texture, Module::Perc };
    if (pickSounds) {
        for (Module m : kSynths) {
            const int instances = m == Module::Perc ? kPercLanes : 1;
            for (int inst = 0; inst < instances; ++inst) {
                const int role = m == Module::Perc ? p.getInt(pid(Module::Perc, inst, perc::Role)) : -1;
                const int index = pickPreset(m, prof.styleMix, role, sr);
                if (index < 0) continue;
                for (const auto& [k, v] : presetKnobs(m, inst, factoryPresets(m)[static_cast<size_t>(index)])) knobs.value[pid(m, inst, k)] = v;
                picks.push_back(SoundPick{ 0.0, static_cast<int>(m), inst, index });
            }
        }
    }
    for (const SoundValue& v : prof.recipe) { const int id = p.find(v.key); if (id >= 0) knobs.value[id] = v.value; }
    for (const SoundRange& r : prof.sounds) {
        const float u = sr.uniform();
        const int id = p.find(r.key);
        if (id < 0) continue;
        const float lo = std::max(r.low, p.desc(id).minValue), hi = std::min(r.high, p.desc(id).maxValue);
        const auto set = knobs.value.find(id);
        if (set != knobs.value.end()) { set->second = std::clamp(set->second, lo, hi); continue; }   // a preset's: held in range
        const float nl = p.toNormalised(id, lo), nh = p.toNormalised(id, hi);
        knobs.value[id] = p.fromNormalised(id, nl + u * (nh - nl));
    }
    knobs.value[pid(Module::Compose, 0, compose::Key)] = static_cast<float>(key);
    knobs.value[pid(Module::Compose, 0, compose::Scale)] = static_cast<float>(scale);
    knobs.value[pid(Module::Compose, 0, compose::LowOwner)] = subOwns ? 1.0f : 0.0f;
    // Every knob any style sets, set by every track -- its own value or the knob as it stands -- so no track on a deck
    // plays on with the last one's, and the knobs always show the whole of what plays.
    for (const int id : managedKnobs(p)) knobs.value.emplace(id, p.get(id));

    Score sc;
    sc.clear(bpm);
    sc.seed = seed;
    sc.keyRoot = key;
    sc.scale = scale;
    for (const auto& kv : knobs.value) {
        const Module m = p.moduleOf(kv.first);
        const bool sound = hasPresets(m) && !presetLeaves(m, p.indexOf(kv.first));
        sc.knobs.push_back(KnobSet{ 0.0, kv.first, kv.second, sound ? 0 : 1 });
    }
    sc.sounds = picks;

    // ---------------------------------------------------------------- events on the body's 8-bar lines
    Rng er = streamOf(seed, cur, unit, "events");
    std::vector<BarMods> mods(static_cast<size_t>(bars));
    struct Throw { int bar; };
    std::vector<Throw> throws;
    std::vector<int> hpSweeps;   // bars of the 32-line a group high-pass sweep leads into
    const auto inReduction = [&](int bar) { return redLen > 0 && bar >= redStart && bar < redReturn; };
    for (int line = bodyFirst * 32 + 8; line < bodyEnd * 32; line += 8) {
        const float u = er.uniform(), kind = er.uniform(), where = er.uniform();
        if (u >= prof.eventRate || inReduction(line - 1) || inReduction(line) || line == redStart || line == redReturn) continue;
        const int last = line - 1;   // the phrase's last bar
        if (kind < prof.throwShare) throws.push_back({ last });
        else if (form == FormType::Endless || kind < prof.throwShare + (1.0f - prof.throwShare) * 0.35f)
            mods[static_cast<size_t>(last)].muteStep = where < 0.5f ? 14 : 12;
        else if ((line % 16) == 0 && kind < prof.throwShare + (1.0f - prof.throwShare) * 0.55f)
            mods[static_cast<size_t>(last)].dropout = true;
        else
            mods[static_cast<size_t>(last)].ghostBoost = 1.6f;
    }
    // A sweep of the group high pass into a 32-bar line of the body (p 0.3) and into every return.
    for (int b = bodyFirst + 1; b < bodyEnd; ++b)
        if (er.uniform() < 0.3f && !inReduction(b * 32 - 1) && b * 32 != redReturn) hpSweeps.push_back(b * 32);
    if (redReturn >= 0 && redLen >= 8) hpSweeps.push_back(redReturn);
    // "Mute one expected hit" before the body's first change.
    if (bodyFirst > 0) mods[static_cast<size_t>(bodyFirst * 32 - 1)].muteStep = 14;
    // The cut before a return: the Peak's bar of nothing but the tails (p 0.5), else a missing hit.
    const bool redCut = form == FormType::Peak && er.uniform() < 0.5f;
    if (redReturn >= 0 && !redCut) mods[static_cast<size_t>(redReturn - 1)].muteStep = 14;

    // ---------------------------------------------------------------- blocks, their operation and their candidates
    const uint64_t blocksSeed = streamSeed(seed, cur, unit, "blocks");
    LayerState st;
    st.active[L(LayerId::Kick)] = true;
    std::vector<LayerId> queue;   // what may still enter, in order
    for (LayerId id : order) if (id != introPerc || form == FormType::Endless) queue.push_back(id);
    std::vector<LayerId> entered = { LayerId::Kick };
    std::vector<int> entryBar(kNumLayers, -1);
    entryBar[L(LayerId::Kick)] = 0;
    if (form == FormType::Endless) {
        // Kick, bass and tops from bar 1 (Erg. 8).
        for (LayerId id : { LayerId::ClosedHat, LayerId::RollingHat }) { st.active[L(id)] = true; entryBar[L(id)] = 0; entered.push_back(id); }
        int tops = 0;
        // With the style's signature: the tonal layer it is likeliest to have (Dub's chord, Hypnotic's ping).
        LayerId signature = LayerId::Count;
        float most = 0.0f;
        for (const LayerChance& c : prof.pool)
            if ((c.layer == LayerId::Ping || c.layer == LayerId::Chord || c.layer == LayerId::Acid || c.layer == LayerId::Drone) && c.chance > most
                && std::find(queue.begin(), queue.end(), c.layer) != queue.end()) { most = c.chance; signature = c.layer; }
        for (auto it = queue.begin(); it != queue.end();) {
            const int g = groupOf(*it);
            const bool take = *it == LayerId::RollingHat || g == 0 || (g == 1 && *it == LayerId::OpenHat) || (g == 2 && tops < 2 && *it != LayerId::GhostKick)
                           || *it == signature;
            if (!take) { ++it; continue; }
            if (g == 2) ++tops;
            if (*it != LayerId::RollingHat) { st.active[L(*it)] = true; entryBar[L(*it)] = 0; entered.push_back(*it); }
            it = queue.erase(it);
        }
    }
    const int poolSize = static_cast<int>(pool.size()) + 2;
    const auto targetCount = [&](int b) {
        return std::clamp(static_cast<int>(std::lround(density[static_cast<size_t>(b)] * poolSize)), 2, prof.densityCap);
    };
    const int half = bars / 2;
    std::vector<int> chosenVariant(static_cast<size_t>(blocks), 0);
    // The operations' choices come from the layers' stream, not from the candidates: a block drawn again changes its own
    // bars only, never what enters later.
    std::vector<size_t> opChoice(static_cast<size_t>(blocks));
    std::vector<bool> swapChoice(static_cast<size_t>(blocks));
    for (int b = 0; b < blocks; ++b) { opChoice[static_cast<size_t>(b)] = static_cast<size_t>(lr.below(2)); swapChoice[static_cast<size_t>(b)] = lr.uniform() < 0.5f; }

    // Removes the latest-entered active layer that @p pick accepts (never kick, offbeat hat, rolling hat).
    const auto removeLatest = [&](LayerState& s, std::vector<LayerId>& ent, bool tonalOnly, LayerId* out) {
        for (auto it = ent.rbegin(); it != ent.rend(); ++it) {
            const LayerId id = *it;
            if (id == LayerId::Kick || id == LayerId::ClosedHat || id == LayerId::RollingHat || !s.active[L(id)]) continue;
            if (tonalOnly && !isTonal(id)) continue;
            s.active[L(id)] = false;
            if (out) *out = id;
            return true;
        }
        return false;
    };

    for (int b = 0; b < blocks; ++b) {
        const bool intro = b < bodyFirst, outro = b >= bodyEnd;
        // The candidates' variants: the block's own stream, so one block drawn again moves no other.
        Rng vr;
        vr.seed(mixSeed(mixSeed(blocksSeed, static_cast<uint64_t>(b)), static_cast<uint64_t>(cur ? cur->count(unit + "block" + std::to_string(b + 1)) : 0)));
        struct Cand {
            float dist = 1e30f;
            std::vector<NoteEvent> notes;
            LayerState after;
            std::vector<LayerId> queue, entered;
            std::vector<BlockOp> ops;
            std::vector<int> entryBar;
            uint32_t variant = 0;
        };
        Cand best;
        for (int c = 0; c < kCandidates; ++c) {
            Cand k;
            k.variant = static_cast<uint32_t>(vr.next() >> 33) | 1u;
            LayerState s = st;
            std::vector<LayerId> q = queue, ent = entered;
            std::vector<int> eb = entryBar;
            std::vector<BlockOp> ops;
            const auto op = [&](int bar, OpKind kind, LayerId layer, LayerId other = LayerId::Count) {
                BlockOp o;
                o.beat = static_cast<double>(bar) * kBar;
                o.kind = kind;
                o.layer = layer == LayerId::Count ? -1 : L(layer);
                o.other = other == LayerId::Count ? -1 : L(other);
                ops.push_back(o);
            };
            const auto enter = [&](int bar, LayerId id, bool logOp) {
                if (id == LayerId::RollingHat) s.density[L(id)] = 1.0f;
                else s.active[L(id)] = true;
                if (eb[L(id)] < 0 || id == LayerId::RollingHat) eb[L(id)] = bar;
                ent.push_back(id);
                if (logOp) op(bar, OpKind::Add, id);
            };
            // --- the block's operation, at its first bar ---
            const int first = b * 32;
            if (b == 0) {
                op(first, OpKind::Start, LayerId::Kick);
            } else if (intro) {
                // The second intro block brings the first of the hats' group.
                LayerId add = LayerId::Count;
                for (auto it = q.begin(); it != q.end(); ++it) if (groupOf(*it) == 1 && *it != LayerId::RollingHat) { add = *it; q.erase(it); break; }
                if (add != LayerId::Count) enter(first, add, true);
                else op(first, OpKind::Hold, LayerId::Count);
            } else if (outro) {
                if (b == blocks - 1) {
                    bool any = false;
                    for (int i = 1; i < kNumLayers; ++i) {
                        const LayerId id = static_cast<LayerId>(i);
                        if (id == LayerId::ClosedHat || id == LayerId::RollingHat || !s.active[i]) continue;
                        if (!any) op(first, OpKind::Remove, id);
                        s.active[i] = false;
                        any = true;
                    }
                    if (!any) op(first, OpKind::Hold, LayerId::Count);
                    s.density[L(LayerId::RollingHat)] = 0.5f;
                }
                // The other outro blocks remove one every eight bars, below.
            } else {
                const int now = s.count(), want = targetCount(b);
                // What may enter now: the sub bass with the body (a big change on a 32-bar line); else in order, the
                // next two if they are of one group (the candidates try both), the last two not before the half.
                std::vector<size_t> can;
                const auto bass = std::find(q.begin(), q.end(), LayerId::Bass);
                const bool bassNow = bass != q.end() && subOwns;
                if (bassNow) can.push_back(static_cast<size_t>(bass - q.begin()));
                for (size_t i = 0; !bassNow && i < q.size() && can.size() < 2; ++i) {
                    if (q.size() <= 2 && first < half) break;
                    if (!can.empty() && groupOf(q[i]) != groupOf(q[can[0]])) break;
                    can.push_back(i);
                }
                if ((now < want || bassNow) && !can.empty()) {
                    const size_t pick = can[opChoice[static_cast<size_t>(b)] % can.size()];
                    const LayerId add = q[pick];
                    q.erase(q.begin() + static_cast<long>(pick));
                    enter(first, add, true);
                } else if (now > want) {
                    LayerId gone = LayerId::Count;
                    if (removeLatest(s, ent, false, &gone)) op(first, OpKind::Remove, gone);
                    else op(first, OpKind::Hold, LayerId::Count);
                } else {
                    // A swap within the clap-and-perc group, or a hold.
                    LayerId out = LayerId::Count, in = LayerId::Count;
                    if (swapChoice[static_cast<size_t>(b)]) {
                        for (LayerId id : ent) if (groupOf(id) == 2 && s.active[L(id)]) out = id;
                        for (size_t i = 0; i < q.size(); ++i) if (groupOf(q[i]) == 2) { in = q[i]; break; }
                    }
                    if (out != LayerId::Count && in != LayerId::Count && (q.size() > 2 || first >= half)) {
                        s.active[L(out)] = false;
                        q.erase(std::find(q.begin(), q.end(), in));
                        q.push_back(out);
                        enter(first, in, false);
                        op(first, OpKind::Swap, in, out);
                    } else {
                        op(first, OpKind::Hold, LayerId::Count);
                    }
                }
            }
            // --- the bars ---
            if (b == bodyFirst && form != FormType::Endless) s.density[L(LayerId::RollingHat)] = 1.0f;   // the full groove
            std::vector<NoteEvent> notes;
            for (int bar = first; bar < first + 32; ++bar) {
                const int in = bar - first;
                if (b == 0 && form != FormType::Endless) {
                    if (bar == 8) enter(bar, LayerId::ClosedHat, true);
                    if (bar == 16) { s.active[L(LayerId::RollingHat)] = true; s.density[L(LayerId::RollingHat)] = 0.5f; eb[L(LayerId::RollingHat)] = bar; op(bar, OpKind::Add, LayerId::RollingHat); }
                    if (bar == 24 && introPerc != LayerId::Count) enter(bar, introPerc, true);
                }
                if (outro && b < blocks - 1 && in % 8 == 0) {
                    LayerId gone = LayerId::Count;
                    if (removeLatest(s, ent, true, &gone) || removeLatest(s, ent, false, &gone)) op(bar, OpKind::Remove, gone);
                    else if (in == 0) op(bar, OpKind::Hold, LayerId::Count);
                }
                if (form != FormType::Endless && bar == bars - 16) { s.active[L(LayerId::RollingHat)] = false; op(bar, OpKind::Remove, LayerId::RollingHat); }
                BarSpec spec = emptyBar(bar, static_cast<double>(bar) * kBar, bpm);
                for (int i = 0; i < kNumLayers; ++i) { spec.active[i] = s.active[i]; spec.density[i] = s.density[i]; }
                spec.variant = k.variant;
                const BarMods& m = mods[static_cast<size_t>(bar)];
                if (m.muteStep >= 0) spec.muteStep[m.muteStep] = true;
                if (m.ghostBoost > 1.0f) {
                    spec.density[L(LayerId::GhostKick)] *= m.ghostBoost;
                    spec.density[L(LayerId::ClapGhost)] *= m.ghostBoost;
                }
                const bool kickOut = m.dropout || inReduction(bar);
                if (kickOut) {
                    for (LayerId id : { LayerId::Kick, LayerId::GhostKick, LayerId::Bass, LayerId::Acid }) spec.active[L(id)] = false;
                    if (inReduction(bar) && form == FormType::Peak) {
                        spec.active[L(LayerId::RollingHat)] = false;
                        for (float& d : spec.density) d *= 0.6f;
                    }
                }
                if (redReturn >= 0 && redCut && bar == redReturn - 1) for (bool& a : spec.active) a = false;
                if (inReduction(bar)) spec.fills = false;
                realizeBar(plan, spec, notes);
            }
            // --- the corridor ---
            std::vector<NoteEvent> local = notes;
            for (NoteEvent& n : local) n.beat -= static_cast<double>(first) * kBar;
            const std::vector<BarProfile> prof32 = barProfiles(local, 0, 32, bands);
            const CorridorStats cs = corridorOf(prof32, 0, 32);
            const float ds = (cs.similarity - prof.simTarget) / 0.04f, dm = (cs.micro - prof.microTarget) / 0.6f;
            k.dist = ds * ds + dm * dm;
            if (k.dist < best.dist) {
                k.notes = std::move(notes);
                k.after = s;
                k.queue = q;
                k.entered = ent;
                k.ops = ops;
                k.entryBar = eb;
                best = std::move(k);
            }
        }
        sc.notes.insert(sc.notes.end(), best.notes.begin(), best.notes.end());
        sc.ops.insert(sc.ops.end(), best.ops.begin(), best.ops.end());
        st = best.after;
        queue = best.queue;
        entered = best.entered;
        entryBar = best.entryBar;
        chosenVariant[static_cast<size_t>(b)] = static_cast<int>(best.variant);
    }
    {
        BlockOp end;
        end.beat = static_cast<double>(bars) * kBar;
        end.kind = OpKind::End;
        sc.ops.push_back(end);
    }
    const auto has = [&](LayerId id) { return entryBar[L(id)] >= 0; };
    const auto entryBeat = [&](LayerId id) { return static_cast<double>(std::max(0, entryBar[L(id)])) * kBar; };

    // ---------------------------------------------------------------- the reduction's swell, cloud, markers
    const int noiseLevel = pid(Module::Perc, 11, perc::Level), noiseCut = pid(Module::Perc, 11, perc::Cutoff);
    if (redLen > 0) {
        BlockOp ko;
        ko.beat = static_cast<double>(redStart) * kBar;
        ko.kind = OpKind::KickOut;
        ko.layer = L(LayerId::Kick);
        sc.ops.push_back(ko);
        BlockOp re = ko;
        re.beat = static_cast<double>(redReturn) * kBar;
        re.kind = OpKind::Return;
        sc.ops.push_back(re);
        sc.markers.push_back(Marker{ static_cast<double>(redStart) * kBar, "Reduction" });
        sc.markers.push_back(Marker{ static_cast<double>(redReturn) * kBar, "Return" });
        // The swell of noise over the last 8 or 16 bars before the return (the cut bar excepted).
        const int swell = std::min(redLen, redLen >= 16 ? 16 : 8);
        const int from = redReturn - swell;
        for (int bar = from; bar < redReturn - (redCut ? 1 : 0); ++bar) {
            NoteEvent n;
            n.beat = static_cast<double>(bar) * kBar;
            n.length = 4.0;
            n.part = percPart(11);
            n.pitch = 49;
            n.velocity = 0.9f;
            sc.notes.push_back(n);
        }
        sc.gestures.push_back(knobs.ramp(noiseLevel, static_cast<double>(from) * kBar, static_cast<double>(swell - 1) * kBar, -36.0f, -12.0f, GestureShape::EaseIn, 0));
        sc.gestures.push_back(knobs.ramp(noiseCut, static_cast<double>(from) * kBar, static_cast<double>(swell - 1) * kBar, 600.0f, 6000.0f, GestureShape::EaseIn, 1));
        sc.gestures.push_back(knobs.home(noiseLevel, static_cast<double>(redReturn) * kBar));
        sc.gestures.push_back(knobs.home(noiseCut, static_cast<double>(redReturn) * kBar));
        // The cloud of the ping's and the chord's last seconds rises through a reduction of eight bars or more.
        const int cloudLevel = pid(Module::Cloud, 0, cloud::Level);
        if (redLen >= 8 && ((has(LayerId::Ping) && entryBar[L(LayerId::Ping)] < redStart) || (has(LayerId::Chord) && entryBar[L(LayerId::Chord)] < redStart))) {
            sc.gestures.push_back(knobs.ramp(cloudLevel, static_cast<double>(redStart) * kBar, static_cast<double>(redLen) * kBar, -60.0f, -3.0f, GestureShape::EaseIn, 0));
            sc.gestures.push_back(knobs.ramp(cloudLevel, static_cast<double>(redReturn) * kBar, 32.0, -3.0f, -60.0f, GestureShape::EaseOut, 0));
            sc.gestures.push_back(knobs.home(cloudLevel, static_cast<double>(redReturn + 8) * kBar));
        }
    }
    for (int b = 0; b < blocks; ++b) {
        const bool intro = b < bodyFirst, outro = b >= bodyEnd;
        sc.markers.push_back(Marker{ static_cast<double>(b) * 32.0 * kBar,
                                     intro ? (b == 0 ? std::string("Intro") : std::string("Intro 2")) : outro ? std::string("Outro")
                                           : "Block " + std::to_string(b - bodyFirst + 1) });
    }

    // ---------------------------------------------------------------- automation
    const int hatsCut = pid(Module::Mix, 0, mix::HatsCut);
    const int lowCut = pid(Module::Mix, 0, mix::LowCut);
    const double introEnd = static_cast<double>(bodyFirst) * 32.0 * kBar, outroStart = static_cast<double>(bodyEnd) * 32.0 * kBar;
    // Intro: the hats' bus opens from 1.5 kHz (Dok. 8.5: "Perc-Bus LP 800 Hz -> offen"); outro: it closes in the last 16 bars.
    if (form != FormType::Endless) {
        sc.gestures.push_back(knobs.ramp(hatsCut, 8.0 * kBar, introEnd - 8.0 * kBar, 1500.0f, knobs.get(hatsCut), GestureShape::Linear, 0));
        sc.gestures.push_back(knobs.ramp(hatsCut, static_cast<double>(bars - 16) * kBar, 16.0 * kBar, knobs.get(hatsCut), 2500.0f, GestureShape::Linear, 0));
    }
    // The rumble with the body (p 0.7; PLAN 7.2's intro is "Kick (+Rumble)"): out in the intro and the outro, in on the
    // body's first bar -- the low end arrives where a set swaps it, and the edges of a track are lighter, as the
    // references' are (their outros 4 to 19 dB under the body, ours 1 to 4 with the rumble through, 27.09.2026).
    if (form != FormType::Endless && uRumble < 0.7f) {
        const int rl = pid(Module::Rumble, 0, rumble::Level);
        sc.gestures.push_back(knobs.step(rl, 0.0, -60.0f));
        sc.gestures.push_back(knobs.step(rl, introEnd, knobs.get(rl)));
        sc.gestures.push_back(knobs.step(rl, outroStart, -60.0f));
    }
    // Macro: the rumble's hall and the stab's brightness grow by a fifth over the track (Dok. 8.5, "Track-Drift").
    {
        const int rd = pid(Module::Rumble, 0, rumble::Decay), cb = pid(Module::Chord, 0, chord::Bright);
        sc.gestures.push_back(knobs.ramp(rd, 0.0, static_cast<double>(bars) * kBar, knobs.get(rd), knobs.get(rd) * 1.2f, GestureShape::Linear, 1));
        if (has(LayerId::Chord))
            sc.gestures.push_back(knobs.ramp(cb, entryBeat(LayerId::Chord), static_cast<double>(bars) * kBar - entryBeat(LayerId::Chord),
                                             knobs.get(cb), knobs.get(cb) * 1.2f, GestureShape::Linear, 1));
    }
    // Micro: the rolling hat's decay moves a few per cent every 16 bars of the body; the open hat's grows from 100 to
    // 400 ms over the 16 bars after it enters; a chord enters 9 dB under its level and steps up every four bars.
    Rng mr = streamOf(seed, cur, unit, "hands");
    {
        const int rollDecay = pid(Module::Perc, 1, perc::NoiseDecay), ohDecay = pid(Module::Perc, 2, perc::NoiseDecay);
        const float d0 = knobs.get(rollDecay);
        for (int bar = bodyFirst * 32; bar < bodyEnd * 32; bar += 16) {
            const float a = d0 * (1.0f + 0.08f * mr.bipolar()), z = d0 * (1.0f + 0.08f * mr.bipolar());
            sc.gestures.push_back(knobs.ramp(rollDecay, static_cast<double>(bar) * kBar, 16.0 * kBar, a, z, GestureShape::MinimumJerk, 1));
        }
        if (has(LayerId::OpenHat))
            sc.gestures.push_back(knobs.ramp(ohDecay, entryBeat(LayerId::OpenHat), 16.0 * kBar, 100.0f, 400.0f, GestureShape::Linear, 1));
        if (has(LayerId::Chord)) {
            const int cl = pid(Module::Chord, 0, chord::Level);
            const float k = knobs.get(cl);
            for (int s = 0; s < 4; ++s) {
                const float v = k - 9.0f + 3.0f * static_cast<float>(s);
                sc.gestures.push_back(knobs.step(cl, entryBeat(LayerId::Chord) + 16.0 * s, v));
            }
        }
    }
    // The throws: from the third beat of the phrase's last bar a ping, a stab or the hats open into the echo, the
    // feedback climbs to the edge and falls back over two bars.
    {
        const int fbId = pid(Module::Dub, 0, dub::Feedback), pingEcho = pid(Module::Dub, 0, dub::PingSend);
        const int hatsEcho = pid(Module::Dub, 0, dub::HatsSend), chordDub = pid(Module::Chord, 0, chord::DubSend);
        for (const Throw& t : throws) {
            const double at = static_cast<double>(t.bar) * kBar + 2.0;
            const float u = er.uniform();
            const bool ping = has(LayerId::Ping) && entryBar[L(LayerId::Ping)] <= t.bar && u < 0.5f;
            const bool stab = !ping && has(LayerId::Chord) && entryBar[L(LayerId::Chord)] <= t.bar && u < 0.8f;
            const float fb = knobs.get(fbId);
            sc.gestures.push_back(knobs.ramp(fbId, at, 2.0, fb, 0.85f, GestureShape::EaseIn, 0));
            sc.gestures.push_back(knobs.ramp(fbId, at + 2.0, 8.0, 0.85f, fb, GestureShape::EaseOut, 0));
            sc.gestures.push_back(knobs.home(fbId, at + 10.0));
            const int send = ping ? pingEcho : stab ? chordDub : hatsEcho;
            const float open = ping || stab ? 0.9f : 0.5f;
            sc.gestures.push_back(knobs.step(send, at, open));
            sc.gestures.push_back(knobs.home(send, at + 2.0));
        }
    }
    // The group high pass: from 20 Hz to 200 .. 400 Hz over the eight bars before the line, back on it.
    for (int line : hpSweeps) {
        const float top = 200.0f + 200.0f * er.uniform();
        sc.gestures.push_back(knobs.ramp(lowCut, static_cast<double>(line - 8) * kBar, 8.0 * kBar, 20.0f, top, GestureShape::EaseIn, 1));
        sc.gestures.push_back(knobs.home(lowCut, static_cast<double>(line) * kBar));
    }
    // The edges filtered (the profile's chance; Dok. 8.5's group high pass): the intro opens from 250 Hz over its first 16 bars, the
    // last 16 bars close to 300 Hz. A track's loudness moves at its edges as the references' do (their LRA 3 to 5 LU, ours
    // 1 to 2 with the kick at full from the first bar to the last, 27.09.2026).
    if (form != FormType::Endless && uEdges < prof.edgeChance) {
        sc.gestures.push_back(knobs.ramp(lowCut, 0.0, 16.0 * kBar, 250.0f, 20.0f, GestureShape::EaseIn, 1));
        sc.gestures.push_back(knobs.ramp(lowCut, static_cast<double>(bars - 16) * kBar, 16.0 * kBar, 20.0f, 300.0f, GestureShape::EaseIn, 1));
    }
    // Meso: two hands on a 16-beat grid over the body, on the filters and sends of what plays; their centre follows the
    // density (the energy of the moment).
    {
        std::vector<HandKnob> hk;
        const auto knob = [&](int id, float range, float rest, float peak, float weight, double from) {
            HandKnob h;
            h.param = id;
            const float base = knobs.offset(id, knobs.get(id));
            h.low = base - range;
            h.high = base + range;
            h.atRest = base + rest;
            h.atPeak = base + peak;
            h.scatter = range * 0.35f;
            h.weight = weight;
            h.from = std::max(from, introEnd);
            h.until = outroStart;
            hk.push_back(h);
        };
        knob(pid(Module::Rumble, 0, rumble::Drive), 0.12f, -0.04f, 0.06f, 1.0f, 0.0);
        knob(pid(Module::Mix, 0, mix::PercCut), 0.25f, -0.15f, 0.0f, 1.0f, 0.0);
        knob(pid(Module::Space, 0, space::Level), 0.06f, -0.02f, 0.03f, 0.7f, 0.0);
        knob(pid(Module::Dub, 0, dub::EchoReturn), 0.08f, -0.03f, 0.03f, 0.7f, 0.0);
        if (has(LayerId::Chord)) knob(pid(Module::Chord, 0, chord::Band), 0.2f, -0.05f, 0.1f, 1.5f, entryBeat(LayerId::Chord) + 64.0);
        if (has(LayerId::Acid)) {
            knob(pid(Module::Acid, 0, synth::Cutoff), 0.25f, -0.05f, 0.18f, 2.0f, entryBeat(LayerId::Acid));
            knob(pid(Module::Acid, 0, synth::Resonance), 0.15f, 0.0f, 0.08f, 1.0f, entryBeat(LayerId::Acid));
        }
        if (has(LayerId::Ping)) {
            knob(pid(Module::Ping, 0, ping::Band), 0.15f, -0.05f, 0.08f, 1.2f, entryBeat(LayerId::Ping));
            knob(pid(Module::Ping, 0, ping::Index), 0.15f, -0.05f, 0.08f, 1.0f, entryBeat(LayerId::Ping));
        }
        if (has(LayerId::Drone)) knob(pid(Module::Drone, 0, drone::Cutoff), 0.15f, -0.05f, 0.08f, 0.8f, entryBeat(LayerId::Drone));
        HandStyle hs;
        const auto energyAt = [&](double beat) {
            const int b = std::clamp(static_cast<int>(beat / (32.0 * kBar)), 0, blocks - 1);
            return density[static_cast<size_t>(b)];
        };
        playHands(sc, hk, hs, energyAt, introEnd, outroStart, mr);
    }

    // ---------------------------------------------------------------- the loudest part, the length, what the track tells
    int peakBlock = bodyFirst;
    if (form == FormType::Endless) peakBlock = blocks / 2;
    else if (form == FormType::Peak && redReturn >= 0) {
        // The densest block from the return on: a layer may still enter after it (28.09.2026, a texture's did).
        const int from = std::min(bodyEnd - 1, redReturn / 32 + (redReturn % 32 == 0 ? 0 : 1));
        int most = -1;
        for (int b = from; b < bodyEnd; ++b) if (targetCount(b) >= most) { most = targetCount(b); peakBlock = b; }
    }
    else {
        int most = -1;
        for (int b = bodyFirst; b < bodyEnd; ++b)
            if (targetCount(b) >= most && !(redLen > 0 && b * 32 <= redStart && redStart < b * 32 + 32)) { most = targetCount(b); peakBlock = b; }
    }
    sc.levels.push_back(LevelMark{ 0.0, static_cast<double>(peakBlock) * 32.0 * kBar, prof.peakLufs, 0.0f });
    sc.lengthBeats = static_cast<double>(bars) * kBar;
    sc.sort();

    if (info != nullptr) {
        TrackInfo& t = *info;
        t = TrackInfo{};
        t.form = form;
        t.style = prof.name;
        t.bpm = bpm;
        t.key = key;
        t.scale = scale;
        t.monotonic = monotonic;
        t.subOwns = subOwns;
        t.bars = bars;
        t.introBars = bodyFirst * 32;
        t.outroBar = bodyEnd * 32;
        t.bassBar = bodyFirst * 32;
        if (redLen > 0) { t.reductions.push_back(redStart); t.returns.push_back(redReturn); }
        t.peakBar = peakBlock * 32;
        t.camelot = monotonic ? std::string("monotonic") : camelotOf(key);
        for (LayerId id : entered) if (std::find(t.layers.begin(), t.layers.end(), L(id)) == t.layers.end()) t.layers.push_back(L(id));
        const std::vector<BarProfile> all = barProfiles(sc.notes, 0, bars, bands);
        const CorridorStats cs = corridorOf(all, bodyFirst * 32, bodyEnd * 32);
        t.similarity = cs.similarity;
        t.micro = cs.micro;
        t.density = cs.density;
    }
    return sc;
}

} // namespace umb
