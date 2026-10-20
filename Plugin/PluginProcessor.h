/**
 * @file PluginProcessor.h
 * @brief The plugin (PLAN 10.1, 12; Phase 5): the engine, the composer on a thread of its own, transport, the performer.
 *
 * **Parameters.** Every entry of the engine's ParamStore is a host parameter (StoreParameter, after Phosphene's and
 * Ephemeris'): the store is the only place a value lives, read and written as a relaxed atomic, so a knob in the plugin
 * and the same knob in tot_render stand at the same place.
 *
 * **Composing.** "Compose" snapshots the parameters, the seed and the rerolls and hands them to the composer thread: a
 * track (composeTrack) or, with set.minutes above zero, a set (composeSet). The finished score waits until the message
 * thread loads it into the engine with processing suspended -- the engine allocates when it loads, and the audio thread
 * never does. Its loudness is measured afterwards on the same thread while it already plays (Leveler.h), and the
 * corrections glide in.
 *
 * **Time.** In a host the playhead is the clock: the engine follows its position in beats and jumps when the host does,
 * at the host's tempo -- the score is loaded with the host's tempo as a constant (forPlayback, PLAN 16.2: "im Host gilt
 * das Host-Tempo"), and a new host tempo loads it again on the message thread from the beat it was at. A set's own tempo
 * drift is the standalone's and the export's. The standalone has its own play and stop.
 *
 * **Performing** (PLAN 10.1, Perform). The engine plays live (Engine::setLive): the perform module's mutes, master
 * filter and echo throw act, and the mixer is in a track's path, so the isolator kills work on a single track too. MIDI
 * reaches them: the keys from middle C up (C to F#) toggle the mutes of kick, sub, hats, perc, ping, bass and pads; the
 * controller 74 (brightness) moves the master filter, the expression pedal the throw (the same in every generator); any controller can be learned for any parameter
 * (learn()). The bindings are part of the state.
 *
 * **Cues** (PLAN 10.3, Cue.h): with cue.enabled the beats, bars, blocks, operations and keys go out as OSC over UDP to
 * `TOT_CUE_HOST` (default this machine) at cue.port, each at the moment it is heard.
 *
 * **Undo** (01.10.2026, the frame): a knob turned on the panel, a preset, a new seed, a reroll, a loaded set or a choice of
 * track or mix is a step (frame::UndoHistory); a step holds only what it changed, so taking it back never takes back the
 * sounds the engine wrote on the knobs since. Knobs moved from MIDI or by a host's automation are not steps.
 *
 * **The headset** (01.10.2026, the frame): the hands of the Quest app in bridge mode arrive as OSC (frame::Headset) while
 * the settings do not say Off: left pinch play and stop, both hands the next track, right pinch the kick out and in, the
 * left hand's height the master filter, the right hand's the echo throw.
 *
 * **Mute** (after Phosphene). The output can be muted: silence at the very end of processBlock, after the meters and the
 * test recording have read the block. `TOT_MUTE=1` -- and the screenshot mode `TOT_SHOT` -- start the plugin muted, and
 * then it never unmutes itself: an automated run makes no sound.
 *
 * @note After Ephemeris' Plugin/PluginProcessor.h at d047d79 (27.09.2026).
 * @author Rene Weller
 */
#pragma once
#include "tot/Engine.h"
#include "tot/SetFile.h"
#include "tot/compose/Composer.h"
#include "tot/compose/Set.h"
#include "Frame.h"
#include "LinkClock.h"
#include "Jam.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

/** @brief One entry of the ParamStore as a host parameter. */
class StoreParameter final : public juce::RangedAudioParameter
{
public:
    /**
     * @brief Binds parameter @p id of @p store (which outlives this object) under the display name @p name.
     * @param[in,out] store  the parameter store
     * @param[in]     id     the parameter
     * @param[in]     name   the display name
     */
    StoreParameter( tot::ParamStore &store, int id, const juce::String &name );
    /**
     * the store's value, normalised
     * @return the store's value, normalised
     */
    float getValue() const override;
    /**
     * writes the store (relaxed atomic)
     * @param[in] newValue  the value, normalised
     */
    void setValue( float newValue ) override;
    /**
     * the descriptor's default, normalised
     * @return the default, normalised
     */
    float getDefaultValue() const override;
    /**
     * the display name (0: no limit)
     * @param[in] maximumStringLength  the longest name the host takes, 0 for no limit
     * @return the display name
     */
    juce::String getName( int maximumStringLength ) const override;
    /**
     * the unit
     * @return the unit
     */
    juce::String getLabel() const override;
    /**
     * steps of a discrete parameter
     * @return the steps of a discrete parameter
     */
    int getNumSteps() const override;
    /**
     * Int, Choice and Toggle are discrete
     * @return true for Int, Choice and Toggle
     */
    bool isDiscrete() const override;
    /**
     * a Toggle is boolean
     * @return true for a Toggle
     */
    bool isBoolean() const override;
    /**
     * the value as the panel shows it
     * @param[in] normalisedValue      the value, normalised
     * @param[in] maximumStringLength  the longest text the host takes
     * @return the value as the panel shows it
     */
    juce::String getText( float normalisedValue, int maximumStringLength ) const override;
    /**
     * parses a choice name, On/Off or a number
     * @param[in] text  a choice name, On/Off or a number
     * @return the value, normalised
     */
    float getValueForText( const juce::String &text ) const override;
    /**
     * the store's own mapping
     * @return the store's mapping
     */
    const juce::NormalisableRange<float> &getNormalisableRange() const override { return m_range; }
    /**
     * the id in the store
     * @return the id in the store
     */
    int paramId() const { return m_id; }

private:
    tot::ParamStore               &m_store;   ///< the store it reads and writes
    int                            m_id;      ///< the id in the store
    juce::String                   m_name;    ///< the display name
    juce::NormalisableRange<float> m_range;   ///< the store's mapping between the real and the normalised value
};

/** @brief A track where it lies in what plays (a track alone: at beat 0 on deck A). */
struct TrackPlace
{
    double         m_start = 0.0;    ///< set beat of the track's first bar
    double         m_swapIn = 0.0;   ///< set beat where it takes over the low end
    double         m_end = 0.0;      ///< set beat of its end
    int            m_deck = 0;       ///< the deck it plays on
    tot::TrackInfo m_info;           ///< what the composer says of it
};

/** @brief What plays: a set, or a track as deck A of a one-track set, with where its tracks lie. */
struct Playing
{
    tot::SetScore           m_set;             ///< the set, or the one track as deck A
    bool                    m_isSet = false;   ///< a set (a DJ mix), not a single track
    std::vector<TrackPlace> m_tracks;          ///< its tracks where they lie
    tot::SetInfo            m_setInfo;         ///< a set's (empty for a track)
};

/** @brief The Totality processor. */
class TotalityProcessor final : public juce::AudioProcessor,
                                private juce::Thread,
                                private juce::Timer,
                                private juce::AudioProcessorParameter::Listener
{
public:
    TotalityProcessor();             ///< registers every parameter and composes a first track
    ~TotalityProcessor() override;   ///< stops the composer and the exporter

    // Composing and curating.
    void compose();   ///< compose with the current settings, seed and rerolls
    void newSeed();   ///< a fresh seed, no rerolls, then compose
    /**
     * @brief Draws @p unit again ("blocks", or in a set "track3.blocks", "track3", "set"), then composes.
     * @param[in] unit  the unit to draw again
     */
    void reroll( const juce::String &unit );
    /**
     * whether the composer thread is at work
     * @return true while the composer thread is at work
     */
    bool isComposing() const { return m_composing.load(); }
    /**
     * @brief Whether the knobs ask for a DJ mix (set.minutes above 0) rather than a single track.
     * @return true when the knobs ask for a DJ mix
     */
    bool mixChosen() const;
    /**
     * @brief A single track or a DJ mix (message thread): set.minutes to 0, or back to the mix's last length (60 min
     *        at first); then composes, if what plays is not that already.
     * @param[in] mix  true for a DJ mix, false for a single track
     * @pre called on the message thread
     */
    void chooseMix( bool mix );
    /**
     * whether what plays is a DJ mix
     * @return true when what plays is a DJ mix
     */
    bool playingMix() const
    {
        std::lock_guard<std::mutex> g( m_lock );
        return m_current.m_isSet;
    }
    /**
     * the seed of what plays
     * @return the seed of what plays
     */
    uint64_t seed() const
    {
        std::lock_guard<std::mutex> g( m_lock );
        return m_seed;
    }
    /**
     * the rerolls, for the panel
     * @return the rerolls as text
     */
    juce::String curationText() const;
    /**
     * @brief The track under @p beat (its index in what plays), -1 if none. In a set, the one that owns the low end.
     * @param[in] beat  the position in beats
     * @return the track's index, -1 if none
     */
    int trackAt( double beat ) const;
    /**
     * @brief Phase 17: rates the track under the playhead (+1 liked, -1 not) into Documents/Totality/ratings.tsv; the
     *        ratings weigh the archetypes and preset groups while compose.use_ratings is on (Preferences.h).
     * @param[in] value  +1 liked, -1 not
     * @return what was rated, for the panel (empty when nothing plays)
     */
    juce::String rate( int value );

    // Transport (the standalone's; a host drives its own).
    /**
     * play or stop (the standalone's transport)
     * @param[in] on  true to play
     */
    void setPlaying( bool on ) { m_playing = on; }
    /**
     * whether the standalone plays
     * @return true while the standalone plays
     */
    bool isPlaying() const { return m_playing.load(); }
    /**
     * @brief The standalone's Ableton Link, as the settings menu says it: off, alone, or how many apps are with it.
     * @return the line
     */
    juce::String linkStatus() const;
    /**
     * @brief The family jam, as the settings menu says it (Jam.h): off, leading, or whom it follows.
     * @return the line
     */
    juce::String jamStatus() const { return m_jam.status(); }
    /**
     * @brief The transposition a follower plays now (semitones; the family jam's root, Jam.h).
     * @return the transposition in semitones
     */
    int jamTranspose() const { return m_jamTransposeOut.load( std::memory_order_relaxed ); }
    /**
     * @brief Jumps to @p beat at the next block (the display goes there at once, also while nothing plays).
     * @param[in] beat  the position in beats
     */
    void seekTo( double beat )
    {
        m_seekRequest = beat;
        m_position = beat;
    }
    /**
     * where the audio thread is, in beats
     * @return where the audio thread is, in beats
     */
    double positionBeats() const { return m_position.load(); }
    /**
     * @brief Counts the scores the engine has loaded: a display compares it to know when to copy again.
     * @return how many scores the engine has loaded
     */
    int scoreVersion() const { return m_scoreVersion.load(); }
    /**
     * @brief A copy of what plays (message thread; for the arrange and eclipse views).
     * @param[out] out  receives the copy
     * @pre called on the message thread
     */
    void copyPlaying( Playing &out ) const
    {
        std::lock_guard<std::mutex> g( m_lock );
        out = m_current;
    }
    /**
     * @brief The factory preset the composer chose for instance @p instance of synth @p m in the track whose sounds the
     *        knobs show (Engine::leadDeck): its index in tot::factoryPresets(m), -1 if none.
     * @param[in] m         the synth
     * @param[in] instance  its instance
     * @return the preset's index
     */
    int composedPreset( tot::ModuleE m, int instance ) const;
    /**
     * @brief Puts factory preset @p index of @p m on instance @p instance's knobs, through the host's parameters.
     * @param[in] m         the synth
     * @param[in] instance  its instance
     * @param[in] index     the preset's index
     */
    void applyPreset( tot::ModuleE m, int instance, int index );
    /**
     * @brief Sets the knobs @p values (store id, real value) through the host's parameters as one undo step named @p what
     *        (15.10.2026: a preset of the player's own, a value typed into the command palette).
     * @param[in] what    the undo step's name
     * @param[in] values  the knobs: store id and real value
     */
    void applyKnobs( const juce::String &what, const std::vector<std::pair<int, float>> &values );
    /**
     * @brief Plays @p values (store id, real value) while a preset is listened to in the browser -- in the store only: no
     *        undo step, nothing to the host --; endPreview() takes them back.
     * @param[in] values  the knobs: store id and real value
     */
    void previewKnobs( const std::vector<std::pair<int, float>> &values );
    /** @brief Back to the knobs as they were before previewKnobs (nothing when no preview plays). */
    void endPreview();
    /**
     * @brief The length of what plays, in beats and seconds (as composed).
     * @param[out] beats    receives the length in beats
     * @param[out] seconds  receives the length in seconds
     */
    void length( double &beats, double &seconds ) const;

    // Files.
    /**
     * writes seed, lengths, rerolls and parameters as an .totset
     * @param[in] file  the .totset file
     * @return false when it cannot be written
     */
    bool saveSet( const juce::File &file );
    /**
     * reads an .totset and composes it
     * @param[in] file  the .totset file
     * @return false when it cannot be read
     */
    bool loadSet( const juce::File &file );
    /**
     * @brief Loads one of the factory's DJ sets (tot/FactorySets.h) and composes it.
     * @param[in] index  the set
     * @return false when there is none of that index
     */
    bool loadFactorySet( int index );
    /**
     * @brief Reads a set from its text (a file's, a factory set's) and composes it.
     * @param[in] name  what the undo step calls it
     * @param[in] text  the set's text
     * @return false when the text is no set
     */
    bool loadSetText( const juce::String &name, const std::string &text );
    /** @brief What an export writes beside the WAV and its MIDI and cues. */
    enum ExportExtraE
    {
        STEMS = 1,
        LOOPS = 2
    };
    /**
     * @brief Renders what plays offline (as composed: not live) to a 24-bit WAV with its cues (a cue chunk and JSON) and
     *        MIDI beside it, on a thread; returns at once. @p extras: STEMS a folder "\<name\>_stems" (their sum is the mix
     *        before the master), LOOPS a folder "\<name\>_loops" with the DJ loops (a track only).
     * @param[in] wav     the WAV file
     * @param[in] extras  what else goes beside it
     */
    void exportTo( const juce::File &wav, int extras );
    /**
     * one line for the panel
     * @return the line
     */
    juce::String status() const;
    /**
     * Phase 17: Documents/Totality/ratings.tsv
     * @return the ratings file
     */
    static juce::File ratingsFile();
    /**
     * @brief The test mode (TOT_SEED, TOT_PLAY = seconds, TOT_RECORD = a WAV file; TOT_SET = minutes, a set): a fixed
     *        seed, play at once, record what the audio thread renders; recordingDone() when the seconds are full.
     * @return true once the test recording is written
     */
    bool recordingDone() const { return m_recordTarget > 0 && m_recordPos.load() >= m_recordTarget; }
    /**
     * writes the recording (message thread)
     * @pre called on the message thread
     */
    void writeRecording();
    /**
     * @brief The meters since the last call (message thread): per deck the peak and the RMS after its mixer channel, the
     *        output's peak, and its loudness over the last 400 ms (K-weighted, BS.1770), -70 in silence.
     * @param[out] deckPeak       receives each deck's peak
     * @param[out] deckRms        receives each deck's RMS
     * @param[out] outPeak        receives the output's peak
     * @param[out] momentaryLufs  receives the momentary loudness in LUFS
     * @pre called on the message thread
     */
    void takeMeters( float *deckPeak, float *deckRms, float &outPeak, float &momentaryLufs );
    /**
     * @brief The mixer's strips since the last call (message thread, the Mixer page): per tot::MeterSink::Strip the
     *        loudest sample and the RMS, both decks together.
     * @param[out] peak  receives each strip's loudest sample
     * @param[out] rms   receives each strip's RMS
     * @pre called on the message thread
     */
    void takeStripMeters( float *peak, float *rms );

    // Muting.
    /**
     * the output is silenced
     * @return true while the output is silenced
     */
    bool muted() const { return m_mute.load( std::memory_order_relaxed ); }
    /**
     * @brief Mutes or unmutes; does nothing while `TOT_MUTE` forces it.
     * @param[in] on  true to mute
     */
    void setMuted( bool on )
    {
        if ( !m_forceMute ) m_mute.store( on, std::memory_order_relaxed );
    }
    /**
     * `TOT_MUTE` (or `TOT_SHOT`) was set: the switch is stuck on
     * @return true when TOT_MUTE or TOT_SHOT holds the mute on
     */
    bool muteForced() const { return m_forceMute; }

    // Undo (the frame).
    /**
     * takes the last step back; false if there was none
     * @return false when there was nothing to undo
     */
    bool undo();
    /**
     * makes the last undone step again
     * @return false when there was nothing to redo
     */
    bool redo();
    /**
     * what undo takes back (empty: nothing)
     * @return what undo takes back, empty for nothing
     */
    juce::String undoName() const { return m_history.undoName(); }
    /**
     * what redo makes again
     * @return what redo makes again, empty for nothing
     */
    juce::String redoName() const { return m_history.redoName(); }
    /**
     * @brief Opens a step of several knobs (a preset, a reset): they are undone together. Close with endStep.
     * @param[in] what  the step's name
     */
    void beginStep( const juce::String &what );
    void endStep();   ///< closes beginStep's step
    /**
     * @brief Store id @p id back to its default (one step).
     * @param[in] id  the store id
     */
    void resetToDefault( int id );

    // The panel's live rings.
    /**
     * @brief The value store id @p id plays at the moment, normalised -- NaN where it is the knob's own.
     * @param[in] id  the store id
     * @return the value played, normalised; NaN where it is the knob's own
     */
    float playedNormalised( int id ) const;

    // The headset (the frame).
    /**
     * the hands arriving from the Quest app
     * @return the hands from the Quest app
     */
    frame::Headset &headset() { return m_headset; }

    // Performing.
    /**
     * @brief Binds the next MIDI controller that arrives to store id @p id; -1 cancels.
     * @param[in] id  the store id, -1 to cancel
     */
    void learn( int id ) { m_learn = id; }
    /**
     * the store id waiting for a controller, or -1
     * @return the store id waiting for a controller, -1 for none
     */
    int learning() const { return m_learn.load(); }
    /**
     * @brief The controller bound to store id @p id, or -1.
     * @param[in] id  the store id
     * @return the controller bound to it, -1 for none
     */
    int controllerFor( int id ) const;
    /**
     * @brief Unbinds store id @p id.
     * @param[in] id  the store id
     */
    void forget( int id );

    /**
     * the engine's parameters
     * @return the engine's parameters
     */
    tot::ParamStore &store() { return m_engine.params(); }
    /**
     * @brief The host parameter of store id @p id, or null.
     * @param[in] id  the store id
     * @return its host parameter, null for none
     */
    StoreParameter *parameter( int id )
    {
        return id >= 0 && id < static_cast<int>( m_params.size() ) ? m_params[static_cast<size_t>( id )] : nullptr;
    }
    /**
     * @brief Sets store id @p id to the real value @p value through its host parameter (a gesture).
     * @param[in] id     the store id
     * @param[in] value  the real value
     */
    void setFromUi( int id, float value ) { setFromMidi( id, value ); }

    // juce::AudioProcessor: a stereo instrument, one program, the state as XML.
    /**
     * prepares the engine and reloads the score
     * @param[in] sampleRate       the sample rate in Hz
     * @param[in] samplesPerBlock  the largest block the host will ask for
     */
    void prepareToPlay( double sampleRate, int samplesPerBlock ) override;
    void releaseResources() override {}   ///< nothing to release
    /**
     * stereo out only
     * @param[in] layouts  the layout the host proposes
     * @return true for stereo out
     */
    bool isBusesLayoutSupported( const BusesLayout &layouts ) const override;
    /**
     * plays, following the host's playhead
     * @param[in,out] buffer  the block's audio, its input replaced by the output
     * @param[in,out] midi    the block's MIDI, in and out
     */
    void processBlock( juce::AudioBuffer<float> &, juce::MidiBuffer & ) override;
    /**
     * the panel
     * @return the panel, owned by the host
     */
    juce::AudioProcessorEditor *createEditor() override;
    /**
     * it has one
     * @return true
     */
    bool hasEditor() const override { return true; }
    /**
     * "Totality"
     * @return "Totality"
     */
    const juce::String getName() const override { return JucePlugin_Name; }
    /**
     * MIDI in: the performer's keys and controllers
     * @return true
     */
    bool acceptsMidi() const override { return true; }
    /**
     * MIDI out: the composer's notes as they play (02.10.2026)
     * @return true
     */
    bool producesMidi() const override { return true; }
    /**
     * the rooms ring on
     * @return the seconds the rooms ring on
     */
    double getTailLengthSeconds() const override { return 8.0; }
    /**
     * one program
     * @return 1
     */
    int getNumPrograms() override { return 1; }
    /**
     * always the one
     * @return 0
     */
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram( int ) override {}   ///< nothing to switch
    /**
     * "Set"
     * @return "Set"
     */
    const juce::String getProgramName( int ) override { return "Set"; }
    void changeProgramName( int, const juce::String & ) override {}   ///< not renameable
    /**
     * seed, rerolls, parameters, controllers as XML
     * @param[out] destData  receives the state
     */
    void getStateInformation( juce::MemoryBlock &destData ) override;
    /**
     * restores them and composes
     * @param[in] data         the saved state
     * @param[in] sizeInBytes  its size in bytes
     */
    void setStateInformation( const void *data, int sizeInBytes ) override;

private:
    /** @brief Nothing: a value alone is no step (the gestures are). */
    void parameterValueChanged( int, float ) override {}
    /**
     * a knob on the panel: a step
     * @param[in] parameterIndex     the host parameter's index
     * @param[in] gestureIsStarting  true when the gesture begins
     */
    void parameterGestureChanged( int parameterIndex, bool gestureIsStarting ) override;
    /**
     * every store value (undo)
     * @return every store value
     */
    std::vector<float> values() const;
    /**
     * seed, rerolls and the mix's length as text (undo)
     * @return the state as text
     */
    juce::String extraState() const;
    /**
     * the inverse; composes if seed or rerolls changed
     * @param[in] text  the state as extraState() wrote it
     */
    void applyExtra( const juce::String &text );
    /**
     * @brief Puts undo step @p s back (@p after false) or makes it again (@p after true).
     * @param[in] s      the undo step
     * @param[in] after  true to make it again, false to take it back
     */
    void applyStep( const frame::UndoStep &s, bool after );
    /**
     * the hands' events, 30 times a second (message thread)
     * @pre called on the message thread
     */
    void pollHeadset();
    void run() override;   ///< the composer thread
    /**
     * loads a finished score on the message thread
     * @pre called on the message thread
     */
    void timerCallback() override;
    /**
     * @brief Composes with the knobs as they are (copied into @p snapshot).
     * @param[out] snapshot  receives the knobs as they are
     * @return what was composed
     */
    Playing composeNow( tot::ParamStore &snapshot );
    /**
     * @brief Hands the loudness corrections, measured while it plays, to the engine (message thread).
     * @pre called on the message thread
     */
    void takeTrims();
    /**
     * @brief @p p's set as the engine plays it here: in a host at the host's tempo (constant), in the standalone as
     *        composed.
     * @param[in] p  what was composed
     * @return the set as the engine plays it here
     */
    tot::SetScore forPlayback( const Playing &p ) const;
    /**
     * @brief Loads @p p into the engine (message thread, processing suspended by the caller).
     * @param[in] p  what was composed
     * @pre called on the message thread
     */
    void loadEngine( const Playing &p );
    /**
     * @brief The performer's MIDI: keys toggle the mutes, controllers move what they are bound to (audio thread).
     * @param[in] midi  the block's MIDI
     * @pre called on the audio thread
     */
    void perform( const juce::MidiBuffer &midi );
    int m_keyboardSeen = 0;   ///< the keyboard target of the last block (audio thread): a change releases every key
    // MIDI out (02.10.2026): the composer's notes as the decks play them, on the channels of the MIDI export (Midi.h).
    tot::NoteTap m_noteTap;   ///< what the decks played in the last process() call (audio thread)
    frame::KeyMemory
        m_keyMemory;   ///< where each held key went (the split, Scale Lock), so its release follows (02.10.2026)
    /** @brief A note-off due later, for a note whose part has none of its own (NoteTap::Note::offSample). */
    struct PendingOff
    {
        int64_t m_sample;    ///< when, on the engine's sample counter
        uint8_t m_channel;   ///< MIDI channel, 1..16
        uint8_t m_pitch;     ///< MIDI note
    };
    std::array<PendingOff, 256> m_pendingOff{};      ///< the note-offs not yet sent (audio thread)
    int                         m_pendingOffs = 0;   ///< how many of m_pendingOff are due
    int64_t                     m_midiExpect = -1;   ///< the sample the next block should start at; another is a jump
    bool                        m_midiSounding = false;   ///< a note-on went out since the last all-notes-off
    /**
     * @brief Writes the notes the decks played in the block from @p start (@p n samples) into @p midi, and the due
     *        offs.
     * @param[in,out] midi   the block's MIDI, written to
     * @param[in]     start  the block's first sample
     * @param[in]     n      how many samples
     */
    void emitMidi( juce::MidiBuffer &midi, int64_t start, int n );
    /**
     * @brief All notes off on every channel, at the block's start, when a note sounded (a stop, a jump).
     * @param[in,out] midi  the block's MIDI, written to
     */
    void silenceMidi( juce::MidiBuffer &midi );
    /**
     * @brief Sets store id @p id to the real value @p value through its host parameter.
     * @param[in] id     the store id
     * @param[in] value  the real value
     */
    void setFromMidi( int id, float value );

    tot::Engine                   m_engine;     ///< the engine
    std::vector<StoreParameter *> m_params;     ///< the host parameters, one per store id (owned by the processor)
    uint64_t                      m_seed = 1;   ///< the seed of what plays
    tot::Curation                 m_curation;   ///< the rerolls and locks
    mutable std::mutex            m_lock;       ///< guards what the composer thread hands over
    std::vector<tot::Rating>      m_ratings;    ///< Phase 17: the player's ratings (m_lock)
    std::unique_ptr<Playing>      m_pending;    ///< composed, waiting to be loaded
    Playing                       m_current;    ///< what the engine plays
    std::atomic<bool>             m_composing{ false };   ///< the composer thread works
    std::atomic<bool>             m_playing{ false };     ///< play is on
    std::atomic<bool>             m_exporting{ false };   ///< an export runs
    std::atomic<bool>             m_again{ false };   ///< compose was asked for while composing: once more when done
    // The loudness (Leveler.h): measured on the composer thread once the score is handed over, while it plays.
    std::atomic<bool>  m_newer{ false };      ///< a newer score is asked for: the measuring of the last one stops
    uint64_t           m_composed = 0;        ///< counts the compositions
    uint64_t           m_pendingId = 0;       ///< m_pending's number (m_lock)
    uint64_t           m_playingId = 0;       ///< m_current's number (m_lock)
    std::vector<float> m_trims[tot::Decks];   ///< the corrections found for the composition m_trimsFor (m_lock)
    std::vector<tot::BalanceDbT> m_bal[tot::Decks];       ///< Phase 18: the parts' corrections found with them (m_lock)
    std::vector<tot::ToneDbT>    m_tone[tot::Decks];      ///< 20.10.2026: the tone trims found with them (m_lock)
    uint64_t                     m_trimsFor = 0;          ///< the composition m_trims and m_bal belong to (m_lock)
    bool                         m_levelled = false;      ///< m_current carries its corrections (m_lock)
    std::atomic<double>          m_position{ 0.0 };       ///< where the engine is, beats (audio thread writes)
    std::atomic<double>          m_seekRequest{ -1.0 };   ///< a jump asked for, beats; -1 none
    double                       m_sampleRate = 48000.0;   ///< the sample rate, Hz
    int                          m_blockSize = 512;        ///< the largest block, samples
    juce::String                 m_lastExport;             ///< the last export's result, for the Export tab
    float m_mixMinutes = 60.0f;   ///< the mix's length while a single track is chosen (chooseMix; the state)
    std::unique_ptr<std::thread> m_exporter;   ///< the export thread while it runs
    std::vector<float>           m_record;     ///< interleaved, allocated in prepareToPlay in the test mode only
    size_t                       m_recordTarget = 0;   ///< TOT_RECORD: how many values m_record takes, 0 off
    std::atomic<size_t>          m_recordPos{ 0 };     ///< how many are written
    bool                         m_autoPlay = false;   ///< TOT_PLAY: play once the first score is loaded
    // The meters: the decks through their taps, the output's peak and a K-weighted mean square.
    std::vector<float>              m_tapBuf;   ///< Decks x 2 x block (prepareToPlay)
    std::array<float *, tot::Decks> m_tapL{};   ///< per deck: its tap's left channel in m_tapBuf
    std::array<float *, tot::Decks> m_tapR{};   ///< per deck: its tap's right channel in m_tapBuf
    // The stems as outputs of their own (02.10.2026): a stereo bus per stem (Engine::stemName) besides the main output,
    // off until the host switches one on; then the engine writes every stem (Engine::setStems) and each bus that is on
    // gets its own. Pre-master, as the export's stems: they sum to the mix before the master.
    std::vector<float>                      m_stemBuf;           ///< STEMS x 2 x block (prepareToPlay)
    std::array<float *, tot::Engine::STEMS> m_stemL{};           ///< per stem: its left channel in m_stemBuf
    std::array<float *, tot::Engine::STEMS> m_stemR{};           ///< per stem: its right channel in m_stemBuf
    bool                                    m_stemsOn = false;   ///< audio thread: the engine writes the stems now
    /**
     * @brief The buses: the main stereo output, then one stereo output per stem, those off by default.
     * @return the buses
     */
    static BusesProperties busLayout();
    std::array<std::atomic<float>, tot::Decks>  m_meterPeak{};   ///< audio thread raises, the editor takes (exchange 0)
    std::array<std::atomic<double>, tot::Decks> m_meterSum{};    ///< sums of squares since the editor last took them
    std::atomic<int>                            m_meterCount{ 0 };   ///< samples in those sums
    std::atomic<float> m_outPeak{ 0.0f };   ///< the output's peak since the editor last took it
    std::atomic<float> m_lufs{ -70.0f };    ///< the output's momentary loudness, LUFS (400 ms)
    /** @brief A biquad in transposed direct form II (the K weighting). */
    struct Biquad
    {
        double m_b0 = 1;   ///< feed-forward coefficient of x[n]
        double m_b1 = 0;   ///< ... of x[n-1]
        double m_b2 = 0;   ///< ... of x[n-2]
        double m_a1 = 0;   ///< feedback coefficient of y[n-1]
        double m_a2 = 0;   ///< ... of y[n-2]
        double m_z1 = 0;   ///< the first state
        double m_z2 = 0;   ///< the second state
        /**
         * @brief One sample @p x through the filter.
         * @param[in] x  the input sample
         * @return the filtered sample
         */
        double run( double x )
        {
            const double y = m_b0 * x + m_z1;
            m_z1 = m_b1 * x - m_a1 * y + m_z2;
            m_z2 = m_b2 * x - m_a2 * y;
            return y;
        }
    };
    Biquad               m_kShelf[2];           ///< the K weighting: its high shelf, per channel
    Biquad               m_kHigh[2];            ///< ... and its high pass, per channel
    double               m_kMs = 0.0;           ///< the K-weighted mean square
    double               m_kCoef = 0.0;         ///< its one-pole coefficient
    std::atomic<int>     m_scoreVersion{ 0 };   ///< counts the scores loaded (the editor redraws)
    tot::MeterSink       m_meterSink;           ///< the strips' levels, added by the decks (the Mixer page)
    std::atomic<int64_t> m_stripSamples{ 0 };   ///< samples rendered since takeStripMeters
    tot::CueSender       m_cues;              ///< the OSC cues' socket and thread (message thread starts and stops it)
    tot::CueTap          m_cueTap;            ///< audio thread: beat range -> cues
    int                  m_cuePort = 0;       ///< the port the sender was started for, 0 = off (message thread)
    double               m_lastBeat = -1.0;   ///< the beat after the last block (audio thread), to see a jump
    std::array<std::atomic<int>, 128> m_ccMap{};             ///< controller number -> store id, -1 unbound
    std::atomic<int>                  m_learn{ -1 };         ///< learn()
    std::atomic<bool>                 m_mute{ false };       ///< muted()
    bool                              m_forceMute = false;   ///< muteForced()
    std::atomic<double> m_hostBpm{ 0.0 };     ///< the host's tempo as the audio thread last saw it, 0 outside a host
    std::atomic<double> m_playedBpm{ 0.0 };   ///< the tempo the engine's score was loaded with, 0 as composed
    // Ableton Link in the standalone (02.10.2026, LinkClock.h; Settings > Ableton Link).
    frame::LinkClock m_link;   ///< the session (joined on the timer when the setting is on)
    // The family jam (02.10.2026, Jam.h; Settings > Family jam).
    /**
     * @brief Audio thread: as the jam's leader, the sections and keys of the coming beat go out, stamped with their
     *        bar.
     * @param[in] before  the beat at the block's start
     * @param[in] after   the beat at its end
     * @param[in] shared  the shared timeline's beat, negative for none
     * @pre called on the audio thread
     */
    void jamLead( double before, double after, double shared );
    /**
     * @brief Audio thread: as a follower, the engine takes the leader's root (on a bar line), energy and breaks.
     * @param[in] before  the beat at the block's start
     * @param[in] shared  the shared timeline's beat, negative for none
     * @pre called on the audio thread
     */
    void jamFollow( double before, double shared );
    /**
     * @brief The root (pitch class) of the track playing at @p beat: its last key mark, else the score's key.
     * @param[in] beat  the position in beats
     * @return the root as a pitch class
     */
    int homeRootAt( double beat ) const;
    /**
     * @brief Which of compose.scale's values a mode named @p mode is (-1: none of them).
     * @param[in] mode  the mode's name
     * @return the scale's value, -1 for none
     */
    static int scaleOfMode( const juce::String &mode );
    frame::JamBus     m_jam{ "Totality" };        ///< the family jam's bus
    bool              m_jamLeadStarted = false;   ///< audio thread: the leader has said where it stands
    double            m_jamLeadLast = -1.0;       ///< audio thread: where the leader's last block ended (a jump)
    double            m_jamLastBefore = -1.0;     ///< audio thread: the previous block's start (its length in beats)
    int               m_jamTranspose = 0;         ///< audio thread: the transposition the engine plays now
    std::atomic<int>  m_jamTransposeOut{ 0 };     ///< the same, for the message thread
    bool              m_jamFollowing = false;     ///< audio thread: the engine follows the jam (setJam was called)
    std::atomic<int>  m_jamComposeKey{ -1 };      ///< a follower's next track: the leader's root (-1: its own key)
    std::atomic<int>  m_jamComposeScale{ -1 };    ///< and its mode as compose.scale (-1: its own)
    juce::String      m_jamLogged;                ///< the last line written to FAMILY_JAM_LOG
    std::atomic<bool> m_linkFollowing{
        false
    };   ///< other apps are in the session: it rules tempo and phase as a host does
    double m_linkBeat = -1.0;      ///< audio thread: the session's beat at this block (the jam's timeline)
    bool   m_linkPlayed = false;   ///< audio thread: the transport last told to or taken from the session
    /**
     * @brief Whether a clock outside rules the tempo: a host's playhead, or a Link session with other apps in it.
     * @return true when a clock outside rules the tempo
     */
    bool followsClock() const
    {
        return wrapperType != wrapperType_Standalone || m_linkFollowing.load( std::memory_order_relaxed );
    }
    uint32_t           m_toldSounds = 0;                 ///< the engine's soundsVersion() the host was last told of
    frame::UndoHistory m_history;                        ///< undo and redo (message thread)
    bool               m_restoring = false;              ///< an undo or the headset moves knobs: no step of their own
    std::vector<std::pair<int, float>> m_previewSaved;   ///< the knobs a preview replaced (store id, real value)
    frame::Headset                     m_headset;        ///< the Quest's hands (message thread)
    juce::TimedCallback m_headsetTick{ [this] { pollHeadset(); } };   ///< polls the headset 30 times a second
};
