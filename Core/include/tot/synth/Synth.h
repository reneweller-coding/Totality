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
 * @author Rene Weller
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
    /**
     * @brief Prepares for a sample rate.
     * @param[in] sampleRate  the sample rate in Hz
     */
    void prepare(double sampleRate);
    /** @brief Silence. */
    void reset();
    /**
     * @brief Reads the effective parameter values (indexed by synth::).
     * @param[in] v          the effective parameter values, indexed by synth::
     * @param[in] minLowCut  the lowest Low Cut the engine allows now (0: none; 100 Hz where the rumble owns the low
     *                       end)
     */
    void update(const float* v, float minLowCut);
    /**
     * @brief Starts a note.
     * @param[in] pitch     MIDI note
     * @param[in] velocity  0..1
     * @param[in] late      samples since its ideal start (0 <= late < 1)
     * @param[in] accent    the 303's accent
     * @param[in] slide     this note slides into the next (the next neither retriggers nor jumps)
     */
    void noteOn(int pitch, float velocity, double late, bool accent, bool slide);
    /** @brief Releases the note (unless a slid note has taken over). */
    void noteOff();
    /**
     * @brief A kick starts: the duck.
     * @param[in] late  samples since the kick's ideal start
     */
    void kick(double late) { m_duck.trigger(late); }
    /**
     * @brief Renders @p n samples into @p L and @p R (replaced).
     * @param[out] L  receives the left channel (replaced)
     * @param[out] R  receives the right channel (replaced)
     * @param[in]  n  how many samples
     */
    void process(float* L, float* R, int n);
    /**
     * @brief Whether anything sounds.
     * @return true while anything sounds
     */
    bool active() const { return m_amp.isActive(); }

private:
    double m_sr = 48000.0;   ///< the sample rate, Hz
    VaOscillator m_osc;                 ///< at twice the rate
    double m_subPhase = 0.0;            ///< the square sub's phase, cycles
    FilterLane m_filt;   ///< the filter, at twice the rate
    HalfbandDesign m_hb;   ///< the half-band filter back down to the rate
    HalfbandDown<float> m_down;   ///< its state
    Envelope m_amp;   ///< the amp envelope
    double m_fenv = 0.0;   ///< the filter envelope (at the high rate)
    double m_fenvDecay = 0.999;   ///< its per-sample factor
    double m_hz = 110.0;   ///< the pitch, Hz
    double m_targetHz = 110.0;   ///< its target (a glide), Hz
    double m_glideCoef = 1.0;   ///< the glide's per-sample step
    bool m_slidePending = false;        ///< the last note slides into the next
    bool m_noteAccent = false;   ///< the note sounding is accented
    float m_velocity = 1.0f;   ///< the note's velocity
    Ducker m_duck;   ///< the duck under the kick
    Svf m_hp;   ///< the strip's low cut
    Svf m_lp;   ///< the strip's high cut
    /// Settings.
    FilterModelE m_model = FilterModelE::JUNO;
    float m_wave = 0.0f;   ///< the wave: 0 saw .. 1 pulse
    float m_pw = 0.5f;   ///< the pulse's width
    float m_sub = 0.3f;   ///< the square sub's level
    float m_cutoff = 350.0f;   ///< the filter's cutoff, Hz
    float m_res = 0.2f;   ///< the filter's resonance
    float m_envAmt = 2.0f;   ///< how far the filter envelope opens it, octaves
    float m_accent = 0.3f;   ///< the accent's amount
    float m_drive = 1.0f;   ///< the drive into the filter
    float m_driveNorm = 1.0f;   ///< the gain that keeps the level after it
    float m_keyTrack = 0.5f;   ///< the cutoff's key tracking, 0..1
    float m_level = 0.3f;   ///< the level, linear
    float m_gl = 0.7f;   ///< the pan's gain, left
    float m_gr = 0.7f;   ///< the pan's gain, right
    float m_glideMs = 0.0f;   ///< the glide, ms
    float m_attack = 0.003f;   ///< the amp envelope's attack, seconds
    float m_decay = 0.25f;   ///< its decay, seconds
    float m_sustain = 0.6f;   ///< its sustain, 0..1
    float m_release = 0.3f;   ///< its release, seconds
    double m_filtDecayS = 0.18;   ///< the filter envelope's decay, seconds
};

} // namespace tot
