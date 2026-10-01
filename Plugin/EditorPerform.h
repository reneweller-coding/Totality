/**
 * @file EditorPerform.h
 * @brief The live pages (PLAN 10.1): Perform -- as at a mixing desk: the groups muted and unmuted, the isolator kills
 *        and faders of the decks, the master filter, the echo throw, each learnable from a MIDI controller, and the
 *        headset's hands while one sends -- and Mixer, the frame's console (01.10.2026): a strip per source with its
 *        meter, fader, pan, sends and mute, the buses and the master, the decks with their meters.
 */
#pragma once
#include "PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

/** @brief A small button that binds the next MIDI controller to a parameter (right click: forget it). */
class LearnButton final : public juce::TextButton {
public:
    /** @brief A learn button for store id @p id of @p p. */
    LearnButton(TotalityProcessor& p, int id);
    void refresh();                                         ///< shows the binding, or that it listens
    void mouseUp(const juce::MouseEvent& e) override;       ///< right click forgets

private:
    TotalityProcessor& proc_;   ///< the processor: its controller map
    int id_;   ///< the store id it binds
};

/** @brief The Perform tab. */
class PerformPage final : public juce::Component, private juce::Timer {
public:
    /** @brief The tab for @p p. */
    explicit PerformPage(TotalityProcessor& p);
    /** @brief The mutes, the filter and the throw, the decks' strips, the learn buttons, the keyboard and the headset's group. */
    void resized() override;
    /** @brief The groups' boxes and titles, the hands of a headset. */
    void paint(juce::Graphics& g) override;

private:
    /** @brief Follows the headset and the learn buttons. */
    void timerCallback() override;
    TotalityProcessor& proc_;   ///< the processor
    juce::OwnedArray<juce::TextButton> mutes_;   ///< the mutes, one per group
    std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> muteAttach_;   ///< the mutes on their parameters
    juce::Slider filter_;   ///< the master filter (perform.filter)
    juce::Slider throw_;   ///< the echo throw (perform.throw)
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> sliderAttach_;   ///< the filter, the throw and the faders on their parameters
    /** @brief A deck's strip: three kills and the fader. */
    struct Strip {
        juce::TextButton kill[3];   ///< the kills: low, mid, high
        juce::Slider fader;   ///< the deck's fader
    };
    Strip strips_[tot::kDecks];   ///< a strip per deck
    juce::OwnedArray<LearnButton> learn_;   ///< the learn buttons beside the controls
    std::vector<juce::Rectangle<int>> learnFor_;   ///< (layout) where each learn button sits
    bool headset_ = false;                         ///< the headset's group is shown
    juce::Rectangle<int> headsetArea_;             ///< where it is drawn
    // The keyboard (01.10.2026, Engine::queueLive): what a MIDI keyboard plays, Replace or Layer, the composer on or off.
    std::unique_ptr<frame::Choice> keyPart_;   ///< perform.keyboard_part: what the keys play
    std::unique_ptr<frame::Choice> keyMode_;   ///< perform.keyboard_mode: Replace or Layer
    std::unique_ptr<frame::Switch> composer_;   ///< perform.composer: the composer's notes on or off
    std::unique_ptr<juce::ComboBoxParameterAttachment> keyPartAttach_;   ///< keyPart_ on its parameter
    std::unique_ptr<juce::ComboBoxParameterAttachment> keyModeAttach_;   ///< keyMode_ on its parameter
    std::unique_ptr<juce::ButtonParameterAttachment> composerAttach_;   ///< composer_ on its parameter
    juce::Rectangle<int> keyArea_;                 ///< the Keyboard group's box
};

/** @brief The Decks sub-page: the decks' meters beside their channels' knobs. */
class DecksPage final : public juce::Component {
public:
    /** @brief The sub-page for @p p. */
    explicit DecksPage(TotalityProcessor& p);
    /** @brief The meters left, the knobs right. */
    void resized() override;
    /** @brief The meters with their held peaks, the output and its loudness. */
    void paint(juce::Graphics& g) override;
    /** @brief A new reading of the decks and the output. */
    void meter(const float* peak, const float* rms, float out, float lufs);

private:
    ScrollingPage knobs_;   ///< the decks' channel knobs
    float peak_[tot::kDecks] = {};   ///< per deck: its peak
    float rms_[tot::kDecks] = {};   ///< per deck: its RMS
    float out_ = 0.0f;   ///< the output's peak
    float lufs_ = -70.0f;   ///< the output's momentary loudness, LUFS
    float hold_[tot::kDecks + 1] = {};   ///< the held peaks: per deck, then the output
};

/** @brief The Mixer tab: Console, Buses and Master, Decks (the frame's sub-tabs). */
class MixerPage final : public juce::Component, private juce::Timer {
public:
    /** @brief The tab for @p p. */
    explicit MixerPage(TotalityProcessor& p);
    /** @brief The sub-tabs over the whole page. */
    void resized() override;
    /** @brief The background. */
    void paint(juce::Graphics& g) override;

private:
    /** @brief Reads the meters into the console's strips and the Decks page, and the sounds the strips name. */
    void timerCallback() override;
    TotalityProcessor& proc_;   ///< the processor
    frame::ControlActions actions_;   ///< the controls' right-click menu
    frame::LiveRings live_;   ///< where each knob's value plays
    frame::SubTabs tabs_;   ///< Console, Buses and Master, Decks
    frame::Console* console_ = nullptr;            ///< (owned by tabs_)
    DecksPage* decks_ = nullptr;                   ///< (owned by tabs_, made when first shown)
    std::vector<std::pair<int, tot::Module>> sounds_;   ///< per strip: the synth whose preset it names (Count: none)
    juce::OwnedArray<juce::TextButton> mutes_;   ///< the strips' mutes
    std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> muteLinks_;   ///< the mutes on their parameters
    float lufs_ = -70.0f;   ///< the output's momentary loudness, LUFS
    float outPeak_ = 0.0f;   ///< the output's peak
    double lastPoll_ = 0.0;   ///< when the strip meters were last taken (ms)
    int tick_ = 0;   ///< counts the timer ticks (some readings every few)
};
