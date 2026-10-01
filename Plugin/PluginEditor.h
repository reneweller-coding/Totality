/**
 * @file PluginEditor.h
 * @brief The plugin's panel (PLAN 10.1): the track or set on top, the instrument in tabs below.
 *
 * Top, the shared frame's header (Frame.h, 01.10.2026): style, key, scale, a single track or a DJ mix (a set) and its
 * length, compose, a new seed, play, mute; the ratings, the status and the rerolls, then undo, redo, help and the
 * settings; the arrange strip -- a set's tracks on their decks, a track's blocks, a lane per group of layers lit where
 * it plays, the playhead, click to jump, the wheel to zoom. Below, the tabs of PLAN
 * 10.1: Set, Arrange, Patterns (the Eclipse view), Low End, Drums, Tones, Dub, Mixer, Perform, Export, Style. The
 * parameter pages are generated from the parameter tables (EditorTheme.h, layoutOf), so a parameter that exists is on
 * the panel without anyone writing it there.
 *
 * `TOT_SHOT` (a PNG file) and `TOT_TAB` (a tab index) render the panel into a picture after the first score is composed
 * and quit the standalone -- how the layout is checked without a person looking. `TOT_SHOT_SIZE` ("1600x1000") the
 * window's size, `TOT_SHOT_AT` (a beat) jumps there first; with `TOT_PLAY` set, the meters then show that place.
 * `TOT_SHOT_ZOOM` ("from:to", beats) zooms the Arrange tab's view to that window.
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

/**
 * @brief The parameters of module instances as knobs, menus and switches, in titled groups: every control with the
 *        frame's right-click menu (MIDI learn, forget, default), a double click to its default, and the live ring.
 */
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
    /** @brief The height the page has in its window (the scrolling page around it says so before heightFor). */
    void setAvailableHeight(int h) { available_ = h; }
    /** @brief The page's parameters in words, group by group (the help's topic for the tab). */
    juce::String describe() const;

private:
    void build();
    TotalityProcessor& proc_;
    frame::ControlActions actions_;               ///< the controls' right-click menu
    frame::LiveRings live_;                       ///< where each knob's value plays
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
    /**
     * @brief A page taller than its window shows its groups in sections, one at a time (01.10.2026, frame::planSections):
     *        the sound and the modulation apart, each cut where the window ends.
     */
    frame::SectionSwitch section_{ totui::skin() };
    bool split_ = false;                          ///< more than one section
    std::vector<int> sectionOf_;                  ///< per box its section
    const std::vector<int>* measuring_ = nullptr; ///< while planning: the boxes being measured
    int available_ = 0, plannedWidth_ = -1, plannedAvailable_ = -1;
    bool shown(size_t box) const;                 ///< the box is in the section shown (or being measured)
    void plan(int width);                         ///< the sections of a page @p width wide in available_
    void applySection();                          ///< the controls of the section shown, the others hidden
    void refit();                                 ///< laid out again, and the scrolling page around it measures again
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
    const ParamPage& page() const { return *page_; }           ///< the page (the help describes it)
private:
    juce::Viewport view_;
    std::unique_ptr<ParamPage> page_;
};

/**
 * @brief What plays, across its length (PLAN 10.1, Arrange): a set's tracks as bars on their decks (their blends where
 *        two overlap), a track's blocks by their markers, the operations of the form as ticks, and a lane per group of
 *        layers (kick, hats, perc, ping, bass, pads) lit where it has notes; a ruler (a track's bars, a mix's minutes);
 *        the playhead; a click jumps. The mouse wheel zooms around the pointer, a drag (or the wheel sideways, or with
 *        Shift) moves along, a double click shows the whole length again; a zoomed view pages on with the playhead.
 */
class ArrangeView final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    /** @brief Shows @p p's score; @p detailed adds the operations' names and a larger matrix (the Arrange tab). */
    ArrangeView(TotalityProcessor& p, bool detailed);
    void paint(juce::Graphics& g) override;                     ///< the tracks or blocks, the matrix, the ruler, the playhead
    void mouseDown(const juce::MouseEvent& e) override;         ///< jumps there (zoomed: on release, if it was no drag)
    void mouseDrag(const juce::MouseEvent& e) override;         ///< moves a zoomed view along
    void mouseUp(const juce::MouseEvent& e) override;           ///< the jump of a zoomed view
    void mouseDoubleClick(const juce::MouseEvent& e) override;  ///< the whole length again
    /** @brief Zooms around the pointer; the wheel sideways, or with Shift, moves along. */
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseMagnify(const juce::MouseEvent& e, float scale) override;   ///< a trackpad's pinch zooms
    /** @brief The beats shown, @p from to @p to; false while the view shows the whole length. */
    bool zoomed(double& from, double& to) const;
    /** @brief Shows the beats @p from to @p to (once a score is there; the screenshots' TOT_SHOT_ZOOM). */
    void zoomTo(double from, double to) { wanted_ = { from, to }; }
    /** @brief Another view whose zoomed window this one marks (the strip on top marks the Arrange tab's). */
    void setDetail(const ArrangeView* detail) { detail_ = detail; }
    static constexpr int kLanes = 6;                            ///< kick, hats, perc, ping, bass (sub, bass, 303), pads
    static const char* const kLaneNames[kLanes];

private:
    /** @brief A note as the matrix draws it: where it begins and ends (beats), how loud. */
    struct Hit { double from, to; float velocity; };
    static constexpr double kNarrowest = 16.0;   ///< the least the view shows, in beats (four bars)
    void timerCallback() override;               ///< pages on with the playhead, repaints while it is shown
    void rebuild();                              ///< the hits of a new score
    void fill(double from, double to, int columns);   ///< the matrix of what is shown, a column per pixel
    void show(double from, double span);         ///< the window, kept inside the length (all of it: not zoomed)
    void zoomAround(float x, double factor);     ///< the window times @p factor, the beat under @p x staying put
    void window(double& from, double& to) const; ///< the beats shown
    double beatAt(float x) const;                ///< the beat under @p x
    TotalityProcessor& proc_;
    bool detailed_;
    int version_ = -1;                    ///< the score the hits were taken from
    Playing playing_;                     ///< a copy of what plays
    std::vector<Hit> hits_[kLanes];       ///< per lane, in the order they begin
    double longest_[kLanes] = {};         ///< a lane's longest hit (how far before a window its hits begin)
    float typical_[kLanes] = {};          ///< a lane's cover per beat where it plays: what lights it fully
    std::vector<float> cells_;            ///< kLanes x columns_, 0..1, for cellsFrom_ .. cellsTo_
    double cellsFrom_ = 0.0, cellsTo_ = 0.0;
    int columns_ = 0, cellsVersion_ = -1;
    double from_ = 0.0, span_ = 0.0;      ///< the window (span 0: the whole length)
    double lastPos_ = -1.0, lastFrom_ = 0.0, lastTo_ = 0.0;   ///< the playhead and the window at the last tick
    float downX_ = 0.0f;                  ///< where a press began
    double downFrom_ = 0.0;               ///< the window's beginning then
    bool dragged_ = false;
    const ArrangeView* detail_ = nullptr;
    std::pair<double, double> wanted_{ 0.0, 0.0 };   ///< zoomTo's window, until a score takes it
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
    ArrangeView& view() { return view_; }   ///< the large view (the strip on top marks its window)

private:
    void timerCallback() override;
    TotalityProcessor& proc_;
    ArrangeView view_;
    juce::Label which_, sounds_;
    juce::OwnedArray<juce::TextButton> rerolls_;
    juce::TextButton track_{ "reroll the whole track" }, set_{ "reroll the mix's plan" };
    juce::String prefix_;   ///< "track3." in a set, empty for a track
};

/** @brief The Export tab (PLAN 10.1): the files, what each holds, the OSC cues' settings. */
class ExportPage final : public juce::Component, private juce::Timer {
public:
    explicit ExportPage(TotalityProcessor& p);
    void resized() override;
    void paint(juce::Graphics& g) override;
    void exportWith(int extras);   ///< asks for a file, then exports (Ctrl+E: plain)
    void save();                   ///< asks for a file, then saves the set (Ctrl+S)
    void load();                   ///< asks for a set, then loads it (Ctrl+O)

private:
    void timerCallback() override;
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

/** @brief The editor: the frame's header and keys, the arrange strip, the tabs, the help over them. */
class TotalityEditor final : public juce::AudioProcessorEditor, private juce::Timer, private juce::ChangeListener {
public:
    explicit TotalityEditor(TotalityProcessor& p);           ///< builds the panel for @p p
    ~TotalityEditor() override;                           ///< stops the refresh timer
    void paint(juce::Graphics& g) override;            ///< the background
    void resized() override;                           ///< scales the body to the window
    void parentHierarchyChanged() override;            ///< the standalone's title bar gets a maximise button
    bool keyPressed(const juce::KeyPress& key) override;   ///< the frame's keys (frame::handleKey)

private:
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;   ///< the settings changed (the backdrop)
    void layoutBody();                                 ///< the top bar, the arrange strip, the tabs, at the design scale
    void toggleFullScreen();                           ///< the standalone's window full screen and back
    bool fullScreen() const;                           ///< whether it is
    bool standalone() const;                           ///< the editor sits in the standalone's window
    void showLength(bool mix);                         ///< the length slider for a track's minutes or a mix's
    void showHelp(bool on);                            ///< the help over the tabs (F1)
    void showSettings();                               ///< the settings menu
    ExportPage* exportPage() const;                    ///< the Export tab's page
    TotalityProcessor& proc_;
    frame::LookAndFeel lnf_{ totui::skin() };          ///< first, so it outlives every component that uses it
    juce::TooltipWindow tooltips_{ nullptr, 700 };
    EditorBody body_;
    frame::Backdrop backdrop_;
    int headerBottom_ = 0;                             ///< where the header ends (the backdrop is strong above)
    juce::SharedResourcePointer<UpdateCheck> updates_;
    juce::HyperlinkButton update_;
    juce::Label status_, rerolls_;
    juce::Component title_;                            ///< the name's place (drawn by the body)
    juce::Rectangle<float> logo_;
    frame::IconButton undo_{ frame::IconButton::Icon::Undo, "Undo (Ctrl+Z)" }, redo_{ frame::IconButton::Icon::Redo, "Redo (Ctrl+Y)" };
    frame::IconButton help_{ frame::IconButton::Icon::Help, "Help (F1)" }, settings_{ frame::IconButton::Icon::Settings, "Settings" };
    frame::IconButton headsetIcon_{ frame::IconButton::Icon::Headset, "A headset sends its hands: the Perform page shows them" };
    std::unique_ptr<frame::HelpView> helpView_;
    juce::ComboBox style_, key_, scale_;
    /** One track or a DJ mix (set.minutes 0 or not, TotalityProcessor::chooseMix), and the length of what is chosen. */
    juce::TextButton trackMode_{ "Track" }, mixMode_{ "DJ mix" };
    juce::Slider length_;
    juce::Label lengthLabel_;
    bool lengthOfMix_ = false;                         ///< the slider shows set.minutes (else compose.minutes)
    bool syncing_ = false;                             ///< the slider is set from its parameter, not by a hand
    juce::TextButton compose_{ "Compose track" }, seed_{ "New seed" }, play_{ "Play" }, mute_{ "Mute" };
    /** Phase 17: the ratings, as thumbs since the frame (01.10.2026). */
    frame::IconButton like_{ frame::IconButton::Icon::ThumbUp, "I like this track: its kind and its sounds come more often (with Favor Ratings)" };
    frame::IconButton dislike_{ frame::IconButton::Icon::ThumbDown, "Not this one: its kind and its sounds come less often (with Favor Ratings)" };
    juce::String rated_;                             ///< what was rated last, shown a while
    int ratedTicks_ = 0;
    std::vector<std::unique_ptr<juce::ComboBoxParameterAttachment>> combos_;
    ArrangeView arrange_;
    juce::TabbedComponent tabs_{ juce::TabbedButtonBar::TabsAtTop };
    juce::String shotPath_;
    int shotTicks_ = 0;
    bool shotHands_ = false;                           ///< TOT_SHOT_HEADSET: the headset's hands kept alive
};
