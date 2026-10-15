/**
 * @file Rumble.h
 * @brief The rumble (PLAN 5.2): the kick's hall above a split, and under it a sine that continues the kick's phase.
 *
 * Every source the research converges on builds the rumble the same way (Dok. 3, 8.4): a copy of the kick into a
 * hall at 100 % wet, 1 to 4 s, overdriven, high-passed at 30 to 50 Hz, low-passed at 100 to 300 Hz, ducked by the
 * kick. That recipe has one weakness a generator cannot hide behind a producer's ears (Erg. 4): a hall turns every
 * frequency's phase differently, so under the kick's fundamental its tail meets the kick's own ring at a random
 * angle -- sometimes adding, sometimes cancelling, beat by beat.
 *
 * **The split.** So the kick goes into the hall only above a split (Split, 80 Hz: a fourth-order Linkwitz-Riley high
 * pass, two Butterworth sections), where the angle is timbre, not level:
 * @code
 *   kick body -> LR4 high pass at Split -> hall (100 % wet, mono sum of the FDN's two returns)
 *             -> tanh at 4x (Drive) -> high pass at Split -> low pass at f0 x Ratio (Resonance)  = the band
 * @endcode
 * **The sub.** Under the split plays no hall but a sine at the kick's end pitch f0, whose phase is the kick's own
 * asymptote: Kick::asymptoticPhase() gives c with the kick's output phase -> f0 t + c, so sin(2 pi (f0 t + c)) is the
 * kick's tail continued, and the two can only add. Its amplitude follows the band's envelope (Sub Attack, Sub
 * Release), so the sub swells and dies with the hall above it.
 *
 * **The duck** is event-driven (Ducker.h): on every kick the band falls by Duck dB over a millisecond, holds (Duck
 * Hold, about the kick's body) and returns (Duck Release, raised cosine). The sub is gated by the same curve all the
 * way down: during the kick's body the kick *is* the sub, and the handover happens in phase. The sub's phase is set
 * anew only once the gate is shut -- a millisecond after the trigger -- so no jump is ever heard.
 *
 * Output mono: nothing under 300 Hz leaves the centre (PLAN 8.2).
 *
 * **Calibration** (27.09.2026, selftest testRumbleLevel). The hall's return of a default kick peaks at -13 dBFS, far
 * below anything a clip would touch, and the first version's rumble sat 29 dB under the kick. Two constants now stage
 * it: +12 dB in front of the clip, so that at the defaults the return's peaks reach it at -1 dBFS and Drive 6 dB takes
 * about 5 dB off them (Dok. 8.4: "SoftClip 4-6 dB"); after the band's filters a gain that makes Level the rumble's
 * level against the kick's, RMS over a beat (-9 dB at the default Level: Dok. 8.7's "-6 .. -10 dB", itself [I]): +7.5 dB
 * with the sub at -4 dB, +5.1 dB since the sub stands at 0 dB (the reference measurement's sub share, PLAN 13.4:
 * 0.45 of the low end in the hypnotic records). With the sub there the band under 80 Hz gains 0.8 to 2.4 dB per beat
 * over the kick alone, and the sub's phase stays within 1.3 degrees of the kick's tail (testRumble, f0 46 .. 61 Hz,
 * halls of 1 and 3.5 s).
 * @author Rene Weller
 */
#pragma once
#include "tot/Dsp.h"
#include "tot/Oversample.h"
#include "tot/fx/Reverb.h"
#include "tot/mix/Ducker.h"
#include <vector>

namespace tot
{

/** @brief The rumble generator. */
class Rumble
{
public:
    /**
     * @brief Allocates for a sample rate and the longest block process() will see.
     * @param[in] sampleRate  the sample rate in Hz
     * @param[in] maxBlock    the longest block process() will see
     */
    void prepare( double sampleRate, int maxBlock );
    /** @brief Silence. */
    void reset();
    /**
     * @brief Reads the effective parameter values.
     * @param[in] v   values indexed by rumble::
     * @param[in] f0  the kick's tuned end pitch, Hz
     */
    void update( const float *v, float f0 );
    /**
     * @brief A kick starts.
     * @param[in] late       how many samples ago it ideally started (0 <= late < 1)
     * @param[in] asymptote  the kick's asymptotic phase c (Kick::asymptoticPhase), cycles
     * @param[in] f0         the kick's end pitch, Hz
     */
    void kick( double late, double asymptote, double f0 );
    /**
     * @brief Renders @p n samples (at most the maxBlock given to prepare()).
     * @param[in]  kickBody  the kick's body after its EQ (Kick::process, @c body)
     * @param[out] out       receives the rumble, mono (replaced)
     * @param[in]  n         how many samples, at most maxBlock
     */
    void process( const float *kickBody, float *out, int n );
    /**
     * @brief The clip at four times the rate (the default) or at the rate (the Quest's quality, PLAN 11: the band is
     *         low-passed, its harmonics stay far under Nyquist).
     * @param[in] on  true to clip at four times the rate
     */
    void setOversampling( bool on ) { m_oversample = on; }
    /**
     * @brief The band's part of the last block alone, for the tests (mono, before the level).
     * @return the band's part of the last block
     */
    const std::vector<float> &lastBand() const { return m_band; }
    /**
     * @brief The sub's part of the last block alone, for the tests (before the level).
     * @return the sub's part of the last block
     */
    const std::vector<float> &lastSub() const { return m_subOut; }
    /**
     * @brief The hall's return in front of the clip, of the last block (for the calibration).
     * @return the hall's return of the last block, in front of the clip
     */
    const std::vector<float> &lastHall() const { return m_pre; }
    /**
     * @brief The sub's current phase in cycles (wrapped), for the tests.
     * @return the sub's current phase in cycles, wrapped
     */
    double subPhase() const { return m_subPhase; }

private:
    double                 m_sr = 48000.0;         ///< the sample rate, Hz
    Svf                    m_split1;               ///< the LR4 high pass at the split: its first Butterworth section
    Svf                    m_split2;               ///< ... its second section
    Reverb                 m_hall;                 ///< the hall
    Oversampler4           m_os;                   ///< the clip at four times the rate
    bool                   m_oversample = true;    ///< setOversampling()
    Svf                    m_postHp;               ///< the band's high pass after the clip
    Svf                    m_postLp;               ///< the band's low pass after the clip
    Ducker                 m_duck;                 ///< the kick's duck
    std::vector<float>     m_in;                   ///< the kick's body, the input
    std::vector<float>     m_hallL;                ///< the hall's return, left
    std::vector<float>     m_hallR;                ///< the hall's return, right
    std::vector<float>     m_band;                 ///< the band above the split
    std::vector<float>     m_subOut;               ///< the sub under it
    std::vector<float>     m_pre;                  ///< the band before the clip
    float                  m_level = 0.0f;         ///< the level, linear
    float                  m_subGain = 0.0f;       ///< the sub's gain, linear
    float                  m_clipGain = 1.0f;      ///< the gain into the clip, linear
    float                  m_envAtt = 0.01f;       ///< the band's envelope follower: attack coefficient
    float                  m_envRel = 0.001f;      ///< ... release coefficient
    float                  m_env = 0.0f;           ///< the follower's envelope
    double                 m_subPhase = 0.0;       ///< the sub's phase, cycles
    double                 m_subInc = 0.0;         ///< the sub's step, cycles per sample
    double                 m_f0 = 50.0;            ///< the pitch the sub runs at
    int                    m_resetIn = -1;         ///< samples until the sub's phase is set anew (-1: nothing pending)
    int                    m_sinceKick = 0;        ///< samples since the last kick's trigger
    double                 m_kickLate = 0.0;       ///< the last kick's sub-sample offset
    double                 m_kickC = 0.0;          ///< the last kick's asymptotic phase, cycles
    double                 m_kickF0 = 50.0;        ///< the last kick's end pitch, Hz
    float                  m_duckDepth = 0.5f;     ///< the duck's depth, 0..1
    float                  m_duckHoldMs = 80.0f;   ///< the duck's hold, ms
    float                  m_duckReleaseMs = 220.0f;   ///< the duck's release, ms
    static constexpr float DuckAttackMs = 1.0f;        ///< the duck's attack: the sub's gate shuts within it
};

}   // namespace tot
