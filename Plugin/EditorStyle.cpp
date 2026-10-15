/**
 * @file EditorStyle.cpp
 * @brief The Style tab (EditorStyle.h).
 */
#include "EditorStyle.h"
#include "tot/compose/Style.h"

using namespace tot;

namespace {

/** The references' medians per style (docs/eval/kalibrierung-phase4.md, Tools/ref_stats.json; 30 titles, 27.09.2026). */
struct RefRow {
    const char* m_what;        ///< the measure
    const char* m_values[4];   ///< its median per style
};
/** @brief the references' medians, a row per measure */
const RefRow Refs[] = {
    { "Centroid, Hz",            { "252", "331", "182", "170" } },
    { "Width S/M, dB",           { "-4.5", "-6.2", "-7.9", "-8.1" } },
    { "LUFS",                    { "-11.5", "-10.8", "-12.2", "-9.9" } },
    { "Loudest 20 s, LUFS",      { "-10.0", "-9.5", "-11.5", "-9.4" } },
    { "LRA, LU",                 { "4.7", "3.0", "3.4", "1.2" } },
    { "Bar similarity (audio)",  { "0.87", "0.955", "0.931", "0.924" } },
};

/** @brief @p v (0..1) as a percentage. */
juce::String pct(float v) { return juce::String(juce::roundToInt(100.0f * v)) + " %"; }

} // namespace

StylePage::StylePage(TotalityProcessor& p)
    : m_proc(p), m_custom(std::make_unique<ParamPage>(p, std::vector<std::pair<ModuleE, int>>{ { ModuleE::CUSTOM, 0 } }))
{
    addAndMakeVisible(m_custom);
    m_take.setTooltip("Set the custom style's knobs to the numbers of the profile shown (then change what you like, and switch Use on)");
    m_take.onClick = [this] {
        ParamStore& s = m_proc.store();
        // The profile without the custom style over it.
        ParamStore base;
        base.copyValuesFrom(s);
        base.set(base.id(ModuleE::CUSTOM, 0, custom::USE), 0.0f);
        const StyleProfile pr = profileOf(base);
        const auto put = [&](int k, float v) { m_proc.setFromUi(s.id(ModuleE::CUSTOM, 0, k), v); };
        put(custom::BPM_LOW, pr.m_bpmLow);
        put(custom::BPM_HIGH, pr.m_bpmHigh);
        put(custom::ARC_WEIGHT, pr.m_arcWeight);
        put(custom::PEAK_WEIGHT, pr.m_peakWeight);
        put(custom::ENDLESS_WEIGHT, pr.m_endlessWeight);
        put(custom::SUB_CHANCE, pr.m_subChance);
        put(custom::BLOCKS_LOW, static_cast<float>(pr.m_blocksLow));
        put(custom::BLOCKS_HIGH, static_cast<float>(pr.m_blocksHigh));
        put(custom::MUTATION, pr.m_mutation);
        put(custom::REROLL, pr.m_reroll);
        put(custom::POLYMETER, pr.m_polymeterChance);
        put(custom::FILL, pr.m_fillChance);
        put(custom::EDGE, pr.m_edgeChance);
        put(custom::MAX_REDUCTION, static_cast<float>(pr.m_maxReduction));
        put(custom::SWING_LOW, pr.m_swingLow);
        put(custom::SWING_HIGH, pr.m_swingHigh);
        put(custom::EVENT_RATE, pr.m_eventRate);
        put(custom::THROW_SHARE, pr.m_throwShare);
        put(custom::DENSITY_CAP, static_cast<float>(pr.m_densityCap));
        put(custom::SIMILARITY, pr.m_simTarget);
        put(custom::PEAK_LUFS, pr.m_peakLufs);
    };
    addAndMakeVisible(m_take);
    startTimerHz(2);
}

void StylePage::timerCallback()
{
    const StyleProfile pr = profileOf(m_proc.store());
    juce::String t;
    t << pr.m_name << pr.m_bpmLow << pr.m_bpmHigh << pr.m_reroll << pr.m_simTarget << pr.m_peakLufs << pr.m_edgeChance << pr.m_maxReduction;
    if (t != m_shown) { m_shown = t; repaint(); }
}

void StylePage::resized()
{
    auto r = getLocalBounds();
    auto left = r.removeFromLeft(430);
    m_take.setBounds(left.removeFromBottom(40).reduced(12, 6));
    m_custom.setBounds(r);
}

void StylePage::paint(juce::Graphics& g)
{
    using namespace totui::colour;

    const StyleProfile pr = profileOf(m_proc.store());
    const bool own = m_proc.store().getBool(m_proc.store().id(ModuleE::CUSTOM, 0, custom::USE));
    auto r = getLocalBounds().withWidth(430).reduced(14, 10);
    g.setColour(Accent);
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText(own ? juce::String("Your style (over ") + pr.m_name + ")" : juce::String("The profile: ") + pr.m_name, r.removeFromTop(22), juce::Justification::left);
    g.setFont(juce::FontOptions(12.5f));
    const auto row = [&](const juce::String& k, const juce::String& v) {
        auto line = r.removeFromTop(17);
        g.setColour(Dim);
        g.drawText(k, line.removeFromLeft(210), juce::Justification::left);
        g.setColour(Ink);
        g.drawText(v, line, juce::Justification::left);
    };
    row("Tempo", juce::String(pr.m_bpmLow, 1) + " .. " + juce::String(pr.m_bpmHigh, 1) + " BPM");
    row("Form: Arc / Peak / Endless", juce::String(pr.m_arcWeight, 2) + " / " + juce::String(pr.m_peakWeight, 2) + " / " + juce::String(pr.m_endlessWeight, 2));
    row("Length", juce::String(pr.m_blocksLow * 32) + " .. " + juce::String(pr.m_blocksHigh * 32) + " bars");
    row("Sub owns the low end", pct(pr.m_subChance));
    row("Mutation / reroll", juce::String(pr.m_mutation, 2) + " / " + juce::String(pr.m_reroll, 2));
    row("Polymeter / fills", pct(pr.m_polymeterChance) + " / " + pct(pr.m_fillChance));
    row("Swing", juce::String(pr.m_swingLow, 1) + " .. " + juce::String(pr.m_swingHigh, 1) + " %");
    row("Filtered edges", pct(pr.m_edgeChance));
    row("Longest kick-out", juce::String(pr.m_maxReduction) + " bars");
    row("Events / throws among them", pct(pr.m_eventRate) + " / " + pct(pr.m_throwShare));
    row("Layers at once, at most", juce::String(pr.m_densityCap));
    row("Corridor: bar similarity", juce::String(pr.m_simTarget, 3));
    row("Loudest part, target", juce::String(pr.m_peakLufs, 1) + " LUFS");
    juce::String pool;
    for (const LayerChance& l : pr.m_pool) if (l.m_chance >= 0.5f) pool << (pool.isEmpty() ? "" : ", ") << LayerNames[static_cast<int>(l.m_layer)];
    row("Likely layers", pool);
    r.removeFromTop(12);
    g.setColour(Accent);
    g.setFont(juce::FontOptions(13.5f, juce::Font::bold));
    g.drawText("The references (medians, 30 titles)", r.removeFromTop(20), juce::Justification::left);
    g.setFont(juce::FontOptions(12.0f));
    {
        auto head = r.removeFromTop(17);
        head.removeFromLeft(150);
        const int cw = head.getWidth() / 4;
        for (int s = 0; s < 4; ++s) {
            g.setColour(s == static_cast<int>(m_proc.store().getInt(m_proc.store().id(ModuleE::COMPOSE, 0, compose::STYLE))) ? Accent : Dim);
            g.drawText(StyleNames[s], head.removeFromLeft(cw), juce::Justification::centred);
        }
    }
    for (const RefRow& rr : Refs) {
        auto line = r.removeFromTop(16);
        g.setColour(Dim);
        g.drawText(rr.m_what, line.removeFromLeft(150), juce::Justification::left);
        const int cw = line.getWidth() / 4;
        g.setColour(Ink);
        for (const char* v : rr.m_values) g.drawText(v, line.removeFromLeft(cw), juce::Justification::centred);
    }
}
