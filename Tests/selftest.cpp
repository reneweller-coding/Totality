/**
 * @file selftest.cpp
 * @brief umb_selftest: every building block measured against an independently derived value.
 *
 * Sections are registered in the table at the bottom; `umb_selftest --list` prints their names (ctest registers one
 * test per name) and `--only a,b` runs the named ones. Only checks that protect something real belong here (the
 * user's rule for Ephemeris, 24.09.2026).
 */
#include "umb/Clock.h"
#include "umb/Engine.h"
#include "umb/Loudness.h"
#include "umb/Midi.h"
#include "umb/Params.h"
#include "umb/Score.h"
#include "umb/compose/Study.h"
#include "umb/pattern/Rack.h"
#include "umb/synth/Kick.h"
#include "umb/synth/Kit.h"
#include "umb/synth/Rumble.h"
#include "umb/synth/SubBass.h"
#include "TestSupport.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace umb;
using namespace umbtest;

namespace {

constexpr double kPiD = 3.141592653589793;

/** The tempo map converts beats to seconds in closed form; checked against a numerical integral over a ramp. */
void testTempoMap()
{
    section("tempo map against a numerical integral");
    TempoMap m;
    m.setConstant(126.0);
    m.add(64.0, 126.0, true);
    m.add(1024.0, 134.0, false);
    double t = 0.0, worst = 0.0;
    const double db = 1.0 / 64.0;
    for (double b = 0.0; b < 1100.0; b += db) {
        t += 60.0 / m.bpmAt(b + 0.5 * db) * db;
        worst = std::max(worst, std::fabs(t - m.secondsAt(b + db)));
    }
    check(worst < 1.0e-6, "secondsAt equals the integral", fmt("%.2e s", worst));
    double inv = 0.0;
    for (double b = 0.0; b < 1100.0; b += 7.3) inv = std::max(inv, std::fabs(m.beatAt(m.secondsAt(b)) - b));
    check(inv < 1.0e-9, "beatAt inverts secondsAt", fmt("%.2e beats", inv));
}

/** The parameter store: text round trip, the kit's defaults, choices by name. */
void testParams()
{
    section("parameters");
    auto p = std::make_unique<ParamStore>();
    check(p->getInt(p->find("perc1.noise_type")) == static_cast<int>(NoiseType::Metal909), "the closed hat plays the metal table");
    check(p->getInt(p->find("perc5.role")) == static_cast<int>(PercRole::Clap), "lane 5 is the clap");
    p->parseText("compose.key=F#; kick.engine=909; perc3.decay=250");
    check(p->getInt(p->find("compose.key")) == 6 && p->getInt(p->find("kick.engine")) == 2, "choices by name");
    const std::string text = p->toText(true);
    auto q = std::make_unique<ParamStore>();
    q->parseText(text);
    bool same = true;
    for (int i = 0; i < p->count(); ++i) same = same && p->get(i) == q->get(i);
    check(same, "text form round-trips every value", fmt("%d parameters", p->count()));
}

/** Renders a kick triggered at @p late with the store's kick settings; returns the output. */
std::vector<float> renderKick(const ParamStore& p, int keyRoot, double late, int n, Kick& k, std::vector<float>* body = nullptr)
{
    float v[64];
    p.readModule(Module::Kick, 0, v);
    Kick::constrain(v, 0.0, keyRoot);
    k.prepare(48000.0);
    k.update(v, keyRoot);
    k.trigger(1.0f, late);
    std::vector<float> out(static_cast<size_t>(n)), b(static_cast<size_t>(n));
    k.process(out.data(), b.data(), n);
    if (body != nullptr) *body = b;
    return out;
}

/** Phase of @p x at @p hz over [from, from + len) samples, relative to the sample grid's t = 0, in cycles. */
double phaseAt(const std::vector<float>& x, double hz, int from, int len, double sr, double t0Samples)
{
    double re = 0.0, im = 0.0;
    for (int i = from; i < from + len; ++i) {
        const double t = (i + t0Samples) / sr;
        re += x[static_cast<size_t>(i)] * std::cos(2.0 * kPiD * hz * t);
        im += x[static_cast<size_t>(i)] * std::sin(2.0 * kPiD * hz * t);
    }
    // x ~ A sin(2 pi (f t + c)) = A (sin(2 pi f t) cos(2 pi c) + cos(2 pi f t) sin(2 pi c)): im ~ cos c, re ~ sin c.
    return std::atan2(re, im) / (2.0 * kPiD);
}

double wrapCycles(double c) { return c - std::round(c); }

/**
 * The kick's asymptotic phase (Kick.h): late in the tail the output is a sine at f_e whose phase is f_e t + c. Measured
 * by projecting the tail onto a sine and a cosine at f_e, for the three engines and a sub-sample onset.
 */
void testKickPhase()
{
    section("the kick's asymptotic phase");
    auto p = std::make_unique<ParamStore>();
    p->parseText("kick.top_level=-60; kick.amp_decay=900; kick.amp_hold=0");
    for (int engine = 0; engine < 3; ++engine) {
        p->set(p->find("kick.engine"), static_cast<float>(engine));
        for (double late : { 0.0, 0.37 }) {
            Kick k;
            const std::vector<float> y = renderKick(*p, 9, late, 48000, k);
            const double f = k.tunedEndHz();
            // t = 0 is the ideal start, `late` samples before sample 0.
            const double measured = phaseAt(y, f, 9600, 9600, 48000.0, late);
            // The 909's triangle has odd harmonics; its fundamental is in phase with the sine (Kick.h).
            const double err = std::fabs(wrapCycles(measured - k.asymptoticPhase())) * 360.0;
            check(err < 3.0, fmt("engine %d, late %.2f: tail in phase with f0 t + c", engine, late).c_str(), fmt("%.2f degrees", err));
        }
    }
}

/** The kick's tuning rules (Kick::tuneToKey). */
void testKickTuning()
{
    section("kick tuning");
    bool keyOk = true, fifthOk = true;
    std::string detail;
    for (int root = 0; root < 12; ++root) {
        const float f = Kick::tuneToKey(root, static_cast<int>(KickTune::Key), 52.0f);
        const double note = 69.0 + 12.0 * std::log2(f / 440.0);
        const int pc = ((static_cast<int>(std::lround(note)) % 12) + 12) % 12;
        const bool tonicFits = root >= 4;   // E .. B have an octave in 41 .. 62 Hz
        const int want = tonicFits ? root : (root + 7) % 12;
        if (pc != want || f < 40.0f || f > 66.0f) { keyOk = false; detail += fmt("%s->%.1f ", kKeyNames[root], f); }
        const float g = Kick::tuneToKey(root, static_cast<int>(KickTune::Fifth), 52.0f);
        const int pg = ((static_cast<int>(std::lround(69.0 + 12.0 * std::log2(g / 440.0))) % 12) + 12) % 12;
        fifthOk = fifthOk && pg == (root + 7) % 12;
    }
    check(keyOk, "Key: the tonic in 41 .. 62 Hz, else the fifth", detail);
    check(fifthOk, "Fifth: the fifth");
}

/**
 * The rumble (Rumble.h): kicks on the quarters at 130 BPM into the rumble. Under 80 Hz the sum of kick and rumble must
 * never hold less energy than the kick alone (the sub adds, it never cancels), and after the kick's body the rumble's
 * sub must be in phase with the kick's tail.
 */
void testRumble()
{
    section("the rumble continues the kick");
    for (float f0 : { 46.0f, 55.0f, 61.0f }) {
        for (float decay : { 1.0f, 3.5f }) {
            auto p = std::make_unique<ParamStore>();
            p->set(p->find("kick.tune"), 0.0f);
            p->set(p->find("kick.pitch_end"), f0);
            p->set(p->find("kick.top_level"), -60.0f);
            p->set(p->find("rumble.decay"), decay);
            float kv[64], rv[64];
            p->readModule(Module::Kick, 0, kv);
            Kick::constrain(kv, 0.0, 9);
            p->readModule(Module::Rumble, 0, rv);
            Kick k;
            Rumble r;
            k.prepare(48000.0);
            r.prepare(48000.0, 32);
            k.update(kv, 9);
            r.update(rv, k.tunedEndHz());
            const double beat = 60.0 / 130.0 * 48000.0;   // samples per beat
            const int beats = 24, n = static_cast<int>(beat * beats);
            std::vector<float> kick(static_cast<size_t>(n)), sum(static_cast<size_t>(n)), sub(static_cast<size_t>(n));
            std::vector<float> ko(32), kb(32), ro(32);
            // Kicks on the beat, each with its sub-sample position; spans split at the kicks as the engine splits them.
            int nextBeat = 0;
            for (int s = 0; s < n;) {
                while (nextBeat < beats && static_cast<int>(std::ceil(nextBeat * beat)) <= s) {
                    const double late = std::ceil(nextBeat * beat) - nextBeat * beat;
                    k.trigger(1.0f, late);
                    r.kick(late, k.asymptoticPhase(), k.tunedEndHz());
                    ++nextBeat;
                }
                int m = std::min(32, n - s);
                if (nextBeat < beats) m = std::min(m, static_cast<int>(std::ceil(nextBeat * beat)) - s);
                k.process(ko.data(), kb.data(), m);
                r.process(kb.data(), ro.data(), m);
                for (int i = 0; i < m; ++i) {
                    const size_t o = static_cast<size_t>(s + i);
                    kick[o] = ko[static_cast<size_t>(i)];
                    sum[o] = ko[static_cast<size_t>(i)] + ro[static_cast<size_t>(i)];
                    sub[o] = r.lastSub()[static_cast<size_t>(i)];
                }
                s += m;
            }
            // Energy under 80 Hz per beat (fourth-order low pass on both), from the fourth beat on.
            auto lowEnergy = [&](const std::vector<float>& x, int b) {
                Svf a, c;
                a.setK(80.0f, 1.8477590f, 48000.0f);
                c.setK(80.0f, 0.7653669f, 48000.0f);
                double e = 0.0;
                const int from = static_cast<int>((b - 1) * beat), to = static_cast<int>((b + 1) * beat);
                for (int i = std::max(0, from); i < std::min(n, to); ++i) {
                    const float y = c.lp(a.lp(x[static_cast<size_t>(i)]));
                    if (i >= static_cast<int>(b * beat)) e += static_cast<double>(y) * y;
                }
                return e;
            };
            double worst = 1e9;
            for (int b = 4; b < beats - 1; ++b) worst = std::min(worst, 10.0 * std::log10(lowEnergy(sum, b) / lowEnergy(kick, b)));
            check(worst >= 0.0, fmt("f0 %.0f Hz, hall %.1f s: under 80 Hz kick + rumble >= kick, every beat", f0, decay).c_str(),
                  fmt("least gain %+.2f dB", worst));
            // Phase: 150 .. 300 ms after every kick, where the kick's tail falls and the sub rises, both projected onto
            // f0. (A correlation of the two signals would read their opposite envelopes, not their phases.)
            const double f = k.tunedEndHz();
            double worstDeg = 0.0;
            for (int b = 8; b < beats - 1; ++b) {
                const int from = static_cast<int>(b * beat + 0.15 * 48000.0), len = static_cast<int>(0.15 * 48000.0);
                const double d = wrapCycles(phaseAt(kick, f, from, len, 48000.0, 0.0) - phaseAt(sub, f, from, len, 48000.0, 0.0));
                worstDeg = std::max(worstDeg, std::fabs(d) * 360.0);
            }
            check(worstDeg < 10.0, fmt("f0 %.0f Hz, hall %.1f s: the sub is in phase with the kick's tail", f0, decay).c_str(),
                  fmt("worst %.1f degrees", worstDeg));
        }
    }
}


/**
 * The rumble's calibration (Rumble.cpp, kHallGain and kBandGain): at the defaults the hall's return reaches the clip
 * with its peaks near -1 dBFS, and Level means the rumble's level against the kick's, RMS over a beat.
 */
void testRumbleLevel()
{
    section("the rumble's level");
    auto p = std::make_unique<ParamStore>();
    p->set(p->find("kick.top_level"), -60.0f);
    float kv[64], rv[64];
    p->readModule(Module::Kick, 0, kv);
    Kick::constrain(kv, 0.0, 9);
    p->readModule(Module::Rumble, 0, rv);
    Kick k;
    Rumble r;
    k.prepare(48000.0);
    r.prepare(48000.0, 32);
    k.update(kv, 9);
    r.update(rv, k.tunedEndHz());
    const double beat = 60.0 / 130.0 * 48000.0;
    const int beats = 16, n = static_cast<int>(beat * beats);
    std::vector<float> ko(static_cast<size_t>(n)), kb(static_cast<size_t>(n)), ro(static_cast<size_t>(n)), hall(static_cast<size_t>(n));
    int nb = 0;
    for (int s = 0; s < n;) {
        while (nb < beats && static_cast<int>(std::ceil(nb * beat)) <= s) {
            const double late = std::ceil(nb * beat) - nb * beat;
            k.trigger(1.0f, late);
            r.kick(late, k.asymptoticPhase(), k.tunedEndHz());
            ++nb;
        }
        int m = std::min(32, n - s);
        if (nb < beats) m = std::min(m, static_cast<int>(std::ceil(nb * beat)) - s);
        k.process(&ko[static_cast<size_t>(s)], &kb[static_cast<size_t>(s)], m);
        r.process(&kb[static_cast<size_t>(s)], &ro[static_cast<size_t>(s)], m);
        for (int i = 0; i < m; ++i) hall[static_cast<size_t>(s + i)] = r.lastHall()[static_cast<size_t>(i)];
        s += m;
    }
    const int from = static_cast<int>(8 * beat);
    auto rms = [&](const std::vector<float>& x) {
        double e = 0.0;
        for (int i = from; i < n; ++i) e += static_cast<double>(x[static_cast<size_t>(i)]) * x[static_cast<size_t>(i)];
        return 10.0 * std::log10(e / (n - from) + 1e-30);
    };
    float peak = 0.0f;
    for (int i = from; i < n; ++i) peak = std::max(peak, std::fabs(hall[static_cast<size_t>(i)]));
    const double peakDb = 20.0 * std::log10(peak);
    check(peakDb > -3.0 && peakDb < 0.0, "the hall's return reaches the clip just under full scale", fmt("peak %.1f dBFS", peakDb));
    const double rel = rms(ro) - rms(ko);
    check(std::fabs(rel - rv[rumble::Level]) < 1.5, "Level is the rumble against the kick", fmt("%.1f dB at Level %.0f dB", rel, rv[rumble::Level]));
}

/** The sub bass's kick lock: a note at the kick's pitch a sixteenth after the kick starts in the kick's phase. */
void testSubLock()
{
    section("the sub bass starts in the kick's phase");
    auto p = std::make_unique<ParamStore>();
    p->parseText("kick.tune=Free; kick.pitch_end=55; kick.top_level=-60; kick.amp_decay=900; sub.duck=0; sub.attack=1");
    Kick k;
    std::vector<float> body;
    const int n = 24000;
    const std::vector<float> kick = renderKick(*p, 9, 0.0, n, k, &body);
    float sv[64];
    p->readModule(Module::Sub, 0, sv);
    SubBass s;
    s.prepare(48000.0);
    s.update(sv);
    const int onset = 5539;   // a sixteenth at 130 BPM is 5538.46 samples: the note's ideal start
    const double late = onset - 5538.4615;
    const double hz = s.noteHz(33);   // A1 = 55 Hz
    const double kickPhase = k.tunedEndHz() * (5538.4615 / 48000.0) + k.asymptoticPhase();
    double ph = hz / k.tunedEndHz() * kickPhase - s.chainPhase(hz);
    ph -= std::floor(ph);
    std::vector<float> y(static_cast<size_t>(n), 0.0f);
    s.process(y.data(), onset);
    s.noteOn(33, 1.0f, late, ph);
    s.process(y.data() + onset, n - onset);
    double kk = 0.0, ss = 0.0, ks = 0.0;
    for (int i = onset + 480; i < onset + 4800; ++i) {
        kk += static_cast<double>(kick[static_cast<size_t>(i)]) * kick[static_cast<size_t>(i)];
        ss += static_cast<double>(y[static_cast<size_t>(i)]) * y[static_cast<size_t>(i)];
        ks += static_cast<double>(kick[static_cast<size_t>(i)]) * y[static_cast<size_t>(i)];
    }
    const double corr = ks / std::sqrt(kk * ss + 1e-30);
    check(corr > 0.97, "sub and kick tail in phase, 10 .. 100 ms after the note", fmt("correlation %.4f", corr));
}

/** The metal table: six bits, the same every time, its energy above 2.5 kHz. */
void testMetalTable()
{
    section("the 909 metal table");
    PercKit a, b;
    a.prepare(48000.0);
    b.prepare(44100.0);
    const std::vector<float>& t = a.metalTable();
    bool sixBits = true;
    for (float x : t) sixBits = sixBits && std::fabs(x * 32.0f - std::round(x * 32.0f)) < 1e-6f;
    check(sixBits, "every value one of 64 levels");
    check(t == b.metalTable(), "the same table whatever the sample rate");
    const std::vector<double> ps = powerSpectrum(t.data(), 16384);
    double low = 0.0, all = 0.0;
    for (size_t i = 1; i < ps.size(); ++i) {
        const double hz = static_cast<double>(i) * PercKit::kMetalRate / 16384.0;
        all += ps[i];
        if (hz < 2500.0) low += ps[i];
    }
    check(low / all < 0.05, "less than 5 % of its energy under 2.5 kHz", fmt("%.2f %%", 100.0 * low / all));
}

/**
 * The kit's levels (Params.cpp, kDefaultKit): a full-velocity hit of every default lane peaks where Dok. 8.7's reference
 * levels put it against the kick's peak (CH -8, OH -10, perc -15, FX -10 dB; the rest [I], see kLaneTargetDb).
 */
constexpr float kLaneTargetDb[kPercLanes] = { -8.0f, -12.0f, -10.0f, -14.0f, -10.0f, -16.0f, -14.0f, -15.0f, -15.0f, -15.0f,
                                              -15.0f, -10.0f };

/** @brief Peak of one hit of lane @p lane at velocity 1 with the store's knobs, stereo. */
float lanePeak(const ParamStore& p, int lane)
{
    PercKit kit;
    kit.prepare(48000.0);
    kit.setTempo(130.0);
    for (int l = 0; l < kPercLanes; ++l) {
        float v[64];
        p.readModule(Module::Perc, l, v);
        kit.update(l, v, 9, 0);
    }
    kit.trigger(lane, 1.0f, 0, 0.0);
    std::vector<float> L(48000), R(48000);
    kit.process(L.data(), R.data(), 48000);
    float pk = 0.0f;
    for (int i = 0; i < 48000; ++i) pk = std::max(pk, std::max(std::fabs(L[static_cast<size_t>(i)]), std::fabs(R[static_cast<size_t>(i)])));
    return pk;
}

void testKitLevels()
{
    section("the kit's levels against the kick");
    auto p = std::make_unique<ParamStore>();
    Kick k;
    const std::vector<float> kick = renderKick(*p, 9, 0.0, 24000, k);
    float kp = 0.0f;
    for (float x : kick) kp = std::max(kp, std::fabs(x));
    const double kickDb = 20.0 * std::log10(kp);
    std::string suggest;
    for (int l = 0; l < kPercLanes; ++l) {
        const double rel = 20.0 * std::log10(lanePeak(*p, l)) - kickDb;
        const double level = p->get(p->id(Module::Perc, l, perc::Level));
        const bool ok = std::fabs(rel - kLaneTargetDb[l]) < 1.5;
        if (!ok) suggest += fmt("perc%d.level=%.1f; ", l + 1, level + kLaneTargetDb[l] - rel);
        check(ok, fmt("lane %d (%s) at %.0f dB against the kick", l + 1, kPercRoleNames[l], kLaneTargetDb[l]).c_str(),
              fmt("%.1f dB", rel));
    }
    if (!suggest.empty()) std::printf("  levels that would meet the targets: %s\n", suggest.c_str());
}

/** The rack's rules (Rack.h, Dok. 8.2). */
void testRack()
{
    section("the pattern rack");
    auto p = std::make_unique<ParamStore>();
    p->set(p->find("compose.humanize"), 0.0f);
    const RackPlan plan = makeRackPlan(*p, 77);
    int quarterViolations = 0, hatClashes = 0, bars = 0;
    std::vector<NoteEvent> notes;
    for (int bar = 0; bar < 64; ++bar) {
        BarSpec spec = emptyBar(bar, 4.0 * bar, 130.0);
        for (bool& a : spec.active) a = true;
        notes.clear();
        realizeBar(plan, spec, notes);
        ++bars;
        std::set<int> hatSteps;
        for (const NoteEvent& n : notes) {
            const double pos = (n.beat - 4.0 * bar) * 4.0;
            const int step = static_cast<int>(std::floor(pos + 1e-6));
            const bool quarter = std::fabs(pos - std::round(pos)) < 0.2 && (static_cast<int>(std::lround(pos)) % 4) == 0;
            bool allowed = n.part == Part::Kick || n.part == percPart(4) || n.part == percPart(7) || n.part == Part::Ping;
            for (int li = 0; li < kNumLayers; ++li) {
                const int lane = layerLane(plan, static_cast<LayerId>(li));
                if (plan.period[li] > 0 && lane >= 0 && n.part == percPart(lane)) allowed = true;   // a cyclic layer crosses them
            }
            if (quarter && !allowed && n.velocity > 0.2f) ++quarterViolations;
            const int lane = laneOf(n.part);
            if (lane >= 0 && lane <= 2) {
                if (hatSteps.count(step)) ++hatClashes;
                hatSteps.insert(step);
            }
        }
    }
    check(quarterViolations == 0, "the quarters belong to the kick (clap, rim and cyclic layers may share them)",
          fmt("%d violations", quarterViolations));
    check(hatClashes == 0, "one hat per step", fmt("%d clashes in %d bars", hatClashes, bars));

    // Anchors: the same every bar of a block. Motion: rolled anew. Loop: bar 3 of a loop equals bar 1.
    bool on1[kSteps], on2[kSteps];
    rollSteps(plan, LayerId::ClosedHat, 3, 0, 1.0f, on1);
    rollSteps(plan, LayerId::ClosedHat, 17, 0, 1.0f, on2);
    check(std::equal(on1, on1 + kSteps, on2), "the offbeat hat is the same in every bar");
    std::set<unsigned> shapes;
    for (int bar = 0; bar < 64; ++bar) {
        rollSteps(plan, LayerId::RollingHat, bar, 0, 1.0f, on1);
        unsigned m = 0;
        for (int s = 0; s < kSteps; ++s) m |= on1[s] ? (1u << s) : 0u;
        shapes.insert(m);
    }
    check(shapes.size() > 12, "the rolling hat is rolled anew every bar (Hypnotic)", fmt("%zu shapes in 64 bars", shapes.size()));
    // A reroll share of 0 rolls the motion once per block: the same shape in every bar of the block.
    RackPlan steady = plan;
    steady.reroll = 0.0f;
    bool steadySame = true;
    rollSteps(steady, LayerId::RollingHat, 32, 1, 1.0f, on1);
    for (int bar = 33; bar < 64; ++bar) {
        rollSteps(steady, LayerId::RollingHat, bar, 1, 1.0f, on2);
        steadySame = steadySame && std::equal(on1, on1 + kSteps, on2);
    }
    check(steadySame, "reroll 0: the rolling hat is the same in every bar of a block");
    const int loop = plan.loopBars[static_cast<int>(LayerId::OpenHat)];
    rollSteps(plan, LayerId::OpenHat, 32 + 0, 1, 1.0f, on1);
    rollSteps(plan, LayerId::OpenHat, 32 + 2, 1, 1.0f, on2);
    check(loop == 1 || std::equal(on1, on1 + kSteps, on2), "a loop's third bar repeats its first", fmt("loop of %d", loop));

    // Swing: the rolling hat's even sixteenths late by (S - 50) % of an eighth, the odd ones on the grid.
    BarSpec spec = emptyBar(5, 20.0, 130.0);
    spec.active[static_cast<int>(LayerId::RollingHat)] = true;
    notes.clear();
    realizeBar(plan, spec, notes);
    double worst = 0.0;
    for (const NoteEvent& n : notes) {
        const double pos = (n.beat - 20.0) * 4.0;
        const int step = static_cast<int>(std::lround(std::floor(pos + 1e-6)));
        const double expect = step + ((step & 1) ? (plan.swing - 50.0) / 100.0 * 2.0 : 0.0);
        worst = std::max(worst, std::fabs(pos - expect));
    }
    check(worst < 1e-9, "MPC swing on the even sixteenths", fmt("swing %.0f %%, worst %.2e steps", static_cast<double>(plan.swing), worst));

    // Determinism: bar 37 alone equals bar 37 after 36 others.
    std::vector<NoteEvent> a, b;
    BarSpec s37 = emptyBar(37, 148.0, 130.0);
    for (bool& x : s37.active) x = true;
    realizeBar(plan, s37, a);
    for (int bar = 0; bar < 37; ++bar) { BarSpec s = emptyBar(bar, 4.0 * bar, 130.0); for (bool& x : s.active) x = true; realizeBar(plan, s, b); }
    b.clear();
    realizeBar(plan, s37, b);
    bool same = a.size() == b.size();
    for (size_t i = 0; same && i < a.size(); ++i) same = a[i].beat == b[i].beat && a[i].velocity == b[i].velocity && a[i].part == b[i].part;
    check(same, "a bar realised alone equals the bar in sequence", fmt("%zu notes", a.size()));
}

/**
 * The rack's Phase 2 (Rack.h): cycles against the bar, Euclidean patterns off the quarters, the ghost chain, trig
 * conditions, fills, the mini-notation.
 */
void testRackPhase2()
{
    section("the rack's cycles, chains and conditions");
    auto p = std::make_unique<ParamStore>();
    p->set(p->find("compose.humanize"), 0.0f);
    // Cycles: a period of p sixteenths is back on the bar's first step after lcm(p, 16) / 16 bars, and not before.
    bool realign = true;
    std::string detail;
    for (int period : { 3, 5, 6, 7, 12, 15, 17 }) {
        RackPlan plan = makeRackPlan(*p, 3);
        const int li = static_cast<int>(LayerId::Ping);
        plan.period[li] = period;
        plan.resetBars[li] = 64;
        plan.cycle[li] = 1;   // one onset, on the period's first position
        int first = -1;
        for (int bar = 1; bar < 40 && first < 0; ++bar) {
            bool on[kSteps];
            rollSteps(plan, LayerId::Ping, bar, 0, 1.0f, on);
            if (on[0]) first = bar;
        }
        int g = period, h = 16;
        while (h) { const int t = g % h; g = h; h = t; }
        const int want = period * 16 / g / 16;
        if (first != want) { realign = false; detail += fmt("%d: bar %d (want %d) ", period, first, want); }
    }
    check(realign, "a cycle meets the bar again after lcm(p, 16) / 16 bars (15 and 17: after 15 and 17)", detail);
    // Euclid: the chosen rotations keep the quarters free, the masks are maximally even.
    int euclidOnQuarter = 0, plans = 0;
    for (uint64_t seed = 1; seed < 200; ++seed) {
        const RackPlan plan = makeRackPlan(*p, seed);
        for (int li = 0; li < kNumLayers; ++li) {
            if (plan.euclid[li] == 0) continue;
            ++plans;
            if (plan.euclid[li] & 0x1111u) ++euclidOnQuarter;
        }
    }
    const uint64_t e516 = euclidMask(5, 16);
    int gaps[5], k = 0, last = -1, firstOn = -1;
    for (int i = 0; i < 16; ++i) if (e516 & (uint64_t(1) << i)) { if (last >= 0) gaps[k++] = i - last; else firstOn = i; last = i; }
    gaps[k++] = 16 - last + firstOn;
    bool even = true;
    for (int i = 0; i < k; ++i) even = even && (gaps[i] == 3 || gaps[i] == 4);
    check(euclidOnQuarter == 0 && plans > 20, "Euclidean patterns off the quarters", fmt("%d of %d on a quarter", euclidOnQuarter, plans));
    check(even && k == 5, "E(5,16) is 3+3+3+3+4 in some rotation");
    // The ghost chain: after a ghost kick, the next sixteenth's chance is halved.
    const RackPlan plan = makeRackPlan(*p, 21);
    int pairs = 0, afterOn = 0;
    for (int bar = 0; bar < 40000; ++bar) {
        bool on[kSteps];
        rollSteps(plan, LayerId::ClapGhost, bar, 0, 1.0f, on);
        for (int s = 2; s < kSteps; ++s) if (layerDef(LayerId::ClapGhost).p[s] > 0.0f && on[s - 2]) { ++pairs; afterOn += on[s] ? 1 : 0; }
    }
    int base = 0, total = 0;
    for (int bar = 0; bar < 40000; ++bar) {
        bool on[kSteps];
        rollSteps(plan, LayerId::ClapGhost, bar, 0, 1.0f, on);
        for (int s = 0; s < kSteps; ++s) if (layerDef(LayerId::ClapGhost).p[s] > 0.0f) { ++total; base += on[s] ? 1 : 0; }
    }
    const double after = pairs > 0 ? static_cast<double>(afterOn) / pairs : 0.0, overall = static_cast<double>(base) / total;
    check(after < 0.9 * overall, "a clap ghost two steps after another is rarer than any clap ghost",
          fmt("%.3f against %.3f", after, overall));
    // Trig conditions: the extra open hat only in the second of four bars.
    bool condOk = true;
    for (int bar = 0; bar < 32; ++bar) {
        BarSpec spec = emptyBar(bar, 4.0 * bar, 130.0);
        spec.active[static_cast<int>(LayerId::OpenHat)] = true;
        std::vector<NoteEvent> notes;
        realizeBar(plan, spec, notes);
        const TrigCond& t = plan.conds[0];
        bool has = false;
        for (const NoteEvent& n : notes) if (std::fabs((n.beat - 4.0 * bar) * 4.0 - t.step) < 0.2 && n.part == percPart(2)) has = true;
        const bool want = (bar % 4) == 1;
        if (want && !has) condOk = false;
    }
    check(condOk, "the open hat's trig condition plays in bar 2 of 4");
    // Fills: only in the last bar of an eight-bar phrase.
    bool fillOk = true;
    int fills = 0;
    for (int bar = 0; bar < 256; ++bar) {
        BarSpec spec = emptyBar(bar, 4.0 * bar, 130.0);
        spec.active[static_cast<int>(LayerId::TomConga)] = true;
        std::vector<NoteEvent> notes;
        realizeBar(plan, spec, notes);
        int last4 = 0;
        for (const NoteEvent& n : notes) if ((n.beat - 4.0 * bar) * 4.0 >= 12.9 && n.shift > 0) ++last4;
        if (last4 > 0) { ++fills; if (bar % 8 != 7) fillOk = false; }
    }
    check(fillOk && fills > 0, "fills only in the last bar of an eight-bar phrase", fmt("%d fills in 32 phrases", fills));
    // Mini-notation.
    const std::string kick = miniNotation(plan, LayerId::Kick, 0, "bd");
    const std::string ping = miniNotation(plan, LayerId::Ping, 0, "p");
    check(kick == "bd ~ ~ ~ bd ~ ~ ~ bd ~ ~ ~ bd ~ ~ ~", "the kick in mini-notation", kick);
    check(ping.front() == '{' && ping.find("}%16") != std::string::npos, "a cyclic layer as a polymetric sequence", ping);
}

/** The study's form (Study.h, PLAN 7.2). */
void testStudyForm()
{
    section("the study's form");
    for (int owner = 0; owner < 2; ++owner) {
        auto p = std::make_unique<ParamStore>();
        p->set(p->find("compose.minutes"), 7.0f);
        p->set(p->find("compose.low_owner"), static_cast<float>(owner));
        const Score s = composeStudy(*p, 5);
        const int bars = static_cast<int>(s.lengthBeats / 4.0);
        // Exactly one operation at every block boundary of the body.
        int blocksWithOne = 0, blocks = 0;
        for (int b = 32; b < bars - 32; b += 32) {
            ++blocks;
            int ops = 0;
            for (const BlockOp& o : s.ops) if (std::fabs(o.beat - 4.0 * b) < 1e-9) ++ops;
            blocksWithOne += ops == 1 ? 1 : 0;
        }
        check(blocksWithOne >= blocks - 1, fmt("owner %d: one operation per block boundary", owner).c_str(), fmt("%d of %d", blocksWithOne, blocks));
        // Every operation on a four-bar line.
        bool onFour = true;
        for (const BlockOp& o : s.ops) onFour = onFour && std::fabs(std::fmod(o.beat, 16.0)) < 1e-9;
        check(onFour, fmt("owner %d: every operation on a multiple of four bars", owner).c_str());
        // No tonal material in the first and last 32 bars (Dok. 8.5), and a bass only where the sub owns the low end.
        int tonalEdge = 0, subNotes = 0;
        for (const NoteEvent& n : s.notes) {
            if (n.part != Part::Sub) continue;
            ++subNotes;
            if (n.beat < 128.0 || n.beat >= s.lengthBeats - 128.0) ++tonalEdge;
        }
        check(tonalEdge == 0, fmt("owner %d: no bass in the first and last 32 bars", owner).c_str(), fmt("%d notes", tonalEdge));
        check(owner == 1 ? subNotes > 0 : subNotes == 0, fmt("owner %d: a bass line exactly where the sub owns the low end", owner).c_str(),
              fmt("%d notes", subNotes));
    }
}

/** Renders @p seconds of the study with block size @p block. */
std::vector<float> renderStudy(int block, double seconds, uint64_t seed, const char* settings = "")
{
    auto e = std::make_unique<Engine>();
    e->params().parseText(settings);
    e->params().set(e->params().find("compose.minutes"), 4.0f);
    const Score s = composeStudy(e->params(), seed);
    e->prepare(48000.0, block);
    e->load(s);
    const int n = static_cast<int>(seconds * 48000.0);
    std::vector<float> out(static_cast<size_t>(2 * n));
    std::vector<float> L(static_cast<size_t>(block)), R(static_cast<size_t>(block));
    for (int done = 0; done < n;) {
        const int m = std::min(block, n - done);
        e->process(L.data(), R.data(), m);
        for (int i = 0; i < m; ++i) { out[static_cast<size_t>(2 * (done + i))] = L[static_cast<size_t>(i)]; out[static_cast<size_t>(2 * (done + i) + 1)] = R[static_cast<size_t>(i)]; }
        done += m;
    }
    return out;
}

/** Blocks of 1, 37 and 512 samples give the same bits (the raster and the event splits, Engine.h). */
void testBlockSizes()
{
    section("block sizes");
    const std::vector<float> a = renderStudy(512, 20.0, 3), b = renderStudy(37, 20.0, 3), c = renderStudy(1, 20.0, 3);
    size_t firstB = a.size(), firstC = a.size();
    for (size_t i = 0; i < a.size(); ++i) {
        if (firstB == a.size() && std::memcmp(&a[i], &b[i], sizeof(float)) != 0) firstB = i;
        if (firstC == a.size() && std::memcmp(&a[i], &c[i], sizeof(float)) != 0) firstC = i;
    }
    check(firstB == a.size(), "37 equals 512, bit for bit", firstB == a.size() ? std::string() : fmt("first difference at %.4f s", firstB / 96000.0));
    check(firstC == a.size(), "1 equals 512, bit for bit", firstC == a.size() ? std::string() : fmt("first difference at %.4f s", firstC / 96000.0));
    double peak = 0.0;
    for (float x : a) peak = std::max(peak, static_cast<double>(std::fabs(x)));
    check(peak > 0.1 && std::isfinite(peak), "it sounds", fmt("peak %.3f", peak));
}

/** The master: the true peak under the ceiling, the low end mono. */
void testMaster()
{
    section("the master");
    for (int owner = 0; owner < 2; ++owner) {
        const std::vector<float> y = renderStudy(512, 60.0, 11, owner == 1 ? "compose.low_owner=Sub" : "");
        LoudnessMeter m;
        m.prepare(48000.0);
        const size_t n = y.size() / 2;
        std::vector<float> L(n), R(n);
        for (size_t i = 0; i < n; ++i) { L[i] = y[2 * i]; R[i] = y[2 * i + 1]; }
        m.process(L.data(), R.data(), static_cast<int>(n));
        const LoudnessReport r = m.report();
        check(r.truePeak <= -0.9, fmt("owner %d: true peak under the -1 dBTP ceiling", owner).c_str(), fmt("%.2f dBTP", r.truePeak));
        check(r.integrated > -30.0 && r.integrated < -6.0, fmt("owner %d: loudness in a sane range", owner).c_str(),
              fmt("%.1f LUFS integrated", r.integrated));
        // Side energy under 80 Hz against the mid's.
        Svf a, b, c, d;
        a.setK(80.0f, 1.8477590f, 48000.0f); b.setK(80.0f, 0.7653669f, 48000.0f);
        c.setK(80.0f, 1.8477590f, 48000.0f); d.setK(80.0f, 0.7653669f, 48000.0f);
        double mid = 0.0, side = 0.0;
        for (size_t i = 0; i < n; ++i) {
            const float mm = b.lp(a.lp(0.5f * (L[i] + R[i]))), ss = d.lp(c.lp(0.5f * (L[i] - R[i])));
            mid += static_cast<double>(mm) * mm;
            side += static_cast<double>(ss) * ss;
        }
        check(10.0 * std::log10(side / mid + 1e-30) < -40.0, fmt("owner %d: under 80 Hz mono", owner).c_str(),
              fmt("side %.1f dB under mid", -10.0 * std::log10(side / mid + 1e-30)));
    }
}

/** The MIDI export: a note-on for every note, format 1. */
void testMidi()
{
    section("MIDI export");
    auto p = std::make_unique<ParamStore>();
    p->set(p->find("compose.low_owner"), 1.0f);
    const Score s = composeStudy(*p, 9);
    const std::vector<uint8_t> f = encodeMidi(s, "test", p.get());
    check(f.size() > 14 && std::memcmp(f.data(), "MThd", 4) == 0 && f[9] == 1, "a format 1 file");
    // Count note-ons (status 0x9n with velocity > 0) by walking the tracks.
    size_t pos = 14, ons = 0;
    while (pos + 8 <= f.size()) {
        const uint32_t len = (static_cast<uint32_t>(f[pos + 4]) << 24) | (static_cast<uint32_t>(f[pos + 5]) << 16) |
                             (static_cast<uint32_t>(f[pos + 6]) << 8) | f[pos + 7];
        size_t q = pos + 8;
        const size_t end = q + len;
        while (q < end) {
            while (f[q] & 0x80) ++q;   // delta time
            ++q;
            const uint8_t st = f[q];
            if (st == 0xFF) { q += 2; uint32_t l = 0; while (f[q] & 0x80) { l = (l << 7) | (f[q] & 0x7F); ++q; } l = (l << 7) | f[q]; q += 1 + l; }
            else if ((st & 0xF0) == 0x90) { if (f[q + 2] > 0) ++ons; q += 3; }
            else q += 3;
        }
        pos = end;
    }
    check(ons == s.notes.size(), "a note-on for every note", fmt("%zu of %zu", ons, s.notes.size()));
}

struct TestSection {
    const char* name;
    std::function<void()> fn;
};

const TestSection kSections[] = {
    { "testTempoMap", testTempoMap },
    { "testParams", testParams },
    { "testKickPhase", testKickPhase },
    { "testKickTuning", testKickTuning },
    { "testRumble", testRumble },
    { "testRumbleLevel", testRumbleLevel },
    { "testSubLock", testSubLock },
    { "testMetalTable", testMetalTable },
    { "testKitLevels", testKitLevels },
    { "testRack", testRack },
    { "testRackPhase2", testRackPhase2 },
    { "testStudyForm", testStudyForm },
    { "testBlockSizes", testBlockSizes },
    { "testMaster", testMaster },
    { "testMidi", testMidi },
};

} // namespace

int main(int argc, char** argv)
{
    std::string only;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--list") == 0) {
            for (const TestSection& s : kSections) std::printf("%s\n", s.name);
            return 0;
        }
        if (std::strcmp(argv[i], "--only") == 0 && i + 1 < argc) only = std::string(",") + argv[++i] + ",";
    }
    for (const TestSection& s : kSections)
        if (only.empty() || only.find(std::string(",") + s.name + ",") != std::string::npos) s.fn();
    return finish();
}
