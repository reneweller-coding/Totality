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
#include "umb/Dsp.h"
#include "umb/fx/Plate.h"
#include "umb/fx/Spring.h"
#include "umb/fx/TapeEcho.h"
#include "umb/mix/Ducker.h"
#include <vector>

namespace umb {

/** @brief The tape echo with its springs, and the plate. */
class DubChain {
public:
    /** @brief Allocates for a sample rate and the longest block process() will see. */
    void prepare(double sampleRate, int maxBlock);
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
    TapeEcho echo_;
    Spring spring_;
    Plate plate_;
    std::vector<float> eL_, eR_, sL_, sR_, pL_, pR_;
    float echoGain_ = 0.7f, springGain_ = 0.35f, plateGain_ = 2.0f;
    double sr_ = 48000.0;
};

/** @brief The event-driven three-band duck. */
class MultibandDucker {
public:
    void prepare(double sampleRate);
    void reset();
    /** @brief Depths of the low and mid bands in dB, and the curve's hold and release. */
    void set(float lowDb, float midDb, float holdMs, float releaseMs);
    /** @brief A kick. */
    void trigger(double late) { duck_.trigger(late); }
    /** @brief Ducks @p L and @p R in place. */
    void process(float* L, float* R, int n);

private:
    /** @brief One channel's crossover: the first and second sections of each LR4, the low band's allpass. */
    struct Bands { Svf split1, low2, high2, split2, mid2, top2, ap; };
    Ducker duck_;
    Bands b_[2];
    float lowDepth_ = 0.7f, midDepth_ = 0.3f;
};

} // namespace umb
