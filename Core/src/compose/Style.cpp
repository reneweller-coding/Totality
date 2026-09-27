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
        .edgeChance = 0.5f,
        .maxReduction = 32,
        .eventRate = 0.35f,
        .throwShare = 0.5f,
        .densityCap = 9,
        .simTarget = 0.90f,   // the references' audio bar similarity + 0.05: the score's runs 0.04 .. 0.07 over the audio's (27.09.2026)
        .microTarget = 1.0f,
        // Fitted on the loudest minute of three composed tracks against the references (27.09.2026): tilt 11.8, ping -5,
        // pads -10 (at its bound; -6 taken), room +2, echo and plate -4.
        .recipe = { { "master.tilt", 12.0f }, { "chord.level", -14.0f }, { "drone.level", -22.0f },
                    { "space.level", 0.0f }, { "ping.level", -15.5f }, { "dub.echo_return", -6.0f }, { "dub.plate_return", 3.0f } },
        .sounds = { { "kick.pitch_decay", 14.0f, 24.0f }, { "kick.amp_decay", 300.0f, 450.0f }, { "kick.drive", 0.25f, 0.5f },
                    { "rumble.decay", 1.5f, 2.8f }, { "rumble.drive", 4.0f, 8.0f }, { "ping.ratio", 1.2f, 2.2f },
                    { "ping.decay", 90.0f, 220.0f }, { "ping.band", 600.0f, 1500.0f }, { "dub.echo_time", 0.5f, 4.49f },
                    { "dub.feedback", 0.2f, 0.35f }, { "space.decay", 0.9f, 1.8f }, { "drone.sweep_bars", 32.0f, 64.0f },
                    { "perc1.noise_type", -0.49f, 1.49f } },
        .peakLufs = -10.0f,
        .styleMix = { 1.0f, 0.0f, 0.0f, 0.0f },
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
        .reroll = 0.08f,   // 0.15 measured 0.909 .. 0.965 against 0.955 (27.09.2026)
        .polymeterChance = 0.5f,
        .swingLow = 50.0f,
        .swingHigh = 55.0f,
        .fillChance = 0.25f,
        .toolReductionChance = 0.5f,
        .edgeChance = 0.5f,
        .maxReduction = 32,
        .eventRate = 0.3f,
        .throwShare = 0.4f,
        .densityCap = 9,
        .simTarget = 0.99f,
        .microTarget = 1.0f,
        // Fitted as Hypnotic's: tilt 12.5, perc -4.7, hats -1.9, room +1.6.
        .recipe = { { "master.tilt", 12.5f }, { "space.level", 3.5f }, { "mix.perc_level", -10.0f },
                    { "mix.hats_level", -5.5f } },
        .sounds = { { "kick.pitch_decay", 14.0f, 22.0f }, { "kick.amp_decay", 250.0f, 400.0f }, { "kick.drive", 0.3f, 0.55f },
                    { "rumble.decay", 1.2f, 2.2f }, { "rumble.drive", 4.0f, 8.0f }, { "chord.detune", 6.0f, 15.0f },
                    { "chord.band", 300.0f, 480.0f }, { "dub.echo_time", 0.5f, 4.49f }, { "dub.feedback", 0.2f, 0.3f },
                    { "space.decay", 0.9f, 1.6f }, { "acid.cutoff", 400.0f, 900.0f }, { "perc1.noise_type", -0.49f, 1.49f } },
        .peakLufs = -9.5f,
        .styleMix = { 0.0f, 1.0f, 0.0f, 0.0f },
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
        .reroll = 0.12f,   // 0.25 measured 0.88 .. 0.91 against 0.93 (27.09.2026)
        .polymeterChance = 0.3f,
        .swingLow = 50.0f,
        .swingHigh = 51.0f,
        .fillChance = 0.1f,
        .toolReductionChance = 0.4f,
        .edgeChance = 0.6f,
        .maxReduction = 32,
        .eventRate = 0.45f,
        .throwShare = 0.8f,
        .densityCap = 8,
        .simTarget = 0.97f,
        .microTarget = 1.0f,
        // Fitted as Hypnotic's: tilt 13.3 (the chords' 1 .. 5 kHz), hats -10 (at its bound; -8 taken), perc -8.4, pads -6.2,
        // echo -3.6, room +2.1 -- the hats reduced, the stabs bright.
        .recipe = { { "master.tilt", 13.0f }, { "mix.hats_level", -14.0f }, { "mix.perc_level", -15.0f }, { "chord.level", -11.0f },
                    { "drone.level", -19.0f }, { "dub.echo_return", -6.5f }, { "space.level", 5.0f } },
        .sounds = { { "kick.pitch_decay", 16.0f, 26.0f }, { "kick.amp_decay", 350.0f, 550.0f }, { "kick.drive", 0.15f, 0.35f },
                    { "rumble.decay", 1.2f, 2.2f }, { "rumble.drive", 3.0f, 6.0f }, { "chord.detune", 6.0f, 15.0f },
                    { "chord.band", 300.0f, 480.0f }, { "chord.crush", 6.0f, 9.0f }, { "dub.echo_time", 0.5f, 4.49f },
                    { "dub.feedback", 0.25f, 0.4f }, { "space.decay", 1.0f, 2.0f }, { "texture.hum_hz", 49.0f, 51.0f },
                    { "kick.click_level", 0.0f, 0.25f }, { "chord.bright", 2500.0f, 4500.0f }, { "chord.dip", -4.0f, -2.0f },
                    { "chord.width", 0.8f, 0.95f } },
        .peakLufs = -11.5f,
        .styleMix = { 0.0f, 0.0f, 1.0f, 0.0f },
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
        .edgeChance = 0.1f,   // Raw records are flat (LRA 1.25): filtered edges put it at 2.4 .. 4.8
        .maxReduction = 8,   // the Raw references cut short: 0.5 reductions a track, median 3 bars (PLAN 13.4)
        .eventRate = 0.3f,
        .throwShare = 0.3f,
        .densityCap = 10,
        .simTarget = 0.96f,
        .microTarget = 1.0f,
        // Fitted as Hypnotic's: tilt 4.4, room +7.4, hats +3.2.
        .recipe = { { "master.tilt", 4.5f }, { "space.level", 10.0f }, { "mix.hats_level", -0.5f } },
        .sounds = { { "kick.pitch_decay", 12.0f, 20.0f }, { "kick.amp_decay", 250.0f, 380.0f }, { "kick.drive", 0.5f, 0.85f },
                    { "rumble.decay", 1.4f, 2.4f }, { "rumble.drive", 7.0f, 12.0f }, { "ping.ratio", 2.0f, 3.5f },
                    { "ping.decay", 80.0f, 160.0f }, { "acid.cutoff", 400.0f, 900.0f }, { "dub.echo_time", 0.5f, 4.49f },
                    { "space.decay", 0.8f, 1.4f }, { "perc1.noise_type", -0.49f, 1.49f }, { "kick.clip", 1.0f, 1.0f } },
        .peakLufs = -9.5f,
        .styleMix = { 0.0f, 0.0f, 0.0f, 1.0f },
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
    for (int k = 0; k < 4; ++k) m.styleMix[k] = lerp(a.styleMix[k], b.styleMix[k], t);
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
    m.edgeChance = lerp(a.edgeChance, b.edgeChance, t);
    m.maxReduction = t < 0.5f ? a.maxReduction : b.maxReduction;
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
    s = axisProfile(s, p.get(p.id(Module::Compose, 0, compose::DubShare)), p.get(p.id(Module::Compose, 0, compose::HypnoticShare)), p);
    // A style of the user's own: its numbers over the profile's (the pool, the recipe and the sounds stay the profile's).
    if (p.getBool(p.id(Module::Custom, 0, custom::Use))) {
        const auto c = [&](int k) { return p.get(p.id(Module::Custom, 0, k)); };
        const auto ci = [&](int k) { return p.getInt(p.id(Module::Custom, 0, k)); };
        s.bpmLow = std::min(c(custom::BpmLow), c(custom::BpmHigh));
        s.bpmHigh = std::max(c(custom::BpmLow), c(custom::BpmHigh));
        s.arcWeight = c(custom::ArcWeight);
        s.peakWeight = c(custom::PeakWeight);
        s.endlessWeight = c(custom::EndlessWeight);
        if (s.arcWeight + s.peakWeight + s.endlessWeight <= 0.0f) s.arcWeight = 1.0f;
        s.subChance = c(custom::SubChance);
        s.blocksLow = std::min(ci(custom::BlocksLow), ci(custom::BlocksHigh));
        s.blocksHigh = std::max(ci(custom::BlocksLow), ci(custom::BlocksHigh));
        s.mutation = c(custom::Mutation);
        s.reroll = c(custom::Reroll);
        s.polymeterChance = c(custom::Polymeter);
        s.fillChance = c(custom::Fill);
        s.edgeChance = c(custom::Edge);
        s.maxReduction = ci(custom::MaxReduction);
        s.swingLow = std::min(c(custom::SwingLow), c(custom::SwingHigh));
        s.swingHigh = std::max(c(custom::SwingLow), c(custom::SwingHigh));
        s.eventRate = c(custom::EventRate);
        s.throwShare = c(custom::ThrowShare);
        s.densityCap = ci(custom::DensityCap);
        s.simTarget = c(custom::Similarity);
        s.peakLufs = c(custom::PeakLufs);
    }
    return s;
}

} // namespace umb
