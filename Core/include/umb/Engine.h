/**
 * @file Engine.h
 * @brief The engine: plays a score through the kick, the rumble, the sub, the kit, the two percussion buses and the
 *        master (PLAN 3; in Phase 1 one deck).
 *
 * **Time** is the sample counter; beats are derived from it through the tempo map (Clock.h), never accumulated.
 *
 * **Determinism across block sizes** (the rule of all three siblings). Parameters and automation are read on an
 * absolute raster of 32 samples, and spans are split exactly at note events, so a host's block size cannot change a
 * single sample (the self test renders with blocks of 1, 37 and 512 and compares bits).
 *
 * **The low end as one system** (PLAN 5.2, 5.3). A kick hands its phase on: at its trigger the engine reads
 * Kick::asymptoticPhase() and gives it to the rumble, whose sub continues the kick's tail, and keeps it for the sub
 * bass, whose locked notes start in the kick's phase at their own onset. compose.low_owner decides who owns the band
 * under the split: with Sub the rumble loses its sine.
 *
 * **Buses.** The kit's lanes go to two buses by role -- hats (closed, rolling, open, ride, shaker) and percussion
 * (the rest) -- each with a level and a low pass, which the form ramps (Dok. 8.5: the perc bus 800 Hz -> open over 32
 * bars). Kick, rumble and sub are mono.
 *
 * **Master** (PLAN 8): the side high-passed under Mono Below (fourth order), the level, a glue compressor, a soft
 * clipper at four times the rate, a true-peak limiter.
 *
 * **Loading** allocates and must not run on the audio thread (as in Ephemeris).
 */
#pragma once
#include "umb/Oversample.h"
#include "umb/Params.h"
#include "umb/Score.h"
#include "umb/fx/Dynamics.h"
#include "umb/fx/Reverb.h"
#include "umb/synth/Kick.h"
#include "umb/synth/Kit.h"
#include "umb/synth/Ping.h"
#include "umb/synth/Rumble.h"
#include "umb/synth/SubBass.h"
#include <cstdint>
#include <vector>

namespace umb {

/** @brief Plays a score. Not thread-safe except for parameter writes (ParamStore is atomic). */
class Engine {
public:
    /** @brief The stems, in the order setStems() takes them: kick, rumble, sub, hats, percussion, ping, the room's return
     *         (before the master). */
    static constexpr int kStems = 7;
    /** @brief Name of stem @p s. */
    static const char* stemName(int s);
    /** @brief The raster the parameters and the automation are read on, in samples. */
    static constexpr int kRaster = 32;

    /** @brief The parameters; any thread may write them. */
    ParamStore& params() { return params_; }
    /** @brief The parameters, read-only. */
    const ParamStore& params() const { return params_; }
    /** @brief Sets the sample rate and the largest block; call before load(). */
    void prepare(double sampleRate, int maxBlock);
    /** @brief Replaces the score (not from the audio thread) and returns to its start. */
    void load(const Score& score);
    /**
     * @brief Renders @p n samples into @p L and @p R (overwritten) and advances the position.
     * @return false once the position has passed the end of the score (the tails still ring)
     */
    bool process(float* L, float* R, int n);
    /** @brief Jumps to @p beat: every voice falls silent, the next note and curve are found again. */
    void seek(double beat);
    /** @brief The score being played. */
    const Score& score() const { return score_; }
    /** @brief Current position in seconds. */
    double seconds() const { return static_cast<double>(sample_) / sampleRate_; }
    /** @brief Current position in beats. */
    double beat() const { return score_.tempo.beatAt(seconds()); }
    /** @brief Length of the score in seconds. */
    double lengthSeconds() const { return score_.tempo.secondsAt(score_.lengthBeats); }
    /** @brief Sample rate set by prepare(). */
    double sampleRate() const { return sampleRate_; }
    /** @brief The value of parameter @p id as the engine plays it: the knob plus the score's automation offset. */
    float played(int id) const;
    /**
     * @brief Stems: from now on every process() call also writes, per stem, what it puts into the mix (before the
     *        master) into @p left[s] and @p right[s], each at least the block long. Null switches them off.
     */
    void setStems(float* const* left, float* const* right) { stemL_ = left; stemR_ = right; }
    /** @brief The kick (for the tests). */
    const Kick& kick() const { return kick_; }

private:
    /** @brief A note event on the sample grid. */
    struct Ev {
        int64_t sample;   ///< when
        uint8_t on;       ///< 0 off, 1 on (offs first at equal samples)
        uint8_t part;     ///< Part
        int pitch;        ///< MIDI note
        float velocity;   ///< 0..1
        int shift;        ///< a kit hit's pitch shift
        double late;      ///< how many samples ago it ideally happened (0 <= late < 1)
        int id;           ///< pairs an off with its on
    };
    /** @brief The automation curves on one parameter, in time order, with a cursor. */
    struct Track {
        int param;                      ///< the knob
        std::vector<Gesture> gestures;  ///< its curves
        size_t cursor = 0;              ///< index of the latest curve that has started, or gestures.size()
        float offset = 0.0f;            ///< the offset at the current cell
    };

    void dispatch(const Ev& e);
    void updateCell();
    void renderSpan(float* L, float* R, int n);
    /** @brief Reads a module instance as played (knob plus automation). */
    void readPlayed(Module m, int instance, float* out) const;

    ParamStore params_;
    Score score_;
    double sampleRate_ = 48000.0;
    int maxBlock_ = 512;
    int64_t sample_ = 0, endSample_ = 0;
    bool cellDirty_ = true;
    std::vector<Ev> events_;
    size_t evCursor_ = 0;
    std::vector<Track> tracks_;
    std::vector<int> trackOf_;   ///< parameter id -> index into tracks_, or -1

    Kick kick_;
    Rumble rumble_;
    SubBass sub_;
    PercKit kit_;
    Ping ping_;
    int keyRoot_ = 9, scale_ = 0;
    bool subOwns_ = false;
    // The last kick, for the sub's lock.
    bool haveKick_ = false;
    double kickTime_ = 0.0;   ///< its ideal start, in samples
    double kickC_ = 0.0, kickF0_ = 50.0;
    int subNote_ = -1;        ///< the id of the sub note sounding

    // Buses and master.
    Svf hatsLp_[2], percLp_[2];
    float hatsGain_ = 1.0f, percGain_ = 1.0f;
    bool laneIsHat_[kPercLanes] = {};
    Svf sideHp1_, sideHp2_;
    Reverb room_;                                   ///< the room (PLAN 5.9)
    float roomReturn_ = 1.0f, hatsSend_ = 0.0f, percSend_ = 0.0f, pingSend_ = 0.0f;
    std::vector<float> sendL_, sendR_, retL_, retR_;
    float masterGain_ = 1.0f, clipDrive_ = 1.0f;
    BusCompressor glue_;
    Oversampler4 clipOs_[2];
    TruePeakLimiter limiter_;

    std::vector<float> kickBuf_, bodyBuf_, rumbleBuf_, subBuf_, pingL_, pingR_;
    float* const* stemL_ = nullptr;
    float* const* stemR_ = nullptr;
    int stemOffset_ = 0;   ///< where in the stem buffers the current span begins
};

} // namespace umb
