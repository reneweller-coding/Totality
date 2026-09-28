/**
 * @file EditorStyle.h
 * @brief The Style tab (PLAN 10.1): the profile the knobs describe (compose.style, its morph and the axes, or a style of
 *        the user's own) as its numbers, the references' measurements per style (PLAN 13.4, the corridor the profiles
 *        are fitted to), and the custom style's knobs.
 */
#pragma once
#include "PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

/** @brief The Style tab. */
class StylePage final : public juce::Component, private juce::Timer {
public:
    explicit StylePage(TotalityProcessor& p);
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;
    TotalityProcessor& proc_;
    ScrollingPage custom_;
    juce::TextButton take_{ "Take the profile's numbers" };
    juce::String shown_;   ///< the profile's numbers as last drawn
};
