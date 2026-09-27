/**
 * @file Rack.cpp
 * @brief The layer tables of Dok. 8.2 and 8.3, the rolls, the cycles, the ghost chains and the rules.
 */
#include "umb/pattern/Rack.h"
#include "umb/Dsp.h"
#include <algorithm>
#include <cmath>

namespace umb {

const char* const kLayerNames[kNumLayers] = { "kick", "ghost kick", "closed hat", "rolling hat", "open hat", "ride", "clap",
                                              "clap b", "clap ghost", "shaker", "tom/conga", "rim", "bass", "ping" };

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
    // Rolling hat: .9 / .7; base 64, accents 100 on the quarters and 80 before them; swung. Motion, as Dok. 8.2 says: the
    // reference measurement (PLAN 13.4, 27.09.2026) puts the hypnotic records' bar similarity at 0.87, and the study with
    // this layer rolled anew each bar at 0.87 too; as a loop it read 0.98 (steadier than any profile's references).
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
    // Ping: its figure is cyclic (RackPlan::cycle); the table only gives its velocity, spread and feel.
    { LayerKind::Loop,
      { 0 }, { V(95) },
      V(14), 0.0f, 6.0f, 0.5f, 0.25, true },
};

/** @brief General MIDI notes of the layers (the ping's is its pitch, set from the plan). */
constexpr int kLayerNote[kNumLayers] = { 36, 36, 42, 44, 46, 51, 39, 39, 39, 70, 45, 37, 45, 60 };

/** @brief A uniform number for (seed, a, b, c), the same whatever else was rolled. */
float roll(uint64_t seed, uint64_t a, uint64_t b, uint64_t c)
{
    const uint64_t h = mixSeed(mixSeed(mixSeed(seed, a), b), c);
    return static_cast<float>((h >> 40) * (1.0 / 16777216.0));
}

enum Salt : uint64_t { kBase = 11, kMutA = 12, kMutB = 13, kMotion = 14, kVel = 15, kJitter = 16, kCycle = 17, kFill = 18 };

bool isCyclic(const RackPlan& plan, int li) { return plan.period[li] > 0; }

/** @brief Base velocity of a layer: its loudest step's. */
float baseVelocity(const LayerDef& d)
{
    float v = 0.0f;
    for (float x : d.vel) v = std::max(v, x);
    return v > 0.0f ? v : 0.7f;
}

/** @brief Position of sixteenth @p s of bar @p bar in a cyclic layer's period. */
int cyclePos(const RackPlan& plan, int li, int bar, int s)
{
    const int period = std::max(1, plan.period[li]);
    const int reset = std::max(1, plan.resetBars[li]);
    const int64_t first = static_cast<int64_t>(bar / reset) * reset * kSteps;
    return static_cast<int>((static_cast<int64_t>(bar) * kSteps + s - first) % period);
}

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

uint64_t euclidMask(int k, int n)
{
    // Bresenham's form of the Euclidean rhythm: step i sounds where (i k mod n) < k. Maximally even, the first onset on
    // step 0 -- a rotation of Bjorklund's (Toussaint 2005).
    uint64_t m = 0;
    for (int i = 0; i < n; ++i) if ((i * k) % n < k) m |= uint64_t(1) << i;
    return m;
}

uint64_t rotateMask(uint64_t mask, int n, int r)
{
    r = ((r % n) + n) % n;
    uint64_t out = 0;
    for (int i = 0; i < n; ++i) if (mask & (uint64_t(1) << i)) out |= uint64_t(1) << ((i + r) % n);
    return out;
}

RackPlan makeRackPlan(const ParamStore& p, uint64_t seed)
{
    RackPlan plan;
    plan.seed = seed;
    Rng rng;
    rng.seed(mixSeed(seed, 0x5241434Bull));   // "RACK"
    // The style's restlessness (RackPlan), after the reference measurement's bar similarity (PLAN 13.4): Hypnotic 0.87
    // (Dok. 8.2's motion, rolled every bar), Raw 0.92 and Dub 0.93, Ostgut 0.955 (most of the motion rolled per block).
    const int styleId = p.getInt(p.id(Module::Compose, 0, compose::Style));
    switch (static_cast<Style>(styleId)) {
    case Style::Hypnotic: plan.mutation = 0.25f;  plan.motionScale = 1.0f; plan.reroll = 1.0f; break;
    case Style::Dub:      plan.mutation = 0.125f; plan.motionScale = 0.8f; plan.reroll = 0.25f; break;
    case Style::RawPeak:  plan.mutation = 0.25f;  plan.motionScale = 1.0f; plan.reroll = 0.3f; break;
    default:              plan.mutation = 0.25f;  plan.motionScale = 1.0f; plan.reroll = 0.15f; break;
    }
    for (int i = 0; i < kNumLayers; ++i) {
        const LayerDef& d = kLayers[i];
        const float u = rng.uniform();
        // Dok. 8.2: one bar 0.10, two 0.55, four 0.35; the style may ask for more one-bar loops, the rest keeping 11 : 7.
        const float two = plan.oneBarLoops + (1.0f - plan.oneBarLoops) * (0.55f / 0.90f);
        plan.loopBars[i] = d.kind != LayerKind::Loop ? 1 : (u < plan.oneBarLoops ? 1 : (u < two ? 2 : 4));
        plan.offsetMs[i] = d.offsetLoMs + rng.uniform() * (d.offsetHiMs - d.offsetLoMs);
    }
    plan.clapB = rng.uniform() < 0.3f;
    plan.tomLane = rng.uniform() < 0.5f ? 9 : 10;
    plan.keyRoot = p.getInt(p.id(Module::Compose, 0, compose::Key));
    plan.scale = p.getInt(p.id(Module::Compose, 0, compose::Scale));
    plan.swing = styleId == static_cast<int>(Style::Dub) ? 50.0f : p.get(p.id(Module::Compose, 0, compose::Swing));
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

    // Phase 2. The ping's figure: a polymeter of 5 or 7 sixteenths (p 0.35 each) or a slipping loop of 15 or 17 (p 0.15
    // each), Euclidean, rotated; one or two pitch classes around the root in 220 .. 415 Hz (PLAN 5.5).
    {
        const int li = static_cast<int>(LayerId::Ping);
        const float w = rng.uniform();
        const int period = w < 0.35f ? 5 : (w < 0.70f ? 7 : (w < 0.85f ? 15 : 17));
        const int k = period == 5 ? 2 : (period == 7 ? 3 : (period == 15 ? 5 + rng.below(2) : 6 + rng.below(2)));
        plan.period[li] = period;
        plan.resetBars[li] = period > 12 ? 32 : 16;
        plan.cycle[li] = rotateMask(euclidMask(k, period), period, rng.below(period));
        plan.pingRoot = 57 + ((plan.keyRoot - 9 + 12) % 12);   // A3 = 57 .. G#4 = 68
        static const int kSecond[] = { 7, 10, 12, 3, 5 };
        static const float kSecondW[] = { 0.35f, 0.25f, 0.20f, 0.10f, 0.10f };
        const float v = rng.uniform();
        float a2 = 0.0f;
        int second = 7;
        for (int i = 0; i < 5; ++i) { a2 += kSecondW[i]; if (v < a2) { second = kSecond[i]; break; } }
        if (!inScale(plan.scale, second)) second = 7;
        const bool twoTones = rng.uniform() < 0.7f;
        for (int pos = 0; pos < period; ++pos)
            plan.pingNote[pos] = plan.pingRoot + ((twoTones && rng.uniform() < 0.35f) ? second : 0);
    }
    // Polymeter: one percussion layer with p 0.25, two with p 0.25, of period 3, 5, 6, 7 or 12, reset every 16 bars
    // (Dok. 8.2: "1-2 Perc-Layer ... p 0.5 pro Track"); the others Euclidean with p 0.4 (E(3,8), E(5,16), E(7,16) off
    // the quarters) or displaced with p 0.3.
    {
        LayerId candidates[] = { LayerId::TomConga, LayerId::Rim, LayerId::Shaker };
        for (int i = 2; i > 0; --i) std::swap(candidates[i], candidates[rng.below(i + 1)]);
        static const int kPeriods[] = { 3, 5, 6, 7, 12 };
        const int polys = rng.uniform() < 0.5f ? (rng.uniform() < 0.5f ? 2 : 1) : 0;
        for (int c = 0; c < 3; ++c) {
            const int li = static_cast<int>(candidates[c]);
            if (c < polys) {
                const int period = kPeriods[rng.below(5)];
                const int k = std::max(1, static_cast<int>(std::lround(period / 3.0)));
                plan.period[li] = period;
                plan.resetBars[li] = 16;
                plan.cycle[li] = rotateMask(euclidMask(k, period), period, rng.below(period));
            } else if (rng.uniform() < 0.4f) {
                static const int kE[3][2] = { { 3, 8 }, { 5, 16 }, { 7, 16 } };
                const int* e = kE[rng.below(3)];
                uint64_t base = euclidMask(e[0], e[1]);
                if (e[1] == 8) base |= base << 8;   // E(3,8) twice in a bar
                // A rotation that keeps every onset off the quarters, searched from a random start.
                const int start = rng.below(16);
                for (int r = 0; r < 16; ++r) {
                    const uint64_t m = rotateMask(base, 16, start + r);
                    if ((m & 0x1111u) == 0) { plan.euclid[li] = static_cast<uint16_t>(m); break; }
                }
            } else if (rng.uniform() < 0.3f) {
                plan.displace[li] = 1 + rng.below(3);
            }
        }
        if (rng.uniform() < 0.3f) plan.displace[static_cast<int>(LayerId::ClapGhost)] = 1 + rng.below(3);
    }
    // Trig conditions (Dok. 8.2): an extra open hat in the second of four bars, a single shaker in the third.
    plan.conds[plan.nConds++] = TrigCond{ LayerId::OpenHat, rng.uniform() < 0.5f ? 14 : 10, 2, 4 };
    plan.conds[plan.nConds++] = TrigCond{ LayerId::Shaker, rng.uniform() < 0.5f ? 13 : 5, 3, 4 };
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

void rollSteps(const RackPlan& plan, LayerId id, int bar, int block, float density, bool* on, const bool* busy)
{
    const int li = static_cast<int>(id);
    const LayerDef& d = kLayers[li];
    for (int s = 0; s < kSteps; ++s) on[s] = false;
    if (density <= 0.0f) return;

    if (isCyclic(plan, li)) {
        // The cycle runs against the bar from its last reset; density thins it the same way in every pass.
        for (int s = 0; s < kSteps; ++s) {
            const int pos = cyclePos(plan, li, bar, s);
            if (!(plan.cycle[li] & (uint64_t(1) << pos))) continue;
            on[s] = density >= 1.0f || roll(plan.seed, li, kCycle, static_cast<uint64_t>(block) * 64 + pos) < density;
        }
        return;
    }
    if (plan.euclid[li] != 0) {
        for (int s = 0; s < kSteps; ++s) {
            if (!(plan.euclid[li] & (1u << s))) continue;
            on[s] = density >= 1.0f || roll(plan.seed, li, kBase, static_cast<uint64_t>(block) * 16 + s) < density;
        }
    } else {
        bool prev1 = false, prev2 = false;
        for (int s = 0; s < kSteps; ++s) {
            const float p = d.p[s];
            if (p <= 0.0f) { prev2 = prev1; prev1 = false; continue; }
            if (p >= 1.0f) { on[s] = true; prev2 = prev1; prev1 = true; continue; }
            float u, q = p * std::min(density, 1.0f);
            switch (d.kind) {
            case LayerKind::Anchor:
                u = roll(plan.seed, li, kBase, static_cast<uint64_t>(block) * 16 + s);
                break;
            case LayerKind::Loop: {
                // Bar 1 (and 3) of the loop is the base roll; bars 2 and 4 re-roll the style's share of the open steps
                // (RackPlan::mutation; Dok. 8.2's "Mutation nur in Takt 2 bzw. 2/4").
                const int pos = (bar % 32) % std::max(1, plan.loopBars[li]);
                u = roll(plan.seed, li, kBase, static_cast<uint64_t>(block) * 16 + s);
                if (pos == 1 || pos == 3) {
                    const uint64_t salt = pos == 1 ? kMutA : kMutB;
                    if (roll(plan.seed, li, salt, static_cast<uint64_t>(block) * 64 + s) < plan.mutation)
                        u = roll(plan.seed, li, salt, static_cast<uint64_t>(block) * 64 + 16 + s);
                }
                break;
            }
            default:
                // The ghost chain (Erg. 7): the layer's own last two steps, the others' ghosts, the half bar before. A step
                // is rolled anew this bar with the style's reroll share, else it keeps its roll of the block.
                u = roll(plan.seed, li, kMotion, static_cast<uint64_t>(bar) * 16 + s);
                if (plan.reroll < 1.0f && roll(plan.seed, li, kMutB, static_cast<uint64_t>(bar) * 16 + s) >= plan.reroll)
                    u = roll(plan.seed, li, kMotion, 0x100000000ull + static_cast<uint64_t>(block) * 16 + s);
                q *= plan.motionScale;
                if (prev1) q *= 0.5f;
                else if (prev2) q *= 0.75f;
                if (busy != nullptr && busy[s]) q *= 0.5f;
                if (s >= 8 && d.p[s - 8] > 0.0f && !on[s - 8]) q = std::min(1.0f, q * 1.5f);
                break;
            }
            on[s] = u < q;
            prev2 = prev1;
            prev1 = on[s];
        }
    }
    if (plan.displace[li] > 0) {
        bool tmp[kSteps];
        for (int s = 0; s < kSteps; ++s) tmp[(s + plan.displace[li]) % kSteps] = on[s];
        for (int s = 0; s < kSteps; ++s) on[s] = tmp[s];
    }
}

void realizeBar(const RackPlan& plan, const BarSpec& spec, std::vector<NoteEvent>& out)
{
    bool on[kNumLayers][kSteps] = {};
    auto L = [](LayerId id) { return static_cast<int>(id); };
    // The layers in order; the ghosts see what the ghosts and the percussion before them took (the chain's cross factor).
    bool busy[kSteps] = {};
    for (int i = 0; i < kNumLayers; ++i) {
        const LayerId id = static_cast<LayerId>(i);
        if (id == LayerId::ClapB || !spec.active[i]) continue;
        const LayerId table = (id == LayerId::ClapA && plan.clapB) ? LayerId::ClapB : id;
        const LayerDef& d = kLayers[static_cast<int>(table)];
        const bool motion = d.kind == LayerKind::Motion;
        rollSteps(plan, table, spec.bar, spec.block, spec.density[i], on[i], motion ? busy : nullptr);
        if (motion || d.perc)
            for (int s = 0; s < kSteps; ++s) busy[s] = busy[s] || on[i][s];
    }
    // Trig conditions.
    for (int c = 0; c < plan.nConds; ++c) {
        const TrigCond& t = plan.conds[c];
        if (spec.active[L(t.layer)] && (spec.bar % t.b) == t.a - 1) on[L(t.layer)][t.step] = true;
    }
    // The rules. Gap reservation: the quarters belong to the kick, the clap and the rim; a cyclic layer crosses them.
    for (int i = 0; i < kNumLayers; ++i) {
        const LayerId id = static_cast<LayerId>(i);
        if (id == LayerId::Kick || id == LayerId::ClapA || id == LayerId::Rim || isCyclic(plan, i)) continue;
        for (int s = 0; s < kSteps; s += 4) on[i][s] = false;
    }
    // One hat per step.
    for (int s = 0; s < kSteps; ++s) {
        if (on[L(LayerId::OpenHat)][s]) { on[L(LayerId::ClosedHat)][s] = false; on[L(LayerId::RollingHat)][s] = false; }
        if (on[L(LayerId::ClosedHat)][s]) on[L(LayerId::RollingHat)][s] = false;
    }
    // A fill: the tom or conga rolls through the sixteenths after the last quarter of an eight-bar phrase (p fillChance;
    // the quarter stays the kick's), never a snare roll.
    bool fill = false;
    if (spec.fills && spec.active[L(LayerId::TomConga)] && (spec.bar % 8) == 7
        && roll(plan.seed, L(LayerId::TomConga), kFill, static_cast<uint64_t>(spec.bar)) < plan.fillChance) {
        fill = true;
        for (int s = 13; s < kSteps; ++s) on[L(LayerId::TomConga)][s] = true;
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
        const bool pattern = isCyclic(plan, i) || plan.euclid[i] != 0;
        for (int s = 0; s < kSteps; ++s) {
            if (!on[i][s]) continue;
            NoteEvent n;
            // Velocity: the step's (a cyclic or Euclidean layer: its loudest step's), a random spread, the collision dip.
            const float base = pattern || d.vel[s] <= 0.0f ? baseVelocity(d) : d.vel[s];
            float vel = base + d.velRandom * (2.0f * roll(plan.seed, i, kVel, static_cast<uint64_t>(spec.bar) * 16 + s) - 1.0f);
            double offMs = plan.offsetMs[i];
            const bool kickHere = on[L(LayerId::Kick)][s] || on[L(LayerId::GhostKick)][s];
            const bool hatHere = on[L(LayerId::ClosedHat)][s] || on[L(LayerId::RollingHat)][s] || on[L(LayerId::OpenHat)][s];
            if (d.perc && (kickHere || hatHere)) {
                if (id == LayerId::Rim && kickHere) offMs = -4.0;   // the sidestick 4 ms before the kick (Dok. 8.3)
                else vel *= 0.75f;
            }
            if (id == LayerId::ClosedHat) vel = std::min(vel, V(89));
            if (fill && id == LayerId::TomConga && s >= 13) { vel = 0.6f + 0.12f * static_cast<float>(s - 13); n.shift = 2 * (s - 12); }
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
            } else if (id == LayerId::Ping) {
                n.part = Part::Ping;
                n.pitch = plan.pingNote[cyclePos(plan, i, spec.bar, s)];
            } else {
                n.part = percPart(layerLane(plan, id));
                n.pitch = kLayerNote[i];
            }
            out.push_back(n);
        }
    }
}

std::string miniNotation(const RackPlan& plan, LayerId id, int bar, const char* name)
{
    const int li = static_cast<int>(id);
    std::string out;
    if (isCyclic(plan, li)) {
        out = "{";
        for (int pos = 0; pos < plan.period[li]; ++pos) {
            if (pos > 0) out += ' ';
            out += (plan.cycle[li] & (uint64_t(1) << pos)) ? name : "~";
        }
        return out + "}%16";
    }
    bool on[kSteps];
    rollSteps(plan, id, bar, bar / 32, 1.0f, on);
    for (int s = 0; s < kSteps; ++s) {
        if (s > 0) out += ' ';
        out += on[s] ? name : "~";
    }
    return out;
}

} // namespace umb
