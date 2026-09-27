/**
 * @file Study.cpp
 * @brief The Phase 1 study: blocks, operations, the reduction, micro-automation.
 */
#include "umb/compose/Study.h"
#include "umb/Dsp.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace umb {

namespace {

/** @brief An automation curve from @p fromValue to @p toValue (real units) as offsets from the knob's position. */
Gesture ramp(const ParamStore& p, int id, double beat, double length, float fromValue, float toValue,
             GestureShape shape = GestureShape::Linear, uint8_t hand = 0)
{
    const float knob = p.toNormalised(id, p.get(id));
    Gesture g;
    g.param = id;
    g.beat = beat;
    g.length = length;
    g.from = p.toNormalised(id, fromValue) - knob;
    g.to = p.toNormalised(id, toValue) - knob;
    g.shape = shape;
    g.hand = hand;
    return g;
}

/** @brief Back to the knob at @p beat. */
Gesture home(int id, double beat)
{
    Gesture g;
    g.param = id;
    g.beat = beat;
    g.length = 0.0;
    g.shape = GestureShape::Step;
    return g;
}

int L(LayerId id) { return static_cast<int>(id); }

} // namespace

Score composeStudy(const ParamStore& p, uint64_t seed)
{
    const float bpm = p.get(p.id(Module::Compose, 0, compose::Bpm));
    const float minutes = p.get(p.id(Module::Compose, 0, compose::Minutes));
    const LowOwner owner = static_cast<LowOwner>(p.getInt(p.id(Module::Compose, 0, compose::LowOwner)));
    const bool subOwns = owner == LowOwner::Sub;

    Score sc;
    sc.clear(bpm);
    sc.seed = seed;
    sc.keyRoot = p.getInt(p.id(Module::Compose, 0, compose::Key));
    sc.scale = p.getInt(p.id(Module::Compose, 0, compose::Scale));

    const int blocks = std::max(4, static_cast<int>(std::lround(minutes * bpm / 4.0 / 32.0)));
    const int bars = blocks * 32;
    const int outroBlocks = blocks >= 7 ? 2 : 1;
    const int bodyFirst = 1, bodyLast = blocks - 1 - outroBlocks;
    const int bodyCount = bodyLast - bodyFirst + 1;
    const int reductionBlock = bodyFirst + static_cast<int>(std::lround(0.55 * (bodyCount - 1)));

    const RackPlan plan = makeRackPlan(p, mixSeed(seed, 1));
    Rng rng;
    rng.seed(mixSeed(seed, 2));

    // The layers a body block may add, in order: the open hat and the full rolling hat first, then the rest shuffled.
    std::vector<LayerId> pool = { LayerId::Shaker, LayerId::TomConga, LayerId::Rim, LayerId::Ride };
    for (int i = static_cast<int>(pool.size()) - 1; i > 0; --i) std::swap(pool[static_cast<size_t>(i)], pool[static_cast<size_t>(rng.below(i + 1))]);
    std::vector<LayerId> queue = { LayerId::OpenHat, LayerId::RollingHat };
    queue.insert(queue.end(), pool.begin(), pool.end());

    bool active[kNumLayers] = {};
    float density[kNumLayers];
    for (float& d : density) d = 1.0f;
    std::vector<LayerId> entered;   // in the order they came, for the outro's mirror

    const int hatsCut = p.id(Module::Mix, 0, mix::HatsCut);
    const int rollDecay = p.id(Module::Perc, 1, perc::NoiseDecay);
    const int ohDecay = p.id(Module::Perc, 2, perc::NoiseDecay);
    const int noiseLevel = p.id(Module::Perc, 11, perc::Level);
    const int noiseCut = p.id(Module::Perc, 11, perc::Cutoff);
    const int rumbleDecay = p.id(Module::Rumble, 0, rumble::Decay);

    auto beatOf = [](int bar) { return 4.0 * bar; };
    auto op = [&](int bar, OpKind kind, LayerId layer, LayerId other = LayerId::Count) {
        BlockOp o;
        o.beat = beatOf(bar);
        o.kind = kind;
        o.layer = layer == LayerId::Count ? -1 : L(layer);
        o.other = other == LayerId::Count ? -1 : L(other);
        sc.ops.push_back(o);
    };

    // Whole-track automation: the rumble's hall grows by a fifth (Dok. 8.5, "Track-Drift").
    {
        const float d0 = p.get(rumbleDecay);
        sc.gestures.push_back(ramp(p, rumbleDecay, 0.0, beatOf(bars), d0, d0 * 1.2f, GestureShape::Linear, 1));
    }
    // The intro: the hats' bus opens over bars 8 .. 32.
    sc.gestures.push_back(ramp(p, hatsCut, beatOf(8), beatOf(24), 1500.0f, 20000.0f, GestureShape::Linear, 0));

    int queuePos = 0;
    for (int bar = 0; bar < bars; ++bar) {
        const int block = bar / 32, inBlock = bar % 32;
        const bool outro = block >= blocks - outroBlocks;
        // --- the form's decisions at the block's first bar ---
        if (inBlock == 0) {
            sc.markers.push_back(Marker{ beatOf(bar), block == 0 ? std::string("Intro") : outro ? std::string("Outro")
                                                         : "Block " + std::to_string(block + 1) });
            if (block == 0) {
                active[L(LayerId::Kick)] = true;
                entered.push_back(LayerId::Kick);
                op(bar, OpKind::Start, LayerId::Kick);
            } else if (block == bodyFirst) {
                const LayerId add = subOwns ? LayerId::Bass : LayerId::GhostKick;
                active[L(add)] = true;
                entered.push_back(add);
                op(bar, OpKind::Add, add);
            } else if (block == reductionBlock) {
                op(bar, OpKind::Hold, LayerId::Count);
            } else if (!outro) {
                if (queuePos < static_cast<int>(queue.size())) {
                    const LayerId add = queue[static_cast<size_t>(queuePos++)];
                    if (add == LayerId::RollingHat) density[L(add)] = 1.0f;
                    else active[L(add)] = true;
                    entered.push_back(add);
                    op(bar, OpKind::Add, add);
                    if (add == LayerId::OpenHat)
                        sc.gestures.push_back(ramp(p, ohDecay, beatOf(bar), beatOf(16), 100.0f, 400.0f, GestureShape::Linear, 1));
                } else {
                    op(bar, OpKind::Hold, LayerId::Count);
                }
            } else {
                // The outro: the layers leave in the reverse order of their entry, the bass first (no tonal
                // material in the last 32 bars, Dok. 8.5).
                int removed = 0;
                if (active[L(LayerId::Bass)]) { active[L(LayerId::Bass)] = false; op(bar, OpKind::Remove, LayerId::Bass); ++removed; }
                for (auto it = entered.rbegin(); it != entered.rend() && removed < 2; ++it) {
                    const LayerId id = *it;
                    if (id == LayerId::Kick || id == LayerId::ClosedHat || id == LayerId::RollingHat || !active[L(id)]) continue;
                    active[L(id)] = false;
                    op(bar, OpKind::Remove, id);
                    ++removed;
                }
                if (block == blocks - 1) {
                    for (int i = 0; i < kNumLayers; ++i)
                        if (i != L(LayerId::Kick) && i != L(LayerId::ClosedHat) && i != L(LayerId::RollingHat)) active[i] = false;
                    density[L(LayerId::RollingHat)] = 0.5f;
                    sc.gestures.push_back(ramp(p, hatsCut, beatOf(bar + 16), beatOf(16), 20000.0f, 2500.0f, GestureShape::Linear, 0));
                }
            }
        }
        // --- inside the intro ---
        if (bar == 8) { active[L(LayerId::ClosedHat)] = true; entered.push_back(LayerId::ClosedHat); op(bar, OpKind::Add, LayerId::ClosedHat); }
        if (bar == 16) {
            active[L(LayerId::RollingHat)] = true;
            density[L(LayerId::RollingHat)] = 0.5f;
            op(bar, OpKind::Add, LayerId::RollingHat);
        }
        // --- the last 16 bars: kick and the offbeat hat ---
        if (bar == bars - 16) {
            active[L(LayerId::RollingHat)] = false;
            op(bar, OpKind::Remove, LayerId::RollingHat);
        }
        // --- micro-events: every 16 bars in the body the rolling hat's decay moves a few per cent ---
        if (inBlock % 16 == 0 && block >= bodyFirst && !outro) {
            const float d0 = p.get(rollDecay);
            const float from = d0 * (1.0f + 0.08f * (2.0f * rng.uniform() - 1.0f));
            const float to = d0 * (1.0f + 0.08f * (2.0f * rng.uniform() - 1.0f));
            sc.gestures.push_back(ramp(p, rollDecay, beatOf(bar), beatOf(16), from, to, GestureShape::MinimumJerk, 1));
        }

        BarSpec spec = emptyBar(bar, beatOf(bar), bpm);
        for (int i = 0; i < kNumLayers; ++i) { spec.active[i] = active[i]; spec.density[i] = density[i]; }
        // --- the reduction: kick out for bars 16 .. 23, a swell, a one-bar dropout, the return ---
        if (block == reductionBlock) {
            if (inBlock == 16) {
                op(bar, OpKind::KickOut, LayerId::Kick);
                sc.markers.push_back(Marker{ beatOf(bar), "Reduction" });
                sc.gestures.push_back(ramp(p, noiseLevel, beatOf(bar), beatOf(7), -36.0f, -12.0f, GestureShape::EaseIn, 0));
                sc.gestures.push_back(ramp(p, noiseCut, beatOf(bar), beatOf(7), 600.0f, 6000.0f, GestureShape::EaseIn, 1));
            }
            if (inBlock >= 16 && inBlock < 24) {
                spec.active[L(LayerId::Kick)] = false;
                spec.active[L(LayerId::GhostKick)] = false;
                spec.active[L(LayerId::Bass)] = false;
                if (inBlock < 23) {
                    NoteEvent n;
                    n.beat = beatOf(bar);
                    n.length = 4.0;
                    n.part = percPart(11);
                    n.pitch = 49;
                    n.velocity = 0.9f;
                    sc.notes.push_back(n);
                } else {
                    for (bool& a : spec.active) a = false;   // the cut: one bar of nothing but the tails
                }
            }
            if (inBlock == 24) {
                op(bar, OpKind::Return, LayerId::Kick);
                sc.markers.push_back(Marker{ beatOf(bar), "Return" });
                sc.gestures.push_back(home(noiseLevel, beatOf(bar)));
                sc.gestures.push_back(home(noiseCut, beatOf(bar)));
            }
        }
        // "Mute one expected hit" before the first block's change.
        if (bar == 31) spec.muteStep[14] = true;
        realizeBar(plan, spec, sc.notes);
    }
    op(bars, OpKind::End, LayerId::Count);
    sc.lengthBeats = beatOf(bars);
    sc.sort();
    return sc;
}

} // namespace umb
