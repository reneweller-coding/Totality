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
    /** @brief The tab for @p p. */
    explicit StylePage(TotalityProcessor& p);
    /** @brief The profile's numbers and the button on top, the custom style's knobs under them. */
    void resized() override;
    /** @brief The style's profile beside the references' medians. */
    void paint(juce::Graphics& g) override;

private:
    /** @brief Follows the style chosen. */
    void timerCallback() override;
    TotalityProcessor& proc_;   ///< the processor
    ScrollingPage custom_;   ///< the custom style's knobs
    juce::TextButton take_{ "Take the profile's numbers" };   ///< copies the profile's numbers into the custom style
    juce::String shown_;   ///< the profile's numbers as last drawn
};
