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
#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace tot;

namespace {

const juce::Colour Back = totui::colour::Bg;           ///< the skin's background
const juce::Colour Panel = totui::colour::Panel;       ///< the skin's panel
const juce::Colour Ink = totui::colour::Ink;           ///< the skin's ink
const juce::Colour Dim = totui::colour::Dim;           ///< the skin's dim ink
const juce::Colour Accent = totui::colour::Accent;     ///< the skin's accent
const juce::Colour Onset = totui::colour::Onset;       ///< the onsets' red

/** @brief Colour of a block by its marker: the edges dark, the body brighter towards the peak, a reduction the motion's teal. */
juce::Colour blockColour(const juce::String& name)
{
    using totui::Family;
    auto tone = [](Family f, float k) { return totui::familyColour(f).interpolatedWith(totui::colour::Bg, k); };
    if (name.startsWith("Intro") || name.startsWith("Outro")) return tone(Family::SPACE, 0.72f);
    if (name.startsWith("Reduction") || name.contains("kick out")) return tone(Family::MOTION, 0.62f);
    if (name.startsWith("Return")) return tone(Family::FILTER, 0.6f);
    if (name.startsWith("Peak")) return tone(Family::FILTER, 0.5f);
    return tone(Family::SOURCE, 0.72f);
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


/** @brief A synth's name in the presets' folder and the browser: its parameters' key without the lane's number ("bass", "perc"). */
juce::String synthKey(const ParamStore& s, ModuleE m)
{
    const std::string& k = s.key(s.id(m, 0, 0));
    return juce::String(k.substr(0, k.find('.'))).trimCharactersAtEnd("0123456789");
}

/** @brief "perc3.decay" -> "Perc 3", "kick.tune" -> "Kick": the module instance a store id belongs to, for the palette and the info line. */
juce::String moduleLabel(const ParamStore& s, int id)
{
    const std::string& k = s.key(id);
    const juce::String p(k.substr(0, k.find('.')));
    const juce::String name = p.trimCharactersAtEnd("0123456789");
    const juce::String digits = p.substring(name.length());
    const juce::String nice = name.substring(0, 1).toUpperCase() + name.substring(1);
    return digits.isEmpty() ? nice : nice + " " + digits;
}

/** @brief The folder of the player's own presets of synth @p m (Documents/Totality/Presets/<synth>). */
juce::File userPresetFolder(const ParamStore& s, ModuleE m) { return documents().getChildFile("Presets").getChildFile(synthKey(s, m)); }

/** @brief The player's own presets of synth @p m, by name. */
juce::Array<juce::File> userPresets(const ParamStore& s, ModuleE m)
{
    juce::Array<juce::File> files = userPresetFolder(s, m).findChildFiles(juce::File::findFiles, false, "*.preset");
    std::sort(files.begin(), files.end(), [](const juce::File& a, const juce::File& b) { return a.getFileName().compareIgnoreCase(b.getFileName()) < 0; });
    return files;
}

/** @brief A preset of the player's own as the knobs of instance @p instance (store id, real value). */
std::vector<std::pair<int, float>> userPresetKnobs(const ParamStore& s, ModuleE m, int instance, const juce::File& f)
{
    std::vector<std::pair<int, float>> out;
    const int n = ParamStore::moduleCount(m);
    for (const juce::String& line : juce::StringArray::fromLines(f.loadFileAsString())) {
        const juce::String l = line.trim();
        if (l.isEmpty() || l.startsWithChar('#') || !l.containsChar('=')) continue;
        const juce::String key = l.upToFirstOccurrenceOf("=", false, false).trim();
        const float v = l.fromFirstOccurrenceOf("=", false, false).trim().getFloatValue();
        for (int i = 0; i < n; ++i) {
            const int id = s.id(m, instance, i);
            const std::string& k = s.key(id);
            if (juce::String(k.substr(k.find('.') + 1)) == key) {
                out.emplace_back(id, v);
                break;
            }
        }
    }
    return out;
}

/** @brief Writes the knobs a preset sets (the synth's own, not the mix's) of instance @p instance as one of the player's presets. */
bool saveUserPreset(const ParamStore& s, ModuleE m, int instance, const juce::String& name)
{
    juce::String text;
    text << "# Totality: a " << synthKey(s, m) << " preset of the player's own -- knob=value, the synth's own knobs (not the mix's)\n";
    for (int i = 0; i < ParamStore::moduleCount(m); ++i) {
        if (presetLeaves(m, i)) continue;
        const int id = s.id(m, instance, i);
        const std::string& k = s.key(id);
        text << juce::String(k.substr(k.find('.') + 1)) << "=" << juce::String(s.get(id), 6) << "\n";
    }
    const juce::File f = userPresetFolder(s, m).getChildFile(juce::File::createLegalFileName(name.trim()) + ".preset");
    return f.getParentDirectory().createDirectory().wasOk() && f.replaceWithText(text);
}

/** @brief The knobs of menu id @p menuId of synth @p m (a factory preset's index + 1, or PresetBar::Yours + one of the player's). */
std::vector<std::pair<int, float>> knobsOf(const ParamStore& s, ModuleE m, int instance, int menuId)
{
    std::vector<std::pair<int, float>> out;
    if (menuId > PresetBar::Yours) {
        const juce::Array<juce::File> yours = userPresets(s, m);
        const int k = menuId - PresetBar::Yours - 1;
        if (k >= 0 && k < yours.size()) out = userPresetKnobs(s, m, instance, yours[k]);
        return out;
    }
    const std::vector<SoundPreset>& list = factoryPresets(m);
    if (menuId >= 1 && menuId <= static_cast<int>(list.size()))
        for (const auto& [k, v] : presetKnobs(m, instance, list[static_cast<size_t>(menuId - 1)])) out.emplace_back(s.id(m, instance, k), v);
    return out;
}

/** @brief Whether the knobs stand where @p values put them (within a thousandth of each one's range). */
bool knobsAt(const ParamStore& s, const std::vector<std::pair<int, float>>& values)
{
    for (const auto& [id, v] : values) {
        const ParamDesc& d = s.desc(id);
        if (std::abs(s.get(id) - v) > 0.001f * std::max(1.0e-6f, d.m_maxValue - d.m_minValue)) return false;
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------------------------------

PresetBar::PresetBar(TotalityProcessor& p, ModuleE m, int instance) : m_proc(p), m_module(m), m_instance(instance)
{
    fillMenu();
    m_menu.onChange = [this] {
        const int id = m_menu.getSelectedId();
        if (id <= 0) return;
        if (id > Yours) m_proc.applyKnobs("preset " + m_menu.getText(), knobsOf(m_proc.store(), m_module, m_instance, id));
        else m_proc.applyPreset(m_module, m_instance, id - 1);
        chosen(id);
    };
    auto step = [this](int by) {
        const int n = static_cast<int>(factoryPresets(m_module).size());
        const int at = m_menu.getSelectedId() >= 1 && m_menu.getSelectedId() <= n ? m_menu.getSelectedId() - 1 : (by > 0 ? -1 : 0);
        choose(((at + by) % n + n) % n);
    };
    m_prev.onClick = [step] { step(-1); };
    m_next.onClick = [step] { step(1); };
    m_prev.setTooltip("the preset before");
    m_next.setTooltip("the next preset");
    m_browse.onClick = [this] {
        if (auto* ed = findParentComponentOfClass<TotalityEditor>())
            ed->browsePresets(m_module, m_instance, [safe = juce::Component::SafePointer<PresetBar>(this)](int id) { if (safe != nullptr) safe->chosen(id); });
    };
    m_save.onClick = [this] { saveAs(); };
    m_star.setColour(juce::Label::textColourId, totui::colour::Accent);
    m_star.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    m_star.setJustificationType(juce::Justification::centred);
    m_star.setTooltip("The knobs have moved from the preset (the disk saves the sound as one of your own)");
    m_composed.setColour(juce::Label::textColourId, totui::colour::Dim);
    m_composed.setTooltip("The preset the composer chose for this synth in the track that plays (compose.pick_sounds; reroll sounds "
                         "draws others). Its values stand on the knobs; turn one and the sound follows.");
    for (juce::Component* c : { static_cast<juce::Component*>(&m_menu), static_cast<juce::Component*>(&m_prev), static_cast<juce::Component*>(&m_next),
                                static_cast<juce::Component*>(&m_browse), static_cast<juce::Component*>(&m_save), static_cast<juce::Component*>(&m_star),
                                static_cast<juce::Component*>(&m_composed) })
        addAndMakeVisible(c);
    startTimerHz(4);
}

void PresetBar::fillMenu()
{
    const int was = m_menu.getSelectedId();
    m_menu.clear(juce::dontSendNotification);
    juce::PopupMenu* root = m_menu.getRootMenu();
    const juce::Array<juce::File> yours = userPresets(m_proc.store(), m_module);
    if (!yours.isEmpty()) {
        juce::PopupMenu sub;
        for (int k = 0; k < yours.size(); ++k) sub.addItem(Yours + k + 1, yours[k].getFileNameWithoutExtension());
        root->addSubMenu("Yours", sub);
        root->addSeparator();
    }
    const std::vector<SoundPreset>& list = factoryPresets(m_module);
    for (size_t g = 0; g < list.size(); g += 64) {
        juce::PopupMenu sub;
        for (size_t i = g; i < g + 64 && i < list.size(); ++i) sub.addItem(static_cast<int>(i) + 1, list[i].m_name);
        root->addSubMenu(list[g].m_group, sub);
    }
    m_menu.setTextWhenNothingSelected(juce::String(static_cast<int>(list.size())) + " presets");
    if (was > 0) m_menu.setSelectedId(was, juce::dontSendNotification);
}

void PresetBar::choose(int index)
{
    m_proc.applyPreset(m_module, m_instance, index);
    chosen(index + 1);
}

void PresetBar::chosen(int menuId)
{
    m_menu.setSelectedId(menuId, juce::dontSendNotification);
    m_loaded = menuId;
    m_loadedKnobs = knobsOf(m_proc.store(), m_module, m_instance, menuId);
    if (m_menu.getText().isNotEmpty()) frame::PresetBrowser::played("Totality", synthKey(m_proc.store(), m_module), m_menu.getText());
    m_star.setText({}, juce::dontSendNotification);
}

void PresetBar::saveAs()
{
    m_ask = std::make_unique<juce::AlertWindow>("Save a preset", "The sound of this " + synthKey(m_proc.store(), m_module)
                                                   + " as a preset of your own (Documents/Totality/Presets):", juce::MessageBoxIconType::NoIcon);
    m_ask->addTextEditor("name", m_loaded > Yours ? m_menu.getText() : juce::String("My ") + synthKey(m_proc.store(), m_module));
    m_ask->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    m_ask->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    m_ask->enterModalState(true, juce::ModalCallbackFunction::create([this](int r) {
        const juce::String name = m_ask != nullptr ? m_ask->getTextEditorContents("name").trim() : juce::String();
        m_ask.reset();
        if (r != 1 || name.isEmpty()) return;
        if (!saveUserPreset(m_proc.store(), m_module, m_instance, name)) {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Totality", "The preset could not be written.");
            return;
        }
        fillMenu();
        const juce::Array<juce::File> yours = userPresets(m_proc.store(), m_module);
        for (int k = 0; k < yours.size(); ++k)
            if (yours[k].getFileNameWithoutExtension() == juce::File::createLegalFileName(name)) chosen(Yours + k + 1);
    }), false);
}

void PresetBar::timerCallback()
{
    const int index = m_proc.composedPreset(m_module, m_instance);
    if (index != m_shown) {
        m_shown = index;
        const std::vector<SoundPreset>& list = factoryPresets(m_module);
        if (index >= 0 && index < static_cast<int>(list.size())) {
            m_composed.setText("this track: " + juce::String(list[static_cast<size_t>(index)].m_name) + " (" + list[static_cast<size_t>(index)].m_group + ")",
                              juce::dontSendNotification);
            chosen(index + 1);   // what plays, until another is chosen
        } else {
            m_composed.setText("this track: the knobs' own sound", juce::dontSendNotification);
        }
    }
    // The star: the knobs have moved from the preset they came from.
    const bool moved = !m_loadedKnobs.empty() && !knobsAt(m_proc.store(), m_loadedKnobs);
    if (m_star.getText().isNotEmpty() != moved) m_star.setText(moved ? "*" : "", juce::dontSendNotification);
}

void PresetBar::resized()
{
    auto r = getLocalBounds().reduced(4, 0);
    auto row = r.removeFromTop(28);
    m_prev.setBounds(row.removeFromLeft(28));
    row.removeFromLeft(4);
    m_menu.setBounds(row.removeFromLeft(260));
    row.removeFromLeft(4);
    m_next.setBounds(row.removeFromLeft(28));
    row.removeFromLeft(4);
    m_browse.setBounds(row.removeFromLeft(30));
    m_save.setBounds(row.removeFromLeft(30));
    m_star.setBounds(row.removeFromLeft(16));
    row.removeFromLeft(6);
    m_composed.setBounds(row);
}

void PresetBar::paint(juce::Graphics&) {}

// ---------------------------------------------------------------------------------------------------

ParamPage::ParamPage(TotalityProcessor& p, std::vector<std::pair<ModuleE, int>> groups, int instances, std::vector<juce::String> names)
    : m_proc(p), m_live([this](int id) { return m_proc.playedNormalised(id); }), m_groups(std::move(groups)), m_instances(instances)
{
    m_actions.m_learn = [this](int id) { m_proc.learn(id); };
    m_actions.m_controllerFor = [this](int id) { return m_proc.controllerFor(id); };
    m_actions.m_forget = [this](int id) { m_proc.forget(id); };
    m_actions.m_reset = [this](int id) { m_proc.resetToDefault(id); };
    m_actions.m_describe = [this](int id) { return juce::String(m_proc.store().desc(id).m_name) + "  (" + m_proc.store().key(id) + ")"; };
    if (m_instances > 1) {
        for (int i = 0; i < m_instances; ++i)
            m_instance.addItem(i < static_cast<int>(names.size()) ? names[static_cast<size_t>(i)] : juce::String(i + 1), i + 1);
        m_instance.setSelectedId(1, juce::dontSendNotification);
        m_instance.onChange = [this] { build(); resized(); repaint(); };
        addAndMakeVisible(m_instance);
    }
    m_section.m_onChange = [this](int) { applySection(); refit(); };
    addChildComponent(m_section);
    build();
}

void ParamPage::build()
{
    m_live.clear();
    m_sliders.clear();
    m_combos.clear();
    m_buttons.clear();
    m_controls.clear();
    m_labels.clear();
    m_boxes.clear();
    const int inst = m_instances > 1 ? m_instance.getSelectedId() - 1 : 0;
    ParamStore& s = m_proc.store();
    // A control's name in its box, without the box's title in front.
    auto shortName = [](const juce::String& name, const juce::String& title) {
        if (name.startsWith(title + " ")) return name.substring(title.length() + 1);
        return name;
    };
    auto make = [&](int id, juce::Colour colour, bool big, const juce::String& title = {}, bool narrow = false) {
        StoreParameter* param = m_proc.parameter(id);
        if (param == nullptr) return Cell{};
        const ParamDesc& d = s.desc(id);
        auto* label = m_labels.add(new juce::Label({}, title.isEmpty() ? juce::String(d.m_name) : shortName(d.m_name, title)));
        label->setJustificationType(juce::Justification::centred);
        label->setColour(juce::Label::textColourId, big ? totui::colour::Ink : totui::colour::Dim);
        label->setFont(juce::FontOptions(big ? 13.0f : 12.0f));
        label->setMinimumHorizontalScale(0.75f);
        addAndMakeVisible(label);
        Cell cell;
        cell.m_big = big;
        if (d.m_curve == CurveE::CHOICE && d.m_choices != nullptr) {
            auto* box = new frame::Choice(&m_actions, id);
            for (int c = 0; c <= static_cast<int>(d.m_maxValue); ++c) box->addItem(d.m_choices[c], c + 1);
            box->setColour(juce::ComboBox::arrowColourId, colour);
            m_controls.add(box);
            m_combos.push_back(std::make_unique<juce::ComboBoxParameterAttachment>(*param, *box));
            cell.m_kind = 1;
            cell.m_narrow = narrow;
        } else if (d.m_curve == CurveE::TOGGLE) {
            auto* b = new frame::Switch(&m_actions, id);
            b->setColour(juce::ToggleButton::tickColourId, colour);
            m_controls.add(b);
            m_buttons.push_back(std::make_unique<juce::ButtonParameterAttachment>(*param, *b));
            cell.m_kind = 2;
        } else {
            auto* sl = new frame::Knob(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow, &m_actions, id);
            sl->setTextBoxStyle(juce::Slider::TextBoxBelow, false, big ? 96 : 70, 16);
            sl->setColour(juce::Slider::rotarySliderFillColourId, colour);
            sl->setTextValueSuffix(d.m_unit[0] != 0 ? juce::String(" ") + d.m_unit : juce::String());
            sl->setTooltip(juce::String(s.key(id)) + ": " + d.m_name + (d.m_unit[0] != 0 ? juce::String(" (") + d.m_unit + ")" : juce::String()));
            m_controls.add(sl);
            m_sliders.push_back(std::make_unique<juce::SliderParameterAttachment>(*param, *sl));
            sl->setDoubleClickReturnValue(true, d.m_defValue);
            m_live.add(*sl, id);
        }
        addAndMakeVisible(m_controls.getLast());
        cell.m_control = m_controls.size() - 1;
        return cell;
    };
    for (const auto& g : m_groups) {
        const int count = ParamStore::moduleCount(g.first);
        const int instance = m_instances > 1 ? inst : g.second;
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
            box.m_title = juce::String(k0.substr(0, k0.find('.'))) + " preset";
            box.m_colour = totui::familyColour(totui::Family::SOURCE);
            auto* bar = new PresetBar(m_proc, g.first, instance);
            m_controls.add(bar);
            addAndMakeVisible(bar);
            addChildComponent(m_labels.add(new juce::Label()));
            Cell c;
            c.m_kind = 4;
            c.m_control = m_controls.size() - 1;
            box.m_cells.push_back(c);
            m_boxes.push_back(std::move(box));
        }
        // A module with instances shown beside others (the decks): its groups carry the instance's name.
        const juce::String suffix = m_instances <= 1 && g.first == ModuleE::DECK ? juce::String(" ") + juce::String::charToString(static_cast<juce::juce_wchar>('A' + instance)) : juce::String();
        for (const totui::GroupSpec& spec : totui::layoutOf(g.first)) {
            Box box;
            box.m_title = juce::String(spec.m_title) + suffix;
            box.m_colour = g.first == ModuleE::DECK ? totui::deckColour(instance) : totui::familyColour(spec.m_family);
            for (const char* key : spec.m_keys) {
                const bool big = key[0] == '*', narrow = key[0] == '~';
                const std::string name = big || narrow ? key + 1 : key;
                for (int i = 0; i < count; ++i) {
                    if (placed[static_cast<size_t>(i)] || keys[static_cast<size_t>(i)] != name) continue;
                    const Cell c = make(s.id(g.first, instance, i), box.m_colour, big, spec.m_title, narrow);
                    if (c.m_control >= 0) box.m_cells.push_back(c);
                    placed[static_cast<size_t>(i)] = true;
                }
            }
            if (!box.m_cells.empty()) m_boxes.push_back(std::move(box));
        }
        // Whatever the panel does not name, so nothing added to a table is lost from the editor.
        Box more;
        more.m_title = totui::layoutOf(g.first).empty() ? juce::String("Settings") : juce::String("More");
        more.m_colour = totui::familyColour(totui::Family::SPACE);
        for (int i = 0; i < count; ++i) {
            if (placed[static_cast<size_t>(i)]) continue;
            const Cell c = make(s.id(g.first, instance, i), more.m_colour, false);
            if (c.m_control >= 0) more.m_cells.push_back(c);
        }
        if (!more.m_cells.empty()) m_boxes.push_back(std::move(more));
    }
    // The sections are planned again for the new boxes (plan(), when the page is measured next).
    m_plannedWidth = -1;
    m_sectionOf.assign(m_boxes.size(), 0);
    m_split = false;
    m_section.setVisible(false);
    applySection();
    if (getWidth() > 0) refit();
    frame::keepKeysForEditor(*this);   // the keys stay the editor's (Space plays, not the switch last clicked)
}

bool ParamPage::shown(size_t box) const
{
    if (m_measuring != nullptr) return std::find(m_measuring->begin(), m_measuring->end(), static_cast<int>(box)) != m_measuring->end();
    return !m_split || (box < m_sectionOf.size() && m_sectionOf[box] == m_section.current());
}

void ParamPage::plan(int width)
{
    if (width == m_plannedWidth && m_available == m_plannedAvailable) return;
    m_plannedWidth = width;
    m_plannedAvailable = m_available;
    juce::StringArray titles;
    for (const Box& b : m_boxes) titles.add(b.m_title);
    // A section's height as the page lays it out: its boxes alone, the row the switch needs, the margins.
    auto height = [this, width](const std::vector<int>& set) {
        const std::vector<Box> kept = m_boxes;
        m_measuring = &set;
        const int h = layoutBoxes({ 10, 10, width - 20, 100 }, false);
        m_measuring = nullptr;
        m_boxes = kept;
        return 20 + 32 + h + 10;
    };
    const frame::SectionPlan p = frame::planSections(titles, m_available, height);
    m_sectionOf.assign(m_boxes.size(), 0);
    for (size_t s = 0; s < p.m_groups.size(); ++s)
        for (int b : p.m_groups[s]) m_sectionOf[static_cast<size_t>(b)] = static_cast<int>(s);
    m_split = p.m_groups.size() > 1;
    if (m_split) m_section.setNames(p.m_names);
    m_section.setVisible(m_split);
    applySection();
}

void ParamPage::applySection()
{
    for (size_t b = 0; b < m_boxes.size(); ++b) {
        const Box& box = m_boxes[b];
        const bool on = shown(b);
        for (const Cell& c : box.m_cells) {
            m_controls[c.m_control]->setVisible(on);
            if (c.m_kind != 4) m_labels[c.m_control]->setVisible(on);
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

int ParamPage::top() const { return m_instances > 1 || m_split ? 32 : 0; }

void ParamPage::reveal(juce::Component& part)
{
    // The box holding it, and its section in front.
    for (size_t b = 0; b < m_boxes.size(); ++b)
        for (const Cell& c : m_boxes[b].m_cells) {
            juce::Component* comp = m_controls[c.m_control];
            if (comp != &part && !comp->isParentOf(&part)) continue;
            if (m_split && b < m_sectionOf.size() && m_sectionOf[b] != m_section.current()) {
                m_section.setCurrent(m_sectionOf[b]);
                applySection();
                refit();
            }
            return;
        }
}

bool ParamPage::bringId(int id)
{
    if (m_instances <= 1) return false;
    const ParamStore& s = m_proc.store();
    if (id < 0 || id >= s.count()) return false;
    const ModuleE m = s.moduleOf(id);
    const int inst = s.instanceOf(id);
    for (const auto& g : m_groups)
        if (g.first == m && inst >= 0 && inst < m_instances) {
            m_instance.setSelectedId(inst + 1, juce::sendNotificationSync);   // build() makes that lane's controls
            return true;
        }
    return false;
}

juce::String ParamPage::describe() const
{
    juce::String t;
    const ParamStore& s = m_proc.store();
    for (const Box& box : m_boxes) {
        t << box.m_title.toUpperCase() << "\n";
        for (const Cell& c : box.m_cells) {
            if (c.m_kind == 4) { t << "  the presets: the 1024 of this synth in sixteen groups, and the composer's choice\n"; continue; }
            int id = -1;
            if (auto* k = dynamic_cast<frame::Knob*>(m_controls[c.m_control])) id = k->id();
            else if (auto* m = dynamic_cast<frame::Choice*>(m_controls[c.m_control])) id = m->id();
            else if (auto* w = dynamic_cast<frame::Switch*>(m_controls[c.m_control])) id = w->id();
            if (id < 0) continue;
            const ParamDesc& d = s.desc(id);
            t << "  " << d.m_name << ": ";
            if (d.m_curve == CurveE::TOGGLE) t << (s.getBool(id) ? "on" : "off");
            else if (d.m_curve == CurveE::CHOICE && d.m_choices != nullptr) t << d.m_choices[s.getInt(id)];
            else t << juce::String(s.get(id), 2) << (d.m_unit[0] != 0 ? juce::String(" ") + d.m_unit : juce::String())
                   << "   (" << juce::String(d.m_minValue, 2) << " .. " << juce::String(d.m_maxValue, 2) << ", default " << juce::String(d.m_defValue, 2) << ")";
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
    constexpr int Title = 22, Pad = 8, Gap = 10;
    auto cellSize = [](const Cell& c) {
        switch (c.m_kind) {
        case 1: return juce::Point<int>(c.m_narrow ? 112 : 140, 100);
        case 2: return juce::Point<int>(92, 100);
        case 4: return juce::Point<int>(720, 34);
        default: return c.m_big ? juce::Point<int>(108, 136) : juce::Point<int>(82, 100);
        }
    };
    const int width = std::max(200, area.getWidth());
    int x = 0, y = 0, lineH = 0;
    std::vector<size_t> line;
    auto closeLine = [&]() {
        for (size_t b : line) m_boxes[b].m_bounds.setHeight(lineH);
        line.clear();
    };
    for (size_t b = 0; b < m_boxes.size(); ++b) {
        Box& box = m_boxes[b];
        if (!shown(b)) { box.m_bounds = {}; continue; }
        const int inner = width - 2 * Pad;
        std::vector<std::pair<int, int>> rows;
        int rw = 0, rh = 0;
        for (const Cell& c : box.m_cells) {
            const auto sz = cellSize(c);
            if (rw > 0 && rw + sz.x > inner) { rows.push_back({ rw, rh }); rw = 0; rh = 0; }
            rw += sz.x;
            rh = std::max(rh, sz.y);
        }
        if (rw > 0) rows.push_back({ rw, rh });
        int bw = 0, bh = Title + Pad;
        for (const auto& r : rows) { bw = std::max(bw, r.first); bh += r.second; }
        bw = std::max(bw + 2 * Pad, 120);
        bh += Pad / 2;
        if (x > 0 && x + bw > width) { closeLine(); x = 0; y += lineH + Gap; lineH = 0; }
        box.m_bounds = { area.getX() + x, area.getY() + y, bw, bh };
        line.push_back(b);
        lineH = std::max(lineH, bh);
        if (apply) {
            int cx = 0, cy = Title, row = 0;
            int rowW = rows.empty() ? 0 : rows[0].first, rowH = rows.empty() ? 0 : rows[0].second;
            for (Cell& c : box.m_cells) {
                const auto sz = cellSize(c);
                if (cx > 0 && cx + sz.x > rowW) { cy += rowH; cx = 0; ++row; rowW = rows[static_cast<size_t>(row)].first; rowH = rows[static_cast<size_t>(row)].second; }
                const int left = box.m_bounds.getX() + Pad + (bw - 2 * Pad - rowW) / 2;
                c.m_bounds = { left + cx, box.m_bounds.getY() + cy + (rowH - sz.y) / 2, sz.x, sz.y };
                cx += sz.x;
                juce::Component* comp = m_controls[c.m_control];
                juce::Label* label = m_labels[c.m_control];
                const auto r = c.m_bounds.reduced(3, 2);
                label->setBounds(r.getX(), r.getY(), r.getWidth(), c.m_kind == 4 ? 0 : 16);
                if (c.m_kind == 4) comp->setBounds(c.m_bounds);
                else if (c.m_kind == 0) comp->setBounds(r.withTrimmedTop(16));
                else comp->setBounds(r.getX() + 2, r.getCentreY() - 12, r.getWidth() - 4, 24);
            }
        }
        x += bw + Gap;
    }
    closeLine();
    return y + lineH;
}

int ParamPage::heightFor(int width) const
{
    const_cast<ParamPage*>(this)->plan(width);   // the sections for this width first
    auto* self = const_cast<ParamPage*>(this);
    const std::vector<Box> kept = m_boxes;
    const int h = self->layoutBoxes({ 10, 10, width - 20, 100 }, false);
    self->m_boxes = kept;
    return 20 + top() + h + 10;
}

void ParamPage::paint(juce::Graphics& g)
{
    for (const Box& box : m_boxes) {
        if (!shown(static_cast<size_t>(&box - m_boxes.data()))) continue;
        const auto r = box.m_bounds.toFloat();
        g.setColour(totui::colour::Group);
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(totui::colour::Edge);
        g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
        g.setColour(box.m_colour);
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.drawText(box.m_title.toUpperCase(), box.m_bounds.getX() + 10, box.m_bounds.getY() + 4, box.m_bounds.getWidth() - 20, 14,
                   juce::Justification::centredLeft);
        g.setColour(box.m_colour.withAlpha(0.45f));
        g.fillRect(r.getX() + 10.0f, r.getY() + 19.0f, r.getWidth() - 20.0f, 1.0f);
    }
}

void ParamPage::resized()
{
    auto area = getLocalBounds().reduced(10);
    if (m_instances > 1 || m_split) {
        auto top = area.removeFromTop(26);
        if (m_split) { m_section.setBounds(top.removeFromRight(m_section.bestWidth())); top.removeFromRight(10); }
        m_instance.setBounds(top.removeFromLeft(160));
    }
    area.removeFromTop(6);
    layoutBoxes(area, true);
}

// ---------------------------------------------------------------------------------------------------

ScrollingPage::ScrollingPage(std::unique_ptr<ParamPage> page) : m_page(std::move(page))
{
    m_view.setViewedComponent(m_page.get(), false);
    m_view.setScrollBarsShown(true, false);
    addAndMakeVisible(m_view);
}

void ScrollingPage::resized()
{
    m_view.setBounds(getLocalBounds());
    m_page->setAvailableHeight(getHeight());   // the sections are cut to it (01.10.2026)
    int w = getWidth(), h = m_page->heightFor(w);
    if (h > getHeight()) { w -= m_view.getScrollBarThickness(); h = m_page->heightFor(w); }
    m_page->setSize(w, std::max(h, getHeight()));
}

// ---------------------------------------------------------------------------------------------------

const char* const ArrangeView::LaneNames[Lanes] = { "kick", "hats", "perc", "ping", "bass", "pads" };

ArrangeView::ArrangeView(TotalityProcessor& p, bool detailed) : m_proc(p), m_detailed(detailed)
{
    setTooltip("Click: jump there. Mouse wheel: zoom in and out around the pointer; drag, or Shift + wheel: move along; "
               "double click: the whole length.");
    startTimerHz(20);
}

void ArrangeView::rebuild()
{
    const bool wasMix = m_playing.m_isSet;
    m_proc.copyPlaying(m_playing);
    if (m_playing.m_isSet != wasMix) { m_from = 0.0; m_span = 0.0; }   // a mix after a track (or back): the whole of it
    for (int l = 0; l < Lanes; ++l) { m_hits[l].clear(); m_longest[l] = 0.0; m_typical[l] = 1.0f; }
    m_cellsVersion = -1;
    const double len = m_playing.m_set.m_lengthBeats;
    if (len <= 0.0) return;
    // Which percussion lanes are hats (their role, as the deck sorts them onto its buses).
    const ParamStore& s = m_proc.store();
    bool hat[PercLanes] = {};
    for (int l = 0; l < PercLanes; ++l) {
        const PercRoleE r = static_cast<PercRoleE>(s.getInt(s.id(ModuleE::PERC, l, perc::ROLE)));
        hat[l] = r == PercRoleE::CLOSED_HAT || r == PercRoleE::ROLLING_HAT || r == PercRoleE::OPEN_HAT || r == PercRoleE::RIDE || r == PercRoleE::SHAKER;
    }
    for (const Score& deck : m_playing.m_set.m_decks) {
        for (const NoteEvent& n : deck.m_notes) {
            int lane = -1;
            const int part = static_cast<int>(n.m_part);
            if (n.m_part == PartE::KICK) lane = 0;
            else if (part >= static_cast<int>(PartE::PERC1) && part <= static_cast<int>(PartE::PERC12)) lane = hat[part - static_cast<int>(PartE::PERC1)] ? 1 : 2;
            else if (n.m_part == PartE::PING) lane = 3;
            else if (n.m_part == PartE::SUB || n.m_part == PartE::BASS || n.m_part == PartE::ACID) lane = 4;
            else if (n.m_part == PartE::CHORD || n.m_part == PartE::DRONE || n.m_part == PartE::TEXTURE) lane = 5;
            if (lane < 0 || n.m_velocity <= 0.0f) continue;
            // The drums as short strokes, so their hits stand apart zoomed in; the rest as long as it sounds.
            const double length = lane < 3 ? 0.2 : std::max(n.m_length, 0.125);
            m_hits[lane].push_back({ n.m_beat, n.m_beat + length, std::min(1.0f, n.m_velocity) });
        }
    }
    for (int l = 0; l < Lanes; ++l) {
        std::vector<Hit>& h = m_hits[l];
        std::sort(h.begin(), h.end(), [](const Hit& a, const Hit& b) { return a.m_from < b.m_from; });
        // What lights a lane fully: its cover (length times velocity) per beat over the bars it plays in -- the same at
        // any zoom, from the whole set to a single hit.
        std::vector<char> on(static_cast<size_t>(len / 4.0) + 2, 0);
        double cover = 0.0;
        int bars = 0;
        for (const Hit& x : h) {
            cover += (x.m_to - x.m_from) * x.m_velocity;
            m_longest[l] = std::max(m_longest[l], x.m_to - x.m_from);
            char& c = on[std::min(on.size() - 1, static_cast<size_t>(std::max(0.0, x.m_from) / 4.0))];
            if (c == 0) { c = 1; ++bars; }
        }
        m_typical[l] = bars > 0 ? static_cast<float>(cover / (4.0 * bars)) : 1.0f;
    }
}

void ArrangeView::fill(double from, double to, int columns)
{
    m_cells.assign(static_cast<size_t>(Lanes * columns), 0.0f);
    m_cellsFrom = from;
    m_cellsTo = to;
    m_columns = columns;
    m_cellsVersion = m_version;
    const double per = (to - from) / columns;   // beats a column
    if (per <= 0.0) return;
    for (int l = 0; l < Lanes; ++l) {
        const std::vector<Hit>& h = m_hits[l];
        float* row = m_cells.data() + static_cast<size_t>(l * columns);
        auto it = std::lower_bound(h.begin(), h.end(), from - m_longest[l], [](const Hit& x, double b) { return x.m_from < b; });
        for (; it != h.end() && it->m_from < to; ++it) {
            if (it->m_to <= from) continue;
            const double a = std::max(it->m_from, from), b = std::min(it->m_to, to);
            const int c0 = static_cast<int>((a - from) / per), c1 = std::min(columns - 1, static_cast<int>((b - from) / per));
            for (int c = c0; c <= c1; ++c) {
                const double cs = from + per * c;
                const double over = std::min(b, cs + per) - std::max(a, cs);
                if (over > 0.0) row[c] += static_cast<float>(over / per) * it->m_velocity;
            }
        }
        const float k = 0.85f / std::max(1.0e-3f, m_typical[l]);
        for (int c = 0; c < columns; ++c) row[c] = std::min(1.0f, row[c] * k);
    }
}

void ArrangeView::window(double& from, double& to) const
{
    const double len = m_playing.m_set.m_lengthBeats;
    if (m_span <= 0.0 || m_span >= len) { from = 0.0; to = std::max(len, 1.0); return; }
    from = std::clamp(m_from, 0.0, len - m_span);
    to = from + m_span;
}

bool ArrangeView::zoomed(double& from, double& to) const
{
    window(from, to);
    return m_span > 0.0 && m_span < m_playing.m_set.m_lengthBeats;
}

void ArrangeView::show(double from, double span)
{
    const double len = m_playing.m_set.m_lengthBeats;
    if (len <= 0.0) return;
    span = std::clamp(span, std::min(len, Narrowest), len);
    if (span >= len - 1.0e-9) { m_from = 0.0; m_span = 0.0; return; }
    m_span = span;
    m_from = std::clamp(from, 0.0, len - span);
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
    const double pos = m_proc.positionBeats();
    double a = 0.0, b = 0.0;
    if (zoomed(a, b) && !m_dragged && a == m_lastFrom && b == m_lastTo && m_lastPos >= a && m_lastPos <= b && (pos < a || pos > b)) {
        show(pos - 0.05 * (b - a), b - a);
        window(a, b);
    }
    m_lastPos = pos;
    m_lastFrom = a;
    m_lastTo = b;
    repaint();
}

void ArrangeView::paint(juce::Graphics& g)
{
    g.fillAll(Panel);
    if (m_proc.scoreVersion() != m_version) { m_version = m_proc.scoreVersion(); rebuild(); }
    const double beats = m_playing.m_set.m_lengthBeats;
    if (m_wanted.second > m_wanted.first && beats > 0.0) {
        show(m_wanted.first, m_wanted.second - m_wanted.first);
        m_wanted = { 0.0, 0.0 };
    }
    if (beats <= 0.0) {
        g.setColour(Dim);
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
    const float head = m_detailed ? 44.0f : 24.0f, rule = m_detailed ? 16.0f : 12.0f, ry = h - rule;
    g.setFont(juce::FontOptions(11.0f));
    if (m_playing.m_isSet) {
        // The tracks on their decks: A on top, B below it; a blend where two overlap.
        const float rowH = (head - 4.0f) / 2.0f;
        for (size_t i = 0; i < m_playing.m_tracks.size(); ++i) {
            const TrackPlace& t = m_playing.m_tracks[i];
            const float y = 2.0f + rowH * static_cast<float>(t.m_deck == 1 ? 1 : 0);
            const float x0 = xOf(t.m_start), x1 = xOf(t.m_end), xs = xOf(t.m_swapIn);
            if (x1 < 0.0f || x0 > w) continue;
            const auto r = clipped(x0 + 1.0f, x1 - 1.0f, y, rowH - 2.0f);
            g.setColour(totui::deckColour(t.m_deck).interpolatedWith(Back, 0.7f));
            g.fillRect(r.withWidth(std::max(1.0f, r.getWidth())));
            g.setColour(totui::deckColour(t.m_deck).interpolatedWith(Back, 0.45f));
            g.fillRect(xs, y, 2.0f, rowH - 2.0f);   // its swap: from here it owns the low end
            g.setColour(Ink);
            const juce::String name = "T" + juce::String(static_cast<int>(i) + 1) + " " + juce::String(t.m_info.m_style) + " " + juce::String(t.m_info.m_camelot);
            if (r.getWidth() > 40.0f) g.drawFittedText(name, r.reduced(4.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, 1);
        }
    } else {
        const std::vector<Marker>& markers = m_playing.m_set.m_decks[0].m_markers;
        for (size_t i = 0; i < markers.size(); ++i) {
            const double b0 = markers[i].m_beat, b1 = i + 1 < markers.size() ? markers[i + 1].m_beat : beats;
            const float x0 = xOf(b0), x1 = xOf(b1);
            if (x1 < 0.0f || x0 > w) continue;
            const juce::String name(markers[i].m_text);
            const auto r = clipped(x0 + 1.0f, x1 - 1.0f, 2.0f, head - 4.0f);
            g.setColour(blockColour(name));
            g.fillRect(r.withWidth(std::max(1.0f, r.getWidth())));
            g.setColour(Ink);
            if (r.getWidth() > 30.0f) g.drawFittedText(name, r.reduced(3.0f, 2.0f).withHeight(16.0f).toNearestInt(), juce::Justification::topLeft, 1);
        }
        if (m_detailed) {
            // The operations of the form, named, under the blocks.
            g.setFont(juce::FontOptions(10.0f));
            float free = 0.0f;   // where the next name may start without covering the last
            for (const BlockOp& o : m_playing.m_set.m_decks[0].m_ops) {
                if (o.m_kind == OpKindE::END || o.m_kind == OpKindE::HOLD) continue;
                const float x = xOf(o.m_beat);
                if (x < -1.0f || x > w) continue;
                g.setColour(Onset.withAlpha(0.8f));
                g.fillRect(x, head - 18.0f, 1.5f, 16.0f);
                juce::String t = OpNames[static_cast<int>(o.m_kind)];
                if (o.m_layer >= 0 && o.m_layer < NumLayers && o.m_kind != OpKindE::KICK_OUT && o.m_kind != OpKindE::RETURN) t << " " << LayerNames[o.m_layer];
                if (x + 3.0f < free) continue;
                const float tw = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), t) + 8.0f;
                g.setColour(Dim);
                g.drawText(t, static_cast<int>(x) + 3, static_cast<int>(head - 18.0f), static_cast<int>(tw), 16, juce::Justification::centredLeft);
                free = x + 3.0f + tw;
            }
        }
    }
    // The ruler: a track's bars, a mix's minutes -- at a step that leaves the numbers room; faint lines through the lanes.
    {
        const TempoMap& tm = m_playing.m_set.m_decks[0].m_tempo;
        const auto clock = [](double secs) {
            const int s = static_cast<int>(std::lround(secs)), hours = s / 3600, mins = (s / 60) % 60;
            return (hours > 0 ? juce::String(hours) + ":" + juce::String(mins).paddedLeft('0', 2) : juce::String(mins)) + ":"
                   + juce::String(s % 60).paddedLeft('0', 2);
        };
        const float room = m_detailed ? 80.0f : 52.0f;
        g.setFont(juce::FontOptions(m_detailed ? 10.5f : 9.0f));
        const auto tick = [&](double beat, const juce::String& label) {
            const float x = xOf(beat);
            g.setColour(Ink.withAlpha(0.07f));
            g.fillRect(x, head, 1.0f, ry - head);
            g.setColour(Dim);
            g.fillRect(x, ry, 1.0f, 4.0f);
            g.drawText(label, juce::Rectangle<float>(x + 3.0f, ry, room, rule), juce::Justification::centredLeft);
        };
        if (m_playing.m_isSet) {
            const double s0 = tm.secondsAt(a), s1 = tm.secondsAt(b);
            static const int ClockSteps[] = { 5, 10, 15, 30, 60, 120, 300, 600, 900, 1800, 3600 };
            int step = 3600;
            for (int k : ClockSteps) if (static_cast<double>(k) * w / std::max(1.0, s1 - s0) >= room) { step = k; break; }
            for (double t = std::ceil(s0 / step) * step; t <= s1; t += step) tick(tm.beatAt(t), clock(t));
        } else {
            const double perBar = w / std::max(1.0e-9, (b - a) / 4.0);
            int step = 1;
            while (step * perBar < room && step < 4096) step *= 2;
            for (int bar = static_cast<int>(std::ceil(a / 4.0 / step)) * step; 4.0 * bar <= b; bar += step)
                tick(4.0 * bar, juce::String(bar + 1) + (m_detailed ? "   " + clock(tm.secondsAt(4.0 * bar)) : juce::String()));
        }
    }
    // The matrix: a lane per group of layers, lit where it plays (a column per pixel, like columns drawn as one).
    const float top = head + 2.0f, laneH = (ry - top - 1.0f) / static_cast<float>(Lanes);
    const int columns = std::max(1, getWidth());
    if (m_cellsVersion != m_version || m_cellsFrom != a || m_cellsTo != b || m_columns != columns) fill(a, b, columns);
    if (laneH > 2.0f) {
        const auto level = [](float v) { return v > 0.0f ? std::max(1, static_cast<int>(v * 24.0f + 0.5f)) : 0; };
        for (int l = 0; l < Lanes; ++l) {
            const float y = top + laneH * static_cast<float>(l);
            const float* row = m_cells.data() + static_cast<size_t>(l * columns);
            for (int c = 0; c < columns;) {
                const int q = level(row[c]);
                int e = c + 1;
                while (e < columns && level(row[e]) == q) ++e;
                if (q > 0) {
                    g.setColour(Ink.withAlpha(0.12f + 0.5f * static_cast<float>(q) / 24.0f));
                    g.fillRect(static_cast<float>(c), y + 1.0f, static_cast<float>(e - c), laneH - 2.0f);
                }
                c = e;
            }
        }
        if (laneH >= 7.0f) {
            g.setFont(juce::FontOptions(std::min(10.0f, laneH)));
            for (int l = 0; l < Lanes; ++l) {
                const juce::Rectangle<float> r(2.0f, top + laneH * static_cast<float>(l), 34.0f, laneH);
                g.setColour(Panel.withAlpha(0.75f));
                g.fillRect(r);
                g.setColour(Dim);
                g.drawText(LaneNames[l], r.reduced(2.0f, 0.0f), juce::Justification::centredLeft);
            }
        }
    }
    // Zoomed: where the window lies in the whole, under the ruler.
    if (zoom) {
        g.setColour(Ink.withAlpha(0.1f));
        g.fillRect(0.0f, h - 2.0f, w, 2.0f);
        g.setColour(Accent.withAlpha(0.85f));
        g.fillRect(w * static_cast<float>(a / beats), h - 2.0f, std::max(3.0f, w * static_cast<float>((b - a) / beats)), 2.0f);
    }
    // The window of the Arrange tab, while it is zoomed and shown, marked on the strip.
    double da = 0.0, db = 0.0;
    if (m_detail != nullptr && m_detail->isShowing() && m_detail->zoomed(da, db)) {
        const float x0 = xOf(da), x1 = std::max(x0 + 2.0f, xOf(db));
        g.setColour(Accent.withAlpha(0.10f));
        g.fillRect(x0, 0.0f, x1 - x0, h);
        g.setColour(Accent.withAlpha(0.7f));
        g.drawRect(juce::Rectangle<float>(x0, 0.0f, x1 - x0, h), 1.0f);
    }
    const float x = xOf(m_proc.positionBeats());
    if (x >= -1.0f && x <= w + 1.0f) {
        g.setColour(Onset);
        g.fillRect(x - 1.0f, 0.0f, 2.0f, h);
    }
}

void ArrangeView::mouseDown(const juce::MouseEvent& e)
{
    m_downX = e.position.x;
    m_dragged = false;
    double a = 0.0, b = 0.0;
    const bool zoom = zoomed(a, b);
    m_downFrom = a;
    if (!zoom) m_proc.seekTo(beatAt(e.position.x));   // the whole length: at once, as ever
}

void ArrangeView::mouseDrag(const juce::MouseEvent& e)
{
    double a = 0.0, b = 0.0;
    if (!zoomed(a, b)) return;
    const float dx = e.position.x - m_downX;
    if (!m_dragged && std::abs(dx) < 4.0f) return;
    m_dragged = true;
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    show(m_downFrom - (b - a) * dx / std::max(1, getWidth()), b - a);
    repaint();
}

void ArrangeView::mouseUp(const juce::MouseEvent& e)
{
    if (m_dragged) {
        m_dragged = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        return;
    }
    double a = 0.0, b = 0.0;
    if (zoomed(a, b)) m_proc.seekTo(beatAt(e.position.x));
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

ArrangePage::ArrangePage(TotalityProcessor& p) : m_proc(p), m_view(p, true)
{
    addAndMakeVisible(m_view);
    m_which.setColour(juce::Label::textColourId, Ink);
    m_which.setMinimumHorizontalScale(0.6f);
    addAndMakeVisible(m_which);
    m_sounds.setColour(juce::Label::textColourId, Dim);
    m_sounds.setMinimumHorizontalScale(0.7f);
    addAndMakeVisible(m_sounds);
    for (const char* unit : UnitNames) {
        // "hands" is the stream of the knobs' moves (the automation); named so on the panel, where a hand means the
        // headset's (01.10.2026). The stream keeps its name: the sets store it.
        auto* b = m_rerolls.add(new juce::TextButton(juce::String("reroll ") + (juce::String(unit) == "hands" ? "moves" : unit)));
        const juce::String u(unit);
        b->onClick = [this, u] { m_proc.reroll(m_prefix + u); };
        addAndMakeVisible(b);
    }
    m_track.onClick = [this] { if (m_prefix.isNotEmpty()) m_proc.reroll(m_prefix.dropLastCharacters(1)); };
    m_set.onClick = [this] { m_proc.reroll("set"); };
    addAndMakeVisible(m_track);
    addAndMakeVisible(m_set);
    startTimerHz(4);
}

void ArrangePage::timerCallback()
{
    Playing p;
    m_proc.copyPlaying(p);
    const int t = m_proc.trackAt(m_proc.positionBeats());
    m_prefix = p.m_isSet && t >= 0 ? "track" + juce::String(t + 1) + "." : juce::String();
    juce::String text;
    if (t >= 0 && t < static_cast<int>(p.m_tracks.size())) {
        const TrackInfo& i = p.m_tracks[static_cast<size_t>(t)].m_info;
        text << (p.m_isSet ? "Track " + juce::String(t + 1) + " of " + juce::String(static_cast<int>(p.m_tracks.size())) + " in the mix: " : juce::String("The track: "))
             << juce::String(i.m_style) << " " << ArchetypeNames[i.m_archetype] << ", " << FormNames[static_cast<int>(i.m_form)] << ", " << juce::String(i.m_bpm, 1) << " BPM, "
             << KeyNames[i.m_key] << " " << ScaleNames[i.m_scale] << " (" << juce::String(i.m_camelot) << "), " << i.m_bars << " bars, "
             << (i.m_subOwns ? "the sub owns the low end" : "the rumble owns the low end") << ", bar similarity " << juce::String(i.m_similarity, 2);
        // Phase 8: the figure and where the waves land.
        if (i.m_figure >= 0)
            text << "; figure: the " << LayerNames[i.m_figure] << " from bar " << (i.m_figureBar + 1);
        if (!i.m_landings.empty()) {
            text << ", waves land on";
            for (int l : i.m_landings) text << " " << (l + 1);
        }
    }
    m_which.setText(text, juce::dontSendNotification);
    // The composer's presets of the track whose sounds the knobs show.
    juce::String sounds;
    for (ModuleE m : { ModuleE::KICK, ModuleE::RUMBLE, ModuleE::SUB, ModuleE::PING, ModuleE::BASS, ModuleE::ACID, ModuleE::CHORD, ModuleE::DRONE, ModuleE::TEXTURE }) {
        const int index = m_proc.composedPreset(m, 0);
        if (index < 0) continue;
        const std::string& k0 = m_proc.store().key(m_proc.store().id(m, 0, 0));
        sounds << (sounds.isEmpty() ? "Sounds: " : ", ") << juce::String(k0.substr(0, k0.find('.'))) << " " << factoryPresets(m)[static_cast<size_t>(index)].m_name;
    }
    m_sounds.setText(sounds, juce::dontSendNotification);
    m_track.setVisible(p.m_isSet);
    m_set.setVisible(p.m_isSet);
}

void ArrangePage::resized()
{
    auto r = getLocalBounds().reduced(10);
    m_which.setBounds(r.removeFromTop(22));
    m_sounds.setBounds(r.removeFromTop(20));
    r.removeFromTop(4);
    auto row = r.removeFromTop(28);
    // Nine units and the two larger ones in a row of the window's usual width (1180).
    for (auto* b : m_rerolls) b->setBounds(row.removeFromLeft(94).reduced(2));
    row.removeFromLeft(8);
    m_track.setBounds(row.removeFromLeft(152).reduced(2));
    m_set.setBounds(row.removeFromLeft(140).reduced(2));
    r.removeFromTop(8);
    m_view.setBounds(r);
}

void ArrangePage::paint(juce::Graphics&) {}

// ---------------------------------------------------------------------------------------------------

ExportPage::ExportPage(TotalityProcessor& p) : m_proc(p)
{
    m_wav.onClick = [this] { exportWith(0); };
    m_stems.onClick = [this] { exportWith(TotalityProcessor::STEMS); };
    m_loops.onClick = [this] { exportWith(TotalityProcessor::LOOPS); };
    m_all.onClick = [this] { exportWith(TotalityProcessor::STEMS | TotalityProcessor::LOOPS); };
    m_save.onClick = [this] { save(); };
    m_load.onClick = [this] { load(); };
    m_save.setTooltip("Save the set: seed, lengths, rerolls and every changed knob (Ctrl+S)");
    m_load.setTooltip("Load a set and compose it (Ctrl+O)");
    m_wav.setTooltip("Export what plays (Ctrl+E)");
    for (auto* b : { &m_wav, &m_stems, &m_loops, &m_all }) addAndMakeVisible(b);
    addAndMakeVisible(m_save);
    addAndMakeVisible(m_load);
    m_status.setColour(juce::Label::textColourId, Ink);
    addAndMakeVisible(m_status);
    m_cue = std::make_unique<ParamPage>(m_proc, std::vector<std::pair<ModuleE, int>>{ { ModuleE::CUE, 0 } });
    addAndMakeVisible(*m_cue);
    startTimerHz(4);
}

void ExportPage::save()
{
    documents().createDirectory();
    m_chooser = std::make_unique<juce::FileChooser>("Save set", documents().getChildFile("totality.totset"), "*.totset");
    m_chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                          [this](const juce::FileChooser& fc) { if (fc.getResult() != juce::File()) m_proc.saveSet(fc.getResult().withFileExtension(".totset")); });
}

void ExportPage::load()
{
    m_chooser = std::make_unique<juce::FileChooser>("Load set", documents(), "*.totset");
    m_chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this](const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) m_proc.loadSet(fc.getResult()); });
}

void ExportPage::exportWith(int extras)
{
    documents().createDirectory();
    m_chooser = std::make_unique<juce::FileChooser>("Export", documents().getChildFile("totality.wav"), "*.wav");
    m_chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, extras](const juce::FileChooser& fc) {
                              if (fc.getResult() != juce::File()) m_proc.exportTo(fc.getResult().withFileExtension(".wav"), extras);
                          });
}

void ExportPage::timerCallback() { m_status.setText(m_proc.status(), juce::dontSendNotification); }

void ExportPage::resized()
{
    auto r = getLocalBounds().reduced(16);
    auto row = r.removeFromTop(30);
    for (auto* b : { &m_wav, &m_stems, &m_loops, &m_all }) b->setBounds(row.removeFromLeft(170).reduced(3));
    row.removeFromLeft(24);
    m_save.setBounds(row.removeFromLeft(frame::IconWidth).reduced(3));
    m_load.setBounds(row.removeFromLeft(frame::IconWidth).reduced(3));
    r.removeFromTop(6);
    m_status.setBounds(r.removeFromTop(22));
    r.removeFromTop(150);   // the text (paint)
    m_cue->setBounds(r.removeFromTop(m_cue->heightFor(r.getWidth())).withWidth(std::min(r.getWidth(), 420)));
}

void ExportPage::paint(juce::Graphics& g)
{
    g.setColour(Dim);
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

TotalityEditor::TotalityEditor(TotalityProcessor& p) : juce::AudioProcessorEditor(p), m_proc(p), m_arrange(p, false)
{
    setLookAndFeel(&m_lnf);
    addAndMakeVisible(m_body);
    // The backdrop (strong behind the header, faint behind the pages), the logo and the name: the frame's (Frame.h).
    m_body.m_painter = [this](juce::Graphics& g) {
        m_backdrop.paint(g, m_body.getLocalBounds(), m_headerBottom, totui::skin(), frame::Settings::of("Totality").backdrop());
        drawLogo(g, m_logo);
        frame::drawTitle(g, totui::skin(), m_title.getBounds().toFloat(), 21.0f);
    };
    m_body.m_onResize = [this] { layoutBody(); };
    frame::Settings::of("Totality").addChangeListener(this);
    ParamStore& s = m_proc.store();
    m_title.setInterceptsMouseClicks(false, false);
    m_body.addAndMakeVisible(m_title);

    auto combo = [&](juce::ComboBox& box, int id) {
        const ParamDesc& d = s.desc(id);
        for (int c = 0; c <= static_cast<int>(d.m_maxValue); ++c) box.addItem(d.m_choices[c], c + 1);
        m_combos.push_back(std::make_unique<juce::ComboBoxParameterAttachment>(*m_proc.parameter(id), box));
        m_body.addAndMakeVisible(box);
    };
    combo(m_style, s.id(ModuleE::COMPOSE, 0, compose::STYLE));
    combo(m_key, s.id(ModuleE::COMPOSE, 0, compose::KEY));
    combo(m_scale, s.id(ModuleE::COMPOSE, 0, compose::SCALE));
    m_style.setTooltip("The style the next track is composed in");
    m_key.setTooltip("The key");
    m_scale.setTooltip("The scale");
    // One track or a DJ mix (a set of tracks on two decks): two buttons that compose what they name, and one length,
    // the one of what is chosen (compose.minutes or set.minutes).
    m_trackMode.setTooltip("Compose a single track (its length beside)");
    m_mixMode.setTooltip("Compose a DJ mix: a set of tracks mixed on two decks (its length beside; a track's time in it and the "
                        "set's course on the Set page)");
    m_trackMode.onClick = [this] { m_proc.chooseMix(false); };
    m_mixMode.onClick = [this] { m_proc.chooseMix(true); };
    frame::connectModes({ &m_trackMode, &m_mixMode }, Accent.withAlpha(0.42f));
    for (auto* b : { &m_trackMode, &m_mixMode }) m_body.addAndMakeVisible(b);
    m_length.setSliderStyle(juce::Slider::LinearHorizontal);
    m_length.setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 20);
    m_length.onValueChange = [this] {
        if (m_syncing) return;
        const ParamStore& st = m_proc.store();
        m_proc.setFromUi(m_lengthOfMix ? st.id(ModuleE::SET, 0, set::MINUTES) : st.id(ModuleE::COMPOSE, 0, compose::MINUTES),
                        static_cast<float>(m_length.getValue()));
    };
    m_length.onDragStart = [this] { m_proc.beginStep("Length"); };   // a drag is one step, not one per pixel
    m_length.onDragEnd = [this] { m_proc.endStep(); };
    m_lengthLabel.setText("Length", juce::dontSendNotification);
    m_lengthLabel.setColour(juce::Label::textColourId, Dim);
    m_lengthLabel.setJustificationType(juce::Justification::centredRight);
    m_body.addAndMakeVisible(m_length);
    m_body.addAndMakeVisible(m_lengthLabel);
    showLength(m_proc.mixChosen());

    m_compose.onClick = [this] { m_proc.compose(); };
    m_seed.onClick = [this] { m_proc.newSeed(); };
    m_play.onClick = [this] { m_proc.setPlaying(!m_proc.isPlaying()); };
    m_compose.setTooltip("Compose with the knobs as they stand, the seed and the rerolls");
    m_seed.setTooltip("A new seed: another track (or mix) from the same settings");
    m_play.setTooltip("Play and stop (Space)");
    m_body.addAndMakeVisible(m_compose);
    for (auto* b : { &m_seed, &m_play }) m_body.addAndMakeVisible(b);
    // Mute, as in Phosphene: silence at the output; TOT_MUTE (or the screenshot mode) holds it on.
    m_mute.setClickingTogglesState(true);
    m_mute.setToggleState(m_proc.muted(), juce::dontSendNotification);
    m_mute.setEnabled(!m_proc.muteForced() || std::getenv("TOT_SHOT") != nullptr);
    m_mute.setColour(juce::TextButton::buttonOnColourId, totui::colour::Red.withAlpha(0.55f));
    m_mute.setTooltip(m_proc.muteForced() ? "Muted by TOT_MUTE: an automated run makes no sound" : "Silence the output");
    m_mute.onClick = [this] { m_proc.setMuted(m_mute.getToggleState()); };
    m_body.addAndMakeVisible(m_mute);
    m_play.setColour(juce::TextButton::buttonColourId, Accent.withAlpha(0.2f));
    m_compose.setColour(juce::TextButton::buttonColourId, Accent.withAlpha(0.12f));
    // The update check (the settings switch it): once a day it asks GitHub for the latest release; a newer one shows here.
    m_update.setColour(juce::HyperlinkButton::textColourId, Accent);
    m_update.setTooltip("Open the release page");
    m_body.addChildComponent(m_update);
    // The tools at the right end of the second row: undo, redo, help, settings -- and the headset, while one sends.
    m_undo.onClick = [this] { m_proc.undo(); };
    m_redo.onClick = [this] { m_proc.redo(); };
    m_help.onClick = [this] { showHelp(m_helpView == nullptr || !m_helpView->isVisible()); };
    m_settings.onClick = [this] { showSettings(); };
    m_headsetIcon.onClick = [this] {
        for (int i = 0; i < m_tabs.getNumTabs(); ++i)
            if (m_tabs.getTabNames()[i] == "Perform") m_tabs.setCurrentTabIndex(i);
    };
    m_search.onClick = [this] { openPalette(); };
    for (auto* b : { &m_undo, &m_redo, &m_search, &m_help, &m_settings }) m_body.addAndMakeVisible(b);
    m_body.addChildComponent(m_headsetIcon);
    // Phase 17: rate the track under the playhead; with Favor Ratings (the Set page) the ratings weigh what comes.
    m_like.onClick = [this] { m_rated = m_proc.rate(1); m_ratedTicks = 60; };
    m_dislike.onClick = [this] { m_rated = m_proc.rate(-1); m_ratedTicks = 60; };
    m_body.addAndMakeVisible(m_like);
    m_body.addAndMakeVisible(m_dislike);
    m_rerolls.setColour(juce::Label::textColourId, Dim);
    m_rerolls.setJustificationType(juce::Justification::centredRight);
    m_status.setColour(juce::Label::textColourId, Ink);
    m_status.setMinimumHorizontalScale(0.8f);
    m_body.addAndMakeVisible(m_rerolls);
    m_body.addAndMakeVisible(m_status);

    m_body.addAndMakeVisible(m_arrange);
    using MT = ModuleE;
    auto page = [&](const char* name, std::vector<std::pair<MT, int>> groups, int instances = 1, std::vector<juce::String> names = {}) {
        m_tabs.addTab(name, Panel, new ScrollingPage(std::make_unique<ParamPage>(m_proc, std::move(groups), instances, std::move(names))), true);
    };
    // A tab of several modules: a small tab each (frame::SubTabs, 01.10.2026), so that no page has to scroll far.
    auto modules = [&](const char* name, std::vector<std::pair<MT, const char*>> mods) {
        auto st = std::make_unique<frame::SubTabs>(totui::skin());
        for (const auto& mod : mods) {
            const MT m = mod.first;
            st->add(mod.second, [this, m] {
                return std::unique_ptr<juce::Component>(new ScrollingPage(std::make_unique<ParamPage>(m_proc, std::vector<std::pair<MT, int>>{ { m, 0 } })));
            });
        }
        m_tabs.addTab(name, Panel, st.release(), true);
    };
    page("Set", { { MT::COMPOSE, 0 }, { MT::SET, 0 }, { MT::DJ_FX, 0 } });
    auto* arrangePage = new ArrangePage(m_proc);
    m_tabs.addTab("Arrange", Panel, arrangePage, true);
    m_arrange.setDetail(&arrangePage->view());
    m_tabs.addTab("Patterns", Panel, new EclipsePage(m_proc), true);
    modules("Low End", { { MT::KICK, "Kick" }, { MT::RUMBLE, "Rumble" }, { MT::SUB, "Sub" } });
    std::vector<juce::String> lanes;
    for (int l = 0; l < PercLanes; ++l) lanes.push_back("Lane " + juce::String(l + 1));
    page("Drums", { { MT::PERC, 0 } }, PercLanes, lanes);
    modules("Tones", { { MT::PING, "Ping" }, { MT::BASS, "Bass" }, { MT::ACID, "Acid" }, { MT::CHORD, "Chord" }, { MT::DRONE, "Drone" },
                       { MT::TEXTURE, "Texture" } });
    modules("Dub", { { MT::DUB, "Dub" }, { MT::SPACE, "Space" }, { MT::CLOUD, "Cloud" } });
    m_tabs.addTab("Mixer", Panel, new MixerPage(m_proc), true);
    m_tabs.addTab("Perform", Panel, new PerformPage(m_proc), true);
    m_tabs.addTab("Export", Panel, new ExportPage(m_proc), true);
    m_tabs.addTab("Style", Panel, new StylePage(m_proc), true);
    m_tabs.setOutline(0);
    m_body.addAndMakeVisible(m_tabs);
    // The family's ways around the panel (15.10.2026, Frame.h): the info line at the bottom, and over everything while
    // open the command palette, the preset browser and the first steps.
    m_info = std::make_unique<frame::InfoLine>(totui::skin(), m_body);
    m_info->m_describe = [this](int id, juce::String& name, juce::String& text) { return describeControl(id, name, text); };
    m_body.addAndMakeVisible(*m_info);
    for (juce::Component* c : { static_cast<juce::Component*>(&m_palette), static_cast<juce::Component*>(&m_browser), static_cast<juce::Component*>(&m_tour) })
        m_body.addChildComponent(c);
    m_tour.m_onDone = [] { frame::Settings::of("Totality").setValue("tourDone", "1"); };
    frame::keepKeysForEditor(m_body);
    setWantsKeyboardFocus(true);

    setResizable(true, true);
    setResizeLimits(800, 520, 4800, 3100);
    setSize(1280, 860);   // the family's window size (01.10.2026)

    if (const char* shot = std::getenv("TOT_SHOT")) {
        m_shotPath = shot;
        if (const char* tab = std::getenv("TOT_TAB")) m_tabs.setCurrentTabIndex(juce::String(tab).getIntValue());
        // TOT_SUBTAB: the small tab of a tab of several (Low End, Tones, Dub, Mixer).
        if (const char* st = std::getenv("TOT_SUBTAB"))
            if (auto* subs = findSubTabs(m_tabs.getCurrentContentComponent())) subs->show(juce::String(st).getIntValue());
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
            hands.m_height[0] = 0.62f;
            hands.m_height[1] = 0.8f;
            hands.m_tracked[0] = hands.m_tracked[1] = true;
            m_proc.headset().inject(hands);
            m_shotHands = true;
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

void TotalityEditor::paint(juce::Graphics& g) { g.fillAll(Back); }

void TotalityEditor::resized()
{
    // The body at the design size (1180 x 760), scaled to the window by its height, and as wide as the window allows.
    const float scale = juce::jlimit(0.5f, 4.0f, std::min(static_cast<float>(getWidth()) / 1180.0f, static_cast<float>(getHeight()) / 760.0f));
    m_body.setTransform(juce::AffineTransform::scale(scale));
    m_body.setBounds(0, 0, juce::roundToInt(static_cast<float>(getWidth()) / scale), juce::roundToInt(static_cast<float>(getHeight()) / scale));
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
        // The standalone's window back where it was left (15.10.2026; not in the screenshot and report modes).
        if (std::getenv("TOT_SHOT") == nullptr && std::getenv("TOT_PAGE_REPORT") == nullptr) safe->m_window.attach(*safe, "Totality");
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
    for (int i = 0; i < m_tabs.getNumTabs(); ++i)
        if (auto* e = dynamic_cast<ExportPage*>(m_tabs.getTabContentComponent(i))) return e;
    return nullptr;
}

bool TotalityEditor::keyPressed(const juce::KeyPress& key)
{
    frame::Keys k;
    if (standalone()) {   // in a host, Space and F11 are the host's
        k.m_playStop = [this] { m_proc.setPlaying(!m_proc.isPlaying()); };
        k.m_fullScreen = [this] { toggleFullScreen(); };
    }
    k.m_undo = [this] { m_proc.undo(); };
    k.m_redo = [this] { m_proc.redo(); };
    k.m_help = [this] { showHelp(m_helpView == nullptr || !m_helpView->isVisible()); };
    k.m_escape = [this] {
        if (m_helpView != nullptr && m_helpView->isVisible()) showHelp(false);
        else if (fullScreen()) toggleFullScreen();
    };
    k.m_save = [this] { if (auto* e = exportPage()) e->save(); };
    k.m_open = [this] { if (auto* e = exportPage()) e->load(); };
    k.m_exportFile = [this] { if (auto* e = exportPage()) e->exportWith(0); };
    k.m_palette = [this] { openPalette(); };
    return frame::handleKey(key, k);
}

void TotalityEditor::showHelp(bool on)
{
    if (on && m_helpView == nullptr) {
        m_helpView = std::make_unique<frame::HelpView>(juce::String::fromUTF8(TotalityData::chapters_txt, TotalityData::chapters_txtSize), totui::skin());
        m_helpView->m_onClose = [this] { showHelp(false); };
        m_helpView->m_extraTopics = [this] {
            std::vector<std::pair<juce::String, juce::String>> t;
            juce::Component* page = m_tabs.getCurrentContentComponent();
            juce::String name = m_tabs.getCurrentTabName();
            if (auto* st = dynamic_cast<frame::SubTabs*>(page)) {   // a tab of several modules: its small tab in front
                name << ": " << st->name(st->current());
                page = st->page();
            }
            if (auto* sp = dynamic_cast<ScrollingPage*>(page)) t.emplace_back("This tab: " + name, sp->page().describe());
            t.emplace_back("Keys", frame::keysText());
            t.emplace_back("Headset (Meta Quest)", frame::headsetGrammar("the kick out and in", {}) + "\n\n" + m_proc.headset().statusText());
            return t;
        };
        m_body.addChildComponent(*m_helpView);
    }
    m_help.setToggleState(on, juce::dontSendNotification);
    if (m_helpView == nullptr) return;
    if (on) m_helpView->refresh();
    m_helpView->setVisible(on);
    layoutBody();
    if (on) m_helpView->grabKeyboardFocus();
    else grabKeyboardFocus();
}

void TotalityEditor::showSettings()
{
    frame::SettingsMenu m;
    m.m_app = "Totality";
    m.m_version = JucePlugin_VersionString;
    m.m_updatesOn = [this] { return m_updates->enabled(); };
    m.m_setUpdates = [this](bool on) { m_updates->setEnabled(on); };
    m.m_canFullScreen = [this] { return standalone(); };
    m.m_isFullScreen = [this] { return fullScreen(); };
    m.m_toggleFullScreen = [this] { toggleFullScreen(); };
    m.m_setWindowScale = [this](float k) {
        if (!fullScreen()) setSize(juce::roundToInt(1280.0f * k), juce::roundToInt(860.0f * k));
    };
    m.m_headsetStatus = [this] { return m_proc.headset().statusText(); };
    if (m_proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone)   // Ableton Link, the standalone only (02.10.2026)
        m.m_linkStatus = [this] { return m_proc.linkStatus(); };
    m.m_jamStatus = [this] { return m_proc.jamStatus(); };   // the family jam (02.10.2026), plugin and standalone
    m.m_about = [] { return juce::String("Hypnotic Berlin techno, composed and synthesised.\ngithub.com/reneweller-coding/Totality"); };
    m.m_moreItems = [this](juce::PopupMenu& pm) {
        pm.addItem("Search: any control, preset or action (Ctrl+K) ...", [this] { openPalette(); });
        pm.addItem("First steps (the tour) ...", [this] { startTour(); });
    };
    m.show(m_settings);
}

void TotalityEditor::changeListenerCallback(juce::ChangeBroadcaster*) { layoutBody(); m_body.repaint(); }   // the settings changed

void TotalityEditor::showLength(bool mix)
{
    // A track: 2 to 16 minutes; a mix: 10 minutes to 12 hours, its first two hours over most of the way.
    const juce::ScopedValueSetter<bool> quiet(m_syncing, true);
    m_lengthOfMix = mix;
    if (mix) {
        m_length.setRange(10.0, 720.0, 1.0);
        m_length.setSkewFactorFromMidPoint(90.0);
        m_length.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v)) + " min"; };
        m_length.setTooltip("The DJ mix's length (Compose mix makes it)");
    } else {
        m_length.setRange(2.0, 16.0, 0.1);
        m_length.setSkewFactor(1.0);
        m_length.textFromValueFunction = [](double v) { return juce::String(v, 1) + " min"; };
        m_length.setTooltip("The track's length (Compose track makes it)");
    }
    m_length.valueFromTextFunction = [](const juce::String& t) { return t.getDoubleValue(); };
    const ParamStore& st = m_proc.store();
    m_length.setValue(st.get(mix ? st.id(ModuleE::SET, 0, set::MINUTES) : st.id(ModuleE::COMPOSE, 0, compose::MINUTES)), juce::dontSendNotification);
    m_length.updateText();
}

void TotalityEditor::layoutBody()
{
    auto area = m_body.getLocalBounds().reduced(10);
    frame::Header h;
    h.m_logo = &m_logo;
    h.m_title = &m_title;
    h.m_titleWidth = static_cast<int>(frame::titleWidth(totui::skin(), 21.0f)) + 12;
    h.m_choices = { { &m_style, 116 }, { &m_key, 62 }, { &m_scale, 116 } };
    h.m_modes = { { &m_trackMode, 64 }, { &m_mixMode, 72 } };
    h.m_lengthLabel = &m_lengthLabel;
    h.m_length = &m_length;
    h.m_actions = { { &m_compose, 116 }, { &m_seed, frame::IconWidth } };
    h.m_play = &m_play;
    h.m_mute = &m_mute;
    h.m_like = &m_like;
    h.m_dislike = &m_dislike;
    h.m_status = &m_status;
    h.m_curation = &m_rerolls;
    h.m_update = &m_update;
    h.m_tools = { &m_headsetIcon, nullptr, &m_undo, &m_redo, nullptr, &m_search, &m_help, &m_settings };
    frame::layoutHeader(area, h);
    m_headerBottom = area.getY();
    // The info line along the bottom (15.10.2026); the overlays over the whole body.
    if (m_info != nullptr) {
        m_info->setBounds(area.removeFromBottom(frame::InfoLine::Height));
        area.removeFromBottom(4);
    }
    for (juce::Component* c : { static_cast<juce::Component*>(&m_palette), static_cast<juce::Component*>(&m_browser), static_cast<juce::Component*>(&m_tour) })
        c->setBounds(m_body.getLocalBounds());
    // The overview, unless the settings fold it away (01.10.2026): then the pages get its height.
    const bool overview = frame::Settings::of("Totality").overview();
    m_arrange.setVisible(overview);
    m_arrange.setBounds(area.removeFromTop(overview ? 96 : 0));
    area.removeFromTop(overview ? 8 : 4);
    m_tabs.setBounds(area);
    if (m_helpView != nullptr) m_helpView->setBounds(area);
}

void TotalityEditor::timerCallback()
{
    // The first steps on the first start (not in an automated run: the screenshots, the report, TOT_MUTE).
    if (m_tourTicks >= 0 && ++m_tourTicks > 25) {
        m_tourTicks = -1;
        if (m_shotPath.isEmpty() && std::getenv("TOT_PAGE_REPORT") == nullptr && !m_proc.muteForced()
            && frame::Settings::of("Totality").value("tourDone").isEmpty())
            startTour();
    }
    if (m_ratedTicks > 0) --m_ratedTicks;
    if (m_shotHands) m_proc.headset().inject(m_proc.headset().hands());
    m_status.setText(m_ratedTicks > 0 && m_rated.isNotEmpty() ? m_rated : m_proc.status(), juce::dontSendNotification);
    m_rerolls.setText(m_proc.curationText(), juce::dontSendNotification);
    {
        const juce::String v = m_updates->enabled() ? m_updates->newer() : juce::String();
        if (v.isNotEmpty() && m_update.getButtonText() != "Version " + v + " available") {
            m_update.setButtonText("Version " + v + " available");
            m_update.setURL(juce::URL(m_updates->page()));
        }
        if (m_update.isVisible() != v.isNotEmpty()) {
            m_update.setVisible(v.isNotEmpty());
            layoutBody();
        }
    }
    // Undo and redo say what they would take back; the headset's sign shows while one is there (or always, if asked).
    {
        const juce::String u = m_proc.undoName(), r = m_proc.redoName();
        m_undo.setEnabled(u.isNotEmpty());
        m_redo.setEnabled(r.isNotEmpty());
        m_undo.setTooltip(u.isEmpty() ? juce::String("Undo (Ctrl+Z)") : "Undo: " + u + " (Ctrl+Z)");
        m_redo.setTooltip(r.isEmpty() ? juce::String("Redo (Ctrl+Y)") : "Redo: " + r + " (Ctrl+Y)");
        const bool hs = m_proc.headset().shown(frame::Settings::of("Totality").headset());
        if (m_headsetIcon.isVisible() != hs) {
            m_headsetIcon.setVisible(hs);
            layoutBody();
        }
    }
    m_play.show(m_proc.isPlaying() ? frame::IconButton::IconE::STOP : frame::IconButton::IconE::PLAY, m_proc.isPlaying() ? "Stop (Space)" : "Play (Space)");
    // The choice and its length as the parameters have them (the Set page and a host move them too); Compose names
    // what it makes, and is lit while what plays is the other.
    {
        const bool mix = m_proc.mixChosen();
        if (mix != m_lengthOfMix) showLength(mix);
        m_trackMode.setToggleState(!mix, juce::dontSendNotification);
        m_mixMode.setToggleState(mix, juce::dontSendNotification);
        const ParamStore& st = m_proc.store();
        const double v = st.get(mix ? st.id(ModuleE::SET, 0, set::MINUTES) : st.id(ModuleE::COMPOSE, 0, compose::MINUTES));
        if (!m_length.isMouseButtonDown() && std::abs(m_length.getValue() - v) > 1.0e-3) {
            const juce::ScopedValueSetter<bool> quiet(m_syncing, true);
            m_length.setValue(v, juce::dontSendNotification);
        }
        m_compose.setButtonText(mix ? "Compose mix" : "Compose track");
        const juce::Colour c = Accent.withAlpha(mix != m_proc.playingMix() && !m_proc.isComposing() ? 0.42f : 0.12f);
        if (m_compose.findColour(juce::TextButton::buttonColourId) != c) m_compose.setColour(juce::TextButton::buttonColourId, c);
    }
    const bool shown = m_proc.muted() && m_shotPath.isEmpty();
    m_mute.setToggleState(shown, juce::dontSendNotification);
    m_mute.show(shown ? frame::IconButton::IconE::SPEAKER_OFF : frame::IconButton::IconE::SPEAKER,
               shown ? (m_proc.muteForced() ? "Muted by TOT_MUTE: an automated run makes no sound" : "Muted: click to hear the output again")
                     : "Silence the output");
    m_compose.setEnabled(!m_proc.isComposing());
    // The test mode: the recording, when full, is written and the standalone quits.
    if (m_proc.recordingDone()) {
        m_proc.writeRecording();
        if (juce::JUCEApplicationBase::isStandaloneApp()) juce::JUCEApplicationBase::quit();
        return;
    }
    // The screenshot mode: wait for the first score, then draw the panel into a file and quit. TOT_SHOT_AT (a beat)
    // moves the playhead there first.
    if (m_shotPath.isNotEmpty() && !m_proc.isComposing() && m_shotTicks == 0)
        if (const char* at = std::getenv("TOT_SHOT_AT")) m_proc.seekTo(std::atof(at));
    // TOT_PAGE_REPORT=<file> (01.10.2026): how far every page reaches past the window, a line each (Frame.h, pageReport),
    // then quit -- which page still scrolls at the usual size.
    if (const char* report = std::getenv("TOT_PAGE_REPORT"); report != nullptr && !m_proc.isComposing()) {
        static int reportTicks = 0;
        if (++reportTicks == 8) {
            juce::File(report).replaceWithText(frame::pageReport(m_tabs));
            if (juce::JUCEApplicationBase::isStandaloneApp()) juce::JUCEApplicationBase::quit();
        }
    }
    // TOT_SHOT_FULL: the window grows until nothing of the page in front scrolls, so the picture shows all of it.
    if (m_shotPath.isNotEmpty() && !m_proc.isComposing() && (m_shotTicks == 6 || m_shotTicks == 12) && std::getenv("TOT_SHOT_FULL") != nullptr) {
        std::function<int(juce::Component&)> overflow = [&](juce::Component& c) {
            int most = 0;
            if (auto* v = dynamic_cast<juce::Viewport*>(&c))
                if (auto* inner = v->getViewedComponent()) most = inner->getHeight() - v->getMaximumVisibleHeight();
            for (auto* child : c.getChildren()) most = std::max(most, overflow(*child));
            return most;
        };
        if (auto* page = m_tabs.getCurrentContentComponent())
            if (const int more = overflow(*page); more > 0) setSize(getWidth(), getHeight() + more + 4);
    }
    if (m_shotPath.isNotEmpty() && !m_proc.isComposing() && m_proc.scoreVersion() > 0 && ++m_shotTicks > 20) {
        const juce::Image img = createComponentSnapshot(getLocalBounds());
        juce::File f(m_shotPath);
        f.deleteFile();
        juce::FileOutputStream out(f);
        juce::PNGImageFormat().writeImageToStream(img, out);
        out.flush();
        m_shotPath = {};
        if (juce::JUCEApplicationBase::isStandaloneApp()) juce::JUCEApplicationBase::quit();
    }
}

// --------------------------------------------------------------------------------------------------- ways around the panel

void TotalityEditor::openPalette()
{
    if (m_browser.isVisible()) m_browser.close();
    m_palette.open(paletteCommands());
}

bool TotalityEditor::revealId(int id) { return frame::revealControl(m_tabs, id, Accent); }

std::vector<frame::CommandPalette::Command> TotalityEditor::paletteCommands()
{
    using Command = frame::CommandPalette::Command;
    std::vector<Command> out;
    auto action = [&out](const juce::String& title, const juce::String& detail, std::function<void()> f) {
        out.push_back({ title, detail, "Action", false, [f](const juce::String&) { f(); } });
    };
    action("Play / stop", "Space", [this] { m_proc.setPlaying(!m_proc.isPlaying()); });
    action("Compose track", "a single track, its length in the header", [this] { m_proc.chooseMix(false); m_proc.compose(); });
    action("Compose DJ mix", "a set of tracks on two decks", [this] { m_proc.chooseMix(true); m_proc.compose(); });
    action("New seed", "another track from the same settings", [this] { m_proc.newSeed(); });
    action("Undo", "Ctrl+Z", [this] { m_proc.undo(); });
    action("Redo", "Ctrl+Y", [this] { m_proc.redo(); });
    action("Mute / unmute the output", "the speaker", [this] { m_proc.setMuted(!m_proc.muted()); });
    action("I like this track", "a thumb up", [this] { m_rated = m_proc.rate(1); m_ratedTicks = 60; });
    action("Not this one", "a thumb down", [this] { m_rated = m_proc.rate(-1); m_ratedTicks = 60; });
    action("Save the set", "Ctrl+S", [this] { if (auto* e = exportPage()) e->save(); });
    action("Open a set", "Ctrl+O", [this] { if (auto* e = exportPage()) e->load(); });
    action("Export WAV + MIDI + cues", "Ctrl+E", [this] { if (auto* e = exportPage()) e->exportWith(0); });
    action("Help", "F1: the manual by topic", [this] { showHelp(true); });
    action("Settings", "the window, the headset, the jam, Link", [this] { showSettings(); });
    action("First steps", "the tour of the panel", [this] { startTour(); });
    action("Overview above the tabs", "show it or fold it away", [] {
        auto& s = frame::Settings::of("Totality");
        s.setOverview(!s.overview());
    });
    if (standalone()) action("Full screen", "F11", [this] { toggleFullScreen(); });
    // The pages, and the small tabs of the pages of several.
    const juce::StringArray names = m_tabs.getTabNames();
    for (int t = 0; t < m_tabs.getNumTabs(); ++t) {
        out.push_back({ names[t], "page", "Page", false, [this, t](const juce::String&) { m_tabs.setCurrentTabIndex(t); } });
        if (auto* st = findSubTabs(m_tabs.getTabContentComponent(t)))
            for (int j = 0; j < st->count(); ++j)
                out.push_back({ names[t] + ": " + st->name(j), "page", "Page", false, [this, t, j](const juce::String&) {
                                   m_tabs.setCurrentTabIndex(t);
                                   if (auto* s2 = findSubTabs(m_tabs.getTabContentComponent(t))) s2->show(j);
                               } });
    }
    // Every control: its page opens and it lights up; a number after its name sets it.
    const ParamStore& s = m_proc.store();
    for (int id = 0; id < s.count(); ++id) {
        if (m_proc.parameter(id) == nullptr) continue;
        const ParamDesc& d = s.desc(id);
        const bool number = d.m_curve != CurveE::CHOICE && d.m_curve != CurveE::TOGGLE;
        out.push_back({ d.m_name, moduleLabel(s, id), "Control", number, [this, id](const juce::String& n) {
                           if (n.isNotEmpty()) m_proc.applyKnobs(juce::String(m_proc.store().desc(id).m_name), { { id, n.getFloatValue() } });
                           revealId(id);
                           if (m_info != nullptr) m_info->showControl(id);
                       } });
    }
    // Every preset of every synth (the kit's on the lane the Drums page shows), the player's own first.
    int lane = 0;
    for (int t = 0; t < m_tabs.getNumTabs(); ++t)
        if (names[t] == "Drums")
            if (auto* sp = dynamic_cast<ScrollingPage*>(m_tabs.getTabContentComponent(t))) lane = sp->page().instance();
    for (int mi = 0; mi < static_cast<int>(ModuleE::COUNT); ++mi) {
        const auto m = static_cast<ModuleE>(mi);
        if (!hasPresets(m)) continue;
        const int inst = m == ModuleE::PERC ? lane : 0;
        const juce::String where = moduleLabel(s, s.id(m, inst, 0));
        const juce::Array<juce::File> yours = userPresets(s, m);
        for (int k = 0; k < yours.size(); ++k)
            out.push_back({ yours[k].getFileNameWithoutExtension(), where + " preset, yours", "Yours", false, [this, m, inst, k](const juce::String&) {
                               const juce::Array<juce::File> files = userPresets(m_proc.store(), m);
                               if (k >= files.size()) return;
                               m_proc.applyKnobs("preset " + files[k].getFileNameWithoutExtension(), userPresetKnobs(m_proc.store(), m, inst, files[k]));
                               frame::PresetBrowser::played("Totality", synthKey(m_proc.store(), m), files[k].getFileNameWithoutExtension());
                               revealId(m_proc.store().id(m, inst, 0));
                           } });
        const std::vector<SoundPreset>& list = factoryPresets(m);
        for (size_t i = 0; i < list.size(); ++i)
            out.push_back({ list[i].m_name, where + " preset, " + list[i].m_group, "Preset", false, [this, m, inst, i](const juce::String&) {
                               m_proc.applyPreset(m, inst, static_cast<int>(i));
                               frame::PresetBrowser::played("Totality", synthKey(m_proc.store(), m), factoryPresets(m)[i].m_name);
                               revealId(m_proc.store().id(m, inst, 0));
                           } });
    }
    return out;
}

bool TotalityEditor::describeControl(int id, juce::String& name, juce::String& text)
{
    const ParamStore& s = m_proc.store();
    if (id < 0 || id >= s.count()) return false;
    const ParamDesc& d = s.desc(id);
    auto fmt = [&d](float v) {
        return juce::String(v, std::abs(v) >= 100.0f ? 0 : std::abs(v) >= 10.0f ? 1 : 2) + (d.m_unit[0] != 0 ? juce::String(" ") + d.m_unit : juce::String());
    };
    name = d.m_name;
    text = moduleLabel(s, id) + "    now " + juce::String(s.format(id));
    if (d.m_curve != CurveE::CHOICE && d.m_curve != CurveE::TOGGLE) {
        text << "    " << fmt(d.m_minValue) << " .. " << fmt(d.m_maxValue) << ", default " << fmt(s.defaultValue(id));
        // Where it plays now, when the score's automation (a section, a ramp) moves it away from where it stands.
        const float played = m_proc.playedNormalised(id);
        if (!std::isnan(played) && std::abs(played - s.toNormalised(id, s.get(id))) > 0.004f)
            text << "    plays at " << fmt(s.fromNormalised(id, played)) << " (the score moves it)";
    }
    if (const int cc = m_proc.controllerFor(id); cc >= 0) text << "    MIDI CC " << cc;
    text << "    [" << juce::String(s.key(id)) << "]";
    return true;
}

void TotalityEditor::browsePresets(ModuleE m, int instance, std::function<void(int)> chosen)
{
    if (m_palette.isVisible()) m_palette.close();
    const ParamStore& s = m_proc.store();
    frame::PresetBrowser::Bank bank;
    bank.m_key = synthKey(s, m);
    bank.m_title = moduleLabel(s, s.id(m, instance, 0)) + " presets";
    const juce::Array<juce::File> yours = userPresets(s, m);
    for (int k = 0; k < yours.size(); ++k)
        bank.m_entries.push_back({ yours[k].getFileNameWithoutExtension(), "Yours", {}, PresetBar::Yours + k + 1, true, {} });
    const std::vector<SoundPreset>& list = factoryPresets(m);
    for (size_t i = 0; i < list.size(); ++i) bank.m_entries.push_back({ list[i].m_name, list[i].m_group, {}, static_cast<int>(i) + 1, false, {} });
    bank.m_preview = [this, m, instance](const frame::PresetBrowser::Entry& e) { m_proc.previewKnobs(knobsOf(m_proc.store(), m, instance, e.m_index)); };
    bank.m_endPreview = [this] { m_proc.endPreview(); };
    bank.m_choose = [this, m, instance, chosen](const frame::PresetBrowser::Entry& e) {
        if (e.m_index > PresetBar::Yours) m_proc.applyKnobs("preset " + e.m_name, knobsOf(m_proc.store(), m, instance, e.m_index));
        else m_proc.applyPreset(m, instance, e.m_index - 1);
        if (chosen) chosen(e.m_index);
    };
    m_browser.open(std::move(bank));
}

void TotalityEditor::startTour()
{
    if (m_helpView != nullptr && m_helpView->isVisible()) showHelp(false);
    std::vector<frame::Tour::Step> steps;
    steps.push_back({ &m_style, "The style", "What the next track is composed in -- Hypnotic, Ostgut, Dub, Raw Peak -- with its key and scale beside it." });
    steps.push_back({ &m_compose, "Compose", "Composes a track (or a DJ mix, with DJ mix chosen) from the knobs, the seed and the rerolls. The die beside it: "
                                            "a new seed, another track from the same settings." });
    steps.push_back({ &m_play, "Play", "Plays and stops (Space). The speaker beside it silences the output." });
    if (m_arrange.isVisible())
        steps.push_back({ &m_arrange, "The overview", "The track's blocks (or the mix's tracks on their decks), a lane per group lit where it plays. "
                                                     "Click to jump there, the wheel zooms." });
    steps.push_back({ &m_tabs.getTabbedButtonBar(), "The pages", "Every part of the instrument: the set, the arrangement, the patterns, the sounds "
                                                                "(each synth with 1024 presets and your own), the mixer, Perform for playing live, Export." });
    steps.push_back({ &m_search, "Search", "Ctrl+K: any knob, preset or action by a few letters -- its page opens and it lights up. A number after a "
                                          "knob's name sets it (\"cutoff 800\")." });
    if (m_info != nullptr)
        steps.push_back({ m_info.get(), "The info line", "What the control under the mouse is: its value, its range, where the score plays it, its MIDI controller." });
    steps.push_back({ &m_help, "Help", "F1: the manual by topic, and the knobs of the page in front." });
    steps.push_back({ &m_settings, "Settings", "The window's size, the headset, the family jam, Ableton Link -- and these first steps again." });
    m_tour.start(std::move(steps));
}
