/**
 * @file Deck.h
 * @brief A deck (PLAN 3): the whole instrument for the tracks of one deck -- every voice, the buses, the sends and rooms,
 *        the track bus -- playing its own score with its own automation. The engine holds three and mixes them.
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
 *   chord bus + drone      -> multiband duck (pads)                                                        +-> sum
 *   texture                                                                                                |
 *   sends: room (hats, perc, ping, bass, 303, drone), dub echo + springs (bass, 303, chord, ping, hats,    |
 *   perc), plate (chord, drone, cloud) -> returns (and the cloud) -> multiband duck (returns)             -+
 *   sum -> group high pass -> tilt -> glue (35 % parallel) -> the Leveler's trim  = the deck's output
 * @endcode
 * Every ducker is triggered by the kick's events, never by its level (Ducker.h). The DJ mixer, the master's mono under
 * Mono Below, the clipper and the limiter follow in the engine.
 *
 * **Time and determinism** (the rule of all three siblings). The engine gives every call the absolute sample; parameters
 * and automation are read on its absolute raster of 32 samples and at the samples it names (a mixer's kill), spans end
 * at note events, so a host's block size cannot change a single sample.
 *
 * **Rest.** A deck plays from shortly before its first note to 20 s after its last, and again for each run of tracks on
 * it; between them (while the other deck plays) it rests and costs nothing. Where it rests is fixed by its score
 * (restChanges()), so resting changes no bit either.
 *
 * **Loading** allocates and must not run on the audio thread.
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
#include <limits>
#include <vector>

namespace umb {

/** @brief One deck. */
class Deck {
public:
    /** @brief The stems, in the order Engine::setStems() takes them. */
    enum Stem : int { kStemKick = 0, kStemRumble, kStemSub, kStemHats, kStemPerc, kStemPing, kStemRoom, kStemBass, kStemAcid,
                      kStemChord, kStemDrone, kStemTexture, kStemDub, kStemCloud, kStems };
    static constexpr int kRaster = 32;   ///< the engine's parameter raster, samples
    static constexpr int64_t kNever = std::numeric_limits<int64_t>::max();

    /** @brief Sets the knobs it reads, the rate and which deck it is (0 .. 2). */
    void prepare(const ParamStore* params, double sampleRate, int index);
    /** @brief Plays @p score from now on (allocates). */
    void load(const Score& score);
    /** @brief Plays nothing. */
    void clear();
    /** @brief Silence; the next note and curve at @p sample found again (a jump to 0 plays what is due at it). */
    void seek(int64_t sample);
    /** @brief Reads the knobs and the automation at @p sample. */
    void updateCell(int64_t sample);
    /** @brief Plays every note due at or before @p sample. */
    void dispatchUntil(int64_t sample);
    /** @brief The sample of the next note not yet played (kNever when none). */
    int64_t nextEvent() const { return evCursor_ < events_.size() ? events_[evCursor_].sample : kNever; }
    /** @brief Whether it plays at @p sample (else it rests). */
    bool playsAt(int64_t sample) const;
    /** @brief The first sample after @p sample where it starts or stops resting (kNever when none). */
    int64_t nextRestChange(int64_t sample) const;
    /**
     * @brief Renders @p n samples from @p sample into @p L and @p R (replaced): at most up to the next raster line and the
     *        next note. With @p stemL / @p stemR, also writes its stems there (replaced, from index 0): each through its
     *        own copy of the linear stages (group high pass, tilt, the ducks' crossovers) and times the gains the
     *        nonlinear ones gave the whole (the drum bus, the glue) and the trim, so they sum to L and R.
     */
    void render(int64_t sample, float* L, float* R, int n, float* const* stemL, float* const* stemR);
    /** @brief Live play (Engine::setLive): the performer's mutes act. */
    void setLive(bool on) { live_ = on; }
    /** @brief Where the engine keeps what it wrote on the knobs (Engine.h; NaN: never), read by played(). */
    void setShown(const float* shown) { shown_ = shown; }
    /** @brief The beat of the knob settings this deck plays from (Score::knobs), -1 before any. */
    double knobGroup() const { return knobGroup_; }
    /** @brief Whether a new group of knob settings was taken since the last call (and forgets it). */
    bool takeNewGroup() { const bool n = newGroup_; newGroup_ = false; return n; }
    /** @brief The value this deck plays parameter @p id from, NaN where it follows the knob. */
    float baseOf(int id) const { return id >= 0 && static_cast<size_t>(id) < base_.size() ? base_[static_cast<size_t>(id)] : std::numeric_limits<float>::quiet_NaN(); }
    /** @brief The Quest's quality (Engine::setQuality): the grain cloud rests, the rumble clips at the rate. */
    void setQuest(bool on) { quest_ = on; rumble_.setOversampling(!on); }
    /** @brief The score it plays. */
    const Score& score() const { return score_; }
    /** @brief Whether it has a score. */
    bool loaded() const { return loaded_; }
    /** @brief The value of parameter @p id as this deck plays it: the knob plus its score's automation. */
    float played(int id) const;
    /** @brief Reads a module instance as played. */
    void readPlayed(Module m, int instance, float* out) const;
    /** @brief The samples of its score's steps on the DJ mixer's knobs (a kill, a swap): the engine reads them there. */
    const std::vector<int64_t>& mixerSteps() const { return mixerSteps_; }
    /** @brief The loudness corrections of its tracks (a trim per LevelMark), found after they began; they glide in. No
     *         allocation for as many trims as the score has marks (reserved at load): the Quest sets them on its audio thread. */
    void setLevelTrims(const std::vector<float>& trims) { lateTrims_.assign(trims.begin(), trims.end()); }
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

    const ParamStore* params_ = nullptr;
    int index_ = 0;
    double sampleRate_ = 48000.0;
    bool loaded_ = false;
    bool live_ = false;
    // The track's absolute knob settings (Score::knobs): the value this deck plays each knob from, NaN where it plays the
    // knob; a hand's turn of the knob away from what the engine wrote on it (shown_) moves it from there.
    std::vector<float> base_;
    size_t knobCursor_ = 0;
    double knobGroup_ = -1.0;
    bool newGroup_ = false;
    const float* shown_ = nullptr;
    /** @brief Takes the knob settings due up to @p beat (a group's replaces the last group's). */
    void applyKnobs(double beat);
    bool quest_ = false;
    uint32_t mutes_ = 0;   ///< the performer's muted groups (perform::MuteKick ..), bit k for group k
    bool muted(int param) const { return ((mutes_ >> (param - perform::MuteKick)) & 1u) != 0; }
    Score score_;
    std::vector<Ev> events_;
    size_t evCursor_ = 0;
    std::vector<Track> tracks_;
    std::vector<int> trackOf_;   ///< parameter id -> index into tracks_, or -1
    std::vector<int64_t> mixerSteps_;
    std::vector<std::pair<int64_t, int64_t>> plays_;   ///< where it plays: [from, to) in samples, sorted

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
    // The track bus.
    Svf groupHp_[2][2];              ///< the group high pass (mix.low_cut), fourth order, per channel
    bool groupHpOn_ = false;
    float tiltHigh_ = 1.0f, tiltCoef_ = 0.1f, tiltState_[2] = {};
    BusCompressor glue_;
    float trimGain_ = 1.0f, trimTarget_ = 1.0f, trimCoef_ = 0.0f;
    std::vector<float> lateTrims_;   ///< setLevelTrims: the corrections found while playing

    std::vector<float> kickBuf_, bodyBuf_, rumbleBuf_, subBuf_;
    std::vector<float> pingL_, pingR_, bassL_, bassR_, acidL_, acidR_, chordL_, chordR_, droneL_, droneR_, texL_, texR_;
    std::vector<float> roomInL_, roomInR_, roomL_, roomR_, echoInL_, echoInR_, plateInL_, plateInR_, dubL_, dubR_;
    std::vector<float> drumL_, drumR_, cloudInL_, cloudInR_, cloudL_, cloudR_;

    // The stems' own copies of the linear stages (render() with stems), and the gains of the nonlinear ones.
    struct StemBus { Svf hp[2][2]; float tilt[2] = {}; };
    StemBus stemBus_[kStems];
    MultibandDucker::Replica stemPads_[2], stemFx_[3];   ///< chord, drone; room, dub, cloud
    float satGain_[2][kRaster] = {}, padsGains_[2 * kRaster] = {}, fxGains_[2 * kRaster] = {}, glueGains_[kRaster] = {},
          trimGains_[kRaster] = {};
    void resetStems();
};

} // namespace umb
