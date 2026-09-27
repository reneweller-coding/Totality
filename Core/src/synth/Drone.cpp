/**
 * @file Drone.cpp
 * @brief The drone and the texture.
 */
#include "umb/synth/Drone.h"
#include "umb/Params.h"
#include <algorithm>
#include <cmath>

namespace umb {

namespace {
constexpr double kTwoPiD = 6.283185307179586;
}

// ------------------------------------------------------------------------------------------------------------ drone

void Drone::prepare(double sampleRate)
{
    sr_ = sampleRate;
    amp_.setSampleRate(sampleRate);
    reset();
}

void Drone::reset()
{
    amp_.kill();
    lpL_.reset();
    lpR_.reset();
}

void Drone::update(const float* v)
{
    level_ = v[drone::Level] <= -59.9f ? 0.0f : dbToGain(v[drone::Level]);
    octave_ = static_cast<int>(std::lround(v[drone::Octave]));
    detune_ = v[drone::Detune];
    cutoff_ = v[drone::Cutoff];
    res_ = v[drone::Resonance];
    sweep_ = v[drone::Sweep];
    sweepBeats_ = 4.0 * v[drone::SweepBars];
    attack_ = v[drone::Attack];
    release_ = v[drone::Release];
}

void Drone::noteOn(int pitch, float velocity)
{
    hz_ = midiToHz(pitch + 12 * octave_);
    while (hz_ < 150.0) hz_ *= 2.0;   // above the band the kick and the rumble own
    vel_ = clampv(velocity, 0.0f, 1.0f);
    const double spread = std::pow(2.0, detune_ / 1200.0);
    a_.set(hz_ / spread, sr_, 0.0f, 0.5f);
    b_.set(hz_ * spread, sr_, 0.0f, 0.5f);
    amp_.setTimes(attack_, 0.1f, 1.0f, release_);
    amp_.noteOn();
}

void Drone::noteOff()
{
    amp_.noteOff();
}

void Drone::at(double beat)
{
    const float fs = static_cast<float>(sr_);
    const float fc = std::min(0.45f * fs, cutoff_ * static_cast<float>(std::pow(2.0, sweep_ * std::sin(kTwoPiD * beat / sweepBeats_))));
    lpL_.set(fc, res_, fs);
    lpR_.copyCoefficients(lpL_);
}

void Drone::process(float* L, float* R, int n)
{
    for (int i = 0; i < n; ++i) {
        if (!amp_.isActive()) { for (; i < n; ++i) L[i] = R[i] = 0.0f; break; }   // resting, from the sample it ended
        const float e = amp_.process() * vel_ * level_;
        L[i] = lpL_.lp(a_.next()) * e;
        R[i] = lpR_.lp(b_.next()) * e;
    }
}

// ---------------------------------------------------------------------------------------------------------- texture

void Texture::prepare(double sampleRate)
{
    sr_ = sampleRate;
    gainStep_ = static_cast<float>(1.0 / (2.0 * sampleRate));
    reset();
}

void Texture::reset()
{
    for (int c = 0; c < 2; ++c) {
        Side& s = s_[c];
        s.rng.seed(0x5445585455524500ull + static_cast<uint64_t>(c));   // "TEXTURE"
        s.burst = 0.0f;
        s.walk = 0.0f;
        s.crackleBand.reset();
        s.erosionBand.reset();
        s.hissLp.reset();
        const float fs = static_cast<float>(sr_);
        s.crackleBand.setQ(2500.0f, 1.2f, fs);
        s.erosionBand.setQ(900.0f, 0.8f, fs);
        s.hissLp.setQ(6000.0f, 0.7071f, fs);
        s.burstDecay = static_cast<float>(std::exp(-1.0 / (0.0015 * sr_)));
    }
    humPhase_ = 0.0;
    gain_ = 0.0f;
    on_ = false;
}

void Texture::update(const float* v)
{
    level_ = v[texture::Level] <= -59.9f ? 0.0f : dbToGain(v[texture::Level]);
    crackle_ = v[texture::Crackle];
    hum_ = v[texture::Hum];
    humHz_ = v[texture::HumHz];
    erosion_ = v[texture::Erosion];
    width_ = v[texture::Width];
}

void Texture::process(float* L, float* R, int n)
{
    const float rate = static_cast<float>(10.0 * crackle_ / sr_);   // clicks per sample
    const float walkStep = static_cast<float>(1.0 / sr_);
    float out[2];
    for (int i = 0; i < n; ++i) {
        if (!on_ && gain_ <= 0.0f) { for (; i < n; ++i) L[i] = R[i] = 0.0f; break; }   // faded out: resting
        gain_ = on_ ? std::min(1.0f, gain_ + gainStep_) : std::max(0.0f, gain_ - gainStep_);
        humPhase_ += humHz_ / sr_;
        humPhase_ -= std::floor(humPhase_);
        const double p = kTwoPiD * humPhase_;
        const float hum = static_cast<float>(std::sin(p) + 0.5 * std::sin(2 * p) + 0.3 * std::sin(3 * p) + 0.15 * std::sin(5 * p)) * 0.35f * hum_;
        for (int c = 0; c < 2; ++c) {
            Side& s = s_[c];
            // A click: a burst whose size is heavy-tailed (the cube of a uniform: many ticks, a few pops).
            if (s.rng.uniform() < rate) {
                const float u = s.rng.uniform();
                s.burst += 0.2f + 2.5f * u * u * u;
            }
            const float nz = s.rng.bipolar();
            float lp, bp, hp;
            s.crackleBand.tick(nz * s.burst, lp, bp, hp);
            const float crackle = bp * s.crackleBand.k;
            s.burst *= s.burstDecay;
            const float hiss = s.hissLp.lp(nz) * 0.02f * crackle_;
            // Erosion: band noise under a slow random walk of its level.
            s.walk += walkStep * (s.rng.bipolar() * 3.0f - s.walk * 0.3f);
            s.erosionBand.tick(s.rng.bipolar(), lp, bp, hp);
            const float erosion = bp * s.erosionBand.k * erosion_ * 0.25f * (0.6f + 0.4f * std::tanh(s.walk * 4.0f));
            out[c] = crackle * 0.5f + hiss + erosion + hum;
        }
        const float mid = 0.5f * (out[0] + out[1]), side = 0.5f * (out[0] - out[1]) * width_;
        L[i] = (mid + side) * level_ * gain_;
        R[i] = (mid - side) * level_ * gain_;
    }
}

} // namespace umb
