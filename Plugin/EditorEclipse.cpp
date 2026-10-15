/**
 * @file EditorEclipse.cpp
 * @brief The Patterns tab (EditorEclipse.h).
 * @author Rene Weller
 */
#include "EditorEclipse.h"
#include "EditorTheme.h"
#include <algorithm>
#include <cmath>

using namespace tot;

namespace {

/**
 * @brief A part's colour: the drums the corona, the tones the families.
 * @param[in] p  the part
 * @return its colour
 */
juce::Colour partColour(PartE p)
{
    using totui::Family;
    switch (p) {
    case PartE::SUB: return totui::familyColour(Family::SPACE);
    case PartE::PING: return totui::familyColour(Family::MOTION);
    case PartE::BASS: return totui::familyColour(Family::FILTER);
    case PartE::ACID: return totui::colour::Onset;
    case PartE::CHORD: case PartE::DRONE: case PartE::TEXTURE: return totui::familyColour(Family::ENVELOPE);
    default: return totui::colour::Accent;
    }
}

/**
 * @brief A part's name: a percussion lane by its role ("Closed Hat"), the others as the score names them.
 * @param[in] s     the parameters
 * @param[in] part  the part
 * @return its name
 */
juce::String partName(const ParamStore& s, int part)
{
    const int lane = part - static_cast<int>(PartE::PERC1);
    if (lane >= 0 && lane < PercLanes) return PercRoleNames[std::clamp(s.getInt(s.id(ModuleE::PERC, lane, perc::ROLE)), 0, NumPercRoles - 1)];
    return PartNames[part];
}

} // namespace

EclipsePage::EclipsePage(TotalityProcessor& p) : m_proc(p) { startTimerHz(30); }

void EclipsePage::gather(double beat)
{
    m_rings.clear();
    m_kick.clear();
    const int bar = static_cast<int>(std::floor(beat / 4.0));
    m_bar = bar;
    // In a set: the deck of the track that owns the low end here.
    m_deck = 0;
    const int t = m_proc.trackAt(beat);
    if (m_playing.m_isSet && t >= 0 && t < static_cast<int>(m_playing.m_tracks.size())) m_deck = m_playing.m_tracks[static_cast<size_t>(t)].m_deck;
    const Score& s = m_playing.m_set.m_decks[m_deck];
    const double b0 = 4.0 * bar, b1 = b0 + 4.0;
    auto it = std::lower_bound(s.m_notes.begin(), s.m_notes.end(), b0 - 0.1, [](const NoteEvent& n, double b) { return n.m_beat < b; });
    std::vector<int> index(NumParts, -1);
    for (; it != s.m_notes.end() && it->m_beat < b1 - 0.1; ++it) {
        const NoteEvent& n = *it;
        if (n.m_velocity <= 0.0f) continue;
        const float pos = static_cast<float>(std::clamp((n.m_beat - b0) / 4.0, 0.0, 0.9999));
        if (n.m_part == PartE::KICK) { m_kick.push_back(pos); continue; }
        const int p = static_cast<int>(n.m_part);
        if (index[static_cast<size_t>(p)] < 0) {
            index[static_cast<size_t>(p)] = static_cast<int>(m_rings.size());
            m_rings.push_back({ p, {} });
        }
        m_rings[static_cast<size_t>(index[static_cast<size_t>(p)])].m_onsets.push_back({ pos, n.m_velocity });
    }
    // The rings from the inside out in part order: the low end near the disc, the tones outside.
    std::sort(m_rings.begin(), m_rings.end(), [](const Ring& a, const Ring& b) { return a.m_part < b.m_part; });
}

void EclipsePage::timerCallback()
{
    if (m_proc.scoreVersion() != m_version) {
        m_version = m_proc.scoreVersion();
        m_proc.copyPlaying(m_playing);
        m_bar = -1;
    }
    const double beat = m_proc.positionBeats();
    if (static_cast<int>(std::floor(beat / 4.0)) != m_bar) gather(beat);
    repaint();
}

void EclipsePage::paint(juce::Graphics& g)
{
    using namespace totui::colour;
    g.fillAll(Bg);
    const auto area = getLocalBounds().reduced(12);
    const int side = std::min(area.getHeight(), area.getWidth() * 55 / 100);
    const juce::Rectangle<float> eclipse = area.withWidth(side).withHeight(side).toFloat();
    const juce::Point<float> c = eclipse.getCentre();
    const float outer = side * 0.48f, disc = side * 0.13f;
    const double beat = m_proc.positionBeats();
    const float phase = static_cast<float>(beat / 4.0 - std::floor(beat / 4.0));
    const auto angleOf = [](float pos) { return -juce::MathConstants<float>::halfPi + juce::MathConstants<float>::twoPi * pos; };

    // Conjunctions: sixteenths where three rings or more meet.
    int count[16] = {};
    for (const Ring& r : m_rings) {
        bool seen[16] = {};
        for (const auto& o : r.m_onsets) {
            const int step = std::clamp(static_cast<int>(std::lround(o.first * 16.0f)) % 16, 0, 15);
            if (!seen[step]) { seen[step] = true; ++count[step]; }
        }
    }
    // The corona: brighter on the kick's beats just after the hand passed them.
    float kickGlow = 0.0f;
    for (float k : m_kick) {
        const float d = phase - k;
        if (d >= 0.0f && d < 0.12f) kickGlow = std::max(kickGlow, 1.0f - d / 0.12f);
    }
    const float coronaR = disc * (1.9f + 0.35f * kickGlow);
    g.setGradientFill(juce::ColourGradient(Accent.withAlpha(0.55f + 0.35f * kickGlow), c.x, c.y, Accent.withAlpha(0.0f), c.x + coronaR, c.y, true));
    g.fillEllipse(c.x - coronaR, c.y - coronaR, 2.0f * coronaR, 2.0f * coronaR);
    for (int s = 0; s < 16; ++s) {
        if (count[s] < 3) continue;
        const float a = angleOf(static_cast<float>(s) / 16.0f);
        const float lit = std::min(1.0f, 0.25f + 0.15f * static_cast<float>(count[s]));
        juce::Path ray;
        const float w = 0.07f;
        ray.addPieSegment(c.x - outer, c.y - outer, 2.0f * outer, 2.0f * outer, a + juce::MathConstants<float>::halfPi - w,
                          a + juce::MathConstants<float>::halfPi + w, disc / outer);
        g.setColour(Accent.withAlpha(0.10f * lit));
        g.fillPath(ray);
    }
    // The rings and their beads.
    const int n = static_cast<int>(m_rings.size());
    for (int i = 0; i < n; ++i) {
        const Ring& r = m_rings[static_cast<size_t>(i)];
        const float rad = disc * 1.7f + (outer - disc * 1.7f) * (n <= 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(n - 1));
        const juce::Colour col = partColour(static_cast<PartE>(r.m_part));
        g.setColour(Faint.withAlpha(0.6f));
        g.drawEllipse(c.x - rad, c.y - rad, 2.0f * rad, 2.0f * rad, 1.0f);
        for (const auto& o : r.m_onsets) {
            const float a = angleOf(o.first);
            const float d = phase - o.first;
            const float lit = d >= 0.0f && d < 0.08f ? 1.0f - d / 0.08f : 0.0f;
            const float pr = 2.0f + 4.0f * o.second + 3.0f * lit;
            g.setColour(col.withAlpha(0.45f + 0.55f * std::max(lit, o.second * 0.6f)));
            g.fillEllipse(c.x + rad * std::cos(a) - pr, c.y + rad * std::sin(a) - pr, 2.0f * pr, 2.0f * pr);
        }
        g.setColour(Dim);
        g.setFont(juce::FontOptions(9.5f));
        g.drawText(partName(m_proc.store(), r.m_part), static_cast<int>(c.x + 4.0f), static_cast<int>(c.y - rad - 11.0f), 90, 11, juce::Justification::left);
    }
    // The umbra, the kick's beads on its rim, and the hand.
    g.setColour(juce::Colour(5, 5, 7));
    g.fillEllipse(c.x - disc, c.y - disc, 2.0f * disc, 2.0f * disc);
    for (float k : m_kick) {
        const float a = angleOf(k);
        g.setColour(juce::Colour(255, 246, 226).withAlpha(0.8f));
        g.fillEllipse(c.x + disc * std::cos(a) - 3.0f, c.y + disc * std::sin(a) - 3.0f, 6.0f, 6.0f);
    }
    const float ha = angleOf(phase);
    g.setColour(Onset.withAlpha(0.85f));
    g.drawLine(c.x + disc * std::cos(ha), c.y + disc * std::sin(ha), c.x + outer * std::cos(ha), c.y + outer * std::sin(ha), 1.5f);
    g.setColour(Ink);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("bar " + juce::String(m_bar + 1) + (m_playing.m_isSet ? juce::String(", deck ") + juce::String::charToString(static_cast<juce::juce_wchar>('A' + m_deck)) : juce::String()),
               eclipse.toNearestInt().removeFromTop(18), juce::Justification::topLeft);

    // The grid: sixteen steps per part of the bar.
    auto grid = area.withTrimmedLeft(side + 20).toFloat();
    if (grid.getWidth() < 120.0f) return;
    std::vector<std::pair<int, std::vector<std::pair<float, float>>>> rows;
    if (!m_kick.empty()) {
        std::vector<std::pair<float, float>> k;
        for (float x : m_kick) k.push_back({ x, 1.0f });
        rows.push_back({ static_cast<int>(PartE::KICK), k });
    }
    for (const Ring& r : m_rings) rows.push_back({ r.m_part, r.m_onsets });
    const float rowH = std::min(26.0f, (grid.getHeight() - 20.0f) / static_cast<float>(std::max<size_t>(1, rows.size())));
    const float nameW = 86.0f, cellW = (grid.getWidth() - nameW) / 16.0f;
    g.setFont(juce::FontOptions(10.0f));
    for (int s = 0; s < 16; ++s) {
        g.setColour(s % 4 == 0 ? Dim : Faint);
        g.drawText(juce::String(s + 1), static_cast<int>(grid.getX() + nameW + cellW * s), static_cast<int>(grid.getY()), static_cast<int>(cellW), 14,
                   juce::Justification::centred);
    }
    for (size_t i = 0; i < rows.size(); ++i) {
        const float y = grid.getY() + 18.0f + rowH * static_cast<float>(i);
        g.setColour(Dim);
        g.drawText(partName(m_proc.store(), rows[i].first), static_cast<int>(grid.getX()), static_cast<int>(y), static_cast<int>(nameW) - 4, static_cast<int>(rowH),
                   juce::Justification::centredLeft);
        for (int s = 0; s < 16; ++s) {
            g.setColour(s / 4 % 2 == 0 ? Group : Raised);
            g.fillRect(grid.getX() + nameW + cellW * s + 1.0f, y + 1.0f, cellW - 2.0f, rowH - 2.0f);
        }
        const juce::Colour col = partColour(static_cast<PartE>(rows[i].first));
        for (const auto& o : rows[i].second) {
            const float stepF = o.first * 16.0f;
            const int s = std::clamp(static_cast<int>(std::lround(stepF)) % 16, 0, 15);
            g.setColour(col.withAlpha(0.3f + 0.7f * o.second));
            g.fillRect(grid.getX() + nameW + cellW * s + 3.0f, y + 3.0f, cellW - 6.0f, rowH - 6.0f);
            // Off the grid: a tick where the onset really lies.
            const float off = stepF - static_cast<float>(std::lround(stepF));
            if (std::fabs(off) > 0.04f) {
                g.setColour(Ink.withAlpha(0.7f));
                g.fillRect(grid.getX() + nameW + cellW * (static_cast<float>(s) + 0.5f + off) - 0.5f, y + 2.0f, 1.0f, rowH - 4.0f);
            }
        }
    }
    const float hx = grid.getX() + nameW + (grid.getWidth() - nameW) * phase;
    g.setColour(Onset.withAlpha(0.7f));
    g.fillRect(hx, grid.getY() + 16.0f, 1.5f, rowH * static_cast<float>(rows.size()) + 4.0f);
}
