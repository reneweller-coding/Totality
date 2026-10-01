/**
 * @file Score.h
 * @brief The score: notes, automation curves, block operations and markers of a track or a set, on a tempo map.
 *
 * The composer writes the score ahead of the audio thread (PLAN 3); the audio render, the MIDI export and the
 * `.totset` file all read the same score. Four kinds of content:
 *
 * - **Notes** of every part, in beats. The kit's lanes are parts of their own, so a lane is a MIDI track and a
 *   stem.
 * - **Automation**: a ramp on a knob -- the perc bus's low pass opened over 32 bars, a delay thrown at the end of a
 *   phrase, the open hat's decay a few per cent longer (PLAN 7.3, 7.4). A curve, not a list of points: start,
 *   length, from, to, shape. Values are **offsets in the knob's normalised range** (-1..1), added to where the user
 *   left the knob; after a curve ends its end value stays until the next curve on the same knob starts. The form
 *   is Ephemeris' gesture, and so is the name.
 * - **Block operations**: what the form did at a block boundary -- a layer added, removed, swapped, a kick-out and
 *   its return (PLAN 7.2) -- so that locking and rerolling can work on a block, and a display can draw the
 *   staircase.
 * - **Markers**: a block, a reduction, a track.
 *
 * @note The gesture curve and the housekeeping are copied from Ephemeris `Core/include/eph/Score.h` at d047d79
 *       (27.09.2026); parts, block operations and the rest are Totality's.
 */
#pragma once
#include "tot/Clock.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace tot {

/** @brief The parts of the score, one MIDI track and one stem each. Appended to, never reordered. */
enum class Part : int { Kick = 0, Sub,
                        Perc1, Perc2, Perc3, Perc4, Perc5, Perc6, Perc7, Perc8, Perc9, Perc10, Perc11, Perc12,
                        /** Phase 2: the ping voices. */
                        Ping,
                        /** Phase 3: the bass synth, the 303 line, the dub chord, the drone, the texture (its notes gate it). */
                        Bass, Acid, Chord, Drone, Texture, Count };
constexpr int kNumParts = static_cast<int>(Part::Count);   ///< number of parts
extern const char* const kPartNames[kNumParts];            ///< "kick", "sub", "perc1" .. "perc12", "ping", "bass", ...
/** @brief Whether a part's notes are one-shots (the kick, the kit, the ping) or held until their end. */
constexpr bool isOneShot(Part p) { return p == Part::Kick || p == Part::Ping || (static_cast<int>(p) >= 2 && static_cast<int>(p) < 14); }
/** @brief The part of kit lane @p lane (0-based). */
constexpr Part percPart(int lane) { return static_cast<Part>(static_cast<int>(Part::Perc1) + lane); }
/** @brief The kit lane of @p part, or -1 if it is not a lane. */
constexpr int laneOf(Part part)
{
    const int i = static_cast<int>(part) - static_cast<int>(Part::Perc1);
    return i >= 0 && i < 12 ? i : -1;
}

/** @brief One note. */
struct NoteEvent {
    double beat = 0.0;        ///< onset in beats (swing and offsets already applied)
    double length = 0.25;     ///< duration in beats
    Part part = Part::Kick;   ///< which part plays it
    int pitch = 36;           ///< MIDI note number (the kit: its General MIDI instrument; the sub: its pitch)
    float velocity = 1.0f;    ///< 0..1
    int shift = 0;            ///< a kit hit's pitch shift in semitones
    bool accent = false;      ///< the 303's accent (Synth.h)
    bool slide = false;       ///< slides into the next note (Synth.h)
};

/** @brief How a curve moves between its two values. */
enum class GestureShape : uint8_t {
    MinimumJerk,   ///< the smooth S of a human reach: 10t^3 - 15t^4 + 6t^5 (Flash and Hogan 1985)
    Linear,        ///< constant speed
    EaseIn,        ///< slow start, t^2
    EaseOut,       ///< fast start, 1 - (1 - t)^2
    Step,          ///< jumps to the end value at the start
};
/** @brief One movement of one knob. */
struct Gesture {
    int param = -1;             ///< parameter id (Params.h)
    double beat = 0.0;          ///< start in beats
    double length = 16.0;       ///< duration in beats (0 behaves like Step)
    float from = 0.0f;          ///< offset at the start, in the knob's normalised range (-1..1)
    float to = 0.0f;            ///< offset at the end
    GestureShape shape = GestureShape::MinimumJerk;   ///< the path between the two
    uint8_t hand = 0;           ///< which hand (PLAN 7.3: at most two at once)
};
/** @brief Position 0..1 along a shape at normalised time @p t (clamped to 0..1). */
double gestureShape(GestureShape shape, double t);
/** @brief Offset of one curve at @p beat: @p from before it, its shape during it, @p to after it. */
float gestureValue(const Gesture& g, double beat);

/** @brief What a block boundary did (PLAN 7.2). */
enum class OpKind : uint8_t { Add, Remove, Swap, Hold, KickOut, Return, Start, End };
/** @brief One operation of the form. */
struct BlockOp {
    double beat = 0.0;          ///< where it takes effect
    OpKind kind = OpKind::Hold; ///< what happens
    int layer = -1;             ///< the layer it concerns (pattern/Rack.h, LayerId), -1 for none
    int other = -1;             ///< Swap: the layer that goes
};
extern const char* const kOpNames[];   ///< "add", "remove", "swap", "hold", "kick out", "return", "start", "end"

/**
 * @brief Phase 18 (28.09.2026): the parts the Leveler sets against the kick, each by its own correction -- the kit's
 *        twelve lanes (0 .. 11, each by its role) and the tonal voices. Their presets differ by 20 dB and more in what
 *        they give, and a mix fitted to the references' spectrum had left them 8 to 10 dB under the research's levels
 *        against the kick (the kick and the rumble filled the mids a stab fills on a record).
 */
enum class BalPart : int { Ping = 12, Bass, Acid, Chord, Drone, Texture, Room, Count };   ///< (the room's return: the guard's)
constexpr int kBalLanes = 12;   ///< the kit's lanes come first
constexpr int kBalParts = static_cast<int>(BalPart::Count);   ///< how many parts the balance corrects
/** @brief A correction per part, dB (0: none). */
using BalanceDb = std::array<float, kBalParts>;
extern const char* const kBalPartNames[kBalParts];   ///< "perc1" .. "perc12", "ping", "bass", "acid", "chord", "drone", "texture", "room"

/**
 * @brief Where a track's loudness is set (Leveler.h, after Ephemeris): its start, where its loudest part begins, what that
 *        part should measure, and the correction found for it -- the master gain from the track's start on.
 */
struct LevelMark {
    double beat = 0.0;          ///< the track's start
    double peakBeat = 0.0;      ///< where its loudest part begins
    float targetLufs = -11.0f;  ///< what that part should measure (the style's, PLAN 8.5)
    float trimDb = 0.0f;        ///< the correction (levelScore); 0 until measured
    BalanceDb balDb{};          ///< Phase 18: the parts' corrections against the kick (levelScore); 0 until measured
    std::array<float, 4> styleMix{};   ///< the track's styles (StyleProfile::styleMix), for the parts' windows; 0: Hypnotic's
    int front = -1;             ///< Phase 20: the kit lane that opens the track (its intro's perc): kept up front by the balance
};

/**
 * @brief A knob a track sets, from a beat on (28.09.2026, after Ephemeris' KnobSet): the sound of a synth as a program
 *        change -- the preset the composer chose for the track (Presets.h) -- and the mix its style sets. Unlike a gesture
 *        it is no offset: the deck that plays the track plays from this value, the knobs show it (Engine.h), and a hand
 *        that turns the knob turns it from there. The knob settings of one beat are a group: a new group on a deck
 *        replaces the last one's.
 */
struct KnobSet {
    double beat = 0.0;    ///< from where it holds (a track's start)
    int param = -1;       ///< the parameter
    float value = 0.0f;   ///< its value, real units
    int kind = 0;         ///< 0 a sound (a preset's knob), 1 a setting of the mix or the composer
};

/** @brief The factory preset the composer chose for a synth of a track (Presets.h), from where it holds. */
struct SoundPick {
    double beat = 0.0;   ///< from where it holds (a track's start)
    int module = 0;      ///< the synth's Module, as an int
    int instance = 0;    ///< which instance (the lane, for the kit)
    int preset = -1;     ///< its index in factoryPresets(module)
};

/** @brief A named position. */
struct Marker {
    double beat = 0.0;   ///< position in beats
    std::string text;    ///< name, e.g. "Intro", "Block 3", "Reduction"
};

/** @brief A score of one track or a whole set. */
struct Score {
    TempoMap tempo;                  ///< the tempo map; every other time is in beats
    uint64_t seed = 1;               ///< the seed the score was composed from
    int keyRoot = 9;                 ///< pitch class of the key (A = 9)
    int scale = 0;                   ///< compose.scale of the track
    double lengthBeats = 0.0;        ///< where the score ends
    std::vector<NoteEvent> notes;    ///< notes, sorted by beat after sort()
    std::vector<Gesture> gestures;   ///< automation, sorted by beat after sort()
    std::vector<BlockOp> ops;        ///< the form's operations, sorted by beat after sort()
    std::vector<Marker> markers;     ///< markers, sorted by beat after sort()
    std::vector<LevelMark> levels;   ///< every track's loudness mark, in beat order (Leveler.h)
    std::vector<KnobSet> knobs;      ///< the knobs every track sets at its start, sorted by beat after sort()
    std::vector<SoundPick> sounds;   ///< the presets the composer chose, sorted by beat after sort()
    /** @brief The value the latest knob setting of @p param at or before @p beat gives it; NaN if none does. */
    float knobAt(int param, double beat) const;
    /** @brief The loudness correction at @p beat in dB: the latest mark's (0 before the first). */
    float trimAt(double beat) const;
    /** @brief The parts' corrections at @p beat, dB: the latest mark's (none before the first). */
    BalanceDb balanceAt(double beat) const;
    /** @brief Sorts every list by beat (stable, so equal beats keep the order they were written in). */
    void sort();
    /** @brief Empties the score and resets the tempo to @p bpm. */
    void clear(double bpm);
    /**
     * @brief The automation offset of @p param at @p beat, in the normalised range.
     *
     * Of the curves on one knob, the latest that has started counts; before the first one the offset is 0. Linear
     * in the number of curves -- for the offline render and the tests; the audio thread keeps a cursor instead.
     */
    float gestureOffset(int param, double beat) const;
};

/**
 * @brief A set (PLAN 7.7): what each deck plays, one after another on it -- the tracks, their automation, the DJ
 *        mixer's moves on the deck's own channel (module deck, instance = the deck) -- all on the set's tempo map
 *        (every deck's Score::tempo is the same), and the set's own markers.
 */
struct SetScore {
    static constexpr int kDecks = 3;     ///< as Params.h kDecks
    Score decks[kDecks];                 ///< deck A, B, C
    double lengthBeats = 0.0;            ///< the set's length
    std::vector<Marker> markers;         ///< tracks, blends, bass swaps
    std::vector<Gesture> gestures;       ///< the set's own automation (the mixer's effects, module djfx)
};

} // namespace tot
