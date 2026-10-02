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
#include "tot/Oversample.h"
#include "tot/Params.h"
#include "tot/NoteTap.h"
#include "tot/Score.h"
#include "tot/fx/Cloud.h"
#include "tot/fx/Dub.h"
#include "tot/fx/Dynamics.h"
#include "tot/fx/Reverb.h"
#include "tot/synth/Chord.h"
#include "tot/synth/Drone.h"
#include "tot/synth/Kick.h"
#include "tot/synth/Kit.h"
#include "tot/synth/Ping.h"
#include "tot/synth/Rumble.h"
#include "tot/synth/SubBass.h"
#include "tot/synth/Synth.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <vector>

namespace tot {

/**
 * @brief Where the decks add the levels of the mixer's strips while the plugin's mixer page looks (01.10.2026): per strip
 *        the loudest sample and the sum of squares since the page last took them. Written by the audio thread (both decks
 *        into one), read and reset by the message thread; null in a deck: nothing is measured, nothing costs.
 */
struct MeterSink {
    /** @brief The mixer page's strips, in its order. */
    enum Strip : int { Kick = 0, Rumble, Sub, Hats, Perc, Ping, Bass, Acid, Chord, Drone, Texture, Room, Dub, Cloud, kStrips };
    std::atomic<float> peak[kStrips] = {};   ///< per strip: the loudest sample since the page last took them
    std::atomic<double> sum[kStrips] = {};   ///< per strip: the sum of squares since the page last took them
    /** @brief Adds one block's readings (audio thread). */
    void add(const float* pk, const double* ss, int from, int to)
    {
        for (int s = from; s < to; ++s) {
            float was = peak[s].load(std::memory_order_relaxed);
            while (pk[s] > was && !peak[s].compare_exchange_weak(was, pk[s], std::memory_order_relaxed)) {}
            double w = sum[s].load(std::memory_order_relaxed);
            while (!sum[s].compare_exchange_weak(w, w + ss[s], std::memory_order_relaxed)) {}
        }
    }
};

/** @brief One deck. */
class Deck {
public:
    /** @brief The stems, in the order Engine::setStems() takes them. */
    enum Stem : int { kStemKick = 0, kStemRumble, kStemSub, kStemHats, kStemPerc, kStemPing, kStemRoom, kStemBass, kStemAcid,
                      kStemChord, kStemDrone, kStemTexture, kStemDub, kStemCloud, kStems };
    static constexpr int kRaster = 32;   ///< the engine's parameter raster, samples
    static constexpr int64_t kNever = std::numeric_limits<int64_t>::max();   ///< no such sample: nothing is due

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
    /**
     * @brief A key of a MIDI keyboard (01.10.2026, Engine::queueLive), played at @p sample on this deck with its sound as
     *        its knobs have it -- past the mutes, which are the composer's. @p target is a perform::keys value (Kit ..
     *        Drone); @p on false releases the key. A mono voice (the bass, the 303, the drone) slides to a key pressed
     *        while another is held, and its release ends the note only when it is the sounding key.
     */
    void liveNote(int64_t sample, int target, int pitch, float velocity, bool on);
    /** @brief Releases every played key (a stop, another keyboard target), and forgets which parts were played. */
    void liveAllOff();
    /** @brief From now on every composer note it plays is also written to @p tap (null: stops; NoteTap.h, MIDI out). */
    void setNoteTap(NoteTap* tap) { noteTap_ = tap; }
    /**
     * @brief The family jam (02.10.2026, the plugin's Jam.h): every pitched part's composed notes @p transpose semitones
     *        from where they were written, from the next note on (a note ends with the shift it began with), and with
     *        @p rhythmOut the rhythm's foundation out, as the leader's break has it.
     */
    void setJam(int transpose, bool rhythmOut) { jamTranspose_ = transpose; jamMutes_ = rhythmOut ? kJamRhythm : 0u; }
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
    /** @brief Phase 18: the parts' corrections of its tracks (one per LevelMark), found after they began; as the trims. */
    void setLevelBalance(const std::vector<BalanceDb>& bal) { lateBal_.assign(bal.begin(), bal.end()); }
    /** @brief Phase 18: from now on keeps the loudest sample of the kick and of every part (after its correction and its
     *         bus's level, before the buses' saturation and the sends) -- the Leveler's reading; switched on, it starts
     *         from nothing. */
    void watchPeaks(bool on);
    /** @brief The kick's loudest sample since watchPeaks(true). */
    float kickPeak() const { return kickPeak_; }
    /** @brief Part @p p's (BalPart, a lane 0 .. 11 first) loudest sample since watchPeaks(true). */
    float partPeak(int p) const { return partPeak_[p]; }
    /** @brief The kick (for the tests). */
    const Kick& kick() const { return kick_; }
    /** @brief From now on adds the strips' levels into @p sink (null: stops). */
    void setMeterSink(MeterSink* sink) { meter_ = sink; }

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
    struct Sends {
        float room = 0.0f;    ///< to the room
        float dub = 0.0f;     ///< to the echo
        float plate = 0.0f;   ///< to the plate
    };

    /** @brief Plays note event @p e: a voice started or released, past the mutes and the keyboard as they stand. */
    void dispatch(const Ev& e);

    const ParamStore* params_ = nullptr;   ///< the knobs it reads
    int index_ = 0;   ///< which deck it is (0 .. 2)
    double sampleRate_ = 48000.0;   ///< the sample rate, Hz
    bool loaded_ = false;   ///< it has a score
    bool live_ = false;   ///< live play (setLive): the performer's mutes and the keyboard act
    /// The track's absolute knob settings (Score::knobs): the value this deck plays each knob from, NaN where it plays the
    /// knob; a hand's turn of the knob away from what the engine wrote on it (shown_) moves it from there.
    std::vector<float> base_;
    size_t knobCursor_ = 0;   ///< index of the next knob setting of the score not yet taken
    double knobGroup_ = -1.0;   ///< the beat of the knob settings it plays from, -1 before any
    bool newGroup_ = false;   ///< a new group of knob settings was taken since takeNewGroup()
    const float* shown_ = nullptr;   ///< what the engine wrote on the knobs (setShown; NaN never), or null
    /** @brief Takes the knob settings due up to @p beat (a group's replaces the last group's). */
    void applyKnobs(double beat);
    bool quest_ = false;   ///< the Quest's quality: the grain cloud rests
    uint32_t mutes_ = 0;   ///< the performer's muted groups (perform::MuteKick ..), bit k for group k
    /** @brief Whether a group is muted -- never for a played key (liveNote). */
    bool muted(int param) const { return !liveEvent_ && (((mutes_ | jamMutes_) >> (param - perform::MuteKick)) & 1u) != 0; }
    /** @brief The perform mutes the family jam adds in the leader's break (setJam). */
    static constexpr uint32_t kJamRhythm = (1u << (perform::MuteKick - perform::MuteKick)) | (1u << (perform::MuteSub - perform::MuteKick)) | (1u << (perform::MuteBass - perform::MuteKick));
    int jamTranspose_ = 0;           ///< the family jam's transposition of the pitched parts (setJam)
    uint32_t jamMutes_ = 0;          ///< the perform mutes the family jam adds now (setJam)
    std::array<std::array<int8_t, 128>, kNumParts> jamShift_{};   ///< the shift each held pitched note began with (dispatch)
    /** @brief Plays @p e past the mutes and the keyboard (dispatch, after the family jam's transposition). */
    void dispatchPlayed(const Ev& e);
    // The keyboard (01.10.2026, liveNote): read from the perform module in live play (updateCell).
    int keyTarget_ = 0;              ///< perform.keyboard_part (perform::keys)
    bool keyReplace_ = true;         ///< perform.keyboard_mode Replace: the played part's generated notes are left out
    bool composerOff_ = false;       ///< perform.composer off: no generated note at all
    uint32_t keyPlayed_ = 0;         ///< by channel: the targets played since the last liveAllOff (bit = target)
    bool liveEvent_ = false;         ///< dispatch() plays a key now: no mute, no silencing
    NoteTap* noteTap_ = nullptr;     ///< where the composer's played notes go for MIDI out (setNoteTap), or null
    /** @brief The perform mute (perform::MuteKick ..) that silences @p part's notes, -1 for none. */
    int muteOf(Part part) const;
    int liveHeld_[perform::keys::Count] = {};   ///< a mono target's sounding key + 1 (0 none)
    uint8_t liveChord_[128] = {};    ///< the chord's keys that sound (1) -- released by liveAllOff
    /** @brief Whether the composer's note-ons of @p part are left out (the composer off, or the keyboard replaces it). */
    bool silenced(Part part) const;
    /** @brief The keyboard target a part belongs to (perform::keys; Off for none). */
    static int targetOf(Part part);
    Score score_;   ///< the score it plays
    std::vector<Ev> events_;   ///< the score's notes on the sample grid, in time order
    size_t evCursor_ = 0;   ///< index of the next event not yet played
    std::vector<Track> tracks_;   ///< the automation, one track per parameter that has curves
    std::vector<int> trackOf_;   ///< parameter id -> index into tracks_, or -1
    std::vector<int64_t> mixerSteps_;   ///< the samples of its score's steps on the DJ mixer's knobs
    std::vector<std::pair<int64_t, int64_t>> plays_;   ///< where it plays: [from, to) in samples, sorted

    Kick kick_;   ///< the kick
    Rumble rumble_;   ///< the rumble: the kick through a reverb, filtered and ducked
    SubBass sub_;   ///< the sub bass sine
    PercKit kit_;   ///< the percussion: twelve lanes
    Ping ping_;   ///< the ping (FM blips)
    MonoSynth bass_;   ///< the bass line
    MonoSynth acid_;   ///< the 303
    ChordSynth chord_;   ///< the dub chord stabs
    Drone drone_;   ///< the drone
    Texture texture_;   ///< the texture: noise and field sounds
    int keyRoot_ = 9;   ///< the key's root, 0 = C (compose.key)
    int scale_ = 0;   ///< the scale (compose.scale)
    bool subOwns_ = false;   ///< the sub bass owns the low end (compose.low_owner): the sine plays the bass line, the rumble leaves the band
    /// The last kick, for the sub's lock.
    bool haveKick_ = false;
    double kickTime_ = 0.0;   ///< its ideal start, in samples
    double kickC_ = 0.0;   ///< the last kick's asymptotic phase, cycles: the sub's lock
    double kickF0_ = 50.0;   ///< the last kick's tuned end frequency, Hz: the sub's lock
    int subNote_ = -1;   ///< the id of the sub's sounding note, -1 none
    int bassNote_ = -1;   ///< the id of the bass's sounding note, -1 none
    int acidNote_ = -1;   ///< the id of the 303's sounding note, -1 none
    int droneNote_ = -1;   ///< the id of the drone's sounding note, -1 none

    // Buses, sends, rooms.
    Svf hatsLp_[2];   ///< the hats bus's low pass, per channel
    Svf percLp_[2];   ///< the percussion bus's low pass, per channel
    float hatsGain_ = 1.0f;   ///< the hats bus's level (mix.hats_level, with its motion)
    float percGain_ = 1.0f;   ///< the percussion bus's level
    bool laneIsHat_[kPercLanes] = {};   ///< per kit lane: it goes to the hats bus (a hat), else to the percussion bus
    Reverb room_;   ///< the room (space)
    float roomReturn_ = 1.0f;   ///< the room's return level
    float hatsSend_ = 0.0f;   ///< the hats bus's room send
    float percSend_ = 0.0f;   ///< the percussion bus's room send
    float pingSend_ = 0.0f;   ///< the ping's room send
    DubChain dub_;   ///< the dub chain: tape echo, spring, plate
    Sends bassSends_;   ///< the bass's sends
    Sends acidSends_;   ///< the 303's sends
    Sends chordSends_;   ///< the chord's sends
    Sends droneSends_;   ///< the drone's sends
    float pingEcho_ = 0.0f;   ///< the ping's echo send
    float hatsEcho_ = 0.0f;   ///< the hats bus's echo send
    float percEcho_ = 0.0f;   ///< the percussion bus's echo send
    float chordLevel_ = 0.25f;   ///< the chord's level, linear (0 at -60 dB)
    MultibandDucker padsDuck_;   ///< ducks the chord and the drone under the kick, by band
    MultibandDucker fxDuck_;   ///< ducks the room, the dub chain and the cloud under the kick, by band
    GrainCloud cloud_;   ///< the grain cloud
    float cloudPing_ = 0.0f;   ///< the ping's send to the cloud
    float cloudChord_ = 0.0f;   ///< the chord's send to the cloud
    float cloudPlate_ = 0.0f;   ///< the cloud's send to the plate
    float drumSat_ = 0.35f;   ///< the drum bus's saturation, 0 .. 1 (mix.drum_sat)
    // The track bus.
    Svf groupHp_[2][2];              ///< the group high pass (mix.low_cut), fourth order, per channel
    bool groupHpOn_ = false;   ///< the group high pass is in (mix.low_cut above 20.5 Hz)
    float tiltHigh_ = 1.0f;   ///< the master tilt's gain on the highs (master.tilt)
    float tiltCoef_ = 0.1f;   ///< the tilt's one-pole coefficient (1 kHz)
    float tiltState_[2] = {};   ///< the tilt's low pass, per channel
    BusCompressor glue_;   ///< the bus compressor on the track
    float trimGain_ = 1.0f;   ///< the loudness trim as it stands, gliding to trimTarget_
    float trimTarget_ = 1.0f;   ///< the trim of the track that plays now
    float trimCoef_ = 0.0f;   ///< the trim's glide (about a second)
    std::vector<float> lateTrims_;   ///< setLevelTrims: the corrections found while playing
    // Phase 18: the parts' gains against the kick (LevelMark::balDb), gliding like the trim; applied at the sources, before
    // the sends, as a fader would.
    float balGain_[kBalParts] = {};     ///< (1 from prepare() on)
    float balTarget_[kBalParts] = {};   ///< the parts' gains of the track that plays now
    bool watch_ = false;   ///< the loudest samples are kept (watchPeaks)
    float kickPeak_ = 0.0f;   ///< the kick's loudest sample since watchPeaks(true)
    float partPeak_[kBalParts] = {};   ///< every part's loudest sample since watchPeaks(true)
    MeterSink* meter_ = nullptr;     ///< setMeterSink
    std::vector<BalanceDb> lateBal_;   ///< setLevelBalance: the parts' corrections found while playing

    std::vector<float> kickBuf_;   ///< the kick's click and body, mono
    std::vector<float> bodyBuf_;   ///< the kick's body alone, mono: the rumble's input
    std::vector<float> rumbleBuf_;   ///< the rumble, mono
    std::vector<float> subBuf_;   ///< the sub bass, mono
    std::vector<float> pingL_;   ///< the ping, left
    std::vector<float> pingR_;   ///< the ping, right
    std::vector<float> bassL_;   ///< the bass, left
    std::vector<float> bassR_;   ///< the bass, right
    std::vector<float> acidL_;   ///< the 303, left
    std::vector<float> acidR_;   ///< the 303, right
    std::vector<float> chordL_;   ///< the chord, left
    std::vector<float> chordR_;   ///< the chord, right
    std::vector<float> droneL_;   ///< the drone, left
    std::vector<float> droneR_;   ///< the drone, right
    std::vector<float> texL_;   ///< the texture, left
    std::vector<float> texR_;   ///< the texture, right
    std::vector<float> roomInL_;   ///< the room's input, left
    std::vector<float> roomInR_;   ///< the room's input, right
    std::vector<float> roomL_;   ///< the room's return, left
    std::vector<float> roomR_;   ///< the room's return, right
    std::vector<float> echoInL_;   ///< the echo's input, left
    std::vector<float> echoInR_;   ///< the echo's input, right
    std::vector<float> plateInL_;   ///< the plate's input, left
    std::vector<float> plateInR_;   ///< the plate's input, right
    std::vector<float> dubL_;   ///< the dub chain's return, left
    std::vector<float> dubR_;   ///< the dub chain's return, right
    std::vector<float> drumL_;   ///< the drum bus after its saturation, left
    std::vector<float> drumR_;   ///< the drum bus after its saturation, right
    std::vector<float> cloudInL_;   ///< the cloud's input, left
    std::vector<float> cloudInR_;   ///< the cloud's input, right
    std::vector<float> cloudL_;   ///< the cloud's return, left
    std::vector<float> cloudR_;   ///< the cloud's return, right

    // The stems' own copies of the linear stages (render() with stems), and the gains of the nonlinear ones.
    /** @brief A stem's own copy of the track bus's linear stages. */
    struct StemBus {
        Svf hp[2][2];          ///< the group high pass: two stages per channel
        float tilt[2] = {};    ///< the tilt's low pass, per channel
    };
    StemBus stemBus_[kStems];   ///< per stem: its copy of the group high pass and the tilt
    MultibandDucker::Replica stemPads_[2];   ///< the pads duck's crossovers for the stems: chord, drone
    MultibandDucker::Replica stemFx_[3];     ///< the fx duck's crossovers for the stems: room, dub, cloud
    float satGain_[2][kRaster] = {};     ///< per channel and sample of a raster cell: the drum bus saturation's gain
    float padsGains_[2 * kRaster] = {};  ///< the pads duck's gains: low and mid band, two a sample of a cell
    float fxGains_[2 * kRaster] = {};    ///< the fx duck's gains: low and mid band, two a sample of a cell
    float glueGains_[kRaster] = {};      ///< the glue's gain per sample of a cell
    float trimGains_[kRaster] = {};      ///< the trim per sample of a cell
    /** @brief Clears the stems' copies of the linear stages (a seek, a load). */
    void resetStems();
};

} // namespace tot
