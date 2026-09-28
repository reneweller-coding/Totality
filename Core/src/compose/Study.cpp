/**
 * @file Study.cpp
 * @brief The study: blocks, operations per style, the reduction, micro-automation, delay throws, the loudness mark.
 */
#include "tot/compose/Study.h"
#include "tot/Dsp.h"
#include "tot/Leveler.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace tot {

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

/** @brief Whether a layer is tonal (none in the first and last 32 bars, Dok. 8.5). */
bool tonal(LayerId id)
{
    return id == LayerId::Bass || id == LayerId::Acid || id == LayerId::Chord || id == LayerId::Drone
        || id == LayerId::Texture || id == LayerId::Ping;
}

bool anyLayer(LayerId) { return true; }

} // namespace

Score composeStudy(const ParamStore& p, uint64_t seed)
{
    const float bpm = p.get(p.id(Module::Compose, 0, compose::Bpm));
    const float minutes = p.get(p.id(Module::Compose, 0, compose::Minutes));
    const LowOwner owner = static_cast<LowOwner>(p.getInt(p.id(Module::Compose, 0, compose::LowOwner)));
    const bool subOwns = owner == LowOwner::Sub;
    const Style style = static_cast<Style>(p.getInt(p.id(Module::Compose, 0, compose::Style)));

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
    auto shuffled = [&rng](std::vector<LayerId> v) {
        for (int i = static_cast<int>(v.size()) - 1; i > 0; --i) std::swap(v[static_cast<size_t>(i)], v[static_cast<size_t>(rng.below(i + 1))]);
        return v;
    };
    auto append = [](std::vector<LayerId>& to, const std::vector<LayerId>& from) { to.insert(to.end(), from.begin(), from.end()); };

    // The layers a body block may add, in order, per style (PLAN 2.8; Dok. 8.5's order: hats and ride, clap and perc,
    // bass, stab, pad and texture -- each style bringing its signature early).
    std::vector<LayerId> queue;
    switch (style) {
    case Style::Hypnotic:   // the ping's cyclic figure, a drone; clap and chord seldom
        queue = { LayerId::OpenHat, LayerId::Ping, LayerId::Drone, LayerId::RollingHat };
        append(queue, shuffled({ LayerId::Shaker, LayerId::TomConga, LayerId::Rim, LayerId::Ride }));
        queue.push_back(LayerId::Texture);
        if (rng.uniform() < 0.3f) queue.push_back(LayerId::ClapA);
        if (rng.uniform() < 0.2f) queue.push_back(LayerId::Chord);
        break;
    case Style::Dub:        // the chord with its full chain first; the hats stay reduced (the rolling hat at half)
        queue = { LayerId::Chord, LayerId::OpenHat, LayerId::Texture };
        append(queue, shuffled({ LayerId::Shaker, LayerId::Rim }));
        if (rng.uniform() < 0.3f) queue.push_back(LayerId::Drone);
        break;
    case Style::RawPeak:    // dense: sixteenth hats, toms, clap; the 303 now and then
        queue = { LayerId::OpenHat, LayerId::RollingHat, LayerId::ClapA, LayerId::TomConga };
        if (rng.uniform() < 0.4f) queue.push_back(LayerId::Acid);
        append(queue, shuffled({ LayerId::Rim, LayerId::Shaker, LayerId::Ride }));
        break;
    default:                // Ostgut: clap on 2 and 4, the ride's eighths; stabs sparingly, a 303 seldom
        queue = { LayerId::OpenHat, LayerId::ClapA, LayerId::RollingHat, LayerId::Ride };
        if (rng.uniform() < 0.5f) queue.push_back(LayerId::Chord);
        else if (rng.uniform() < 0.5f) queue.push_back(LayerId::Acid);
        append(queue, shuffled({ LayerId::Shaker, LayerId::TomConga, LayerId::Rim }));
        break;
    }

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
    const int chordLevel = p.id(Module::Chord, 0, chord::Level);
    const int chordBand = p.id(Module::Chord, 0, chord::Band);
    const int chordDub = p.id(Module::Chord, 0, chord::DubSend);
    const int acidCut = p.id(Module::Acid, 0, synth::Cutoff);
    const int feedback = p.id(Module::Dub, 0, dub::Feedback);
    const int pingEcho = p.id(Module::Dub, 0, dub::PingSend);
    const int hatsEcho = p.id(Module::Dub, 0, dub::HatsSend);
    const int cloudLevel = p.id(Module::Cloud, 0, cloud::Level);
    float band = p.get(chordBand), acid = p.get(acidCut);

    auto beatOf = [](int bar) { return 4.0 * bar; };
    auto op = [&](int bar, OpKind kind, LayerId layer, LayerId other = LayerId::Count) {
        BlockOp o;
        o.beat = beatOf(bar);
        o.kind = kind;
        o.layer = layer == LayerId::Count ? -1 : L(layer);
        o.other = other == LayerId::Count ? -1 : L(other);
        sc.ops.push_back(o);
    };
    // What enters, and the curves that come with it.
    auto enter = [&](int bar, LayerId add) {
        if (add == LayerId::RollingHat) density[L(add)] = 1.0f;
        else active[L(add)] = true;
        entered.push_back(add);
        op(bar, OpKind::Add, add);
        if (add == LayerId::OpenHat)
            sc.gestures.push_back(ramp(p, ohDecay, beatOf(bar), beatOf(16), 100.0f, 400.0f, GestureShape::Linear, 1));
        if (add == LayerId::Chord) {
            // "Nudging louder into the mix every few bars" (Dok. 5): 9 dB under the knob, a step up every four bars.
            const float k = p.get(chordLevel);
            for (int s = 0; s < 4; ++s) {
                const float v = k - 9.0f + 3.0f * static_cast<float>(s);
                sc.gestures.push_back(ramp(p, chordLevel, beatOf(bar + 4 * s), 0.0, v, v, GestureShape::Step, 0));
            }
        }
    };
    // Removes the latest-entered active layer that @p pick accepts; false if there is none.
    auto leave = [&](int bar, bool (*pick)(LayerId)) {
        for (auto it = entered.rbegin(); it != entered.rend(); ++it) {
            const LayerId id = *it;
            if (id == LayerId::Kick || id == LayerId::ClosedHat || id == LayerId::RollingHat || !active[L(id)] || !pick(id)) continue;
            active[L(id)] = false;
            op(bar, OpKind::Remove, id);
            return true;
        }
        return false;
    };

    // Whole-track automation: the rumble's hall grows by a fifth (Dok. 8.5, "Track-Drift").
    {
        const float d0 = p.get(rumbleDecay);
        sc.gestures.push_back(ramp(p, rumbleDecay, 0.0, beatOf(bars), d0, d0 * 1.2f, GestureShape::Linear, 1));
    }
    // The intro: the hats' bus opens over bars 8 .. 32.
    sc.gestures.push_back(ramp(p, hatsCut, beatOf(8), beatOf(24), 1500.0f, 20000.0f, GestureShape::Linear, 0));

    // The loudness mark: the track's loudest part is the block after the reduction (the densest), else the body's last.
    {
        const int peakBlock = reductionBlock + 1 <= bodyLast ? reductionBlock + 1 : bodyLast;
        LevelMark lm{ 0.0, beatOf(peakBlock * 32), styleTargetLufs(static_cast<int>(style)), 0.0f };
        lm.styleMix[static_cast<size_t>(std::clamp(static_cast<int>(style), 0, 3))] = 1.0f;
        sc.levels.push_back(lm);
    }

    const float throwChance = style == Style::Dub ? 0.6f : 0.3f;
    int queuePos = 0;
    for (int bar = 0; bar < bars; ++bar) {
        const int block = bar / 32, inBlock = bar % 32;
        const bool outro = block >= blocks - outroBlocks;
        const bool body = block >= bodyFirst && !outro;
        // --- the form's decisions at the block's first bar ---
        if (inBlock == 0) {
            sc.markers.push_back(Marker{ beatOf(bar), block == 0 ? std::string("Intro") : outro ? std::string("Outro")
                                                         : "Block " + std::to_string(block + 1) });
            if (block == 0) {
                active[L(LayerId::Kick)] = true;
                entered.push_back(LayerId::Kick);
                op(bar, OpKind::Start, LayerId::Kick);
            } else if (block == bodyFirst) {
                enter(bar, subOwns ? LayerId::Bass : LayerId::GhostKick);
            } else if (block == reductionBlock) {
                op(bar, OpKind::Hold, LayerId::Count);
            } else if (!outro) {
                if (queuePos < static_cast<int>(queue.size())) enter(bar, queue[static_cast<size_t>(queuePos++)]);
                else op(bar, OpKind::Hold, LayerId::Count);
            } else if (block == blocks - 1) {
                // The last block: kick and hats; the rolling hat thins, the hats' bus closes.
                bool any = false;
                for (int i = 0; i < kNumLayers; ++i) {
                    if (i == L(LayerId::Kick) || i == L(LayerId::ClosedHat) || i == L(LayerId::RollingHat) || !active[i]) continue;
                    if (!any) op(bar, OpKind::Remove, static_cast<LayerId>(i));
                    active[i] = false;
                    any = true;
                }
                if (!any) op(bar, OpKind::Hold, LayerId::Count);
                density[L(LayerId::RollingHat)] = 0.5f;
                sc.gestures.push_back(ramp(p, hatsCut, beatOf(bar + 16), beatOf(16), 20000.0f, 2500.0f, GestureShape::Linear, 0));
            }
        }
        // The outro's blocks before the last: the layers leave in the reverse order of their entry, the tonal ones first
        // (none in the last 32 bars, Dok. 8.5), one every eight bars.
        if (outro && block < blocks - 1 && inBlock % 8 == 0) {
            if (!leave(bar, tonal) && !leave(bar, anyLayer) && inBlock == 0) op(bar, OpKind::Hold, LayerId::Count);
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
        if (inBlock % 16 == 0 && body) {
            const float d0 = p.get(rollDecay);
            const float from = d0 * (1.0f + 0.08f * (2.0f * rng.uniform() - 1.0f));
            const float to = d0 * (1.0f + 0.08f * (2.0f * rng.uniform() - 1.0f));
            sc.gestures.push_back(ramp(p, rollDecay, beatOf(bar), beatOf(16), from, to, GestureShape::MinimumJerk, 1));
        }
        // --- the meso level (Dok. 8.5's table): the stab's band wanders up to an octave either way of its knob over 16
        //     or 32 bars; the 303's cutoff makes its trips ---
        if (inBlock % 16 == 0 && body && active[L(LayerId::Chord)] && (inBlock == 0 || rng.uniform() < 0.5f)) {
            const float to = p.get(chordBand) * std::pow(2.0f, 2.0f * rng.uniform() - 1.0f);
            const int len = inBlock == 0 && rng.uniform() < 0.5f ? 32 : 16;
            sc.gestures.push_back(ramp(p, chordBand, beatOf(bar), beatOf(len), band, to, GestureShape::MinimumJerk, 1));
            band = to;
        }
        if (inBlock % 16 == 0 && body && active[L(LayerId::Acid)]) {
            const float to = 350.0f * std::pow(2.0f, 2.5f * rng.uniform());
            sc.gestures.push_back(ramp(p, acidCut, beatOf(bar), beatOf(16), acid, to, GestureShape::MinimumJerk, 1));
            acid = to;
        }
        // --- a delay throw at the end of a 16-bar phrase (Dok. 8.5, PLAN 7.4): from the third beat of its last bar a
        //     ping, a stab or the hats open into the echo, the feedback climbs to the edge and falls back over two bars ---
        if (body && inBlock % 16 == 15 && rng.uniform() < throwChance) {
            const double at = beatOf(bar) + 2.0;
            LayerId from = LayerId::ClosedHat;
            const float u = rng.uniform();
            if (active[L(LayerId::Ping)] && u < 0.5f) from = LayerId::Ping;
            else if (active[L(LayerId::Chord)] && u < 0.8f) from = LayerId::Chord;
            const float fb = p.get(feedback);
            sc.gestures.push_back(ramp(p, feedback, at, 2.0, fb, 0.85f, GestureShape::EaseIn, 0));
            sc.gestures.push_back(ramp(p, feedback, at + 2.0, 8.0, 0.85f, fb, GestureShape::EaseOut, 0));
            sc.gestures.push_back(home(feedback, at + 10.0));
            const int send = from == LayerId::Ping ? pingEcho : from == LayerId::Chord ? chordDub : hatsEcho;
            const float open = from == LayerId::ClosedHat ? 0.5f : 0.9f;
            sc.gestures.push_back(ramp(p, send, at, 0.0, open, open, GestureShape::Step, 1));
            sc.gestures.push_back(home(send, at + 2.0));
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
                // The cloud of the ping's and the chord's last seconds rises through the kick's absence (PLAN 5.7).
                if (active[L(LayerId::Ping)] || active[L(LayerId::Chord)])
                    sc.gestures.push_back(ramp(p, cloudLevel, beatOf(bar), beatOf(8), -60.0f, -3.0f, GestureShape::EaseIn, 0));
            }
            if (inBlock >= 16 && inBlock < 24) {
                spec.active[L(LayerId::Kick)] = false;
                spec.active[L(LayerId::GhostKick)] = false;
                spec.active[L(LayerId::Bass)] = false;
                spec.active[L(LayerId::Acid)] = false;
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
                if (active[L(LayerId::Ping)] || active[L(LayerId::Chord)]) {
                    sc.gestures.push_back(ramp(p, cloudLevel, beatOf(bar), beatOf(8), -3.0f, -60.0f, GestureShape::EaseOut, 0));
                    sc.gestures.push_back(home(cloudLevel, beatOf(bar + 8)));
                }
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

} // namespace tot
