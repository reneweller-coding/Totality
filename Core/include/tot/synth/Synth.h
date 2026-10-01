/**
 * @file Synth.h
 * @brief The mono synth (PLAN 5.3): the bass in the SH-101 idiom and the 303 line, one voice each, one parameter table.
 *
 * Signal path, at twice the sample rate where anything is nonlinear:
 * @code
 *   PolyBLEP saw..pulse + square sub an octave down -> tanh drive -> circuit filter (Filters.h, Ephemeris' models)
 *   -> half-band decimator -> high pass (Low Cut) -> low pass (High Cut) -> amp envelope -> duck -> level, pan
 * @endcode
 * **The filter** is one of Ephemeris' circuit models, solved sample by sample: the Juno's IR3109 cascade by default
 * (the SH-101 has the same chip), the diode ladder for the 303 line. Its cutoff is Cutoff times 2^(Env Amount x env) times
 * the key tracking, the envelope an exponential Decay retriggered by every note that does not slide.
 *
 * **Accent** (the 303's): an accented note opens the envelope further (Accent times an octave and a half) and plays
 * louder (Accent times 4 dB). **Slide**: the note after a slid one does not retrigger either envelope and glides to its
 * pitch in Glide ms (or 60 ms when Glide is 0); a plain Glide above 0 glides every note (portamento).
 *
 * **Low end** (PLAN 5.2): where the rumble owns the band under the split, the engine raises the voice's Low Cut to at
 * least 100 Hz, so the bass carries its harmonics above the kick's fundamental and never its own fundamental under it.
 */
#pragma once
#include "tot/Dsp.h"
#include "tot/Halfband.h"
#include "tot/mix/Ducker.h"
#include "tot/synth/Filters.h"
#include "tot/synth/Oscillator.h"

namespace tot {

/** @brief One monophonic synth voice with its strip. */
class MonoSynth {
public:
    /** @brief Prepares for a sample rate. */
    void prepare(double sampleRate);
    /** @brief Silence. */
    void reset();
    /**
     * @brief Reads the effective parameter values (indexed by synth::).
     * @param minLowCut the lowest Low Cut the engine allows now (0: none; 100 Hz where the rumble owns the low end)
     */
    void update(const float* v, float minLowCut);
    /**
     * @brief Starts a note.
     * @param pitch    MIDI note
     * @param velocity 0..1
     * @param late     samples since its ideal start (0 <= late < 1)
     * @param accent   the 303's accent
     * @param slide    this note slides into the next (the next neither retriggers nor jumps)
     */
    void noteOn(int pitch, float velocity, double late, bool accent, bool slide);
    /** @brief Releases the note (unless a slid note has taken over). */
    void noteOff();
    /** @brief A kick starts: the duck. */
    void kick(double late) { duck_.trigger(late); }
    /** @brief Renders @p n samples into @p L and @p R (replaced). */
    void process(float* L, float* R, int n);
    /** @brief Whether anything sounds. */
    bool active() const { return amp_.isActive(); }

private:
    double sr_ = 48000.0;   ///< the sample rate, Hz
    VaOscillator osc_;                 ///< at twice the rate
    double subPhase_ = 0.0;            ///< the square sub's phase, cycles
    FilterLane filt_;   ///< the filter, at twice the rate
    HalfbandDesign hb_;   ///< the half-band filter back down to the rate
    HalfbandDown<float> down_;   ///< its state
    Envelope amp_;   ///< the amp envelope
    double fenv_ = 0.0;   ///< the filter envelope (at the high rate)
    double fenvDecay_ = 0.999;   ///< its per-sample factor
    double hz_ = 110.0;   ///< the pitch, Hz
    double targetHz_ = 110.0;   ///< its target (a glide), Hz
    double glideCoef_ = 1.0;   ///< the glide's per-sample step
    bool slidePending_ = false;        ///< the last note slides into the next
    bool noteAccent_ = false;   ///< the note sounding is accented
    float velocity_ = 1.0f;   ///< the note's velocity
    Ducker duck_;   ///< the duck under the kick
    Svf hp_;   ///< the strip's low cut
    Svf lp_;   ///< the strip's high cut
    /// Settings.
    FilterModel model_ = FilterModel::Juno;
    float wave_ = 0.0f;   ///< the wave: 0 saw .. 1 pulse
    float pw_ = 0.5f;   ///< the pulse's width
    float sub_ = 0.3f;   ///< the square sub's level
    float cutoff_ = 350.0f;   ///< the filter's cutoff, Hz
    float res_ = 0.2f;   ///< the filter's resonance
    float envAmt_ = 2.0f;   ///< how far the filter envelope opens it, octaves
    float accent_ = 0.3f;   ///< the accent's amount
    float drive_ = 1.0f;   ///< the drive into the filter
    float driveNorm_ = 1.0f;   ///< the gain that keeps the level after it
    float keyTrack_ = 0.5f;   ///< the cutoff's key tracking, 0..1
    float level_ = 0.3f;   ///< the level, linear
    float gl_ = 0.7f;   ///< the pan's gain, left
    float gr_ = 0.7f;   ///< the pan's gain, right
    float glideMs_ = 0.0f;   ///< the glide, ms
    float attack_ = 0.003f;   ///< the amp envelope's attack, seconds
    float decay_ = 0.25f;   ///< its decay, seconds
    float sustain_ = 0.6f;   ///< its sustain, 0..1
    float release_ = 0.3f;   ///< its release, seconds
    double filtDecayS_ = 0.18;   ///< the filter envelope's decay, seconds
};

} // namespace tot
