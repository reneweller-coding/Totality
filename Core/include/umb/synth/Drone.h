/**
 * @file Drone.h
 * @brief The drone (PLAN 5.7) and the texture (Dok. 8.4): what lies under the loop in the hypnotic records.
 *
 * **Drone.** Two saws Detune cents apart, one to each side, on the tonic or the fifth above 150 Hz (the band under it is
 * the kick's and the rumble's, PLAN 5.2), through a resonant low pass whose cutoff a sine moves by +-Sweep octaves over
 * Sweep Bars -- the filter ride over 32 to 64 bars of Dok. 8.5 -- with minutes-long envelopes. The sine's phase is the
 * absolute beat, set by the engine on its absolute raster (at()), so a bar alone sounds as the bar in sequence and the
 * span splits of a host's blocks change no bit (a beat carried across a span by beat + i * rate would round differently).
 *
 * **Texture.** Three sounds of the room the record was made in, at -20 to -30 dB (Dok. 8.4):
 *  - vinyl crackle: clicks at random times (a Poisson process of some ten a second at Crackle 1), each a short decaying
 *    burst of noise through a band pass at 2.5 kHz, their sizes heavy-tailed (a few loud pops among many ticks), and a
 *    faint surface hiss under them;
 *  - mains hum at Hum Freq with its second, third and fifth harmonic (a transformer's), a whisper of its level wandering;
 *  - eroded noise: noise band-passed at 900 Hz whose level a slow random walk moves, the "Rausch-Erosion".
 * Left and right from their own random streams, blended towards mono by Width. It fades in and out with its notes (the
 * score marks where it plays) over two seconds.
 */
#pragma once
#include "umb/Dsp.h"
#include "umb/synth/Oscillator.h"
#include <cstdint>

namespace umb {

/** @brief The drone voice. */
class Drone {
public:
    void prepare(double sampleRate);
    void reset();
    /** @brief Reads the effective parameter values (indexed by drone::) and the tempo (the sweep's period is in bars). */
    void update(const float* v);
    void noteOn(int pitch, float velocity);
    void noteOff();
    /** @brief Sets the sweep for the absolute beat @p beat (the engine's raster cell). */
    void at(double beat);
    /** @brief Renders @p n samples into @p L and @p R (replaced). */
    void process(float* L, float* R, int n);
    bool active() const { return amp_.isActive(); }

private:
    double sr_ = 48000.0;
    VaOscillator a_, b_;
    Envelope amp_;
    Svf lpL_, lpR_;
    double hz_ = 220.0;
    float vel_ = 1.0f, level_ = 0.1f, detune_ = 8.0f, cutoff_ = 700.0f, res_ = 0.25f, sweep_ = 1.5f, attack_ = 4.0f, release_ = 6.0f;
    double sweepBeats_ = 128.0;
    int octave_ = 0;
};

/** @brief The texture. */
class Texture {
public:
    void prepare(double sampleRate);
    void reset();
    /** @brief Reads the effective parameter values (indexed by texture::). */
    void update(const float* v);
    /** @brief Fades in (@p on) or out over two seconds. */
    void gate(bool on) { on_ = on; }
    void process(float* L, float* R, int n);

private:
    struct Side {
        Rng rng;
        float burst = 0.0f, burstDecay = 0.99f;
        Svf crackleBand, erosionBand, hissLp;
        float walk = 0.0f;
    };
    double sr_ = 48000.0;
    Side s_[2];
    double humPhase_ = 0.0;
    float gain_ = 0.0f, gainStep_ = 0.0f;
    bool on_ = false;
    float level_ = 0.05f, crackle_ = 0.5f, hum_ = 0.2f, erosion_ = 0.4f, width_ = 0.8f;
    double humHz_ = 50.0;
};

} // namespace umb
