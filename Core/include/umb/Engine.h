/**
 * @file Engine.h
 * @brief The engine: plays a score through every voice of a deck, its buses, sends and rooms, and the master (PLAN 3;
 *        in Phases 1 to 3 one deck).
 *
 * **Time** is the sample counter; beats are derived from it through the tempo map (Clock.h), never accumulated.
 *
 * **Determinism across block sizes** (the rule of all three siblings). Parameters and automation are read on an
 * absolute raster of 32 samples, and spans are split exactly at note events, so a host's block size cannot change a
 * single sample (the self test renders with blocks of 1, 37 and 512 and compares bits).
 *
 * **The low end as one system** (PLAN 5.2, 5.3). A kick hands its phase on: the rumble's sub continues the kick's tail,
 * and the sub bass's locked notes start in the kick's phase at their own onset. compose.low_owner decides who owns the
 * band under the split: with Sub the rumble loses its sine and the bass line plays the sub's sine as well as the synth.
 * Whoever owns it, the bass synth and the 303 line are high-passed at 100 Hz or more: under it only kick, rumble and sub.
 *
 * **Signal flow** (PLAN 5.9, 8):
 * @code
 *   kick + rumble + hats bus + perc bus  -> drum bus (three saturation stages)                           -+
 *   sub, ping, bass, 303                                                                                   |
 *   chord bus + drone      -> multiband duck (pads)                                                        +-> mix
 *   texture                                                                                                |
 *   sends: room (hats, perc, ping, bass, 303, drone), dub echo + springs (bass, 303, chord, ping, hats,    |
 *   perc), plate                                                                                           |
 *   (chord, drone, cloud) -> returns -> multiband duck (returns)                                          -+
 *   the cloud: grains of the ping and the chord -> returns, and into the plate                             |
 *   mix -> side mono under Mono Below -> level -> tilt -> glue (35 % parallel) -> the Leveler's trim -> [vinyl cut]
 *       -> clip at 4x -> limiter
 * @endcode
 * Every ducker is triggered by the kick's events, never by its level (Ducker.h).
 *
 * **Loading** allocates and must not run on the audio thread (as in Ephemeris).
 */
#pragma once
#include "umb/Oversample.h"
#include "umb/Params.h"
#include "umb/Score.h"
#include "umb/fx/Cloud.h"
#include "umb/fx/Dub.h"
#include "umb/fx/Dynamics.h"
#include "umb/fx/Reverb.h"
#include "umb/synth/Chord.h"
#include "umb/synth/Drone.h"
#include "umb/synth/Kick.h"
#include "umb/synth/Kit.h"
#include "umb/synth/Ping.h"
#include "umb/synth/Rumble.h"
#include "umb/synth/SubBass.h"
#include "umb/synth/Synth.h"
#include <cstdint>
#include <vector>

namespace umb {

/** @brief Plays a score. Not thread-safe except for parameter writes (ParamStore is atomic). */
class Engine {
public:
    /** @brief The stems, in the order setStems() takes them (before the master). */
    enum Stem : int { kStemKick = 0, kStemRumble, kStemSub, kStemHats, kStemPerc, kStemPing, kStemRoom, kStemBass, kStemAcid,
                      kStemChord, kStemDrone, kStemTexture, kStemDub, kStemCloud, kStems };
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
    /** @brief The loudness corrections of the score playing (a trim per LevelMark), found after it began; they glide in. */
    void setLevelTrims(const std::vector<float>& trims);
    /** @brief The kick (for the tests). */
    const Kick& kick() const { return kick_; }

private:
    /** @brief A note event on the sample grid. */
    struct Ev {
        int64_t sample;   ///< when
        uint8_t on;       ///< 0 off, 1 on (offs first at equal samples)
        uint8_t part;     ///< Part
        bool accent;      ///< the 303's accent
        bool slide;       ///< slides into the next note
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
    /** @brief A voice's sends. */
    struct Sends { float room = 0.0f, dub = 0.0f, plate = 0.0f; };

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
    MonoSynth bass_, acid_;
    ChordSynth chord_;
    Drone drone_;
    Texture texture_;
    int keyRoot_ = 9, scale_ = 0;
    bool subOwns_ = false;
    // The last kick, for the sub's lock.
    bool haveKick_ = false;
    double kickTime_ = 0.0;   ///< its ideal start, in samples
    double kickC_ = 0.0, kickF0_ = 50.0;
    int subNote_ = -1, bassNote_ = -1, acidNote_ = -1, droneNote_ = -1;   ///< the ids of the notes sounding
    double beatsPerSample_ = 0.0;   ///< at the current cell

    // Buses, sends, rooms.
    Svf hatsLp_[2], percLp_[2];
    float hatsGain_ = 1.0f, percGain_ = 1.0f;
    bool laneIsHat_[kPercLanes] = {};
    Reverb room_;
    float roomReturn_ = 1.0f, hatsSend_ = 0.0f, percSend_ = 0.0f, pingSend_ = 0.0f;
    DubChain dub_;
    Sends bassSends_, acidSends_, chordSends_, droneSends_;
    float pingEcho_ = 0.0f, hatsEcho_ = 0.0f, percEcho_ = 0.0f;   ///< the echo's sends from the ping and the kit buses
    float chordLevel_ = 0.25f;
    MultibandDucker padsDuck_, fxDuck_;
    GrainCloud cloud_;
    float cloudPing_ = 0.0f, cloudChord_ = 0.0f, cloudPlate_ = 0.0f;
    float drumSat_ = 0.35f;
    // Master.
    Svf sideHp1_, sideHp2_;
    float masterGain_ = 1.0f, clipDrive_ = 1.0f;
    float trimGain_ = 1.0f, trimTarget_ = 1.0f, trimCoef_ = 0.0f;
    std::vector<float> lateTrims_;   ///< setLevelTrims: the corrections found while playing
    float tiltLow_ = 1.0f, tiltHigh_ = 1.0f, tiltCoef_ = 0.1f, tiltState_[2] = {};
    bool cut_ = false;
    BandLimit cutLp_[2];
    Svf cutHp1_[2], cutHp2_[2];
    float cutEnv_ = 0.0f, cutAtt_ = 0.1f, cutRel_ = 0.001f;
    BusCompressor glue_;
    Oversampler4 clipOs_[2];
    TruePeakLimiter limiter_;

    std::vector<float> kickBuf_, bodyBuf_, rumbleBuf_, subBuf_;
    std::vector<float> pingL_, pingR_, bassL_, bassR_, acidL_, acidR_, chordL_, chordR_, droneL_, droneR_, texL_, texR_;
    std::vector<float> roomInL_, roomInR_, roomL_, roomR_, echoInL_, echoInR_, plateInL_, plateInR_, dubL_, dubR_;
    std::vector<float> drumL_, drumR_, cloudInL_, cloudInR_, cloudL_, cloudR_;
    float* const* stemL_ = nullptr;
    float* const* stemR_ = nullptr;
    int stemOffset_ = 0;   ///< where in the stem buffers the current span begins
};

} // namespace umb
