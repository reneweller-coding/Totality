/**
 * @file Style.cpp
 * @brief The four profiles, their morph and the axes.
 */
#include "umb/compose/Style.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <string>

namespace umb {

namespace {

using L = LayerId;

// Designated initialisers (C++20): every value stands by its name, in the order of Style.h.
const StyleProfile kProfiles[] = {
    // Hypnotic: the centre. The rumble owns the low end (0.7); a processed, rolling 909 kick; two or three cyclic layers;
    // the ping's ostinato, drones and texture; the chord seldom. Tool 0.5, Endless 0.3, Peak 0.2 (PLAN 2.8).
    {
        .name = "Hypnotic",
        .bpmLow = 128.0f,
        .bpmHigh = 133.0f,
        .arcWeight = 0.5f,
        .peakWeight = 0.2f,
        .endlessWeight = 0.3f,
        .subChance = 0.3f,
        .intro64Chance = 0.4f,
        .blocksLow = 6,
        .blocksHigh = 8,
        .pool = { { L::GhostKick, 0.8f }, { L::OpenHat, 0.9f }, { L::Ride, 0.4f }, { L::ClapA, 0.3f }, { L::Shaker, 0.6f },
                  { L::TomConga, 0.5f }, { L::Rim, 0.5f }, { L::Ping, 0.9f }, { L::Acid, 0.05f }, { L::Chord, 0.2f },
                  { L::Drone, 0.6f }, { L::Texture, 0.5f } },
        .mutation = 0.25f,
        .motionScale = 1.0f,
        .reroll = 0.15f,
        .polymeterChance = 0.8f,
        .swingLow = 51.0f,
        .swingHigh = 56.0f,
        .fillChance = 0.15f,
        .toolReductionChance = 0.5f,
        .eventRate = 0.35f,
        .throwShare = 0.5f,
        .densityCap = 9,
        .simTarget = 0.80f,
        .microTarget = 1.0f,
        .recipe = { { "kick.engine", 2.0f }, { "chord.level", -8.0f }, { "drone.level", -16.0f }, { "space.level", -2.0f } },
        .sounds = { { "kick.pitch_decay", 14.0f, 24.0f }, { "kick.amp_decay", 300.0f, 450.0f }, { "kick.drive", 0.25f, 0.5f },
                    { "rumble.decay", 1.5f, 2.8f }, { "rumble.drive", 4.0f, 8.0f }, { "ping.ratio", 1.2f, 2.2f },
                    { "ping.decay", 90.0f, 220.0f }, { "ping.band", 600.0f, 1500.0f }, { "dub.echo_time", 0.5f, 4.49f },
                    { "dub.feedback", 0.2f, 0.35f }, { "space.decay", 0.9f, 1.8f }, { "drone.sweep_bars", 32.0f, 64.0f },
                    { "perc1.noise_type", -0.49f, 1.49f } },
        .peakLufs = -10.0f,
    },
    // Ostgut: the clap on 2 and 4, the ride's eighths, the open hat on the offbeat, one cyclic layer; dub stabs sparingly,
    // a 303 seldom. Tool 0.6, Peak 0.4.
    {
        .name = "Ostgut",
        .bpmLow = 125.0f,
        .bpmHigh = 131.0f,
        .arcWeight = 0.6f,
        .peakWeight = 0.4f,
        .endlessWeight = 0.0f,
        .subChance = 0.5f,
        .intro64Chance = 0.4f,
        .blocksLow = 6,
        .blocksHigh = 8,
        .pool = { { L::GhostKick, 0.6f }, { L::OpenHat, 0.95f }, { L::Ride, 0.8f }, { L::ClapA, 0.9f }, { L::Shaker, 0.5f },
                  { L::TomConga, 0.5f }, { L::Rim, 0.5f }, { L::Ping, 0.3f }, { L::Acid, 0.25f }, { L::Chord, 0.4f },
                  { L::Drone, 0.2f }, { L::Texture, 0.2f } },
        .mutation = 0.25f,
        .motionScale = 1.0f,
        .reroll = 0.15f,
        .polymeterChance = 0.5f,
        .swingLow = 50.0f,
        .swingHigh = 55.0f,
        .fillChance = 0.25f,
        .toolReductionChance = 0.5f,
        .eventRate = 0.3f,
        .throwShare = 0.4f,
        .densityCap = 9,
        .simTarget = 0.88f,
        .microTarget = 1.0f,
        .recipe = { { "kick.engine", 2.0f }, { "space.level", 2.0f }, { "mix.perc_level", -5.0f } },
        .sounds = { { "kick.pitch_decay", 14.0f, 22.0f }, { "kick.amp_decay", 250.0f, 400.0f }, { "kick.drive", 0.3f, 0.55f },
                    { "rumble.decay", 1.2f, 2.2f }, { "rumble.drive", 4.0f, 8.0f }, { "chord.detune", 6.0f, 15.0f },
                    { "chord.band", 300.0f, 480.0f }, { "dub.echo_time", 0.5f, 4.49f }, { "dub.feedback", 0.2f, 0.3f },
                    { "space.decay", 0.9f, 1.6f }, { "acid.cutoff", 400.0f, 900.0f }, { "perc1.noise_type", -0.49f, 1.49f } },
        .peakLufs = -9.5f,
    },
    // Dub: the sub bass owns the low end (0.8), a soft kick, straight time, motion from the echo, the hats reduced; the
    // dub chord with its whole chain, vinyl crackle. Tool 0.7, Endless 0.2, Peak 0.1.
    {
        .name = "Dub",
        .bpmLow = 125.0f,
        .bpmHigh = 130.0f,
        .arcWeight = 0.7f,
        .peakWeight = 0.1f,
        .endlessWeight = 0.2f,
        .subChance = 0.8f,
        .intro64Chance = 0.5f,
        .blocksLow = 6,
        .blocksHigh = 8,
        .pool = { { L::GhostKick, 0.4f }, { L::OpenHat, 0.7f }, { L::Ride, 0.2f }, { L::ClapA, 0.3f }, { L::Shaker, 0.6f },
                  { L::TomConga, 0.2f }, { L::Rim, 0.5f }, { L::Ping, 0.3f }, { L::Acid, 0.0f }, { L::Chord, 0.95f },
                  { L::Drone, 0.3f }, { L::Texture, 0.7f } },
        .mutation = 0.125f,
        .motionScale = 0.8f,
        .reroll = 0.25f,
        .polymeterChance = 0.3f,
        .swingLow = 50.0f,
        .swingHigh = 51.0f,
        .fillChance = 0.1f,
        .toolReductionChance = 0.4f,
        .eventRate = 0.45f,
        .throwShare = 0.8f,
        .densityCap = 8,
        .simTarget = 0.86f,
        .microTarget = 1.0f,
        .recipe = { { "kick.engine", 0.0f }, { "kick.click_level", 0.2f }, { "master.tilt", 5.0f }, { "chord.bright", 3500.0f },
                    { "chord.dip", -3.0f }, { "mix.hats_level", -6.0f } },
        .sounds = { { "kick.pitch_decay", 16.0f, 26.0f }, { "kick.amp_decay", 350.0f, 550.0f }, { "kick.drive", 0.15f, 0.35f },
                    { "rumble.decay", 1.2f, 2.2f }, { "rumble.drive", 3.0f, 6.0f }, { "chord.detune", 6.0f, 15.0f },
                    { "chord.band", 300.0f, 480.0f }, { "chord.crush", 6.0f, 9.0f }, { "dub.echo_time", 0.5f, 4.49f },
                    { "dub.feedback", 0.25f, 0.4f }, { "space.decay", 1.0f, 2.0f }, { "texture.hum_hz", 49.0f, 51.0f } },
        .peakLufs = -11.5f,
    },
    // Raw/Peak: the kick harder, distortion before the low pass, the rumble overdriven; denser, sixteenth hats, toms,
    // longer reductions; noise and metallic pings. Peak 0.6, Tool 0.4.
    {
        .name = "Raw Peak",
        .bpmLow = 131.0f,
        .bpmHigh = 136.0f,
        .arcWeight = 0.4f,
        .peakWeight = 0.6f,
        .endlessWeight = 0.0f,
        .subChance = 0.2f,
        .intro64Chance = 0.3f,
        .blocksLow = 6,
        .blocksHigh = 8,
        .pool = { { L::GhostKick, 0.7f }, { L::OpenHat, 0.9f }, { L::Ride, 0.5f }, { L::ClapA, 0.7f }, { L::Shaker, 0.6f },
                  { L::TomConga, 0.8f }, { L::Rim, 0.6f }, { L::Ping, 0.3f }, { L::Acid, 0.35f }, { L::Chord, 0.05f },
                  { L::Drone, 0.2f }, { L::Texture, 0.3f } },
        .mutation = 0.25f,
        .motionScale = 1.0f,
        .reroll = 0.3f,
        .polymeterChance = 0.5f,
        .swingLow = 50.0f,
        .swingHigh = 53.0f,
        .fillChance = 0.3f,
        .toolReductionChance = 0.6f,
        .eventRate = 0.3f,
        .throwShare = 0.3f,
        .densityCap = 10,
        .simTarget = 0.85f,
        .microTarget = 1.0f,
        .recipe = { { "kick.clip", 1.0f }, { "master.tilt", 8.0f }, { "space.level", 3.0f } },
        .sounds = { { "kick.pitch_decay", 12.0f, 20.0f }, { "kick.amp_decay", 250.0f, 380.0f }, { "kick.drive", 0.5f, 0.85f },
                    { "rumble.decay", 1.4f, 2.4f }, { "rumble.drive", 7.0f, 12.0f }, { "ping.ratio", 2.0f, 3.5f },
                    { "ping.decay", 80.0f, 160.0f }, { "acid.cutoff", 400.0f, 900.0f }, { "dub.echo_time", 0.5f, 4.49f },
                    { "space.decay", 0.8f, 1.4f }, { "perc1.noise_type", -0.49f, 1.49f } },
        .peakLufs = -9.5f,
    },
};

float lerp(float a, float b, float t) { return a + (b - a) * t; }
int lerpi(int a, int b, float t) { return static_cast<int>(std::lround(lerp(static_cast<float>(a), static_cast<float>(b), t))); }

} // namespace

const StyleProfile& styleProfile(Style style)
{
    const int i = std::clamp(static_cast<int>(style), 0, static_cast<int>(Style::Count) - 1);
    return kProfiles[i];
}

StyleProfile morphProfile(const StyleProfile& a, const StyleProfile& b, float t, const ParamStore& p)
{
    t = std::clamp(t, 0.0f, 1.0f);
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    StyleProfile m = t < 0.5f ? a : b;
    m.bpmLow = lerp(a.bpmLow, b.bpmLow, t);
    m.bpmHigh = lerp(a.bpmHigh, b.bpmHigh, t);
    m.arcWeight = lerp(a.arcWeight, b.arcWeight, t);
    m.peakWeight = lerp(a.peakWeight, b.peakWeight, t);
    m.endlessWeight = lerp(a.endlessWeight, b.endlessWeight, t);
    m.subChance = lerp(a.subChance, b.subChance, t);
    m.intro64Chance = lerp(a.intro64Chance, b.intro64Chance, t);
    m.blocksLow = lerpi(a.blocksLow, b.blocksLow, t);
    m.blocksHigh = lerpi(a.blocksHigh, b.blocksHigh, t);
    m.mutation = lerp(a.mutation, b.mutation, t);
    m.motionScale = lerp(a.motionScale, b.motionScale, t);
    m.reroll = lerp(a.reroll, b.reroll, t);
    m.polymeterChance = lerp(a.polymeterChance, b.polymeterChance, t);
    m.swingLow = lerp(a.swingLow, b.swingLow, t);
    m.swingHigh = lerp(a.swingHigh, b.swingHigh, t);
    m.fillChance = lerp(a.fillChance, b.fillChance, t);
    m.toolReductionChance = lerp(a.toolReductionChance, b.toolReductionChance, t);
    m.eventRate = lerp(a.eventRate, b.eventRate, t);
    m.throwShare = lerp(a.throwShare, b.throwShare, t);
    m.densityCap = lerpi(a.densityCap, b.densityCap, t);
    m.simTarget = lerp(a.simTarget, b.simTarget, t);
    m.microTarget = lerp(a.microTarget, b.microTarget, t);
    m.peakLufs = lerp(a.peakLufs, b.peakLufs, t);
    // The pool, layer by layer.
    std::map<int, float> ca, cb;
    for (const LayerChance& c : a.pool) ca[static_cast<int>(c.layer)] = c.chance;
    for (const LayerChance& c : b.pool) cb[static_cast<int>(c.layer)] = c.chance;
    m.pool.clear();
    for (int l = 0; l < kNumLayers; ++l) {
        const auto ia = ca.find(l), ib = cb.find(l);
        if (ia == ca.end() && ib == cb.end()) continue;
        const float va = ia == ca.end() ? 0.0f : ia->second, vb = ib == cb.end() ? 0.0f : ib->second;
        m.pool.push_back({ static_cast<LayerId>(l), lerp(va, vb, t) });
    }
    // The recipes, knob by knob: a knob only one sets is at its default in the other.
    std::map<std::string, std::pair<float, float>> r;
    for (const SoundValue& v : a.recipe) {
        const int id = p.find(v.key);
        r[v.key] = { v.value, id >= 0 ? p.defaultValue(id) : v.value };
    }
    for (const SoundValue& v : b.recipe) {
        const int id = p.find(v.key);
        auto it = r.find(v.key);
        if (it == r.end()) r[v.key] = { id >= 0 ? p.defaultValue(id) : v.value, v.value };
        else it->second.second = v.value;
    }
    m.recipe.clear();
    for (const StyleProfile* s : { &a, &b })
        for (const SoundValue& v : s->recipe) {
            const auto it = r.find(v.key);
            if (it == r.end()) continue;
            m.recipe.push_back({ v.key, lerp(it->second.first, it->second.second, t) });
            r.erase(it);
        }
    // The sound ranges: a knob both draw is drawn between the interpolated bounds; one only one draws, as the nearer does.
    m.sounds.clear();
    const StyleProfile& near = t < 0.5f ? a : b;
    const StyleProfile& far = t < 0.5f ? b : a;
    for (const SoundRange& s : near.sounds) {
        SoundRange out = s;
        for (const SoundRange& o : far.sounds)
            if (std::strcmp(o.key, s.key) == 0) {
                const float tt = &near == &a ? t : 1.0f - t;
                out.low = lerp(s.low, o.low, tt);
                out.high = lerp(s.high, o.high, tt);
            }
        m.sounds.push_back(out);
    }
    return m;
}

StyleProfile axisProfile(const StyleProfile& base, float dubShare, float hypnoticShare, const ParamStore& p)
{
    StyleProfile s = base;
    if (dubShare > 0.0f) s = morphProfile(s, styleProfile(Style::Dub), dubShare, p);
    if (hypnoticShare > 0.0f) s = morphProfile(s, styleProfile(Style::Hypnotic), hypnoticShare, p);
    return s;
}

StyleProfile profileOf(const ParamStore& p)
{
    const Style style = static_cast<Style>(p.getInt(p.id(Module::Compose, 0, compose::Style)));
    StyleProfile s = styleProfile(style);
    const int to = p.getInt(p.id(Module::Compose, 0, compose::MorphTo));
    if (to > 0) s = morphProfile(s, styleProfile(static_cast<Style>(to - 1)), p.get(p.id(Module::Compose, 0, compose::Morph)), p);
    return axisProfile(s, p.get(p.id(Module::Compose, 0, compose::DubShare)), p.get(p.id(Module::Compose, 0, compose::HypnoticShare)), p);
}

} // namespace umb
