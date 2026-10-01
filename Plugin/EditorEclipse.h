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
    /** @brief The tab for @p p. */
    explicit EclipsePage(TotalityProcessor& p);
    /** @brief The eclipse of the bar that plays and the grid beside it. */
    void paint(juce::Graphics& g) override;

private:
    /** @brief Gathers the bar that plays when it changes, and repaints. */
    void timerCallback() override;
    /** @brief The onsets of the bar at @p beat: per part, (position in the bar 0..1, velocity). */
    void gather(double beat);
    TotalityProcessor& proc_;   ///< the processor: what plays, where it is
    int version_ = -1;   ///< the score playing_ was copied from
    Playing playing_;   ///< a copy of what plays
    /** @brief One part's ring: its onsets in the bar. */
    struct Ring {
        int part;                                       ///< the part (Part)
        std::vector<std::pair<float, float>> onsets;    ///< position in the bar 0..1 and velocity of each onset
    };
    std::vector<Ring> rings_;   ///< the rings of the bar shown, in part order
    int bar_ = -1;   ///< the bar shown, -1 none
    int deck_ = 0;   ///< the deck it is taken from
    std::vector<float> kick_;   ///< the kick's onsets in the bar (0..1)
};
