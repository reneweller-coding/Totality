/**
 * @file EditorStyle.cpp
 * @brief The Style tab (EditorStyle.h).
 */
#include "EditorStyle.h"
#include "tot/compose/Style.h"

using namespace tot;

namespace {

/** The references' medians per style (docs/eval/kalibrierung-phase4.md, Tools/ref_stats.json; 30 titles, 27.09.2026). */
struct RefRow { const char* what; const char* values[4]; };
const RefRow kRefs[] = {
    { "Centroid, Hz",            { "252", "331", "182", "170" } },
    { "Width S/M, dB",           { "-4.5", "-6.2", "-7.9", "-8.1" } },
    { "LUFS",                    { "-11.5", "-10.8", "-12.2", "-9.9" } },
    { "Loudest 20 s, LUFS",      { "-10.0", "-9.5", "-11.5", "-9.4" } },
    { "LRA, LU",                 { "4.7", "3.0", "3.4", "1.2" } },
    { "Bar similarity (audio)",  { "0.87", "0.955", "0.931", "0.924" } },
};

juce::String pct(float v) { return juce::String(juce::roundToInt(100.0f * v)) + " %"; }

} // namespace

StylePage::StylePage(TotalityProcessor& p)
    : proc_(p), custom_(std::make_unique<ParamPage>(p, std::vector<std::pair<Module, int>>{ { Module::Custom, 0 } }))
{
    addAndMakeVisible(custom_);
    take_.setTooltip("Set the custom style's knobs to the numbers of the profile shown (then change what you like, and switch Use on)");
    take_.onClick = [this] {
        ParamStore& s = proc_.store();
        // The profile without the custom style over it.
        ParamStore base;
        base.copyValuesFrom(s);
        base.set(base.id(Module::Custom, 0, custom::Use), 0.0f);
        const StyleProfile pr = profileOf(base);
        const auto put = [&](int k, float v) { proc_.setFromUi(s.id(Module::Custom, 0, k), v); };
        put(custom::BpmLow, pr.bpmLow);
        put(custom::BpmHigh, pr.bpmHigh);
        put(custom::ArcWeight, pr.arcWeight);
        put(custom::PeakWeight, pr.peakWeight);
        put(custom::EndlessWeight, pr.endlessWeight);
        put(custom::SubChance, pr.subChance);
        put(custom::BlocksLow, static_cast<float>(pr.blocksLow));
        put(custom::BlocksHigh, static_cast<float>(pr.blocksHigh));
        put(custom::Mutation, pr.mutation);
        put(custom::Reroll, pr.reroll);
        put(custom::Polymeter, pr.polymeterChance);
        put(custom::Fill, pr.fillChance);
        put(custom::Edge, pr.edgeChance);
        put(custom::MaxReduction, static_cast<float>(pr.maxReduction));
        put(custom::SwingLow, pr.swingLow);
        put(custom::SwingHigh, pr.swingHigh);
        put(custom::EventRate, pr.eventRate);
        put(custom::ThrowShare, pr.throwShare);
        put(custom::DensityCap, static_cast<float>(pr.densityCap));
        put(custom::Similarity, pr.simTarget);
        put(custom::PeakLufs, pr.peakLufs);
    };
    addAndMakeVisible(take_);
    startTimerHz(2);
}

void StylePage::timerCallback()
{
    const StyleProfile pr = profileOf(proc_.store());
    juce::String t;
    t << pr.name << pr.bpmLow << pr.bpmHigh << pr.reroll << pr.simTarget << pr.peakLufs << pr.edgeChance << pr.maxReduction;
    if (t != shown_) { shown_ = t; repaint(); }
}

void StylePage::resized()
{
    auto r = getLocalBounds();
    auto left = r.removeFromLeft(430);
    take_.setBounds(left.removeFromBottom(40).reduced(12, 6));
    custom_.setBounds(r);
}

void StylePage::paint(juce::Graphics& g)
{
    using namespace totui::colour;
    g.fillAll(panel);
    const StyleProfile pr = profileOf(proc_.store());
    const bool own = proc_.store().getBool(proc_.store().id(Module::Custom, 0, custom::Use));
    auto r = getLocalBounds().withWidth(430).reduced(14, 10);
    g.setColour(amber);
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText(own ? juce::String("Your style (over ") + pr.name + ")" : juce::String("The profile: ") + pr.name, r.removeFromTop(22), juce::Justification::left);
    g.setFont(juce::FontOptions(12.5f));
    const auto row = [&](const juce::String& k, const juce::String& v) {
        auto line = r.removeFromTop(17);
        g.setColour(dim);
        g.drawText(k, line.removeFromLeft(210), juce::Justification::left);
        g.setColour(ink);
        g.drawText(v, line, juce::Justification::left);
    };
    row("Tempo", juce::String(pr.bpmLow, 1) + " .. " + juce::String(pr.bpmHigh, 1) + " BPM");
    row("Form: Arc / Peak / Endless", juce::String(pr.arcWeight, 2) + " / " + juce::String(pr.peakWeight, 2) + " / " + juce::String(pr.endlessWeight, 2));
    row("Length", juce::String(pr.blocksLow * 32) + " .. " + juce::String(pr.blocksHigh * 32) + " bars");
    row("Sub owns the low end", pct(pr.subChance));
    row("Mutation / reroll", juce::String(pr.mutation, 2) + " / " + juce::String(pr.reroll, 2));
    row("Polymeter / fills", pct(pr.polymeterChance) + " / " + pct(pr.fillChance));
    row("Swing", juce::String(pr.swingLow, 1) + " .. " + juce::String(pr.swingHigh, 1) + " %");
    row("Filtered edges", pct(pr.edgeChance));
    row("Longest kick-out", juce::String(pr.maxReduction) + " bars");
    row("Events / throws among them", pct(pr.eventRate) + " / " + pct(pr.throwShare));
    row("Layers at once, at most", juce::String(pr.densityCap));
    row("Corridor: bar similarity", juce::String(pr.simTarget, 3));
    row("Loudest part, target", juce::String(pr.peakLufs, 1) + " LUFS");
    juce::String pool;
    for (const LayerChance& l : pr.pool) if (l.chance >= 0.5f) pool << (pool.isEmpty() ? "" : ", ") << kLayerNames[static_cast<int>(l.layer)];
    row("Likely layers", pool);
    r.removeFromTop(12);
    g.setColour(amber);
    g.setFont(juce::FontOptions(13.5f, juce::Font::bold));
    g.drawText("The references (medians, 30 titles)", r.removeFromTop(20), juce::Justification::left);
    g.setFont(juce::FontOptions(12.0f));
    {
        auto head = r.removeFromTop(17);
        head.removeFromLeft(150);
        const int cw = head.getWidth() / 4;
        for (int s = 0; s < 4; ++s) {
            g.setColour(s == static_cast<int>(proc_.store().getInt(proc_.store().id(Module::Compose, 0, compose::Style))) ? amber : dim);
            g.drawText(kStyleNames[s], head.removeFromLeft(cw), juce::Justification::centred);
        }
    }
    for (const RefRow& rr : kRefs) {
        auto line = r.removeFromTop(16);
        g.setColour(dim);
        g.drawText(rr.what, line.removeFromLeft(150), juce::Justification::left);
        const int cw = line.getWidth() / 4;
        g.setColour(ink);
        for (const char* v : rr.values) g.drawText(v, line.removeFromLeft(cw), juce::Justification::centred);
    }
}
