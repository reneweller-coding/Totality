/**
 * @file Ping.h
 * @brief The ping (PLAN 5.5, Erg. 1): the struck, metallic-organic "drip" and "clonk" of the hypnotic school -- FM with an
 *        inharmonic ratio through a low-pass gate with a vactrol's decay, then a slowly wandering band pass.
 *
 * **The source** is two-operator phase modulation: y = sin(2 pi phi_c + I(t) sin(2 pi phi_m)). The carrier sweeps down
 * from (1 + Pitch Amount) times its note onto the note in Pitch Decay (30 to 90 ms, Erg. 1); the modulator follows the
 * carrier at Ratio times its frequency -- a ratio that is no whole number (sqrt 2, e, 7:4) gives no chord but the
 * partials of a struck object -- and the index falls from Index times the velocity with its own time constant, so the
 * hit is bright and the tail nearly a sine.
 *
 * **The low-pass gate** (Buchla 292; Parker and D'Angelo, "A Digital Model of the Buchla Lowpass-Gate", DAFx 2013).
 * A vactrol -- a lamp and a photoresistor -- opens a low pass and a VCA together. Its light answers in a millisecond,
 * its resistance recovers slowly and the more slowly the darker it gets. Here the vactrol's state v jumps towards 1
 * with a 1 ms time constant at a hit and then falls with a time constant that grows as it falls,
 * tau(v) = LPG Release (0.2 + 1.6 (1 - v)^2) -- the long, soft tail of a pinged gate -- and v sets both the low pass
 * (80 Hz times 225^v, two poles, Resonance) and the gain (v^1.5). LPG blends between this gate and a plain exponential
 * amplitude (Decay): at 0 the ping is a clean FM bleep, at 1 a Buchla bongo.
 *
 * **The bus**: the voices' sum through a band pass (Band, Band Q) whose centre a slow sine moves by +-Sweep octaves at
 * Sweep Rate -- the phase of that sine is a function of the absolute sample the engine passes, so a bar alone sounds as
 * the bar in sequence (Band Mix blends it with the dry sum). Each note sits at its own place in the stereo field,
 * spread by Width around Pan.
 *
 * Eight voices, the oldest taken when a ninth note comes. Scalar: eight voices of a few sines are cheap.
 */
#pragma once
#include "tot/Dsp.h"
#include <cstdint>

namespace tot {

/** @brief The ping voices and their bus. */
class Ping {
public:
    static constexpr int kVoices = 8;   ///< voices

    /** @brief Prepares for a sample rate. */
    void prepare(double sampleRate);
    /** @brief Silence. */
    void reset();
    /** @brief Reads the effective parameter values (indexed by ping::). */
    void update(const float* v);
    /**
     * @brief Starts a note.
     * @param pitch    MIDI note of the carrier
     * @param velocity 0..1
     * @param late     samples since its ideal start (0 <= late < 1)
     * @param serial   a number of the note (its place in the score), for its deterministic place in the stereo field
     */
    void noteOn(int pitch, float velocity, double late, uint32_t serial);
    /**
     * @brief Renders @p n samples of the bus into @p L and @p R (replaced).
     * @param sample the absolute sample of the first one (the sweep's phase)
     */
    void process(float* L, float* R, int n, int64_t sample);
    /** @brief Whether any voice sounds. */
    bool active() const;

private:
    /** @brief One struck note: its phases, its envelopes, its vactrol, its place. */
    struct Voice {
        bool active = false;   ///< it sounds
        double pc = 0.0;   ///< the carrier's phase, cycles
        double pm = 0.0;   ///< the modulator's phase, cycles
        double f0 = 440.0;             ///< the note
        double ep = 1.0;   ///< the pitch envelope
        double dp = 1.0;   ///< its per-sample factor
        double ei = 1.0;   ///< the index envelope
        double di = 1.0;   ///< its per-sample factor
        double ea = 1.0;   ///< the amplitude envelope
        double da = 1.0;   ///< its per-sample factor
        double vac = 0.0;              ///< the vactrol's state
        float vel = 1.0f;   ///< the note's velocity
        float gl = 0.7f;   ///< the pan's gain, left
        float gr = 0.7f;   ///< the pan's gain, right
        Svf lpg;                       ///< the gate's low pass
        int age = 0;                   ///< samples since the start
    };
    double sr_ = 48000.0;   ///< the sample rate, Hz
    Voice v_[kVoices];   ///< the voices
    int next_ = 0;   ///< the voice the next note takes
    // Settings.
    float level_ = 0.5f;   ///< ping.level, linear
    float pan_ = 0.0f;   ///< the centre of the notes' places, -1 .. 1
    float width_ = 0.5f;   ///< how far the notes spread around it
    double ratio_ = 1.414;   ///< the modulator's frequency ratio
    double index_ = 2.0;   ///< the modulation index at the hit
    double pitchAmt_ = 0.8;   ///< how far above the note the carrier starts (times the note - 1)
    double pitchTau_ = 0.045;   ///< the pitch sweep's time constant, s
    double indexTau_ = 0.06;   ///< the index's time constant, s
    double decayS_ = 0.12;   ///< the plain amplitude decay, s
    double lpgRelease_ = 0.15;   ///< the vactrol's release, s
    float lpgMix_ = 0.6f;   ///< gate against plain decay, 0..1
    float damping_ = 1.3f;   ///< the gate's low pass damping (from Resonance)
    float bandHz_ = 900.0f;   ///< the bus's band pass, Hz
    float bandK_ = 1.0f;   ///< its damping, 1 / Q
    float sweepOct_ = 0.5f;   ///< the band's sweep, octaves either way
    float bandMix_ = 0.5f;   ///< band pass against the dry sum
    double sweepRate_ = 0.07;   ///< the sweep's rate, Hz
    double vacAttack_ = 0.0;   ///< per-sample factor of the vactrol's 1 ms attack
    Svf bandL_;   ///< the bus's band pass, left
    Svf bandR_;   ///< the bus's band pass, right
};

} // namespace tot
