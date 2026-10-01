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
#include "tot/Dsp.h"
#include "tot/synth/Oscillator.h"
#include <cstdint>

namespace tot {

/** @brief The drone voice. */
class Drone {
public:
    /** @brief Sets the sample rate and falls silent. */
    void prepare(double sampleRate);
    /** @brief Silence: the envelope closed, the filters cleared. */
    void reset();
    /** @brief Reads the effective parameter values (indexed by drone::) and the tempo (the sweep's period is in bars). */
    void update(const float* v);
    /** @brief Starts @p pitch (MIDI, moved by Octave) at @p velocity 0..1, or glides there while a note sounds. */
    void noteOn(int pitch, float velocity);
    /** @brief Lets the note go: the release begins. */
    void noteOff();
    /** @brief Sets the sweep for the absolute beat @p beat (the engine's raster cell). */
    void at(double beat);
    /** @brief Renders @p n samples into @p L and @p R (replaced). */
    void process(float* L, float* R, int n);
    /** @brief Whether it sounds (the envelope is open). */
    bool active() const { return amp_.isActive(); }

private:
    double sr_ = 48000.0;   ///< the sample rate, Hz
    VaOscillator a_;   ///< the saw on the left
    VaOscillator b_;   ///< the saw on the right, Detune cents above
    Envelope amp_;   ///< the envelope, minutes long
    Svf lpL_;   ///< the low pass, left
    Svf lpR_;   ///< the low pass, right
    double hz_ = 220.0;   ///< the note's frequency, Hz
    float vel_ = 1.0f;   ///< the note's velocity
    float level_ = 0.1f;   ///< drone.level, linear
    float detune_ = 8.0f;   ///< the saws' distance, cents
    float cutoff_ = 700.0f;   ///< the low pass's cutoff at the sweep's centre, Hz
    float res_ = 0.25f;   ///< the low pass's resonance
    float sweep_ = 1.5f;   ///< the sweep's depth, octaves either way
    float attack_ = 4.0f;   ///< the envelope's attack, seconds
    float release_ = 6.0f;   ///< the envelope's release, seconds
    double sweepBeats_ = 128.0;   ///< the sweep's period, beats
    int octave_ = 0;   ///< octaves the note is moved by
};

/** @brief The texture. */
class Texture {
public:
    /** @brief Sets the sample rate and falls silent. */
    void prepare(double sampleRate);
    /** @brief Silence: the streams and filters started again, the gate closed. */
    void reset();
    /** @brief Reads the effective parameter values (indexed by texture::). */
    void update(const float* v);
    /** @brief Fades in (@p on) or out over two seconds. */
    void gate(bool on) { on_ = on; }
    /** @brief Renders @p n samples into @p L and @p R (replaced). */
    void process(float* L, float* R, int n);

private:
    /** @brief One side's noise: its random stream, its crackle, its filters. */
    struct Side {
        Rng rng;   ///< the side's random stream
        float burst = 0.0f;   ///< the crackle's burst as it decays
        float burstDecay = 0.99f;   ///< the burst's decay per sample (1.5 ms)
        Svf crackleBand;   ///< the band pass at 2.5 kHz the bursts go through
        Svf erosionBand;   ///< the band pass at 900 Hz of the eroded noise
        Svf hissLp;   ///< the surface hiss's low pass at 6 kHz
        float walk = 0.0f;   ///< the random walk of the erosion's level
    };
    double sr_ = 48000.0;   ///< the sample rate, Hz
    Side s_[2];   ///< left, right
    double humPhase_ = 0.0;   ///< the hum's phase, cycles
    float gain_ = 0.0f;   ///< the fade, 0..1
    float gainStep_ = 0.0f;   ///< the fade's step per sample (two seconds)
    bool on_ = false;   ///< the gate: fading in (true) or out
    float level_ = 0.05f;   ///< texture.level, linear
    float crackle_ = 0.5f;   ///< the crackle's amount
    float hum_ = 0.2f;   ///< the hum's amount
    float erosion_ = 0.4f;   ///< the eroded noise's amount
    float width_ = 0.8f;   ///< the width: 0 mono, 1 the sides apart
    double humHz_ = 50.0;   ///< the hum's frequency (Hum Freq), Hz
};

} // namespace tot
