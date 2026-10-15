/**
 * @file Style.cpp
 * @brief The four profiles, their morph and the axes.
 */
#include "tot/compose/Style.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <string>

namespace tot {

namespace {

using LT = LayerIdE;   ///< The layers, shortly.

/// Designated initialisers (C++20): every value stands by its name, in the order of Style.h.
const StyleProfile Profiles[] = {
    // Hypnotic: the centre. The rumble owns the low end (0.7); a processed, rolling 909 kick; two or three cyclic layers;
    // the ping's ostinato, drones and texture; the chord seldom. Tool 0.5, Endless 0.3, Peak 0.2 (PLAN 2.8).
    {
        .m_name = "Hypnotic",
        .m_bpmLow = 128.0f,
        .m_bpmHigh = 133.0f,
        .m_arcWeight = 0.5f,
        .m_peakWeight = 0.2f,
        .m_endlessWeight = 0.3f,
        .m_subChance = 0.3f,
        .m_intro64Chance = 0.4f,
        .m_blocksLow = 6,
        .m_blocksHigh = 8,
        .m_pool = { { LT::GHOST_KICK, 0.8f }, { LT::OPEN_HAT, 0.9f }, { LT::RIDE, 0.4f }, { LT::CLAP_A, 0.3f }, { LT::SHAKER, 0.6f },
                  { LT::TOM_CONGA, 0.5f }, { LT::RIM, 0.5f }, { LT::PING, 0.9f }, { LT::ACID, 0.05f }, { LT::CHORD, 0.2f },
                  { LT::DRONE, 0.6f }, { LT::TEXTURE, 0.5f } },
        .m_mutation = 0.25f,
        .m_motionScale = 1.0f,
        .m_reroll = 0.15f,
        .m_polymeterChance = 0.8f,
        .m_swingLow = 51.0f,
        .m_swingHigh = 56.0f,
        .m_fillChance = 0.15f,
        .m_toolReductionChance = 0.5f,
        .m_edgeChance = 0.5f,
        .m_maxReduction = 32,
        .m_eventRate = 0.35f,
        .m_throwShare = 0.5f,
        .m_densityCap = 9,
        .m_simTarget = 0.90f,   // the references' audio bar similarity + 0.05: the score's runs 0.04 .. 0.07 over the audio's (27.09.2026)
        .m_microTarget = 1.0f,
        // Fitted on the loudest minute of three composed tracks against the references (27.09.2026): tilt 11.8, ping -5,
        // pads -10 (at its bound; -6 taken), room +2, echo and plate -4.
        .m_recipe = { { "master.tilt", 12.0f }, { "chord.level", -14.0f }, { "drone.level", -22.0f },
                    { "space.level", 0.0f }, { "ping.level", -15.5f }, { "dub.echo_return", -6.0f }, { "dub.plate_return", 3.0f } },
        .m_sounds = { { "kick.pitch_decay", 14.0f, 24.0f }, { "kick.amp_decay", 300.0f, 450.0f }, { "kick.drive", 0.25f, 0.5f },
                    { "rumble.decay", 1.5f, 2.8f }, { "rumble.drive", 4.0f, 8.0f }, { "ping.ratio", 1.2f, 2.2f },
                    { "ping.decay", 90.0f, 220.0f }, { "ping.band", 600.0f, 1500.0f }, { "dub.echo_time", 0.5f, 4.49f },
                    { "dub.feedback", 0.2f, 0.35f }, { "space.decay", 0.9f, 1.8f }, { "drone.sweep_bars", 32.0f, 64.0f },
                    { "perc1.noise_type", -0.49f, 1.49f } },
        .m_peakLufs = -10.0f,
        .m_styleMix = { 1.0f, 0.0f, 0.0f, 0.0f },
    },
    // Ostgut: the clap on 2 and 4, the ride's eighths, the open hat on the offbeat, one cyclic layer; dub stabs sparingly,
    // a 303 seldom. Tool 0.6, Peak 0.4.
    {
        .m_name = "Ostgut",
        .m_bpmLow = 125.0f,
        .m_bpmHigh = 131.0f,
        .m_arcWeight = 0.6f,
        .m_peakWeight = 0.4f,
        .m_endlessWeight = 0.0f,
        .m_subChance = 0.5f,
        .m_intro64Chance = 0.4f,
        .m_blocksLow = 6,
        .m_blocksHigh = 8,
        .m_pool = { { LT::GHOST_KICK, 0.6f }, { LT::OPEN_HAT, 0.95f }, { LT::RIDE, 0.8f }, { LT::CLAP_A, 0.9f }, { LT::SHAKER, 0.5f },
                  { LT::TOM_CONGA, 0.5f }, { LT::RIM, 0.5f }, { LT::PING, 0.3f }, { LT::ACID, 0.25f }, { LT::CHORD, 0.4f },
                  { LT::DRONE, 0.2f }, { LT::TEXTURE, 0.2f } },
        .m_mutation = 0.25f,
        .m_motionScale = 1.0f,
        .m_reroll = 0.08f,   // 0.15 measured 0.909 .. 0.965 against 0.955 (27.09.2026)
        .m_polymeterChance = 0.5f,
        .m_swingLow = 50.0f,
        .m_swingHigh = 55.0f,
        .m_fillChance = 0.25f,
        .m_toolReductionChance = 0.5f,
        .m_edgeChance = 0.5f,
        .m_maxReduction = 32,
        .m_eventRate = 0.3f,
        .m_throwShare = 0.4f,
        .m_densityCap = 9,
        .m_simTarget = 0.99f,
        .m_microTarget = 1.0f,
        // Fitted as Hypnotic's: tilt 12.5, perc -4.7, hats -1.9, room +1.6.
        .m_recipe = { { "master.tilt", 12.5f }, { "space.level", 3.5f }, { "mix.perc_level", -10.0f },
                    { "mix.hats_level", -5.5f } },
        .m_sounds = { { "kick.pitch_decay", 14.0f, 22.0f }, { "kick.amp_decay", 250.0f, 400.0f }, { "kick.drive", 0.3f, 0.55f },
                    { "rumble.decay", 1.2f, 2.2f }, { "rumble.drive", 4.0f, 8.0f }, { "chord.detune", 6.0f, 15.0f },
                    { "chord.band", 300.0f, 480.0f }, { "dub.echo_time", 0.5f, 4.49f }, { "dub.feedback", 0.2f, 0.3f },
                    { "space.decay", 0.9f, 1.6f }, { "acid.cutoff", 400.0f, 900.0f }, { "perc1.noise_type", -0.49f, 1.49f } },
        .m_peakLufs = -9.5f,
        .m_styleMix = { 0.0f, 1.0f, 0.0f, 0.0f },
    },
    // Dub: the sub bass owns the low end (0.8), a soft kick, straight time, motion from the echo, the hats reduced; the
    // dub chord with its whole chain, vinyl crackle. Tool 0.7, Endless 0.2, Peak 0.1.
    {
        .m_name = "Dub",
        .m_bpmLow = 125.0f,
        .m_bpmHigh = 130.0f,
        .m_arcWeight = 0.7f,
        .m_peakWeight = 0.1f,
        .m_endlessWeight = 0.2f,
        .m_subChance = 0.8f,
        .m_intro64Chance = 0.5f,
        .m_blocksLow = 6,
        .m_blocksHigh = 8,
        .m_pool = { { LT::GHOST_KICK, 0.4f }, { LT::OPEN_HAT, 0.7f }, { LT::RIDE, 0.2f }, { LT::CLAP_A, 0.3f }, { LT::SHAKER, 0.6f },
                  { LT::TOM_CONGA, 0.2f }, { LT::RIM, 0.5f }, { LT::PING, 0.3f }, { LT::ACID, 0.0f }, { LT::CHORD, 0.95f },
                  { LT::DRONE, 0.3f }, { LT::TEXTURE, 0.7f } },
        .m_mutation = 0.125f,
        .m_motionScale = 0.8f,
        .m_reroll = 0.12f,   // 0.25 measured 0.88 .. 0.91 against 0.93 (27.09.2026)
        .m_polymeterChance = 0.3f,
        .m_swingLow = 50.0f,
        .m_swingHigh = 51.0f,
        .m_fillChance = 0.1f,
        .m_toolReductionChance = 0.4f,
        .m_edgeChance = 0.6f,
        .m_maxReduction = 32,
        .m_eventRate = 0.45f,
        .m_throwShare = 0.8f,
        .m_densityCap = 8,
        .m_simTarget = 0.97f,
        .m_microTarget = 1.0f,
        // Fitted as Hypnotic's: tilt 13.3 (the chords' 1 .. 5 kHz), hats -10 (at its bound; -8 taken), perc -8.4, pads -6.2,
        // echo -3.6, room +2.1 -- the hats reduced, the stabs bright.
        .m_recipe = { { "master.tilt", 13.0f }, { "mix.hats_level", -14.0f }, { "mix.perc_level", -15.0f }, { "chord.level", -11.0f },
                    { "drone.level", -19.0f }, { "dub.echo_return", -6.5f }, { "space.level", 5.0f } },
        .m_sounds = { { "kick.pitch_decay", 16.0f, 26.0f }, { "kick.amp_decay", 350.0f, 550.0f }, { "kick.drive", 0.15f, 0.35f },
                    { "rumble.decay", 1.2f, 2.2f }, { "rumble.drive", 3.0f, 6.0f }, { "chord.detune", 6.0f, 15.0f },
                    { "chord.band", 300.0f, 480.0f }, { "chord.crush", 6.0f, 9.0f }, { "dub.echo_time", 0.5f, 4.49f },
                    { "dub.feedback", 0.25f, 0.4f }, { "space.decay", 1.0f, 2.0f }, { "texture.hum_hz", 49.0f, 51.0f },
                    { "kick.click_level", 0.0f, 0.25f }, { "chord.bright", 2500.0f, 4500.0f }, { "chord.dip", -4.0f, -2.0f },
                    { "chord.width", 0.8f, 0.95f } },
        .m_peakLufs = -11.5f,
        .m_styleMix = { 0.0f, 0.0f, 1.0f, 0.0f },
    },
    // Raw/Peak: the kick harder, distortion before the low pass, the rumble overdriven; denser, sixteenth hats, toms,
    // longer reductions; noise and metallic pings. Peak 0.6, Tool 0.4.
    {
        .m_name = "Raw Peak",
        .m_bpmLow = 131.0f,
        .m_bpmHigh = 136.0f,
        .m_arcWeight = 0.4f,
        .m_peakWeight = 0.6f,
        .m_endlessWeight = 0.0f,
        .m_subChance = 0.2f,
        .m_intro64Chance = 0.3f,
        .m_blocksLow = 6,
        .m_blocksHigh = 8,
        .m_pool = { { LT::GHOST_KICK, 0.7f }, { LT::OPEN_HAT, 0.9f }, { LT::RIDE, 0.5f }, { LT::CLAP_A, 0.7f }, { LT::SHAKER, 0.6f },
                  { LT::TOM_CONGA, 0.8f }, { LT::RIM, 0.6f }, { LT::PING, 0.3f }, { LT::ACID, 0.35f }, { LT::CHORD, 0.05f },
                  { LT::DRONE, 0.2f }, { LT::TEXTURE, 0.3f } },
        .m_mutation = 0.25f,
        .m_motionScale = 1.0f,
        .m_reroll = 0.3f,
        .m_polymeterChance = 0.5f,
        .m_swingLow = 50.0f,
        .m_swingHigh = 53.0f,
        .m_fillChance = 0.3f,
        .m_toolReductionChance = 0.6f,
        .m_edgeChance = 0.1f,   // Raw records are flat (LRA 1.25): filtered edges put it at 2.4 .. 4.8
        .m_maxReduction = 8,   // the Raw references cut short: 0.5 reductions a track, median 3 bars (PLAN 13.4)
        .m_eventRate = 0.3f,
        .m_throwShare = 0.3f,
        .m_densityCap = 10,
        .m_simTarget = 0.96f,
        .m_microTarget = 1.0f,
        // Fitted as Hypnotic's: tilt 4.4, room +7.4, hats +3.2.
        .m_recipe = { { "master.tilt", 4.5f }, { "space.level", 10.0f }, { "mix.hats_level", -0.5f } },
        .m_sounds = { { "kick.pitch_decay", 12.0f, 20.0f }, { "kick.amp_decay", 250.0f, 380.0f }, { "kick.drive", 0.5f, 0.85f },
                    { "rumble.decay", 1.4f, 2.4f }, { "rumble.drive", 7.0f, 12.0f }, { "ping.ratio", 2.0f, 3.5f },
                    { "ping.decay", 80.0f, 160.0f }, { "acid.cutoff", 400.0f, 900.0f }, { "dub.echo_time", 0.5f, 4.49f },
                    { "space.decay", 0.8f, 1.4f }, { "perc1.noise_type", -0.49f, 1.49f }, { "kick.clip", 1.0f, 1.0f } },
        .m_peakLufs = -9.5f,
        .m_styleMix = { 0.0f, 0.0f, 0.0f, 1.0f },
    },
};

/** @brief @p a to @p b at @p t. */
float lerp(float a, float b, float t) { return a + (b - a) * t; }
/** @brief @p a to @p b at @p t, rounded. */
int lerpi(int a, int b, float t) { return static_cast<int>(std::lround(lerp(static_cast<float>(a), static_cast<float>(b), t))); }

} // namespace

const StyleProfile& styleProfile(StyleE style)
{
    const int i = std::clamp(static_cast<int>(style), 0, static_cast<int>(StyleE::COUNT) - 1);
    return Profiles[i];
}

StyleProfile morphProfile(const StyleProfile& a, const StyleProfile& b, float t, const ParamStore& p)
{
    t = std::clamp(t, 0.0f, 1.0f);
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    StyleProfile m = t < 0.5f ? a : b;
    for (int k = 0; k < 4; ++k) m.m_styleMix[k] = lerp(a.m_styleMix[k], b.m_styleMix[k], t);
    m.m_bpmLow = lerp(a.m_bpmLow, b.m_bpmLow, t);
    m.m_bpmHigh = lerp(a.m_bpmHigh, b.m_bpmHigh, t);
    m.m_arcWeight = lerp(a.m_arcWeight, b.m_arcWeight, t);
    m.m_peakWeight = lerp(a.m_peakWeight, b.m_peakWeight, t);
    m.m_endlessWeight = lerp(a.m_endlessWeight, b.m_endlessWeight, t);
    m.m_subChance = lerp(a.m_subChance, b.m_subChance, t);
    m.m_intro64Chance = lerp(a.m_intro64Chance, b.m_intro64Chance, t);
    m.m_blocksLow = lerpi(a.m_blocksLow, b.m_blocksLow, t);
    m.m_blocksHigh = lerpi(a.m_blocksHigh, b.m_blocksHigh, t);
    m.m_mutation = lerp(a.m_mutation, b.m_mutation, t);
    m.m_motionScale = lerp(a.m_motionScale, b.m_motionScale, t);
    m.m_reroll = lerp(a.m_reroll, b.m_reroll, t);
    m.m_polymeterChance = lerp(a.m_polymeterChance, b.m_polymeterChance, t);
    m.m_swingLow = lerp(a.m_swingLow, b.m_swingLow, t);
    m.m_swingHigh = lerp(a.m_swingHigh, b.m_swingHigh, t);
    m.m_fillChance = lerp(a.m_fillChance, b.m_fillChance, t);
    m.m_toolReductionChance = lerp(a.m_toolReductionChance, b.m_toolReductionChance, t);
    m.m_edgeChance = lerp(a.m_edgeChance, b.m_edgeChance, t);
    m.m_maxReduction = t < 0.5f ? a.m_maxReduction : b.m_maxReduction;
    m.m_eventRate = lerp(a.m_eventRate, b.m_eventRate, t);
    m.m_throwShare = lerp(a.m_throwShare, b.m_throwShare, t);
    m.m_densityCap = lerpi(a.m_densityCap, b.m_densityCap, t);
    m.m_simTarget = lerp(a.m_simTarget, b.m_simTarget, t);
    m.m_microTarget = lerp(a.m_microTarget, b.m_microTarget, t);
    m.m_peakLufs = lerp(a.m_peakLufs, b.m_peakLufs, t);
    // The pool, layer by layer.
    std::map<int, float> ca, cb;
    for (const LayerChance& c : a.m_pool) ca[static_cast<int>(c.m_layer)] = c.m_chance;
    for (const LayerChance& c : b.m_pool) cb[static_cast<int>(c.m_layer)] = c.m_chance;
    m.m_pool.clear();
    for (int l = 0; l < NumLayers; ++l) {
        const auto ia = ca.find(l), ib = cb.find(l);
        if (ia == ca.end() && ib == cb.end()) continue;
        const float va = ia == ca.end() ? 0.0f : ia->second, vb = ib == cb.end() ? 0.0f : ib->second;
        m.m_pool.push_back({ static_cast<LayerIdE>(l), lerp(va, vb, t) });
    }
    // The recipes, knob by knob: a knob only one sets is at its default in the other.
    std::map<std::string, std::pair<float, float>> r;
    for (const SoundValue& v : a.m_recipe) {
        const int id = p.find(v.m_key);
        r[v.m_key] = { v.m_value, id >= 0 ? p.defaultValue(id) : v.m_value };
    }
    for (const SoundValue& v : b.m_recipe) {
        const int id = p.find(v.m_key);
        auto it = r.find(v.m_key);
        if (it == r.end()) r[v.m_key] = { id >= 0 ? p.defaultValue(id) : v.m_value, v.m_value };
        else it->second.second = v.m_value;
    }
    m.m_recipe.clear();
    for (const StyleProfile* s : { &a, &b })
        for (const SoundValue& v : s->m_recipe) {
            const auto it = r.find(v.m_key);
            if (it == r.end()) continue;
            m.m_recipe.push_back({ v.m_key, lerp(it->second.first, it->second.second, t) });
            r.erase(it);
        }
    // The sound ranges: a knob both draw is drawn between the interpolated bounds; one only one draws, as the nearer does.
    m.m_sounds.clear();
    const StyleProfile& near = t < 0.5f ? a : b;
    const StyleProfile& far = t < 0.5f ? b : a;
    for (const SoundRange& s : near.m_sounds) {
        SoundRange out = s;
        for (const SoundRange& o : far.m_sounds)
            if (std::strcmp(o.m_key, s.m_key) == 0) {
                const float tt = &near == &a ? t : 1.0f - t;
                out.m_low = lerp(s.m_low, o.m_low, tt);
                out.m_high = lerp(s.m_high, o.m_high, tt);
            }
        m.m_sounds.push_back(out);
    }
    return m;
}

StyleProfile axisProfile(const StyleProfile& base, float dubShare, float hypnoticShare, const ParamStore& p)
{
    StyleProfile s = base;
    if (dubShare > 0.0f) s = morphProfile(s, styleProfile(StyleE::DUB), dubShare, p);
    if (hypnoticShare > 0.0f) s = morphProfile(s, styleProfile(StyleE::HYPNOTIC), hypnoticShare, p);
    return s;
}

StyleProfile profileOf(const ParamStore& p)
{
    const StyleE style = static_cast<StyleE>(p.getInt(p.id(ModuleE::COMPOSE, 0, compose::STYLE)));
    StyleProfile s = styleProfile(style);
    const int to = p.getInt(p.id(ModuleE::COMPOSE, 0, compose::MORPH_TO));
    if (to > 0) s = morphProfile(s, styleProfile(static_cast<StyleE>(to - 1)), p.get(p.id(ModuleE::COMPOSE, 0, compose::MORPH)), p);
    s = axisProfile(s, p.get(p.id(ModuleE::COMPOSE, 0, compose::DUB_SHARE)), p.get(p.id(ModuleE::COMPOSE, 0, compose::HYPNOTIC_SHARE)), p);
    // A style of the user's own: its numbers over the profile's (the pool, the recipe and the sounds stay the profile's).
    if (p.getBool(p.id(ModuleE::CUSTOM, 0, custom::USE))) {
        const auto c = [&](int k) { return p.get(p.id(ModuleE::CUSTOM, 0, k)); };
        const auto ci = [&](int k) { return p.getInt(p.id(ModuleE::CUSTOM, 0, k)); };
        s.m_bpmLow = std::min(c(custom::BPM_LOW), c(custom::BPM_HIGH));
        s.m_bpmHigh = std::max(c(custom::BPM_LOW), c(custom::BPM_HIGH));
        s.m_arcWeight = c(custom::ARC_WEIGHT);
        s.m_peakWeight = c(custom::PEAK_WEIGHT);
        s.m_endlessWeight = c(custom::ENDLESS_WEIGHT);
        if (s.m_arcWeight + s.m_peakWeight + s.m_endlessWeight <= 0.0f) s.m_arcWeight = 1.0f;
        s.m_subChance = c(custom::SUB_CHANCE);
        s.m_blocksLow = std::min(ci(custom::BLOCKS_LOW), ci(custom::BLOCKS_HIGH));
        s.m_blocksHigh = std::max(ci(custom::BLOCKS_LOW), ci(custom::BLOCKS_HIGH));
        s.m_mutation = c(custom::MUTATION);
        s.m_reroll = c(custom::REROLL);
        s.m_polymeterChance = c(custom::POLYMETER);
        s.m_fillChance = c(custom::FILL);
        s.m_edgeChance = c(custom::EDGE);
        s.m_maxReduction = ci(custom::MAX_REDUCTION);
        s.m_swingLow = std::min(c(custom::SWING_LOW), c(custom::SWING_HIGH));
        s.m_swingHigh = std::max(c(custom::SWING_LOW), c(custom::SWING_HIGH));
        s.m_eventRate = c(custom::EVENT_RATE);
        s.m_throwShare = c(custom::THROW_SHARE);
        s.m_densityCap = ci(custom::DENSITY_CAP);
        s.m_simTarget = c(custom::SIMILARITY);
        s.m_peakLufs = c(custom::PEAK_LUFS);
    }
    return s;
}

} // namespace tot
