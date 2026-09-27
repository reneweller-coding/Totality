/**
 * @file EditorPerform.h
 * @brief The live pages (PLAN 10.1): Perform -- as at a mixing desk: the groups muted and unmuted, the isolator kills
 *        and faders of the decks, the master filter, the echo throw, each learnable from a MIDI controller -- and Mixer,
 *        the meters of the decks and the output beside the mix's and the master's knobs.
 */
#pragma once
#include "PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

/** @brief A small button that binds the next MIDI controller to a parameter (right click: forget it). */
class LearnButton final : public juce::TextButton {
public:
    LearnButton(UmbraProcessor& p, int id);
    void refresh();                                         ///< shows the binding, or that it listens
    void mouseUp(const juce::MouseEvent& e) override;       ///< right click forgets

private:
    UmbraProcessor& proc_;
    int id_;
};

/** @brief The Perform tab. */
class PerformPage final : public juce::Component, private juce::Timer {
public:
    explicit PerformPage(UmbraProcessor& p);
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;
    UmbraProcessor& proc_;
    juce::OwnedArray<juce::TextButton> mutes_;
    std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> muteAttach_;
    juce::Slider filter_, throw_;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> sliderAttach_;
    /** @brief A deck's strip: three kills and the fader. */
    struct Strip {
        juce::TextButton kill[3];
        juce::Slider fader;
    };
    Strip strips_[umb::kDecks];
    juce::OwnedArray<LearnButton> learn_;
    std::vector<juce::Rectangle<int>> learnFor_;   ///< (layout) where each learn button sits
};

/** @brief The Mixer tab: meters and the mix's, the master's, the motion's and the decks' knobs. */
class MixerPage final : public juce::Component, private juce::Timer {
public:
    explicit MixerPage(UmbraProcessor& p);
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;
    UmbraProcessor& proc_;
    ScrollingPage knobs_;
    float peak_[umb::kDecks] = {}, rms_[umb::kDecks] = {}, out_ = 0.0f, lufs_ = -70.0f;
    float hold_[umb::kDecks + 1] = {};
};
