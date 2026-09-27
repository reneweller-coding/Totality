/**
 * @file Ping.cpp
 * @brief The ping voices: FM, the low-pass gate, the wandering band pass.
 */
#include "umb/synth/Ping.h"
#include "umb/Params.h"
#include <algorithm>
#include <cmath>

namespace umb {

namespace {
constexpr double kTwoPiD = 6.283185307179586;
constexpr double kLn1000 = 6.907755278982137;
constexpr int kCoefEvery = 4;   ///< the gate's low pass is retuned every four samples
}

void Ping::prepare(double sampleRate)
{
    sr_ = sampleRate;
    vacAttack_ = 1.0 - std::exp(-1.0 / (0.001 * sr_));
    reset();
}

void Ping::reset()
{
    for (Voice& v : v_) { v = Voice{}; v.lpg.reset(); }
    bandL_.reset();
    bandR_.reset();
    bandL_.setK(bandHz_, bandK_, static_cast<float>(sr_));
    bandR_.setK(bandHz_, bandK_, static_cast<float>(sr_));
    next_ = 0;
}

bool Ping::active() const
{
    for (const Voice& v : v_) if (v.active) return true;
    return false;
}

void Ping::update(const float* p)
{
    level_ = p[ping::Level] <= -59.9f ? 0.0f : dbToGain(p[ping::Level]);
    pan_ = p[ping::Pan];
    width_ = p[ping::Width];
    ratio_ = p[ping::Ratio];
    index_ = p[ping::Index];
    indexTau_ = std::max(0.002, static_cast<double>(p[ping::IndexDecay]) * 0.001);
    pitchAmt_ = p[ping::PitchAmount];
    pitchTau_ = std::max(0.002, static_cast<double>(p[ping::PitchDecay]) * 0.001);
    decayS_ = std::max(0.01, static_cast<double>(p[ping::Decay]) * 0.001);
    lpgMix_ = p[ping::Lpg];
    lpgRelease_ = std::max(0.01, static_cast<double>(p[ping::LpgRelease]) * 0.001);
    damping_ = 2.0f - 1.9f * clampv(p[ping::Resonance], 0.0f, 1.0f);
    bandHz_ = p[ping::Band];
    bandK_ = 1.0f / std::max(0.2f, p[ping::BandQ]);
    bandMix_ = p[ping::BandMix];
    sweepOct_ = p[ping::Sweep];
    sweepRate_ = p[ping::SweepRate];
}

void Ping::noteOn(int pitch, float velocity, double late, uint32_t serial)
{
    Voice& v = v_[next_];
    next_ = (next_ + 1) % kVoices;
    v = Voice{};
    v.active = true;
    v.f0 = midiToHz(pitch);
    v.vel = clampv(velocity, 0.0f, 1.0f);
    v.dp = std::exp(-1.0 / (pitchTau_ * sr_));
    v.di = std::exp(-1.0 / (indexTau_ * sr_));
    v.da = std::exp(-kLn1000 / (decayS_ * sr_));
    v.ep = std::pow(v.dp, late);
    v.ei = std::pow(v.di, late);
    v.ea = std::pow(v.da, late);
    // The phases `late` samples in, at the starting frequency (close enough for the fraction of a sample).
    const double f = v.f0 * (1.0 + pitchAmt_);
    v.pc = f * late / sr_;
    v.pm = f * ratio_ * late / sr_;
    // Its place: Pan, spread by Width, from the note's serial (the same every render).
    const uint64_t h = mixSeed(0x50494E47ull, serial);   // "PING"
    const float u = static_cast<float>((h >> 40) * (1.0 / 16777216.0));
    const float pos = clampv(pan_ + width_ * (2.0f * u - 1.0f), -1.0f, 1.0f);
    const float th = (pos + 1.0f) * 0.25f * kPi;
    v.gl = std::cos(th) * 1.41421356f;
    v.gr = std::sin(th) * 1.41421356f;
    v.lpg.reset();
    v.lpg.setK(80.0f, damping_, static_cast<float>(sr_));
}

void Ping::process(float* L, float* R, int n, int64_t sample)
{
    for (int i = 0; i < n; ++i) { L[i] = 0.0f; R[i] = 0.0f; }
    const float fs = static_cast<float>(sr_);
    for (Voice& v : v_) {
        if (!v.active) continue;
        for (int i = 0; i < n; ++i) {
            const double f = v.f0 * (1.0 + pitchAmt_ * v.ep);
            const double idx = index_ * v.vel * v.ei;
            const double y = std::sin(kTwoPiD * v.pc + idx * std::sin(kTwoPiD * v.pm));
            v.pc += f / sr_;
            v.pm += f * ratio_ / sr_;
            v.pc -= std::floor(v.pc);
            v.pm -= std::floor(v.pm);
            v.ep *= v.dp;
            v.ei *= v.di;
            v.ea *= v.da;
            // The vactrol: a millisecond up, then down with a time constant that grows as it darkens.
            if (v.age < static_cast<int>(0.002 * sr_)) v.vac += (1.0 - v.vac) * vacAttack_;
            else {
                const double dark = 1.0 - v.vac;
                const double tau = lpgRelease_ * (0.2 + 1.6 * dark * dark);
                v.vac -= v.vac / (tau * sr_);
            }
            if ((v.age % kCoefEvery) == 0) v.lpg.setK(static_cast<float>(80.0 * std::pow(225.0, v.vac)), damping_, fs);
            const float gated = v.lpg.lp(static_cast<float>(y)) * static_cast<float>(std::pow(v.vac, 1.5));
            const float plain = static_cast<float>(y * v.ea);
            const float out = v.vel * (lpgMix_ * gated + (1.0f - lpgMix_) * plain);
            L[i] += out * v.gl;
            R[i] += out * v.gr;
            ++v.age;
        }
        if (v.age > 256 && v.ea < 1.0e-4 && v.vac < 1.0e-4) v.active = false;
    }
    // The bus: a band pass whose centre a slow sine moves, its phase from the absolute sample.
    for (int i = 0; i < n; ++i) {
        // On an absolute raster of 16 samples only, so the host's blocks cannot move a coefficient.
        if (((sample + i) & 15) == 0) {
            const double t = static_cast<double>(sample + i) / sr_;
            const float centre = bandHz_ * static_cast<float>(std::pow(2.0, sweepOct_ * std::sin(kTwoPiD * sweepRate_ * t)));
            bandL_.setK(centre, bandK_, fs);
            bandR_.setK(centre, bandK_, fs);
        }
        float lp, bp, hp;
        bandL_.tick(L[i], lp, bp, hp);
        const float bl = bp * bandL_.k;
        bandR_.tick(R[i], lp, bp, hp);
        const float br = bp * bandR_.k;
        L[i] = (L[i] * (1.0f - bandMix_) + bl * bandMix_) * level_;
        R[i] = (R[i] * (1.0f - bandMix_) + br * bandMix_) * level_;
    }
}

} // namespace umb
