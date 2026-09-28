/**
 * @file EditorEclipse.h
 * @brief The Patterns tab (PLAN 10.1): the Eclipse view and the step grid of the bar that plays.
 *
 * **The Eclipse.** The kick is the dark disc in the middle -- the umbra -- and every other part that plays in the bar is
 * a ring around it, a bar once round, its onsets as beads (larger the louder), the hand of the playhead sweeping over
 * them. A layer on the sixteen-step matrix shows the same beads bar after bar; a polymeter or a slipping layer's beads
 * move on from bar to bar -- they precess, visibly. Where three rings or more have an onset on the same sixteenth, the
 * corona lights up at that angle: a conjunction.
 *
 * **The grid** beside it: the same bar as sixteen steps per part, brighter the louder, and whether a step is shifted off
 * the grid (swing, feel) as a tick.
 */
#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

/** @brief The Patterns tab. */
class EclipsePage final : public juce::Component, private juce::Timer {
public:
    explicit EclipsePage(TotalityProcessor& p);
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;
    /** @brief The onsets of the bar at @p beat: per part, (position in the bar 0..1, velocity). */
    void gather(double beat);
    TotalityProcessor& proc_;
    int version_ = -1;
    Playing playing_;
    struct Ring { int part; std::vector<std::pair<float, float>> onsets; };
    std::vector<Ring> rings_;
    int bar_ = -1, deck_ = 0;
    std::vector<float> kick_;   ///< the kick's onsets in the bar (0..1)
};
