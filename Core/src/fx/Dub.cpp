/**
 * @file Dub.cpp
 * @brief The dub chain and the multiband duck.
 */
#include "tot/fx/Dub.h"
#include "tot/Params.h"
#include <algorithm>
#include <cmath>

namespace tot {

namespace {
constexpr float kSqrt2 = 1.41421356f;
const double kEchoBeats[] = { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };   // dub.echo_time: 1/16, 1/8, 3/16, 1/4, 3/8, 1/2
}

void DubChain::prepare(double sampleRate, int maxBlock)
{
    sr_ = sampleRate;
    echo_.prepare(sampleRate, 2.5, 0x4543484Full);   // "ECHO"
    spring_.prepare(sampleRate);
    plate_.prepare(sampleRate);
    const size_t n = static_cast<size_t>(std::max(1, maxBlock));
    eL_.assign(n, 0.0f); eR_.assign(n, 0.0f);
    sL_.assign(n, 0.0f); sR_.assign(n, 0.0f);
    pL_.assign(n, 0.0f); pR_.assign(n, 0.0f);
    reset();
}

void DubChain::reset()
{
    echo_.reset();
    plate_.reset();
    spring_.reset();
}

void DubChain::update(const float* v, double bpm)
{
    EchoSettings e;
    const int t = std::clamp(static_cast<int>(std::lround(v[dub::EchoTime])), 0, 5);
    e.delaySeconds = kEchoBeats[t] * 60.0 / std::max(20.0, bpm);
    e.feedback = v[dub::Feedback];
    e.toneHz = v[dub::Tone];
    e.lowCutHz = v[dub::LowCut];
    e.wowMs = v[dub::Wow];
    e.flutterMs = v[dub::Flutter];
    e.driveDb = v[dub::Drive];
    e.pingPong = true;
    echo_.set(e);
    spring_.set(v[dub::SpringDecay], 4000.0f);
    plate_.set(v[dub::PlateDecay], v[dub::PlateDamping], v[dub::PlatePreDelay] * 0.001f * static_cast<float>(sr_),
               v[dub::PlateLowCut], 8000.0f);
    echoGain_ = v[dub::EchoReturn] <= -59.9f ? 0.0f : dbToGain(v[dub::EchoReturn]);
    springGain_ = v[dub::Spring] <= -59.9f ? 0.0f : dbToGain(v[dub::Spring]);
    plateGain_ = v[dub::PlateReturn] <= -59.9f ? 0.0f : dbToGain(v[dub::PlateReturn]);
}

void DubChain::process(const float* echoL, const float* echoR, const float* plateL, const float* plateR, float* L, float* R, int n)
{
    for (int i = 0; i < n; ++i) { eL_[i] = eR_[i] = sL_[i] = sR_[i] = pL_[i] = pR_[i] = 0.0f; }
    echo_.process(echoL, echoR, eL_.data(), eR_.data(), n);
    spring_.process(echoL, echoR, sL_.data(), sR_.data(), n);
    plate_.process(plateL, plateR, pL_.data(), pR_.data(), n);
    for (int i = 0; i < n; ++i) {
        const size_t k = static_cast<size_t>(i);
        L[i] = eL_[k] * echoGain_ + sL_[k] * springGain_ + pL_[k] * plateGain_;
        R[i] = eR_[k] * echoGain_ + sR_[k] * springGain_ + pR_[k] * plateGain_;
    }
}

void MultibandDucker::prepare(double sampleRate)
{
    duck_.prepare(sampleRate);
    const float fs = static_cast<float>(sampleRate);
    for (Bands& b : b_) {
        for (Svf* f : { &b.split1, &b.low2, &b.high2 }) f->setK(200.0f, kSqrt2, fs);
        for (Svf* f : { &b.split2, &b.mid2, &b.top2, &b.ap }) f->setK(2000.0f, kSqrt2, fs);
    }
    reset();
}

void MultibandDucker::reset()
{
    duck_.reset();
    for (Bands& b : b_)
        for (Svf* f : { &b.split1, &b.low2, &b.high2, &b.split2, &b.mid2, &b.top2, &b.ap }) f->reset();
}

void MultibandDucker::set(float lowDb, float midDb, float holdMs, float releaseMs)
{
    lowDepth_ = 1.0f - dbToGain(-lowDb);
    midDepth_ = 1.0f - dbToGain(-midDb);
    duck_.set(1.0f, 1.0f, holdMs, releaseMs);
}

float MultibandDucker::band(Bands& b, float x, float gl, float gm)
{
    float lp, bp, hp, l1, h1;
    b.split1.tick(x, l1, bp, h1);
    const float low = b.low2.lp(l1);
    b.high2.tick(h1, lp, bp, hp);
    const float high = hp;
    b.split2.tick(high, l1, bp, h1);
    const float mid = b.mid2.lp(l1);
    b.top2.tick(h1, lp, bp, hp);
    const float top = hp;
    // The low band through the 2 kHz crossover's allpass (LP2^2 + HP2^2 = x - 2 k bp), in phase with the others.
    b.ap.tick(low, lp, bp, hp);
    const float lowAp = low - 2.0f * kSqrt2 * bp;
    return lowAp * gl + mid * gm + top;
}

void MultibandDucker::process(float* L, float* R, int n, float* gains)
{
    float* ch[2] = { L, R };
    for (int i = 0; i < n; ++i) {
        duck_.next();
        const float a = duck_.amount();
        const float gl = 1.0f - lowDepth_ * a, gm = 1.0f - midDepth_ * a;
        if (gains != nullptr) { gains[2 * i] = gl; gains[2 * i + 1] = gm; }
        for (int c = 0; c < 2; ++c) ch[c][i] = band(b_[c], ch[c][i], gl, gm);
    }
}

MultibandDucker::Replica MultibandDucker::replica() const
{
    Replica r{ { b_[0], b_[1] } };
    for (Bands& b : r.b)
        for (Svf* f : { &b.split1, &b.low2, &b.high2, &b.split2, &b.mid2, &b.top2, &b.ap }) f->reset();
    return r;
}

void MultibandDucker::apply(Replica& r, const float* gains, float* L, float* R, int n)
{
    for (int i = 0; i < n; ++i) {
        L[i] = band(r.b[0], L[i], gains[2 * i], gains[2 * i + 1]);
        R[i] = band(r.b[1], R[i], gains[2 * i], gains[2 * i + 1]);
    }
}

} // namespace tot
