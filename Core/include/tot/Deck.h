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
    enum StripE : int { KICK = 0, RUMBLE, SUB, HATS, PERC, PING, BASS, ACID, CHORD, DRONE, TEXTURE, ROOM, DUB, CLOUD, STRIPS };
    std::atomic<float> m_peak[STRIPS] = {};   ///< per strip: the loudest sample since the page last took them
    std::atomic<double> m_sum[STRIPS] = {};   ///< per strip: the sum of squares since the page last took them
    /** @brief Adds one block's readings (audio thread). */
    void add(const float* pk, const double* ss, int from, int to)
    {
        for (int s = from; s < to; ++s) {
            float was = m_peak[s].load(std::memory_order_relaxed);
            while (pk[s] > was && !m_peak[s].compare_exchange_weak(was, pk[s], std::memory_order_relaxed)) {}
            double w = m_sum[s].load(std::memory_order_relaxed);
            while (!m_sum[s].compare_exchange_weak(w, w + ss[s], std::memory_order_relaxed)) {}
        }
    }
};

/** @brief One deck. */
class Deck {
public:
    /** @brief The stems, in the order Engine::setStems() takes them. */
    enum StemE : int { STEM_KICK = 0, STEM_RUMBLE, STEM_SUB, STEM_HATS, STEM_PERC, STEM_PING, STEM_ROOM, STEM_BASS, STEM_ACID,
                      STEM_CHORD, STEM_DRONE, STEM_TEXTURE, STEM_DUB, STEM_CLOUD, STEMS };
    static constexpr int Raster = 32;   ///< the engine's parameter raster, samples
    static constexpr int64_t Never = std::numeric_limits<int64_t>::max();   ///< no such sample: nothing is due

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
    /** @brief The sample of the next note not yet played (Never when none). */
    int64_t nextEvent() const { return m_evCursor < m_events.size() ? m_events[m_evCursor].m_sample : Never; }
    /** @brief Whether it plays at @p sample (else it rests). */
    bool playsAt(int64_t sample) const;
    /** @brief The first sample after @p sample where it starts or stops resting (Never when none). */
    int64_t nextRestChange(int64_t sample) const;
    /**
     * @brief Renders @p n samples from @p sample into @p L and @p R (replaced): at most up to the next raster line and the
     *        next note. With @p stemL / @p stemR, also writes its stems there (replaced, from index 0): each through its
     *        own copy of the linear stages (group high pass, tilt, the ducks' crossovers) and times the gains the
     *        nonlinear ones gave the whole (the drum bus, the glue) and the trim, so they sum to L and R.
     */
    void render(int64_t sample, float* L, float* R, int n, float* const* stemL, float* const* stemR);
    /** @brief Live play (Engine::setLive): the performer's mutes act. */
    void setLive(bool on) { m_live = on; }
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
    void setNoteTap(NoteTap* tap) { m_noteTap = tap; }
    /**
     * @brief The family jam (02.10.2026, the plugin's Jam.h): every pitched part's composed notes @p transpose semitones
     *        from where they were written, from the next note on (a note ends with the shift it began with), and with
     *        @p rhythmOut the rhythm's foundation out, as the leader's break has it.
     */
    void setJam(int transpose, bool rhythmOut) { m_jamTranspose = transpose; m_jamMutes = rhythmOut ? JamRhythm : 0u; }
    /** @brief Where the engine keeps what it wrote on the knobs (Engine.h; NaN: never), read by played(). */
    void setShown(const float* shown) { m_shown = shown; }
    /** @brief The beat of the knob settings this deck plays from (Score::knobs), -1 before any. */
    double knobGroup() const { return m_knobGroup; }
    /** @brief Whether a new group of knob settings was taken since the last call (and forgets it). */
    bool takeNewGroup() { const bool n = m_newGroup; m_newGroup = false; return n; }
    /** @brief The value this deck plays parameter @p id from, NaN where it follows the knob. */
    float baseOf(int id) const { return id >= 0 && static_cast<size_t>(id) < m_base.size() ? m_base[static_cast<size_t>(id)] : std::numeric_limits<float>::quiet_NaN(); }
    /** @brief The Quest's quality (Engine::setQuality): the grain cloud rests, the rumble clips at the rate. */
    void setQuest(bool on) { m_quest = on; m_rumble.setOversampling(!on); }
    /** @brief The score it plays. */
    const Score& score() const { return m_score; }
    /** @brief Whether it has a score. */
    bool loaded() const { return m_loaded; }
    /** @brief The value of parameter @p id as this deck plays it: the knob plus its score's automation. */
    float played(int id) const;
    /** @brief Reads a module instance as played. */
    void readPlayed(ModuleE m, int instance, float* out) const;
    /** @brief The samples of its score's steps on the DJ mixer's knobs (a kill, a swap): the engine reads them there. */
    const std::vector<int64_t>& mixerSteps() const { return m_mixerSteps; }
    /** @brief The loudness corrections of its tracks (a trim per LevelMark), found after they began; they glide in. No
     *         allocation for as many trims as the score has marks (reserved at load): the Quest sets them on its audio thread. */
    void setLevelTrims(const std::vector<float>& trims) { m_lateTrims.assign(trims.begin(), trims.end()); }
    /** @brief Phase 18: the parts' corrections of its tracks (one per LevelMark), found after they began; as the trims. */
    void setLevelBalance(const std::vector<BalanceDbT>& bal) { m_lateBal.assign(bal.begin(), bal.end()); }
    /** @brief Phase 18: from now on keeps the loudest sample of the kick and of every part (after its correction and its
     *         bus's level, before the buses' saturation and the sends) -- the Leveler's reading; switched on, it starts
     *         from nothing. */
    void watchPeaks(bool on);
    /** @brief The kick's loudest sample since watchPeaks(true). */
    float kickPeak() const { return m_kickPeak; }
    /** @brief Part @p p's (BalPart, a lane 0 .. 11 first) loudest sample since watchPeaks(true). */
    float partPeak(int p) const { return m_partPeak[p]; }
    /** @brief The kick (for the tests). */
    const Kick& kick() const { return m_kick; }
    /** @brief From now on adds the strips' levels into @p sink (null: stops). */
    void setMeterSink(MeterSink* sink) { m_meter = sink; }

private:
    /** @brief A note event on the sample grid. */
    struct Ev {
        int64_t m_sample;   ///< when
        uint8_t m_on;       ///< 0 off, 1 on (offs first at equal samples)
        uint8_t m_part;     ///< Part
        bool m_accent;      ///< the 303's accent
        bool m_slide;       ///< slides into the next note
        int m_pitch;        ///< MIDI note
        float m_velocity;   ///< 0..1
        int m_shift;        ///< a kit hit's pitch shift
        double m_late;      ///< how many samples ago it ideally happened (0 <= late < 1)
        int m_id;           ///< pairs an off with its on
    };
    /** @brief The automation curves on one parameter, in time order, with a cursor. */
    struct Track {
        int m_param;                      ///< the knob
        std::vector<Gesture> m_gestures;  ///< its curves
        size_t m_cursor = 0;              ///< index of the latest curve that has started, or gestures.size()
        float m_offset = 0.0f;            ///< the offset at the current cell
    };
    /** @brief A voice's sends. */
    struct Sends {
        float m_room = 0.0f;    ///< to the room
        float m_dub = 0.0f;     ///< to the echo
        float m_plate = 0.0f;   ///< to the plate
    };

    /** @brief Plays note event @p e: a voice started or released, past the mutes and the keyboard as they stand. */
    void dispatch(const Ev& e);

    const ParamStore* m_params = nullptr;   ///< the knobs it reads
    int m_index = 0;   ///< which deck it is (0 .. 2)
    double m_sampleRate = 48000.0;   ///< the sample rate, Hz
    bool m_loaded = false;   ///< it has a score
    bool m_live = false;   ///< live play (setLive): the performer's mutes and the keyboard act
    /// The track's absolute knob settings (Score::knobs): the value this deck plays each knob from, NaN where it plays the
    /// knob; a hand's turn of the knob away from what the engine wrote on it (m_shown) moves it from there.
    std::vector<float> m_base;
    size_t m_knobCursor = 0;   ///< index of the next knob setting of the score not yet taken
    double m_knobGroup = -1.0;   ///< the beat of the knob settings it plays from, -1 before any
    bool m_newGroup = false;   ///< a new group of knob settings was taken since takeNewGroup()
    const float* m_shown = nullptr;   ///< what the engine wrote on the knobs (setShown; NaN never), or null
    /** @brief Takes the knob settings due up to @p beat (a group's replaces the last group's). */
    void applyKnobs(double beat);
    bool m_quest = false;   ///< the Quest's quality: the grain cloud rests
    uint32_t m_mutes = 0;   ///< the performer's muted groups (perform::MuteKick ..), bit k for group k
    /** @brief Whether a group is muted -- never for a played key (liveNote). */
    bool muted(int param) const { return !m_liveEvent && (((m_mutes | m_jamMutes) >> (param - perform::MUTE_KICK)) & 1u) != 0; }
    /** @brief The perform mutes the family jam adds in the leader's break (setJam). */
    static constexpr uint32_t JamRhythm = (1u << (perform::MUTE_KICK - perform::MUTE_KICK)) | (1u << (perform::MUTE_SUB - perform::MUTE_KICK)) | (1u << (perform::MUTE_BASS - perform::MUTE_KICK));
    int m_jamTranspose = 0;           ///< the family jam's transposition of the pitched parts (setJam)
    uint32_t m_jamMutes = 0;          ///< the perform mutes the family jam adds now (setJam)
    std::array<std::array<int8_t, 128>, NumParts> m_jamShift{};   ///< the shift each held pitched note began with (dispatch)
    /** @brief Plays @p e past the mutes and the keyboard (dispatch, after the family jam's transposition). */
    void dispatchPlayed(const Ev& e);
    // The keyboard (01.10.2026, liveNote): read from the perform module in live play (updateCell).
    int m_keyTarget = 0;              ///< perform.keyboard_part (perform::keys)
    bool m_keyReplace = true;         ///< perform.keyboard_mode Replace: the played part's generated notes are left out
    bool m_composerOff = false;       ///< perform.composer off: no generated note at all
    uint32_t m_keyPlayed = 0;         ///< by channel: the targets played since the last liveAllOff (bit = target)
    bool m_liveEvent = false;         ///< dispatch() plays a key now: no mute, no silencing
    NoteTap* m_noteTap = nullptr;     ///< where the composer's played notes go for MIDI out (setNoteTap), or null
    /** @brief The perform mute (perform::MuteKick ..) that silences @p part's notes, -1 for none. */
    int muteOf(PartE part) const;
    int m_liveHeld[perform::keys::COUNT] = {};   ///< a mono target's sounding key + 1 (0 none)
    uint8_t m_liveChord[128] = {};    ///< the chord's keys that sound (1) -- released by liveAllOff
    /** @brief Whether the composer's note-ons of @p part are left out (the composer off, or the keyboard replaces it). */
    bool silenced(PartE part) const;
    /** @brief The keyboard target a part belongs to (perform::keys; Off for none). */
    static int targetOf(PartE part);
    Score m_score;   ///< the score it plays
    std::vector<Ev> m_events;   ///< the score's notes on the sample grid, in time order
    size_t m_evCursor = 0;   ///< index of the next event not yet played
    std::vector<Track> m_tracks;   ///< the automation, one track per parameter that has curves
    std::vector<int> m_trackOf;   ///< parameter id -> index into m_tracks, or -1
    std::vector<int64_t> m_mixerSteps;   ///< the samples of its score's steps on the DJ mixer's knobs
    std::vector<std::pair<int64_t, int64_t>> m_plays;   ///< where it plays: [from, to) in samples, sorted

    Kick m_kick;   ///< the kick
    Rumble m_rumble;   ///< the rumble: the kick through a reverb, filtered and ducked
    SubBass m_sub;   ///< the sub bass sine
    PercKit m_kit;   ///< the percussion: twelve lanes
    Ping m_ping;   ///< the ping (FM blips)
    MonoSynth m_bass;   ///< the bass line
    MonoSynth m_acid;   ///< the 303
    ChordSynth m_chord;   ///< the dub chord stabs
    Drone m_drone;   ///< the drone
    Texture m_texture;   ///< the texture: noise and field sounds
    int m_keyRoot = 9;   ///< the key's root, 0 = C (compose.key)
    int m_scale = 0;   ///< the scale (compose.scale)
    bool m_subOwns = false;   ///< the sub bass owns the low end (compose.low_owner): the sine plays the bass line, the rumble leaves the band
    /// The last kick, for the sub's lock.
    bool m_haveKick = false;
    double m_kickTime = 0.0;   ///< its ideal start, in samples
    double m_kickC = 0.0;   ///< the last kick's asymptotic phase, cycles: the sub's lock
    double m_kickF0 = 50.0;   ///< the last kick's tuned end frequency, Hz: the sub's lock
    int m_subNote = -1;   ///< the id of the sub's sounding note, -1 none
    int m_bassNote = -1;   ///< the id of the bass's sounding note, -1 none
    int m_acidNote = -1;   ///< the id of the 303's sounding note, -1 none
    int m_droneNote = -1;   ///< the id of the drone's sounding note, -1 none

    // Buses, sends, rooms.
    Svf m_hatsLp[2];   ///< the hats bus's low pass, per channel
    Svf m_percLp[2];   ///< the percussion bus's low pass, per channel
    float m_hatsGain = 1.0f;   ///< the hats bus's level (mix.hats_level, with its motion)
    float m_percGain = 1.0f;   ///< the percussion bus's level
    bool m_laneIsHat[PercLanes] = {};   ///< per kit lane: it goes to the hats bus (a hat), else to the percussion bus
    Reverb m_room;   ///< the room (space)
    float m_roomReturn = 1.0f;   ///< the room's return level
    float m_hatsSend = 0.0f;   ///< the hats bus's room send
    float m_percSend = 0.0f;   ///< the percussion bus's room send
    float m_pingSend = 0.0f;   ///< the ping's room send
    DubChain m_dub;   ///< the dub chain: tape echo, spring, plate
    Sends m_bassSends;   ///< the bass's sends
    Sends m_acidSends;   ///< the 303's sends
    Sends m_chordSends;   ///< the chord's sends
    Sends m_droneSends;   ///< the drone's sends
    float m_pingEcho = 0.0f;   ///< the ping's echo send
    float m_hatsEcho = 0.0f;   ///< the hats bus's echo send
    float m_percEcho = 0.0f;   ///< the percussion bus's echo send
    float m_chordLevel = 0.25f;   ///< the chord's level, linear (0 at -60 dB)
    MultibandDucker m_padsDuck;   ///< ducks the chord and the drone under the kick, by band
    MultibandDucker m_fxDuck;   ///< ducks the room, the dub chain and the cloud under the kick, by band
    GrainCloud m_cloud;   ///< the grain cloud
    float m_cloudPing = 0.0f;   ///< the ping's send to the cloud
    float m_cloudChord = 0.0f;   ///< the chord's send to the cloud
    float m_cloudPlate = 0.0f;   ///< the cloud's send to the plate
    float m_drumSat = 0.35f;   ///< the drum bus's saturation, 0 .. 1 (mix.drum_sat)
    // The track bus.
    Svf m_groupHp[2][2];              ///< the group high pass (mix.low_cut), fourth order, per channel
    bool m_groupHpOn = false;   ///< the group high pass is in (mix.low_cut above 20.5 Hz)
    float m_tiltHigh = 1.0f;   ///< the master tilt's gain on the highs (master.tilt)
    float m_tiltCoef = 0.1f;   ///< the tilt's one-pole coefficient (1 kHz)
    float m_tiltState[2] = {};   ///< the tilt's low pass, per channel
    BusCompressor m_glue;   ///< the bus compressor on the track
    float m_trimGain = 1.0f;   ///< the loudness trim as it stands, gliding to m_trimTarget
    float m_trimTarget = 1.0f;   ///< the trim of the track that plays now
    float m_trimCoef = 0.0f;   ///< the trim's glide (about a second)
    std::vector<float> m_lateTrims;   ///< setLevelTrims: the corrections found while playing
    // Phase 18: the parts' gains against the kick (LevelMark::balDb), gliding like the trim; applied at the sources, before
    // the sends, as a fader would.
    float m_balGain[BalParts] = {};     ///< (1 from prepare() on)
    float m_balTarget[BalParts] = {};   ///< the parts' gains of the track that plays now
    bool m_watch = false;   ///< the loudest samples are kept (watchPeaks)
    float m_kickPeak = 0.0f;   ///< the kick's loudest sample since watchPeaks(true)
    float m_partPeak[BalParts] = {};   ///< every part's loudest sample since watchPeaks(true)
    MeterSink* m_meter = nullptr;     ///< setMeterSink
    std::vector<BalanceDbT> m_lateBal;   ///< setLevelBalance: the parts' corrections found while playing

    std::vector<float> m_kickBuf;   ///< the kick's click and body, mono
    std::vector<float> m_bodyBuf;   ///< the kick's body alone, mono: the rumble's input
    std::vector<float> m_rumbleBuf;   ///< the rumble, mono
    std::vector<float> m_subBuf;   ///< the sub bass, mono
    std::vector<float> m_pingL;   ///< the ping, left
    std::vector<float> m_pingR;   ///< the ping, right
    std::vector<float> m_bassL;   ///< the bass, left
    std::vector<float> m_bassR;   ///< the bass, right
    std::vector<float> m_acidL;   ///< the 303, left
    std::vector<float> m_acidR;   ///< the 303, right
    std::vector<float> m_chordL;   ///< the chord, left
    std::vector<float> m_chordR;   ///< the chord, right
    std::vector<float> m_droneL;   ///< the drone, left
    std::vector<float> m_droneR;   ///< the drone, right
    std::vector<float> m_texL;   ///< the texture, left
    std::vector<float> m_texR;   ///< the texture, right
    std::vector<float> m_roomInL;   ///< the room's input, left
    std::vector<float> m_roomInR;   ///< the room's input, right
    std::vector<float> m_roomL;   ///< the room's return, left
    std::vector<float> m_roomR;   ///< the room's return, right
    std::vector<float> m_echoInL;   ///< the echo's input, left
    std::vector<float> m_echoInR;   ///< the echo's input, right
    std::vector<float> m_plateInL;   ///< the plate's input, left
    std::vector<float> m_plateInR;   ///< the plate's input, right
    std::vector<float> m_dubL;   ///< the dub chain's return, left
    std::vector<float> m_dubR;   ///< the dub chain's return, right
    std::vector<float> m_drumL;   ///< the drum bus after its saturation, left
    std::vector<float> m_drumR;   ///< the drum bus after its saturation, right
    std::vector<float> m_cloudInL;   ///< the cloud's input, left
    std::vector<float> m_cloudInR;   ///< the cloud's input, right
    std::vector<float> m_cloudL;   ///< the cloud's return, left
    std::vector<float> m_cloudR;   ///< the cloud's return, right

    // The stems' own copies of the linear stages (render() with stems), and the gains of the nonlinear ones.
    /** @brief A stem's own copy of the track bus's linear stages. */
    struct StemBus {
        Svf m_hp[2][2];          ///< the group high pass: two stages per channel
        float m_tilt[2] = {};    ///< the tilt's low pass, per channel
    };
    StemBus m_stemBus[STEMS];   ///< per stem: its copy of the group high pass and the tilt
    MultibandDucker::Replica m_stemPads[2];   ///< the pads duck's crossovers for the stems: chord, drone
    MultibandDucker::Replica m_stemFx[3];     ///< the fx duck's crossovers for the stems: room, dub, cloud
    float m_satGain[2][Raster] = {};     ///< per channel and sample of a raster cell: the drum bus saturation's gain
    float m_padsGains[2 * Raster] = {};  ///< the pads duck's gains: low and mid band, two a sample of a cell
    float m_fxGains[2 * Raster] = {};    ///< the fx duck's gains: low and mid band, two a sample of a cell
    float m_glueGains[Raster] = {};      ///< the glue's gain per sample of a cell
    float m_trimGains[Raster] = {};      ///< the trim per sample of a cell
    /** @brief Clears the stems' copies of the linear stages (a seek, a load). */
    void resetStems();
};

} // namespace tot
