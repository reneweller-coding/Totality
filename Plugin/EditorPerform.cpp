/**
 * @file EditorPerform.cpp
 * @brief The live pages (EditorPerform.h).
 */
#include "EditorPerform.h"
#include "tot/Presets.h"
#include <cmath>

using namespace tot;

namespace {

/** @brief the mutes' names, in the order of perform::MuteKick .. */
const char* const kMuteNames[perform::kMutes] = { "Kick", "Sub", "Hats", "Perc", "Ping", "Bass", "Pads" };
const char* const kKeyOf[perform::kMutes] = { "C3", "C#3", "D3", "D#3", "E3", "F3", "F#3" };   ///< the key that toggles each mute

/** @brief Gain @p g in dB, -120 for silence. */
float toDb(float g) { return g > 1e-6f ? 20.0f * std::log10(g) : -120.0f; }

} // namespace

// ---------------------------------------------------------------------------------------------------

LearnButton::LearnButton(TotalityProcessor& p, int id) : juce::TextButton("learn"), proc_(p), id_(id)
{
    setTooltip("MIDI learn: click, then move a controller (right click: forget it)");
    onClick = [this] { proc_.learn(proc_.learning() == id_ ? -1 : id_); refresh(); };
}

void LearnButton::refresh()
{
    const int cc = proc_.controllerFor(id_);
    const juce::String t = proc_.learning() == id_ ? juce::String("move ...") : cc >= 0 ? "CC " + juce::String(cc) : juce::String("learn");
    if (getButtonText() != t) setButtonText(t);
    setToggleState(proc_.learning() == id_, juce::dontSendNotification);
}

void LearnButton::mouseUp(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { proc_.forget(id_); refresh(); return; }
    juce::TextButton::mouseUp(e);
}

// ---------------------------------------------------------------------------------------------------

PerformPage::PerformPage(TotalityProcessor& p) : proc_(p)
{
    ParamStore& s = proc_.store();
    for (int k = 0; k < perform::kMutes; ++k) {
        auto* b = mutes_.add(new juce::TextButton(kMuteNames[k]));
        b->setClickingTogglesState(true);
        b->setColour(juce::TextButton::buttonOnColourId, totui::colour::onset.withAlpha(0.6f));
        b->setTooltip(juce::String("Mute ") + kMuteNames[k] + ": its notes stop, its tails ring out (MIDI key " + kKeyOf[k] + ")");
        const int id = s.id(Module::Perform, 0, perform::MuteKick + k);
        muteAttach_.push_back(std::make_unique<juce::ButtonParameterAttachment>(*proc_.parameter(id), *b));
        addAndMakeVisible(b);
        addAndMakeVisible(learn_.add(new LearnButton(proc_, id)));
    }
    auto bigSlider = [&](juce::Slider& sl, int id, juce::Colour c) {
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);
        sl.setColour(juce::Slider::trackColourId, c);
        sl.setColour(juce::Slider::thumbColourId, c);
        sl.setDoubleClickReturnValue(true, 0.0);
        sliderAttach_.push_back(std::make_unique<juce::SliderParameterAttachment>(*proc_.parameter(id), sl));
        addAndMakeVisible(sl);
        addAndMakeVisible(learn_.add(new LearnButton(proc_, id)));
    };
    bigSlider(filter_, s.id(Module::Perform, 0, perform::Filter), totui::familyColour(totui::Family::Filter));
    filter_.setTooltip("The master filter: left a low pass, right a high pass, the middle open (controller 74; double click: open)");
    bigSlider(throw_, s.id(Module::Perform, 0, perform::Throw), totui::familyColour(totui::Family::Motion));
    throw_.setTooltip("The echo throw: the whole mix into the mixer's tape echo (the expression pedal)");
    {   // The keyboard (01.10.2026): its choosers from the parameters' own names.
        auto choice = [&](std::unique_ptr<frame::Choice>& box, std::unique_ptr<juce::ComboBoxParameterAttachment>& attach, int id,
                          const char* tip) {
            const ParamDesc& d = s.desc(id);
            box = std::make_unique<frame::Choice>(nullptr, id);
            for (int c = 0; c <= static_cast<int>(d.maxValue); ++c) box->addItem(d.choices[c], c + 1);
            box->setTooltip(tip);
            attach = std::make_unique<juce::ComboBoxParameterAttachment>(*proc_.parameter(id), *box);
            addAndMakeVisible(*box);
        };
        choice(keyPart_, keyPartAttach_, s.id(Module::Perform, 0, perform::KeyboardPart),
               "What the keys of a MIDI keyboard play: the kit (C1 the kick, C#1 to C2 the lanes), the bass, the 303, the ping, "
               "the chord, the drone, or by channel (1 kit, 2 bass, 3 303, 4 ping, 5 chord, 6 drone, 10 kit). Off: the keys "
               "from middle C toggle the mutes.");
        choice(keyMode_, keyModeAttach_, s.id(Module::Perform, 0, perform::KeyboardMode),
               "Replace: the voice the keyboard plays leaves the composer's notes out; Layer: it plays over them.");
        const int cid = s.id(Module::Perform, 0, perform::Composer);
        composer_ = std::make_unique<frame::Switch>(nullptr, cid);
        composer_->setButtonText("Composer");
        composer_->setTooltip("On: the composer's notes play. Off: only what the keyboard plays -- the mix, the filters and the "
                              "effects go on as composed.");
        composerAttach_ = std::make_unique<juce::ButtonParameterAttachment>(*proc_.parameter(cid), *composer_);
        addAndMakeVisible(*composer_);
    }
    for (int d = 0; d < kDecks; ++d) {
        Strip& st = strips_[d];
        static const char* const kBand[3] = { "Low", "Mid", "High" };
        for (int b = 0; b < 3; ++b) {
            juce::TextButton& k = st.kill[b];
            k.setButtonText(juce::String("Kill ") + kBand[b]);
            k.setColour(juce::TextButton::buttonOnColourId, totui::deckColour(d).withAlpha(0.5f));
            const int id = s.id(Module::Deck, d, deck::Low + b);
            k.onClick = [this, id] { proc_.setFromUi(id, proc_.store().get(id) <= -59.9f ? 0.0f : -60.0f); };
            addAndMakeVisible(k);
            addAndMakeVisible(learn_.add(new LearnButton(proc_, id)));
        }
        st.fader.setSliderStyle(juce::Slider::LinearVertical);
        st.fader.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
        st.fader.setColour(juce::Slider::trackColourId, totui::deckColour(d));
        st.fader.setColour(juce::Slider::thumbColourId, totui::deckColour(d));
        st.fader.setDoubleClickReturnValue(true, 0.0);
        const int fid = s.id(Module::Deck, d, deck::Fader);
        sliderAttach_.push_back(std::make_unique<juce::SliderParameterAttachment>(*proc_.parameter(fid), st.fader));
        addAndMakeVisible(st.fader);
        addAndMakeVisible(learn_.add(new LearnButton(proc_, fid)));
    }
    startTimerHz(8);
}

void PerformPage::timerCallback()
{
    // The headset's group: shown while one sends (or always, as the settings say), its hands live.
    const bool hs = proc_.headset().shown(frame::Settings::of("Totality").headset());
    if (hs != headset_) { headset_ = hs; resized(); }
    if (headset_) repaint(headsetArea_);
    const ParamStore& s = proc_.store();
    for (int d = 0; d < kDecks; ++d)
        for (int b = 0; b < 3; ++b)
            strips_[d].kill[b].setToggleState(s.get(s.id(Module::Deck, d, deck::Low + b)) <= -59.9f, juce::dontSendNotification);
    for (auto* l : learn_) l->refresh();
}

void PerformPage::resized()
{
    auto r = getLocalBounds().reduced(16);
    // The mutes: a row of large pads, each with its learn button below.
    auto mutes = r.removeFromTop(110);
    const int w = mutes.getWidth() / perform::kMutes;
    int li = 0;
    for (int k = 0; k < perform::kMutes; ++k) {
        auto cell = mutes.removeFromLeft(w).reduced(5, 0);
        mutes_[k]->setBounds(cell.removeFromTop(78));
        learn_[li++]->setBounds(cell.removeFromTop(26).reduced(0, 3));
    }
    r.removeFromTop(28);
    auto line = [&](juce::Slider& sl) {
        auto row = r.removeFromTop(40);
        row.removeFromLeft(130);   // the name (paint)
        learn_[li++]->setBounds(row.removeFromRight(80).reduced(4, 8));
        sl.setBounds(row);
        r.removeFromTop(8);
    };
    line(filter_);
    line(throw_);
    r.removeFromTop(24);
    // The decks: three strips, each three kills and a fader.
    const int sw = std::min(260, r.getWidth() / kDecks);
    for (int d = 0; d < kDecks; ++d) {
        auto strip = r.removeFromLeft(sw).reduced(8, 0);
        strip.removeFromTop(22);   // the deck's name (paint)
        auto kills = strip.removeFromLeft(strip.getWidth() / 2);
        for (int b = 2; b >= 0; --b) {
            auto k = kills.removeFromTop(40);
            strips_[d].kill[b].setBounds(k.removeFromLeft(k.getWidth() * 3 / 5).reduced(2));
            learn_[li + b]->setBounds(k.reduced(2, 8));
        }
        li += 3;
        learn_[li++]->setBounds(strip.removeFromBottom(26).reduced(10, 3));
        strips_[d].fader.setBounds(strip.reduced(10, 0));
    }
    // Right of the decks: the keyboard, the headset under it.
    auto right = r.withTrimmedLeft(20);
    keyArea_ = right.removeFromTop(150);
    {
        auto k = keyArea_.reduced(12, 8);
        k.removeFromTop(24);   // the title (paint)
        auto keyLine = [&](juce::Component& c) {
            auto row = k.removeFromTop(36);
            row.removeFromLeft(110);   // the name (paint)
            c.setBounds(row.removeFromLeft(std::min(220, row.getWidth())).reduced(0, 4));
        };
        keyLine(*keyPart_);
        keyLine(*keyMode_);
        keyLine(*composer_);
    }
    right.removeFromTop(12);
    headsetArea_ = headset_ ? right : juce::Rectangle<int>();
}

void PerformPage::paint(juce::Graphics& g)
{

    g.setColour(totui::colour::dim);
    g.setFont(juce::FontOptions(13.0f));
    auto r = getLocalBounds().reduced(16);
    g.drawText("MUTE -- the keys C3 to F#3 toggle them (while the keyboard plays nothing); a muted group's notes stop, its tails ring out",
               r.getX(), r.getY() + 112, r.getWidth(), 20,
               juce::Justification::left);
    g.drawText("Master Filter", r.getX(), filter_.getY(), 120, filter_.getHeight(), juce::Justification::centredLeft);
    g.drawText("Echo Throw", r.getX(), throw_.getY(), 120, throw_.getHeight(), juce::Justification::centredLeft);
    for (int d = 0; d < kDecks; ++d) {
        const auto b = strips_[d].kill[2].getBounds();
        g.setColour(totui::deckColour(d));
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(juce::String("DECK ") + juce::String::charToString(static_cast<juce::juce_wchar>('A' + d)) + (d == 2 ? " (loops)" : ""),
                   b.getX(), b.getY() - 22, 200, 18, juce::Justification::left);
    }
    if (!keyArea_.isEmpty()) {   // the keyboard's box, as the frame draws a group
        const frame::Skin& sk = totui::skin();
        const auto box = keyArea_.toFloat();
        g.setColour(sk.group);
        g.fillRoundedRectangle(box, sk.radius);
        g.setColour(sk.edge);
        g.drawRoundedRectangle(box.reduced(0.5f), sk.radius, 1.0f);
        g.setColour(sk.family(frame::Family::Source));
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.drawText("KEYBOARD", keyArea_.getX() + 12, keyArea_.getY() + 8, 200, 16, juce::Justification::centredLeft);
        g.setColour(totui::colour::dim);
        g.setFont(juce::FontOptions(13.0f));
        for (auto [c, name] : { std::pair<juce::Component*, const char*>{ keyPart_.get(), "Plays" }, { keyMode_.get(), "Mode" },
                                { composer_.get(), "Composer" } })
            g.drawText(name, keyArea_.getX() + 12, c->getY(), 96, c->getHeight(), juce::Justification::centredLeft);
    }
    if (headset_ && !headsetArea_.isEmpty())   // the headset (the frame): what the hands do, and how they stand now
        frame::drawHeadsetBox(g, headsetArea_, totui::skin(), proc_.headset(), "kick out / in", {});
}

// ---------------------------------------------------------------------------------------------------

DecksPage::DecksPage(TotalityProcessor& p)
    : knobs_(std::make_unique<ParamPage>(p, std::vector<std::pair<Module, int>>{ { Module::Deck, 0 }, { Module::Deck, 1 }, { Module::Deck, 2 } }))
{
    addAndMakeVisible(knobs_);
}

void DecksPage::meter(const float* pk, const float* rms, float out, float lufs)
{
    for (int d = 0; d < kDecks; ++d) {
        peak_[d] = std::max(pk[d], peak_[d] * 0.85f);
        rms_[d] = rms[d];
        hold_[d] = std::max(toDb(pk[d]), hold_[d] - 0.6f);
    }
    out_ = std::max(out, out_ * 0.85f);
    hold_[kDecks] = std::max(toDb(out), hold_[kDecks] - 0.6f);
    lufs_ = lufs;
    repaint(getLocalBounds().withWidth(220));
}

void DecksPage::resized()
{
    knobs_.setBounds(getLocalBounds().withTrimmedLeft(220));
}

void DecksPage::paint(juce::Graphics& g)
{
    using namespace totui::colour;
    auto r = getLocalBounds().withWidth(220).reduced(12);
    g.setColour(ink);
    g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    g.drawText(lufs_ > -69.0f ? juce::String(lufs_, 1) + " LUFS" : juce::String("-- LUFS"), r.removeFromTop(28), juce::Justification::centredLeft);
    g.setColour(dim);
    g.setFont(juce::FontOptions(11.0f));
    g.drawFittedText("momentary (400 ms, K-weighted); the styles aim at -9.4 to -11.5 LUFS in their loudest part", r.removeFromTop(30),
                     juce::Justification::topLeft, 3);
    r.removeFromTop(8);
    // Meters: the three decks after their channels, and the output; -48 .. +6 dB.
    const auto yOf = [&](float db, juce::Rectangle<int> m) {
        const float t = juce::jlimit(0.0f, 1.0f, (db + 48.0f) / 54.0f);
        return static_cast<float>(m.getBottom()) - t * static_cast<float>(m.getHeight());
    };
    const int mw = r.getWidth() / (kDecks + 1);
    for (int i = 0; i <= kDecks; ++i) {
        auto m = r.withX(r.getX() + i * mw).withWidth(mw).reduced(8, 0).withTrimmedBottom(22);
        g.setColour(group);
        g.fillRect(m);
        const float pdb = toDb(i < kDecks ? peak_[i] : out_), rdb = i < kDecks ? toDb(rms_[i]) : pdb;
        const juce::Colour c = i < kDecks ? totui::deckColour(i) : accent;
        g.setColour(c.withAlpha(0.35f));
        g.fillRect(juce::Rectangle<float>(static_cast<float>(m.getX()), yOf(pdb, m), static_cast<float>(m.getWidth()), static_cast<float>(m.getBottom()) - yOf(pdb, m)));
        if (i < kDecks) {
            g.setColour(c.withAlpha(0.85f));
            g.fillRect(juce::Rectangle<float>(static_cast<float>(m.getX()) + 3.0f, yOf(rdb, m), static_cast<float>(m.getWidth()) - 6.0f, static_cast<float>(m.getBottom()) - yOf(rdb, m)));
        }
        g.setColour(hold_[i] > -0.1f ? red : ink);
        g.fillRect(static_cast<float>(m.getX()), yOf(hold_[i], m) - 1.0f, static_cast<float>(m.getWidth()), 2.0f);
        g.setColour(dim);
        g.drawText(i < kDecks ? juce::String::charToString(static_cast<juce::juce_wchar>('A' + i)) : juce::String("Out"), m.getX() - 6, m.getBottom() + 2,
                   m.getWidth() + 12, 18, juce::Justification::centred);
    }
    g.setColour(faint);
    for (int db : { 0, -6, -12, -24, -36 }) {
        const float y = yOf(static_cast<float>(db), r.withTrimmedBottom(22));
        g.fillRect(static_cast<float>(r.getX()), y, static_cast<float>(r.getWidth()), 1.0f);
    }
}

// ---------------------------------------------------------------------------------------------------

namespace {
/** @brief A strip of the console: its name, the meter it reads, its synth, its fader, pan and sends, its mute. */
struct StripSpec {
    const char* name;   ///< the strip's name
    int meter;   ///< the strip it reads (MeterSink::Strip)
    Module synth;   ///< the synth whose preset it names (Module::Count: none)
    const char* fader;   ///< its fader's key
    std::vector<std::pair<const char*, const char*>> knobs;   ///< key, label; "Pan" is not a send
    int mute;                                                   ///< perform::Mute* offset from MuteKick, -1 none
};
} // namespace

MixerPage::MixerPage(TotalityProcessor& p)
    : proc_(p), live_([this](int id) { return proc_.playedNormalised(id); }), tabs_(totui::skin())
{
    actions_.learn = [this](int id) { proc_.learn(id); };
    actions_.controllerFor = [this](int id) { return proc_.controllerFor(id); };
    actions_.forget = [this](int id) { proc_.forget(id); };
    actions_.reset = [this](int id) { proc_.resetToDefault(id); };
    actions_.describe = [this](int id) { return juce::String(proc_.store().desc(id).name) + "  (" + proc_.store().key(id) + ")"; };
    using F = totui::Family;
    tabs_.add("Console", [this] {
        auto console = std::make_unique<frame::Console>(totui::skin());
        console_ = console.get();
        ParamStore& s = proc_.store();
        const std::vector<StripSpec> specs = {
            { "Kick", MeterSink::Kick, Module::Kick, "kick.level", {}, 0 },
            { "Rumble", MeterSink::Rumble, Module::Rumble, "rumble.level", {}, 0 },
            { "Sub", MeterSink::Sub, Module::Sub, "sub.level", {}, 1 },
            { "Hats", MeterSink::Hats, Module::Count, "mix.hats_level", { { "space.hats_send", "Room" }, { "dub.hats_send", "Echo" } }, 2 },
            { "Perc", MeterSink::Perc, Module::Count, "mix.perc_level", { { "space.perc_send", "Room" }, { "dub.perc_send", "Echo" } }, 3 },
            { "Ping", MeterSink::Ping, Module::Ping, "ping.level", { { "ping.pan", "Pan" }, { "space.ping_send", "Room" }, { "dub.ping_send", "Echo" }, { "cloud.ping_send", "Cloud" } }, 4 },
            { "Bass", MeterSink::Bass, Module::Bass, "bass.level", { { "bass.pan", "Pan" }, { "bass.room_send", "Room" }, { "bass.dub_send", "Echo" } }, 5 },
            { "303", MeterSink::Acid, Module::Acid, "acid.level", { { "acid.pan", "Pan" }, { "acid.room_send", "Room" }, { "acid.dub_send", "Echo" } }, 5 },
            { "Chord", MeterSink::Chord, Module::Chord, "chord.level", { { "chord.dub_send", "Echo" }, { "chord.plate_send", "Plate" }, { "cloud.chord_send", "Cloud" } }, 6 },
            { "Drone", MeterSink::Drone, Module::Drone, "drone.level", { { "drone.room_send", "Room" }, { "drone.plate_send", "Plate" } }, 6 },
            { "Texture", MeterSink::Texture, Module::Texture, "texture.level", {}, -1 },
            { "Room", MeterSink::Room, Module::Count, "space.level", {}, -1 },
            { "Dub", MeterSink::Dub, Module::Count, "dub.echo_return", { { "dub.plate_return", "Plate" } }, -1 },
            { "Cloud", MeterSink::Cloud, Module::Count, "cloud.level", { { "cloud.plate_send", "Plate" } }, -1 },
        };
        auto control = [&](const char* key, const juce::String& label, bool send) {
            frame::ChannelStrip::Control c;
            c.id = s.find(key);
            c.param = c.id >= 0 ? proc_.parameter(c.id) : nullptr;
            c.label = label;
            c.send = send;
            return c;
        };
        sounds_.clear();
        for (const StripSpec& sp : specs) {
            std::vector<frame::ChannelStrip::Control> knobs;
            for (const auto& [key, label] : sp.knobs) knobs.push_back(control(key, label, juce::String(label) != "Pan"));
            const F family = sp.meter >= MeterSink::Room ? F::Space : sp.meter <= MeterSink::Perc ? F::Source : sp.meter <= MeterSink::Acid ? F::Filter : F::Envelope;
            auto& strip = console->add(std::make_unique<frame::ChannelStrip>(totui::skin(), sp.name, totui::familyColour(family),
                                                                              control(sp.fader, juce::String(sp.name) + " level", false), knobs, &actions_, &live_));
            sounds_.emplace_back(sp.meter, sp.synth);
            if (sp.mute >= 0) {
                auto* m = mutes_.add(new juce::TextButton("M"));
                m->setClickingTogglesState(true);
                m->setColour(juce::TextButton::buttonOnColourId, totui::colour::onset.withAlpha(0.6f));
                m->setTooltip("Mute the group (as on the Perform page)");
                m->setWantsKeyboardFocus(false);
                muteLinks_.push_back(std::make_unique<juce::ButtonParameterAttachment>(*proc_.parameter(s.id(Module::Perform, 0, perform::MuteKick + sp.mute)), *m));
                strip.setHeadButton(m);
            }
        }
        // The output: the master's level, and the meter of what leaves.
        console->add(std::make_unique<frame::ChannelStrip>(totui::skin(), "Out", totui::colour::accent, control("master.level", "Master level", false),
                                                           std::vector<frame::ChannelStrip::Control>{}, &actions_, &live_));
        return std::unique_ptr<juce::Component>(std::move(console));
    });
    tabs_.add("Buses and Master", [this] {
        return std::unique_ptr<juce::Component>(std::make_unique<ScrollingPage>(
            std::make_unique<ParamPage>(proc_, std::vector<std::pair<Module, int>>{ { Module::Mix, 0 }, { Module::Master, 0 }, { Module::Motion, 0 } })));
    });
    tabs_.add("Decks", [this] {
        auto d = std::make_unique<DecksPage>(proc_);
        decks_ = d.get();
        return std::unique_ptr<juce::Component>(std::move(d));
    });
    addAndMakeVisible(tabs_);
    startTimerHz(30);
}

void MixerPage::timerCallback()
{
    if (!isShowing()) return;
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const double dt = lastPoll_ > 0.0 ? juce::jlimit(0.0, 0.5, now - lastPoll_) : 1.0 / 30.0;
    lastPoll_ = now;
    float pk[kDecks], rms[kDecks], out = 0.0f, lufs = -70.0f;
    proc_.takeMeters(pk, rms, out, lufs);
    float spk[MeterSink::kStrips], srms[MeterSink::kStrips];
    proc_.takeStripMeters(spk, srms);
    if (decks_ != nullptr && decks_->isShowing()) decks_->meter(pk, rms, out, lufs);
    lufs_ = lufs;
    if (console_ != nullptr && console_->isShowing()) {
        for (int i = 0; i < console_->size() && i < static_cast<int>(sounds_.size()); ++i)
            console_->strip(i).meter(spk[sounds_[static_cast<size_t>(i)].first], srms[sounds_[static_cast<size_t>(i)].first], dt);
        if (console_->size() > static_cast<int>(sounds_.size())) console_->strip(console_->size() - 1).meter(out, out * 0.7071f, dt);
        // Now and then: the sound each synth's strip plays.
        if (++tick_ % 15 == 0)
            for (size_t i = 0; i < sounds_.size(); ++i) {
                const Module m = sounds_[i].second;
                juce::String name;
                if (m != Module::Count)
                    if (const int index = proc_.composedPreset(m, 0); index >= 0) name = factoryPresets(m)[static_cast<size_t>(index)].name;
                console_->strip(static_cast<int>(i)).setSound(name);
            }
        repaint(getLocalBounds().removeFromTop(36));
    }
}

void MixerPage::resized() { tabs_.setBounds(getLocalBounds()); }

void MixerPage::paint(juce::Graphics& g)
{
    // The loudness at the console's top right.
    if (tabs_.current() != 0) return;
    g.setColour(totui::colour::ink);
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText(lufs_ > -69.0f ? juce::String(lufs_, 1) + " LUFS" : juce::String("-- LUFS"), getLocalBounds().removeFromTop(36).reduced(14, 0),
               juce::Justification::centredRight);
}
