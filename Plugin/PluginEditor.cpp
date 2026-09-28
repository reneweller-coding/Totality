/**
 * @file PluginEditor.cpp
 * @brief The plugin's panel.
 */
#include "PluginEditor.h"
#include "EditorEclipse.h"
#include "EditorPerform.h"
#include "EditorStyle.h"
#include "tot/Presets.h"
#include <cstdlib>

using namespace tot;

namespace {

const juce::Colour kBack = totui::colour::bg, kPanel = totui::colour::panel, kInk = totui::colour::ink, kDim = totui::colour::dim,
                   kAccent = totui::colour::amber, kOnset = totui::colour::onset;

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
    : proc_(p), groups_(std::move(groups)), instances_(instances)
{
    if (instances_ > 1) {
        for (int i = 0; i < instances_; ++i)
            instance_.addItem(i < static_cast<int>(names.size()) ? names[static_cast<size_t>(i)] : juce::String(i + 1), i + 1);
        instance_.setSelectedId(1, juce::dontSendNotification);
        instance_.onChange = [this] { build(); resized(); repaint(); };
        addAndMakeVisible(instance_);
    }
    build();
}

void ParamPage::build()
{
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
            auto* box = new juce::ComboBox();
            for (int c = 0; c <= static_cast<int>(d.maxValue); ++c) box->addItem(d.choices[c], c + 1);
            box->setColour(juce::ComboBox::arrowColourId, colour);
            controls_.add(box);
            combos_.push_back(std::make_unique<juce::ComboBoxParameterAttachment>(*param, *box));
            cell.kind = 1;
            cell.narrow = narrow;
        } else if (d.curve == Curve::Toggle) {
            auto* b = new juce::ToggleButton();
            b->setColour(juce::ToggleButton::tickColourId, colour);
            controls_.add(b);
            buttons_.push_back(std::make_unique<juce::ButtonParameterAttachment>(*param, *b));
            cell.kind = 2;
        } else {
            auto* sl = new juce::Slider(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            sl->setTextBoxStyle(juce::Slider::TextBoxBelow, false, big ? 96 : 70, 16);
            sl->setColour(juce::Slider::rotarySliderFillColourId, colour);
            sl->setTextValueSuffix(d.unit[0] != 0 ? juce::String(" ") + d.unit : juce::String());
            sl->setTooltip(juce::String(s.key(id)) + ": " + d.name + (d.unit[0] != 0 ? juce::String(" (") + d.unit + ")" : juce::String()));
            controls_.add(sl);
            sliders_.push_back(std::make_unique<juce::SliderParameterAttachment>(*param, *sl));
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
}

int ParamPage::top() const { return instances_ > 1 ? 32 : 0; }

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
    auto* self = const_cast<ParamPage*>(this);
    const std::vector<Box> kept = boxes_;
    const int h = self->layoutBoxes({ 10, 10, width - 20, 100 }, false);
    self->boxes_ = kept;
    return 20 + top() + h + 10;
}

void ParamPage::paint(juce::Graphics& g)
{
    for (const Box& box : boxes_) {
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
    if (instances_ > 1) {
        auto top = area.removeFromTop(26);
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
    int w = getWidth(), h = page_->heightFor(w);
    if (h > getHeight()) { w -= view_.getScrollBarThickness(); h = page_->heightFor(w); }
    page_->setSize(w, std::max(h, getHeight()));
}

// ---------------------------------------------------------------------------------------------------

const char* const ArrangeView::kLaneNames[kLanes] = { "kick", "hats", "perc", "ping", "bass", "pads" };

void ArrangeView::rebuild()
{
    proc_.copyPlaying(playing_);
    lanes_.assign(static_cast<size_t>(kLanes * kBins), 0.0f);
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
            const int b0 = std::clamp(static_cast<int>(n.beat / len * kBins), 0, kBins - 1);
            const int b1 = std::clamp(static_cast<int>((n.beat + std::max(n.length, 0.25)) / len * kBins), b0, kBins - 1);
            for (int b = b0; b <= b1; ++b) {
                float& v = lanes_[static_cast<size_t>(lane * kBins + b)];
                v = std::min(1.0f, v + 0.08f);
            }
        }
    }
}

void ArrangeView::paint(juce::Graphics& g)
{
    g.fillAll(kPanel);
    if (proc_.scoreVersion() != version_) { version_ = proc_.scoreVersion(); rebuild(); }
    const double beats = playing_.set.lengthBeats;
    if (beats <= 0.0) {
        g.setColour(kDim);
        g.drawText("composing ...", getLocalBounds(), juce::Justification::centred);
        return;
    }
    const float w = static_cast<float>(getWidth()), h = static_cast<float>(getHeight());
    const auto xOf = [&](double b) { return static_cast<float>(b / beats) * w; };
    const float head = detailed_ ? 44.0f : 24.0f;
    g.setFont(juce::FontOptions(11.0f));
    if (playing_.isSet) {
        // The tracks on their decks: A on top, B below it; a blend where two overlap.
        const float rowH = (head - 4.0f) / 2.0f;
        for (size_t i = 0; i < playing_.tracks.size(); ++i) {
            const TrackPlace& t = playing_.tracks[i];
            const float y = 2.0f + rowH * static_cast<float>(t.deck == 1 ? 1 : 0);
            const float x0 = xOf(t.start), x1 = xOf(t.end), xs = xOf(t.swapIn);
            g.setColour(totui::deckColour(t.deck).interpolatedWith(kBack, 0.7f));
            g.fillRect(x0 + 1.0f, y, std::max(1.0f, x1 - x0 - 2.0f), rowH - 2.0f);
            g.setColour(totui::deckColour(t.deck).interpolatedWith(kBack, 0.45f));
            g.fillRect(xs, y, 2.0f, rowH - 2.0f);   // its swap: from here it owns the low end
            g.setColour(kInk);
            const juce::String name = "T" + juce::String(static_cast<int>(i) + 1) + " " + juce::String(t.info.style) + " " + juce::String(t.info.camelot);
            if (x1 - x0 > 40.0f) g.drawFittedText(name, juce::Rectangle<int>(static_cast<int>(x0) + 4, static_cast<int>(y), static_cast<int>(x1 - x0) - 8, static_cast<int>(rowH - 2.0f)), juce::Justification::centredLeft, 1);
        }
    } else {
        const std::vector<Marker>& markers = playing_.set.decks[0].markers;
        for (size_t i = 0; i < markers.size(); ++i) {
            const double b0 = markers[i].beat, b1 = i + 1 < markers.size() ? markers[i + 1].beat : beats;
            const float x0 = xOf(b0), x1 = xOf(b1);
            const juce::String name(markers[i].text);
            g.setColour(blockColour(name));
            g.fillRect(x0 + 1.0f, 2.0f, std::max(1.0f, x1 - x0 - 2.0f), head - 4.0f);
            g.setColour(kInk);
            if (x1 - x0 > 30.0f) g.drawFittedText(name, juce::Rectangle<int>(static_cast<int>(x0) + 4, 4, static_cast<int>(x1 - x0) - 8, 16), juce::Justification::topLeft, 1);
        }
        if (detailed_) {
            // The operations of the form, named, under the blocks.
            g.setFont(juce::FontOptions(10.0f));
            float free = 0.0f;   // where the next name may start without covering the last
            for (const BlockOp& o : playing_.set.decks[0].ops) {
                if (o.kind == OpKind::End || o.kind == OpKind::Hold) continue;
                const float x = xOf(o.beat);
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
    // The matrix: a lane per group of layers, lit where it plays.
    const float top = head + 2.0f, laneH = (h - top - 2.0f) / static_cast<float>(kLanes);
    if (!lanes_.empty() && laneH > 2.0f) {
        const float bw = w / static_cast<float>(kBins);
        for (int l = 0; l < kLanes; ++l) {
            const float y = top + laneH * static_cast<float>(l);
            for (int b = 0; b < kBins; ++b) {
                const float a = lanes_[static_cast<size_t>(l * kBins + b)];
                if (a <= 0.0f) continue;
                g.setColour(kInk.withAlpha(0.12f + 0.5f * a));
                g.fillRect(bw * static_cast<float>(b), y + 1.0f, bw + 0.5f, laneH - 2.0f);
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
    const float x = xOf(proc_.positionBeats());
    g.setColour(kOnset);
    g.fillRect(x - 1.0f, 0.0f, 2.0f, h);
}

void ArrangeView::mouseDown(const juce::MouseEvent& e)
{
    double beats = 0.0, secs = 0.0;
    proc_.length(beats, secs);
    proc_.seekTo(beats * e.position.x / std::max(1.0f, static_cast<float>(getWidth())));
}

// ---------------------------------------------------------------------------------------------------

ArrangePage::ArrangePage(TotalityProcessor& p) : proc_(p), view_(p, true)
{
    addAndMakeVisible(view_);
    which_.setColour(juce::Label::textColourId, kInk);
    addAndMakeVisible(which_);
    sounds_.setColour(juce::Label::textColourId, kDim);
    sounds_.setMinimumHorizontalScale(0.7f);
    addAndMakeVisible(sounds_);
    for (const char* unit : kUnitNames) {
        auto* b = rerolls_.add(new juce::TextButton(juce::String("reroll ") + unit));
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
        text << (p.isSet ? "Track " + juce::String(t + 1) + " of " + juce::String(static_cast<int>(p.tracks.size())) + ": " : juce::String("The track: "))
             << juce::String(i.style) << ", " << kFormNames[static_cast<int>(i.form)] << ", " << juce::String(i.bpm, 1) << " BPM, "
             << kKeyNames[i.key] << " " << kScaleNames[i.scale] << " (" << juce::String(i.camelot) << "), " << i.bars << " bars, "
             << (i.subOwns ? "the sub owns the low end" : "the rumble owns the low end") << ", bar similarity " << juce::String(i.similarity, 2);
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
    view_.repaint();
}

void ArrangePage::resized()
{
    auto r = getLocalBounds().reduced(10);
    which_.setBounds(r.removeFromTop(22));
    sounds_.setBounds(r.removeFromTop(20));
    r.removeFromTop(4);
    auto row = r.removeFromTop(28);
    for (auto* b : rerolls_) b->setBounds(row.removeFromLeft(104).reduced(2));
    row.removeFromLeft(12);
    track_.setBounds(row.removeFromLeft(170).reduced(2));
    set_.setBounds(row.removeFromLeft(160).reduced(2));
    r.removeFromTop(8);
    view_.setBounds(r);
}

void ArrangePage::paint(juce::Graphics& g) { g.fillAll(kPanel); }

// ---------------------------------------------------------------------------------------------------

ExportPage::ExportPage(TotalityProcessor& p) : proc_(p)
{
    wav_.onClick = [this] { exportWith(0); };
    stems_.onClick = [this] { exportWith(TotalityProcessor::kStems); };
    loops_.onClick = [this] { exportWith(TotalityProcessor::kLoops); };
    all_.onClick = [this] { exportWith(TotalityProcessor::kStems | TotalityProcessor::kLoops); };
    save_.onClick = [this] {
        documents().createDirectory();
        chooser_ = std::make_unique<juce::FileChooser>("Save set", documents().getChildFile("totality.totset"), "*.totset");
        chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                              [this](const juce::FileChooser& fc) { if (fc.getResult() != juce::File()) proc_.saveSet(fc.getResult().withFileExtension(".totset")); });
    };
    load_.onClick = [this] {
        chooser_ = std::make_unique<juce::FileChooser>("Load set", documents(), "*.totset");
        chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this](const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) proc_.loadSet(fc.getResult()); });
    };
    for (auto* b : { &wav_, &stems_, &loops_, &all_, &save_, &load_ }) addAndMakeVisible(b);
    status_.setColour(juce::Label::textColourId, kInk);
    addAndMakeVisible(status_);
    cue_ = std::make_unique<ParamPage>(proc_, std::vector<std::pair<Module, int>>{ { Module::Cue, 0 } });
    addAndMakeVisible(*cue_);
    startTimerHz(4);
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
    g.fillAll(kPanel);
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
    body_.painter = [this](juce::Graphics& g) { g.fillAll(kBack); drawLogo(g, logo_); };
    body_.onResize = [this] { layoutBody(); };
    ParamStore& s = proc_.store();
    title_.setText("TOTALITY", juce::dontSendNotification);
    title_.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    title_.setColour(juce::Label::textColourId, kAccent);
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
    auto slider = [&](juce::Slider& sl, juce::Label& label, const char* text, int id, const char* tip) {
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
        sliders_.push_back(std::make_unique<juce::SliderParameterAttachment>(*proc_.parameter(id), sl));
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, kDim);
        sl.setTooltip(tip);
        body_.addAndMakeVisible(sl);
        body_.addAndMakeVisible(label);
    };
    slider(minutes_, minutesLabel_, "Track min", s.id(Module::Compose, 0, compose::Minutes), "The length of a track");
    slider(setMinutes_, setLabel_, "Set min", s.id(Module::Set, 0, set::Minutes),
           "The length of a DJ set of tracks mixed on two decks; 0: a single track");

    compose_.onClick = [this] { proc_.compose(); };
    seed_.onClick = [this] { proc_.newSeed(); };
    play_.onClick = [this] { proc_.setPlaying(!proc_.isPlaying()); };
    for (auto* b : { &compose_, &seed_, &play_ }) body_.addAndMakeVisible(b);
    // Mute, as in Phosphene: silence at the output; TOT_MUTE (or the screenshot mode) holds it on.
    mute_.setClickingTogglesState(true);
    mute_.setToggleState(proc_.muted(), juce::dontSendNotification);
    mute_.setEnabled(!proc_.muteForced() || std::getenv("TOT_SHOT") != nullptr);
    mute_.setColour(juce::TextButton::buttonOnColourId, totui::colour::red.withAlpha(0.55f));
    mute_.setTooltip(proc_.muteForced() ? "Muted by TOT_MUTE: an automated run makes no sound" : "Silence the output");
    mute_.onClick = [this] { proc_.setMuted(mute_.getToggleState()); };
    body_.addAndMakeVisible(mute_);
    play_.setColour(juce::TextButton::buttonColourId, kAccent.withAlpha(0.22f));
    compose_.setColour(juce::TextButton::buttonColourId, kAccent.withAlpha(0.14f));
    // The update check: once a day it asks GitHub for the latest release (nothing else is sent); a newer one shows here.
    checkUpdates_.setToggleState(updates_->enabled(), juce::dontSendNotification);
    checkUpdates_.setTooltip("Once a day, ask GitHub whether a newer Totality is out (nothing else is sent, nothing is downloaded)");
    checkUpdates_.onClick = [this] { updates_->setEnabled(checkUpdates_.getToggleState()); };
    body_.addAndMakeVisible(checkUpdates_);
    update_.setColour(juce::HyperlinkButton::textColourId, kAccent);
    update_.setTooltip("Open the release page");
    body_.addChildComponent(update_);
    full_.setTooltip("Full screen (F11; Esc leaves it)");
    full_.onClick = [this] { toggleFullScreen(); };
    body_.addChildComponent(full_);
    setWantsKeyboardFocus(true);
    rerolls_.setColour(juce::Label::textColourId, kDim);
    status_.setColour(juce::Label::textColourId, kInk);
    body_.addAndMakeVisible(rerolls_);
    body_.addAndMakeVisible(status_);

    body_.addAndMakeVisible(arrange_);
    using M = Module;
    auto page = [&](const char* name, std::vector<std::pair<M, int>> groups, int instances = 1, std::vector<juce::String> names = {}) {
        tabs_.addTab(name, kPanel, new ScrollingPage(std::make_unique<ParamPage>(proc_, std::move(groups), instances, std::move(names))), true);
    };
    page("Set", { { M::Compose, 0 }, { M::Set, 0 }, { M::DjFx, 0 } });
    tabs_.addTab("Arrange", kPanel, new ArrangePage(proc_), true);
    tabs_.addTab("Patterns", kPanel, new EclipsePage(proc_), true);
    page("Low End", { { M::Kick, 0 }, { M::Rumble, 0 }, { M::Sub, 0 } });
    std::vector<juce::String> lanes;
    for (int l = 0; l < kPercLanes; ++l) lanes.push_back("Lane " + juce::String(l + 1));
    page("Drums", { { M::Perc, 0 } }, kPercLanes, lanes);
    page("Tones", { { M::Ping, 0 }, { M::Bass, 0 }, { M::Acid, 0 }, { M::Chord, 0 }, { M::Drone, 0 }, { M::Texture, 0 } });
    page("Dub", { { M::Dub, 0 }, { M::Space, 0 }, { M::Cloud, 0 } });
    tabs_.addTab("Mixer", kPanel, new MixerPage(proc_), true);
    tabs_.addTab("Perform", kPanel, new PerformPage(proc_), true);
    tabs_.addTab("Export", kPanel, new ExportPage(proc_), true);
    tabs_.addTab("Style", kPanel, new StylePage(proc_), true);
    body_.addAndMakeVisible(tabs_);

    setResizable(true, true);
    setResizeLimits(800, 520, 4800, 3100);
    setSize(1180, 760);

    if (const char* shot = std::getenv("TOT_SHOT")) {
        shotPath_ = shot;
        if (const char* tab = std::getenv("TOT_TAB")) tabs_.setCurrentTabIndex(juce::String(tab).getIntValue());
        if (const char* size = std::getenv("TOT_SHOT_SIZE")) {
            const juce::String sz(size);
            setSize(sz.upToFirstOccurrenceOf("x", false, false).getIntValue(), sz.fromFirstOccurrenceOf("x", false, false).getIntValue());
        }
    }
    startTimerHz(15);
}

TotalityEditor::~TotalityEditor()
{
    stopTimer();
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
        safe->full_.setVisible(window != nullptr);
        safe->layoutBody();
    });
}

void TotalityEditor::toggleFullScreen()
{
    auto* window = findParentComponentOfClass<juce::DocumentWindow>();
    if (window == nullptr) return;
    auto& desktop = juce::Desktop::getInstance();
    const bool on = desktop.getKioskModeComponent() != window;
    desktop.setKioskModeComponent(on ? window : nullptr, false);
    full_.setToggleState(on, juce::dontSendNotification);
    grabKeyboardFocus();
}

bool TotalityEditor::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::F11Key) { toggleFullScreen(); return true; }
    if (key.getKeyCode() == juce::KeyPress::escapeKey && juce::Desktop::getInstance().getKioskModeComponent() != nullptr) {
        toggleFullScreen();
        return true;
    }
    return false;
}

void TotalityEditor::layoutBody()
{
    auto area = body_.getLocalBounds().reduced(10);
    auto top = area.removeFromTop(34);
    logo_ = top.removeFromLeft(34).toFloat().reduced(2.0f);
    title_.setBounds(top.removeFromLeft(112));
    style_.setBounds(top.removeFromLeft(120).reduced(3));
    key_.setBounds(top.removeFromLeft(64).reduced(3));
    scale_.setBounds(top.removeFromLeft(120).reduced(3));
    minutesLabel_.setBounds(top.removeFromLeft(70));
    minutes_.setBounds(top.removeFromLeft(122).reduced(2));
    setLabel_.setBounds(top.removeFromLeft(56));
    setMinutes_.setBounds(top.removeFromLeft(122).reduced(2));
    mute_.setBounds(top.removeFromRight(proc_.muteForced() && shotPath_.isEmpty() ? 100 : 70).reduced(3));
    play_.setBounds(top.removeFromRight(76).reduced(3));
    seed_.setBounds(top.removeFromRight(90).reduced(3));
    compose_.setBounds(top.removeFromRight(96).reduced(3));
    area.removeFromTop(4);
    auto third = area.removeFromTop(20);
    if (full_.isVisible()) full_.setBounds(third.removeFromRight(100));
    checkUpdates_.setBounds(third.removeFromRight(120));
    update_.setBounds(third.removeFromRight(190));
    status_.setBounds(third.removeFromLeft(third.getWidth() * 3 / 5));
    rerolls_.setBounds(third);
    area.removeFromTop(4);
    arrange_.setBounds(area.removeFromTop(96));
    area.removeFromTop(8);
    tabs_.setBounds(area);
}

void TotalityEditor::timerCallback()
{
    status_.setText(proc_.status(), juce::dontSendNotification);
    rerolls_.setText(proc_.curationText(), juce::dontSendNotification);
    {
        const juce::String v = checkUpdates_.getToggleState() ? updates_->newer() : juce::String();
        if (v.isNotEmpty() && update_.getButtonText() != "Version " + v + " available") {
            update_.setButtonText("Version " + v + " available");
            update_.setURL(juce::URL(updates_->page()));
        }
        update_.setVisible(v.isNotEmpty());
    }
    play_.setButtonText(proc_.isPlaying() ? "Stop" : "Play");
    const bool shown = proc_.muted() && shotPath_.isEmpty();
    mute_.setToggleState(shown, juce::dontSendNotification);
    mute_.setButtonText(shown ? (proc_.muteForced() ? "Muted (env)" : "Muted") : "Mute");
    compose_.setEnabled(!proc_.isComposing());
    arrange_.repaint();
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
