/**
 * @file Rack.cpp
 * @brief The layer tables of Dok. 8.2 and 8.3, the rolls, the cycles, the ghost chains and the rules.
 */
#include "tot/pattern/Rack.h"
#include "tot/compose/Style.h"
#include "tot/Dsp.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <vector>

namespace tot {

const char* const kLayerNames[kNumLayers] = { "kick", "ghost kick", "closed hat", "rolling hat", "open hat", "ride", "clap",
                                              "clap b", "clap ghost", "shaker", "tom/conga", "rim", "bass", "ping", "chord",
                                              "drone", "acid", "texture" };

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
    // Chord: Dok. 8.2's stab -- the downbeat .6, the third eighth .3, beat 3's offbeat .4; 79 % velocity with a wide spread
    // (Brootle's "Random Velocity 79 %"), straight, an eighth long.
    { LayerKind::Loop,
      { .6f, 0, 0, 0, 0, 0, .3f, 0, 0, 0, .4f, 0, 0, 0, 0, 0 },
      { V(100), 0, 0, 0, 0, 0, V(100), 0, 0, 0, V(100), 0, 0, 0, 0, 0 },
      V(20), 0.0f, 0.0f, 0.0f, 0.5, false },
    // Drone: a note on the downbeat of every eighth bar, held for them (realizeBar).
    { LayerKind::Anchor,
      { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { V(90), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0.0f, 0.0f, 0.0f, 0.0f, 32.0, false },
    // Acid: the 303's sixteenths, three in four sounding; pitches, accents and slides rolled per block (realizeBar).
    { LayerKind::Loop,
      { .75f, .75f, .75f, .75f, .75f, .75f, .75f, .75f, .75f, .75f, .75f, .75f, .75f, .75f, .75f, .75f },
      { V(96), V(96), V(96), V(96), V(96), V(96), V(96), V(96), V(96), V(96), V(96), V(96), V(96), V(96), V(96), V(96) },
      V(6), 0.0f, 3.0f, 0.5f, 0.18, false },
    // Texture: a note a bar, which gates it (Drone.h).
    { LayerKind::Anchor,
      { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { V(100), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0.0f, 0.0f, 0.0f, 0.0f, 4.0, false },
};

/** @brief General MIDI notes of the layers (the ping's is its pitch, set from the plan). */
constexpr int kLayerNote[kNumLayers] = { 36, 36, 42, 44, 46, 51, 39, 39, 39, 70, 45, 37, 45, 60, 57, 57, 45, 60 };

/** @brief A uniform number for (seed, a, b, c), the same whatever else was rolled. */
float roll(uint64_t seed, uint64_t a, uint64_t b, uint64_t c)
{
    const uint64_t h = mixSeed(mixSeed(mixSeed(seed, a), b), c);
    return static_cast<float>((h >> 40) * (1.0 / 16777216.0));
}

enum Salt : uint64_t { kBase = 11, kMutA = 12, kMutB = 13, kMotion = 14, kVel = 15, kJitter = 16, kCycle = 17, kFill = 18,
                       kAcid = 19 };

bool isCyclic(const RackPlan& plan, int li) { return plan.period[li] > 0; }

/** @brief The seed of layer @p li's rolls in a block of candidate @p variant (0: the layer's own seed). */
uint64_t seedOf(const RackPlan& plan, int li, uint32_t variant)
{
    return variant == 0 ? plan.layerSeed[li] : mixSeed(plan.layerSeed[li], 0x5641524900000000ull + variant);   // "VARI"
}

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

uint16_t offQuarterMask(int k, int r)
{
    const uint64_t slots = rotateMask(euclidMask(k, 12), 12, r);
    uint16_t m = 0;
    for (int j = 0, step = 0; step < kSteps; ++step) {
        if (step % 4 == 0) continue;   // the quarters are the kick's
        if ((slots >> j++) & 1u) m = static_cast<uint16_t>(m | (1u << step));
    }
    return m;
}

RackSettings rackSettings(const ParamStore& p)
{
    RackSettings s;
    const StyleProfile& prof = styleProfile(static_cast<Style>(p.getInt(p.id(Module::Compose, 0, compose::Style))));
    s.keyRoot = p.getInt(p.id(Module::Compose, 0, compose::Key));
    s.scale = p.getInt(p.id(Module::Compose, 0, compose::Scale));
    s.swing = prof.swingHigh <= 51.0f ? 50.0f : p.get(p.id(Module::Compose, 0, compose::Swing));   // Dub plays straight
    s.humanizeMs = p.get(p.id(Module::Compose, 0, compose::Humanize));
    s.mutation = prof.mutation;
    s.motionScale = prof.motionScale;
    s.reroll = prof.reroll;
    s.polymeterChance = prof.polymeterChance;
    s.fillChance = prof.fillChance;
    return s;
}

RackPlan makeRackPlan(const ParamStore& p, uint64_t seed)
{
    return makeRackPlan(rackSettings(p), seed);
}

RackPlan makeRackPlan(const RackSettings& st, uint64_t seed)
{
    RackPlan plan;
    plan.seed = seed;
    for (uint64_t& s : plan.layerSeed) s = seed;
    Rng rng;
    rng.seed(mixSeed(seed, 0x5241434Bull));   // "RACK"
    // The style's restlessness (RackPlan, Style.h), after the reference measurement's bar similarity (PLAN 13.4):
    // Hypnotic 0.87, Raw 0.92 and Dub 0.93, Ostgut 0.955 (most of the motion rolled per block). Hypnotic rolled every
    // bar while the mix was dull (Phase 2); with the balance fitted to the references (Phase 3: highs +10 dB) the hats and
    // the ping weigh more in the onset profiles, and 0.15 meets 0.87 (0.84 .. 0.87 over three seeds): its difference is
    // the ping's cycles.
    plan.mutation = st.mutation;
    plan.motionScale = st.motionScale;
    plan.reroll = st.reroll;
    plan.fillChance = st.fillChance;
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
    plan.keyRoot = st.keyRoot;
    plan.scale = st.scale;
    plan.swing = st.swing;
    plan.humanizeMs = st.humanizeMs;
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
    // (Dok. 8.2: "1-2 Perc-Layer ... p 0.5 pro Track"); the others Euclidean with p 0.4 -- E(3,8) twice, or five or
    // seven onsets off the quarters -- or displaced with p 0.3.
    {
        LayerId candidates[] = { LayerId::TomConga, LayerId::Rim, LayerId::Shaker };
        for (int i = 2; i > 0; --i) std::swap(candidates[i], candidates[rng.below(i + 1)]);
        static const int kPeriods[] = { 3, 5, 6, 7, 12 };
        const int polys = rng.uniform() < st.polymeterChance ? (rng.uniform() < 0.5f ? 2 : 1) : 0;
        for (int c = 0; c < 3; ++c) {
            const int li = static_cast<int>(candidates[c]);
            if (c < polys) {
                const int period = kPeriods[rng.below(5)];
                const int k = std::max(1, static_cast<int>(std::lround(period / 3.0)));
                plan.period[li] = period;
                plan.resetBars[li] = 16;
                plan.cycle[li] = rotateMask(euclidMask(k, period), period, rng.below(period));
            } else if (rng.uniform() < 0.4f) {
                const int kind = rng.below(3);
                if (kind == 0) {
                    // E(3,8) twice in a bar, at a rotation that keeps every onset off the quarters, searched from a
                    // random start.
                    const uint64_t base = euclidMask(3, 8) | (euclidMask(3, 8) << 8);
                    const int start = rng.below(16);
                    for (int r = 0; r < 16; ++r) {
                        const uint64_t m = rotateMask(base, 16, start + r);
                        if ((m & 0x1111u) == 0) { plan.euclid[li] = static_cast<uint16_t>(m); break; }
                    }
                } else {
                    // Five or seven onsets: E(5,16) and E(7,16) have one on every place of the beat, no rotation
                    // keeps them off the quarters -- until 01.10.2026 these two of the three draws fell back to the
                    // matrix unseen (the rim's .4, .2, .2: under one hit a bar). E(5,12) and E(7,12) on the twelve
                    // sixteenths beside the quarters instead (one draw for the rotation, as before).
                    plan.euclid[li] = offQuarterMask(kind == 1 ? 5 : 7, rng.below(12));
                }
            } else if (rng.uniform() < 0.3f) {
                plan.displace[li] = 1 + rng.below(3);
            }
        }
        if (rng.uniform() < 0.3f) plan.displace[static_cast<int>(LayerId::ClapGhost)] = 1 + rng.below(3);
    }
    // Phase 3: the harmony (Dok. 8.6). The chord in close position from the root in 220 .. 415 Hz.
    {
        plan.chordRoot = 57 + ((plan.keyRoot - 9 + 12) % 12);
        const float c = rng.uniform();
        const bool noNinth = !inScale(plan.scale, 2);   // Phrygian and the pentatonic
        if (c < 0.5f) { plan.nChordTones = 3; plan.chordTones[0] = 0; plan.chordTones[1] = 3; plan.chordTones[2] = 7; }
        else if (c < 0.8f || (c < 0.9f && noNinth)) {
            plan.nChordTones = 4; plan.chordTones[0] = 0; plan.chordTones[1] = 3; plan.chordTones[2] = 7; plan.chordTones[3] = 10;
        } else if (c < 0.9f) {   // add9 (where the scale has no ninth, the seventh above)
            plan.nChordTones = 4; plan.chordTones[0] = 0; plan.chordTones[1] = 3; plan.chordTones[2] = 7; plan.chordTones[3] = 14;
        } else {                 // the fourth in the bass
            plan.nChordTones = 4; plan.chordTones[0] = -7; plan.chordTones[1] = 0; plan.chordTones[2] = 3; plan.chordTones[3] = 7;
        }
        if (rng.uniform() < 0.2f) {
            const float s = rng.uniform();
            int shuttle = s < 0.3f ? 1 : (s < 0.55f ? 10 : (s < 0.8f ? 5 : 3));   // bII, bVII, iv, bIII
            if (!inScale(plan.scale, shuttle)) shuttle = 10;
            plan.shuttle = shuttle > 6 ? shuttle - 12 : shuttle;
            plan.shuttleBars = rng.uniform() < 0.5f ? 2 : 4;
            // A triad in the scale's thirds on the shuttle's degree: every second degree up (a quartal shape in the
            // pentatonic), so the second chord stays in the key and carries no major seventh.
            const ScaleDef& d = scaleDef(plan.scale);
            int deg = 0;
            while (deg < d.size && d.steps[deg] != shuttle) ++deg;
            for (int t = 0; t < 3; ++t) {
                const int k = deg + 2 * t;
                plan.shuttleTones[t] = plan.shuttle + d.steps[k % d.size] + 12 * (k / d.size) - d.steps[deg];
            }
        }
        plan.droneNote = plan.chordRoot + (rng.uniform() < 0.7f ? 0 : 7);
        plan.acidRoot = plan.bassRoot + 12;
        if (plan.acidRoot < 45) plan.acidRoot += 12;   // from A2 on: the 303 speaks through its harmonics, over the sub
    }
    // Trig conditions (Dok. 8.2): an extra open hat in the second of four bars, a single shaker in the third.
    plan.conds[plan.nConds++] = TrigCond{ LayerId::OpenHat, rng.uniform() < 0.5f ? 14 : 10, 2, 4 };
    plan.conds[plan.nConds++] = TrigCond{ LayerId::Shaker, rng.uniform() < 0.5f ? 13 : 5, 3, 4 };
    return plan;
}

void makeFigure(RackPlan& plan, LayerId voice, uint64_t seed)
{
    plan.figure = voice;
    plan.figBars = 1;
    plan.figOn = plan.figAccent = plan.figSlide = 0;
    plan.figLength = 0.5;
    for (int8_t& p : plan.figPitch) p = 0;
    if (voice == LayerId::Count) return;
    Rng rng;
    rng.seed(mixSeed(seed, 0x464947555245ull));   // "FIGURE"
    const auto weighted = [&](const float* w, int n) {
        float sum = 0.0f;
        for (int i = 0; i < n; ++i) sum += w[i];
        float u = rng.uniform() * sum;
        for (int i = 0; i < n; ++i) { if (u < w[i]) return i; u -= w[i]; }
        return n - 1;
    };
    const auto set = [](uint32_t& mask, int pos, bool on) { if (on) mask |= 1u << pos; else mask &= ~(1u << pos); };
    const auto has = [](uint32_t mask, int pos) { return ((mask >> pos) & 1u) != 0; };
    const auto inKey = [&](int iv) { return inScale(plan.scale, ((iv % 12) + 12) % 12); };

    switch (voice) {
    case LayerId::Acid: {
        // A 303 line on the off-quarter sixteenths (the quarters are the kick's): its alphabet, accents, slides.
        plan.figBars = rng.uniform() < 0.45f ? 1 : 2;
        const float density = 0.5f + 0.25f * rng.uniform();
        const auto pitch = [&]() {
            const float u = rng.uniform();
            const int iv = u < 0.45f ? 0 : u < 0.65f ? 12 : u < 0.75f ? 7 : u < 0.85f ? 10 : u < 0.95f ? 3 : 5;
            return inKey(iv) ? iv : 0;
        };
        const auto rollPlace = [&](int pos) {
            set(plan.figOn, pos, rng.uniform() < density);
            plan.figPitch[pos] = static_cast<int8_t>(pitch());
            set(plan.figAccent, pos, rng.uniform() < 0.3f);
        };
        for (int s = 0; s < kSteps; ++s) if (s % 4 != 0) rollPlace(s);
        for (int tries = 0; std::popcount(plan.figOn & 0xFFFFu) < 5 && tries < 64; ++tries) {
            const int s = 1 + rng.below(15);
            if (s % 4 != 0) set(plan.figOn, s, true);
        }
        if (plan.figBars == 2) {
            // The answer: the first bar again, two to four places rolled anew.
            for (int s = 0; s < kSteps; ++s) {
                set(plan.figOn, 16 + s, has(plan.figOn, s));
                set(plan.figAccent, 16 + s, has(plan.figAccent, s));
                plan.figPitch[16 + s] = plan.figPitch[s];
            }
            const int changes = 2 + rng.below(3);
            for (int c = 0; c < changes; ++c) {
                int s = 1 + rng.below(15);
                if (s % 4 == 0) ++s;
                rollPlace(16 + s);
            }
        }
        // The line starts from the root, wherever its first sixteenth falls.
        for (int pos = 0; pos < 16; ++pos) if (has(plan.figOn, pos)) { plan.figPitch[pos] = 0; break; }
        // Slides into a sounding sixteenth (the gate holds over).
        const int places = plan.figBars * kSteps;
        for (int pos = 0; pos < places; ++pos) {
            const int next = (pos + 1) % places;
            if (has(plan.figOn, pos) && has(plan.figOn, next)) set(plan.figSlide, pos, rng.uniform() < 0.2f);
        }
        break;
    }
    case LayerId::Chord: {
        // The stab's rhythm (the steps of one or two bars) and its length in beats.
        struct Rhythm { float w; int bars; uint32_t on; double length; };
        static const Rhythm kRhythms[] = {
            { 0.20f, 1, (1u << 2) | (1u << 6) | (1u << 10) | (1u << 14), 0.5 },                                  // the offbeat eighths
            { 0.15f, 1, (1u << 0) | (1u << 10), 0.5 },                                                             // Dok. 8.2's stab
            { 0.15f, 1, (1u << 0) | (1u << 3) | (1u << 6) | (1u << 8) | (1u << 11) | (1u << 14), 0.4 },           // 3-3-2 twice
            { 0.10f, 1, (1u << 3) | (1u << 10), 0.5 },                                                             // a late pair
            { 0.15f, 1, (1u << 0), 3.0 },                                                                          // one long chord
            { 0.15f, 2, (1u << 0) | (1u << 6) | (1u << 10) | (1u << 19) | (1u << 26), 0.5 },                       // call, answer
            { 0.10f, 1, 0, 0.4 },                                                                                  // E(5,16)
        };
        float w[7];
        for (int i = 0; i < 7; ++i) w[i] = kRhythms[i].w;
        const Rhythm& r = kRhythms[weighted(w, 7)];
        plan.figBars = r.bars;
        plan.figLength = r.length;
        plan.figOn = r.on;
        if (plan.figOn == 0) {
            // E(5,16) rotated off the downbeat, from a random start.
            const uint64_t e = euclidMask(5, 16);
            const int start = 1 + rng.below(15);
            for (int k = 0; k < 16 && plan.figOn == 0; ++k) {
                const uint64_t m = rotateMask(e, 16, start + k);
                if ((m & 1u) == 0) plan.figOn = static_cast<uint32_t>(m);
            }
        }
        for (int pos = 0; pos < plan.figBars * kSteps; ++pos) if (has(plan.figOn, pos)) { set(plan.figAccent, pos, true); break; }
        break;
    }
    case LayerId::Bass: {
        struct Riff { float w; uint32_t on; };
        static const Riff kRiffs[] = {
            { 0.30f, (1u << 2) | (1u << 6) | (1u << 10) | (1u << 14) },                                                      // offbeat
            { 0.20f, (1u << 2) | (1u << 3) | (1u << 6) | (1u << 7) | (1u << 10) | (1u << 11) | (1u << 14) | (1u << 15) },   // gallop
            { 0.20f, 0xEEEEu },                                                                                             // rolling
            { 0.15f, (1u << 3) | (1u << 6) | (1u << 10) | (1u << 13) },                                                     // syncopated
            { 0.15f, (1u << 2) | (1u << 7) | (1u << 10) },                                                                  // sparse
        };
        float w[5];
        for (int i = 0; i < 5; ++i) w[i] = kRiffs[i].w;
        const uint32_t riff = kRiffs[weighted(w, 5)].on;
        plan.figBars = rng.uniform() < 0.3f ? 2 : 1;
        plan.figOn = riff | (plan.figBars == 2 ? riff << 16 : 0u);
        std::vector<int> onsets;
        for (int pos = 0; pos < plan.figBars * kSteps; ++pos) if (has(plan.figOn, pos)) onsets.push_back(pos);
        // The octave on one or two places, the alphabet's second tone on the motif's last onset.
        for (int k = 0; k < 2; ++k)
            if (rng.uniform() < 0.3f) plan.figPitch[onsets[static_cast<size_t>(rng.below(static_cast<int>(onsets.size())))]] = 12;
        // (The alphabet's b2 and b5 only where the scale has them; else the fifth.)
        if (plan.bassSize > 1) plan.figPitch[onsets.back()] = static_cast<int8_t>(inKey(plan.bassSet[1]) ? plan.bassSet[1] : 7);
        for (int pos : onsets) if (pos % kSteps == 2 || pos % kSteps == 3) set(plan.figAccent, pos, true);
        break;
    }
    case LayerId::Ping: {
        const int li = static_cast<int>(LayerId::Ping);
        const float u = rng.uniform();
        const int period = u < 0.35f ? 16 : u < 0.55f ? 12 : u < 0.8f ? 7 : 5;
        const int k = period == 16 ? 5 + rng.below(3) : period == 12 ? 4 + rng.below(2) : period == 7 ? 3 : 2;
        plan.period[li] = period;
        plan.resetBars[li] = 16;
        plan.cycle[li] = rotateMask(euclidMask(k, period), period, rng.below(period));
        static const int kSecond[] = { 7, 10, 12, 3, 5 };
        static const float kSecondW[] = { 0.35f, 0.25f, 0.20f, 0.10f, 0.10f };
        int second = kSecond[weighted(kSecondW, 5)];
        if (!inKey(second)) second = 7;
        static const int kThird[] = { 3, 5, 7, 10, 12, 15 };
        int third = kThird[rng.below(6)];
        if (third == second || !inKey(third)) third = second == 12 ? 7 : 12;
        bool first = true;
        for (int pos = 0; pos < period; ++pos) {
            const float v = rng.uniform();
            const bool onset = ((plan.cycle[li] >> pos) & 1u) != 0;
            const int iv = (onset && first) ? 0 : v < 0.45f ? 0 : v < 0.8f ? second : third;
            if (onset) first = false;
            plan.pingNote[pos] = plan.pingRoot + iv;
        }
        break;
    }
    default:
        plan.figure = LayerId::Count;
        break;
    }
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

void rollSteps(const RackPlan& plan, LayerId id, int bar, int block, float density, bool* on, const bool* busy, uint32_t variant)
{
    const int li = static_cast<int>(id);
    const uint64_t sd = seedOf(plan, li, variant);
    const LayerDef& d = kLayers[li];
    for (int s = 0; s < kSteps; ++s) on[s] = false;
    if (density <= 0.0f) return;

    // Phase 8: the figure plays its motif, the same in every pass (the ping's figure is its cycle, below).
    if (id == plan.figure && id != LayerId::Ping && plan.figOn != 0) {
        const int base = (bar % std::max(1, plan.figBars)) * kSteps;
        for (int s = 0; s < kSteps; ++s) on[s] = ((plan.figOn >> (base + s)) & 1u) != 0;
        return;
    }
    if (isCyclic(plan, li)) {
        // The cycle runs against the bar from its last reset; density thins it the same way in every pass.
        for (int s = 0; s < kSteps; ++s) {
            const int pos = cyclePos(plan, li, bar, s);
            if (!(plan.cycle[li] & (uint64_t(1) << pos))) continue;
            on[s] = density >= 1.0f || roll(sd, li, kCycle, static_cast<uint64_t>(block) * 64 + pos) < density;
        }
        return;
    }
    if (plan.euclid[li] != 0) {
        for (int s = 0; s < kSteps; ++s) {
            if (!(plan.euclid[li] & (1u << s))) continue;
            on[s] = density >= 1.0f || roll(sd, li, kBase, static_cast<uint64_t>(block) * 16 + s) < density;
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
                u = roll(sd, li, kBase, static_cast<uint64_t>(block) * 16 + s);
                break;
            case LayerKind::Loop: {
                // Bar 1 (and 3) of the loop is the base roll; bars 2 and 4 re-roll the style's share of the open steps
                // (RackPlan::mutation; Dok. 8.2's "Mutation nur in Takt 2 bzw. 2/4").
                const int pos = (bar % 32) % std::max(1, plan.loopBars[li]);
                u = roll(sd, li, kBase, static_cast<uint64_t>(block) * 16 + s);
                if (pos == 1 || pos == 3) {
                    const uint64_t salt = pos == 1 ? kMutA : kMutB;
                    if (roll(sd, li, salt, static_cast<uint64_t>(block) * 64 + s) < plan.mutation)
                        u = roll(sd, li, salt, static_cast<uint64_t>(block) * 64 + 16 + s);
                }
                break;
            }
            default:
                // The ghost chain (Erg. 7): the layer's own last two steps, the others' ghosts, the half bar before. A step
                // is rolled anew this bar with the style's reroll share, else it keeps its roll of the block.
                u = roll(sd, li, kMotion, static_cast<uint64_t>(bar) * 16 + s);
                if (plan.reroll < 1.0f && roll(sd, li, kMutB, static_cast<uint64_t>(bar) * 16 + s) >= plan.reroll)
                    u = roll(sd, li, kMotion, 0x100000000ull + static_cast<uint64_t>(block) * 16 + s);
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
        // Phase 10: a loop that plays is heard -- a loop bar its rolls left empty keeps its likeliest step (the rim's
        // three chances of .4, .2 and .2 left it silent for whole blocks, 28.09.2026).
        if (d.kind == LayerKind::Loop && density >= 1.0f) {
            bool any = false;
            for (int s = 0; s < kSteps; ++s) any = any || on[s];
            int best = -1;
            for (int s = 0; s < kSteps && !any; ++s) if (d.p[s] > 0.0f && (best < 0 || d.p[s] > d.p[best])) best = s;
            if (best >= 0) on[best] = true;
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
        rollSteps(plan, table, spec.bar, spec.block, spec.density[i], on[i], motion ? busy : nullptr, spec.variant);
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
        // The tonal layers that are no bass may sit on the quarters: the chord's stab on the downbeat, the drone, the texture.
        if (id == LayerId::Chord || id == LayerId::Drone || id == LayerId::Texture) continue;
        for (int s = 0; s < kSteps; s += 4) on[i][s] = false;
    }
    // The drone speaks every eighth bar.
    if ((spec.bar % 8) != 0) for (int s = 0; s < kSteps; ++s) on[L(LayerId::Drone)][s] = false;
    // One hat per step.
    for (int s = 0; s < kSteps; ++s) {
        if (on[L(LayerId::OpenHat)][s]) { on[L(LayerId::ClosedHat)][s] = false; on[L(LayerId::RollingHat)][s] = false; }
        if (on[L(LayerId::ClosedHat)][s]) on[L(LayerId::RollingHat)][s] = false;
    }
    // A fill: the tom or conga rolls through the sixteenths after the last quarter of an eight-bar phrase (p fillChance;
    // the quarter stays the kick's), never a snare roll.
    bool fill = false;
    if (spec.fills && spec.active[L(LayerId::TomConga)] && (spec.bar % 8) == 7
        && roll(seedOf(plan, L(LayerId::TomConga), spec.variant), L(LayerId::TomConga), kFill, static_cast<uint64_t>(spec.bar)) < plan.fillChance) {
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
        // Phase 18: the layers no rule ties to the kit (kick, bass, ping, chord, 303, drone) draw their spread and jitter
        // apart from the block's candidate, so a kit layer drawn again leaves them bit for bit (it may pick another
        // candidate). Every track has such a voice now.
        const bool untied = id == LayerId::Kick || id == LayerId::Bass || id == LayerId::Ping || id == LayerId::Chord
                         || id == LayerId::Acid || id == LayerId::Drone;
        const uint32_t variant = untied ? 0u : spec.variant;
        for (int s = 0; s < kSteps; ++s) {
            if (!on[i][s]) continue;
            NoteEvent n;
            // Velocity: the step's (a cyclic or Euclidean layer: its loudest step's), a random spread, the collision dip.
            const float base = pattern || d.vel[s] <= 0.0f ? baseVelocity(d) : d.vel[s];
            float vel = base + d.velRandom * (2.0f * roll(seedOf(plan, i, variant), i, kVel, static_cast<uint64_t>(spec.bar) * 16 + s) - 1.0f);
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
                const double u1 = std::max(1.0e-9, static_cast<double>(roll(seedOf(plan, i, variant), i, kJitter, static_cast<uint64_t>(spec.bar) * 32 + s)));
                const double u2 = roll(seedOf(plan, i, variant), i, kJitter, static_cast<uint64_t>(spec.bar) * 32 + 16 + s);
                const double g = std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2);
                offMs += std::min(10.0, std::fabs(g) * plan.humanizeMs);
            }
            if (!kickLike) beat += offMs * msToBeats;
            n.beat = beat;
            n.length = d.length;
            // Phase 8: the figure's place in its motif; its accents lift the velocity.
            const bool figure = id == plan.figure && id != LayerId::Ping && plan.figOn != 0;
            const int fig = (spec.bar % std::max(1, plan.figBars)) * kSteps + s;
            if (figure) vel = baseVelocity(d) * (((plan.figAccent >> fig) & 1u) ? 1.12f : 0.9f)
                            + 0.5f * d.velRandom * (2.0f * roll(seedOf(plan, i, 0), i, kVel, static_cast<uint64_t>(spec.bar) * 16 + s) - 1.0f);
            n.velocity = clampv(vel, 0.05f, 1.0f);
            if (figure && id == LayerId::Bass) {
                n.part = Part::Bass;
                n.pitch = plan.bassRoot + plan.figPitch[fig];
            } else if (figure && id == LayerId::Acid) {
                n.part = Part::Acid;
                int iv = plan.figPitch[fig];
                if (!inScale(plan.scale, iv % 12) || !(plan.acidMask & (1u << (iv % 12)))) iv = iv >= 12 ? 12 : 0;
                n.pitch = plan.acidRoot + iv;
                n.accent = ((plan.figAccent >> fig) & 1u) != 0;
                n.slide = ((plan.figSlide >> fig) & 1u) != 0;
                if (n.slide) n.length = 0.3;
            } else if (figure && id == LayerId::Chord) {
                const bool other = plan.shuttleBars > 0 && (spec.bar % plan.shuttleBars) == plan.shuttleBars - 1;
                const int nt = other ? 3 : plan.nChordTones;
                for (int t = 0; t < nt; ++t) {
                    NoteEvent c = n;
                    c.part = Part::Chord;
                    c.length = plan.figLength;
                    c.pitch = plan.chordRoot + (other ? plan.shuttleTones[t] : plan.chordTones[t]);
                    out.push_back(c);
                }
                continue;
            } else if (kickLike) {
                n.part = Part::Kick;
                n.pitch = 36;
            } else if (id == LayerId::Bass) {
                n.part = Part::Bass;   // the synth; where the sub owns the low end the engine plays the sine too
                // Root ostinato; the alphabet's other tones only on the last two sixteenths (Dok. 8.6).
                int interval = 0;
                if (plan.bassSize > 1 && s >= 14) {
                    const int k = 1 + static_cast<int>(roll(seedOf(plan, i, spec.variant), i, kBase, 1000 + static_cast<uint64_t>(spec.bar) * 2 + (s - 14)) * static_cast<float>(plan.bassSize - 1));
                    interval = plan.bassSet[std::min(k, plan.bassSize - 1)];
                    if (!inScale(plan.scale, interval % 12)) interval = 7;   // (b2, b5 only in a scale that has them)
                }
                n.pitch = plan.bassRoot + interval;
            } else if (id == LayerId::Ping) {
                n.part = Part::Ping;
                n.pitch = plan.pingNote[cyclePos(plan, i, spec.bar, s)];
            } else if (id == LayerId::Chord) {
                // Every tone of the chord (or of the shuttle's, in its bars), each a note of its own.
                const bool other = plan.shuttleBars > 0 && (spec.bar % plan.shuttleBars) == plan.shuttleBars - 1;
                const int nt = other ? 3 : plan.nChordTones;
                for (int t = 0; t < nt; ++t) {
                    NoteEvent c = n;
                    c.part = Part::Chord;
                    c.pitch = plan.chordRoot + (other ? plan.shuttleTones[t] : plan.chordTones[t]);
                    out.push_back(c);
                }
                continue;
            } else if (id == LayerId::Drone) {
                n.part = Part::Drone;
                n.pitch = plan.droneNote;
                n.length = 32.0;
            } else if (id == LayerId::Texture) {
                n.part = Part::Texture;
                n.pitch = 60;
                n.length = 4.0;
            } else if (id == LayerId::Acid) {
                // Pitch, accent and slide of this step, rolled once per block: the root 0.45, its octave 0.2, the fifth,
                // the flat seventh and the minor third 0.1 each, within the scale (Dok. 8.6's alphabet, a 303 idiom).
                n.part = Part::Acid;
                const uint64_t key = static_cast<uint64_t>(spec.block) * 16 + s;
                const float u = roll(seedOf(plan, i, spec.variant), i, kAcid, key);
                int iv = u < 0.45f ? 0 : (u < 0.65f ? 12 : (u < 0.75f ? 7 : (u < 0.85f ? 10 : (u < 0.95f ? 3 : 5))));
                if (!inScale(plan.scale, iv) || !(plan.acidMask & (1u << (iv % 12)))) iv = 0;
                n.pitch = plan.acidRoot + iv;
                n.accent = roll(seedOf(plan, i, spec.variant), i, kAcid, 1000 + key) < 0.3f;
                n.slide = roll(seedOf(plan, i, spec.variant), i, kAcid, 2000 + key) < 0.2f;
                if (n.slide) n.length = 0.3;   // the gate holds into the next sixteenth
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

} // namespace tot
