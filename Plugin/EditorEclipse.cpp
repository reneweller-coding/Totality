/**
 * @file EditorEclipse.cpp
 * @brief The Patterns tab (EditorEclipse.h).
 */
#include "EditorEclipse.h"
#include "EditorTheme.h"
#include <algorithm>
#include <cmath>

using namespace tot;

namespace {

/** @brief A part's colour: the drums the corona, the tones the families. */
juce::Colour partColour(Part p)
{
    using totui::Family;
    switch (p) {
    case Part::Sub: return totui::familyColour(Family::Space);
    case Part::Ping: return totui::familyColour(Family::Motion);
    case Part::Bass: return totui::familyColour(Family::Filter);
    case Part::Acid: return totui::colour::onset;
    case Part::Chord: case Part::Drone: case Part::Texture: return totui::familyColour(Family::Envelope);
    default: return totui::colour::amber;
    }
}

/** @brief A part's name: a percussion lane by its role ("Closed Hat"), the others as the score names them. */
juce::String partName(const ParamStore& s, int part)
{
    const int lane = part - static_cast<int>(Part::Perc1);
    if (lane >= 0 && lane < kPercLanes) return kPercRoleNames[std::clamp(s.getInt(s.id(Module::Perc, lane, perc::Role)), 0, kNumPercRoles - 1)];
    return kPartNames[part];
}

} // namespace

EclipsePage::EclipsePage(TotalityProcessor& p) : proc_(p) { startTimerHz(30); }

void EclipsePage::gather(double beat)
{
    rings_.clear();
    kick_.clear();
    const int bar = static_cast<int>(std::floor(beat / 4.0));
    bar_ = bar;
    // In a set: the deck of the track that owns the low end here.
    deck_ = 0;
    const int t = proc_.trackAt(beat);
    if (playing_.isSet && t >= 0 && t < static_cast<int>(playing_.tracks.size())) deck_ = playing_.tracks[static_cast<size_t>(t)].deck;
    const Score& s = playing_.set.decks[deck_];
    const double b0 = 4.0 * bar, b1 = b0 + 4.0;
    auto it = std::lower_bound(s.notes.begin(), s.notes.end(), b0 - 0.1, [](const NoteEvent& n, double b) { return n.beat < b; });
    std::vector<int> index(kNumParts, -1);
    for (; it != s.notes.end() && it->beat < b1 - 0.1; ++it) {
        const NoteEvent& n = *it;
        if (n.velocity <= 0.0f) continue;
        const float pos = static_cast<float>(std::clamp((n.beat - b0) / 4.0, 0.0, 0.9999));
        if (n.part == Part::Kick) { kick_.push_back(pos); continue; }
        const int p = static_cast<int>(n.part);
        if (index[static_cast<size_t>(p)] < 0) {
            index[static_cast<size_t>(p)] = static_cast<int>(rings_.size());
            rings_.push_back({ p, {} });
        }
        rings_[static_cast<size_t>(index[static_cast<size_t>(p)])].onsets.push_back({ pos, n.velocity });
    }
    // The rings from the inside out in part order: the low end near the disc, the tones outside.
    std::sort(rings_.begin(), rings_.end(), [](const Ring& a, const Ring& b) { return a.part < b.part; });
}

void EclipsePage::timerCallback()
{
    if (proc_.scoreVersion() != version_) {
        version_ = proc_.scoreVersion();
        proc_.copyPlaying(playing_);
        bar_ = -1;
    }
    const double beat = proc_.positionBeats();
    if (static_cast<int>(std::floor(beat / 4.0)) != bar_) gather(beat);
    repaint();
}

void EclipsePage::paint(juce::Graphics& g)
{
    using namespace totui::colour;
    g.fillAll(bg);
    const auto area = getLocalBounds().reduced(12);
    const int side = std::min(area.getHeight(), area.getWidth() * 55 / 100);
    const juce::Rectangle<float> eclipse = area.withWidth(side).withHeight(side).toFloat();
    const juce::Point<float> c = eclipse.getCentre();
    const float outer = side * 0.48f, disc = side * 0.13f;
    const double beat = proc_.positionBeats();
    const float phase = static_cast<float>(beat / 4.0 - std::floor(beat / 4.0));
    const auto angleOf = [](float pos) { return -juce::MathConstants<float>::halfPi + juce::MathConstants<float>::twoPi * pos; };

    // Conjunctions: sixteenths where three rings or more meet.
    int count[16] = {};
    for (const Ring& r : rings_) {
        bool seen[16] = {};
        for (const auto& o : r.onsets) {
            const int step = std::clamp(static_cast<int>(std::lround(o.first * 16.0f)) % 16, 0, 15);
            if (!seen[step]) { seen[step] = true; ++count[step]; }
        }
    }
    // The corona: brighter on the kick's beats just after the hand passed them.
    float kickGlow = 0.0f;
    for (float k : kick_) {
        const float d = phase - k;
        if (d >= 0.0f && d < 0.12f) kickGlow = std::max(kickGlow, 1.0f - d / 0.12f);
    }
    const float coronaR = disc * (1.9f + 0.35f * kickGlow);
    g.setGradientFill(juce::ColourGradient(amber.withAlpha(0.55f + 0.35f * kickGlow), c.x, c.y, amber.withAlpha(0.0f), c.x + coronaR, c.y, true));
    g.fillEllipse(c.x - coronaR, c.y - coronaR, 2.0f * coronaR, 2.0f * coronaR);
    for (int s = 0; s < 16; ++s) {
        if (count[s] < 3) continue;
        const float a = angleOf(static_cast<float>(s) / 16.0f);
        const float lit = std::min(1.0f, 0.25f + 0.15f * static_cast<float>(count[s]));
        juce::Path ray;
        const float w = 0.07f;
        ray.addPieSegment(c.x - outer, c.y - outer, 2.0f * outer, 2.0f * outer, a + juce::MathConstants<float>::halfPi - w,
                          a + juce::MathConstants<float>::halfPi + w, disc / outer);
        g.setColour(amber.withAlpha(0.10f * lit));
        g.fillPath(ray);
    }
    // The rings and their beads.
    const int n = static_cast<int>(rings_.size());
    for (int i = 0; i < n; ++i) {
        const Ring& r = rings_[static_cast<size_t>(i)];
        const float rad = disc * 1.7f + (outer - disc * 1.7f) * (n <= 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(n - 1));
        const juce::Colour col = partColour(static_cast<Part>(r.part));
        g.setColour(faint.withAlpha(0.6f));
        g.drawEllipse(c.x - rad, c.y - rad, 2.0f * rad, 2.0f * rad, 1.0f);
        for (const auto& o : r.onsets) {
            const float a = angleOf(o.first);
            const float d = phase - o.first;
            const float lit = d >= 0.0f && d < 0.08f ? 1.0f - d / 0.08f : 0.0f;
            const float pr = 2.0f + 4.0f * o.second + 3.0f * lit;
            g.setColour(col.withAlpha(0.45f + 0.55f * std::max(lit, o.second * 0.6f)));
            g.fillEllipse(c.x + rad * std::cos(a) - pr, c.y + rad * std::sin(a) - pr, 2.0f * pr, 2.0f * pr);
        }
        g.setColour(dim);
        g.setFont(juce::FontOptions(9.5f));
        g.drawText(partName(proc_.store(), r.part), static_cast<int>(c.x + 4.0f), static_cast<int>(c.y - rad - 11.0f), 90, 11, juce::Justification::left);
    }
    // The umbra, the kick's beads on its rim, and the hand.
    g.setColour(juce::Colour(5, 5, 7));
    g.fillEllipse(c.x - disc, c.y - disc, 2.0f * disc, 2.0f * disc);
    for (float k : kick_) {
        const float a = angleOf(k);
        g.setColour(juce::Colour(255, 246, 226).withAlpha(0.8f));
        g.fillEllipse(c.x + disc * std::cos(a) - 3.0f, c.y + disc * std::sin(a) - 3.0f, 6.0f, 6.0f);
    }
    const float ha = angleOf(phase);
    g.setColour(onset.withAlpha(0.85f));
    g.drawLine(c.x + disc * std::cos(ha), c.y + disc * std::sin(ha), c.x + outer * std::cos(ha), c.y + outer * std::sin(ha), 1.5f);
    g.setColour(ink);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("bar " + juce::String(bar_ + 1) + (playing_.isSet ? juce::String(", deck ") + juce::String::charToString(static_cast<juce::juce_wchar>('A' + deck_)) : juce::String()),
               eclipse.toNearestInt().removeFromTop(18), juce::Justification::topLeft);

    // The grid: sixteen steps per part of the bar.
    auto grid = area.withTrimmedLeft(side + 20).toFloat();
    if (grid.getWidth() < 120.0f) return;
    std::vector<std::pair<int, std::vector<std::pair<float, float>>>> rows;
    if (!kick_.empty()) {
        std::vector<std::pair<float, float>> k;
        for (float x : kick_) k.push_back({ x, 1.0f });
        rows.push_back({ static_cast<int>(Part::Kick), k });
    }
    for (const Ring& r : rings_) rows.push_back({ r.part, r.onsets });
    const float rowH = std::min(26.0f, (grid.getHeight() - 20.0f) / static_cast<float>(std::max<size_t>(1, rows.size())));
    const float nameW = 86.0f, cellW = (grid.getWidth() - nameW) / 16.0f;
    g.setFont(juce::FontOptions(10.0f));
    for (int s = 0; s < 16; ++s) {
        g.setColour(s % 4 == 0 ? dim : faint);
        g.drawText(juce::String(s + 1), static_cast<int>(grid.getX() + nameW + cellW * s), static_cast<int>(grid.getY()), static_cast<int>(cellW), 14,
                   juce::Justification::centred);
    }
    for (size_t i = 0; i < rows.size(); ++i) {
        const float y = grid.getY() + 18.0f + rowH * static_cast<float>(i);
        g.setColour(dim);
        g.drawText(partName(proc_.store(), rows[i].first), static_cast<int>(grid.getX()), static_cast<int>(y), static_cast<int>(nameW) - 4, static_cast<int>(rowH),
                   juce::Justification::centredLeft);
        for (int s = 0; s < 16; ++s) {
            g.setColour(s / 4 % 2 == 0 ? group : raised);
            g.fillRect(grid.getX() + nameW + cellW * s + 1.0f, y + 1.0f, cellW - 2.0f, rowH - 2.0f);
        }
        const juce::Colour col = partColour(static_cast<Part>(rows[i].first));
        for (const auto& o : rows[i].second) {
            const float stepF = o.first * 16.0f;
            const int s = std::clamp(static_cast<int>(std::lround(stepF)) % 16, 0, 15);
            g.setColour(col.withAlpha(0.3f + 0.7f * o.second));
            g.fillRect(grid.getX() + nameW + cellW * s + 3.0f, y + 3.0f, cellW - 6.0f, rowH - 6.0f);
            // Off the grid: a tick where the onset really lies.
            const float off = stepF - static_cast<float>(std::lround(stepF));
            if (std::fabs(off) > 0.04f) {
                g.setColour(ink.withAlpha(0.7f));
                g.fillRect(grid.getX() + nameW + cellW * (static_cast<float>(s) + 0.5f + off) - 0.5f, y + 2.0f, 1.0f, rowH - 4.0f);
            }
        }
    }
    const float hx = grid.getX() + nameW + (grid.getWidth() - nameW) * phase;
    g.setColour(onset.withAlpha(0.7f));
    g.fillRect(hx, grid.getY() + 16.0f, 1.5f, rowH * static_cast<float>(rows.size()) + 4.0f);
}
