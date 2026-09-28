/**
 * @file Chord.h
 * @brief The dub chord (PLAN 5.6): a minor triad or seventh from two detuned saws per tone, then the Basic Channel colour
 *        on its bus -- a band pass that breathes, a few bits, a phaser, the hole at 600 Hz.
 *
 * Dok. 8.4's recipe, in the order the signal takes it:
 * @code
 *   per tone: two PolyBLEP saws +-Detune cents, one left and one right (Width), a low pass at Brightness that the
 *             note's own envelope opens by Env Amount octaves, the amp envelope (A 10 ms / D 300 ms / S 15 %)
 *   bus:      band pass (or high pass) at Band, moved +-Sweep octaves by a sine of Sweep Rate
 *             -> Bits at Crush Mix (Redux 6-8 bit at about 20 %) -> four-stage phaser at Phaser Rate
 *             -> the dip at 600 Hz (-7 dB, wide)
 * @endcode
 * The echo, the springs and the plate follow on the sends (Dub.h). Every modulation's phase comes from the absolute
 * sample the engine passes, and the bus's coefficients change on an absolute raster of 16 samples: a bar alone sounds as
 * the bar in sequence.
 */
#pragma once
#include "tot/Dsp.h"
#include "tot/synth/Oscillator.h"
#include <cstdint>

namespace tot {

/** @brief The chord voices and their bus. */
class ChordSynth {
public:
    static constexpr int kVoices = 8;   ///< two chords of four tones overlapping

    /** @brief Prepares for a sample rate. */
    void prepare(double sampleRate);
    /** @brief Silence. */
    void reset();
    /** @brief Reads the effective parameter values (indexed by chord::). */
    void update(const float* v);
    /** @brief Starts a tone. @param late samples since its ideal start */
    void noteOn(int pitch, float velocity, double late);
    /** @brief Releases every voice playing @p pitch. */
    void noteOff(int pitch);
    /**
     * @brief Renders @p n samples of the bus into @p L and @p R (replaced).
     * @param sample the absolute sample of the first one (the modulations' phase)
     */
    void process(float* L, float* R, int n, int64_t sample);

private:
    struct Voice {
        bool active = false;
        int pitch = -1;
        VaOscillator a, b;
        Envelope amp;
        double fenv = 0.0;
        float vel = 1.0f;
        Svf lpA, lpB;
        uint32_t age = 0;              ///< when it started, in notes (the oldest is taken)
        uint32_t n = 0;                ///< samples since it started
    };
    void setBus(int64_t sample);
    double sr_ = 48000.0;
    Voice v_[kVoices];
    uint32_t clock_ = 0;
    // Settings.
    float detune_ = 10.0f, width_ = 0.6f, attack_ = 0.01f, decay_ = 0.3f, sustain_ = 0.15f, release_ = 0.4f;
    float bright_ = 2500.0f, envAmt_ = 1.5f, bandHz_ = 380.0f, bandK_ = 1.4f, sweep_ = 0.5f, bits_ = 8.0f, crushMix_ = 0.2f;
    float phaser_ = 0.5f, dipGain_ = 0.45f;
    double sweepRate_ = 0.2, phaserRate_ = 0.5, fenvDecay_ = 0.999;
    int octave_ = 0;
    bool highPass_ = false;
    Svf bandL_, bandR_, dipL_, dipR_;
    float apL_[4] = {}, apR_[4] = {}, apCoef_ = 0.0f;   ///< the phaser's first-order all-passes
};

} // namespace tot
