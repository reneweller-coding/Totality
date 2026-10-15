/**
 * @file EditorStyle.h
 * @brief The Style tab (PLAN 10.1): the profile the knobs describe (compose.style, its morph and the axes, or a style of
 *        the user's own) as its numbers, the references' measurements per style (PLAN 13.4, the corridor the profiles
 *        are fitted to), and the custom style's knobs.
 * @author Rene Weller
 */
#pragma once
#include "PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

/** @brief The Style tab. */
class StylePage final : public juce::Component, private juce::Timer {
public:
    /**
     * @brief The tab for @p p.
     * @param[in,out] p  the processor
     */
    explicit StylePage(TotalityProcessor& p);
    /** @brief The profile's numbers and the button on top, the custom style's knobs under them. */
    void resized() override;
    /**
     * @brief The style's profile beside the references' medians.
     * @param[in,out] g  the graphics context to draw into
     */
    void paint(juce::Graphics& g) override;

private:
    /** @brief Follows the style chosen. */
    void timerCallback() override;
    TotalityProcessor& m_proc;   ///< the processor
    ScrollingPage m_custom;   ///< the custom style's knobs
    juce::TextButton m_take{ "Take the profile's numbers" };   ///< copies the profile's numbers into the custom style
    juce::String m_shown;   ///< the profile's numbers as last drawn
};
