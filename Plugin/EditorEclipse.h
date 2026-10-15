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
 * @author Rene Weller
 */
#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

/** @brief The Patterns tab. */
class EclipsePage final : public juce::Component, private juce::Timer
{
public:
    /**
     * @brief The tab for @p p.
     * @param[in,out] p  the processor
     */
    explicit EclipsePage( TotalityProcessor &p );
    /**
     * @brief The eclipse of the bar that plays and the grid beside it.
     * @param[in,out] g  the graphics context to draw into
     */
    void paint( juce::Graphics &g ) override;

private:
    /** @brief Gathers the bar that plays when it changes, and repaints. */
    void timerCallback() override;
    /**
     * @brief The onsets of the bar at @p beat: per part, (position in the bar 0..1, velocity).
     * @param[in] beat  a position in the bar
     */
    void gather( double beat );
    TotalityProcessor &m_proc;           ///< the processor: what plays, where it is
    int                m_version = -1;   ///< the score m_playing was copied from
    Playing            m_playing;        ///< a copy of what plays
    /** @brief One part's ring: its onsets in the bar. */
    struct Ring
    {
        int                                  m_part;     ///< the part (Part)
        std::vector<std::pair<float, float>> m_onsets;   ///< position in the bar 0..1 and velocity of each onset
    };
    std::vector<Ring>  m_rings;      ///< the rings of the bar shown, in part order
    int                m_bar = -1;   ///< the bar shown, -1 none
    int                m_deck = 0;   ///< the deck it is taken from
    std::vector<float> m_kick;       ///< the kick's onsets in the bar (0..1)
};
