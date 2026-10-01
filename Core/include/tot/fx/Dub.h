/**
 * @file Dub.h
 * @brief The dub chain (PLAN 5.9) and the multiband duck (PLAN 8.3).
 *
 * **The dub chain** is the mixing desk as an instrument of Basic Channel (Dok. 1, 3): a send into a tape echo -- a dotted
 * eighth by default, 20 to 30 % feedback, the loop high-passed at 200 Hz and low-passed at 4 to 5 kHz so every repeat is
 * thinner and darker, wow and flutter of a worn machine, thrown up to self-oscillation by the form's automation -- with
 * the springs of a Roland RE-201 on the same send beside it ("dirty preamps, a unique spring reverb, and noisy tape
 * delay", Dok. 3), and a second send into a plate of four seconds (Dattorro). Ephemeris' TapeEcho, Spring and Plate, whose
 * echo times follow the tempo here.
 *
 * **The multiband duck** is Dok. 8.7's: on every kick the band under 200 Hz falls by 8 to 12 dB, 200 Hz to 2 kHz by 2 to
 * 4 dB, the top not at all, for the first 100 to 150 ms. The split is a three-band Linkwitz-Riley crossover (LR4 at 200 Hz
 * and 2 kHz, the low band through the 2 kHz crossover's allpass), whose bands are in phase and sum flat -- the whole an
 * allpass at rest -- so each band takes exactly its gain. (A subtractive split, x minus a low pass, sums to the input
 * exactly but is not in phase: at 60 Hz it took 5.5 of 10 dB.) The gains share one event-driven curve (Ducker.h).
 */
#pragma once
#include "tot/Dsp.h"
#include "tot/fx/Plate.h"
#include "tot/fx/Spring.h"
#include "tot/fx/TapeEcho.h"
#include "tot/mix/Ducker.h"
#include <vector>

namespace tot {

/** @brief The tape echo with its springs, and the plate. */
class DubChain {
public:
    /** @brief Allocates for a sample rate and the longest block process() will see. */
    void prepare(double sampleRate, int maxBlock);
    /** @brief Silence in the echo, the springs and the plate. */
    void reset();
    /**
     * @brief Reads the effective parameter values (indexed by dub::).
     * @param bpm the tempo the echo time follows
     */
    void update(const float* v, double bpm);
    /**
     * @brief Renders the returns of @p n samples (n <= maxBlock) into @p L and @p R (replaced).
     * @param echoL,echoR   the echo send (the springs take it too)
     * @param plateL,plateR the plate send
     */
    void process(const float* echoL, const float* echoR, const float* plateL, const float* plateR, float* L, float* R, int n);

private:
    TapeEcho echo_;   ///< the tape echo
    Spring spring_;   ///< the springs on the echo send
    Plate plate_;   ///< the plate
    std::vector<float> eL_;   ///< the echo's return, left
    std::vector<float> eR_;   ///< the echo's return, right
    std::vector<float> sL_;   ///< the springs' return, left
    std::vector<float> sR_;   ///< the springs' return, right
    std::vector<float> pL_;   ///< the plate's return, left
    std::vector<float> pR_;   ///< the plate's return, right
    float echoGain_ = 0.7f;   ///< the echo's return level
    float springGain_ = 0.35f;   ///< the springs' return level
    float plateGain_ = 2.0f;   ///< the plate's return level
    double sr_ = 48000.0;   ///< the sample rate, Hz
};

/** @brief The event-driven three-band duck. */
class MultibandDucker {
public:
    /** @brief Sets the crossovers (200 Hz, 2.5 kHz) for a sample rate and clears them. */
    void prepare(double sampleRate);
    /** @brief Clears the crossovers and the duck. */
    void reset();
    /** @brief Depths of the low and mid bands in dB, and the curve's hold and release. */
    void set(float lowDb, float midDb, float holdMs, float releaseMs);
    /** @brief A kick. */
    void trigger(double late) { duck_.trigger(late); }
    /** @brief Ducks @p L and @p R in place; with @p gains, keeps each sample's low and mid gain (two a sample). */
    void process(float* L, float* R, int n, float* gains = nullptr);

private:
    /** @brief One channel's crossover: the first and second sections of each LR4, the low band's allpass. */
    struct Bands {
        Svf split1;   ///< the 200 Hz split
        Svf low2;     ///< its low pass again (fourth order)
        Svf high2;    ///< its high pass again
        Svf split2;   ///< the 2.5 kHz split of the high band
        Svf mid2;     ///< its low pass again
        Svf top2;     ///< its high pass again
        Svf ap;       ///< the 2.5 kHz allpass the low band goes through
    };
    /** @brief Splits @p x with @p b and puts it back together with the low band times @p gl and the mid band times @p gm. */
    static float band(Bands& b, float x, float gl, float gm);

public:
    /** @brief A stem's own crossover: the duck is linear in what it ducks, so the stems' ducks sum to the whole's. */
    struct Replica {
        Bands b[2];   ///< the crossovers, per channel
    };
    /** @brief A replica with this ducker's crossover and a clear state. */
    Replica replica() const;
    /** @brief Ducks a stem in place with the gains process() kept. */
    static void apply(Replica& r, const float* gains, float* L, float* R, int n);

private:
    Ducker duck_;   ///< the duck's curve, started by the kicks
    Bands b_[2];   ///< the crossovers, per channel
    float lowDepth_ = 0.7f;   ///< the low band's depth, linear
    float midDepth_ = 0.3f;   ///< the mid band's depth, linear
};

} // namespace tot
