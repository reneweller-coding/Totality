/**
 * @file Rack.cpp
 * @brief The layer tables of Dok. 8.2 and 8.3, the rolls and the rules.
 */
#include "umb/pattern/Rack.h"
#include "umb/Dsp.h"
#include <algorithm>
#include <cmath>

namespace umb {

const char* const kLayerNames[kNumLayers] = { "kick", "ghost kick", "closed hat", "rolling hat", "open hat", "ride", "clap",
                                              "clap b", "clap ghost", "shaker", "tom/conga", "rim", "bass" };

namespace {

constexpr float V(int midi) { return static_cast<float>(midi) / 127.0f; }

/**
 * The matrices of Dok. 8.2 (steps 1 .. 16 there are indices 0 .. 15 here) and the velocities, ranges, offsets and
 * swing shares of Dok. 8.3. Velocities: the middle of the range, with the random spread covering the range.
 */
const LayerDef kLayers[kNumLayers] = {
    // Kick: the quarters, 127, on the grid, never swung.
    { LayerKind::Anchor,
      { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 },
      { V(127), 0, 0, 0, V(127), 0, 0, 0, V(127), 0, 0, 0, V(127), 0, 0, 0 },
      0.0f, 0.0f, 0.0f, 0.0f, 0.25, false },
    // Ghost kick: before beats 2 and 4, rarely the last sixteenth; 40 .. 60.
    { LayerKind::Motion,
      { 0, 0, 0, .25f, 0, 0, 0, 0, 0, 0, 0, .25f, 0, 0, 0, .10f },
      { 0, 0, 0, V(50), 0, 0, 0, 0, 0, 0, 0, V(50), 0, 0, 0, V(50) },
      V(10), 0.0f, 0.0f, 0.0f, 0.25, false },
    // Closed hat on the offbeat: 80 .. 90 (capped at 89), pushed 0 .. 9 ms.
    { LayerKind::Anchor,
      { 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0 },
      { 0, 0, V(84), 0, 0, 0, V(84), 0, 0, 0, V(84), 0, 0, 0, V(84), 0 },
      V(5), 0.0f, 9.0f, 1.0f, 0.1, false },
    // Rolling hat: .9 / .7; base 64, accents 100 on the quarters and 80 before them; swung.
    { LayerKind::Motion,
      { .9f, .7f, .9f, .7f, .9f, .7f, .9f, .7f, .9f, .7f, .9f, .7f, .9f, .7f, .9f, .7f },
      { V(100), V(64), V(64), V(80), V(100), V(64), V(64), V(80), V(100), V(64), V(64), V(80), V(100), V(64), V(64), V(80) },
      V(12), 0.0f, 0.0f, 1.0f, 0.1, false },
    // Open hat on the offbeats, 64 .. 90 (the one on beat 2 at 64), pushed 5 .. 15 ms.
    { LayerKind::Loop,
      { 0, 0, .6f, 0, 0, 0, .9f, 0, 0, 0, .6f, 0, 0, 0, .9f, 0 },
      { 0, 0, V(77), 0, 0, 0, V(64), 0, 0, 0, V(77), 0, 0, 0, V(77), 0 },
      V(8), 5.0f, 15.0f, 1.0f, 0.5, false },
    // Ride on the eighths, 100 and 70 alternating, half swung.
    { LayerKind::Loop,
      { .8f, 0, .8f, 0, .8f, 0, .8f, 0, .8f, 0, .8f, 0, .8f, 0, .8f, 0 },
      { V(100), 0, V(70), 0, V(100), 0, V(70), 0, V(100), 0, V(70), 0, V(100), 0, V(70), 0 },
      V(5), 0.0f, 5.0f, 0.5f, 0.5, false },
    // Clap A: 2 and 4, 110 .. 127, straight.
    { LayerKind::Loop,
      { 0, 0, 0, 0, .9f, 0, 0, 0, 0, 0, 0, 0, .9f, 0, 0, 0 },
      { 0, 0, 0, 0, V(118), 0, 0, 0, 0, 0, 0, 0, V(118), 0, 0, 0 },
      V(8), 0.0f, 0.0f, 0.0f, 0.25, false },
    // Clap B: displaced.
    { LayerKind::Loop,
      { 0, 0, 0, 0, .8f, 0, 0, .5f, 0, .5f, 0, 0, .8f, 0, 0, .5f },
      { 0, 0, 0, 0, V(118), 0, 0, V(105), 0, V(105), 0, 0, V(118), 0, 0, V(105) },
      V(8), 0.0f, 0.0f, 0.0f, 0.25, false },
    // Clap ghosts: 50 .. 70, pushed 5 .. 12 ms, swung.
    { LayerKind::Motion,
      { 0, .15f, 0, .15f, 0, .15f, 0, .2f, 0, .15f, 0, .15f, 0, .15f, 0, .25f },
      { 0, V(60), 0, V(60), 0, V(60), 0, V(60), 0, V(60), 0, V(60), 0, V(60), 0, V(60) },
      V(10), 5.0f, 12.0f, 1.0f, 0.25, true },
    // Shaker: 70 .. 100, pushed 5 .. 15 ms, swung.
    { LayerKind::Motion,
      { 0, 0, .7f, 0, 0, 0, .4f, 0, 0, 0, .7f, 0, 0, 0, .4f, 0 },
      { 0, 0, V(85), 0, 0, 0, V(85), 0, 0, 0, V(85), 0, 0, 0, V(85), 0 },
      V(14), 5.0f, 15.0f, 1.0f, 0.25, true },
    // Tom/conga in the gaps.
    { LayerKind::Loop,
      { 0, .3f, 0, .3f, 0, .3f, 0, .3f, 0, .3f, 0, .3f, 0, .4f, .2f, .5f },
      { 0, V(85), 0, V(85), 0, V(85), 0, V(85), 0, V(85), 0, V(85), 0, V(85), V(85), V(85) },
      V(14), 5.0f, 15.0f, 1.0f, 0.25, true },
    // Rim: 70 .. 90, straight; 4 ms early where it meets the kick.
    { LayerKind::Loop,
      { .4f, 0, 0, 0, 0, 0, .2f, 0, 0, 0, 0, 0, 0, 0, .2f, 0 },
      { V(80), 0, 0, 0, 0, 0, V(80), 0, 0, 0, 0, 0, 0, 0, V(80), 0 },
      V(5), 0.0f, 0.0f, 0.0f, 0.1, true },
    // Bass: the sixteenth ostinato around the kicks, 90 .. 110, half swung.
    { LayerKind::Loop,
      { 0, .5f, .8f, .5f, 0, .5f, .8f, .5f, 0, .5f, .8f, .5f, 0, .5f, .8f, .6f },
      { 0, V(100), V(100), V(100), 0, V(100), V(100), V(100), 0, V(100), V(100), V(100), 0, V(100), V(100), V(100) },
      V(5), 0.0f, 5.0f, 0.5f, 0.2, false },
};

/** @brief General MIDI notes of the layers. */
constexpr int kLayerNote[kNumLayers] = { 36, 36, 42, 44, 46, 51, 39, 39, 39, 70, 45, 37, 45 };

/** @brief A uniform number for (seed, a, b, c), the same whatever else was rolled. */
float roll(uint64_t seed, uint64_t a, uint64_t b, uint64_t c)
{
    const uint64_t h = mixSeed(mixSeed(mixSeed(seed, a), b), c);
    return static_cast<float>((h >> 40) * (1.0 / 16777216.0));
}

enum Salt : uint64_t { kBase = 11, kMutA = 12, kMutB = 13, kMotion = 14, kVel = 15, kJitter = 16 };

bool isQuarter(int s) { return (s & 3) == 0; }

} // namespace

const LayerDef& layerDef(LayerId id) { return kLayers[static_cast<int>(id)]; }

int layerLane(const RackPlan& plan, LayerId id)
{
    switch (id) {
    case LayerId::ClosedHat:  return 0;
    case LayerId::RollingHat: return 1;
    case LayerId::OpenHat:    return 2;
    case LayerId::Ride:       return 3;
    case LayerId::ClapA:
    case LayerId::ClapB:      return 4;
    case LayerId::ClapGhost:  return 5;
    case LayerId::Rim:        return 7;
    case LayerId::Shaker:     return 8;
    case LayerId::TomConga:   return plan.tomLane;
    default:                  return -1;
    }
}

RackPlan makeRackPlan(const ParamStore& p, uint64_t seed)
{
    RackPlan plan;
    plan.seed = seed;
    Rng rng;
    rng.seed(mixSeed(seed, 0x5241434Bull));   // "RACK"
    for (int i = 0; i < kNumLayers; ++i) {
        const LayerDef& d = kLayers[i];
        const float u = rng.uniform();
        plan.loopBars[i] = d.kind != LayerKind::Loop ? 1 : (u < 0.10f ? 1 : (u < 0.65f ? 2 : 4));   // Dok. 8.2
        plan.offsetMs[i] = d.offsetLoMs + rng.uniform() * (d.offsetHiMs - d.offsetLoMs);
    }
    plan.clapB = rng.uniform() < 0.3f;
    plan.tomLane = rng.uniform() < 0.5f ? 9 : 10;
    plan.keyRoot = p.getInt(p.id(Module::Compose, 0, compose::Key));
    plan.scale = p.getInt(p.id(Module::Compose, 0, compose::Scale));
    const int style = p.getInt(p.id(Module::Compose, 0, compose::Style));
    plan.swing = style == static_cast<int>(Style::Dub) ? 50.0f : p.get(p.id(Module::Compose, 0, compose::Swing));
    plan.humanizeMs = p.get(p.id(Module::Compose, 0, compose::Humanize));
    // The bass's root: the octave of the key's root between 41 and 82 Hz (E1 .. E2), never under 35 Hz (Dok. 8.4).
    plan.bassRoot = 24 + plan.keyRoot;
    if (midiToHz(plan.bassRoot) < 41.0) plan.bassRoot += 12;
    // The bass alphabet (Dok. 8.6): {1} 0.3, {1,b7} 0.15, {1,5} 0.15, {1,b3} 0.1, {1,b2} 0.1, {1,4} 0.08, {1,b5} 0.07,
    // {1,b3,5,b7} 0.05.
    struct Set { float w; int n; int s[4]; };
    static const Set kSets[] = {
        { 0.30f, 1, { 0 } }, { 0.15f, 2, { 0, 10 } }, { 0.15f, 2, { 0, 7 } }, { 0.10f, 2, { 0, 3 } }, { 0.10f, 2, { 0, 1 } },
        { 0.08f, 2, { 0, 5 } }, { 0.07f, 2, { 0, 6 } }, { 0.05f, 4, { 0, 3, 7, 10 } },
    };
    float u = rng.uniform(), acc = 0.0f;
    const Set* pick = &kSets[0];
    for (const Set& s : kSets) { acc += s.w; if (u < acc) { pick = &s; break; } }
    plan.bassSize = pick->n;
    for (int i = 0; i < 4; ++i) plan.bassSet[i] = i < pick->n ? pick->s[i] : 0;
    return plan;
}

BarSpec emptyBar(int bar, double beat, double bpm)
{
    BarSpec s;
    s.bar = bar;
    s.block = bar / 32;
    s.barInBlock = bar % 32;
    s.beat = beat;
    s.bpm = bpm;
    for (int i = 0; i < kNumLayers; ++i) { s.active[i] = false; s.density[i] = 1.0f; }
    return s;
}

void rollSteps(const RackPlan& plan, LayerId id, int bar, int block, float density, bool* on)
{
    const int li = static_cast<int>(id);
    const LayerDef& d = kLayers[li];
    for (int s = 0; s < kSteps; ++s) {
        const float p = d.p[s];
        if (p <= 0.0f || density <= 0.0f) { on[s] = false; continue; }
        if (p >= 1.0f) { on[s] = true; continue; }
        float u;
        switch (d.kind) {
        case LayerKind::Anchor:
            u = roll(plan.seed, li, kBase, static_cast<uint64_t>(block) * 16 + s);
            break;
        case LayerKind::Loop: {
            // Bar 1 (and 3) of the loop is the base roll; bars 2 and 4 re-roll a quarter of the open steps.
            const int pos = (bar % 32) % std::max(1, plan.loopBars[li]);
            u = roll(plan.seed, li, kBase, static_cast<uint64_t>(block) * 16 + s);
            if (pos == 1 || pos == 3) {
                const uint64_t salt = pos == 1 ? kMutA : kMutB;
                if (roll(plan.seed, li, salt, static_cast<uint64_t>(block) * 64 + s) < 0.25f)
                    u = roll(plan.seed, li, salt, static_cast<uint64_t>(block) * 64 + 16 + s);
            }
            break;
        }
        default:
            u = roll(plan.seed, li, kMotion, static_cast<uint64_t>(bar) * 16 + s);
            break;
        }
        on[s] = u < p * std::min(density, 1.0f);
    }
}

void realizeBar(const RackPlan& plan, const BarSpec& spec, std::vector<NoteEvent>& out)
{
    bool on[kNumLayers][kSteps] = {};
    for (int i = 0; i < kNumLayers; ++i) {
        LayerId id = static_cast<LayerId>(i);
        if (id == LayerId::ClapB) continue;   // the clap layer is ClapA, played with B's matrix if the plan says so
        if (!spec.active[i]) continue;
        const LayerId table = (id == LayerId::ClapA && plan.clapB) ? LayerId::ClapB : id;
        rollSteps(plan, table, spec.bar, spec.block, spec.density[i], on[i]);
    }
    auto L = [](LayerId id) { return static_cast<int>(id); };
    // The rules. Gap reservation: the quarters belong to the kick, the clap and the rim.
    for (int i = 0; i < kNumLayers; ++i) {
        const LayerId id = static_cast<LayerId>(i);
        if (id == LayerId::Kick || id == LayerId::ClapA || id == LayerId::Rim) continue;
        for (int s = 0; s < kSteps; s += 4) on[i][s] = false;
    }
    // One hat per step.
    for (int s = 0; s < kSteps; ++s) {
        if (on[L(LayerId::OpenHat)][s]) { on[L(LayerId::ClosedHat)][s] = false; on[L(LayerId::RollingHat)][s] = false; }
        if (on[L(LayerId::ClosedHat)][s]) on[L(LayerId::RollingHat)][s] = false;
    }
    // "Mute one expected hit": everything but the kick falls silent on the marked steps.
    for (int s = 0; s < kSteps; ++s) {
        if (!spec.muteStep[s]) continue;
        for (int i = 0; i < kNumLayers; ++i) if (i != L(LayerId::Kick)) on[i][s] = false;
    }

    const double msToBeats = spec.bpm / 60000.0;
    const double swingBeats = (plan.swing - 50.0) / 100.0 * 0.5;   // Linn: the even sixteenth late by (S - 50) % of an eighth
    for (int i = 0; i < kNumLayers; ++i) {
        const LayerId id = static_cast<LayerId>(i);
        const LayerId table = (id == LayerId::ClapA && plan.clapB) ? LayerId::ClapB : id;
        const LayerDef& d = kLayers[static_cast<int>(table)];
        const bool kickLike = id == LayerId::Kick || id == LayerId::GhostKick;
        for (int s = 0; s < kSteps; ++s) {
            if (!on[i][s]) continue;
            NoteEvent n;
            // Velocity: the step's, a random spread, the collision dip.
            float vel = d.vel[s] + d.velRandom * (2.0f * roll(plan.seed, i, kVel, static_cast<uint64_t>(spec.bar) * 16 + s) - 1.0f);
            double offMs = plan.offsetMs[i];
            const bool kickHere = on[L(LayerId::Kick)][s] || on[L(LayerId::GhostKick)][s];
            const bool hatHere = on[L(LayerId::ClosedHat)][s] || on[L(LayerId::RollingHat)][s] || on[L(LayerId::OpenHat)][s];
            if (d.perc && (kickHere || hatHere)) {
                if (id == LayerId::Rim && kickHere) offMs = -4.0;   // the sidestick 4 ms before the kick (Dok. 8.3)
                else vel *= 0.75f;
            }
            if (id == LayerId::ClosedHat) vel = std::min(vel, V(89));
            // Timing: swing on the even sixteenths, the feel offset, a late jitter (never on the kick).
            double beat = spec.beat + 0.25 * s;
            if ((s & 1) == 1) beat += swingBeats * d.swing;
            if (!kickLike && plan.humanizeMs > 0.0f) {
                // Half-normal from two uniforms (Box-Muller's radius), never early.
                const double u1 = std::max(1.0e-9, static_cast<double>(roll(plan.seed, i, kJitter, static_cast<uint64_t>(spec.bar) * 32 + s)));
                const double u2 = roll(plan.seed, i, kJitter, static_cast<uint64_t>(spec.bar) * 32 + 16 + s);
                const double g = std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2);
                offMs += std::min(10.0, std::fabs(g) * plan.humanizeMs);
            }
            if (!kickLike) beat += offMs * msToBeats;
            n.beat = beat;
            n.length = d.length;
            n.velocity = clampv(vel, 0.05f, 1.0f);
            if (kickLike) {
                n.part = Part::Kick;
                n.pitch = 36;
            } else if (id == LayerId::Bass) {
                n.part = Part::Sub;
                // Root ostinato; the alphabet's other tones only on the last two sixteenths (Dok. 8.6).
                int interval = 0;
                if (plan.bassSize > 1 && s >= 14) {
                    const int k = 1 + static_cast<int>(roll(plan.seed, i, kBase, 1000 + static_cast<uint64_t>(spec.bar) * 2 + (s - 14)) * static_cast<float>(plan.bassSize - 1));
                    interval = plan.bassSet[std::min(k, plan.bassSize - 1)];
                }
                n.pitch = plan.bassRoot + interval;
            } else {
                n.part = percPart(layerLane(plan, id));
                n.pitch = kLayerNote[i];
            }
            out.push_back(n);
        }
    }
}

} // namespace umb
