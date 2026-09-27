/**
 * @file Rumble.cpp
 * @brief The rumble: split, hall, clip, band filters, the phase-continuing sub, the duck.
 */
#include "umb/synth/Rumble.h"
#include "umb/Params.h"
#include <algorithm>
#include <cmath>

namespace umb {

namespace {
constexpr double kTwoPiD = 6.283185307179586;
/** @brief Gain on the hall's return before the clip (calibrated, see Rumble.h). */
constexpr float kHallGain = 3.98f;   // +12 dB: the return's peaks at the defaults near -1 dBFS
/** @brief Gain on the band after its filters (calibrated, see Rumble.h). */
constexpr float kBandGain = 1.80f;   // +5.1 dB: at Level 0 dB the rumble (band and sub) as loud as the kick, RMS over a beat
}

void Rumble::prepare(double sampleRate, int maxBlock)
{
    sr_ = sampleRate;
    hall_.prepare(sampleRate);
    duck_.prepare(sampleRate);
    const size_t n = static_cast<size_t>(std::max(1, maxBlock));
    in_.assign(n, 0.0f);
    hallL_.assign(n, 0.0f);
    hallR_.assign(n, 0.0f);
    band_.assign(n, 0.0f);
    pre_.assign(n, 0.0f);
    subOut_.assign(n, 0.0f);
    reset();
}

void Rumble::reset()
{
    split1_.reset();
    split2_.reset();
    hall_.reset();
    os_.reset();
    postHp_.reset();
    postLp_.reset();
    duck_.reset();
    env_ = 0.0f;
    subPhase_ = 0.0;
    resetIn_ = -1;
    sinceKick_ = 0;
}

void Rumble::update(const float* v, float f0)
{
    const float fs = static_cast<float>(sr_);
    level_ = v[rumble::Level] <= -59.9f ? 0.0f : dbToGain(v[rumble::Level]);
    subGain_ = v[rumble::Sub] <= -59.9f ? 0.0f : dbToGain(v[rumble::Sub]);
    const float split = v[rumble::Split];
    split1_.setK(split, 1.41421356f, fs);
    split2_.setK(split, 1.41421356f, fs);
    hall_.set(v[rumble::Size], v[rumble::Decay], v[rumble::Damping], v[rumble::PreDelay] * 0.001f * fs, 20.0f, 4000.0f);
    clipGain_ = dbToGain(v[rumble::Drive]);
    postHp_.setK(split, 1.41421356f, fs);
    postLp_.set(std::max(split * 1.2f, f0 * v[rumble::Ratio]), v[rumble::Resonance], fs);
    envAtt_ = 1.0f - std::exp(-1.0f / (v[rumble::SubAttack] * 0.001f * fs));
    envRel_ = 1.0f - std::exp(-1.0f / (v[rumble::SubRelease] * 0.001f * fs));
    duckDepth_ = 1.0f - dbToGain(-v[rumble::Duck]);
    duckHoldMs_ = v[rumble::DuckHold];
    duckReleaseMs_ = v[rumble::DuckRelease];
    duck_.set(duckDepth_, kDuckAttackMs, duckHoldMs_, duckReleaseMs_);
    // The sub's own pitch changes only with a kick (kick()), so a retuned kick and its sub never disagree.
}

void Rumble::kick(double late, double asymptote, double f0)
{
    duck_.trigger(late);
    kickLate_ = late;
    kickC_ = asymptote;
    kickF0_ = f0;
    sinceKick_ = 0;
    // The sub keeps its old phase until the gate has shut, then continues the new kick (Rumble.h).
    resetIn_ = std::max(1, static_cast<int>(std::ceil(kDuckAttackMs * 0.001 * sr_)));
}

void Rumble::process(const float* kickBody, float* out, int n)
{
    // The split and the hall.
    for (int i = 0; i < n; ++i) {
        float lp, bp, hp;
        split1_.tick(kickBody[i], lp, bp, hp);
        split2_.tick(hp, lp, bp, hp);
        in_[static_cast<size_t>(i)] = hp;
    }
    hall_.process(in_.data(), in_.data(), hallL_.data(), hallR_.data(), n);

    const float g = clipGain_, ig = 1.0f / clipGain_;
    for (int i = 0; i < n; ++i) {
        const size_t k = static_cast<size_t>(i);
        const float x = 0.5f * (hallL_[k] + hallR_[k]) * kHallGain;
        pre_[k] = x;
        float y = os_.process(x, [g, ig](float s) { return std::tanh(g * s) * ig; });
        float lp, bp, hp;
        postHp_.tick(y, lp, bp, hp);
        y = postLp_.lp(hp) * kBandGain;
        // The band's envelope (full-wave, one pole up and down), which the sub follows.
        const float a = std::fabs(y);
        env_ += (a - env_) * (a > env_ ? envAtt_ : envRel_);
        // The duck: the band by its depth, the sub all the way.
        const float duckGain = duck_.next();
        const float gate = 1.0f - duck_.amount();
        band_[k] = y * duckGain;
        if (resetIn_ >= 0 && --resetIn_ < 0) {
            // Continue the kick: this sample lies t = (samples since the trigger + late) / fs after its ideal start,
            // where its phase is f0 t + c.
            const double t = (static_cast<double>(sinceKick_) + kickLate_) / sr_;
            const double ph = kickF0_ * t + kickC_;
            subPhase_ = ph - std::floor(ph);
            f0_ = kickF0_;
        }
        // A sine's mean absolute value is 2/pi of its peak: the sub stands at the band's peak level times Sub.
        subOut_[k] = static_cast<float>(std::sin(kTwoPiD * subPhase_)) * env_ * 1.5707963f * subGain_ * gate;
        out[i] = (band_[k] + subOut_[k]) * level_;
        subInc_ = f0_ / sr_;
        subPhase_ += subInc_;
        subPhase_ -= std::floor(subPhase_);
        ++sinceKick_;
    }
}

} // namespace umb
