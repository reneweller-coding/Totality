/**
 * @file NoteTap.h
 * @brief The notes the decks played in one process() call, for the plugin's MIDI out (02.10.2026).
 *
 * A deck writes every composer note it actually plays into the tap -- after the performer's mutes and the keyboard's
 * Replace and composer switch, so MIDI out is what is heard -- with its absolute sample; the plugin turns them into
 * MIDI events on the channels of the MIDI export (Midi.h, midiChannelOf). A played key is not written: it came in as
 * MIDI already. Fixed capacity, no allocation: the audio thread fills it and the same thread empties it.
 */
#pragma once
#include <cstdint>

namespace tot {

/** @brief A fixed-capacity list of played notes; clear() before a process() call, read after it. */
struct NoteTap {
    /** @brief One note-on or note-off as the deck played it. */
    struct Note {
        int64_t sample;     ///< when, on the engine's sample counter
        uint8_t part;       ///< the Part
        uint8_t pitch;      ///< MIDI note number
        uint8_t velocity;   ///< 1..127 for an on, 0 for an off
        bool oneShot;       ///< the part has no note-offs of its own (the kick, the kit, the ping)
    };
    static constexpr int kCapacity = 2048;   ///< more notes than any block of 4096 samples plays
    Note notes[kCapacity];                   ///< the notes, in the order they were played
    int count = 0;                           ///< how many of notes[] are valid
    /** @brief Forgets the notes of the last call. */
    void clear() { count = 0; }
    /** @brief Adds a note (dropped when full). */
    void add(int64_t sample, int part, int pitch, float velocity, bool on, bool oneShot)
    {
        if (count >= kCapacity || pitch < 0 || pitch > 127) return;
        const int v = on ? (velocity <= 0.0f ? 1 : velocity >= 1.0f ? 127 : 1 + static_cast<int>(velocity * 126.0f + 0.5f)) : 0;
        notes[count++] = Note{ sample, static_cast<uint8_t>(part), static_cast<uint8_t>(pitch), static_cast<uint8_t>(v), oneShot };
    }
};

} // namespace tot
