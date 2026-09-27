/**
 * @file Chord.cpp
 * @brief The dub chord's voices and bus.
 */
#include "umb/synth/Chord.h"
#include "umb/Params.h"
#include <algorithm>
#include <cmath>

namespace umb {

namespace {
constexpr double kTwoPiD = 6.283185307179586;
constexpr double kLn1000 = 6.907755278982137;
}

void ChordSynth::prepare(double sampleRate)
{
    sr_ = sampleRate;
    for (Voice& v : v_) v.amp.setSampleRate(sampleRate);
    reset();
}

void ChordSynth::reset()
{
    for (Voice& v : v_) { v.active = false; v.amp.kill(); v.lpA.reset(); v.lpB.reset(); v.pitch = -1; }
    bandL_.reset(); bandR_.reset(); dipL_.reset(); dipR_.reset();
    for (int i = 0; i < 4; ++i) apL_[i] = apR_[i] = 0.0f;
    setBus(0);
}

void ChordSynth::update(const float* p)
{
    detune_ = p[chord::Detune];
    width_ = p[chord::Width];
    octave_ = static_cast<int>(std::lround(p[chord::Octave]));
    attack_ = p[chord::Attack] * 0.001f;
    decay_ = p[chord::Decay] * 0.001f;
    sustain_ = p[chord::Sustain];
    release_ = p[chord::Release] * 0.001f;
    bright_ = p[chord::Bright];
    envAmt_ = p[chord::EnvAmount];
    fenvDecay_ = std::exp(-kLn1000 / (std::max(0.01, static_cast<double>(decay_)) * sr_));
    bandHz_ = p[chord::Band];
    highPass_ = std::lround(p[chord::BandMode]) == 1;
    bandK_ = 1.0f / std::max(0.2f, p[chord::BandQ]);
    sweep_ = p[chord::Sweep];
    sweepRate_ = p[chord::SweepRate];
    bits_ = p[chord::Crush];
    crushMix_ = p[chord::CrushMix];
    phaser_ = p[chord::Phaser];
    phaserRate_ = p[chord::PhaserRate];
    dipGain_ = dbToGain(p[chord::Dip]);
}

void ChordSynth::noteOn(int pitch, float velocity, double late)
{
    // A free voice, else the oldest.
    int pick = 0;
    uint32_t oldest = 0;
    for (int i = 0; i < kVoices; ++i) {
        if (!v_[i].active) { pick = i; break; }
        const uint32_t age = clock_ - v_[i].age;
        if (age >= oldest) { oldest = age; pick = i; }
    }
    Voice& v = v_[pick];
    v.active = true;
    v.pitch = pitch;
    v.vel = clampv(velocity, 0.0f, 1.0f);
    v.age = clock_++;
    v.n = 0;
    const double hz = midiToHz(pitch + 12 * octave_);
    const double spread = std::pow(2.0, detune_ / 1200.0);
    v.a.set(hz / spread, sr_, 0.0f, 0.5f);
    v.b.set(hz * spread, sr_, 0.0f, 0.5f);
    v.a.restart(0.0, late);
    v.b.restart(0.37, late);
    v.amp.setTimes(attack_, decay_, sustain_, release_);
    v.amp.noteOn();
    v.amp.advanceAttack(late);
    v.fenv = std::pow(fenvDecay_, late);
}

void ChordSynth::noteOff(int pitch)
{
    for (Voice& v : v_) if (v.active && v.pitch == pitch) v.amp.noteOff();
}

void ChordSynth::setBus(int64_t sample)
{
    const float fs = static_cast<float>(sr_);
    const double t = static_cast<double>(sample) / sr_;
    const float centre = bandHz_ * static_cast<float>(std::pow(2.0, sweep_ * std::sin(kTwoPiD * sweepRate_ * t)));
    bandL_.setK(centre, bandK_, fs);
    bandR_.setK(centre, bandK_, fs);
    dipL_.setQ(600.0f, 0.7f, fs);
    dipR_.setQ(600.0f, 0.7f, fs);
    // The phaser's notches sweep 300 Hz .. 3 kHz; one first-order all-pass coefficient for all four stages.
    const double ph = 0.5 + 0.5 * std::sin(kTwoPiD * phaserRate_ * t + 1.1);
    const double fc = 300.0 * std::pow(10.0, ph);
    const double w = std::tan(kTwoPiD * 0.5 * std::min(fc, 0.45 * sr_) / sr_);
    apCoef_ = static_cast<float>((w - 1.0) / (w + 1.0));
}

void ChordSynth::process(float* L, float* R, int n, int64_t sample)
{
    const float fs = static_cast<float>(sr_);
    // Width 1: oscillator a hard left, b hard right; width 0: both in the middle (constant power).
    const float near = std::cos(0.25f * kPi * (1.0f - width_)), far = std::sin(0.25f * kPi * (1.0f - width_));
    for (int i = 0; i < n; ++i) { L[i] = 0.0f; R[i] = 0.0f; }
    for (Voice& v : v_) {
        if (!v.active) continue;
        for (int i = 0; i < n; ++i) {
            // The voice's low pass follows its envelope every eighth sample of the note (its own clock, not the host's).
            if ((v.n & 7u) == 0) {
                const float fc = std::min(0.45f * fs, bright_ * static_cast<float>(std::pow(2.0, envAmt_ * v.fenv)));
                v.lpA.setQ(fc, 0.7071f, fs);
                v.lpB.copyCoefficients(v.lpA);
            }
            ++v.n;
            const float e = v.amp.process() * v.vel;
            // Its envelope ended: free at this very sample. (Freed at the span's end, it ran its filters on for a
            // host-dependent while, and a voice taken again started from a state the blocks had chosen.)
            if (!v.amp.isActive()) { v.active = false; break; }
            const float a = v.lpA.lp(v.a.next()) * e, b = v.lpB.lp(v.b.next()) * e;
            v.fenv *= fenvDecay_;
            L[i] += a * near + b * far;
            R[i] += b * near + a * far;
        }
    }
    const float levels = std::pow(2.0f, bits_ - 1.0f);
    for (int i = 0; i < n; ++i) {
        if (((sample + i) & 15) == 0) setBus(sample + i);
        float lp, bp, hp;
        bandL_.tick(L[i], lp, bp, hp);
        float l = highPass_ ? hp : bp * bandL_.k;
        bandR_.tick(R[i], lp, bp, hp);
        float r = highPass_ ? hp : bp * bandR_.k;
        // A few bits, mixed in.
        l += crushMix_ * (std::round(l * levels) / levels - l);
        r += crushMix_ * (std::round(r * levels) / levels - r);
        // The phaser: four first-order all-passes, their sum with the dry signal (notches), at the depth Phaser.
        float xl = l, xr = r;
        for (int s = 0; s < 4; ++s) {
            const float yl = apCoef_ * xl + apL_[s];
            apL_[s] = xl - apCoef_ * yl;
            xl = yl;
            const float yr = apCoef_ * xr + apR_[s];
            apR_[s] = xr - apCoef_ * yr;
            xr = yr;
        }
        l = l + phaser_ * 0.5f * (xl - l);
        r = r + phaser_ * 0.5f * (xr - r);
        // The dip at 600 Hz.
        dipL_.tick(l, lp, bp, hp);
        l += (dipGain_ - 1.0f) * bp * dipL_.k;
        dipR_.tick(r, lp, bp, hp);
        r += (dipGain_ - 1.0f) * bp * dipR_.k;
        L[i] = l;
        R[i] = r;
    }
}

} // namespace umb
