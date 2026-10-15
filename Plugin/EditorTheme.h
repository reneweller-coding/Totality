/**
 * @file EditorTheme.h
 * @brief The editor's look: Totality's skin of the shared frame (Frame.h), the function families, and how every
 *        module's controls fall into groups.
 *
 * **The skin** (01.10.2026, the GUIs unified): a Berlin club's concrete at night -- near black, the corona's bone white
 * for the text, the title and the accent, the onsets' red for the playhead and a mute, hard corners, the name in a
 * condensed DIN letter spaced wide, and behind the panel a total eclipse over a concrete hall (Resources/backdrop.jpg).
 * The families of function are the same on every page and in every generator, as the colour-coded panels of the
 * machines the styles were made on: gold for the sources (oscillators, engines, levels of a voice), copper for the
 * filters, sage for the envelopes, teal for everything that moves by itself (sweeps, the motion, the echo's time),
 * steel blue for the room and the mix (sends, returns, halls).
 *
 * **The groups.** A module's page is not a grid of every knob of its table but the panel of an instrument: its controls
 * in titled groups, the ones a hand goes to first as large encoders. layoutOf() says which; a parameter it does not name
 * lands in a group "More" at the end, so nothing added to a table is ever lost from the editor.
 *
 * @note After Ephemeris' Plugin/EditorTheme.h at d047d79 (27.09.2026); the look and feel is the frame's since
 *       01.10.2026, the palette and the groups are Totality's.
 * @author Rene Weller
 */
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Frame.h"
#include "tot/Params.h"
#include <vector>

namespace totui
{

/** @brief The palette. */
namespace colour
{
const juce::Colour Bg{ 0xff08080a };       ///< the window: the club at night
const juce::Colour Panel{ 0xa00e0f12 };    ///< a page (the eclipse shows faintly through it)
const juce::Colour Group{ 0xff141519 };    ///< a group box
const juce::Colour Raised{ 0xff1e1f24 };   ///< buttons, menus, the knobs' bodies
const juce::Colour Edge{ 0xff2c2d33 };     ///< hairlines
const juce::Colour Ink{ 0xffe8e5de };      ///< text: the corona's bone white
const juce::Colour Dim{ 0xff8c8b91 };      ///< names, secondary text
const juce::Colour Faint{ 0xff44454c };    ///< tracks, axes, the off state
const juce::Colour Accent{ 0xffeae3d2 };   ///< the corona: the title, the tab in front
const juce::Colour Onset{ 0xffd6553f };    ///< the onsets' red: the playhead, a mute
const juce::Colour Green{ 0xff7fd49a };    ///< a meter in range
const juce::Colour Red{ 0xffe06a5f };      ///< a meter over
}   // namespace colour

using Family = frame::FamilyE;   ///< The module families (the frame's).
/**
 * @brief The colour of a family.
 * @param[in] f  the family
 * @return its colour
 */
juce::Colour familyColour( Family f );
/**
 * @brief The colour of deck @p d (A, B, C).
 * @param[in] d  the deck
 * @return its colour
 */
juce::Colour deckColour( int d );
/**
 * @brief Totality's skin of the frame: the palette above, the eclipse behind it, the logo.
 * @return the skin
 */
const frame::Skin &skin();

/**
 * @brief One group of a module's panel: its title, its family and its parameters by key ("*cutoff": a large one,
 *        "~wave": a narrow menu).
 */
struct GroupSpec
{
    const char               *m_title;    ///< the group's title
    Family                    m_family;   ///< its family: the colour
    std::vector<const char *> m_keys;     ///< its parameters by key
};
/**
 * @brief The panel of a module: its groups, in order (empty: one group of everything).
 * @param[in] m  the module
 * @return its page's groups
 */
const std::vector<GroupSpec> &layoutOf( tot::ModuleE m );

}   // namespace totui
