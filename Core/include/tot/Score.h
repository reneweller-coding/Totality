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
 * @author Rene Weller
 */
#pragma once
#include "tot/Clock.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace tot
{

/** @brief The parts of the score, one MIDI track and one stem each. Appended to, never reordered. */
enum class PartE : int
{
    KICK = 0,
    SUB,
    PERC1,
    PERC2,
    PERC3,
    PERC4,
    PERC5,
    PERC6,
    PERC7,
    PERC8,
    PERC9,
    PERC10,
    PERC11,
    PERC12,
    /** Phase 2: the ping voices. */
    PING,
    /** Phase 3: the bass synth, the 303 line, the dub chord, the drone, the texture (its notes gate it). */
    BASS,
    ACID,
    CHORD,
    DRONE,
    TEXTURE,
    COUNT
};
constexpr int            NumParts = static_cast<int>( PartE::COUNT );   ///< number of parts
extern const char *const PartNames[NumParts];   ///< "kick", "sub", "perc1" .. "perc12", "ping", "bass", ...
/**
 * @brief Whether a part's notes are one-shots (the kick, the kit, the ping) or held until their end.
 * @param[in] p  the part
 * @return true when its notes are one-shots
 */
constexpr bool isOneShot( PartE p )
{
    return p == PartE::KICK || p == PartE::PING || ( static_cast<int>( p ) >= 2 && static_cast<int>( p ) < 14 );
}
/**
 * @brief The part of kit lane @p lane (0-based).
 * @param[in] lane  the kit lane, 0-based
 * @return its part
 */
constexpr PartE percPart( int lane )
{
    return static_cast<PartE>( static_cast<int>( PartE::PERC1 ) + lane );
}
/**
 * @brief The kit lane of @p part, or -1 if it is not a lane.
 * @param[in] part  the part
 * @return its kit lane, -1 if it is not a lane
 */
constexpr int laneOf( PartE part )
{
    const int i = static_cast<int>( part ) - static_cast<int>( PartE::PERC1 );
    return i >= 0 && i < 12 ? i : -1;
}

/** @brief One note. */
struct NoteEvent
{
    double m_beat = 0.0;           ///< onset in beats (swing and offsets already applied)
    double m_length = 0.25;        ///< duration in beats
    PartE  m_part = PartE::KICK;   ///< which part plays it
    int    m_pitch = 36;           ///< MIDI note number (the kit: its General MIDI instrument; the sub: its pitch)
    float  m_velocity = 1.0f;      ///< 0..1
    int    m_shift = 0;            ///< a kit hit's pitch shift in semitones
    bool   m_accent = false;       ///< the 303's accent (Synth.h)
    bool   m_slide = false;        ///< slides into the next note (Synth.h)
};

/** @brief How a curve moves between its two values. */
enum class GestureShapeE : uint8_t
{
    MINIMUM_JERK,   ///< the smooth S of a human reach: 10t^3 - 15t^4 + 6t^5 (Flash and Hogan 1985)
    LINEAR,         ///< constant speed
    EASE_IN,        ///< slow start, t^2
    EASE_OUT,       ///< fast start, 1 - (1 - t)^2
    STEP,           ///< jumps to the end value at the start
};
/** @brief One movement of one knob. */
struct Gesture
{
    int           m_param = -1;      ///< parameter id (Params.h)
    double        m_beat = 0.0;      ///< start in beats
    double        m_length = 16.0;   ///< duration in beats (0 behaves like Step)
    float         m_from = 0.0f;     ///< offset at the start, in the knob's normalised range (-1..1)
    float         m_to = 0.0f;       ///< offset at the end
    GestureShapeE m_shape = GestureShapeE::MINIMUM_JERK;   ///< the path between the two
    uint8_t       m_hand = 0;                              ///< which hand (PLAN 7.3: at most two at once)
};
/**
 * @brief Position 0..1 along a shape at normalised time @p t (clamped to 0..1).
 * @param[in] shape  the shape
 * @param[in] t      the position in the gesture, 0..1
 * @return the shape's value, 0..1
 */
double gestureShape( GestureShapeE shape, double t );
/**
 * @brief Offset of one curve at @p beat: @p from before it, its shape during it, @p to after it.
 * @param[in] g     the gesture
 * @param[in] beat  the position in beats
 * @return its offset there
 */
float gestureValue( const Gesture &g, double beat );

/** @brief What a block boundary did (PLAN 7.2). */
enum class OpKindE : uint8_t
{
    ADD,
    REMOVE,
    SWAP,
    HOLD,
    KICK_OUT,
    RETURN,
    START,
    END
};
/** @brief One operation of the form. */
struct BlockOp
{
    double  m_beat = 0.0;             ///< where it takes effect
    OpKindE m_kind = OpKindE::HOLD;   ///< what happens
    int     m_layer = -1;             ///< the layer it concerns (pattern/Rack.h, LayerId), -1 for none
    int     m_other = -1;             ///< Swap: the layer that goes
};
extern const char *const OpNames[];   ///< "add", "remove", "swap", "hold", "kick out", "return", "start", "end"

/**
 * @brief Phase 18 (28.09.2026): the parts the Leveler sets against the kick, each by its own correction -- the kit's
 *        twelve lanes (0 .. 11, each by its role) and the tonal voices. Their presets differ by 20 dB and more in what
 *        they give, and a mix fitted to the references' spectrum had left them 8 to 10 dB under the research's levels
 *        against the kick (the kick and the rumble filled the mids a stab fills on a record).
 */
enum class BalPartE : int
{
    PING = 12,
    BASS,
    ACID,
    CHORD,
    DRONE,
    TEXTURE,
    ROOM,
    COUNT
};   ///< (the room's return: the guard's)
constexpr int BalLanes = 12;                                    ///< the kit's lanes come first
constexpr int BalParts = static_cast<int>( BalPartE::COUNT );   ///< how many parts the balance corrects
/** @brief A correction per part, dB (0: none). */
using BalanceDbT = std::array<float, BalParts>;
extern const char *const
    BalPartNames[BalParts];   ///< "perc1" .. "perc12", "ping", "bass", "acid", "chord", "drone", "texture", "room"

/**
 * @brief Where a track's loudness is set (Leveler.h, after Ephemeris): its start, where its loudest part begins, what that
 *        part should measure, and the correction found for it -- the master gain from the track's start on.
 */
struct LevelMark
{
    double     m_beat = 0.0;            ///< the track's start
    double     m_peakBeat = 0.0;        ///< where its loudest part begins
    float      m_targetLufs = -11.0f;   ///< what that part should measure (the style's, PLAN 8.5)
    float      m_trimDb = 0.0f;         ///< the correction (levelScore); 0 until measured
    BalanceDbT m_balDb{};   ///< Phase 18: the parts' corrections against the kick (levelScore); 0 until measured
    std::array<float, 4>
        m_styleMix{};   ///< the track's styles (StyleProfile::styleMix), for the parts' windows; 0: Hypnotic's
    int m_front =
        -1;   ///< Phase 20: the kit lane that opens the track (its intro's perc): kept up front by the balance
};

/**
 * @brief A knob a track sets, from a beat on (28.09.2026, after Ephemeris' KnobSet): the sound of a synth as a program
 *        change -- the preset the composer chose for the track (Presets.h) -- and the mix its style sets. Unlike a gesture
 *        it is no offset: the deck that plays the track plays from this value, the knobs show it (Engine.h), and a hand
 *        that turns the knob turns it from there. The knob settings of one beat are a group: a new group on a deck
 *        replaces the last one's.
 */
struct KnobSet
{
    double m_beat = 0.0;     ///< from where it holds (a track's start)
    int    m_param = -1;     ///< the parameter
    float  m_value = 0.0f;   ///< its value, real units
    int    m_kind = 0;       ///< 0 a sound (a preset's knob), 1 a setting of the mix or the composer
};

/** @brief The factory preset the composer chose for a synth of a track (Presets.h), from where it holds. */
struct SoundPick
{
    double m_beat = 0.0;     ///< from where it holds (a track's start)
    int    m_module = 0;     ///< the synth's Module, as an int
    int    m_instance = 0;   ///< which instance (the lane, for the kit)
    int    m_preset = -1;    ///< its index in factoryPresets(module)
};

/** @brief A named position. */
struct Marker
{
    double      m_beat = 0.0;   ///< position in beats
    std::string m_text;         ///< name, e.g. "Intro", "Block 3", "Reduction"
};

/** @brief A score of one track or a whole set. */
struct Score
{
    TempoMap               m_tempo;               ///< the tempo map; every other time is in beats
    uint64_t               m_seed = 1;            ///< the seed the score was composed from
    int                    m_keyRoot = 9;         ///< pitch class of the key (A = 9)
    int                    m_scale = 0;           ///< compose.scale of the track
    double                 m_lengthBeats = 0.0;   ///< where the score ends
    std::vector<NoteEvent> m_notes;               ///< notes, sorted by beat after sort()
    std::vector<Gesture>   m_gestures;            ///< automation, sorted by beat after sort()
    std::vector<BlockOp>   m_ops;                 ///< the form's operations, sorted by beat after sort()
    std::vector<Marker>    m_markers;             ///< markers, sorted by beat after sort()
    std::vector<LevelMark> m_levels;              ///< every track's loudness mark, in beat order (Leveler.h)
    std::vector<KnobSet>   m_knobs;    ///< the knobs every track sets at its start, sorted by beat after sort()
    std::vector<SoundPick> m_sounds;   ///< the presets the composer chose, sorted by beat after sort()
    /**
     * @brief The value the latest knob setting of @p param at or before @p beat gives it; NaN if none does.
     * @param[in] param  the parameter
     * @param[in] beat   the position in beats
     * @return the value the latest knob setting gives it, NaN if none does
     */
    float knobAt( int param, double beat ) const;
    /**
     * @brief The loudness correction at @p beat in dB: the latest mark's (0 before the first).
     * @param[in] beat  the position in beats
     * @return the loudness correction there in dB
     */
    float trimAt( double beat ) const;
    /**
     * @brief The parts' corrections at @p beat, dB: the latest mark's (none before the first).
     * @param[in] beat  the position in beats
     * @return the parts' corrections there in dB
     */
    BalanceDbT balanceAt( double beat ) const;
    /** @brief Sorts every list by beat (stable, so equal beats keep the order they were written in). */
    void sort();
    /**
     * @brief Empties the score and resets the tempo to @p bpm.
     * @param[in] bpm  the new tempo in beats per minute
     */
    void clear( double bpm );
    /**
     * @brief The automation offset of @p param at @p beat, in the normalised range.
     *
     * Of the curves on one knob, the latest that has started counts; before the first one the offset is 0. Linear
     * in the number of curves -- for the offline render and the tests; the audio thread keeps a cursor instead.
     * @param[in] param  the parameter
     * @param[in] beat   the position in beats
     * @return the automation offset in the normalised range
     */
    float gestureOffset( int param, double beat ) const;
};

/**
 * @brief A set (PLAN 7.7): what each deck plays, one after another on it -- the tracks, their automation, the DJ
 *        mixer's moves on the deck's own channel (module deck, instance = the deck) -- all on the set's tempo map
 *        (every deck's Score::tempo is the same), and the set's own markers.
 */
struct SetScore
{
    static constexpr int Decks = 3;             ///< as Params.h Decks
    Score                m_decks[Decks];        ///< deck A, B, C
    double               m_lengthBeats = 0.0;   ///< the set's length
    std::vector<Marker>  m_markers;             ///< tracks, blends, bass swaps
    std::vector<Gesture> m_gestures;            ///< the set's own automation (the mixer's effects, module djfx)
};

}   // namespace tot
