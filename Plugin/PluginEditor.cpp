/**
 * @file PluginEditor.cpp
 * @brief The plugin's panel.
 */
#include "PluginEditor.h"
#include "EditorEclipse.h"
#include "EditorPerform.h"
#include "EditorStyle.h"
#include "tot/Presets.h"
#include "TotalityData.h"
#include <cstdlib>

using namespace tot;

namespace {

const juce::Colour kBack = totui::colour::bg;           ///< the skin's background
const juce::Colour kPanel = totui::colour::panel;       ///< the skin's panel
const juce::Colour kInk = totui::colour::ink;           ///< the skin's ink
const juce::Colour kDim = totui::colour::dim;           ///< the skin's dim ink
const juce::Colour kAccent = totui::colour::accent;     ///< the skin's accent
const juce::Colour kOnset = totui::colour::onset;       ///< the onsets' red

/** @brief Colour of a block by its marker: the edges dark, the body brighter towards the peak, a reduction the motion's teal. */
juce::Colour blockColour(const juce::String& name)
{
    using totui::Family;
    auto tone = [](Family f, float k) { return totui::familyColour(f).interpolatedWith(totui::colour::bg, k); };
    if (name.startsWith("Intro") || name.startsWith("Outro")) return tone(Family::Space, 0.72f);
    if (name.startsWith("Reduction") || name.contains("kick out")) return tone(Family::Motion, 0.62f);
    if (name.startsWith("Return")) return tone(Family::Filter, 0.6f);
    if (name.startsWith("Peak")) return tone(Family::Filter, 0.5f);
    return tone(Family::Source, 0.72f);
}

/** @brief The documents folder of Totality. */
juce::File documents() { return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Totality"); }

/** @brief The small tabs of a page: the page itself, or the first SubTabs inside it (the Mixer's MixerPage holds its). */
frame::SubTabs* findSubTabs(juce::Component* c)
{
    if (c == nullptr) return nullptr;
    if (auto* subs = dynamic_cast<frame::SubTabs*>(c)) return subs;
    for (auto* child : c->getChildren())
        if (auto* subs = findSubTabs(child)) return subs;
    return nullptr;
}

} // namespace

// ---------------------------------------------------------------------------------------------------

PresetBar::PresetBar(TotalityProcessor& p, Module m, int instance) : proc_(p), module_(m), instance_(instance)
{
    const std::vector<SoundPreset>& list = factoryPresets(m);
    juce::PopupMenu* root = menu_.getRootMenu();
    for (size_t g = 0; g < list.size(); g += 64) {
        juce::PopupMenu sub;
        for (size_t i = g; i < g + 64 && i < list.size(); ++i) sub.addItem(static_cast<int>(i) + 1, list[i].name);
        root->addSubMenu(list[g].group, sub);
    }
    menu_.setTextWhenNothingSelected(juce::String(static_cast<int>(list.size())) + " presets");
    menu_.onChange = [this] { if (menu_.getSelectedId() > 0) proc_.applyPreset(module_, instance_, menu_.getSelectedId() - 1); };
    prev_.onClick = [this] { const int n = static_cast<int>(factoryPresets(module_).size()); choose(((std::max(1, menu_.getSelectedId()) - 2) + n) % n); };
    next_.onClick = [this] { const int n = static_cast<int>(factoryPresets(module_).size()); choose(menu_.getSelectedId() % n); };
    prev_.setTooltip("the preset before");
    next_.setTooltip("the next preset");
    composed_.setColour(juce::Label::textColourId, totui::colour::dim);
    composed_.setTooltip("The preset the composer chose for this synth in the track that plays (compose.pick_sounds; reroll sounds "
                         "draws others). Its values stand on the knobs; turn one and the sound follows.");
    for (juce::Component* c : { static_cast<juce::Component*>(&menu_), static_cast<juce::Component*>(&prev_), static_cast<juce::Component*>(&next_),
                                static_cast<juce::Component*>(&composed_) })
        addAndMakeVisible(c);
    startTimerHz(4);
}

void PresetBar::choose(int index)
{
    menu_.setSelectedId(index + 1, juce::dontSendNotification);
    proc_.applyPreset(module_, instance_, index);
}

void PresetBar::timerCallback()
{
    const int index = proc_.composedPreset(module_, instance_);
    if (index == shown_) return;
    shown_ = index;
    const std::vector<SoundPreset>& list = factoryPresets(module_);
    if (index >= 0 && index < static_cast<int>(list.size())) {
        composed_.setText("this track: " + juce::String(list[static_cast<size_t>(index)].name) + " (" + list[static_cast<size_t>(index)].group + ")",
                          juce::dontSendNotification);
        menu_.setSelectedId(index + 1, juce::dontSendNotification);   // what plays, until another is chosen
    } else {
        composed_.setText("this track: the knobs' own sound", juce::dontSendNotification);
    }
}

void PresetBar::resized()
{
    auto r = getLocalBounds().reduced(4, 0);
    auto row = r.removeFromTop(28);
    prev_.setBounds(row.removeFromLeft(28));
    row.removeFromLeft(4);
    menu_.setBounds(row.removeFromLeft(280));
    row.removeFromLeft(4);
    next_.setBounds(row.removeFromLeft(28));
    row.removeFromLeft(10);
    composed_.setBounds(row);
}

void PresetBar::paint(juce::Graphics&) {}

// ---------------------------------------------------------------------------------------------------

ParamPage::ParamPage(TotalityProcessor& p, std::vector<std::pair<Module, int>> groups, int instances, std::vector<juce::String> names)
    : proc_(p), live_([this](int id) { return proc_.playedNormalised(id); }), groups_(std::move(groups)), instances_(instances)
{
    actions_.learn = [this](int id) { proc_.learn(id); };
    actions_.controllerFor = [this](int id) { return proc_.controllerFor(id); };
    actions_.forget = [this](int id) { proc_.forget(id); };
    actions_.reset = [this](int id) { proc_.resetToDefault(id); };
    actions_.describe = [this](int id) { return juce::String(proc_.store().desc(id).name) + "  (" + proc_.store().key(id) + ")"; };
    if (instances_ > 1) {
        for (int i = 0; i < instances_; ++i)
            instance_.addItem(i < static_cast<int>(names.size()) ? names[static_cast<size_t>(i)] : juce::String(i + 1), i + 1);
        instance_.setSelectedId(1, juce::dontSendNotification);
        instance_.onChange = [this] { build(); resized(); repaint(); };
        addAndMakeVisible(instance_);
    }
    section_.onChange = [this](int) { applySection(); refit(); };
    addChildComponent(section_);
    build();
}

void ParamPage::build()
{
    live_.clear();
    sliders_.clear();
    combos_.clear();
    buttons_.clear();
    controls_.clear();
    labels_.clear();
    boxes_.clear();
    const int inst = instances_ > 1 ? instance_.getSelectedId() - 1 : 0;
    ParamStore& s = proc_.store();
    // A control's name in its box, without the box's title in front.
    auto shortName = [](const juce::String& name, const juce::String& title) {
        if (name.startsWith(title + " ")) return name.substring(title.length() + 1);
        return name;
    };
    auto make = [&](int id, juce::Colour colour, bool big, const juce::String& title = {}, bool narrow = false) {
        StoreParameter* param = proc_.parameter(id);
        if (param == nullptr) return Cell{};
        const ParamDesc& d = s.desc(id);
        auto* label = labels_.add(new juce::Label({}, title.isEmpty() ? juce::String(d.name) : shortName(d.name, title)));
        label->setJustificationType(juce::Justification::centred);
        label->setColour(juce::Label::textColourId, big ? totui::colour::ink : totui::colour::dim);
        label->setFont(juce::FontOptions(big ? 13.0f : 12.0f));
        label->setMinimumHorizontalScale(0.75f);
        addAndMakeVisible(label);
        Cell cell;
        cell.big = big;
        if (d.curve == Curve::Choice && d.choices != nullptr) {
            auto* box = new frame::Choice(&actions_, id);
            for (int c = 0; c <= static_cast<int>(d.maxValue); ++c) box->addItem(d.choices[c], c + 1);
            box->setColour(juce::ComboBox::arrowColourId, colour);
            controls_.add(box);
            combos_.push_back(std::make_unique<juce::ComboBoxParameterAttachment>(*param, *box));
            cell.kind = 1;
            cell.narrow = narrow;
        } else if (d.curve == Curve::Toggle) {
            auto* b = new frame::Switch(&actions_, id);
            b->setColour(juce::ToggleButton::tickColourId, colour);
            controls_.add(b);
            buttons_.push_back(std::make_unique<juce::ButtonParameterAttachment>(*param, *b));
            cell.kind = 2;
        } else {
            auto* sl = new frame::Knob(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow, &actions_, id);
            sl->setTextBoxStyle(juce::Slider::TextBoxBelow, false, big ? 96 : 70, 16);
            sl->setColour(juce::Slider::rotarySliderFillColourId, colour);
            sl->setTextValueSuffix(d.unit[0] != 0 ? juce::String(" ") + d.unit : juce::String());
            sl->setTooltip(juce::String(s.key(id)) + ": " + d.name + (d.unit[0] != 0 ? juce::String(" (") + d.unit + ")" : juce::String()));
            controls_.add(sl);
            sliders_.push_back(std::make_unique<juce::SliderParameterAttachment>(*param, *sl));
            sl->setDoubleClickReturnValue(true, d.defValue);
            live_.add(*sl, id);
        }
        addAndMakeVisible(controls_.getLast());
        cell.control = controls_.size() - 1;
        return cell;
    };
    for (const auto& g : groups_) {
        const int count = ParamStore::moduleCount(g.first);
        const int instance = instances_ > 1 ? inst : g.second;
        std::vector<std::string> keys(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            const std::string& k = s.key(s.id(g.first, instance, i));
            keys[static_cast<size_t>(i)] = k.substr(k.find('.') + 1);
        }
        std::vector<bool> placed(static_cast<size_t>(count), false);
        // A synth's presets first (Presets.h): the 1024 and the one the composer chose for the track that plays.
        if (hasPresets(g.first)) {
            Box box;
            const std::string& k0 = s.key(s.id(g.first, instance, 0));
            box.title = juce::String(k0.substr(0, k0.find('.'))) + " preset";
            box.colour = totui::familyColour(totui::Family::Source);
            auto* bar = new PresetBar(proc_, g.first, instance);
            controls_.add(bar);
            addAndMakeVisible(bar);
            addChildComponent(labels_.add(new juce::Label()));
            Cell c;
            c.kind = 4;
            c.control = controls_.size() - 1;
            box.cells.push_back(c);
            boxes_.push_back(std::move(box));
        }
        // A module with instances shown beside others (the decks): its groups carry the instance's name.
        const juce::String suffix = instances_ <= 1 && g.first == Module::Deck ? juce::String(" ") + juce::String::charToString(static_cast<juce::juce_wchar>('A' + instance)) : juce::String();
        for (const totui::GroupSpec& spec : totui::layoutOf(g.first)) {
            Box box;
            box.title = juce::String(spec.title) + suffix;
            box.colour = g.first == Module::Deck ? totui::deckColour(instance) : totui::familyColour(spec.family);
            for (const char* key : spec.keys) {
                const bool big = key[0] == '*', narrow = key[0] == '~';
                const std::string name = big || narrow ? key + 1 : key;
                for (int i = 0; i < count; ++i) {
                    if (placed[static_cast<size_t>(i)] || keys[static_cast<size_t>(i)] != name) continue;
                    const Cell c = make(s.id(g.first, instance, i), box.colour, big, spec.title, narrow);
                    if (c.control >= 0) box.cells.push_back(c);
                    placed[static_cast<size_t>(i)] = true;
                }
            }
            if (!box.cells.empty()) boxes_.push_back(std::move(box));
        }
        // Whatever the panel does not name, so nothing added to a table is lost from the editor.
        Box more;
        more.title = totui::layoutOf(g.first).empty() ? juce::String("Settings") : juce::String("More");
        more.colour = totui::familyColour(totui::Family::Space);
        for (int i = 0; i < count; ++i) {
            if (placed[static_cast<size_t>(i)]) continue;
            const Cell c = make(s.id(g.first, instance, i), more.colour, false);
            if (c.control >= 0) more.cells.push_back(c);
        }
        if (!more.cells.empty()) boxes_.push_back(std::move(more));
    }
    // The sections are planned again for the new boxes (plan(), when the page is measured next).
    plannedWidth_ = -1;
    sectionOf_.assign(boxes_.size(), 0);
    split_ = false;
    section_.setVisible(false);
    applySection();
    if (getWidth() > 0) refit();
    frame::keepKeysForEditor(*this);   // the keys stay the editor's (Space plays, not the switch last clicked)
}

bool ParamPage::shown(size_t box) const
{
    if (measuring_ != nullptr) return std::find(measuring_->begin(), measuring_->end(), static_cast<int>(box)) != measuring_->end();
    return !split_ || (box < sectionOf_.size() && sectionOf_[box] == section_.current());
}

void ParamPage::plan(int width)
{
    if (width == plannedWidth_ && available_ == plannedAvailable_) return;
    plannedWidth_ = width;
    plannedAvailable_ = available_;
    juce::StringArray titles;
    for (const Box& b : boxes_) titles.add(b.title);
    // A section's height as the page lays it out: its boxes alone, the row the switch needs, the margins.
    auto height = [this, width](const std::vector<int>& set) {
        const std::vector<Box> kept = boxes_;
        measuring_ = &set;
        const int h = layoutBoxes({ 10, 10, width - 20, 100 }, false);
        measuring_ = nullptr;
        boxes_ = kept;
        return 20 + 32 + h + 10;
    };
    const frame::SectionPlan p = frame::planSections(titles, available_, height);
    sectionOf_.assign(boxes_.size(), 0);
    for (size_t s = 0; s < p.groups.size(); ++s)
        for (int b : p.groups[s]) sectionOf_[static_cast<size_t>(b)] = static_cast<int>(s);
    split_ = p.groups.size() > 1;
    if (split_) section_.setNames(p.names);
    section_.setVisible(split_);
    applySection();
}

void ParamPage::applySection()
{
    for (size_t b = 0; b < boxes_.size(); ++b) {
        const Box& box = boxes_[b];
        const bool on = shown(b);
        for (const Cell& c : box.cells) {
            controls_[c.control]->setVisible(on);
            if (c.kind != 4) labels_[c.control]->setVisible(on);
        }
    }
}

void ParamPage::refit()
{
    resized();
    repaint();
    // The page that hosts this one measures again: the section shown is shorter or taller than the other.
    for (auto* p = getParentComponent(); p != nullptr; p = p->getParentComponent())
        if (dynamic_cast<juce::Viewport*>(p) != nullptr) {
            if (auto* host = p->getParentComponent()) host->resized();
            break;
        }
}

int ParamPage::top() const { return instances_ > 1 || split_ ? 32 : 0; }

juce::String ParamPage::describe() const
{
    juce::String t;
    const ParamStore& s = proc_.store();
    for (const Box& box : boxes_) {
        t << box.title.toUpperCase() << "\n";
        for (const Cell& c : box.cells) {
            if (c.kind == 4) { t << "  the presets: the 1024 of this synth in sixteen groups, and the composer's choice\n"; continue; }
            int id = -1;
            if (auto* k = dynamic_cast<frame::Knob*>(controls_[c.control])) id = k->id();
            else if (auto* m = dynamic_cast<frame::Choice*>(controls_[c.control])) id = m->id();
            else if (auto* w = dynamic_cast<frame::Switch*>(controls_[c.control])) id = w->id();
            if (id < 0) continue;
            const ParamDesc& d = s.desc(id);
            t << "  " << d.name << ": ";
            if (d.curve == Curve::Toggle) t << (s.getBool(id) ? "on" : "off");
            else if (d.curve == Curve::Choice && d.choices != nullptr) t << d.choices[s.getInt(id)];
            else t << juce::String(s.get(id), 2) << (d.unit[0] != 0 ? juce::String(" ") + d.unit : juce::String())
                   << "   (" << juce::String(d.minValue, 2) << " .. " << juce::String(d.maxValue, 2) << ", default " << juce::String(d.defValue, 2) << ")";
            t << "   [" << s.key(id) << "]\n";
        }
        t << "\n";
    }
    return t.trimEnd();
}

int ParamPage::layoutBoxes(juce::Rectangle<int> area, bool apply)
{
    // The panel of an instrument: titled boxes side by side, flowing into rows across the page, every box of a row as
    // tall as the tallest; inside a box its controls in a line (wrapping where the page is narrow).
    constexpr int kTitle = 22, kPad = 8, kGap = 10;
    auto cellSize = [](const Cell& c) {
        switch (c.kind) {
        case 1: return juce::Point<int>(c.narrow ? 112 : 140, 100);
        case 2: return juce::Point<int>(92, 100);
        case 4: return juce::Point<int>(720, 34);
        default: return c.big ? juce::Point<int>(108, 136) : juce::Point<int>(82, 100);
        }
    };
    const int width = std::max(200, area.getWidth());
    int x = 0, y = 0, lineH = 0;
    std::vector<size_t> line;
    auto closeLine = [&]() {
        for (size_t b : line) boxes_[b].bounds.setHeight(lineH);
        line.clear();
    };
    for (size_t b = 0; b < boxes_.size(); ++b) {
        Box& box = boxes_[b];
        if (!shown(b)) { box.bounds = {}; continue; }
        const int inner = width - 2 * kPad;
        std::vector<std::pair<int, int>> rows;
        int rw = 0, rh = 0;
        for (const Cell& c : box.cells) {
            const auto sz = cellSize(c);
            if (rw > 0 && rw + sz.x > inner) { rows.push_back({ rw, rh }); rw = 0; rh = 0; }
            rw += sz.x;
            rh = std::max(rh, sz.y);
        }
        if (rw > 0) rows.push_back({ rw, rh });
        int bw = 0, bh = kTitle + kPad;
        for (const auto& r : rows) { bw = std::max(bw, r.first); bh += r.second; }
        bw = std::max(bw + 2 * kPad, 120);
        bh += kPad / 2;
        if (x > 0 && x + bw > width) { closeLine(); x = 0; y += lineH + kGap; lineH = 0; }
        box.bounds = { area.getX() + x, area.getY() + y, bw, bh };
        line.push_back(b);
        lineH = std::max(lineH, bh);
        if (apply) {
            int cx = 0, cy = kTitle, row = 0;
            int rowW = rows.empty() ? 0 : rows[0].first, rowH = rows.empty() ? 0 : rows[0].second;
            for (Cell& c : box.cells) {
                const auto sz = cellSize(c);
                if (cx > 0 && cx + sz.x > rowW) { cy += rowH; cx = 0; ++row; rowW = rows[static_cast<size_t>(row)].first; rowH = rows[static_cast<size_t>(row)].second; }
                const int left = box.bounds.getX() + kPad + (bw - 2 * kPad - rowW) / 2;
                c.bounds = { left + cx, box.bounds.getY() + cy + (rowH - sz.y) / 2, sz.x, sz.y };
                cx += sz.x;
                juce::Component* comp = controls_[c.control];
                juce::Label* label = labels_[c.control];
                const auto r = c.bounds.reduced(3, 2);
                label->setBounds(r.getX(), r.getY(), r.getWidth(), c.kind == 4 ? 0 : 16);
                if (c.kind == 4) comp->setBounds(c.bounds);
                else if (c.kind == 0) comp->setBounds(r.withTrimmedTop(16));
                else comp->setBounds(r.getX() + 2, r.getCentreY() - 12, r.getWidth() - 4, 24);
            }
        }
        x += bw + kGap;
    }
    closeLine();
    return y + lineH;
}

int ParamPage::heightFor(int width) const
{
    const_cast<ParamPage*>(this)->plan(width);   // the sections for this width first
    auto* self = const_cast<ParamPage*>(this);
    const std::vector<Box> kept = boxes_;
    const int h = self->layoutBoxes({ 10, 10, width - 20, 100 }, false);
    self->boxes_ = kept;
    return 20 + top() + h + 10;
}

void ParamPage::paint(juce::Graphics& g)
{
    for (const Box& box : boxes_) {
        if (!shown(static_cast<size_t>(&box - boxes_.data()))) continue;
        const auto r = box.bounds.toFloat();
        g.setColour(totui::colour::group);
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(totui::colour::edge);
        g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
        g.setColour(box.colour);
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.drawText(box.title.toUpperCase(), box.bounds.getX() + 10, box.bounds.getY() + 4, box.bounds.getWidth() - 20, 14,
                   juce::Justification::centredLeft);
        g.setColour(box.colour.withAlpha(0.45f));
        g.fillRect(r.getX() + 10.0f, r.getY() + 19.0f, r.getWidth() - 20.0f, 1.0f);
    }
}

void ParamPage::resized()
{
    auto area = getLocalBounds().reduced(10);
    if (instances_ > 1 || split_) {
        auto top = area.removeFromTop(26);
        if (split_) { section_.setBounds(top.removeFromRight(section_.bestWidth())); top.removeFromRight(10); }
        instance_.setBounds(top.removeFromLeft(160));
    }
    area.removeFromTop(6);
    layoutBoxes(area, true);
}

// ---------------------------------------------------------------------------------------------------

ScrollingPage::ScrollingPage(std::unique_ptr<ParamPage> page) : page_(std::move(page))
{
    view_.setViewedComponent(page_.get(), false);
    view_.setScrollBarsShown(true, false);
    addAndMakeVisible(view_);
}

void ScrollingPage::resized()
{
    view_.setBounds(getLocalBounds());
    page_->setAvailableHeight(getHeight());   // the sections are cut to it (01.10.2026)
    int w = getWidth(), h = page_->heightFor(w);
    if (h > getHeight()) { w -= view_.getScrollBarThickness(); h = page_->heightFor(w); }
    page_->setSize(w, std::max(h, getHeight()));
}

// ---------------------------------------------------------------------------------------------------

const char* const ArrangeView::kLaneNames[kLanes] = { "kick", "hats", "perc", "ping", "bass", "pads" };

ArrangeView::ArrangeView(TotalityProcessor& p, bool detailed) : proc_(p), detailed_(detailed)
{
    setTooltip("Click: jump there. Mouse wheel: zoom in and out around the pointer; drag, or Shift + wheel: move along; "
               "double click: the whole length.");
    startTimerHz(20);
}

void ArrangeView::rebuild()
{
    const bool wasMix = playing_.isSet;
    proc_.copyPlaying(playing_);
    if (playing_.isSet != wasMix) { from_ = 0.0; span_ = 0.0; }   // a mix after a track (or back): the whole of it
    for (int l = 0; l < kLanes; ++l) { hits_[l].clear(); longest_[l] = 0.0; typical_[l] = 1.0f; }
    cellsVersion_ = -1;
    const double len = playing_.set.lengthBeats;
    if (len <= 0.0) return;
    // Which percussion lanes are hats (their role, as the deck sorts them onto its buses).
    const ParamStore& s = proc_.store();
    bool hat[kPercLanes] = {};
    for (int l = 0; l < kPercLanes; ++l) {
        const PercRole r = static_cast<PercRole>(s.getInt(s.id(Module::Perc, l, perc::Role)));
        hat[l] = r == PercRole::ClosedHat || r == PercRole::RollingHat || r == PercRole::OpenHat || r == PercRole::Ride || r == PercRole::Shaker;
    }
    for (const Score& deck : playing_.set.decks) {
        for (const NoteEvent& n : deck.notes) {
            int lane = -1;
            const int part = static_cast<int>(n.part);
            if (n.part == Part::Kick) lane = 0;
            else if (part >= static_cast<int>(Part::Perc1) && part <= static_cast<int>(Part::Perc12)) lane = hat[part - static_cast<int>(Part::Perc1)] ? 1 : 2;
            else if (n.part == Part::Ping) lane = 3;
            else if (n.part == Part::Sub || n.part == Part::Bass || n.part == Part::Acid) lane = 4;
            else if (n.part == Part::Chord || n.part == Part::Drone || n.part == Part::Texture) lane = 5;
            if (lane < 0 || n.velocity <= 0.0f) continue;
            // The drums as short strokes, so their hits stand apart zoomed in; the rest as long as it sounds.
            const double length = lane < 3 ? 0.2 : std::max(n.length, 0.125);
            hits_[lane].push_back({ n.beat, n.beat + length, std::min(1.0f, n.velocity) });
        }
    }
    for (int l = 0; l < kLanes; ++l) {
        std::vector<Hit>& h = hits_[l];
        std::sort(h.begin(), h.end(), [](const Hit& a, const Hit& b) { return a.from < b.from; });
        // What lights a lane fully: its cover (length times velocity) per beat over the bars it plays in -- the same at
        // any zoom, from the whole set to a single hit.
        std::vector<char> on(static_cast<size_t>(len / 4.0) + 2, 0);
        double cover = 0.0;
        int bars = 0;
        for (const Hit& x : h) {
            cover += (x.to - x.from) * x.velocity;
            longest_[l] = std::max(longest_[l], x.to - x.from);
            char& c = on[std::min(on.size() - 1, static_cast<size_t>(std::max(0.0, x.from) / 4.0))];
            if (c == 0) { c = 1; ++bars; }
        }
        typical_[l] = bars > 0 ? static_cast<float>(cover / (4.0 * bars)) : 1.0f;
    }
}

void ArrangeView::fill(double from, double to, int columns)
{
    cells_.assign(static_cast<size_t>(kLanes * columns), 0.0f);
    cellsFrom_ = from;
    cellsTo_ = to;
    columns_ = columns;
    cellsVersion_ = version_;
    const double per = (to - from) / columns;   // beats a column
    if (per <= 0.0) return;
    for (int l = 0; l < kLanes; ++l) {
        const std::vector<Hit>& h = hits_[l];
        float* row = cells_.data() + static_cast<size_t>(l * columns);
        auto it = std::lower_bound(h.begin(), h.end(), from - longest_[l], [](const Hit& x, double b) { return x.from < b; });
        for (; it != h.end() && it->from < to; ++it) {
            if (it->to <= from) continue;
            const double a = std::max(it->from, from), b = std::min(it->to, to);
            const int c0 = static_cast<int>((a - from) / per), c1 = std::min(columns - 1, static_cast<int>((b - from) / per));
            for (int c = c0; c <= c1; ++c) {
                const double cs = from + per * c;
                const double over = std::min(b, cs + per) - std::max(a, cs);
                if (over > 0.0) row[c] += static_cast<float>(over / per) * it->velocity;
            }
        }
        const float k = 0.85f / std::max(1.0e-3f, typical_[l]);
        for (int c = 0; c < columns; ++c) row[c] = std::min(1.0f, row[c] * k);
    }
}

void ArrangeView::window(double& from, double& to) const
{
    const double len = playing_.set.lengthBeats;
    if (span_ <= 0.0 || span_ >= len) { from = 0.0; to = std::max(len, 1.0); return; }
    from = std::clamp(from_, 0.0, len - span_);
    to = from + span_;
}

bool ArrangeView::zoomed(double& from, double& to) const
{
    window(from, to);
    return span_ > 0.0 && span_ < playing_.set.lengthBeats;
}

void ArrangeView::show(double from, double span)
{
    const double len = playing_.set.lengthBeats;
    if (len <= 0.0) return;
    span = std::clamp(span, std::min(len, kNarrowest), len);
    if (span >= len - 1.0e-9) { from_ = 0.0; span_ = 0.0; return; }
    span_ = span;
    from_ = std::clamp(from, 0.0, len - span);
}

void ArrangeView::zoomAround(float x, double factor)
{
    double a = 0.0, b = 0.0;
    window(a, b);
    const double t = std::clamp(static_cast<double>(x) / std::max(1, getWidth()), 0.0, 1.0);
    const double at = a + (b - a) * t, span = (b - a) * factor;
    show(at - span * t, span);
    repaint();
}

double ArrangeView::beatAt(float x) const
{
    double a = 0.0, b = 0.0;
    window(a, b);
    return a + (b - a) * std::clamp(static_cast<double>(x) / std::max(1, getWidth()), 0.0, 1.0);
}

void ArrangeView::timerCallback()
{
    if (!isShowing()) return;
    // A zoomed view pages on when the playhead runs out of it -- not when the view was moved away from the playhead.
    const double pos = proc_.positionBeats();
    double a = 0.0, b = 0.0;
    if (zoomed(a, b) && !dragged_ && a == lastFrom_ && b == lastTo_ && lastPos_ >= a && lastPos_ <= b && (pos < a || pos > b)) {
        show(pos - 0.05 * (b - a), b - a);
        window(a, b);
    }
    lastPos_ = pos;
    lastFrom_ = a;
    lastTo_ = b;
    repaint();
}

void ArrangeView::paint(juce::Graphics& g)
{
    g.fillAll(kPanel);
    if (proc_.scoreVersion() != version_) { version_ = proc_.scoreVersion(); rebuild(); }
    const double beats = playing_.set.lengthBeats;
    if (wanted_.second > wanted_.first && beats > 0.0) {
        show(wanted_.first, wanted_.second - wanted_.first);
        wanted_ = { 0.0, 0.0 };
    }
    if (beats <= 0.0) {
        g.setColour(kDim);
        g.drawText("composing ...", getLocalBounds(), juce::Justification::centred);
        return;
    }
    double a = 0.0, b = 0.0;
    const bool zoom = zoomed(a, b);
    const float w = static_cast<float>(getWidth()), h = static_cast<float>(getHeight());
    const auto xOf = [&](double beat) { return static_cast<float>((beat - a) / (b - a)) * w; };
    // A bar from x0 to x1 inside the view, and the room for its name (from the view's edge where it began before it).
    const auto clipped = [&](float x0, float x1, float y, float height) {
        return juce::Rectangle<float>(std::max(x0, -2.0f), y, std::min(x1, w + 2.0f) - std::max(x0, -2.0f), height);
    };
    const float head = detailed_ ? 44.0f : 24.0f, rule = detailed_ ? 16.0f : 12.0f, ry = h - rule;
    g.setFont(juce::FontOptions(11.0f));
    if (playing_.isSet) {
        // The tracks on their decks: A on top, B below it; a blend where two overlap.
        const float rowH = (head - 4.0f) / 2.0f;
        for (size_t i = 0; i < playing_.tracks.size(); ++i) {
            const TrackPlace& t = playing_.tracks[i];
            const float y = 2.0f + rowH * static_cast<float>(t.deck == 1 ? 1 : 0);
            const float x0 = xOf(t.start), x1 = xOf(t.end), xs = xOf(t.swapIn);
            if (x1 < 0.0f || x0 > w) continue;
            const auto r = clipped(x0 + 1.0f, x1 - 1.0f, y, rowH - 2.0f);
            g.setColour(totui::deckColour(t.deck).interpolatedWith(kBack, 0.7f));
            g.fillRect(r.withWidth(std::max(1.0f, r.getWidth())));
            g.setColour(totui::deckColour(t.deck).interpolatedWith(kBack, 0.45f));
            g.fillRect(xs, y, 2.0f, rowH - 2.0f);   // its swap: from here it owns the low end
            g.setColour(kInk);
            const juce::String name = "T" + juce::String(static_cast<int>(i) + 1) + " " + juce::String(t.info.style) + " " + juce::String(t.info.camelot);
            if (r.getWidth() > 40.0f) g.drawFittedText(name, r.reduced(4.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, 1);
        }
    } else {
        const std::vector<Marker>& markers = playing_.set.decks[0].markers;
        for (size_t i = 0; i < markers.size(); ++i) {
            const double b0 = markers[i].beat, b1 = i + 1 < markers.size() ? markers[i + 1].beat : beats;
            const float x0 = xOf(b0), x1 = xOf(b1);
            if (x1 < 0.0f || x0 > w) continue;
            const juce::String name(markers[i].text);
            const auto r = clipped(x0 + 1.0f, x1 - 1.0f, 2.0f, head - 4.0f);
            g.setColour(blockColour(name));
            g.fillRect(r.withWidth(std::max(1.0f, r.getWidth())));
            g.setColour(kInk);
            if (r.getWidth() > 30.0f) g.drawFittedText(name, r.reduced(3.0f, 2.0f).withHeight(16.0f).toNearestInt(), juce::Justification::topLeft, 1);
        }
        if (detailed_) {
            // The operations of the form, named, under the blocks.
            g.setFont(juce::FontOptions(10.0f));
            float free = 0.0f;   // where the next name may start without covering the last
            for (const BlockOp& o : playing_.set.decks[0].ops) {
                if (o.kind == OpKind::End || o.kind == OpKind::Hold) continue;
                const float x = xOf(o.beat);
                if (x < -1.0f || x > w) continue;
                g.setColour(kOnset.withAlpha(0.8f));
                g.fillRect(x, head - 18.0f, 1.5f, 16.0f);
                juce::String t = kOpNames[static_cast<int>(o.kind)];
                if (o.layer >= 0 && o.layer < kNumLayers && o.kind != OpKind::KickOut && o.kind != OpKind::Return) t << " " << kLayerNames[o.layer];
                if (x + 3.0f < free) continue;
                const float tw = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), t) + 8.0f;
                g.setColour(kDim);
                g.drawText(t, static_cast<int>(x) + 3, static_cast<int>(head - 18.0f), static_cast<int>(tw), 16, juce::Justification::centredLeft);
                free = x + 3.0f + tw;
            }
        }
    }
    // The ruler: a track's bars, a mix's minutes -- at a step that leaves the numbers room; faint lines through the lanes.
    {
        const TempoMap& tm = playing_.set.decks[0].tempo;
        const auto clock = [](double secs) {
            const int s = static_cast<int>(std::lround(secs)), hours = s / 3600, mins = (s / 60) % 60;
            return (hours > 0 ? juce::String(hours) + ":" + juce::String(mins).paddedLeft('0', 2) : juce::String(mins)) + ":"
                   + juce::String(s % 60).paddedLeft('0', 2);
        };
        const float room = detailed_ ? 80.0f : 52.0f;
        g.setFont(juce::FontOptions(detailed_ ? 10.5f : 9.0f));
        const auto tick = [&](double beat, const juce::String& label) {
            const float x = xOf(beat);
            g.setColour(kInk.withAlpha(0.07f));
            g.fillRect(x, head, 1.0f, ry - head);
            g.setColour(kDim);
            g.fillRect(x, ry, 1.0f, 4.0f);
            g.drawText(label, juce::Rectangle<float>(x + 3.0f, ry, room, rule), juce::Justification::centredLeft);
        };
        if (playing_.isSet) {
            const double s0 = tm.secondsAt(a), s1 = tm.secondsAt(b);
            static const int kClockSteps[] = { 5, 10, 15, 30, 60, 120, 300, 600, 900, 1800, 3600 };
            int step = 3600;
            for (int k : kClockSteps) if (static_cast<double>(k) * w / std::max(1.0, s1 - s0) >= room) { step = k; break; }
            for (double t = std::ceil(s0 / step) * step; t <= s1; t += step) tick(tm.beatAt(t), clock(t));
        } else {
            const double perBar = w / std::max(1.0e-9, (b - a) / 4.0);
            int step = 1;
            while (step * perBar < room && step < 4096) step *= 2;
            for (int bar = static_cast<int>(std::ceil(a / 4.0 / step)) * step; 4.0 * bar <= b; bar += step)
                tick(4.0 * bar, juce::String(bar + 1) + (detailed_ ? "   " + clock(tm.secondsAt(4.0 * bar)) : juce::String()));
        }
    }
    // The matrix: a lane per group of layers, lit where it plays (a column per pixel, like columns drawn as one).
    const float top = head + 2.0f, laneH = (ry - top - 1.0f) / static_cast<float>(kLanes);
    const int columns = std::max(1, getWidth());
    if (cellsVersion_ != version_ || cellsFrom_ != a || cellsTo_ != b || columns_ != columns) fill(a, b, columns);
    if (laneH > 2.0f) {
        const auto level = [](float v) { return v > 0.0f ? std::max(1, static_cast<int>(v * 24.0f + 0.5f)) : 0; };
        for (int l = 0; l < kLanes; ++l) {
            const float y = top + laneH * static_cast<float>(l);
            const float* row = cells_.data() + static_cast<size_t>(l * columns);
            for (int c = 0; c < columns;) {
                const int q = level(row[c]);
                int e = c + 1;
                while (e < columns && level(row[e]) == q) ++e;
                if (q > 0) {
                    g.setColour(kInk.withAlpha(0.12f + 0.5f * static_cast<float>(q) / 24.0f));
                    g.fillRect(static_cast<float>(c), y + 1.0f, static_cast<float>(e - c), laneH - 2.0f);
                }
                c = e;
            }
        }
        if (laneH >= 7.0f) {
            g.setFont(juce::FontOptions(std::min(10.0f, laneH)));
            for (int l = 0; l < kLanes; ++l) {
                const juce::Rectangle<float> r(2.0f, top + laneH * static_cast<float>(l), 34.0f, laneH);
                g.setColour(kPanel.withAlpha(0.75f));
                g.fillRect(r);
                g.setColour(kDim);
                g.drawText(kLaneNames[l], r.reduced(2.0f, 0.0f), juce::Justification::centredLeft);
            }
        }
    }
    // Zoomed: where the window lies in the whole, under the ruler.
    if (zoom) {
        g.setColour(kInk.withAlpha(0.1f));
        g.fillRect(0.0f, h - 2.0f, w, 2.0f);
        g.setColour(kAccent.withAlpha(0.85f));
        g.fillRect(w * static_cast<float>(a / beats), h - 2.0f, std::max(3.0f, w * static_cast<float>((b - a) / beats)), 2.0f);
    }
    // The window of the Arrange tab, while it is zoomed and shown, marked on the strip.
    double da = 0.0, db = 0.0;
    if (detail_ != nullptr && detail_->isShowing() && detail_->zoomed(da, db)) {
        const float x0 = xOf(da), x1 = std::max(x0 + 2.0f, xOf(db));
        g.setColour(kAccent.withAlpha(0.10f));
        g.fillRect(x0, 0.0f, x1 - x0, h);
        g.setColour(kAccent.withAlpha(0.7f));
        g.drawRect(juce::Rectangle<float>(x0, 0.0f, x1 - x0, h), 1.0f);
    }
    const float x = xOf(proc_.positionBeats());
    if (x >= -1.0f && x <= w + 1.0f) {
        g.setColour(kOnset);
        g.fillRect(x - 1.0f, 0.0f, 2.0f, h);
    }
}

void ArrangeView::mouseDown(const juce::MouseEvent& e)
{
    downX_ = e.position.x;
    dragged_ = false;
    double a = 0.0, b = 0.0;
    const bool zoom = zoomed(a, b);
    downFrom_ = a;
    if (!zoom) proc_.seekTo(beatAt(e.position.x));   // the whole length: at once, as ever
}

void ArrangeView::mouseDrag(const juce::MouseEvent& e)
{
    double a = 0.0, b = 0.0;
    if (!zoomed(a, b)) return;
    const float dx = e.position.x - downX_;
    if (!dragged_ && std::abs(dx) < 4.0f) return;
    dragged_ = true;
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    show(downFrom_ - (b - a) * dx / std::max(1, getWidth()), b - a);
    repaint();
}

void ArrangeView::mouseUp(const juce::MouseEvent& e)
{
    if (dragged_) {
        dragged_ = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        return;
    }
    double a = 0.0, b = 0.0;
    if (zoomed(a, b)) proc_.seekTo(beatAt(e.position.x));
}

void ArrangeView::mouseDoubleClick(const juce::MouseEvent&)
{
    show(0.0, 0.0);
    repaint();
}

void ArrangeView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    double a = 0.0, b = 0.0;
    window(a, b);
    // Sideways (a trackpad, a tilting wheel) or with Shift: along the length; else in and out, about 1.4 times a notch.
    const bool sideways = std::abs(wheel.deltaX) > std::abs(wheel.deltaY);
    if (sideways || e.mods.isShiftDown()) {
        const float d = sideways ? wheel.deltaX : wheel.deltaY;
        show(a - (b - a) * 0.5 * d, b - a);
        repaint();
        return;
    }
    if (wheel.deltaY != 0.0f) zoomAround(e.position.x, std::pow(2.0, -2.0 * wheel.deltaY));
}

void ArrangeView::mouseMagnify(const juce::MouseEvent& e, float scale)
{
    if (scale > 0.0f) zoomAround(e.position.x, 1.0 / scale);
}

// ---------------------------------------------------------------------------------------------------

ArrangePage::ArrangePage(TotalityProcessor& p) : proc_(p), view_(p, true)
{
    addAndMakeVisible(view_);
    which_.setColour(juce::Label::textColourId, kInk);
    which_.setMinimumHorizontalScale(0.6f);
    addAndMakeVisible(which_);
    sounds_.setColour(juce::Label::textColourId, kDim);
    sounds_.setMinimumHorizontalScale(0.7f);
    addAndMakeVisible(sounds_);
    for (const char* unit : kUnitNames) {
        // "hands" is the stream of the knobs' moves (the automation); named so on the panel, where a hand means the
        // headset's (01.10.2026). The stream keeps its name: the sets store it.
        auto* b = rerolls_.add(new juce::TextButton(juce::String("reroll ") + (juce::String(unit) == "hands" ? "moves" : unit)));
        const juce::String u(unit);
        b->onClick = [this, u] { proc_.reroll(prefix_ + u); };
        addAndMakeVisible(b);
    }
    track_.onClick = [this] { if (prefix_.isNotEmpty()) proc_.reroll(prefix_.dropLastCharacters(1)); };
    set_.onClick = [this] { proc_.reroll("set"); };
    addAndMakeVisible(track_);
    addAndMakeVisible(set_);
    startTimerHz(4);
}

void ArrangePage::timerCallback()
{
    Playing p;
    proc_.copyPlaying(p);
    const int t = proc_.trackAt(proc_.positionBeats());
    prefix_ = p.isSet && t >= 0 ? "track" + juce::String(t + 1) + "." : juce::String();
    juce::String text;
    if (t >= 0 && t < static_cast<int>(p.tracks.size())) {
        const TrackInfo& i = p.tracks[static_cast<size_t>(t)].info;
        text << (p.isSet ? "Track " + juce::String(t + 1) + " of " + juce::String(static_cast<int>(p.tracks.size())) + " in the mix: " : juce::String("The track: "))
             << juce::String(i.style) << " " << kArchetypeNames[i.archetype] << ", " << kFormNames[static_cast<int>(i.form)] << ", " << juce::String(i.bpm, 1) << " BPM, "
             << kKeyNames[i.key] << " " << kScaleNames[i.scale] << " (" << juce::String(i.camelot) << "), " << i.bars << " bars, "
             << (i.subOwns ? "the sub owns the low end" : "the rumble owns the low end") << ", bar similarity " << juce::String(i.similarity, 2);
        // Phase 8: the figure and where the waves land.
        if (i.figure >= 0)
            text << "; figure: the " << kLayerNames[i.figure] << " from bar " << (i.figureBar + 1);
        if (!i.landings.empty()) {
            text << ", waves land on";
            for (int l : i.landings) text << " " << (l + 1);
        }
    }
    which_.setText(text, juce::dontSendNotification);
    // The composer's presets of the track whose sounds the knobs show.
    juce::String sounds;
    for (Module m : { Module::Kick, Module::Rumble, Module::Sub, Module::Ping, Module::Bass, Module::Acid, Module::Chord, Module::Drone, Module::Texture }) {
        const int index = proc_.composedPreset(m, 0);
        if (index < 0) continue;
        const std::string& k0 = proc_.store().key(proc_.store().id(m, 0, 0));
        sounds << (sounds.isEmpty() ? "Sounds: " : ", ") << juce::String(k0.substr(0, k0.find('.'))) << " " << factoryPresets(m)[static_cast<size_t>(index)].name;
    }
    sounds_.setText(sounds, juce::dontSendNotification);
    track_.setVisible(p.isSet);
    set_.setVisible(p.isSet);
}

void ArrangePage::resized()
{
    auto r = getLocalBounds().reduced(10);
    which_.setBounds(r.removeFromTop(22));
    sounds_.setBounds(r.removeFromTop(20));
    r.removeFromTop(4);
    auto row = r.removeFromTop(28);
    // Nine units and the two larger ones in a row of the window's usual width (1180).
    for (auto* b : rerolls_) b->setBounds(row.removeFromLeft(94).reduced(2));
    row.removeFromLeft(8);
    track_.setBounds(row.removeFromLeft(152).reduced(2));
    set_.setBounds(row.removeFromLeft(140).reduced(2));
    r.removeFromTop(8);
    view_.setBounds(r);
}

void ArrangePage::paint(juce::Graphics&) {}

// ---------------------------------------------------------------------------------------------------

ExportPage::ExportPage(TotalityProcessor& p) : proc_(p)
{
    wav_.onClick = [this] { exportWith(0); };
    stems_.onClick = [this] { exportWith(TotalityProcessor::kStems); };
    loops_.onClick = [this] { exportWith(TotalityProcessor::kLoops); };
    all_.onClick = [this] { exportWith(TotalityProcessor::kStems | TotalityProcessor::kLoops); };
    save_.onClick = [this] { save(); };
    load_.onClick = [this] { load(); };
    save_.setTooltip("Save the set: seed, lengths, rerolls and every changed knob (Ctrl+S)");
    load_.setTooltip("Load a set and compose it (Ctrl+O)");
    wav_.setTooltip("Export what plays (Ctrl+E)");
    for (auto* b : { &wav_, &stems_, &loops_, &all_, &save_, &load_ }) addAndMakeVisible(b);
    status_.setColour(juce::Label::textColourId, kInk);
    addAndMakeVisible(status_);
    cue_ = std::make_unique<ParamPage>(proc_, std::vector<std::pair<Module, int>>{ { Module::Cue, 0 } });
    addAndMakeVisible(*cue_);
    startTimerHz(4);
}

void ExportPage::save()
{
    documents().createDirectory();
    chooser_ = std::make_unique<juce::FileChooser>("Save set", documents().getChildFile("totality.totset"), "*.totset");
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                          [this](const juce::FileChooser& fc) { if (fc.getResult() != juce::File()) proc_.saveSet(fc.getResult().withFileExtension(".totset")); });
}

void ExportPage::load()
{
    chooser_ = std::make_unique<juce::FileChooser>("Load set", documents(), "*.totset");
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this](const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) proc_.loadSet(fc.getResult()); });
}

void ExportPage::exportWith(int extras)
{
    documents().createDirectory();
    chooser_ = std::make_unique<juce::FileChooser>("Export", documents().getChildFile("totality.wav"), "*.wav");
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, extras](const juce::FileChooser& fc) {
                              if (fc.getResult() != juce::File()) proc_.exportTo(fc.getResult().withFileExtension(".wav"), extras);
                          });
}

void ExportPage::timerCallback() { status_.setText(proc_.status(), juce::dontSendNotification); }

void ExportPage::resized()
{
    auto r = getLocalBounds().reduced(16);
    auto row = r.removeFromTop(30);
    for (auto* b : { &wav_, &stems_, &loops_, &all_ }) b->setBounds(row.removeFromLeft(170).reduced(3));
    row.removeFromLeft(24);
    save_.setBounds(row.removeFromLeft(130).reduced(3));
    load_.setBounds(row.removeFromLeft(130).reduced(3));
    r.removeFromTop(6);
    status_.setBounds(r.removeFromTop(22));
    r.removeFromTop(150);   // the text (paint)
    cue_->setBounds(r.removeFromTop(cue_->heightFor(r.getWidth())).withWidth(std::min(r.getWidth(), 420)));
}

void ExportPage::paint(juce::Graphics& g)
{
    g.setColour(kDim);
    g.setFont(juce::FontOptions(13.0f));
    const juce::String text =
        "The export renders what plays as it was composed (the performer's mutes, filter and throw are live only) at 48 kHz:\n"
        "- a 24-bit WAV with its cues as markers (the bass's entry, the kick-outs and returns, the outro; in a set every track, "
        "swap and loop) and the same cues as JSON beside it (<name>.wav.cues.json),\n"
        "- the MIDI file with the tempo map and a channel per part,\n"
        "- with stems: a 32-bit WAV per element into <name>_stems; their sum is the mix before the master, exactly,\n"
        "- with DJ loops (a track): 4 and 8 bars of its loudest block, seamless, the mix and kick, hats and perc alone.\n"
        "An .totset holds the seed, the lengths, the rerolls and every changed knob.";
    g.drawFittedText(text, getLocalBounds().reduced(16).withTrimmedTop(64).withHeight(140), juce::Justification::topLeft, 8);
}

// ---------------------------------------------------------------------------------------------------

void drawLogo(juce::Graphics& g, juce::Rectangle<float> r)
{
    const float s = std::min(r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre(s, s);
    const juce::Point<float> c = r.getCentre();
    g.setColour(juce::Colour(12, 12, 15));
    g.fillRoundedRectangle(r, s / 5.0f);
    // The streamers of the corona, long along the equator (make_icon.py: streamers, layout).
    const bool small = s <= 32.0f;
    const float disc = s * (small ? 0.22f : 0.2f);
    const int count = small ? 24 : 48;
    const float width = std::max(1.0f, disc * (small ? 0.09f : 0.055f));
    const juce::Colour corona(238, 214, 168);
    for (int i = 0; i < count; ++i) {
        const float a = juce::MathConstants<float>::twoPi * static_cast<float>(i) / static_cast<float>(count);
        const float w = 0.55f + 0.45f * std::sin(7.0f * a + 1.3f) * std::sin(3.0f * a);
        const float e = std::pow(std::abs(std::cos(a)), 3.0f);
        const float length = disc * (0.18f + 0.95f * e * (0.6f + 0.4f * w) + 0.12f * w) * (small ? 0.8f : 1.0f);
        const float r0 = disc * 1.08f, r1 = r0 + length;
        g.setColour(corona.withAlpha(0.45f + 0.5f * std::abs(std::cos(a))));
        g.drawLine(c.x + r0 * std::cos(a), c.y + r0 * std::sin(a), c.x + r1 * std::cos(a), c.y + r1 * std::sin(a), width);
    }
    // The rim of light, and the moon's disc.
    g.setColour(corona);
    g.fillEllipse(c.x - disc * 1.06f, c.y - disc * 1.06f, 2.12f * disc, 2.12f * disc);
    g.setColour(juce::Colour(5, 5, 7));
    g.fillEllipse(c.x - disc, c.y - disc, 2.0f * disc, 2.0f * disc);
}

// ---------------------------------------------------------------------------------------------------

TotalityEditor::TotalityEditor(TotalityProcessor& p) : juce::AudioProcessorEditor(p), proc_(p), arrange_(p, false)
{
    setLookAndFeel(&lnf_);
    addAndMakeVisible(body_);
    // The backdrop (strong behind the header, faint behind the pages), the logo and the name: the frame's (Frame.h).
    body_.painter = [this](juce::Graphics& g) {
        backdrop_.paint(g, body_.getLocalBounds(), headerBottom_, totui::skin(), frame::Settings::of("Totality").backdrop());
        drawLogo(g, logo_);
        frame::drawTitle(g, totui::skin(), title_.getBounds().toFloat(), 21.0f);
    };
    body_.onResize = [this] { layoutBody(); };
    frame::Settings::of("Totality").addChangeListener(this);
    ParamStore& s = proc_.store();
    title_.setInterceptsMouseClicks(false, false);
    body_.addAndMakeVisible(title_);

    auto combo = [&](juce::ComboBox& box, int id) {
        const ParamDesc& d = s.desc(id);
        for (int c = 0; c <= static_cast<int>(d.maxValue); ++c) box.addItem(d.choices[c], c + 1);
        combos_.push_back(std::make_unique<juce::ComboBoxParameterAttachment>(*proc_.parameter(id), box));
        body_.addAndMakeVisible(box);
    };
    combo(style_, s.id(Module::Compose, 0, compose::Style));
    combo(key_, s.id(Module::Compose, 0, compose::Key));
    combo(scale_, s.id(Module::Compose, 0, compose::Scale));
    style_.setTooltip("The style the next track is composed in");
    key_.setTooltip("The key");
    scale_.setTooltip("The scale");
    // One track or a DJ mix (a set of tracks on two decks): two buttons that compose what they name, and one length,
    // the one of what is chosen (compose.minutes or set.minutes).
    trackMode_.setTooltip("Compose a single track (its length beside)");
    mixMode_.setTooltip("Compose a DJ mix: a set of tracks mixed on two decks (its length beside; a track's time in it and the "
                        "set's course on the Set page)");
    trackMode_.onClick = [this] { proc_.chooseMix(false); };
    mixMode_.onClick = [this] { proc_.chooseMix(true); };
    frame::connectModes({ &trackMode_, &mixMode_ }, kAccent.withAlpha(0.42f));
    for (auto* b : { &trackMode_, &mixMode_ }) body_.addAndMakeVisible(b);
    length_.setSliderStyle(juce::Slider::LinearHorizontal);
    length_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 20);
    length_.onValueChange = [this] {
        if (syncing_) return;
        const ParamStore& st = proc_.store();
        proc_.setFromUi(lengthOfMix_ ? st.id(Module::Set, 0, set::Minutes) : st.id(Module::Compose, 0, compose::Minutes),
                        static_cast<float>(length_.getValue()));
    };
    length_.onDragStart = [this] { proc_.beginStep("Length"); };   // a drag is one step, not one per pixel
    length_.onDragEnd = [this] { proc_.endStep(); };
    lengthLabel_.setText("Length", juce::dontSendNotification);
    lengthLabel_.setColour(juce::Label::textColourId, kDim);
    lengthLabel_.setJustificationType(juce::Justification::centredRight);
    body_.addAndMakeVisible(length_);
    body_.addAndMakeVisible(lengthLabel_);
    showLength(proc_.mixChosen());

    compose_.onClick = [this] { proc_.compose(); };
    seed_.onClick = [this] { proc_.newSeed(); };
    play_.onClick = [this] { proc_.setPlaying(!proc_.isPlaying()); };
    compose_.setTooltip("Compose with the knobs as they stand, the seed and the rerolls");
    seed_.setTooltip("A new seed: another track (or mix) from the same settings");
    play_.setTooltip("Play and stop (Space)");
    for (auto* b : { &compose_, &seed_, &play_ }) body_.addAndMakeVisible(b);
    // Mute, as in Phosphene: silence at the output; TOT_MUTE (or the screenshot mode) holds it on.
    mute_.setClickingTogglesState(true);
    mute_.setToggleState(proc_.muted(), juce::dontSendNotification);
    mute_.setEnabled(!proc_.muteForced() || std::getenv("TOT_SHOT") != nullptr);
    mute_.setColour(juce::TextButton::buttonOnColourId, totui::colour::red.withAlpha(0.55f));
    mute_.setTooltip(proc_.muteForced() ? "Muted by TOT_MUTE: an automated run makes no sound" : "Silence the output");
    mute_.onClick = [this] { proc_.setMuted(mute_.getToggleState()); };
    body_.addAndMakeVisible(mute_);
    play_.setColour(juce::TextButton::buttonColourId, kAccent.withAlpha(0.2f));
    compose_.setColour(juce::TextButton::buttonColourId, kAccent.withAlpha(0.12f));
    // The update check (the settings switch it): once a day it asks GitHub for the latest release; a newer one shows here.
    update_.setColour(juce::HyperlinkButton::textColourId, kAccent);
    update_.setTooltip("Open the release page");
    body_.addChildComponent(update_);
    // The tools at the right end of the second row: undo, redo, help, settings -- and the headset, while one sends.
    undo_.onClick = [this] { proc_.undo(); };
    redo_.onClick = [this] { proc_.redo(); };
    help_.onClick = [this] { showHelp(helpView_ == nullptr || !helpView_->isVisible()); };
    settings_.onClick = [this] { showSettings(); };
    headsetIcon_.onClick = [this] {
        for (int i = 0; i < tabs_.getNumTabs(); ++i)
            if (tabs_.getTabNames()[i] == "Perform") tabs_.setCurrentTabIndex(i);
    };
    for (auto* b : { &undo_, &redo_, &help_, &settings_ }) body_.addAndMakeVisible(b);
    body_.addChildComponent(headsetIcon_);
    // Phase 17: rate the track under the playhead; with Favor Ratings (the Set page) the ratings weigh what comes.
    like_.onClick = [this] { rated_ = proc_.rate(1); ratedTicks_ = 60; };
    dislike_.onClick = [this] { rated_ = proc_.rate(-1); ratedTicks_ = 60; };
    body_.addAndMakeVisible(like_);
    body_.addAndMakeVisible(dislike_);
    rerolls_.setColour(juce::Label::textColourId, kDim);
    rerolls_.setJustificationType(juce::Justification::centredRight);
    status_.setColour(juce::Label::textColourId, kInk);
    status_.setMinimumHorizontalScale(0.8f);
    body_.addAndMakeVisible(rerolls_);
    body_.addAndMakeVisible(status_);

    body_.addAndMakeVisible(arrange_);
    using M = Module;
    auto page = [&](const char* name, std::vector<std::pair<M, int>> groups, int instances = 1, std::vector<juce::String> names = {}) {
        tabs_.addTab(name, kPanel, new ScrollingPage(std::make_unique<ParamPage>(proc_, std::move(groups), instances, std::move(names))), true);
    };
    // A tab of several modules: a small tab each (frame::SubTabs, 01.10.2026), so that no page has to scroll far.
    auto modules = [&](const char* name, std::vector<std::pair<M, const char*>> mods) {
        auto st = std::make_unique<frame::SubTabs>(totui::skin());
        for (const auto& mod : mods) {
            const M m = mod.first;
            st->add(mod.second, [this, m] {
                return std::unique_ptr<juce::Component>(new ScrollingPage(std::make_unique<ParamPage>(proc_, std::vector<std::pair<M, int>>{ { m, 0 } })));
            });
        }
        tabs_.addTab(name, kPanel, st.release(), true);
    };
    page("Set", { { M::Compose, 0 }, { M::Set, 0 }, { M::DjFx, 0 } });
    auto* arrangePage = new ArrangePage(proc_);
    tabs_.addTab("Arrange", kPanel, arrangePage, true);
    arrange_.setDetail(&arrangePage->view());
    tabs_.addTab("Patterns", kPanel, new EclipsePage(proc_), true);
    modules("Low End", { { M::Kick, "Kick" }, { M::Rumble, "Rumble" }, { M::Sub, "Sub" } });
    std::vector<juce::String> lanes;
    for (int l = 0; l < kPercLanes; ++l) lanes.push_back("Lane " + juce::String(l + 1));
    page("Drums", { { M::Perc, 0 } }, kPercLanes, lanes);
    modules("Tones", { { M::Ping, "Ping" }, { M::Bass, "Bass" }, { M::Acid, "Acid" }, { M::Chord, "Chord" }, { M::Drone, "Drone" },
                       { M::Texture, "Texture" } });
    modules("Dub", { { M::Dub, "Dub" }, { M::Space, "Space" }, { M::Cloud, "Cloud" } });
    tabs_.addTab("Mixer", kPanel, new MixerPage(proc_), true);
    tabs_.addTab("Perform", kPanel, new PerformPage(proc_), true);
    tabs_.addTab("Export", kPanel, new ExportPage(proc_), true);
    tabs_.addTab("Style", kPanel, new StylePage(proc_), true);
    tabs_.setOutline(0);
    body_.addAndMakeVisible(tabs_);
    frame::keepKeysForEditor(body_);
    setWantsKeyboardFocus(true);

    setResizable(true, true);
    setResizeLimits(800, 520, 4800, 3100);
    setSize(1280, 860);   // the family's window size (01.10.2026)

    if (const char* shot = std::getenv("TOT_SHOT")) {
        shotPath_ = shot;
        if (const char* tab = std::getenv("TOT_TAB")) tabs_.setCurrentTabIndex(juce::String(tab).getIntValue());
        // TOT_SUBTAB: the small tab of a tab of several (Low End, Tones, Dub, Mixer).
        if (const char* st = std::getenv("TOT_SUBTAB"))
            if (auto* subs = findSubTabs(tabs_.getCurrentContentComponent())) subs->show(juce::String(st).getIntValue());
        if (const char* zoom = std::getenv("TOT_SHOT_ZOOM")) {
            const juce::String z(zoom);
            arrangePage->view().zoomTo(z.upToFirstOccurrenceOf(":", false, false).getDoubleValue(), z.fromFirstOccurrenceOf(":", false, false).getDoubleValue());
        }
        if (const char* size = std::getenv("TOT_SHOT_SIZE")) {
            const juce::String sz(size);
            setSize(sz.upToFirstOccurrenceOf("x", false, false).getIntValue(), sz.fromFirstOccurrenceOf("x", false, false).getIntValue());
        }
        if (std::getenv("TOT_SHOT_HELP") != nullptr) showHelp(true);
        if (std::getenv("TOT_SHOT_HEADSET") != nullptr) {   // the headset's controls in the pictures: hands as if one sent them
            frame::Hands hands;
            hands.height[0] = 0.62f;
            hands.height[1] = 0.8f;
            hands.tracked[0] = hands.tracked[1] = true;
            proc_.headset().inject(hands);
            shotHands_ = true;
        }
    }
    startTimerHz(15);
}

TotalityEditor::~TotalityEditor()
{
    stopTimer();
    frame::Settings::of("Totality").removeChangeListener(this);
    setLookAndFeel(nullptr);
}

void TotalityEditor::paint(juce::Graphics& g) { g.fillAll(kBack); }

void TotalityEditor::resized()
{
    // The body at the design size (1180 x 760), scaled to the window by its height, and as wide as the window allows.
    const float scale = juce::jlimit(0.5f, 4.0f, std::min(static_cast<float>(getWidth()) / 1180.0f, static_cast<float>(getHeight()) / 760.0f));
    body_.setTransform(juce::AffineTransform::scale(scale));
    body_.setBounds(0, 0, juce::roundToInt(static_cast<float>(getWidth()) / scale), juce::roundToInt(static_cast<float>(getHeight()) / scale));
}

void TotalityEditor::parentHierarchyChanged()
{
    // A maximise button beside the other two, on the next turn of the message loop (the standalone's window is still
    // putting its content in when this is called). A host's window finds nothing here.
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<TotalityEditor>(this)] {
        if (safe == nullptr) return;
        auto* window = safe->findParentComponentOfClass<juce::DocumentWindow>();
        if (window != nullptr)
            window->setTitleBarButtonsRequired(juce::DocumentWindow::minimiseButton | juce::DocumentWindow::maximiseButton
                                                   | juce::DocumentWindow::closeButton, false);
        safe->layoutBody();
    });
}

bool TotalityEditor::standalone() const { return findParentComponentOfClass<juce::DocumentWindow>() != nullptr; }

bool TotalityEditor::fullScreen() const
{
    auto* window = findParentComponentOfClass<juce::DocumentWindow>();
    return window != nullptr && juce::Desktop::getInstance().getKioskModeComponent() == window;
}

void TotalityEditor::toggleFullScreen()
{
    auto* window = findParentComponentOfClass<juce::DocumentWindow>();
    if (window == nullptr) return;
    juce::Desktop::getInstance().setKioskModeComponent(fullScreen() ? nullptr : window, false);
    grabKeyboardFocus();
}

ExportPage* TotalityEditor::exportPage() const
{
    for (int i = 0; i < tabs_.getNumTabs(); ++i)
        if (auto* e = dynamic_cast<ExportPage*>(tabs_.getTabContentComponent(i))) return e;
    return nullptr;
}

bool TotalityEditor::keyPressed(const juce::KeyPress& key)
{
    frame::Keys k;
    if (standalone()) {   // in a host, Space and F11 are the host's
        k.playStop = [this] { proc_.setPlaying(!proc_.isPlaying()); };
        k.fullScreen = [this] { toggleFullScreen(); };
    }
    k.undo = [this] { proc_.undo(); };
    k.redo = [this] { proc_.redo(); };
    k.help = [this] { showHelp(helpView_ == nullptr || !helpView_->isVisible()); };
    k.escape = [this] {
        if (helpView_ != nullptr && helpView_->isVisible()) showHelp(false);
        else if (fullScreen()) toggleFullScreen();
    };
    k.save = [this] { if (auto* e = exportPage()) e->save(); };
    k.open = [this] { if (auto* e = exportPage()) e->load(); };
    k.exportFile = [this] { if (auto* e = exportPage()) e->exportWith(0); };
    return frame::handleKey(key, k);
}

void TotalityEditor::showHelp(bool on)
{
    if (on && helpView_ == nullptr) {
        helpView_ = std::make_unique<frame::HelpView>(juce::String::fromUTF8(TotalityData::chapters_txt, TotalityData::chapters_txtSize), totui::skin());
        helpView_->onClose = [this] { showHelp(false); };
        helpView_->extraTopics = [this] {
            std::vector<std::pair<juce::String, juce::String>> t;
            juce::Component* page = tabs_.getCurrentContentComponent();
            juce::String name = tabs_.getCurrentTabName();
            if (auto* st = dynamic_cast<frame::SubTabs*>(page)) {   // a tab of several modules: its small tab in front
                name << ": " << st->name(st->current());
                page = st->page();
            }
            if (auto* sp = dynamic_cast<ScrollingPage*>(page)) t.emplace_back("This tab: " + name, sp->page().describe());
            t.emplace_back("Keys", frame::keysText());
            t.emplace_back("Headset (Meta Quest)", frame::headsetGrammar("the kick out and in", {}) + "\n\n" + proc_.headset().statusText());
            return t;
        };
        body_.addChildComponent(*helpView_);
    }
    help_.setToggleState(on, juce::dontSendNotification);
    if (helpView_ == nullptr) return;
    if (on) helpView_->refresh();
    helpView_->setVisible(on);
    layoutBody();
    if (on) helpView_->grabKeyboardFocus();
    else grabKeyboardFocus();
}

void TotalityEditor::showSettings()
{
    frame::SettingsMenu m;
    m.app = "Totality";
    m.version = JucePlugin_VersionString;
    m.updatesOn = [this] { return updates_->enabled(); };
    m.setUpdates = [this](bool on) { updates_->setEnabled(on); };
    m.canFullScreen = [this] { return standalone(); };
    m.isFullScreen = [this] { return fullScreen(); };
    m.toggleFullScreen = [this] { toggleFullScreen(); };
    m.setWindowScale = [this](float k) {
        if (!fullScreen()) setSize(juce::roundToInt(1280.0f * k), juce::roundToInt(860.0f * k));
    };
    m.headsetStatus = [this] { return proc_.headset().statusText(); };
    if (proc_.wrapperType == juce::AudioProcessor::wrapperType_Standalone)   // Ableton Link, the standalone only (02.10.2026)
        m.linkStatus = [this] { return proc_.linkStatus(); };
    m.about = [] { return juce::String("Hypnotic Berlin techno, composed and synthesised.\ngithub.com/reneweller-coding/Totality"); };
    m.show(settings_);
}

void TotalityEditor::changeListenerCallback(juce::ChangeBroadcaster*) { layoutBody(); body_.repaint(); }   // the settings changed

void TotalityEditor::showLength(bool mix)
{
    // A track: 2 to 16 minutes; a mix: 10 minutes to 12 hours, its first two hours over most of the way.
    const juce::ScopedValueSetter<bool> quiet(syncing_, true);
    lengthOfMix_ = mix;
    if (mix) {
        length_.setRange(10.0, 720.0, 1.0);
        length_.setSkewFactorFromMidPoint(90.0);
        length_.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v)) + " min"; };
        length_.setTooltip("The DJ mix's length (Compose mix makes it)");
    } else {
        length_.setRange(2.0, 16.0, 0.1);
        length_.setSkewFactor(1.0);
        length_.textFromValueFunction = [](double v) { return juce::String(v, 1) + " min"; };
        length_.setTooltip("The track's length (Compose track makes it)");
    }
    length_.valueFromTextFunction = [](const juce::String& t) { return t.getDoubleValue(); };
    const ParamStore& st = proc_.store();
    length_.setValue(st.get(mix ? st.id(Module::Set, 0, set::Minutes) : st.id(Module::Compose, 0, compose::Minutes)), juce::dontSendNotification);
    length_.updateText();
}

void TotalityEditor::layoutBody()
{
    auto area = body_.getLocalBounds().reduced(10);
    frame::Header h;
    h.logo = &logo_;
    h.title = &title_;
    h.titleWidth = static_cast<int>(frame::titleWidth(totui::skin(), 21.0f)) + 12;
    h.choices = { { &style_, 116 }, { &key_, 62 }, { &scale_, 116 } };
    h.modes = { { &trackMode_, 64 }, { &mixMode_, 72 } };
    h.lengthLabel = &lengthLabel_;
    h.length = &length_;
    h.actions = { { &compose_, 116 }, { &seed_, 88 } };
    h.play = &play_;
    h.mute = &mute_;
    h.like = &like_;
    h.dislike = &dislike_;
    h.status = &status_;
    h.curation = &rerolls_;
    h.update = &update_;
    h.tools = { &headsetIcon_, nullptr, &undo_, &redo_, nullptr, &help_, &settings_ };
    frame::layoutHeader(area, h);
    headerBottom_ = area.getY();
    // The overview, unless the settings fold it away (01.10.2026): then the pages get its height.
    const bool overview = frame::Settings::of("Totality").overview();
    arrange_.setVisible(overview);
    arrange_.setBounds(area.removeFromTop(overview ? 96 : 0));
    area.removeFromTop(overview ? 8 : 4);
    tabs_.setBounds(area);
    if (helpView_ != nullptr) helpView_->setBounds(area);
}

void TotalityEditor::timerCallback()
{
    if (ratedTicks_ > 0) --ratedTicks_;
    if (shotHands_) proc_.headset().inject(proc_.headset().hands());
    status_.setText(ratedTicks_ > 0 && rated_.isNotEmpty() ? rated_ : proc_.status(), juce::dontSendNotification);
    rerolls_.setText(proc_.curationText(), juce::dontSendNotification);
    {
        const juce::String v = updates_->enabled() ? updates_->newer() : juce::String();
        if (v.isNotEmpty() && update_.getButtonText() != "Version " + v + " available") {
            update_.setButtonText("Version " + v + " available");
            update_.setURL(juce::URL(updates_->page()));
        }
        if (update_.isVisible() != v.isNotEmpty()) {
            update_.setVisible(v.isNotEmpty());
            layoutBody();
        }
    }
    // Undo and redo say what they would take back; the headset's sign shows while one is there (or always, if asked).
    {
        const juce::String u = proc_.undoName(), r = proc_.redoName();
        undo_.setEnabled(u.isNotEmpty());
        redo_.setEnabled(r.isNotEmpty());
        undo_.setTooltip(u.isEmpty() ? juce::String("Undo (Ctrl+Z)") : "Undo: " + u + " (Ctrl+Z)");
        redo_.setTooltip(r.isEmpty() ? juce::String("Redo (Ctrl+Y)") : "Redo: " + r + " (Ctrl+Y)");
        const bool hs = proc_.headset().shown(frame::Settings::of("Totality").headset());
        if (headsetIcon_.isVisible() != hs) {
            headsetIcon_.setVisible(hs);
            layoutBody();
        }
    }
    play_.setButtonText(proc_.isPlaying() ? "Stop" : "Play");
    // The choice and its length as the parameters have them (the Set page and a host move them too); Compose names
    // what it makes, and is lit while what plays is the other.
    {
        const bool mix = proc_.mixChosen();
        if (mix != lengthOfMix_) showLength(mix);
        trackMode_.setToggleState(!mix, juce::dontSendNotification);
        mixMode_.setToggleState(mix, juce::dontSendNotification);
        const ParamStore& st = proc_.store();
        const double v = st.get(mix ? st.id(Module::Set, 0, set::Minutes) : st.id(Module::Compose, 0, compose::Minutes));
        if (!length_.isMouseButtonDown() && std::abs(length_.getValue() - v) > 1.0e-3) {
            const juce::ScopedValueSetter<bool> quiet(syncing_, true);
            length_.setValue(v, juce::dontSendNotification);
        }
        compose_.setButtonText(mix ? "Compose mix" : "Compose track");
        const juce::Colour c = kAccent.withAlpha(mix != proc_.playingMix() && !proc_.isComposing() ? 0.42f : 0.12f);
        if (compose_.findColour(juce::TextButton::buttonColourId) != c) compose_.setColour(juce::TextButton::buttonColourId, c);
    }
    const bool shown = proc_.muted() && shotPath_.isEmpty();
    mute_.setToggleState(shown, juce::dontSendNotification);
    mute_.setButtonText(shown ? (proc_.muteForced() ? "Muted (env)" : "Muted") : "Mute");
    compose_.setEnabled(!proc_.isComposing());
    // The test mode: the recording, when full, is written and the standalone quits.
    if (proc_.recordingDone()) {
        proc_.writeRecording();
        if (juce::JUCEApplicationBase::isStandaloneApp()) juce::JUCEApplicationBase::quit();
        return;
    }
    // The screenshot mode: wait for the first score, then draw the panel into a file and quit. TOT_SHOT_AT (a beat)
    // moves the playhead there first.
    if (shotPath_.isNotEmpty() && !proc_.isComposing() && shotTicks_ == 0)
        if (const char* at = std::getenv("TOT_SHOT_AT")) proc_.seekTo(std::atof(at));
    // TOT_PAGE_REPORT=<file> (01.10.2026): how far every page reaches past the window, a line each (Frame.h, pageReport),
    // then quit -- which page still scrolls at the usual size.
    if (const char* report = std::getenv("TOT_PAGE_REPORT"); report != nullptr && !proc_.isComposing()) {
        static int reportTicks = 0;
        if (++reportTicks == 8) {
            juce::File(report).replaceWithText(frame::pageReport(tabs_));
            if (juce::JUCEApplicationBase::isStandaloneApp()) juce::JUCEApplicationBase::quit();
        }
    }
    // TOT_SHOT_FULL: the window grows until nothing of the page in front scrolls, so the picture shows all of it.
    if (shotPath_.isNotEmpty() && !proc_.isComposing() && (shotTicks_ == 6 || shotTicks_ == 12) && std::getenv("TOT_SHOT_FULL") != nullptr) {
        std::function<int(juce::Component&)> overflow = [&](juce::Component& c) {
            int most = 0;
            if (auto* v = dynamic_cast<juce::Viewport*>(&c))
                if (auto* inner = v->getViewedComponent()) most = inner->getHeight() - v->getMaximumVisibleHeight();
            for (auto* child : c.getChildren()) most = std::max(most, overflow(*child));
            return most;
        };
        if (auto* page = tabs_.getCurrentContentComponent())
            if (const int more = overflow(*page); more > 0) setSize(getWidth(), getHeight() + more + 4);
    }
    if (shotPath_.isNotEmpty() && !proc_.isComposing() && proc_.scoreVersion() > 0 && ++shotTicks_ > 20) {
        const juce::Image img = createComponentSnapshot(getLocalBounds());
        juce::File f(shotPath_);
        f.deleteFile();
        juce::FileOutputStream out(f);
        juce::PNGImageFormat().writeImageToStream(img, out);
        out.flush();
        shotPath_ = {};
        if (juce::JUCEApplicationBase::isStandaloneApp()) juce::JUCEApplicationBase::quit();
    }
}
