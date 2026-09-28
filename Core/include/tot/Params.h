/**
 * @file Params.h
 * @brief The parameter system: descriptor tables per module, instantiated in blocks.
 *
 * A module declares its parameters once as a table of descriptors and may exist several times -- the twelve
 * lanes of the percussion kit are one module in twelve instances. A parameter's id is the base index of its
 * module instance plus its index in the table; its text key is "<prefix>.<key>" or "<prefix><instance>.<key>"
 * ("compose.bpm", "perc3.decay").
 *
 * From the same tables come the host parameters, OSC addresses, the preset text form, the manual and the
 * `--list` output of tot_render. The composer never writes parameters; it reads a snapshot, and what it plays on
 * a knob it writes into the score as an automation curve (Score.h).
 *
 * Values are stored as std::atomic<float> in their real range (Hz, ms, dB), so the audio thread can read what
 * another thread wrote without a lock.
 *
 * @note The store (ParamStore) is copied from Ephemeris `Core/include/eph/Params.h` at d047d79 (27.09.2026), which
 *       had it from Phosphene; the module tables are Totality's own. Rule kept: a table is only ever appended to,
 *       never reordered, because the indices sit in presets, `.totset` files and the plugin's state.
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
enum class Curve : uint8_t {
    Linear,   ///< proportional
    Log,      ///< logarithmic; minimum must be > 0
    Int,      ///< integer steps, linear
    Choice,   ///< integer index into a list of names
    Toggle,   ///< 0 or 1
};

/** @brief Static description of one parameter. */
struct ParamDesc {
    const char* key;                        ///< identifier inside the module, snake_case
    const char* name;                       ///< display name (English)
    const char* unit;                       ///< unit for display, may be empty
    float minValue;                         ///< lowest real value
    float maxValue;                         ///< highest real value
    float defValue;                         ///< default real value
    Curve curve;                            ///< mapping to the knob
    const char* const* choices = nullptr;   ///< names for Curve::Choice (maxValue + 1 entries)
};

/** @brief The modules that own parameters. Appended to, never reordered. */
enum class Module : int { Compose = 0, Kick, Rumble, Sub, Perc, Mix, Master,
                          /** Phase 2: the ping voices (Ping.h). */
                          Ping,
                          /** Phase 2: the free-running modulation of the whole instrument (Dok. 8.4; Engine.h). */
                          Motion,
                          /** Phase 2: the room the hats, the percussion and the ping are sent into (PLAN 5.9). */
                          Space,
                          /** Phase 3: the bass synth and the 303 line (one table, Synth.h), the dub chord, the drone, the
                           *  texture, the dub chain (tape echo, springs, plate). */
                          Bass, Acid, Chord, Drone, Texture, Dub,
                          /** Phase 3: the granular cloud of the ping and the chord (Cloud.h). */
                          Cloud,
                          /** Phase 4: the DJ mixer -- a channel per deck (three instances), and its effects. */
                          Deck, DjFx,
                          /** Phase 4: the set composer (compose/Set.h). */
                          Set,
                          /** Phase 5: the performer's controls (live only, Engine::setLive), the OSC cues (Cue.h), a
                           *  style of the user's own (the Style tab; compose/Style.h). */
                          Perform, Cue, Custom, Count };

constexpr int kDecks = 3;        ///< the decks of a set (PLAN 3): two for the tracks, a third for the loops

constexpr int kPercLanes = 12;   ///< instances of the percussion module (the lanes of the kit)

/** @brief Parameters of the composer (read as a snapshot when a track is planned). */
namespace compose {
enum : int { Bpm, Key, Scale, Style, Minutes,
             Swing,      ///< MPC swing in per cent (50 = straight), on the layers that swing (PLAN 6.6)
             Humanize,   ///< standard deviation of the random timing, ms (never on the kick)
             LowOwner,   ///< who owns the band under 80 Hz: the rumble or the sub bass (PLAN 5.2)
             // Phase 4: the composer (compose/Composer.h, Style.h).
             Auto,           ///< the composer draws tempo, key, scale and the low end's owner from the style (else the knobs)
             Form,           ///< Auto, Arc, Peak or Endless (PLAN 7.2)
             MorphTo,        ///< Off or a style the profile is morphed towards (PLAN 7.6)
             Morph,          ///< how far, 0..1
             DubShare,       ///< Dok. 8.0's axis: the profile pulled towards Dub, 0..1
             HypnoticShare,  ///< and towards Hypnotic, 0..1
             PickSounds,     ///< the composer chooses a factory preset per synth and track (Presets.h), else the knobs sound
             /** Phase 13: Auto (drawn by the style) or one of the track archetypes (Composer.h: Tool, Roller, Stab,
              *  Acid, Bleep, Dub Chord, Tribal). */
             Archetype,
             UseRatings,     ///< Phase 17: the player's ratings weigh the archetypes and preset groups (Preferences.h)
             Count };
}
/**
 * @brief Parameters of the kick (PLAN 5.1): Phosphene's kick table, then the 909 engine's layer and the EQ.
 */
namespace kick {
enum : int { Engine, Tune, PitchEnd, PitchStart, PitchDecay, PunchDecay, Punch, AmpAttack, AmpHold, AmpDecay,
             Drive, Clip, ClickLevel, ClickTone, ClickDecay, Tone, Level, TailLimit,
             // Totality: the kick's EQ (Dok. 8.4) and the second layer, a 909 top pitched down and high-passed
             // (the Berghain recipe, Dok. 3).
             LowCut, DipFreq, Dip, TopLevel, TopPitch, TopDecay, TopDrive, TopCut, Count };
}
/** @brief Parameters of the rumble (PLAN 5.2): the kick's hall above the split, a sine that continues it below. */
namespace rumble {
enum : int { Level, Split, Decay, Size, Damping, PreDelay, Drive, Ratio, Resonance, Sub, SubAttack, SubRelease,
             Duck, DuckHold, DuckRelease, Count };
}
/** @brief Parameters of the sub bass (PLAN 5.3): a sine, locked to the kick's phase. */
namespace sub {
enum : int { Level, Octave, Attack, Decay, Sustain, Release, LowPass, Drive, Lock, Duck, DuckHold, DuckRelease, Count };
}
/**
 * @brief Parameters of one percussion lane (module Perc, "perc1" .. "perc12"): Phosphene's lane table, then the
 *        noise source's type (the 909's metal table, Kit.h).
 */
namespace perc {
enum : int { Active, Role, Engine, Pitch, PitchAmount, PitchDecay, FmRatio, FmIndex, ModeSet, ModeDamp,
             MetalScale, Noise, NoiseDecay, Bursts, BurstSpacing, Decay, Filter, Cutoff, Resonance, LowCut,
             Drive, Level, Pan, Choke, Shift, Density, Tune, PanDepth, PanBars, CutTrack,
             // Totality: what the noise source plays -- white noise, or the 909's metal table (Kit.h).
             NoiseType, Count };
}
/** @brief Parameters of the mix: the two percussion buses (PLAN 7.3: the perc bus's low pass is a ramp target). */
namespace mix {
enum : int { HatsLevel, HatsCut, PercLevel, PercCut,
             // Phase 3: the drum bus's saturation (Dok. 8.7: three or four stages at 10 to 20 %), the multiband duck of the
             // pads and returns (Dok. 8.7: 20-200 Hz 8-12 dB, 200 Hz-2 kHz 2-4 dB).
             DrumSat, DuckLow, DuckMid,
             // Phase 4: the track's group high pass (Dok. 8.5: "Master-/Gruppen-HP 20 -> 200..400 Hz und zurueck").
             LowCut, Count };
}
/** @brief Parameters of the master (PLAN 8). */
namespace master {
enum : int { Level, Threshold, Ratio, Clip, Ceiling, MonoBelow,
             // Phase 3: a tilt around 1 kHz (PLAN 8.6), the vinyl cut profile (Erg. 3: a dynamic limit on 6-10 kHz, 16 kHz
             // low pass, mono under 150 Hz).
             Tilt, Cut, Count };
}
/**
 * @brief Parameters of a synth voice (modules Bass and Acid, PLAN 5.3): a PolyBLEP oscillator with a sub oscillator, one
 *        of Ephemeris' circuit filters at twice the rate, envelopes, accent and glide, and its strip.
 */
namespace synth {
enum : int { Level, Pan, Wave, PulseWidth, SubOsc, Filter, Cutoff, Resonance, EnvAmount, Decay, Accent, AmpAttack,
             AmpDecay, AmpSustain, AmpRelease, Glide, Drive, KeyTrack, LowCut, HighCut, DubSend, RoomSend, Duck, DuckRelease,
             Count };
}
/** @brief Parameters of the dub chord (PLAN 5.6). */
namespace chord {
enum : int { Level, Width, Detune, Octave, Attack, Decay, Sustain, Release, Bright, EnvAmount, Band, BandMode, BandQ,
             Sweep, SweepRate, Crush, CrushMix, Phaser, PhaserRate, Dip, DubSend, PlateSend, Count };
}
/** @brief Parameters of the drone (PLAN 5.7). */
namespace drone {
enum : int { Level, Octave, Detune, Cutoff, Resonance, Sweep, SweepBars, Attack, Release, PlateSend, RoomSend, Count };
}
/** @brief Parameters of the texture (PLAN 5.7): vinyl crackle, mains hum, eroded noise. */
namespace texture {
enum : int { Level, Crackle, Hum, HumHz, Erosion, Width, Count };
}
/** @brief Parameters of the dub chain (PLAN 5.9): the tape echo with its springs, the plate. */
namespace dub {
enum : int { EchoTime, Feedback, Tone, LowCut, Wow, Flutter, Drive, EchoReturn, Spring, SpringDecay, PlateDecay,
             PlateDamping, PlatePreDelay, PlateLowCut, PlateReturn, PingSend, HatsSend, PercSend, Count };
}
/** @brief Parameters of a DJ mixer channel (PLAN 7.7): fader, the isolator's three bands (kill at -60 dB), a bipolar
 *         filter (below 0 a low pass, above 0 a high pass), the send into the mixer's effects. */
namespace deck {
enum : int { Fader, Low, Mid, High, Filter, FxSend, Count };
}
/** @brief Parameters of the DJ mixer's effects: a tempo echo and a hall on one send. */
namespace djfx {
enum : int { EchoTime, Feedback, EchoReturn, HallDecay, HallReturn, Count };
}
/** @brief Parameters of the set (PLAN 7.1, 7.7): its dramaturgy, whether the styles wander with its energy, how often
 *         a loop of the last track plays on under the next, how often a break comes from the mixer's effects, the
 *         blend's length. */
namespace set {
enum : int { Dramaturgy, Journey, Loops, FxBreaks, BlendBars,
             Minutes,   ///< the plugin's length of a set; 0: a single track (compose.minutes)
             /** Phase 9: a track's own time in a set (swap to swap, its body; about three minutes in the club), and
              *  how busy the DJ's hand is on the channels between the blends. */
             TrackMinutes, DjHand,
             Count };
}
/**
 * @brief The performer's controls (PLAN 10.1, Perform): the master filter (bipolar as a channel's), the echo throw of the
 *        whole mix into the mixer's echo, and a mute per group of parts -- a muted part's notes are not played, its tails
 *        ring out. Live only (Engine::setLive): a render or an export plays the score as it was composed.
 */
namespace perform {
enum : int { Filter, Throw, MuteKick, MuteSub, MuteHats, MutePerc, MutePing, MuteBass, MutePads, Count };
constexpr int kMutes = MutePads - MuteKick + 1;   ///< the groups: kick (and its rumble), sub, hats, perc, ping, bass and 303, pads
}
/** @brief The OSC cues (Cue.h): on or off, and the UDP port. */
namespace cue {
enum : int { Enabled, Port, Count };
}
/** @brief A style of the user's own (the Style tab): with Use, its numbers replace those of the profile the knobs describe
 *         (profileOf); the defaults are Hypnotic's. */
namespace custom {
enum : int { Use, BpmLow, BpmHigh, ArcWeight, PeakWeight, EndlessWeight, SubChance, BlocksLow, BlocksHigh, Mutation, Reroll,
             Polymeter, Fill, Edge, MaxReduction, SwingLow, SwingHigh, EventRate, ThrowShare, DensityCap, Similarity, PeakLufs,
             Count };
}
/** @brief The set's dramaturgies (PLAN 7.1). */
enum class Dramaturgy : int { WarmUp = 0, Peak, Closing, Sunday, Flat,
                              /** Phase 11: Klock's arc ("starts hard, then in 15 minutes eases into a supercruise",
                               *  building again in the last half hour) and a night of many hours. */
                              Cruise, Marathon, Count };
/** @brief Parameters of the granular cloud (PLAN 5.7, Cloud.h). */
namespace cloud {
enum : int { Level, Density, Size, Pitch, Spray, PingSend, ChordSend, PlateSend, Count };
}

/** @brief Parameters of the ping (PLAN 5.5, Ping.h): FM through a low-pass gate, a wandering band pass. */
namespace ping {
enum : int { Level, Pan, Width, Ratio, Index, IndexDecay, PitchAmount, PitchDecay, Decay, Lpg, LpgRelease, Resonance,
             Band, BandQ, BandMix, Sweep, SweepRate, Count };
}

/**
 * @brief Parameters of the global motion (Dok. 8.4, PLAN 5.8): LFOs with incommensurable periods -- 7, 11 and 13 beats,
 *        and a slow filter drift at 0.065 Hz -- on the filters of the hats and of the other lanes (their own, not the
 *        buses' low passes, which the form ramps), the hats' level and decay, and the rumble's drive. Their phases come from the absolute beat and second, so a bar alone moves as the bar in
 *        sequence.
 */
namespace motion {
enum : int { Amount, HatsCut, HatsLevel, HatDecay, PercCut, RumbleDrive, Count };
}

/** @brief Parameters of the room (PLAN 5.9): Ephemeris' FDN hall on a send, its return high-passed (Dok. 8.7: 200-400 Hz). */
namespace space {
enum : int { Level, Size, Decay, Damping, PreDelay, LowCut, HighCut, HatsSend, PercSend, PingSend, Count };
}

/** @brief What a percussion lane plays in the groove; decides its patterns and its MIDI note. */
enum class PercRole : int { ClosedHat = 0, RollingHat, OpenHat, Ride, Clap, ClapGhost, Snare, Rim, Shaker, Tom, Conga,
                            Noise, Count };
constexpr int kNumPercRoles = static_cast<int>(PercRole::Count);   ///< number of roles
/** @brief Sound sources of a percussion lane (Phosphene's five). */
enum class PercEngine : int { Noise = 0, Metal, Modal, Tone, Fm, Count };
/** @brief What the noise source plays. */
enum class NoiseType : int { White = 0, Metal909, Count };
/** @brief The kick's engines: Phosphene's sweep and resonator, and the 909's shaped triangle (Kick.h). */
enum class KickEngine : int { Sweep = 0, Resonator, Tr909, Count };
/** @brief The four style profiles of PLAN 2.8, in the order of compose.style. */
enum class Style : int { Hypnotic = 0, Ostgut, Dub, RawPeak, Count };
/** @brief Who owns the band under 80 Hz (PLAN 5.2). */
enum class LowOwner : int { Rumble = 0, Sub, Count };

extern const char* const kKeyNames[12];          ///< names of compose.key, C .. B
extern const char* const kScaleNames[];          ///< names of compose.scale
extern const char* const kStyleNames[];          ///< names of compose.style
extern const char* const kPercRoleNames[kNumPercRoles];   ///< names of perc.role

/** @brief The scales of compose.scale (PLAN 2.6): Aeolian, Dorian, Phrygian, the Aeolian-Dorian hexachord, minor pentatonic. */
enum class Scale : int { Aeolian = 0, Dorian, Phrygian, Hexachord, MinorPentatonic, Count };
/** @brief One scale: its size and the semitones of its degrees above the root. */
struct ScaleDef {
    int size;       ///< number of degrees (5 .. 7)
    int steps[7];   ///< semitones of the degrees, ascending from 0
};
/** @brief The definition of scale @p scale (compose.scale order); Aeolian for an index out of range. */
const ScaleDef& scaleDef(int scale);
/** @brief Whether @p semitones above the root lies in scale @p scale. */
bool inScale(int scale, int semitones);
/** @brief The kick's tuning rules, in the order of kick.tune (Kick.h, tuneToKey). */
enum class KickTune : int { Free = 0, Key, Fifth, FlatSeventh, Count };

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

    /** @brief Number of parameters. */
    int count() const { return static_cast<int>(entries_.size()); }
    /** @brief First id of a module instance; -1 if it does not exist. */
    int base(Module m, int instance = 0) const;
    /** @brief Id of parameter @p index of a module instance; -1 if it does not exist. */
    int id(Module m, int instance, int index) const { const int b = base(m, instance); return b < 0 ? -1 : b + index; }
    /** @brief Descriptor of @p id. */
    const ParamDesc& desc(int id) const { return *entries_[static_cast<size_t>(id)].desc; }
    /** @brief Full text key of @p id ("perc3.decay"). */
    const std::string& key(int id) const { return entries_[static_cast<size_t>(id)].key; }
    /** @brief The module, the instance and the index within the module of parameter @p id. */
    Module moduleOf(int id) const { return entries_[static_cast<size_t>(id)].module; }
    int instanceOf(int id) const { return entries_[static_cast<size_t>(id)].instance; }   ///< @copydoc moduleOf
    int indexOf(int id) const { const Entry& e = entries_[static_cast<size_t>(id)]; return id - base(e.module, e.instance); }   ///< @copydoc moduleOf
    /** @brief Id for a text key, or -1. */
    int find(std::string_view key) const;

    /** @brief Current real value. */
    float get(int id) const { return values_[static_cast<size_t>(id)].load(std::memory_order_relaxed); }
    /** @brief Current value rounded to an integer (Int, Choice, Toggle). */
    int getInt(int id) const;
    /** @brief Current value as a switch. */
    bool getBool(int id) const { return get(id) >= 0.5f; }
    /** @brief Sets a real value, clamped to the range (and rounded for discrete curves). */
    void set(int id, float value);
    /** @brief Sets from a normalised 0..1 position. */
    void setNormalised(int id, float norm) { set(id, fromNormalised(id, norm)); }

    /** @brief Real value to normalised 0..1. */
    float toNormalised(int id, float value) const;
    /** @brief Normalised 0..1 to real value. */
    float fromNormalised(int id, float norm) const;

    /** @brief All parameters back to their defaults. */
    void resetDefaults();
    /** @brief Default of @p id: the descriptor's, or the instance's own where instances differ (the kit's lanes). */
    float defaultValue(int id) const { return defaults_[static_cast<size_t>(id)]; }
    /** @brief Copies every value from another store (for snapshots on another thread). */
    void copyValuesFrom(const ParamStore& other);
    /**
     * @brief Copies the current values of one module instance into @p out, indexed like its table.
     * @param m        module
     * @param instance instance index
     * @param out      at least as many floats as the module has parameters
     */
    void readModule(Module m, int instance, float* out) const;
    /** @brief Number of parameters of a module. */
    static int moduleCount(Module m);

    /**
     * @brief Applies "key=value" assignments separated by whitespace, newlines or ';'.
     *
     * Choice parameters accept their name ("compose.key=F#") or index. Lines starting with '#' are comments.
     * @param text  the assignments
     * @param error receives a message for the first bad assignment, may be null
     * @return false if any assignment failed (the good ones are still applied)
     */
    bool parseText(std::string_view text, std::string* error = nullptr);
    /**
     * @brief The text form, one "key=value" per line.
     * @param onlyChanged leave out parameters at their default
     */
    std::string toText(bool onlyChanged) const;
    /** @brief A value formatted for display ("330 Hz", "F#"). */
    std::string format(int id) const;

private:
    struct Entry {
        const ParamDesc* desc;
        std::string key;
        Module module;
        int instance;
    };
    std::vector<Entry> entries_;
    std::unique_ptr<std::atomic<float>[]> values_;
    std::vector<float> defaults_;
    std::unordered_map<std::string, int> index_;
    static constexpr int kMaxInstances = 16;
    int bases_[static_cast<int>(Module::Count)][kMaxInstances] = {};
};

} // namespace tot
