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
    /** @brief One tone: two saws, their low passes, its envelopes. */
    struct Voice {
        bool active = false;           ///< it sounds
        int pitch = -1;                ///< MIDI note, -1 none
        VaOscillator a;                ///< the saw on the left
        VaOscillator b;                ///< the saw on the right
        Envelope amp;                  ///< the amp envelope
        double fenv = 0.0;             ///< the filter envelope, 1 .. 0
        float vel = 1.0f;              ///< the note's velocity
        Svf lpA;                       ///< saw a's low pass
        Svf lpB;                       ///< saw b's low pass
        uint32_t age = 0;              ///< when it started, in notes (the oldest is taken)
        uint32_t n = 0;                ///< samples since it started
    };
    /** @brief Sets the bus's band pass and the dip for the absolute sample @p sample (the sweep's phase). */
    void setBus(int64_t sample);
    double sr_ = 48000.0;   ///< the sample rate, Hz
    Voice v_[kVoices];   ///< the voices
    uint32_t clock_ = 0;   ///< counts the notes started (a voice's age)
    // Settings.
    float detune_ = 10.0f;   ///< the saws' distance, cents
    float width_ = 0.6f;   ///< how far the two saws stand apart, 0..1
    float attack_ = 0.01f;   ///< the amp envelope's attack, seconds
    float decay_ = 0.3f;   ///< its decay, seconds
    float sustain_ = 0.15f;   ///< its sustain, 0..1
    float release_ = 0.4f;   ///< its release, seconds
    float bright_ = 2500.0f;   ///< the voices' low pass, Hz (Brightness)
    float envAmt_ = 1.5f;   ///< how far the note's envelope opens it, octaves
    float bandHz_ = 380.0f;   ///< the bus's band pass, Hz (Band)
    float bandK_ = 1.4f;   ///< its damping, 1 / Q
    float sweep_ = 0.5f;   ///< the band's sweep, octaves either way
    float bits_ = 8.0f;   ///< the crusher's bits
    float crushMix_ = 0.2f;   ///< the crusher's share
    float phaser_ = 0.5f;   ///< the phaser's share
    float dipGain_ = 0.45f;   ///< the dip at 600 Hz, linear
    double sweepRate_ = 0.2;   ///< the band's sweep rate, Hz
    double phaserRate_ = 0.5;   ///< the phaser's rate, Hz
    double fenvDecay_ = 0.999;   ///< the filter envelope's decay per sample
    int octave_ = 0;   ///< octaves the notes are moved by
    bool highPass_ = false;   ///< a high pass on the bus instead of the band pass
    Svf bandL_;   ///< the bus's band pass, left
    Svf bandR_;   ///< the bus's band pass, right
    Svf dipL_;   ///< the dip at 600 Hz, left
    Svf dipR_;   ///< the dip at 600 Hz, right
    float apL_[4] = {};   ///< the phaser's all-passes, left
    float apR_[4] = {};   ///< the phaser's all-passes, right
    float apCoef_ = 0.0f;   ///< the phaser's all-pass coefficient
};

} // namespace tot
