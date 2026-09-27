/**
 * @file PluginEditor.cpp
 * @brief The plugin's panel.
 */
#include "PluginEditor.h"
#include "EditorEclipse.h"
#include "EditorPerform.h"
#include "EditorStyle.h"
#include <cstdlib>

using namespace umb;

namespace {

const juce::Colour kBack = umbui::colour::bg, kPanel = umbui::colour::panel, kInk = umbui::colour::ink, kDim = umbui::colour::dim,
                   kAccent = umbui::colour::amber, kOnset = umbui::colour::onset;

/** @brief Colour of a block by its marker: the edges dark, the body brighter towards the peak, a reduction the motion's teal. */
juce::Colour blockColour(const juce::String& name)
{
    using umbui::Family;
    auto tone = [](Family f, float k) { return umbui::familyColour(f).interpolatedWith(umbui::colour::bg, k); };
    if (name.startsWith("Intro") || name.startsWith("Outro")) return tone(Family::Space, 0.72f);
    if (name.startsWith("Reduction") || name.contains("kick out")) return tone(Family::Motion, 0.62f);
    if (name.startsWith("Return")) return tone(Family::Filter, 0.6f);
    if (name.startsWith("Peak")) return tone(Family::Filter, 0.5f);
    return tone(Family::Source, 0.72f);
}

/** @brief The documents folder of Umbra. */
juce::File documents() { return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Umbra"); }

} // namespace

// ---------------------------------------------------------------------------------------------------

ParamPage::ParamPage(UmbraProcessor& p, std::vector<std::pair<Module, int>> groups, int instances, std::vector<juce::String> names)
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
        label->setColour(juce::Label::textColourId, big ? umbui::colour::ink : umbui::colour::dim);
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
        // A module with instances shown beside others (the decks): its groups carry the instance's name.
        const juce::String suffix = instances_ <= 1 && g.first == Module::Deck ? juce::String(" ") + juce::String::charToString(static_cast<juce::juce_wchar>('A' + instance)) : juce::String();
        for (const umbui::GroupSpec& spec : umbui::layoutOf(g.first)) {
            Box box;
            box.title = juce::String(spec.title) + suffix;
            box.colour = g.first == Module::Deck ? umbui::deckColour(instance) : umbui::familyColour(spec.family);
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
        more.title = umbui::layoutOf(g.first).empty() ? juce::String("Settings") : juce::String("More");
        more.colour = umbui::familyColour(umbui::Family::Space);
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
                label->setBounds(r.getX(), r.getY(), r.getWidth(), 16);
                if (c.kind == 0) comp->setBounds(r.withTrimmedTop(16));
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
        g.setColour(umbui::colour::group);
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(umbui::colour::edge);
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
            g.setColour(umbui::deckColour(t.deck).interpolatedWith(kBack, 0.7f));
            g.fillRect(x0 + 1.0f, y, std::max(1.0f, x1 - x0 - 2.0f), rowH - 2.0f);
            g.setColour(umbui::deckColour(t.deck).interpolatedWith(kBack, 0.45f));
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

ArrangePage::ArrangePage(UmbraProcessor& p) : proc_(p), view_(p, true)
{
    addAndMakeVisible(view_);
    which_.setColour(juce::Label::textColourId, kInk);
    addAndMakeVisible(which_);
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
    track_.setVisible(p.isSet);
    set_.setVisible(p.isSet);
    view_.repaint();
}

void ArrangePage::resized()
{
    auto r = getLocalBounds().reduced(10);
    which_.setBounds(r.removeFromTop(22));
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

ExportPage::ExportPage(UmbraProcessor& p) : proc_(p)
{
    wav_.onClick = [this] { exportWith(0); };
    stems_.onClick = [this] { exportWith(UmbraProcessor::kStems); };
    loops_.onClick = [this] { exportWith(UmbraProcessor::kLoops); };
    all_.onClick = [this] { exportWith(UmbraProcessor::kStems | UmbraProcessor::kLoops); };
    save_.onClick = [this] {
        documents().createDirectory();
        chooser_ = std::make_unique<juce::FileChooser>("Save set", documents().getChildFile("umbra.umbset"), "*.umbset");
        chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                              [this](const juce::FileChooser& fc) { if (fc.getResult() != juce::File()) proc_.saveSet(fc.getResult().withFileExtension(".umbset")); });
    };
    load_.onClick = [this] {
        chooser_ = std::make_unique<juce::FileChooser>("Load set", documents(), "*.umbset");
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
    chooser_ = std::make_unique<juce::FileChooser>("Export", documents().getChildFile("umbra.wav"), "*.wav");
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
        "An .umbset holds the seed, the lengths, the rerolls and every changed knob.";
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
    const float disc = s * (s > 24.0f ? 0.2f : 0.24f);
    juce::ColourGradient corona(juce::Colour(238, 214, 168).withAlpha(0.85f), c.x, c.y, juce::Colour(238, 214, 168).withAlpha(0.0f),
                                c.x + disc * 1.8f, c.y, true);
    g.setGradientFill(corona);
    g.fillEllipse(c.x - disc * 1.8f, c.y - disc * 1.8f, disc * 3.6f, disc * 3.6f);
    if (s > 24.0f) {
        const int rings = s <= 48.0f ? 2 : 3;
        const float width = std::max(1.0f, s / 90.0f);
        for (int k = 0; k < rings; ++k) {
            const float rad = disc * 1.55f + (s * 0.44f - disc * 1.55f) * static_cast<float>(k) / static_cast<float>(std::max(1, rings - 1));
            g.setColour(juce::Colour(120, 112, 104).withAlpha(0.33f));
            g.drawEllipse(c.x - rad, c.y - rad, 2.0f * rad, 2.0f * rad, width);
            static const int steps[3] = { 16, 12, 7 };
            for (int i = 0; i < steps[k]; i += (k == 0 ? 3 : 2)) {
                const float a = -juce::MathConstants<float>::halfPi + juce::MathConstants<float>::twoPi * static_cast<float>(i) / static_cast<float>(steps[k]) + 0.4f * static_cast<float>(k);
                const float pr = std::max(s * 0.018f, width * 1.4f);
                g.setColour(i == 0 ? juce::Colour(214, 85, 63) : juce::Colour(238, 214, 168).withAlpha(0.8f));
                g.fillEllipse(c.x + rad * std::cos(a) - pr, c.y + rad * std::sin(a) - pr, 2.0f * pr, 2.0f * pr);
            }
        }
    }
    g.setColour(juce::Colour(5, 5, 7));
    g.fillEllipse(c.x - disc, c.y - disc, 2.0f * disc, 2.0f * disc);
    const float a = -juce::MathConstants<float>::pi / 4.0f, br = disc * 0.14f;
    g.setColour(juce::Colour(255, 246, 226));
    g.fillEllipse(c.x + disc * std::cos(a) - br, c.y + disc * std::sin(a) - br, 2.0f * br, 2.0f * br);
}

// ---------------------------------------------------------------------------------------------------

UmbraEditor::UmbraEditor(UmbraProcessor& p) : juce::AudioProcessorEditor(p), proc_(p), arrange_(p, false)
{
    setLookAndFeel(&lnf_);
    addAndMakeVisible(body_);
    body_.painter = [this](juce::Graphics& g) { g.fillAll(kBack); drawLogo(g, logo_); };
    body_.onResize = [this] { layoutBody(); };
    ParamStore& s = proc_.store();
    title_.setText("UMBRA", juce::dontSendNotification);
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
    // Mute, as in Phosphene: silence at the output; UMB_MUTE (or the screenshot mode) holds it on.
    mute_.setClickingTogglesState(true);
    mute_.setToggleState(proc_.muted(), juce::dontSendNotification);
    mute_.setEnabled(!proc_.muteForced() || std::getenv("UMB_SHOT") != nullptr);
    mute_.setColour(juce::TextButton::buttonOnColourId, umbui::colour::red.withAlpha(0.55f));
    mute_.setTooltip(proc_.muteForced() ? "Muted by UMB_MUTE: an automated run makes no sound" : "Silence the output");
    mute_.onClick = [this] { proc_.setMuted(mute_.getToggleState()); };
    body_.addAndMakeVisible(mute_);
    play_.setColour(juce::TextButton::buttonColourId, kAccent.withAlpha(0.22f));
    compose_.setColour(juce::TextButton::buttonColourId, kAccent.withAlpha(0.14f));
    // The update check: once a day it asks GitHub for the latest release (nothing else is sent); a newer one shows here.
    checkUpdates_.setToggleState(updates_->enabled(), juce::dontSendNotification);
    checkUpdates_.setTooltip("Once a day, ask GitHub whether a newer Umbra is out (nothing else is sent, nothing is downloaded)");
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

    if (const char* shot = std::getenv("UMB_SHOT")) {
        shotPath_ = shot;
        if (const char* tab = std::getenv("UMB_TAB")) tabs_.setCurrentTabIndex(juce::String(tab).getIntValue());
        if (const char* size = std::getenv("UMB_SHOT_SIZE")) {
            const juce::String sz(size);
            setSize(sz.upToFirstOccurrenceOf("x", false, false).getIntValue(), sz.fromFirstOccurrenceOf("x", false, false).getIntValue());
        }
    }
    startTimerHz(15);
}

UmbraEditor::~UmbraEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void UmbraEditor::paint(juce::Graphics& g) { g.fillAll(kBack); }

void UmbraEditor::resized()
{
    // The body at the design size (1180 x 760), scaled to the window by its height, and as wide as the window allows.
    const float scale = juce::jlimit(0.5f, 4.0f, std::min(static_cast<float>(getWidth()) / 1180.0f, static_cast<float>(getHeight()) / 760.0f));
    body_.setTransform(juce::AffineTransform::scale(scale));
    body_.setBounds(0, 0, juce::roundToInt(static_cast<float>(getWidth()) / scale), juce::roundToInt(static_cast<float>(getHeight()) / scale));
}

void UmbraEditor::parentHierarchyChanged()
{
    // A maximise button beside the other two, on the next turn of the message loop (the standalone's window is still
    // putting its content in when this is called). A host's window finds nothing here.
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<UmbraEditor>(this)] {
        if (safe == nullptr) return;
        auto* window = safe->findParentComponentOfClass<juce::DocumentWindow>();
        if (window != nullptr)
            window->setTitleBarButtonsRequired(juce::DocumentWindow::minimiseButton | juce::DocumentWindow::maximiseButton
                                                   | juce::DocumentWindow::closeButton, false);
        safe->full_.setVisible(window != nullptr);
        safe->layoutBody();
    });
}

void UmbraEditor::toggleFullScreen()
{
    auto* window = findParentComponentOfClass<juce::DocumentWindow>();
    if (window == nullptr) return;
    auto& desktop = juce::Desktop::getInstance();
    const bool on = desktop.getKioskModeComponent() != window;
    desktop.setKioskModeComponent(on ? window : nullptr, false);
    full_.setToggleState(on, juce::dontSendNotification);
    grabKeyboardFocus();
}

bool UmbraEditor::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::F11Key) { toggleFullScreen(); return true; }
    if (key.getKeyCode() == juce::KeyPress::escapeKey && juce::Desktop::getInstance().getKioskModeComponent() != nullptr) {
        toggleFullScreen();
        return true;
    }
    return false;
}

void UmbraEditor::layoutBody()
{
    auto area = body_.getLocalBounds().reduced(10);
    auto top = area.removeFromTop(34);
    logo_ = top.removeFromLeft(34).toFloat().reduced(2.0f);
    title_.setBounds(top.removeFromLeft(90));
    style_.setBounds(top.removeFromLeft(120).reduced(3));
    key_.setBounds(top.removeFromLeft(64).reduced(3));
    scale_.setBounds(top.removeFromLeft(120).reduced(3));
    minutesLabel_.setBounds(top.removeFromLeft(70));
    minutes_.setBounds(top.removeFromLeft(130).reduced(2));
    setLabel_.setBounds(top.removeFromLeft(56));
    setMinutes_.setBounds(top.removeFromLeft(130).reduced(2));
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

void UmbraEditor::timerCallback()
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
    // The screenshot mode: wait for the first score, then draw the panel into a file and quit. UMB_SHOT_AT (a beat)
    // moves the playhead there first.
    if (shotPath_.isNotEmpty() && !proc_.isComposing() && shotTicks_ == 0)
        if (const char* at = std::getenv("UMB_SHOT_AT")) proc_.seekTo(std::atof(at));
    // UMB_SHOT_FULL: the window grows until nothing of the page in front scrolls, so the picture shows all of it.
    if (shotPath_.isNotEmpty() && !proc_.isComposing() && (shotTicks_ == 6 || shotTicks_ == 12) && std::getenv("UMB_SHOT_FULL") != nullptr) {
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
