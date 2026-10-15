/**
 * @file Params.h
 * @brief The parameter system: descriptor tables per module, instantiated in blocks.
 *
 * A module declares its parameters once as a table of descriptors and may exist several times -- the twelve
 * lanes of the percussion kit are one module in twelve instances. A parameter's id is the base index of its
 * module instance plus its index in the table; its text key is "\<prefix\>.\<key\>" or "\<prefix\>\<instance\>.\<key\>"
 * ("compose.bpm", "perc3.decay").
 *
 * From the same tables come the host parameters, OSC addresses, the preset text form, the manual and the
 * `--list` output of tot_render. The composer never writes parameters; it reads a snapshot, and what it plays on
 * a knob it writes into the score as an automation curve (Score.h).
 *
 * Values are stored as std::atomic\<float\> in their real range (Hz, ms, dB), so the audio thread can read what
 * another thread wrote without a lock.
 *
 * @note The store (ParamStore) is copied from Ephemeris `Core/include/eph/Params.h` at d047d79 (27.09.2026), which
 *       had it from Phosphene; the module tables are Totality's own. Rule kept: a table is only ever appended to,
 *       never reordered, because the indices sit in presets, `.totset` files and the plugin's state.
 * @author Rene Weller
 */
#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tot {

/** @brief How a parameter maps between its real value and the normalised 0..1 of a knob. */
enum class CurveE : uint8_t {
    LINEAR,   ///< proportional
    LOG,      ///< logarithmic; minimum must be > 0
    INT,      ///< integer steps, linear
    CHOICE,   ///< integer index into a list of names
    TOGGLE,   ///< 0 or 1
};

/** @brief Static description of one parameter. */
struct ParamDesc {
    const char* m_key;                        ///< identifier inside the module, snake_case
    const char* m_name;                       ///< display name (English)
    const char* m_unit;                       ///< unit for display, may be empty
    float m_minValue;                         ///< lowest real value
    float m_maxValue;                         ///< highest real value
    float m_defValue;                         ///< default real value
    CurveE m_curve;                            ///< mapping to the knob
    const char* const* m_choices = nullptr;   ///< names for Curve::Choice (maxValue + 1 entries)
};

/** @brief The modules that own parameters. Appended to, never reordered. */
enum class ModuleE : int { COMPOSE = 0, KICK, RUMBLE, SUB, PERC, MIX, MASTER,
                          /** Phase 2: the ping voices (Ping.h). */
                          PING,
                          /** Phase 2: the free-running modulation of the whole instrument (Dok. 8.4; Engine.h). */
                          MOTION,
                          /** Phase 2: the room the hats, the percussion and the ping are sent into (PLAN 5.9). */
                          SPACE,
                          /** Phase 3: the bass synth and the 303 line (one table, Synth.h), the dub chord, the drone, the
                           *  texture, the dub chain (tape echo, springs, plate). */
                          BASS, ACID, CHORD, DRONE, TEXTURE, DUB,
                          /** Phase 3: the granular cloud of the ping and the chord (Cloud.h). */
                          CLOUD,
                          /** Phase 4: the DJ mixer -- a channel per deck (three instances), and its effects. */
                          DECK, DJ_FX,
                          /** Phase 4: the set composer (compose/Set.h). */
                          SET,
                          /** Phase 5: the performer's controls (live only, Engine::setLive), the OSC cues (Cue.h), a
                           *  style of the user's own (the Style tab; compose/Style.h). */
                          PERFORM, CUE, CUSTOM, COUNT };

constexpr int Decks = 3;        ///< the decks of a set (PLAN 3): two for the tracks, a third for the loops

constexpr int PercLanes = 12;   ///< instances of the percussion module (the lanes of the kit)

/** @brief Parameters of the composer (read as a snapshot when a track is planned). */
namespace compose {
enum : int { BPM, KEY, SCALE, STYLE, MINUTES,
             SWING,      ///< MPC swing in per cent (50 = straight), on the layers that swing (PLAN 6.6)
             HUMANIZE,   ///< standard deviation of the random timing, ms (never on the kick)
             LOW_OWNER,   ///< who owns the band under 80 Hz: the rumble or the sub bass (PLAN 5.2)
             // Phase 4: the composer (compose/Composer.h, Style.h).
             AUTO,           ///< the composer draws tempo, key, scale and the low end's owner from the style (else the knobs)
             FORM,           ///< Auto, Arc, Peak or Endless (PLAN 7.2)
             MORPH_TO,        ///< Off or a style the profile is morphed towards (PLAN 7.6)
             MORPH,          ///< how far, 0..1
             DUB_SHARE,       ///< Dok. 8.0's axis: the profile pulled towards Dub, 0..1
             HYPNOTIC_SHARE,  ///< and towards Hypnotic, 0..1
             PICK_SOUNDS,     ///< the composer chooses a factory preset per synth and track (Presets.h), else the knobs sound
             /** Phase 13: Auto (drawn by the style) or one of the track archetypes (Composer.h: Tool, Roller, Stab,
              *  Acid, Bleep, Dub Chord, Tribal). */
             ARCHETYPE,
             USE_RATINGS,     ///< Phase 17: the player's ratings weigh the archetypes and preset groups (Preferences.h)
             COUNT };
}
/**
 * @brief Parameters of the kick (PLAN 5.1): Phosphene's kick table, then the 909 engine's layer and the EQ.
 */
namespace kick {
enum : int { ENGINE, TUNE, PITCH_END, PITCH_START, PITCH_DECAY, PUNCH_DECAY, PUNCH, AMP_ATTACK, AMP_HOLD, AMP_DECAY,
             DRIVE, CLIP, CLICK_LEVEL, CLICK_TONE, CLICK_DECAY, TONE, LEVEL, TAIL_LIMIT,
             // Totality: the kick's EQ (Dok. 8.4) and the second layer, a 909 top pitched down and high-passed
             // (the Berghain recipe, Dok. 3).
             LOW_CUT, DIP_FREQ, DIP, TOP_LEVEL, TOP_PITCH, TOP_DECAY, TOP_DRIVE, TOP_CUT, COUNT };
}
/** @brief Parameters of the rumble (PLAN 5.2): the kick's hall above the split, a sine that continues it below. */
namespace rumble {
enum : int { LEVEL, SPLIT, DECAY, SIZE, DAMPING, PRE_DELAY, DRIVE, RATIO, RESONANCE, SUB, SUB_ATTACK, SUB_RELEASE,
             DUCK, DUCK_HOLD, DUCK_RELEASE, COUNT };
}
/** @brief Parameters of the sub bass (PLAN 5.3): a sine, locked to the kick's phase. */
namespace sub {
enum : int { LEVEL, OCTAVE, ATTACK, DECAY, SUSTAIN, RELEASE, LOW_PASS, DRIVE, LOCK, DUCK, DUCK_HOLD, DUCK_RELEASE, COUNT };
}
/**
 * @brief Parameters of one percussion lane (module Perc, "perc1" .. "perc12"): Phosphene's lane table, then the
 *        noise source's type (the 909's metal table, Kit.h).
 */
namespace perc {
enum : int { ACTIVE, ROLE, ENGINE, PITCH, PITCH_AMOUNT, PITCH_DECAY, FM_RATIO, FM_INDEX, MODE_SET, MODE_DAMP,
             METAL_SCALE, NOISE, NOISE_DECAY, BURSTS, BURST_SPACING, DECAY, FILTER, CUTOFF, RESONANCE, LOW_CUT,
             DRIVE, LEVEL, PAN, CHOKE, SHIFT, DENSITY, TUNE, PAN_DEPTH, PAN_BARS, CUT_TRACK,
             // Totality: what the noise source plays -- white noise, or the 909's metal table (Kit.h).
             NOISE_TYPE, COUNT };
}
/** @brief Parameters of the mix: the two percussion buses (PLAN 7.3: the perc bus's low pass is a ramp target). */
namespace mix {
enum : int { HATS_LEVEL, HATS_CUT, PERC_LEVEL, PERC_CUT,
             // Phase 3: the drum bus's saturation (Dok. 8.7: three or four stages at 10 to 20 %), the multiband duck of the
             // pads and returns (Dok. 8.7: 20-200 Hz 8-12 dB, 200 Hz-2 kHz 2-4 dB).
             DRUM_SAT, DUCK_LOW, DUCK_MID,
             // Phase 4: the track's group high pass (Dok. 8.5: "Master-/Gruppen-HP 20 -> 200..400 Hz und zurueck").
             LOW_CUT, COUNT };
}
/** @brief Parameters of the master (PLAN 8). */
namespace master {
enum : int { LEVEL, THRESHOLD, RATIO, CLIP, CEILING, MONO_BELOW,
             // Phase 3: a tilt around 1 kHz (PLAN 8.6), the vinyl cut profile (Erg. 3: a dynamic limit on 6-10 kHz, 16 kHz
             // low pass, mono under 150 Hz).
             TILT, CUT, COUNT };
}
/**
 * @brief Parameters of a synth voice (modules Bass and Acid, PLAN 5.3): a PolyBLEP oscillator with a sub oscillator, one
 *        of Ephemeris' circuit filters at twice the rate, envelopes, accent and glide, and its strip.
 */
namespace synth {
enum : int { LEVEL, PAN, WAVE, PULSE_WIDTH, SUB_OSC, FILTER, CUTOFF, RESONANCE, ENV_AMOUNT, DECAY, ACCENT, AMP_ATTACK,
             AMP_DECAY, AMP_SUSTAIN, AMP_RELEASE, GLIDE, DRIVE, KEY_TRACK, LOW_CUT, HIGH_CUT, DUB_SEND, ROOM_SEND, DUCK, DUCK_RELEASE,
             COUNT };
}
/** @brief Parameters of the dub chord (PLAN 5.6). */
namespace chord {
enum : int { LEVEL, WIDTH, DETUNE, OCTAVE, ATTACK, DECAY, SUSTAIN, RELEASE, BRIGHT, ENV_AMOUNT, BAND, BAND_MODE, BAND_Q,
             SWEEP, SWEEP_RATE, CRUSH, CRUSH_MIX, PHASER, PHASER_RATE, DIP, DUB_SEND, PLATE_SEND, COUNT };
}
/** @brief Parameters of the drone (PLAN 5.7). */
namespace drone {
enum : int { LEVEL, OCTAVE, DETUNE, CUTOFF, RESONANCE, SWEEP, SWEEP_BARS, ATTACK, RELEASE, PLATE_SEND, ROOM_SEND, COUNT };
}
/** @brief Parameters of the texture (PLAN 5.7): vinyl crackle, mains hum, eroded noise. */
namespace texture {
enum : int { LEVEL, CRACKLE, HUM, HUM_HZ, EROSION, WIDTH, COUNT };
}
/** @brief Parameters of the dub chain (PLAN 5.9): the tape echo with its springs, the plate. */
namespace dub {
enum : int { ECHO_TIME, FEEDBACK, TONE, LOW_CUT, WOW, FLUTTER, DRIVE, ECHO_RETURN, SPRING, SPRING_DECAY, PLATE_DECAY,
             PLATE_DAMPING, PLATE_PRE_DELAY, PLATE_LOW_CUT, PLATE_RETURN, PING_SEND, HATS_SEND, PERC_SEND, COUNT };
}
/** @brief Parameters of a DJ mixer channel (PLAN 7.7): fader, the isolator's three bands (kill at -60 dB), a bipolar
 *         filter (below 0 a low pass, above 0 a high pass), the send into the mixer's effects. */
namespace deck {
enum : int { FADER, LOW, MID, HIGH, FILTER, FX_SEND, COUNT };
}
/** @brief Parameters of the DJ mixer's effects: a tempo echo and a hall on one send. */
namespace djfx {
enum : int { ECHO_TIME, FEEDBACK, ECHO_RETURN, HALL_DECAY, HALL_RETURN, COUNT };
}
/** @brief Parameters of the set (PLAN 7.1, 7.7): its dramaturgy, whether the styles wander with its energy, how often
 *         a loop of the last track plays on under the next, how often a break comes from the mixer's effects, the
 *         blend's length. */
namespace set {
enum : int { DRAMATURGY, JOURNEY, LOOPS, FX_BREAKS, BLEND_BARS,
             MINUTES,   ///< the plugin's length of a set; 0: a single track (compose.minutes)
             /** Phase 9: a track's own time in a set (swap to swap, its body; about three minutes in the club), and
              *  how busy the DJ's hand is on the channels between the blends. */
             TRACK_MINUTES, DJ_HAND,
             COUNT };
}
/**
 * @brief The performer's controls (PLAN 10.1, Perform): the master filter (bipolar as a channel's), the echo throw of the
 *        whole mix into the mixer's echo, and a mute per group of parts -- a muted part's notes are not played, its tails
 *        ring out. Live only (Engine::setLive): a render or an export plays the score as it was composed.
 */
namespace perform {
enum : int { FILTER, THROW, MUTE_KICK, MUTE_SUB, MUTE_HATS, MUTE_PERC, MUTE_PING, MUTE_BASS, MUTE_PADS,
             KEYBOARD_PART,   ///< what a MIDI keyboard plays (perform::keys, 01.10.2026); off: the keys toggle the mutes
             KEYBOARD_MODE,   ///< 0 Replace: the played voice's generated notes are left out; 1 Layer: it plays over them
             COMPOSER,       ///< on: the composer's notes play; off: only what the keyboard plays
             KEYBOARD_LOWER,  ///< the keys below KeyboardSplit play this voice instead (perform::keys; Off: no split; 02.10.2026)
             KEYBOARD_SPLIT,  ///< the split key: C1 (36) .. C5 (84), 12 a step
             KEYBOARD_SCALE,  ///< Scale Lock: a played key goes to the nearest note of the track's key and scale (not the kit)
             KEYBOARD_VELOCITY,   ///< the velocity curve: as played, soft, hard, fixed (frame::shapeVelocity)
             COUNT };
constexpr int Mutes = MUTE_PADS - MUTE_KICK + 1;   ///< the groups: kick (and its rumble), sub, hats, perc, ping, bass and 303, pads
/**
 * @brief The keyboard's targets (perform.keyboard_part, 01.10.2026, Engine::queueLive): the kit -- C1 (36) the kick,
 *        C#1 .. C2 the twelve lanes --, the bass (its sub with it where the sub owns the low end), the 303, the ping, the
 *        chord, the drone, or by channel (1 the kit, 2 the bass, 3 the 303, 4 the ping, 5 the chord, 6 the drone; 10 the
 *        kit as well, General MIDI's drum channel).
 */
namespace keys {
enum : int { OFF, KIT, BASS, ACID, PING, CHORD, DRONE, BY_CHANNEL, COUNT };
}
}
/** @brief The OSC cues (Cue.h): on or off, and the UDP port. */
namespace cue {
enum : int { ENABLED, PORT, COUNT };
}
/** @brief A style of the user's own (the Style tab): with Use, its numbers replace those of the profile the knobs describe
 *         (profileOf); the defaults are Hypnotic's. */
namespace custom {
enum : int { USE, BPM_LOW, BPM_HIGH, ARC_WEIGHT, PEAK_WEIGHT, ENDLESS_WEIGHT, SUB_CHANCE, BLOCKS_LOW, BLOCKS_HIGH, MUTATION, REROLL,
             POLYMETER, FILL, EDGE, MAX_REDUCTION, SWING_LOW, SWING_HIGH, EVENT_RATE, THROW_SHARE, DENSITY_CAP, SIMILARITY, PEAK_LUFS,
             COUNT };
}
/** @brief The set's dramaturgies (PLAN 7.1). */
enum class DramaturgyE : int { WARM_UP = 0, PEAK, CLOSING, SUNDAY, FLAT,
                              /** Phase 11: Klock's arc ("starts hard, then in 15 minutes eases into a supercruise",
                               *  building again in the last half hour) and a night of many hours. */
                              CRUISE, MARATHON, COUNT };
/** @brief Parameters of the granular cloud (PLAN 5.7, Cloud.h). */
namespace cloud {
enum : int { LEVEL, DENSITY, SIZE, PITCH, SPRAY, PING_SEND, CHORD_SEND, PLATE_SEND, COUNT };
}

/** @brief Parameters of the ping (PLAN 5.5, Ping.h): FM through a low-pass gate, a wandering band pass. */
namespace ping {
enum : int { LEVEL, PAN, WIDTH, RATIO, INDEX, INDEX_DECAY, PITCH_AMOUNT, PITCH_DECAY, DECAY, LPG, LPG_RELEASE, RESONANCE,
             BAND, BAND_Q, BAND_MIX, SWEEP, SWEEP_RATE, COUNT };
}

/**
 * @brief Parameters of the global motion (Dok. 8.4, PLAN 5.8): LFOs with incommensurable periods -- 7, 11 and 13 beats,
 *        and a slow filter drift at 0.065 Hz -- on the filters of the hats and of the other lanes (their own, not the
 *        buses' low passes, which the form ramps), the hats' level and decay, and the rumble's drive. Their phases come from the absolute beat and second, so a bar alone moves as the bar in
 *        sequence.
 */
namespace motion {
enum : int { AMOUNT, HATS_CUT, HATS_LEVEL, HAT_DECAY, PERC_CUT, RUMBLE_DRIVE, COUNT };
}

/** @brief Parameters of the room (PLAN 5.9): Ephemeris' FDN hall on a send, its return high-passed (Dok. 8.7: 200-400 Hz). */
namespace space {
enum : int { LEVEL, SIZE, DECAY, DAMPING, PRE_DELAY, LOW_CUT, HIGH_CUT, HATS_SEND, PERC_SEND, PING_SEND, COUNT };
}

/** @brief What a percussion lane plays in the groove; decides its patterns and its MIDI note. */
enum class PercRoleE : int { CLOSED_HAT = 0, ROLLING_HAT, OPEN_HAT, RIDE, CLAP, CLAP_GHOST, SNARE, RIM, SHAKER, TOM, CONGA,
                            NOISE, COUNT };
constexpr int NumPercRoles = static_cast<int>(PercRoleE::COUNT);   ///< number of roles
/** @brief Sound sources of a percussion lane (Phosphene's five). */
enum class PercEngineE : int { NOISE = 0, METAL, MODAL, TONE, FM, COUNT };
/** @brief What the noise source plays. */
enum class NoiseTypeE : int { WHITE = 0, METAL909, COUNT };
/** @brief The kick's engines: Phosphene's sweep and resonator, and the 909's shaped triangle (Kick.h). */
enum class KickEngineE : int { SWEEP = 0, RESONATOR, TR909, COUNT };
/** @brief The four style profiles of PLAN 2.8, in the order of compose.style. */
enum class StyleE : int { HYPNOTIC = 0, OSTGUT, DUB, RAW_PEAK, COUNT };
/** @brief Who owns the band under 80 Hz (PLAN 5.2). */
enum class LowOwnerE : int { RUMBLE = 0, SUB, COUNT };

extern const char* const KeyNames[12];          ///< names of compose.key, C .. B
extern const char* const ScaleNames[];          ///< names of compose.scale
extern const char* const StyleNames[];          ///< names of compose.style
extern const char* const PercRoleNames[NumPercRoles];   ///< names of perc.role

/** @brief The scales of compose.scale (PLAN 2.6): Aeolian, Dorian, Phrygian, the Aeolian-Dorian hexachord, minor pentatonic. */
enum class ScaleE : int { AEOLIAN = 0, DORIAN, PHRYGIAN, HEXACHORD, MINOR_PENTATONIC, COUNT };
/** @brief One scale: its size and the semitones of its degrees above the root. */
struct ScaleDef {
    int m_size;       ///< number of degrees (5 .. 7)
    int m_steps[7];   ///< semitones of the degrees, ascending from 0
};
/**
 * @brief The definition of scale @p scale (compose.scale order); Aeolian for an index out of range.
 * @param[in] scale  the scale
 * @return its definition
 */
const ScaleDef& scaleDef(int scale);
/**
 * @brief Whether @p semitones above the root lies in scale @p scale.
 * @param[in] scale      the scale
 * @param[in] semitones  the interval above the root
 * @return true when the scale has it
 */
bool inScale(int scale, int semitones);
/** @brief The kick's tuning rules, in the order of kick.tune (Kick.h, tuneToKey). */
enum class KickTuneE : int { FREE = 0, KEY, FIFTH, FLAT_SEVENTH, COUNT };

/**
 * @brief All parameter values of one engine, lock-free readable from the audio thread.
 *
 * Construction builds the registry from the module tables. Not copyable (atomics); use copyValuesFrom() for
 * snapshots.
 */
class ParamStore {
public:
    ParamStore();
    ParamStore(const ParamStore&) = delete;
    ParamStore& operator=(const ParamStore&) = delete;

    /**
     * @brief Number of parameters.
     * @return the number of parameters
     */
    int count() const { return static_cast<int>(m_entries.size()); }
    /**
     * @brief First id of a module instance; -1 if it does not exist.
     * @param[in] m         the module
     * @param[in] instance  its instance
     * @return the first id of the module instance, -1 if it does not exist
     */
    int base(ModuleE m, int instance = 0) const;
    /**
     * @brief Id of parameter @p index of a module instance; -1 if it does not exist.
     * @param[in] m         the module
     * @param[in] instance  its instance
     * @param[in] index     the parameter's index within the module
     * @return the parameter's id, -1 if it does not exist
     */
    int id(ModuleE m, int instance, int index) const { const int b = base(m, instance); return b < 0 ? -1 : b + index; }
    /**
     * @brief Descriptor of @p id.
     * @param[in] id  the parameter
     * @return its descriptor
     */
    const ParamDesc& desc(int id) const { return *m_entries[static_cast<size_t>(id)].m_desc; }
    /**
     * @brief Full text key of @p id ("perc3.decay").
     * @param[in] id  the parameter
     * @return its full text key
     */
    const std::string& key(int id) const { return m_entries[static_cast<size_t>(id)].m_key; }
    /**
     * @brief The module, the instance and the index within the module of parameter @p id.
     * @param[in] id  the parameter
     * @return its module
     */
    ModuleE moduleOf(int id) const { return m_entries[static_cast<size_t>(id)].m_module; }
    /**
     * @copybrief moduleOf
     * @param[in] id  the parameter
     * @return its module's instance
     */
    int instanceOf(int id) const { return m_entries[static_cast<size_t>(id)].m_instance; }
    /**
     * @copybrief moduleOf
     * @param[in] id  the parameter
     * @return its index within the module
     */
    int indexOf(int id) const { const Entry& e = m_entries[static_cast<size_t>(id)]; return id - base(e.m_module, e.m_instance); }
    /**
     * @brief Id for a text key, or -1.
     * @param[in] key  the text key
     * @return its id, -1 when there is none
     */
    int find(std::string_view key) const;

    /**
     * @brief Current real value.
     * @param[in] id  the parameter
     * @return its current real value
     */
    float get(int id) const { return m_values[static_cast<size_t>(id)].load(std::memory_order_relaxed); }
    /**
     * @brief Current value rounded to an integer (Int, Choice, Toggle).
     * @param[in] id  the parameter
     * @return its current value rounded to an integer
     */
    int getInt(int id) const;
    /**
     * @brief Current value as a switch.
     * @param[in] id  the parameter
     * @return its current value as a switch
     */
    bool getBool(int id) const { return get(id) >= 0.5f; }
    /**
     * @brief Sets a real value, clamped to the range (and rounded for discrete curves).
     * @param[in] id     the parameter
     * @param[in] value  the real value
     */
    void set(int id, float value);
    /**
     * @brief Sets from a normalised 0..1 position.
     * @param[in] id    the parameter
     * @param[in] norm  the position, 0..1
     */
    void setNormalised(int id, float norm) { set(id, fromNormalised(id, norm)); }

    /**
     * @brief Real value to normalised 0..1.
     * @param[in] id     the parameter
     * @param[in] value  a real value
     * @return its normalised position, 0..1
     */
    float toNormalised(int id, float value) const;
    /**
     * @brief Normalised 0..1 to real value.
     * @param[in] id    the parameter
     * @param[in] norm  a normalised position, 0..1
     * @return the real value there
     */
    float fromNormalised(int id, float norm) const;

    /** @brief All parameters back to their defaults. */
    void resetDefaults();
    /**
     * @brief Default of @p id: the descriptor's, or the instance's own where instances differ (the kit's lanes).
     * @param[in] id  the parameter
     * @return its default value
     */
    float defaultValue(int id) const { return m_defaults[static_cast<size_t>(id)]; }
    /**
     * @brief Copies every value from another store (for snapshots on another thread).
     * @param[in] other  the store whose values are copied
     */
    void copyValuesFrom(const ParamStore& other);
    /**
     * @brief Copies the current values of one module instance into @p out, indexed like its table.
     * @param[in]     m         module
     * @param[in]     instance  instance index
     * @param[in,out] out       at least as many floats as the module has parameters
     */
    void readModule(ModuleE m, int instance, float* out) const;
    /**
     * @brief Number of parameters of a module.
     * @param[in] m  the module
     * @return how many parameters an instance of it has
     */
    static int moduleCount(ModuleE m);

    /**
     * @brief Applies "key=value" assignments separated by whitespace, newlines or ';'.
     *
     * Choice parameters accept their name ("compose.key=F#") or index. Lines starting with '#' are comments.
     * @param[in]  text   the assignments
     * @param[out] error  receives a message for the first bad assignment, may be null
     * @return false if any assignment failed (the good ones are still applied)
     */
    bool parseText(std::string_view text, std::string* error = nullptr);
    /**
     * @brief The text form, one "key=value" per line.
     * @param[in] onlyChanged  leave out parameters at their default
     * @return the text form
     */
    std::string toText(bool onlyChanged) const;
    /**
     * @brief A value formatted for display ("330 Hz", "F#").
     * @param[in] id  the parameter
     * @return its value formatted for display
     */
    std::string format(int id) const;

private:
    /** @brief One parameter of the store: its description, its key, its module and instance. */
    struct Entry {
        const ParamDesc* m_desc;   ///< its description
        std::string m_key;   ///< its key ("kick.decay", "perc3.level")
        ModuleE m_module;   ///< its module
        int m_instance;   ///< its instance
    };
    std::vector<Entry> m_entries;   ///< the parameters, by id
    std::unique_ptr<std::atomic<float>[]> m_values;   ///< their values, real units (atomic: any thread)
    std::vector<float> m_defaults;   ///< their defaults
    std::unordered_map<std::string, int> m_index;   ///< key -> id
    static constexpr int MaxInstances = 16;   ///< the most instances a module can have
    int m_bases[static_cast<int>(ModuleE::COUNT)][MaxInstances] = {};   ///< per module and instance: the id of its first parameter, -1 none
};

} // namespace tot
