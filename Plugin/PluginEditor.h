/**
 * @file PluginEditor.h
 * @brief The plugin's panel (PLAN 10.1): the track or set on top, the instrument in tabs below.
 *
 * Top: style, key, scale, the track's and the set's length, compose, a new seed, play, mute; a row of rerolls (one per
 * unit of the track under the playhead, SetFile.h) and the files; the arrange strip -- a set's tracks on their decks, a
 * track's blocks, a lane per group of layers lit where it plays, the playhead, click to jump. Below, the tabs of PLAN
 * 10.1: Set, Arrange, Patterns (the Eclipse view), Low End, Drums, Tones, Dub, Mixer, Perform, Export, Style. The
 * parameter pages are generated from the parameter tables (EditorTheme.h, layoutOf), so a parameter that exists is on
 * the panel without anyone writing it there.
 *
 * `TOT_SHOT` (a PNG file) and `TOT_TAB` (a tab index) render the panel into a picture after the first score is composed
 * and quit the standalone -- how the layout is checked without a person looking. `TOT_SHOT_SIZE` ("1600x1000") the
 * window's size, `TOT_SHOT_AT` (a beat) jumps there first; with `TOT_PLAY` set, the meters then show that place.
 *
 * @note After Ephemeris' Plugin/PluginEditor.h at d047d79 (27.09.2026).
 */
#pragma once
#include "PluginProcessor.h"
#include "EditorTheme.h"
#include "UpdateCheck.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <vector>

/**
 * @brief A synth's factory presets (tot/Presets.h): a menu of the 1024 in their sixteen groups, a step back and forth, and
 *        the preset the composer chose for the track that plays -- the menu follows it while you have not chosen another.
 */
class PresetBar final : public juce::Component, private juce::Timer {
public:
    PresetBar(TotalityProcessor& p, tot::Module m, int instance);
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;
    void choose(int index);
    TotalityProcessor& proc_;
    tot::Module module_;
    int instance_;
    int shown_ = -2;   ///< the composer's preset the label shows
    juce::ComboBox menu_;
    juce::TextButton prev_{ "<" }, next_{ ">" };
    juce::Label composed_;
};

/** @brief The parameters of module instances as knobs, menus and switches, in titled groups. */
class ParamPage final : public juce::Component {
public:
    /**
     * @brief A page for the modules in @p groups.
     * @param p         the processor
     * @param groups    module and instance pairs, shown one after the other
     * @param instances how many instances the modules have; above one, a selector picks the instance
     * @param names     the instances' names for the selector (empty: 1, 2, 3 ...)
     */
    ParamPage(TotalityProcessor& p, std::vector<std::pair<tot::Module, int>> groups, int instances = 1,
              std::vector<juce::String> names = {});
    void resized() override;                  ///< lays the groups out, flowing across the page
    void paint(juce::Graphics& g) override;   ///< the group boxes and their titles
    /** @brief The height the page needs at @p width (for a page in a viewport). */
    int heightFor(int width) const;

private:
    void build();
    TotalityProcessor& proc_;
    std::vector<std::pair<tot::Module, int>> groups_;
    int instances_;
    juce::ComboBox instance_;
    juce::OwnedArray<juce::Component> controls_;
    juce::OwnedArray<juce::Label> labels_;
    /** @brief A control of the page: its name above it, large or not (EditorTheme.h, layoutOf). */
    struct Cell {
        int control = -1;          ///< index into controls_ and labels_
        bool big = false;          ///< a large encoder
        bool narrow = false;       ///< a narrow menu
        int kind = 0;              ///< 0 a knob, 1 a menu, 2 a switch, 4 a preset bar
        juce::Rectangle<int> bounds;
    };
    /** @brief A titled group of cells, drawn as a box. */
    struct Box {
        juce::String title;
        juce::Colour colour;
        std::vector<Cell> cells;
        juce::Rectangle<int> bounds;
    };
    std::vector<Box> boxes_;
    int top() const;   ///< height of the instance bar
    /** @brief Places the boxes and their cells in @p area (@p apply: move the components too); returns the height used. */
    int layoutBoxes(juce::Rectangle<int> area, bool apply);
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> sliders_;
    std::vector<std::unique_ptr<juce::ComboBoxParameterAttachment>> combos_;
    std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> buttons_;
};

/** @brief A component in a viewport: it scrolls when it needs more height than the tab gives. */
class ScrollingPage final : public juce::Component {
public:
    explicit ScrollingPage(std::unique_ptr<ParamPage> page);   ///< takes the page over
    void resized() override;                                   ///< the page as wide as the view, as tall as it needs
private:
    juce::Viewport view_;
    std::unique_ptr<ParamPage> page_;
};

/**
 * @brief What plays, across its length (PLAN 10.1, Arrange): a set's tracks as bars on their decks (their blends where
 *        two overlap), a track's blocks by their markers, the operations of the form as ticks, and a lane per group of
 *        layers (kick, hats, perc, ping, bass, pads) lit where it has notes; the playhead; a click jumps.
 */
class ArrangeView final : public juce::Component {
public:
    /** @brief Shows @p p's score; @p detailed adds the operations' names and a larger matrix (the Arrange tab). */
    ArrangeView(TotalityProcessor& p, bool detailed) : proc_(p), detailed_(detailed) {}
    void paint(juce::Graphics& g) override;                     ///< the tracks or blocks, the matrix, the playhead
    void mouseDown(const juce::MouseEvent& e) override;         ///< jumps to the clicked position
    static constexpr int kLanes = 6;                            ///< kick, hats, perc, ping, bass (sub, bass, 303), pads
    static const char* const kLaneNames[kLanes];

private:
    static constexpr int kBins = 600;     ///< columns of the matrix across the length
    void rebuild();                       ///< the matrix of a new score
    TotalityProcessor& proc_;
    bool detailed_;
    int version_ = -1;                    ///< the score the matrix was built from
    Playing playing_;                     ///< a copy of what plays
    std::vector<float> lanes_;            ///< kLanes x kBins activity, 0..1
};

/**
 * @brief The Arrange tab: the arrangement large, and the rerolls of the track under the playhead -- each unit on its own
 *        stream (SetFile.h), so a reroll changes that and nothing else.
 */
class ArrangePage final : public juce::Component, private juce::Timer {
public:
    explicit ArrangePage(TotalityProcessor& p);
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;
    TotalityProcessor& proc_;
    ArrangeView view_;
    juce::Label which_, sounds_;
    juce::OwnedArray<juce::TextButton> rerolls_;
    juce::TextButton track_{ "reroll the whole track" }, set_{ "reroll the set's plan" };
    juce::String prefix_;   ///< "track3." in a set, empty for a track
};

/** @brief The Export tab (PLAN 10.1): the files, what each holds, the OSC cues' settings. */
class ExportPage final : public juce::Component, private juce::Timer {
public:
    explicit ExportPage(TotalityProcessor& p);
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;
    void exportWith(int extras);
    TotalityProcessor& proc_;
    juce::TextButton wav_{ "WAV + MIDI + cues" }, stems_{ "... with stems" }, loops_{ "... with DJ loops" }, all_{ "... with both" };
    juce::TextButton save_{ "Save .totset" }, load_{ "Load .totset" };
    juce::Label status_;
    std::unique_ptr<ParamPage> cue_;
    std::unique_ptr<juce::FileChooser> chooser_;
};

/**
 * @brief The editor's body: everything, drawn at the design size and scaled to the window (as Phosphene's and
 *        Ephemeris'), so a maximised or full-screen window shows the panel larger rather than emptier.
 */
class EditorBody final : public juce::Component {
public:
    std::function<void(juce::Graphics&)> painter;   ///< draws the background and the logo
    std::function<void()> onResize;                 ///< lays the controls out
    void paint(juce::Graphics& g) override { if (painter) painter(g); }
    void resized() override { if (onResize) onResize(); }
};

/** @brief The logo (Deploy/make_icon.py, drawn as vectors): the moon's dark disc, its rim, the corona in streamers. */
void drawLogo(juce::Graphics& g, juce::Rectangle<float> r);

/** @brief The editor. */
class TotalityEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit TotalityEditor(TotalityProcessor& p);           ///< builds the panel for @p p
    ~TotalityEditor() override;                           ///< stops the refresh timer
    void paint(juce::Graphics& g) override;            ///< the background
    void resized() override;                           ///< scales the body to the window
    void parentHierarchyChanged() override;            ///< the standalone's title bar gets a maximise button
    bool keyPressed(const juce::KeyPress& key) override;   ///< F11: full screen (the standalone), Esc leaves it

private:
    void timerCallback() override;
    void layoutBody();                                 ///< the top bar, the arrange strip, the tabs, at the design scale
    void toggleFullScreen();                           ///< the standalone's window full screen and back
    TotalityProcessor& proc_;
    totui::LookAndFeel lnf_;                           ///< first, so it outlives every component that uses it
    juce::TooltipWindow tooltips_{ nullptr, 700 };
    EditorBody body_;
    juce::TextButton full_{ "Full screen" };
    juce::SharedResourcePointer<UpdateCheck> updates_;
    juce::HyperlinkButton update_;
    juce::ToggleButton checkUpdates_{ "Update check" };
    juce::Label title_, status_, rerolls_;
    juce::Rectangle<float> logo_;
    juce::ComboBox style_, key_, scale_;
    juce::Slider minutes_, setMinutes_;
    juce::Label minutesLabel_, setLabel_;
    juce::TextButton compose_{ "Compose" }, seed_{ "New seed" }, play_{ "Play" }, mute_{ "Mute" };
    std::vector<std::unique_ptr<juce::ComboBoxParameterAttachment>> combos_;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> sliders_;
    ArrangeView arrange_;
    juce::TabbedComponent tabs_{ juce::TabbedButtonBar::TabsAtTop };
    juce::String shotPath_;
    int shotTicks_ = 0;
};
