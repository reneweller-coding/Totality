/**
 * @file Composer.cpp
 * @brief One track: form, harmony, rack, layers, candidates, events, hands, sounds (Composer.h).
 */
#include "tot/compose/Composer.h"
#include "tot/Presets.h"
#include "tot/Dsp.h"
#include "tot/compose/Corridor.h"
#include "tot/compose/GestureEngine.h"
#include "tot/pattern/Rack.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace tot {

const char* const FormNames[] = { "Arc", "Peak", "Endless" };
const char* const ArchetypeNames[] = { "Tool", "Roller", "Stab", "Acid", "Bleep", "Dub Chord", "Tribal" };
const char* const OpeningNames[] = { "drums", "percussion first", "atmosphere first" };
const char* const UnitNames[9] = { "form", "harmony", "rack", "layers", "blocks", "events", "hands", "sounds", "figure" };

std::string camelotOf(int key)
{
    const int k = ((key % 12) + 12) % 12;
    return std::to_string(((k * 7) % 12 + 4) % 12 + 1) + "A";
}

namespace {

constexpr int Candidates = 8;   ///< how many variants of a block's form are drawn; the one nearest the plan is taken
constexpr int Bar = 4;   ///< beats per bar
/** The names of the layers in a unit ("rack.clap"). */
const char* const LayerUnit[NumLayers] = { "kick", "ghost", "ch", "rolling", "oh", "ride", "clap", "clapb", "clapghost",
                                             "shaker", "tom", "rim", "bass", "ping", "chord", "drone", "acid", "texture" };

/** @brief The FNV-1a hash of @p s. */
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

/** @brief A random stream seeded for unit @p unit's @p name (streamSeed). */
Rng streamOf(uint64_t seed, const Curation* cur, const std::string& unit, const std::string& name)
{
    Rng r;
    r.seed(streamSeed(seed, cur, unit, name));
    return r;
}

/** @brief The index of layer @p id. */
int L(LayerIdE id) { return static_cast<int>(id); }

/** @brief Whether layer @p id plays notes of the key (bass, 303, chord, drone, texture, ping). */
bool isTonal(LayerIdE id)
{
    return id == LayerIdE::BASS || id == LayerIdE::ACID || id == LayerIdE::CHORD || id == LayerIdE::DRONE
        || id == LayerIdE::TEXTURE || id == LayerIdE::PING;
}

/** @brief Dok. 8.5's order of entry: 0 the sub bass (with the body), 1 hats and ride, 2 clap and perc, 3 the 303 and the
 *         ping, 4 the stab, 5 pad and texture. */
int groupOf(LayerIdE id)
{
    switch (id) {
    case LayerIdE::BASS: return 0;
    case LayerIdE::OPEN_HAT: case LayerIdE::RIDE: case LayerIdE::ROLLING_HAT: return 1;
    case LayerIdE::CLAP_A: case LayerIdE::SHAKER: case LayerIdE::TOM_CONGA: case LayerIdE::RIM: case LayerIdE::GHOST_KICK: return 2;
    case LayerIdE::ACID: case LayerIdE::PING: return 3;
    case LayerIdE::CHORD: return 4;
    default: return 5;
    }
}

/** @brief Phase 13: the knobs an archetype's mix moves (archetypeMix); every track sets them. */
const char* const ArchetypeKeys[] = { "mix.perc_level", "chord.level", "chord.dub_send", "dub.feedback", "acid.level", "ping.level" };

/** @brief Phase 13: the archetypes' weights per style (Hypnotic, Ostgut, Dub, Raw): Tool, Roller, Stab, Acid, Bleep, Dub
 *         Chord, Tribal. */
const float ArchetypeW[4][static_cast<int>(ArchetypeE::COUNT)] = {
    { 0.15f, 0.35f, 0.05f, 0.05f, 0.25f, 0.10f, 0.05f },
    { 0.25f, 0.15f, 0.25f, 0.15f, 0.05f, 0.05f, 0.10f },
    { 0.15f, 0.10f, 0.05f, 0.00f, 0.05f, 0.65f, 0.00f },
    { 0.15f, 0.10f, 0.05f, 0.30f, 0.10f, 0.00f, 0.30f },
};

/** @brief Sets layer @p id's chance in @p prof's pool (adds it where it is missing). */
void setChance(StyleProfile& prof, LayerIdE id, float chance)
{
    for (LayerChance& c : prof.m_pool) if (c.m_layer == id) { c.m_chance = chance; return; }
    prof.m_pool.push_back(LayerChance{ id, chance });
}

/** @brief Phase 13: the profile as the archetype plays it -- its pool, cycles, events and forms (Composer.h). */
void applyArchetype(StyleProfile& prof, ArchetypeE a)
{
    switch (a) {
    case ArchetypeE::TOOL:
        for (LayerIdE id : { LayerIdE::CHORD, LayerIdE::ACID, LayerIdE::PING }) setChance(prof, id, 0.0f);
        prof.m_arcWeight *= 2.0f;
        prof.m_peakWeight *= 0.5f;
        break;
    case ArchetypeE::ROLLER:
        for (LayerIdE id : { LayerIdE::TOM_CONGA, LayerIdE::SHAKER }) setChance(prof, id, 1.0f);
        setChance(prof, LayerIdE::RIM, 0.9f);
        setChance(prof, LayerIdE::GHOST_KICK, 0.9f);
        for (LayerIdE id : { LayerIdE::CHORD, LayerIdE::ACID, LayerIdE::PING }) setChance(prof, id, 0.0f);
        prof.m_polymeterChance = 1.0f;
        prof.m_endlessWeight = std::max(prof.m_endlessWeight, 0.3f) * 1.5f;
        prof.m_peakWeight *= 0.5f;
        break;
    case ArchetypeE::STAB:
        setChance(prof, LayerIdE::CHORD, 1.0f);
        setChance(prof, LayerIdE::ACID, 0.0f);
        setChance(prof, LayerIdE::PING, 0.2f);
        prof.m_throwShare = std::min(0.9f, prof.m_throwShare + 0.2f);
        break;
    case ArchetypeE::ACID:
        setChance(prof, LayerIdE::ACID, 1.0f);
        setChance(prof, LayerIdE::CHORD, 0.1f);
        setChance(prof, LayerIdE::PING, 0.2f);
        prof.m_peakWeight *= 1.3f;
        break;
    case ArchetypeE::BLEEP:
        setChance(prof, LayerIdE::PING, 1.0f);
        setChance(prof, LayerIdE::CHORD, 0.1f);
        setChance(prof, LayerIdE::ACID, 0.0f);
        break;
    case ArchetypeE::DUB_CHORD:
        setChance(prof, LayerIdE::CHORD, 1.0f);
        setChance(prof, LayerIdE::TEXTURE, 0.8f);
        setChance(prof, LayerIdE::DRONE, 0.5f);
        setChance(prof, LayerIdE::ACID, 0.0f);
        prof.m_throwShare = std::min(0.95f, prof.m_throwShare + 0.3f);
        prof.m_eventRate = std::min(0.6f, prof.m_eventRate + 0.1f);
        break;
    case ArchetypeE::TRIBAL:
        for (LayerIdE id : { LayerIdE::TOM_CONGA, LayerIdE::CLAP_A, LayerIdE::RIM }) setChance(prof, id, 1.0f);
        setChance(prof, LayerIdE::SHAKER, 0.8f);
        setChance(prof, LayerIdE::CHORD, 0.0f);
        setChance(prof, LayerIdE::PING, 0.2f);
        setChance(prof, LayerIdE::ACID, 0.1f);
        prof.m_fillChance = std::min(0.6f, prof.m_fillChance + 0.2f);
        prof.m_peakWeight *= 2.0f;
        prof.m_densityCap += 1;
        break;
    default:
        break;
    }
}

/** @brief The knobs the composer sets on every track (its KnobSets): every synth's sound, every knob a style names. */
const std::vector<int>& managedKnobs(const ParamStore& p)
{
    static const std::vector<int> ids = [&p] {
        std::set<int> s;
        for (int id = 0; id < p.count(); ++id) {
            const ModuleE m = p.moduleOf(id);
            if (hasPresets(m) && !presetLeaves(m, p.indexOf(id))) s.insert(id);
        }
        for (int st = 0; st < 4; ++st) {
            const StyleProfile& prof = styleProfile(static_cast<StyleE>(st));
            for (const SoundValue& v : prof.m_recipe) if (const int id = p.find(v.m_key); id >= 0) s.insert(id);
            for (const SoundRange& r : prof.m_sounds) if (const int id = p.find(r.m_key); id >= 0) s.insert(id);
        }
        // Phase 13: what the archetypes lift (archetypeMix).
        for (const char* key : ArchetypeKeys) if (const int id = p.find(key); id >= 0) s.insert(id);
        return std::vector<int>(s.begin(), s.end());
    }();
    return ids;
}

/** @brief The values a track sets on the knobs, and gestures in those values (offsets from the knobs underneath). */
struct TrackKnobs {
    const ParamStore& m_p;   ///< the knobs underneath
    std::map<int, float> m_value;   ///< real units
    /** @brief The track's value of @p id where it sets one, else the knob's. */
    float get(int id) const { const auto it = m_value.find(id); return it == m_value.end() ? m_p.get(id) : it->second; }
    /** @brief A gesture's offset: from the track's own value where it sets the knob (a KnobSet), else from the knob. */
    float offset(int id, float v) const { return m_p.toNormalised(id, v) - m_p.toNormalised(id, get(id)); }
    /**
     * @brief A gesture on @p id from @p beat over @p length beats, from @p from to @p to (real units) in @p shape, by hand @p
     *        hand.
     */
    Gesture ramp(int id, double beat, double length, float from, float to, GestureShapeE shape, uint8_t hand) const
    {
        Gesture g;
        g.m_param = id;
        g.m_beat = beat;
        g.m_length = length;
        g.m_from = offset(id, from);
        g.m_to = offset(id, to);
        g.m_shape = shape;
        g.m_hand = hand;
        return g;
    }
    /** @brief A step of @p id to @p v at @p beat. */
    Gesture step(int id, double beat, float v) const { return ramp(id, beat, 0.0, v, v, GestureShapeE::STEP, 0); }
    /** @brief Back to the track's own value at @p beat. */
    Gesture home(int id, double beat) const { return step(id, beat, get(id)); }
};

/** @brief Which layers play, and how densely. */
struct LayerState {
    bool m_active[NumLayers] = {};   ///< per layer: it plays
    float m_density[NumLayers];   ///< per layer: how densely, 0..1
    LayerState() { for (float& d : m_density) d = 1.0f; }
    /** @brief Layers sounding beside the kick; the rolling hat counts once it plays in full. */
    int count() const
    {
        int n = 0;
        for (int i = 1; i < NumLayers; ++i)
            if (m_active[i] && !(i == L(LayerIdE::ROLLING_HAT) && m_density[i] < 1.0f)) ++n;
        return n;
    }
};

/** @brief What an event does to one bar. */
struct BarMods {
    int m_muteStep = -1;        ///< a step where only the kick plays
    bool m_dropout = false;     ///< the kick (and ghost, bass, 303) out for the bar
    float m_ghostBoost = 1.0f;  ///< a ghost more
    uint32_t m_muteLayers = 0;  ///< Phase 8: whole layers out for the bar (bit LayerId): Mills' mutes, a breath, the centre out
};

/** @brief The bit of layer @p id in a mask. */
uint32_t bitOf(LayerIdE id) { return 1u << static_cast<int>(id); }
/** @brief " for n bars", for the log of what a block does. */
std::string fmtBars(int n) { return " for " + std::to_string(n) + (n == 1 ? " bar" : " bars"); }
/** @brief The centre (The Acid Mind's "removing the center"): everything but the kick and the hats. */
constexpr uint32_t Centre = (1u << static_cast<int>(LayerIdE::CLAP_A)) | (1u << static_cast<int>(LayerIdE::CLAP_B))
                           | (1u << static_cast<int>(LayerIdE::CLAP_GHOST)) | (1u << static_cast<int>(LayerIdE::SHAKER))
                           | (1u << static_cast<int>(LayerIdE::TOM_CONGA)) | (1u << static_cast<int>(LayerIdE::RIM))
                           | (1u << static_cast<int>(LayerIdE::BASS)) | (1u << static_cast<int>(LayerIdE::PING))
                           | (1u << static_cast<int>(LayerIdE::CHORD)) | (1u << static_cast<int>(LayerIdE::ACID))
                           | (1u << static_cast<int>(LayerIdE::GHOST_KICK));
/** @brief The kick and the claps (Mills' "Kick + Claps kurz gemutet"). */
constexpr uint32_t KickClap = (1u << static_cast<int>(LayerIdE::KICK)) | (1u << static_cast<int>(LayerIdE::GHOST_KICK))
                             | (1u << static_cast<int>(LayerIdE::CLAP_A)) | (1u << static_cast<int>(LayerIdE::CLAP_B))
                             | (1u << static_cast<int>(LayerIdE::CLAP_GHOST));
/** @brief The clap-and-perc group but the ghosts. */
constexpr uint32_t Percs = (1u << static_cast<int>(LayerIdE::CLAP_A)) | (1u << static_cast<int>(LayerIdE::CLAP_B))
                          | (1u << static_cast<int>(LayerIdE::SHAKER)) | (1u << static_cast<int>(LayerIdE::TOM_CONGA))
                          | (1u << static_cast<int>(LayerIdE::RIM));

/** @brief Dok. 8.9's harmony check as a filter: at most four pitch classes over what plays, the bass at most two. */
void limitHarmony(RackPlan& plan, const std::vector<LayerIdE>& pool, bool subOwns)
{
    const auto has = [&](LayerIdE id) { return std::find(pool.begin(), pool.end(), id) != pool.end(); };
    if (plan.m_bassSize > 2) { plan.m_bassSize = 2; plan.m_bassSet[0] = 0; plan.m_bassSet[1] = 7; }
    const auto pcs = [&]() {
        std::set<int> s = { 0 };
        const auto add = [&](int semis) { s.insert(((semis % 12) + 12) % 12); };
        if (has(LayerIdE::CHORD)) {
            for (int t = 0; t < plan.m_nChordTones; ++t) add(plan.m_chordTones[t]);
            if (plan.m_shuttleBars > 0) for (int t = 0; t < 3; ++t) add(plan.m_shuttleTones[t]);
        }
        if (subOwns) for (int t = 0; t < plan.m_bassSize; ++t) add(plan.m_bassSet[t]);
        if (has(LayerIdE::PING)) for (int k = 0; k < std::max(1, plan.m_period[L(LayerIdE::PING)]); ++k) add(plan.m_pingNote[k] - plan.m_pingRoot);
        if (has(LayerIdE::DRONE)) add(plan.m_droneNote - plan.m_chordRoot);
        if (has(LayerIdE::ACID))
            for (int k = 0; k < 12; ++k) if ((plan.m_acidMask & (1u << k)) && inScale(plan.m_scale, k)) add(k);
        return s;
    };
    // The 303's alphabet (root, octave, fifth, flat seventh, minor third, fourth) is its own: narrowed to the pitch
    // classes the rest already plays, else to root and fifth.
    if (has(LayerIdE::ACID)) plan.m_acidMask = static_cast<uint16_t>((1u << 0) | (1u << 7) | (1u << 10) | (1u << 3) | (1u << 5));
    const auto narrowAcid = [&] {
        uint16_t mask = (1u << 0) | (1u << 7);
        std::set<int> others = { 0, 7 };
        if (has(LayerIdE::CHORD)) for (int t = 0; t < plan.m_nChordTones; ++t) others.insert(((plan.m_chordTones[t] % 12) + 12) % 12);
        if (subOwns) for (int t = 0; t < plan.m_bassSize; ++t) others.insert(plan.m_bassSet[t] % 12);
        for (int k : others) if (inScale(plan.m_scale, k)) mask = static_cast<uint16_t>(mask | (1u << k));
        plan.m_acidMask = mask;
    };
    // The figure (Phase 8) gives way last: what the track is remembered by keeps its tones longest.
    const LayerIdE fig = plan.m_figure;
    if (pcs().size() <= 4) return;
    plan.m_shuttleBars = 0;                                              // the second chord first
    if (pcs().size() <= 4) return;
    if (fig != LayerIdE::PING) {                                        // then the ping's second tone
        for (int k = 0; k < 64; ++k) plan.m_pingNote[k] = plan.m_pingRoot;
        if (pcs().size() <= 4) return;
    }
    if (has(LayerIdE::ACID) && fig != LayerIdE::ACID) {                  // then the 303's alphabet
        narrowAcid();
        if (pcs().size() <= 4) return;
    }
    plan.m_nChordTones = 3;                                              // the chord a triad
    plan.m_chordTones[0] = 0; plan.m_chordTones[1] = 3; plan.m_chordTones[2] = 7;
    if (pcs().size() <= 4) return;
    if (has(LayerIdE::ACID) && fig == LayerIdE::ACID) {                  // at last the figure: the 303's alphabet,
        narrowAcid();
        if (pcs().size() <= 4) return;
    }
    if (fig == LayerIdE::PING) {                                        // the ping's third tone, then its second
        std::map<int, int> count;
        for (int k = 0; k < std::max(1, plan.m_period[L(LayerIdE::PING)]); ++k) ++count[plan.m_pingNote[k] - plan.m_pingRoot];
        int rarest = 0, n = 1 << 30;
        for (const auto& [iv, c] : count) if (iv != 0 && c < n) { n = c; rarest = iv; }
        for (int k = 0; k < 64; ++k) if (plan.m_pingNote[k] - plan.m_pingRoot == rarest) plan.m_pingNote[k] = plan.m_pingRoot;
        if (pcs().size() <= 4) return;
        for (int k = 0; k < 64; ++k) plan.m_pingNote[k] = plan.m_pingRoot;
    }
}

} // namespace

ArchetypeE pickArchetype(const StyleProfile& prof, float u, int exclude, const Preferences* prefs)
{
    constexpr int n = static_cast<int>(ArchetypeE::COUNT);
    float w[n] = {};
    for (int s = 0; s < 4; ++s) for (int k = 0; k < n; ++k) w[k] += prof.m_styleMix[s] * ArchetypeW[s][k];
    if (prefs != nullptr) for (int k = 0; k < n; ++k) w[k] *= prefs->m_archetype[k];   // (Phase 17: the ratings)
    if (exclude >= 0 && exclude < n) w[exclude] = 0.0f;
    float sum = 0.0f;
    for (float x : w) sum += x;
    if (sum <= 0.0f) return exclude == 0 ? ArchetypeE::ROLLER : ArchetypeE::TOOL;
    float x = u * sum;
    int last = 0;
    for (int k = 0; k < n; ++k) {
        if (w[k] <= 0.0f) continue;
        last = k;
        if (x < w[k]) return static_cast<ArchetypeE>(k);
        x -= w[k];
    }
    return static_cast<ArchetypeE>(last);
}

Score composeTrack(const ParamStore& p, uint64_t seed, const TrackRequest& req, const Curation* cur, const std::string& unit,
                   TrackInfo* info)
{
    StyleProfile prof = req.m_profile != nullptr ? *req.m_profile : profileOf(p);   // (the archetype changes it, below)
    // Phase 11: every stream of a track is the seed's in its style -- one seed in two styles gave two tracks with the same
    // form, the same landings and the same breaks at the same bars (28.09.2026).
    const uint64_t tseed = mixSeed(seed, hashName(prof.m_name));
    const auto pid = [&](ModuleE m, int i, int k) { return p.id(m, i, k); };
    const bool autoOn = p.getInt(pid(ModuleE::COMPOSE, 0, compose::AUTO)) != 0;
    const float energy = req.m_energy;

    // ---------------------------------------------------------------- harmony: tempo, key, scale, the low end's owner
    Rng hr = streamOf(tseed, cur, unit, "harmony");
    const float uBpm = hr.uniform(), uScale = hr.uniform(), uMono = hr.uniform(), uLow = hr.uniform();
    const int uKey = hr.below(12);
    float bpm = req.m_bpm > 0.0f ? req.m_bpm
              : autoOn ? std::round(2.0f * (prof.m_bpmLow + uBpm * (prof.m_bpmHigh - prof.m_bpmLow))) * 0.5f
                       : p.get(pid(ModuleE::COMPOSE, 0, compose::BPM));
    const int key = req.m_key >= 0 ? req.m_key : autoOn ? uKey : p.getInt(pid(ModuleE::COMPOSE, 0, compose::KEY));
    int scale = p.getInt(pid(ModuleE::COMPOSE, 0, compose::SCALE));
    if (req.m_scale >= 0) scale = req.m_scale;
    else if (autoOn) {
        // Dok. 8.6: Aeolian 0.6, Dorian 0.15, the Aeolian-Dorian hexachord 0.1, Phrygian 0.1, the minor pentatonic 0.05.
        scale = uScale < 0.6f ? static_cast<int>(ScaleE::AEOLIAN) : uScale < 0.75f ? static_cast<int>(ScaleE::DORIAN)
              : uScale < 0.85f ? static_cast<int>(ScaleE::HEXACHORD) : uScale < 0.95f ? static_cast<int>(ScaleE::PHRYGIAN)
              : static_cast<int>(ScaleE::MINOR_PENTATONIC);
    }
    const bool monotonic = autoOn && req.m_key < 0 && uMono < 0.08f;
    const bool subOwns = req.m_lowOwner >= 0 ? req.m_lowOwner == 1
                       : autoOn ? uLow < prof.m_subChance
                                : p.getInt(pid(ModuleE::COMPOSE, 0, compose::LOW_OWNER)) == static_cast<int>(LowOwnerE::SUB);

    // ---------------------------------------------------------------- form
    Rng fr = streamOf(tseed, cur, unit, "form");
    const float uForm = fr.uniform(), uIntro = fr.uniform(), uRed = fr.uniform(), uRedLen = fr.uniform(), uRedPos = fr.uniform();
    const float uRumble = fr.uniform(), uEdges = fr.uniform(), uArch = fr.uniform();
    const float uOpen = fr.uniform(), uKickIn = fr.uniform();   // (Phase 20, after the others: they stay as they were)
    const float uKickOut = fr.uniform(), uHatOut = fr.uniform();
    // Phase 13: the archetype -- the set's, the knob's, else drawn by the style -- and the profile as it plays it.
    const int archKnob = p.getInt(pid(ModuleE::COMPOSE, 0, compose::ARCHETYPE));
    const ArchetypeE arch = req.m_archetype >= 0 ? static_cast<ArchetypeE>(req.m_archetype)
                         : archKnob > 0 ? static_cast<ArchetypeE>(archKnob - 1) : pickArchetype(prof, uArch, -1, req.m_prefs);
    applyArchetype(prof, arch);
    FormTypeE form = req.m_form;
    if (form == FormTypeE::COUNT) {
        const int knob = p.getInt(pid(ModuleE::COMPOSE, 0, compose::FORM));
        if (knob > 0) form = static_cast<FormTypeE>(knob - 1);
        else {
            // The set's energy leans towards the Peak at its height and away from it at the ends.
            float wA = prof.m_arcWeight, wP = prof.m_peakWeight, wE = prof.m_endlessWeight;
            if (energy >= 0.0f) wP *= 0.5f + energy;
            const float u = uForm * (wA + wP + wE);
            form = u < wA ? FormTypeE::ARC : u < wA + wP ? FormTypeE::PEAK : FormTypeE::ENDLESS;
        }
    }
    if (req.m_mixable && form == FormTypeE::ENDLESS) form = FormTypeE::ARC;
    const int blocks = req.m_blocks > 0 ? req.m_blocks
                     : std::clamp(static_cast<int>(std::lround(p.get(pid(ModuleE::COMPOSE, 0, compose::MINUTES)) * bpm / 128.0)), 4, 12);
    const int bars = blocks * 32;
    int introBlocks = form == FormTypeE::ENDLESS ? 0 : (uIntro < prof.m_intro64Chance && blocks >= 8 ? 2 : 1);
    int outroBlocks = introBlocks;
    while (introBlocks > 1 && blocks - introBlocks - outroBlocks < 3) { --introBlocks; --outroBlocks; }
    const int bodyFirst = introBlocks, bodyEnd = blocks - outroBlocks;   // body blocks [bodyFirst, bodyEnd)
    const int bodyBars = (bodyEnd - bodyFirst) * 32;
    // The reduction: its length and its return on a 16- or 32-bar line at 50 .. 65 % of the body.
    int redLen = 0;
    if (form == FormTypeE::PEAK) redLen = uRedLen < 0.5f ? 8 : uRedLen < 0.85f ? 16 : 32;
    else if (form == FormTypeE::ARC && uRed < prof.m_toolReductionChance) redLen = uRedLen < 0.5f ? 4 : 8;
    redLen = std::min(redLen, prof.m_maxReduction);
    // Phase 9: a set's track is short (about three minutes of body, Set.h) and its kick-out stays a moment of it -- 8 bars
    // in a body of three blocks or less, 16 in four; the long breaks of a set come from the mixer (Mix-Dok. 8: "Breaks
    // kommen vom DJ ... der Track selbst braucht keinen Breakdown"). A 32-bar kick-out took a set's whole middle block.
    if (req.m_mixable) redLen = std::min(redLen, bodyEnd - bodyFirst <= 3 ? 8 : 16);
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
        if (form == FormTypeE::ENDLESS) d = (b % 3 == 2) ? 1.0f : 0.85f;
        else if (b < bodyFirst) d = b == 0 ? 0.3f : 0.45f;
        else if (b >= bodyEnd) d = (b == blocks - 1) ? 0.3f : (outroBlocks == 2 ? 0.6f : 0.45f);
        else {
            const int i = b - bodyFirst;
            d = std::min(1.0f, 0.6f + 0.25f * static_cast<float>(i));   // full to the body's end; the outro falls
            if (form == FormTypeE::PEAK && redReturn >= 0 && b * 32 >= redReturn && b * 32 < redReturn + 64) d = 1.0f;
        }
        if (energy >= 0.0f && b >= bodyFirst && b < bodyEnd) d = std::clamp(d * (0.85f + 0.3f * energy), 0.3f, 1.0f);
        // Phase 19 (29.09.2026, the user on the set: "die ersten zwei Minuten jedes Tracks ... nur Kick und Hi Hat"): a
        // set's track is heard from its body's first block, under the outgoing track, and owns the floor from its second
        // (Set.h): 0.85 under the other, everything from the swap on -- the swap is its drop. The template's climb (0.6,
        // 0.85, 1.0) is a track's alone. The set's first track the same from its body's first block: nothing lies under it.
        if ((req.m_mixable || req.m_quickStart) && b >= bodyFirst && b < bodyEnd) d = (b == bodyFirst && !req.m_quickStart) ? 0.85f : 1.0f;
        density[static_cast<size_t>(b)] = d;
    }

    // ---------------------------------------------------------------- layers: the pool and the order of entry
    Rng lr = streamOf(tseed, cur, unit, "layers");
    std::vector<LayerIdE> pool;
    std::vector<float> keep;   // how firmly a layer stays when the pool must shrink: its chance, scattered
    for (const LayerChance& c : prof.m_pool) {
        const float u = lr.uniform(), v = lr.uniform();   // drawn for every layer, so one chance moves no other draw
        if (u >= c.m_chance) continue;
        if (monotonic && (c.m_layer == LayerIdE::PING || c.m_layer == LayerIdE::CHORD || c.m_layer == LayerIdE::DRONE || c.m_layer == LayerIdE::ACID)) continue;
        pool.push_back(c.m_layer);
        keep.push_back(c.m_chance + 0.25f * v);
    }
    if (subOwns) { pool.push_back(LayerIdE::BASS); keep.push_back(10.0f); }
    // Phase 8: the figure, the voice the track is remembered by -- since Phase 13 the archetype's: the chord's stabs (Stab,
    // Dub Chord), the 303 (Acid), the ping (Bleep), a bass riff for half the Tools where the sub owns the low end. Phase 18
    // (28.09.2026, the user on a set and a track of the 19:54 build: "zum überwiegenden Teil nur aus Kick und Hi Hats"):
    // every track has one. The Roller rolls a bass riff where the sub owns the low end; the Roller over the rumble, the
    // Tribal track and the other Tools take the style's lead -- of ping, chord and 303 the one its pool likes best
    // (Hypnotic the ping, Ostgut and Dub the chord, Raw the 303). A monotonic track keeps its one tone: its figure plays
    // the root and its octave, a chord's stab goes to the ping. It is in the pool whatever the pool drew, and stays when it
    // must shrink. The voice is the layers' choice; its motif is the
    // stream "figure"'s (makeFigure), so a reroll of the figure draws another motif for the same voice.
    LayerIdE figure = LayerIdE::COUNT;
    {
        const float u = lr.uniform();
        const auto lead = [&prof]() {
            LayerIdE best = LayerIdE::PING;
            float most = -1.0f;
            for (const LayerChance& c : prof.m_pool)
                if ((c.m_layer == LayerIdE::PING || c.m_layer == LayerIdE::CHORD || c.m_layer == LayerIdE::ACID) && c.m_chance > most) {
                    most = c.m_chance;
                    best = c.m_layer;
                }
            return best;
        };
        switch (arch) {
        case ArchetypeE::TOOL: figure = subOwns && u < 0.5f ? LayerIdE::BASS : lead(); break;
        case ArchetypeE::ROLLER: figure = subOwns ? LayerIdE::BASS : lead(); break;
        case ArchetypeE::STAB: case ArchetypeE::DUB_CHORD: figure = LayerIdE::CHORD; break;
        case ArchetypeE::ACID: figure = LayerIdE::ACID; break;
        case ArchetypeE::BLEEP: figure = LayerIdE::PING; break;
        default: figure = lead(); break;   // the Tribal track
        }
        if (monotonic && figure == LayerIdE::CHORD) figure = LayerIdE::PING;
        if (figure != LayerIdE::COUNT) {
            const auto it = std::find(pool.begin(), pool.end(), figure);
            if (it == pool.end()) { pool.push_back(figure); keep.push_back(10.0f); }
            else keep[static_cast<size_t>(it - pool.begin())] = 10.0f;
        }
    }
    // Phase 10 (28.09.2026, the user on a set: "in den ersten 3 Minuten passiert praktisch überhaupt nichts ausser Kick und
    // Hi-Hat ... praktisch gar keine Percussion"): the pool is the groove the track reaches, not one layer a block. It
    // was cut to as many layers as the track had blocks to bring them in, and a set's short track kept three or four,
    // mostly hats. Now it holds at least the style's cap less three -- six or seven voices beside the kick and the offbeat
    // and rolling hats -- filled from the layers the draw left out, the percussion first and the likeliest first, and at
    // most the cap; the build brings several in a block (below). The likeliest layers (a style's signature) stay.
    {
        const size_t least = static_cast<size_t>(std::max(4, prof.m_densityCap - 3));
        std::vector<std::pair<float, LayerIdE>> missing;
        for (const LayerChance& c : prof.m_pool) {
            if (c.m_chance <= 0.0f || std::find(pool.begin(), pool.end(), c.m_layer) != pool.end()) continue;
            if (monotonic && (c.m_layer == LayerIdE::PING || c.m_layer == LayerIdE::CHORD || c.m_layer == LayerIdE::DRONE || c.m_layer == LayerIdE::ACID)) continue;
            missing.emplace_back(c.m_chance + (isTonal(c.m_layer) ? 0.0f : 1.0f), c.m_layer);
        }
        std::stable_sort(missing.begin(), missing.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        // Percussion beyond the hats first: two voices of clap, toms and rim (Hypnotic, Dub), three (Ostgut, Raw) -- a
        // pool of drones and textures left a track with its hats and a shaker. (Phase 19: the shaker no longer counts, it
        // plays on the hats' bus and is one more hat to the ear; a set's Roller had a tom a bar as its whole percussion.)
        static const float PercVoices[4] = { 2.0f, 3.0f, 2.0f, 3.0f };
        float percWant = 0.0f;
        for (int st = 0; st < 4; ++st) percWant += prof.m_styleMix[st] * PercVoices[st];
        const auto percussion = [](LayerIdE id) { return id == LayerIdE::CLAP_A || id == LayerIdE::TOM_CONGA || id == LayerIdE::RIM; };
        int percHave = 0;
        for (LayerIdE id : pool) percHave += percussion(id) ? 1 : 0;
        for (const auto& [weight, layer] : missing) {
            if (percHave >= static_cast<int>(std::lround(percWant))) break;
            if (!percussion(layer)) continue;
            pool.push_back(layer);
            keep.push_back(weight - 0.5f);   // (over the tonal layers of like chance when the cap trims)
            ++percHave;
        }
        for (const auto& [weight, layer] : missing) {
            if (pool.size() >= least) break;
            if (std::find(pool.begin(), pool.end(), layer) != pool.end()) continue;
            pool.push_back(layer);
            keep.push_back(weight - 1.0f);
        }
        // The cap trims the weakest -- never the percussion the floor asked for (Phase 19: an Ostgut Roller's cap was
        // filled with hats, ride, ghost kick and shaker, and cut the clap; its percussion was a tom a bar).
        const size_t maxPool = static_cast<size_t>(std::max(4, prof.m_densityCap));
        while (pool.size() > maxPool) {
            int percNow = 0;
            for (LayerIdE id : pool) percNow += percussion(id) ? 1 : 0;
            const bool spare = percNow <= static_cast<int>(std::lround(percWant));
            size_t weakest = pool.size();
            for (size_t i = 0; i < pool.size(); ++i) {
                if (spare && percussion(pool[i])) continue;
                if (weakest == pool.size() || keep[i] < keep[weakest]) weakest = i;
            }
            if (weakest == pool.size()) break;
            pool.erase(pool.begin() + static_cast<long>(weakest));
            keep.erase(keep.begin() + static_cast<long>(weakest));
        }
    }
    // Phase 20: the opening (Opening, Composer.h), by the style's weights -- from the reference records' first 8 bars
    // (the kick at once / percussion first / an atmosphere or a tonal bed first): Hypnotic 2-3 / 2 / 3 of 7, Ostgut
    // 3 / 3 / 5 of 11, Dub 1 / 1 / 3-4 of 6, Raw 4-5 / 1 / 1 of 6. The atmosphere is the texture: in the pool whatever
    // the pool drew (and, where it was drawn, one voice more for the build it no longer serves).
    OpeningE opening = OpeningE::DRUMS;
    int kickIn = 0, kickLeaves = 0, hatOut = 0;
    if (form != FormTypeE::ENDLESS) {
        static const float OpenW[4][3] = { { 0.35f, 0.30f, 0.35f }, { 0.30f, 0.30f, 0.40f }, { 0.20f, 0.20f, 0.60f }, { 0.70f, 0.15f, 0.15f } };
        float w[3] = {};
        for (int st = 0; st < 4; ++st) for (int k = 0; k < 3; ++k) w[k] += prof.m_styleMix[st] * OpenW[st][k];
        // (Phase 21, 01.10.2026: a set's first track opens the mix with nothing under it -- never with the kick. 24 sets
        // measured: 5 began as the kick alone or with muffled hats.)
        if (req.m_quickStart) w[0] = 0.0f;
        const float u = uOpen * (w[0] + w[1] + w[2]);
        opening = u < w[0] ? OpeningE::DRUMS : u < w[0] + w[1] ? OpeningE::PERCUSSION_FIRST : OpeningE::ATMOSPHERE;
        if (req.m_opening >= 0 && req.m_opening < static_cast<int>(OpeningE::COUNT)) opening = static_cast<OpeningE>(req.m_opening);
        if (opening != OpeningE::DRUMS) kickIn = (uKickIn < 0.6f ? 8 : 16) / (req.m_quickStart ? 2 : 1);
        // Phase 20: the ending, from the reference records' last 64 bars (Tools, outro measurement): 22 of 30 end without
        // the kick -- it leaves 4 to 36 bars before the end, a third of them earlier still; Hypnotic never has it in its
        // last 8 bars, Raw 4 of 6 to the end. The hats have left in 18 of 30 (their median 9 bars before the end);
        // percussion and an atmosphere carry the last bars. The kick leaves 0, 8, 16 or 32 bars before the end (at least
        // as far as the opening held it back), the closed hat 8 or 16 bars before it or not at all.
        static const float KickOutW[4][4] = { { 0.00f, 0.20f, 0.30f, 0.50f }, { 0.15f, 0.25f, 0.30f, 0.30f },
                                               { 0.15f, 0.10f, 0.25f, 0.50f }, { 0.60f, 0.25f, 0.15f, 0.00f } };
        static const float HatStay[4] = { 0.20f, 0.45f, 0.25f, 0.35f };
        float kw[4] = {}, stay = 0.0f;
        for (int st = 0; st < 4; ++st) {
            for (int k = 0; k < 4; ++k) kw[k] += prof.m_styleMix[st] * KickOutW[st][k];
            stay += prof.m_styleMix[st] * HatStay[st];
        }
        static const int Out[4] = { 0, 8, 16, 32 };
        float uk = uKickOut * (kw[0] + kw[1] + kw[2] + kw[3]);
        for (int k = 0; k < 4; ++k) { if (uk < kw[k] || k == 3) { kickLeaves = Out[k]; break; } uk -= kw[k]; }
        kickLeaves = std::max(kickLeaves, kickIn);
        hatOut = uHatOut < stay ? 0 : uHatOut < stay + 0.6f * (1.0f - stay) ? 8 : 16;
        if (opening == OpeningE::ATMOSPHERE) {
            if (std::find(pool.begin(), pool.end(), LayerIdE::TEXTURE) == pool.end()) {
                pool.push_back(LayerIdE::TEXTURE);
                keep.push_back(10.0f);
            } else {
                LayerIdE more = LayerIdE::COUNT;
                float most = 0.0f;
                for (const LayerChance& c : prof.m_pool) {
                    if (c.m_chance <= most || c.m_layer == LayerIdE::TEXTURE || std::find(pool.begin(), pool.end(), c.m_layer) != pool.end()) continue;
                    if (monotonic && (c.m_layer == LayerIdE::PING || c.m_layer == LayerIdE::CHORD || c.m_layer == LayerIdE::DRONE || c.m_layer == LayerIdE::ACID)) continue;
                    most = c.m_chance;
                    more = c.m_layer;
                }
                if (more != LayerIdE::COUNT) { pool.push_back(more); keep.push_back(most); }
            }
            // (A place more under the density's cap: the atmosphere plays from the first bar, and the cap had the build
            // throw its figure out again at bar 97.)
            ++prof.m_densityCap;
        }
    }
    // The order: by group, shuffled within it. (The rolling hat's other half comes with the body's first bar.)
    std::vector<LayerIdE> order = pool;
    for (int i = static_cast<int>(order.size()) - 1; i > 0; --i) std::swap(order[static_cast<size_t>(i)], order[static_cast<size_t>(lr.below(i + 1))]);
    std::stable_sort(order.begin(), order.end(), [](LayerIdE a, LayerIdE b) { return groupOf(a) < groupOf(b); });
    // Phase 18: the hats' group and the percussion's interleaved, each its likeliest first (the pool's chances, scattered):
    // every other entry of the build is a percussion voice. By group, Ostgut's clap (0.9) waited behind the open hat, the
    // ride and the ghost kick until after bar 137.
    {
        std::vector<std::pair<float, LayerIdE>> hats, percs;
        for (size_t i = 0; i < pool.size(); ++i) {
            // (The shaker: a hat to the ear; the ghost kick: an accent, no voice -- both among the hats here.)
            if (groupOf(pool[i]) == 1 || pool[i] == LayerIdE::SHAKER || pool[i] == LayerIdE::GHOST_KICK) hats.emplace_back(keep[i], pool[i]);
            else if (groupOf(pool[i]) == 2) percs.emplace_back(keep[i], pool[i]);
        }
        const auto likeliest = [](const auto& a, const auto& b) { return a.first > b.first; };
        std::stable_sort(hats.begin(), hats.end(), likeliest);
        std::stable_sort(percs.begin(), percs.end(), likeliest);
        std::vector<LayerIdE> woven;
        for (size_t i = 0; i < std::max(hats.size(), percs.size()); ++i) {
            if (i < percs.size()) woven.push_back(percs[i].second);
            if (i < hats.size()) woven.push_back(hats[i].second);
        }
        std::vector<LayerIdE> rest;
        for (LayerIdE id : order) if (groupOf(id) != 1 && groupOf(id) != 2) rest.push_back(id);
        order.clear();
        for (LayerIdE id : rest) if (groupOf(id) == 0) order.push_back(id);
        order.insert(order.end(), woven.begin(), woven.end());
        for (LayerIdE id : rest) if (groupOf(id) != 0) order.push_back(id);
    }
    // The intro's one perc: the first of the clap-and-perc group that is no ghost -- and no shaker (Phase 19: it plays on
    // the hats' bus and sounds as one more hat; a set's first track had its clap at bar 97).
    LayerIdE introPerc = LayerIdE::COUNT;
    for (LayerIdE id : order) if (groupOf(id) == 2 && id != LayerIdE::GHOST_KICK && id != LayerIdE::SHAKER) { introPerc = id; break; }
    if (introPerc == LayerIdE::COUNT)
        for (LayerIdE id : order) if (groupOf(id) == 2 && id != LayerIdE::GHOST_KICK) { introPerc = id; break; }
    // Phase 13: the Roller's and the Tribal track's toms are their face -- the intro's perc, so the kind is heard at once.
    if ((arch == ArchetypeE::ROLLER || arch == ArchetypeE::TRIBAL) && std::find(order.begin(), order.end(), LayerIdE::TOM_CONGA) != order.end())
        introPerc = LayerIdE::TOM_CONGA;

    // ---------------------------------------------------------------- rack
    Rng rr = streamOf(tseed, cur, unit, "rack");
    RackSettings rs;
    rs.m_keyRoot = key;
    rs.m_scale = scale;
    rs.m_swing = prof.m_swingLow + rr.uniform() * (prof.m_swingHigh - prof.m_swingLow);
    rs.m_humanizeMs = p.get(pid(ModuleE::COMPOSE, 0, compose::HUMANIZE));
    rs.m_mutation = prof.m_mutation;
    rs.m_motionScale = prof.m_motionScale;
    rs.m_reroll = prof.m_reroll;
    rs.m_polymeterChance = prof.m_polymeterChance;
    rs.m_fillChance = prof.m_fillChance;
    RackPlan plan = makeRackPlan(rs, streamSeed(tseed, cur, unit, "rack"));
    for (int l = 0; l < NumLayers; ++l) {
        const int n = cur != nullptr ? cur->count(unit + "rack." + LayerUnit[l]) : 0;
        if (n > 0) plan.m_layerSeed[l] = mixSeed(mixSeed(plan.m_seed, 0x4C41594552ull + static_cast<uint64_t>(l)), static_cast<uint64_t>(n));
    }
    if (cur != nullptr && cur->count(unit + "rack.clap") > 0) plan.m_layerSeed[L(LayerIdE::CLAP_B)] = plan.m_layerSeed[L(LayerIdE::CLAP_A)];
    if (monotonic) plan.m_bassSize = 1;
    // The bass at most two tones (limitHarmony's first rule) before the bass's figure reads its second.
    if (plan.m_bassSize > 2) { plan.m_bassSize = 2; plan.m_bassSet[0] = 0; plan.m_bassSet[1] = 7; }
    makeFigure(plan, figure, streamSeed(tseed, cur, unit, "figure"));
    if (monotonic) for (int8_t& fp : plan.m_figPitch) fp = fp == 12 ? static_cast<int8_t>(12) : static_cast<int8_t>(0);   // (its one tone)
    limitHarmony(plan, pool, subOwns);
    // Phase 21 (01.10.2026, the user on a mix: it "begann ... wieder mit einer Solo-Kick"): the voice that opens the
    // track carries it. A tom or a rim left to the matrix played one or two hits a bar (a set's first track: its tom 24 dB
    // under the kick, the body's percussion 13); it gets a Euclidean figure on the twelve sixteenths off the quarters --
    // E(5,12), E(6,12) or E(7,12), rotated, five to seven hits a bar (offQuarterMask) -- unless the rack gave it a cycle
    // or a figure already. From the rack's stream, so a reroll of the rack draws it anew.
    if ((introPerc == LayerIdE::TOM_CONGA || introPerc == LayerIdE::RIM) && plan.m_period[L(introPerc)] == 0 && plan.m_euclid[L(introPerc)] == 0) {
        Rng er;
        er.seed(mixSeed(streamSeed(tseed, cur, unit, "rack"), 0x4F50454E494E47ull));
        // (The two draws one after the other: as arguments of one call their order would be the compiler's.)
        const int onsets = 5 + er.below(3);
        const int rotation = er.below(12);
        plan.m_euclid[L(introPerc)] = offQuarterMask(onsets, rotation);
        plan.m_displace[L(introPerc)] = 0;
    }
    int bands[NumParts];
    partBands(plan, bands);

    // ---------------------------------------------------------------- sounds
    // A factory preset per synth (Presets.h), drawn by the fit of its group to the profile's style mix -- a lane of the kit
    // among the presets made for its role -- then the style's mix (its recipe), then its ranges: a knob a preset sets is
    // held inside the style's range, a knob no preset sets is drawn in it. Every knob the composer manages is set, at the
    // track's start, as an absolute value (Score::knobs): the knobs show them while the track plays (Engine.h).
    Rng sr = streamOf(tseed, cur, unit, "sounds");
    TrackKnobs knobs{ p, {} };
    std::vector<SoundPick> picks;
    const bool pickSounds = p.getBool(pid(ModuleE::COMPOSE, 0, compose::PICK_SOUNDS));
    static const ModuleE Synths[] = { ModuleE::KICK, ModuleE::RUMBLE, ModuleE::SUB, ModuleE::PING, ModuleE::BASS, ModuleE::ACID, ModuleE::CHORD,
                                      ModuleE::DRONE, ModuleE::TEXTURE, ModuleE::PERC };
    if (pickSounds) {
        for (ModuleE m : Synths) {
            const int instances = m == ModuleE::PERC ? PercLanes : 1;
            for (int inst = 0; inst < instances; ++inst) {
                const int role = m == ModuleE::PERC ? p.getInt(pid(ModuleE::PERC, inst, perc::ROLE)) : -1;
                const int index = pickPreset(m, prof.m_styleMix, role, sr, req.m_prefs);
                if (index < 0) continue;
                for (const auto& [k, v] : presetKnobs(m, inst, factoryPresets(m)[static_cast<size_t>(index)])) knobs.m_value[pid(m, inst, k)] = v;
                picks.push_back(SoundPick{ 0.0, static_cast<int>(m), inst, index });
            }
        }
    }
    for (const SoundValue& v : prof.m_recipe) { const int id = p.find(v.m_key); if (id >= 0) knobs.m_value[id] = v.m_value; }
    for (const SoundRange& r : prof.m_sounds) {
        const float u = sr.uniform();
        const int id = p.find(r.m_key);
        if (id < 0) continue;
        const float lo = std::max(r.m_low, p.desc(id).m_minValue), hi = std::min(r.m_high, p.desc(id).m_maxValue);
        const auto set = knobs.m_value.find(id);
        if (set != knobs.m_value.end()) { set->second = std::clamp(set->second, lo, hi); continue; }   // a preset's: held in range
        const float nl = p.toNormalised(id, lo), nh = p.toNormalised(id, hi);
        knobs.m_value[id] = p.fromNormalised(id, nl + u * (nh - nl));
    }
    knobs.m_value[pid(ModuleE::COMPOSE, 0, compose::KEY)] = static_cast<float>(key);
    knobs.m_value[pid(ModuleE::COMPOSE, 0, compose::SCALE)] = static_cast<float>(scale);
    knobs.m_value[pid(ModuleE::COMPOSE, 0, compose::LOW_OWNER)] = subOwns ? 1.0f : 0.0f;
    // Every knob any style sets, set by every track -- its own value or the knob as it stands -- so no track on a deck
    // plays on with the last one's, and the knobs always show the whole of what plays.
    for (const int id : managedKnobs(p)) knobs.m_value.emplace(id, p.get(id));
    // Phase 13: the archetype's mix -- the Roller's and the Tribal track's percussion up, the figure's voice up.
    {
        const auto lift = [&](const char* key, float by) {
            const int id = p.find(key);
            if (id >= 0) knobs.m_value[id] = std::clamp(knobs.get(id) + by, p.desc(id).m_minValue, p.desc(id).m_maxValue);
        };
        switch (arch) {
        case ArchetypeE::TOOL: lift("mix.perc_level", 1.0f); break;
        case ArchetypeE::ROLLER: lift("mix.perc_level", 3.0f); break;
        case ArchetypeE::TRIBAL: lift("mix.perc_level", 4.0f); break;
        case ArchetypeE::STAB: lift("chord.level", 2.0f); break;
        case ArchetypeE::DUB_CHORD: lift("chord.level", 1.0f); lift("chord.dub_send", 0.2f); lift("dub.feedback", 0.05f); break;
        case ArchetypeE::ACID: lift("acid.level", 2.0f); break;
        case ArchetypeE::BLEEP: lift("ping.level", 2.0f); break;
        default: break;
        }
    }
    // Phase 8: the chord as the figure sounds for longer than the old stabs (1.3 an eighth long a bar, 0.65 beats): its
    // level comes down by 70 % of the difference, so the widest voice of the mix does not widen and fill it (28.09.2026:
    // the Dub tracks measured -2 to -3 dB S/M against the references' -8).
    if (figure == LayerIdE::CHORD) {
        double sounding = 0.0;
        for (int pos = 0; pos < plan.m_figBars * Steps; ++pos)
            if ((plan.m_figOn >> pos) & 1u) sounding += std::min(plan.m_figLength, 1.5);
        sounding /= plan.m_figBars;
        const int cl = pid(ModuleE::CHORD, 0, chord::LEVEL);
        if (sounding > 0.65) knobs.m_value[cl] = knobs.get(cl) - static_cast<float>(0.7 * 10.0 * std::log10(sounding / 0.65));
    }

    Score sc;
    sc.clear(bpm);
    sc.m_seed = seed;
    sc.m_keyRoot = key;
    sc.m_scale = scale;
    for (const auto& kv : knobs.m_value) {
        const ModuleE m = p.moduleOf(kv.first);
        const bool sound = hasPresets(m) && !presetLeaves(m, p.indexOf(kv.first));
        sc.m_knobs.push_back(KnobSet{ 0.0, kv.first, kv.second, sound ? 0 : 1 });
    }
    sc.m_sounds = picks;

    // ---------------------------------------------------------------- events on the body's 8-bar lines
    Rng er = streamOf(tseed, cur, unit, "events");
    std::vector<BarMods> mods(static_cast<size_t>(bars));
    struct Throw { int m_bar; };
    std::vector<Throw> throws;
    std::vector<int> hpSweeps;   // bars of the 32-line a group high-pass sweep leads into
    const auto inReduction = [&](int bar) { return redLen > 0 && bar >= redStart && bar < redReturn; };
    for (int line = bodyFirst * 32 + 8; line < bodyEnd * 32; line += 8) {
        const float u = er.uniform(), kind = er.uniform(), where = er.uniform();
        if (u >= prof.m_eventRate || inReduction(line - 1) || inReduction(line) || line == redStart || line == redReturn) continue;
        const int last = line - 1;   // the phrase's last bar
        if (kind < prof.m_throwShare) throws.push_back({ last });
        else if (form == FormTypeE::ENDLESS || kind < prof.m_throwShare + (1.0f - prof.m_throwShare) * 0.35f)
            mods[static_cast<size_t>(last)].m_muteStep = where < 0.5f ? 14 : 12;
        else if ((line % 16) == 0 && kind < prof.m_throwShare + (1.0f - prof.m_throwShare) * 0.55f)
            mods[static_cast<size_t>(last)].m_dropout = true;
        else
            mods[static_cast<size_t>(last)].m_ghostBoost = 1.6f;
    }
    // A sweep of the group high pass into a 32-bar line of the body (p 0.3) and into every return.
    for (int b = bodyFirst + 1; b < bodyEnd; ++b)
        if (er.uniform() < 0.3f && !inReduction(b * 32 - 1) && b * 32 != redReturn) hpSweeps.push_back(b * 32);
    if (redReturn >= 0 && redLen >= 8) hpSweeps.push_back(redReturn);
    // "Mute one expected hit" before the body's first change.
    if (bodyFirst > 0) mods[static_cast<size_t>(bodyFirst * 32 - 1)].m_muteStep = 14;
    // The cut before a return: the Peak's bar of nothing but the tails (p 0.5), else a missing hit.
    const bool redCut = form == FormTypeE::PEAK && er.uniform() < 0.5f;
    if (redReturn >= 0 && !redCut) mods[static_cast<size_t>(redReturn - 1)].m_muteStep = 14;

    // ---------------------------------------------------------------- Phase 8: the waves
    // The body's ups and downs between the operations (28.09.2026, the user: "keine richtigen Ups und Downs ... ein starres
    // 32-Takt-Raster"; the references change something audible 4.2 times in 64 bars, the tracks did 1.5). From the
    // figure's entry to the outro the body runs in waves of 32 or 64 bars. The figure's knobs and the hats' bus build
    // towards a wave's last bar and resolve on its line, the landing (the automation below); two to four bars before a
    // landing something drops away so that it lands -- the centre, the kick and the claps, the figure, or the kick for a
    // bar (Dok. "Spannungsmittel ohne Drop"); in the middle of a wave one element breathes out for 8 or 16 bars and comes
    // back with a throw (The Acid Mind's 64-bar cell: "remove one low element ... return the missing voice").
    int figureBlock = -1;
    if (figure != LayerIdE::COUNT) {
        // The body's second block (Dok. 8.5's template: +Bass at 33, +Stab at 65); the bass with the body; the Endless's from
        // its first bar.
        if (form == FormTypeE::ENDLESS) figureBlock = 0;
        else if (figure == LayerIdE::BASS || req.m_quickStart) figureBlock = bodyFirst;   // (Phase 19: the set's first track at once)
        else figureBlock = std::min(bodyFirst + 1, bodyEnd - 1);
    }
    struct Wave { int m_start = 0, m_land = 0, m_shape = 0; float m_depth = 1.0f; };
    std::vector<Wave> waves;
    std::vector<std::pair<int, std::string>> moments;
    int mainLanding = -1;
    const int rumbleLevel = pid(ModuleE::RUMBLE, 0, rumble::LEVEL);
    // The styles' shares, fitted to the references' movement over eight bars (analyze_ref.py, meso_*: 28.09.2026) --
    // Hypnotic moves in the low end and the highs and least in the mids, Raw in the highs with its loudness flat, Ostgut
    // and Dub in all three. Per style (Hypnotic, Ostgut, Dub, Raw): a wave of 64 bars rather than 32, a breath in a
    // wave, the depth of the figure's waves and of the hats', the down before a landing (the centre, the kick and the
    // claps, the figure, the kick for a bar, none) and what breathes (the figure, the low end, the percussion, the tops).
    static const float Long[4] = { 0.4f, 0.35f, 0.5f, 0.3f }, Breath[4] = { 0.7f, 0.5f, 0.6f, 0.5f };   // (Phase 13: Ostgut and Dub land more often)
    static const float FigDepth[4] = { 0.6f, 1.0f, 1.0f, 0.5f }, HatDepth[4] = { 1.15f, 1.0f, 1.0f, 1.7f };
    static const float Down[4][5] = { { 0.30f, 0.15f, 0.25f, 0.15f, 0.15f }, { 0.30f, 0.20f, 0.20f, 0.15f, 0.15f },
                                       { 0.35f, 0.10f, 0.30f, 0.10f, 0.15f }, { 0.10f, 0.25f, 0.15f, 0.15f, 0.35f } };   // (Raw: its loudness flat)
    static const float BreathOf[4][4] = { { 0.20f, 0.40f, 0.10f, 0.30f }, { 0.30f, 0.30f, 0.25f, 0.15f },
                                           { 0.35f, 0.35f, 0.20f, 0.10f }, { 0.15f, 0.05f, 0.20f, 0.60f } };
    const auto mixOf = [&](const float* table) {
        float v = 0.0f;
        for (int s = 0; s < 4; ++s) v += prof.m_styleMix[s] * table[s];
        return v;
    };
    // (Phase 13: the Acid, Bleep and Stab tracks ride their figure harder.)
    const float figDepth = mixOf(FigDepth) * (arch == ArchetypeE::ACID ? 1.4f : arch == ArchetypeE::BLEEP || arch == ArchetypeE::STAB ? 1.2f : 1.0f);
    const float hatDepth = mixOf(HatDepth);
    {
        const float p64 = mixOf(Long), pBreath = mixOf(Breath);
        float downW[5] = {}, breathW[4] = {};
        for (int s = 0; s < 4; ++s) {
            for (int k = 0; k < 5; ++k) downW[k] += prof.m_styleMix[s] * Down[s][k];
            for (int k = 0; k < 4; ++k) breathW[k] += prof.m_styleMix[s] * BreathOf[s][k];
        }
        const auto pick = [](const float* w, int n, float u) {
            float sum = 0.0f;
            for (int k = 0; k < n; ++k) sum += w[k];
            u *= sum;
            for (int k = 0; k < n; ++k) { if (u < w[k]) return k; u -= w[k]; }
            return n - 1;
        };
        const int from = form == FormTypeE::ENDLESS ? 32 : (figureBlock >= 0 ? figureBlock * 32 : bodyFirst * 32);
        const int to = bodyEnd * 32;
        for (int a = from; to - a >= 32;) {
            Wave w;
            w.m_start = a;
            int len = (to - a >= 64 && er.uniform() < p64) ? 64 : 32;
            const int rest = to - (a + len);
            if (rest > 0 && rest < 32) len = to - a <= 64 ? to - a : to - a - 32;      // no stub of a wave before the outro
            w.m_land = a + len;
            if (redLen > 0 && redReturn > a && redStart < w.m_land) w.m_land = redReturn;   // the reduction's return is its landing
            if (to - w.m_land > 0 && to - w.m_land < 16) w.m_land = to;
            const float u = er.uniform();
            w.m_shape = u < 0.5f ? 0 : (u < 0.8f ? 1 : 2);                               // creep, arch, open and close
            w.m_depth = 0.6f + 0.3f * er.uniform();
            waves.push_back(w);
            a = w.m_land;
        }
        // The main landing: the one nearest 60 % of the body that is neither a return nor the outro's first bar.
        const double aim = bodyFirst * 32 + 0.6 * bodyBars;
        const auto plain = [&](int land) { return land != redReturn && land != redStart && land < to; };
        for (Wave& w : waves)
            if (plain(w.m_land) && (mainLanding < 0 || std::fabs(w.m_land - aim) < std::fabs(mainLanding - aim))) mainLanding = w.m_land;
        for (Wave& w : waves) if (w.m_land == mainLanding) w.m_depth = 1.0f;
        const auto mute = [&](int from, int n, uint32_t mask) {
            for (int bar = std::max(0, from); bar < std::min(bars, from + n); ++bar) mods[static_cast<size_t>(bar)].m_muteLayers |= mask;
        };
        const auto figureAt = [&](int bar) { return figureBlock >= 0 && bar >= figureBlock * 32 + 8 && figure != LayerIdE::BASS; };
        for (const Wave& w : waves) {
            // The down before the landing (not before a kick-out or a return, which have their own, nor into the outro).
            if (plain(w.m_land)) {
                // The main landing: the centre (0.6) or the figure out before it; the others by the style.
                const bool main = w.m_land == mainLanding;
                const float u = er.uniform(), v = er.uniform();
                static const float Main[5] = { 0.6f, 0.0f, 0.4f, 0.0f, 0.0f };
                int kind = pick(main ? Main : downW, 5, u);
                if (kind == 2 && !figureAt(w.m_land - 4)) kind = main ? 0 : 4;
                int n = 0;
                std::string what;
                if (kind == 0) {
                    n = v < 0.5f ? 2 : 4;
                    mute(w.m_land - n, n, Centre);
                    what = "the centre out";
                } else if (kind == 1) {
                    n = v < 0.5f ? 1 : 2;
                    mute(w.m_land - n, n, KickClap);
                    what = "kick and claps out";
                } else if (kind == 2) {
                    n = 4;
                    mute(w.m_land - n, n, bitOf(figure));
                    what = "the figure out";
                } else if (kind == 3) {
                    n = 1;
                    mods[static_cast<size_t>(w.m_land - 1)].m_dropout = true;
                    what = "the kick out";
                }
                if (n > 0) moments.emplace_back(w.m_land - n, what + fmtBars(n));
            }
            // The breath in the middle of the wave.
            const int len = w.m_land - w.m_start;
            if (len >= 32 && er.uniform() < pBreath) {
                const int span = len >= 64 ? 16 : 8;
                const int at = w.m_start + len / 2;
                int kind = pick(breathW, 4, er.uniform());
                if (kind == 0 && !figureAt(at)) kind = 1;
                const bool clash = redLen > 0 && at < redReturn && at + span > redStart;
                if (!clash) {
                    std::string what;
                    if (kind == 0) { mute(at, span, bitOf(figure)); what = "the figure"; }
                    else if (kind == 1 && subOwns) { mute(at, span, bitOf(LayerIdE::BASS) | bitOf(LayerIdE::ACID)); what = "the bass"; }
                    else if (kind == 1) {
                        sc.m_gestures.push_back(knobs.step(rumbleLevel, static_cast<double>(at) * Bar, -60.0f));
                        sc.m_gestures.push_back(knobs.home(rumbleLevel, static_cast<double>(at + span) * Bar));
                        what = "the rumble";
                    } else if (kind == 2) { mute(at, span, Percs); what = "the percussion"; }
                    else { mute(at, span, bitOf(LayerIdE::OPEN_HAT) | bitOf(LayerIdE::RIDE) | bitOf(LayerIdE::ROLLING_HAT)); what = "the tops"; }
                    throws.push_back({ at + span - 1 });
                    moments.emplace_back(at, "breath: " + what + " out" + fmtBars(span));
                }
            }
        }
    }

    // ---------------------------------------------------------------- blocks, their operation and their candidates
    const uint64_t blocksSeed = streamSeed(tseed, cur, unit, "blocks");
    LayerState st;
    st.m_active[L(LayerIdE::KICK)] = kickIn == 0;   // (Phase 20: the kick enters on bar 9 or 17 when the opening holds it back)
    std::vector<LayerIdE> queue;   // what may still enter, in order
    for (LayerIdE id : order)
        if ((id != introPerc || form == FormTypeE::ENDLESS) && !(opening == OpeningE::ATMOSPHERE && id == LayerIdE::TEXTURE)) queue.push_back(id);
    std::vector<LayerIdE> entered = { LayerIdE::KICK };
    std::vector<int> entryBar(NumLayers, -1);
    entryBar[L(LayerIdE::KICK)] = 0;
    if (form == FormTypeE::ENDLESS) {
        // Kick, bass and tops from bar 1 (Erg. 8).
        for (LayerIdE id : { LayerIdE::CLOSED_HAT, LayerIdE::ROLLING_HAT }) { st.m_active[L(id)] = true; entryBar[L(id)] = 0; entered.push_back(id); }
        int tops = 0;
        // With the track's signature: its figure (Phase 8), else the tonal layer the style is likeliest to have.
        LayerIdE signature = figure;
        float most = 0.0f;
        if (signature == LayerIdE::COUNT)
            for (const LayerChance& c : prof.m_pool)
                if ((c.m_layer == LayerIdE::PING || c.m_layer == LayerIdE::CHORD || c.m_layer == LayerIdE::ACID || c.m_layer == LayerIdE::DRONE) && c.m_chance > most
                    && std::find(queue.begin(), queue.end(), c.m_layer) != queue.end()) { most = c.m_chance; signature = c.m_layer; }
        for (auto it = queue.begin(); it != queue.end();) {
            const int g = groupOf(*it);
            const bool take = *it == LayerIdE::ROLLING_HAT || g == 0 || (g == 1 && *it == LayerIdE::OPEN_HAT) || (g == 2 && tops < 2 && *it != LayerIdE::GHOST_KICK)
                           || *it == signature;
            if (!take) { ++it; continue; }
            if (g == 2) ++tops;
            if (*it != LayerIdE::ROLLING_HAT) { st.m_active[L(*it)] = true; entryBar[L(*it)] = 0; entered.push_back(*it); }
            it = queue.erase(it);
        }
    }
    const int poolSize = static_cast<int>(pool.size()) + 2;
    const auto targetCount = [&](int b) {
        return std::clamp(static_cast<int>(std::lround(density[static_cast<size_t>(b)] * poolSize)), 2, prof.m_densityCap);
    };
    const int half = bars / 2;
    std::vector<int> chosenVariant(static_cast<size_t>(blocks), 0);
    // The operations' choices come from the layers' stream, not from the candidates: a block drawn again changes its own
    // bars only, never what enters later.
    std::vector<size_t> opChoice(static_cast<size_t>(blocks));
    std::vector<bool> swapChoice(static_cast<size_t>(blocks));
    // Phase 8: a hat or a perc comes in on its block's first bar (0.5), on its 8-bar line (0.3) or its 16-bar line (0.2)
    // -- Dok. "jeder Einsatz sitzt auf Takt 1 einer 8-Takt-Phrase"; the bass, the figure and the big changes keep the 32.
    std::vector<int> entryOffset(static_cast<size_t>(blocks));
    for (int b = 0; b < blocks; ++b) {
        opChoice[static_cast<size_t>(b)] = static_cast<size_t>(lr.below(2));
        swapChoice[static_cast<size_t>(b)] = lr.uniform() < 0.5f;
        const float u = lr.uniform();
        entryOffset[static_cast<size_t>(b)] = u < 0.5f ? 0 : (u < 0.8f ? 8 : 16);
    }

    // Removes the latest-entered active layer that @p pick accepts (never kick, offbeat hat, rolling hat).
    const auto removeLatest = [&](LayerState& s, std::vector<LayerIdE>& ent, bool tonalOnly, LayerIdE* out, bool spareEnd = false) {
        for (auto it = ent.rbegin(); it != ent.rend(); ++it) {
            const LayerIdE id = *it;
            if (id == LayerIdE::KICK || id == LayerIdE::CLOSED_HAT || id == LayerIdE::ROLLING_HAT || !s.m_active[L(id)]) continue;
            if (spareEnd && (id == introPerc || id == LayerIdE::TEXTURE)) continue;   // (Phase 20: they carry the end)
            if (tonalOnly && !isTonal(id)) continue;
            s.m_active[L(id)] = false;
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
            float m_dist = 1e30f;
            std::vector<NoteEvent> m_notes;
            LayerState m_after;
            std::vector<LayerIdE> m_queue, m_entered;
            std::vector<BlockOp> m_ops;
            std::vector<int> m_entryBar;
            uint32_t m_variant = 0;
        };
        Cand best;
        for (int c = 0; c < Candidates; ++c) {
            Cand k;
            k.m_variant = static_cast<uint32_t>(vr.next() >> 33) | 1u;
            LayerState s = st;
            std::vector<LayerIdE> q = queue, ent = entered;
            std::vector<int> eb = entryBar;
            std::vector<BlockOp> ops;
            const auto op = [&](int bar, OpKindE kind, LayerIdE layer, LayerIdE other = LayerIdE::COUNT) {
                BlockOp o;
                o.m_beat = static_cast<double>(bar) * Bar;
                o.m_kind = kind;
                o.m_layer = layer == LayerIdE::COUNT ? -1 : L(layer);
                o.m_other = other == LayerIdE::COUNT ? -1 : L(other);
                ops.push_back(o);
            };
            const auto enter = [&](int bar, LayerIdE id, bool logOp) {
                if (id == LayerIdE::ROLLING_HAT) s.m_density[L(id)] = 1.0f;
                else s.m_active[L(id)] = true;
                if (eb[L(id)] < 0 || id == LayerIdE::ROLLING_HAT) eb[L(id)] = bar;
                ent.push_back(id);
                if (logOp) op(bar, OpKindE::ADD, id);
            };
            // --- the block's operation, at its first bar (a hat's or a perc's entry on its 8- or 16-bar line) ---
            const int first = b * 32;
            std::vector<std::pair<int, LayerIdE>> pendings;   // entries inside the block: (bar, layer)
            bool hatGoes = false;   // (Phase 20: the closed hat leaves before the end, in the last block)
            if (b == 0) {
                op(first, OpKindE::START, form == FormTypeE::ENDLESS || opening == OpeningE::DRUMS ? LayerIdE::KICK
                                         : opening == OpeningE::ATMOSPHERE ? LayerIdE::TEXTURE : LayerIdE::CLOSED_HAT);
            } else if (intro) {
                // The second intro block brings the first of the hats' group.
                LayerIdE add = LayerIdE::COUNT;
                for (auto it = q.begin(); it != q.end(); ++it) if (groupOf(*it) == 1 && *it != LayerIdE::ROLLING_HAT) { add = *it; q.erase(it); break; }
                if (add != LayerIdE::COUNT) enter(first, add, true);
                else op(first, OpKindE::HOLD, LayerIdE::COUNT);
            } else if (outro) {
                if (b == blocks - 1) {
                    // Phase 20: the last block keeps the intro's perc and the atmosphere to the end (the records' last bars
                    // are percussion and an atmosphere); where the closed hat leaves before the end, a second percussion
                    // stays with them -- or, with none to stay, the hat does.
                    LayerIdE second = LayerIdE::COUNT;
                    for (LayerIdE id : ent)
                        if (groupOf(id) == 2 && id != LayerIdE::GHOST_KICK && id != LayerIdE::SHAKER && id != introPerc && s.m_active[L(id)]) { second = id; break; }
                    hatGoes = hatOut > 0 && (second != LayerIdE::COUNT || s.m_active[L(LayerIdE::TEXTURE)]);
                    bool any = false;
                    // (The kick leaving 32 bars before the end is this block's operation: one a block.)
                    if (kickLeaves >= 32 && s.m_active[L(LayerIdE::KICK)]) {
                        s.m_active[L(LayerIdE::KICK)] = false;
                        op(first, OpKindE::REMOVE, LayerIdE::KICK);
                        any = true;
                    }
                    for (int i = 1; i < NumLayers; ++i) {
                        const LayerIdE id = static_cast<LayerIdE>(i);
                        if (id == LayerIdE::CLOSED_HAT || id == LayerIdE::ROLLING_HAT || id == introPerc || id == LayerIdE::TEXTURE
                            || (hatGoes && id == second) || !s.m_active[i]) continue;
                        if (!any) op(first, OpKindE::REMOVE, id);
                        s.m_active[i] = false;
                        any = true;
                    }
                    if (!any) op(first, OpKindE::HOLD, LayerIdE::COUNT);
                    s.m_density[L(LayerIdE::ROLLING_HAT)] = 0.5f;
                }
                // The other outro blocks remove one every eight bars, below.
            } else {
                // (Phase 10: the first half builds to two under the style's cap, so two layers still enter after it.)
                const int now = s.count(), want = first < half ? std::min(targetCount(b), prof.m_densityCap - 2) : targetCount(b);
                // What may enter now: the sub bass with the body (a big change on a 32-bar line); else in order, the
                // next two if they are of one group (the candidates try both), the last two not before the half.
                std::vector<size_t> can;
                const auto bass = std::find(q.begin(), q.end(), LayerIdE::BASS);
                const bool bassNow = bass != q.end() && subOwns;
                // Phase 8: the figure on its block, before whatever else waits. (Phase 18: also where the pool is small --
                // the rule that keeps the last two layers for the second half held the figure back to bar 129 of 224.)
                const auto fig = std::find(q.begin(), q.end(), figure);
                const bool figureNow = !bassNow && figure != LayerIdE::COUNT && fig != q.end() && figureBlock >= 0 && b >= figureBlock;
                // (The layers kept for the second half: two of those waiting beside the figure.)
                const size_t waiting = q.size() - (fig != q.end() ? 1u : 0u);
                if (bassNow) can.push_back(static_cast<size_t>(bass - q.begin()));
                else if (figureNow) can.push_back(static_cast<size_t>(fig - q.begin()));
                for (size_t i = 0; !bassNow && !figureNow && i < q.size() && can.size() < 2; ++i) {
                    if (waiting <= 2 && first < half) break;
                    if (q[i] == figure) continue;
                    if (!can.empty() && groupOf(q[i]) != groupOf(q[can[0]])) break;
                    can.push_back(i);
                }
                // Two layers enter in the second half, whatever the density says (Phase 8: the figure's early entry
                // took a late one's place), while the cap allows.
                int late = 0;
                for (int l = 1; l < NumLayers; ++l) if (eb[static_cast<size_t>(l)] >= half) ++late;
                const bool lateNeeded = first >= half && late < 2 && now < prof.m_densityCap;
                if ((now < want || bassNow || figureNow || lateNeeded) && !can.empty()) {
                    const size_t pick = can[opChoice[static_cast<size_t>(b)] % can.size()];
                    const LayerIdE add = q[pick];
                    q.erase(q.begin() + static_cast<long>(pick));
                    const int off = (groupOf(add) == 1 || groupOf(add) == 2) ? entryOffset[static_cast<size_t>(b)] : 0;
                    if (off == 0) enter(first, add, true);
                    else pendings.emplace_back(first + off, add);
                    // Phase 19: the set's first track brings its figure on the 8-bar line after its bass took the block.
                    if (req.m_quickStart && bassNow && figure != LayerIdE::COUNT && figure != LayerIdE::BASS && b == figureBlock) {
                        const auto f2 = std::find(q.begin(), q.end(), figure);
                        if (f2 != q.end()) { pendings.emplace_back(first + 8, figure); q.erase(f2); }
                    }
                    // Phase 10: the build brings more than one in a block -- up to two hats or percs on the next 8- and
                    // 16-bar lines after the main entry's, while the block is under its density (Mix-Dok. 3, the Tool
                    // template: density 0.3 -> 0.6 at bar 33 -> 0.85 at 65 -> 1.0 at 97; the canon brings something in
                    // on most 8-bar lines of its first minutes: "The Bells" at 1, 9, 11, 17, 25, 33, 41, 65).
                    // (On the block's free 8-bar lines: bars 8 and 16 of it when the main entry is on its first bar, 8
                    // and 24 when it is on 16, 16 and 24 when on 8.)
                    int room = std::min(2, want - now - 1);
                    for (int line = 1; room > 0 && line <= 3; ++line) {
                        if (8 * line == off) continue;
                        const int at = first + 8 * line;
                        if (q.size() - (std::find(q.begin(), q.end(), figure) != q.end() ? 1u : 0u) <= 2 && first < half) break;
                        const auto next = std::find_if(q.begin(), q.end(), [](LayerIdE id) {
                            return (groupOf(id) == 1 || groupOf(id) == 2) && id != LayerIdE::ROLLING_HAT;
                        });
                        if (next == q.end()) break;
                        pendings.emplace_back(at, *next);
                        q.erase(next);
                        --room;
                    }
                } else if (now > want) {
                    LayerIdE gone = LayerIdE::COUNT;
                    if (removeLatest(s, ent, false, &gone)) op(first, OpKindE::REMOVE, gone);
                    else op(first, OpKindE::HOLD, LayerIdE::COUNT);
                } else {
                    // A swap within the clap-and-perc group, or a hold.
                    LayerIdE out = LayerIdE::COUNT, in = LayerIdE::COUNT;
                    if (swapChoice[static_cast<size_t>(b)]) {
                        for (LayerIdE id : ent) if (groupOf(id) == 2 && s.m_active[L(id)]) out = id;
                        for (size_t i = 0; i < q.size(); ++i) if (groupOf(q[i]) == 2) { in = q[i]; break; }
                    }
                    if (out != LayerIdE::COUNT && in != LayerIdE::COUNT && (q.size() > 2 || first >= half)) {
                        s.m_active[L(out)] = false;
                        q.erase(std::find(q.begin(), q.end(), in));
                        q.push_back(out);
                        enter(first, in, false);
                        op(first, OpKindE::SWAP, in, out);
                    } else {
                        op(first, OpKindE::HOLD, LayerIdE::COUNT);
                    }
                }
            }
            // --- the bars ---
            if (b == bodyFirst && form != FormTypeE::ENDLESS) s.m_density[L(LayerIdE::ROLLING_HAT)] = 1.0f;   // the full groove
            std::vector<NoteEvent> notes;
            for (int bar = first; bar < first + 32; ++bar) {
                const int in = bar - first;
                if (b == 0 && form != FormTypeE::ENDLESS) {
                    // Phase 20: the opening (Composer.h) -- never the kick alone. (Phase 11: a set's first track at half
                    // the distances.) What enters with the first bar comes without an operation of its own (the start's).
                    const int step = req.m_quickStart ? 4 : 8;
                    const auto rolling = [&](int at, bool logOp) {
                        if (bar != at) return;
                        s.m_active[L(LayerIdE::ROLLING_HAT)] = true;
                        s.m_density[L(LayerIdE::ROLLING_HAT)] = 0.5f;
                        eb[L(LayerIdE::ROLLING_HAT)] = bar;
                        if (logOp) op(bar, OpKindE::ADD, LayerIdE::ROLLING_HAT);
                    };
                    if (opening == OpeningE::ATMOSPHERE) {
                        if (bar == 0) {
                            enter(0, LayerIdE::TEXTURE, false);
                            if (introPerc != LayerIdE::COUNT) enter(0, introPerc, false);
                        }
                        if (bar == step) enter(bar, LayerIdE::CLOSED_HAT, true);
                        rolling(2 * step, true);
                    } else {
                        if (bar == 0) {
                            enter(0, LayerIdE::CLOSED_HAT, false);
                            if (introPerc != LayerIdE::COUNT) enter(0, introPerc, false);
                        }
                        if (opening == OpeningE::PERCUSSION_FIRST) rolling(0, false);
                        else rolling(step, true);
                    }
                    if (kickIn > 0 && bar == kickIn) enter(bar, LayerIdE::KICK, true);
                }
                if (outro && b < blocks - 1 && in % 8 == 0) {
                    LayerIdE gone = LayerIdE::COUNT;
                    if (removeLatest(s, ent, true, &gone, true) || removeLatest(s, ent, false, &gone, true)) op(bar, OpKindE::REMOVE, gone);
                    else if (in == 0) op(bar, OpKindE::HOLD, LayerIdE::COUNT);
                }
                if (form != FormTypeE::ENDLESS && bar == bars - 16) { s.m_active[L(LayerIdE::ROLLING_HAT)] = false; op(bar, OpKindE::REMOVE, LayerIdE::ROLLING_HAT); }
                // (Phase 20: the kick leaves kickLeaves bars before the end, the closed hat hatOut -- the ending above.)
                if (form != FormTypeE::ENDLESS && kickLeaves > 0 && kickLeaves < 32 && bar == bars - kickLeaves) { s.m_active[L(LayerIdE::KICK)] = false; op(bar, OpKindE::REMOVE, LayerIdE::KICK); }
                if (form != FormTypeE::ENDLESS && hatGoes && bar == bars - hatOut) { s.m_active[L(LayerIdE::CLOSED_HAT)] = false; op(bar, OpKindE::REMOVE, LayerIdE::CLOSED_HAT); }
                for (const auto& [at, layer] : pendings) if (bar == at) enter(bar, layer, true);
                BarSpec spec = emptyBar(bar, static_cast<double>(bar) * Bar, bpm);
                for (int i = 0; i < NumLayers; ++i) { spec.m_active[i] = s.m_active[i]; spec.m_density[i] = s.m_density[i]; }
                spec.m_variant = k.m_variant;
                const BarMods& m = mods[static_cast<size_t>(bar)];
                if (m.m_muteStep >= 0) spec.m_muteStep[m.m_muteStep] = true;
                if (m.m_ghostBoost > 1.0f) {
                    spec.m_density[L(LayerIdE::GHOST_KICK)] *= m.m_ghostBoost;
                    spec.m_density[L(LayerIdE::CLAP_GHOST)] *= m.m_ghostBoost;
                }
                const bool kickOut = m.m_dropout || inReduction(bar);
                if (kickOut) {
                    for (LayerIdE id : { LayerIdE::KICK, LayerIdE::GHOST_KICK, LayerIdE::BASS, LayerIdE::ACID }) spec.m_active[L(id)] = false;
                    if (inReduction(bar) && form == FormTypeE::PEAK) {
                        spec.m_active[L(LayerIdE::ROLLING_HAT)] = false;
                        for (float& d : spec.m_density) d *= 0.6f;
                    }
                }
                for (int i = 0; m.m_muteLayers != 0 && i < NumLayers; ++i) if ((m.m_muteLayers >> i) & 1u) spec.m_active[i] = false;
                if (redReturn >= 0 && redCut && bar == redReturn - 1) for (bool& a : spec.m_active) a = false;
                if (inReduction(bar)) spec.m_fills = false;
                realizeBar(plan, spec, notes);
            }
            // --- the corridor ---
            std::vector<NoteEvent> local = notes;
            for (NoteEvent& n : local) n.m_beat -= static_cast<double>(first) * Bar;
            const std::vector<BarProfile> prof32 = barProfiles(local, 0, 32, bands);
            const CorridorStats cs = corridorOf(prof32, 0, 32);
            const float ds = (cs.m_similarity - prof.m_simTarget) / 0.04f, dm = (cs.m_micro - prof.m_microTarget) / 0.6f;
            k.m_dist = ds * ds + dm * dm;
            if (k.m_dist < best.m_dist) {
                k.m_notes = std::move(notes);
                k.m_after = s;
                k.m_queue = q;
                k.m_entered = ent;
                k.m_ops = ops;
                k.m_entryBar = eb;
                best = std::move(k);
            }
        }
        sc.m_notes.insert(sc.m_notes.end(), best.m_notes.begin(), best.m_notes.end());
        sc.m_ops.insert(sc.m_ops.end(), best.m_ops.begin(), best.m_ops.end());
        st = best.m_after;
        queue = best.m_queue;
        entered = best.m_entered;
        entryBar = best.m_entryBar;
        chosenVariant[static_cast<size_t>(b)] = static_cast<int>(best.m_variant);
    }
    {
        BlockOp end;
        end.m_beat = static_cast<double>(bars) * Bar;
        end.m_kind = OpKindE::END;
        sc.m_ops.push_back(end);
    }
    const auto has = [&](LayerIdE id) { return entryBar[L(id)] >= 0; };
    const auto entryBeat = [&](LayerIdE id) { return static_cast<double>(std::max(0, entryBar[L(id)])) * Bar; };

    // ---------------------------------------------------------------- the reduction's swell, cloud, markers
    const int noiseLevel = pid(ModuleE::PERC, 11, perc::LEVEL), noiseCut = pid(ModuleE::PERC, 11, perc::CUTOFF);
    if (redLen > 0) {
        BlockOp ko;
        ko.m_beat = static_cast<double>(redStart) * Bar;
        ko.m_kind = OpKindE::KICK_OUT;
        ko.m_layer = L(LayerIdE::KICK);
        sc.m_ops.push_back(ko);
        BlockOp re = ko;
        re.m_beat = static_cast<double>(redReturn) * Bar;
        re.m_kind = OpKindE::RETURN;
        sc.m_ops.push_back(re);
        sc.m_markers.push_back(Marker{ static_cast<double>(redStart) * Bar, "Reduction" });
        sc.m_markers.push_back(Marker{ static_cast<double>(redReturn) * Bar, "Return" });
        // The swell of noise over the last 8 or 16 bars before the return (the cut bar excepted).
        const int swell = std::min(redLen, redLen >= 16 ? 16 : 8);
        const int from = redReturn - swell;
        for (int bar = from; bar < redReturn - (redCut ? 1 : 0); ++bar) {
            NoteEvent n;
            n.m_beat = static_cast<double>(bar) * Bar;
            n.m_length = 4.0;
            n.m_part = percPart(11);
            n.m_pitch = 49;
            n.m_velocity = 0.9f;
            sc.m_notes.push_back(n);
        }
        sc.m_gestures.push_back(knobs.ramp(noiseLevel, static_cast<double>(from) * Bar, static_cast<double>(swell - 1) * Bar, -36.0f, -12.0f, GestureShapeE::EASE_IN, 0));
        sc.m_gestures.push_back(knobs.ramp(noiseCut, static_cast<double>(from) * Bar, static_cast<double>(swell - 1) * Bar, 600.0f, 6000.0f, GestureShapeE::EASE_IN, 1));
        sc.m_gestures.push_back(knobs.home(noiseLevel, static_cast<double>(redReturn) * Bar));
        sc.m_gestures.push_back(knobs.home(noiseCut, static_cast<double>(redReturn) * Bar));
        // The cloud of the ping's and the chord's last seconds rises through a reduction of eight bars or more.
        const int cloudLevel = pid(ModuleE::CLOUD, 0, cloud::LEVEL);
        if (redLen >= 8 && ((has(LayerIdE::PING) && entryBar[L(LayerIdE::PING)] < redStart) || (has(LayerIdE::CHORD) && entryBar[L(LayerIdE::CHORD)] < redStart))) {
            sc.m_gestures.push_back(knobs.ramp(cloudLevel, static_cast<double>(redStart) * Bar, static_cast<double>(redLen) * Bar, -60.0f, -3.0f, GestureShapeE::EASE_IN, 0));
            sc.m_gestures.push_back(knobs.ramp(cloudLevel, static_cast<double>(redReturn) * Bar, 32.0, -3.0f, -60.0f, GestureShapeE::EASE_OUT, 0));
            sc.m_gestures.push_back(knobs.home(cloudLevel, static_cast<double>(redReturn + 8) * Bar));
        }
    }
    // Phase 8: a swell of noise over the eight bars into the main landing (Raw 0.6, Ostgut 0.45, Hypnotic 0.35, Dub 0.25).
    if (mainLanding > 0) {
        float pSwell = 0.0f;
        static const float Swell[4] = { 0.35f, 0.45f, 0.25f, 0.6f };
        for (int s = 0; s < 4; ++s) pSwell += prof.m_styleMix[s] * Swell[s];
        if (er.uniform() < pSwell) {
            const int from = mainLanding - 8;
            for (int bar = from; bar < mainLanding; ++bar) {
                NoteEvent n;
                n.m_beat = static_cast<double>(bar) * Bar;
                n.m_length = 4.0;
                n.m_part = percPart(11);
                n.m_pitch = 49;
                n.m_velocity = 0.9f;
                sc.m_notes.push_back(n);
            }
            sc.m_gestures.push_back(knobs.ramp(noiseLevel, static_cast<double>(from) * Bar, 7.0 * Bar, -36.0f, -14.0f, GestureShapeE::EASE_IN, 0));
            sc.m_gestures.push_back(knobs.ramp(noiseCut, static_cast<double>(from) * Bar, 7.0 * Bar, 600.0f, 6000.0f, GestureShapeE::EASE_IN, 1));
            sc.m_gestures.push_back(knobs.home(noiseLevel, static_cast<double>(mainLanding) * Bar));
            sc.m_gestures.push_back(knobs.home(noiseCut, static_cast<double>(mainLanding) * Bar));
            moments.emplace_back(from, "a swell into the landing" + fmtBars(8));
        }
    }
    for (int b = 0; b < blocks; ++b) {
        const bool intro = b < bodyFirst, outro = b >= bodyEnd;
        sc.m_markers.push_back(Marker{ static_cast<double>(b) * 32.0 * Bar,
                                     intro ? (b == 0 ? std::string("Intro") : std::string("Intro 2")) : outro ? std::string("Outro")
                                           : "Block " + std::to_string(b - bodyFirst + 1) });
    }

    // ---------------------------------------------------------------- automation
    const int hatsCut = pid(ModuleE::MIX, 0, mix::HATS_CUT);
    const int lowCut = pid(ModuleE::MIX, 0, mix::LOW_CUT);
    const double introEnd = static_cast<double>(bodyFirst) * 32.0 * Bar, outroStart = static_cast<double>(bodyEnd) * 32.0 * Bar;
    // Intro: the hats' bus opens from 1.5 kHz (Dok. 8.5: "Perc-Bus LP 800 Hz -> offen"); outro: it closes in the last 16 bars.
    if (form != FormTypeE::ENDLESS) {
        // (Phase 20: from the hats' entry -- the first bar, but in the atmosphere's opening.)
        const double open = opening == OpeningE::ATMOSPHERE ? (req.m_quickStart ? 4.0 : 8.0) * Bar : 0.0;
        const double over = req.m_quickStart ? 8.0 * Bar : introEnd - open;
        // (Phase 20: from 4 kHz -- from 1.5 kHz the hats lay 30 to 40 dB under the kick for the intro's first half, and
        // a drums opening sounded as the kick alone.)
        sc.m_gestures.push_back(knobs.ramp(hatsCut, open, over, 4000.0f, knobs.get(hatsCut), GestureShapeE::LINEAR, 0));
        sc.m_gestures.push_back(knobs.ramp(hatsCut, static_cast<double>(bars - 16) * Bar, 16.0 * Bar, knobs.get(hatsCut), 2500.0f, GestureShapeE::LINEAR, 0));
    }
    // The rumble with the body (p 0.7; PLAN 7.2's intro is "Kick (+Rumble)"): out in the intro and the outro, in on the
    // body's first bar -- the low end arrives where a set swaps it, and the edges of a track are lighter, as the
    // references' are (their outros 4 to 19 dB under the body, ours 1 to 4 with the rumble through, 27.09.2026).
    if (form != FormTypeE::ENDLESS && uRumble < 0.7f) {
        const int rl = pid(ModuleE::RUMBLE, 0, rumble::LEVEL);
        sc.m_gestures.push_back(knobs.step(rl, 0.0, -60.0f));
        sc.m_gestures.push_back(knobs.step(rl, introEnd, knobs.get(rl)));
        sc.m_gestures.push_back(knobs.step(rl, outroStart, -60.0f));
    }
    // Macro: the rumble's hall and the stab's brightness grow by a fifth over the track (Dok. 8.5, "Track-Drift").
    {
        const int rd = pid(ModuleE::RUMBLE, 0, rumble::DECAY), cb = pid(ModuleE::CHORD, 0, chord::BRIGHT);
        sc.m_gestures.push_back(knobs.ramp(rd, 0.0, static_cast<double>(bars) * Bar, knobs.get(rd), knobs.get(rd) * 1.2f, GestureShapeE::LINEAR, 1));
        if (has(LayerIdE::CHORD))
            sc.m_gestures.push_back(knobs.ramp(cb, entryBeat(LayerIdE::CHORD), static_cast<double>(bars) * Bar - entryBeat(LayerIdE::CHORD),
                                             knobs.get(cb), knobs.get(cb) * 1.2f, GestureShapeE::LINEAR, 1));
    }
    // Micro: the rolling hat's decay moves a few per cent every 16 bars of the body; the open hat's grows from 100 to
    // 400 ms over the 16 bars after it enters; a chord enters 9 dB under its level and steps up every four bars.
    Rng mr = streamOf(tseed, cur, unit, "hands");
    {
        const int rollDecay = pid(ModuleE::PERC, 1, perc::NOISE_DECAY), ohDecay = pid(ModuleE::PERC, 2, perc::NOISE_DECAY);
        const float d0 = knobs.get(rollDecay);
        for (int bar = bodyFirst * 32; bar < bodyEnd * 32; bar += 16) {
            const float a = d0 * (1.0f + 0.08f * mr.bipolar()), z = d0 * (1.0f + 0.08f * mr.bipolar());
            sc.m_gestures.push_back(knobs.ramp(rollDecay, static_cast<double>(bar) * Bar, 16.0 * Bar, a, z, GestureShapeE::MINIMUM_JERK, 1));
        }
        if (has(LayerIdE::OPEN_HAT))
            sc.m_gestures.push_back(knobs.ramp(ohDecay, entryBeat(LayerIdE::OPEN_HAT), 16.0 * Bar, 100.0f, 400.0f, GestureShapeE::LINEAR, 1));
        if (has(LayerIdE::CHORD)) {
            const int cl = pid(ModuleE::CHORD, 0, chord::LEVEL);
            const float k = knobs.get(cl);
            for (int s = 0; s < 4; ++s) {
                const float v = k - 9.0f + 3.0f * static_cast<float>(s);
                sc.m_gestures.push_back(knobs.step(cl, entryBeat(LayerIdE::CHORD) + 16.0 * s, v));
            }
        }
    }
    // The throws: from the third beat of the phrase's last bar a ping, a stab or the hats open into the echo, the
    // feedback climbs to the edge and falls back over two bars.
    {
        const int fbId = pid(ModuleE::DUB, 0, dub::FEEDBACK), pingEcho = pid(ModuleE::DUB, 0, dub::PING_SEND);
        const int hatsEcho = pid(ModuleE::DUB, 0, dub::HATS_SEND), chordDub = pid(ModuleE::CHORD, 0, chord::DUB_SEND);
        for (const Throw& t : throws) {
            const double at = static_cast<double>(t.m_bar) * Bar + 2.0;
            const float u = er.uniform();
            const bool ping = has(LayerIdE::PING) && entryBar[L(LayerIdE::PING)] <= t.m_bar && u < 0.5f;
            const bool stab = !ping && has(LayerIdE::CHORD) && entryBar[L(LayerIdE::CHORD)] <= t.m_bar && u < 0.8f;
            const float fb = knobs.get(fbId);
            sc.m_gestures.push_back(knobs.ramp(fbId, at, 2.0, fb, 0.85f, GestureShapeE::EASE_IN, 0));
            sc.m_gestures.push_back(knobs.ramp(fbId, at + 2.0, 8.0, 0.85f, fb, GestureShapeE::EASE_OUT, 0));
            sc.m_gestures.push_back(knobs.home(fbId, at + 10.0));
            const int send = ping ? pingEcho : stab ? chordDub : hatsEcho;
            const float open = ping || stab ? 0.9f : 0.5f;
            sc.m_gestures.push_back(knobs.step(send, at, open));
            sc.m_gestures.push_back(knobs.home(send, at + 2.0));
        }
    }
    // The group high pass: from 20 Hz to 200 .. 400 Hz over the eight bars before the line, back on it.
    for (int line : hpSweeps) {
        const float top = 200.0f + 200.0f * er.uniform();
        sc.m_gestures.push_back(knobs.ramp(lowCut, static_cast<double>(line - 8) * Bar, 8.0 * Bar, 20.0f, top, GestureShapeE::EASE_IN, 1));
        sc.m_gestures.push_back(knobs.home(lowCut, static_cast<double>(line) * Bar));
    }
    // Phase 8: the waves on the figure's knobs and the hats' bus (Dok. "Automation auf drei Zeitskalen", the meso scale:
    // "filter cutoff creeping open over 16 to 32 bars"; Mills' rides "faded in on mixer"). After a landing a wave drops
    // them to its low, then builds: a creep up to its landing (0.5), an arch over its middle (0.3), or an opening that
    // closes slowly to the landing (0.2); after the last wave they go home over eight bars. Offsets in the knob's
    // normalised range from the track's own value, scaled by the wave's depth (the main landing's the deepest).
    {
        struct WaveKnob { int m_id; float m_low, m_high; double m_from; float m_scale = 1.0f; };
        std::vector<WaveKnob> wk;
        if (figure != LayerIdE::COUNT && has(figure)) {
            const double fb = entryBeat(figure);
            switch (figure) {
            case LayerIdE::ACID:
                wk.push_back({ pid(ModuleE::ACID, 0, synth::CUTOFF), -0.16f, 0.10f, fb, figDepth });
                wk.push_back({ pid(ModuleE::ACID, 0, synth::ENV_AMOUNT), -0.08f, 0.10f, fb, figDepth });
                wk.push_back({ pid(ModuleE::ACID, 0, synth::RESONANCE), -0.05f, 0.12f, fb, figDepth });
                break;
            case LayerIdE::CHORD:
                wk.push_back({ pid(ModuleE::CHORD, 0, chord::BRIGHT), -0.18f, 0.08f, fb, figDepth });
                wk.push_back({ pid(ModuleE::CHORD, 0, chord::BAND), -0.10f, 0.10f, fb, figDepth });
                break;
            case LayerIdE::PING:
                wk.push_back({ pid(ModuleE::PING, 0, ping::INDEX), -0.12f, 0.12f, fb, figDepth });
                wk.push_back({ pid(ModuleE::PING, 0, ping::BAND), -0.12f, 0.10f, fb, figDepth });
                break;
            case LayerIdE::BASS:
                wk.push_back({ pid(ModuleE::BASS, 0, synth::CUTOFF), -0.10f, 0.08f, fb, figDepth });
                break;
            default:
                break;
            }
        }
        // The hats' bus: its low pass down to 2.9 kHz at the most, its level 5 dB.
        wk.push_back({ hatsCut, -std::min(0.42f, 0.30f * hatDepth), 0.0f, 0.0 });
        const int hatsLevel = pid(ModuleE::MIX, 0, mix::HATS_LEVEL);
        wk.push_back({ hatsLevel, -0.08f * hatDepth, 0.0f, 0.0 });
        const auto real = [&](int id, float off) {
            return p.fromNormalised(id, std::clamp(p.toNormalised(id, knobs.get(id)) + off, 0.0f, 1.0f));
        };
        for (const WaveKnob& k : wk) {
            float at = 0.0f;
            bool any = false;
            for (const Wave& w : waves) {
                const double a = static_cast<double>(w.m_start) * Bar, len = static_cast<double>(w.m_land - w.m_start) * Bar;
                if (a < k.m_from) continue;
                // (The wave over a kick-out keeps the hats open: the kick-out is its down already, 28.09.2026.)
                const bool open = (k.m_id == hatsCut || k.m_id == hatsLevel) && redLen > 0 && w.m_land == redReturn;
                const float lo = open ? 0.0f : k.m_low * k.m_scale * w.m_depth, hi = open ? 0.0f : k.m_high * k.m_scale * w.m_depth;
                any = true;
                if (w.m_shape == 0) {
                    sc.m_gestures.push_back(knobs.ramp(k.m_id, a, 2.0 * Bar, real(k.m_id, at), real(k.m_id, lo), GestureShapeE::EASE_OUT, 0));
                    sc.m_gestures.push_back(knobs.ramp(k.m_id, a + 2.0 * Bar, len - 2.0 * Bar, real(k.m_id, lo), real(k.m_id, hi), GestureShapeE::EASE_IN, 0));
                    at = hi;
                } else if (w.m_shape == 1) {
                    sc.m_gestures.push_back(knobs.ramp(k.m_id, a, len / 2.0, real(k.m_id, at), real(k.m_id, hi), GestureShapeE::MINIMUM_JERK, 0));
                    sc.m_gestures.push_back(knobs.ramp(k.m_id, a + len / 2.0, len / 2.0, real(k.m_id, hi), real(k.m_id, lo), GestureShapeE::MINIMUM_JERK, 0));
                    at = lo;
                } else {
                    sc.m_gestures.push_back(knobs.ramp(k.m_id, a, 4.0 * Bar, real(k.m_id, at), real(k.m_id, hi), GestureShapeE::EASE_OUT, 0));
                    sc.m_gestures.push_back(knobs.ramp(k.m_id, a + 4.0 * Bar, len - 4.0 * Bar, real(k.m_id, hi), real(k.m_id, lo), GestureShapeE::EASE_IN, 0));
                    at = lo;
                }
            }
            if (any) {
                const double e = static_cast<double>(waves.back().m_land) * Bar;
                sc.m_gestures.push_back(knobs.ramp(k.m_id, e, 8.0 * Bar, real(k.m_id, at), knobs.get(k.m_id), GestureShapeE::MINIMUM_JERK, 0));
            }
        }
    }
    // The edges filtered (the profile's chance; Dok. 8.5's group high pass): the intro opens from 250 Hz over its first 16 bars, the
    // last 16 bars close to 300 Hz. A track's loudness moves at its edges as the references' do (their LRA 3 to 5 LU, ours
    // 1 to 2 with the kick at full from the first bar to the last, 27.09.2026).
    if (form != FormTypeE::ENDLESS && uEdges < prof.m_edgeChance) {
        sc.m_gestures.push_back(knobs.ramp(lowCut, 0.0, 16.0 * Bar, 250.0f, 20.0f, GestureShapeE::EASE_IN, 1));
        sc.m_gestures.push_back(knobs.ramp(lowCut, static_cast<double>(bars - 16) * Bar, 16.0 * Bar, 20.0f, 300.0f, GestureShapeE::EASE_IN, 1));
    }
    // Meso: two hands on a 16-beat grid over the body, on the filters and sends of what plays; their centre follows the
    // density (the energy of the moment).
    {
        std::vector<HandKnob> hk;
        const auto knob = [&](int id, float range, float rest, float peak, float weight, double from) {
            HandKnob h;
            h.m_param = id;
            const float base = knobs.offset(id, knobs.get(id));
            h.m_low = base - range;
            h.m_high = base + range;
            h.m_atRest = base + rest;
            h.m_atPeak = base + peak;
            h.m_scatter = range * 0.35f;
            h.m_weight = weight;
            h.m_from = std::max(from, introEnd);
            h.m_until = outroStart;
            hk.push_back(h);
        };
        knob(pid(ModuleE::RUMBLE, 0, rumble::DRIVE), 0.12f, -0.04f, 0.06f, 1.0f, 0.0);
        knob(pid(ModuleE::MIX, 0, mix::PERC_CUT), 0.25f, -0.15f, 0.0f, 1.0f, 0.0);
        knob(pid(ModuleE::SPACE, 0, space::LEVEL), 0.06f, -0.02f, 0.03f, 0.7f, 0.0);
        knob(pid(ModuleE::DUB, 0, dub::ECHO_RETURN), 0.08f, -0.03f, 0.03f, 0.7f, 0.0);
        // (The figure's knobs are the waves'; a knob follows the gesture that started last, so the hands leave them.)
        if (has(LayerIdE::CHORD) && figure != LayerIdE::CHORD) knob(pid(ModuleE::CHORD, 0, chord::BAND), 0.2f, -0.05f, 0.1f, 1.5f, entryBeat(LayerIdE::CHORD) + 64.0);
        if (has(LayerIdE::ACID) && figure != LayerIdE::ACID) {
            knob(pid(ModuleE::ACID, 0, synth::CUTOFF), 0.25f, -0.05f, 0.18f, 2.0f, entryBeat(LayerIdE::ACID));
            knob(pid(ModuleE::ACID, 0, synth::RESONANCE), 0.15f, 0.0f, 0.08f, 1.0f, entryBeat(LayerIdE::ACID));
        }
        if (has(LayerIdE::PING) && figure != LayerIdE::PING) {
            knob(pid(ModuleE::PING, 0, ping::BAND), 0.15f, -0.05f, 0.08f, 1.2f, entryBeat(LayerIdE::PING));
            knob(pid(ModuleE::PING, 0, ping::INDEX), 0.15f, -0.05f, 0.08f, 1.0f, entryBeat(LayerIdE::PING));
        }
        if (has(LayerIdE::DRONE)) knob(pid(ModuleE::DRONE, 0, drone::CUTOFF), 0.15f, -0.05f, 0.08f, 0.8f, entryBeat(LayerIdE::DRONE));
        HandStyle hs;
        const auto energyAt = [&](double beat) {
            const int b = std::clamp(static_cast<int>(beat / (32.0 * Bar)), 0, blocks - 1);
            return density[static_cast<size_t>(b)];
        };
        playHands(sc, hk, hs, energyAt, introEnd, outroStart, mr);
    }

    // ---------------------------------------------------------------- the loudest part, the length, what the track tells
    int peakBlock = bodyFirst;
    if (form == FormTypeE::ENDLESS) peakBlock = blocks / 2;
    else if (form == FormTypeE::PEAK && redReturn >= 0) {
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
    {
        LevelMark lm{ 0.0, static_cast<double>(peakBlock) * 32.0 * Bar, prof.m_peakLufs, 0.0f };
        for (int sm = 0; sm < 4; ++sm) lm.m_styleMix[static_cast<size_t>(sm)] = prof.m_styleMix[sm];   // (the parts' windows, Leveler.h)
        if (introPerc != LayerIdE::COUNT) lm.m_front = layerLane(plan, introPerc);   // (Phase 20: the opening's voice, up front)
        sc.m_levels.push_back(lm);
    }
    sc.m_lengthBeats = static_cast<double>(bars) * Bar;
    sc.sort();

    if (info != nullptr) {
        TrackInfo& t = *info;
        t = TrackInfo{};
        t.m_form = form;
        t.m_style = prof.m_name;
        t.m_bpm = bpm;
        t.m_key = key;
        t.m_scale = scale;
        t.m_monotonic = monotonic;
        t.m_subOwns = subOwns;
        t.m_bars = bars;
        t.m_introBars = bodyFirst * 32;
        t.m_outroBar = bodyEnd * 32;
        t.m_bassBar = bodyFirst * 32;
        if (redLen > 0) { t.m_reductions.push_back(redStart); t.m_returns.push_back(redReturn); }
        t.m_peakBar = peakBlock * 32;
        t.m_camelot = monotonic ? std::string("monotonic") : camelotOf(key);
        for (LayerIdE id : entered) if (std::find(t.m_layers.begin(), t.m_layers.end(), L(id)) == t.m_layers.end()) t.m_layers.push_back(L(id));
        const std::vector<BarProfile> all = barProfiles(sc.m_notes, 0, bars, bands);
        const CorridorStats cs = corridorOf(all, bodyFirst * 32, bodyEnd * 32);
        t.m_similarity = cs.m_similarity;
        t.m_micro = cs.m_micro;
        t.m_density = cs.m_density;
        t.m_archetype = static_cast<int>(arch);
        t.m_opening = static_cast<int>(opening);
        t.m_kickBar = kickIn;
        t.m_kickLeaves = kickLeaves;
        for (const SoundPick& k : picks) {
            const std::vector<SoundPreset>& list = factoryPresets(static_cast<ModuleE>(k.m_module));
            if (k.m_preset >= 0 && k.m_preset < static_cast<int>(list.size())) t.m_groups.push_back(list[static_cast<size_t>(k.m_preset)].m_group);
        }
        if (figure != LayerIdE::COUNT && has(figure)) {
            t.m_figure = L(figure);
            t.m_figureBar = entryBar[L(figure)];
            t.m_figureBars = plan.m_figBars;
        }
        for (const Wave& w : waves) if (w.m_land < bars) t.m_landings.push_back(w.m_land);   // (the Endless's last wave ends with it)
        // (A figure that had to wait for the second half was not there to drop out before it came.)
        for (const auto& mo : moments)
            if (mo.second.find("figure") == std::string::npos || (t.m_figureBar >= 0 && mo.first >= t.m_figureBar)) t.m_moments.push_back(mo);
        std::sort(t.m_moments.begin(), t.m_moments.end());
    }
    return sc;
}

} // namespace tot
