/**
 * @file Ping.cpp
 * @brief The ping voices: FM, the low-pass gate, the wandering band pass.
 * @author Rene Weller
 */
#include "tot/synth/Ping.h"
#include "tot/Params.h"
#include <algorithm>
#include <cmath>

namespace tot {

namespace {
constexpr double TwoPiD = 6.283185307179586;   ///< 2 pi
constexpr double Ln1000 = 6.907755278982137;   ///< ln 1000: a decay of 60 dB
constexpr int CoefEvery = 4;   ///< the gate's low pass is retuned every four samples
}

void Ping::prepare(double sampleRate)
{
    m_sr = sampleRate;
    m_vacAttack = 1.0 - std::exp(-1.0 / (0.001 * m_sr));
    reset();
}

void Ping::reset()
{
    for (Voice& v : m_v) { v = Voice{}; v.m_lpg.reset(); }
    m_bandL.reset();
    m_bandR.reset();
    m_bandL.setK(m_bandHz, m_bandK, static_cast<float>(m_sr));
    m_bandR.setK(m_bandHz, m_bandK, static_cast<float>(m_sr));
    m_next = 0;
}

bool Ping::active() const
{
    for (const Voice& v : m_v) if (v.m_active) return true;
    return false;
}

void Ping::update(const float* p)
{
    m_level = p[ping::LEVEL] <= -59.9f ? 0.0f : dbToGain(p[ping::LEVEL]);
    m_pan = p[ping::PAN];
    m_width = p[ping::WIDTH];
    m_ratio = p[ping::RATIO];
    m_index = p[ping::INDEX];
    m_indexTau = std::max(0.002, static_cast<double>(p[ping::INDEX_DECAY]) * 0.001);
    m_pitchAmt = p[ping::PITCH_AMOUNT];
    m_pitchTau = std::max(0.002, static_cast<double>(p[ping::PITCH_DECAY]) * 0.001);
    m_decayS = std::max(0.01, static_cast<double>(p[ping::DECAY]) * 0.001);
    m_lpgMix = p[ping::LPG];
    m_lpgRelease = std::max(0.01, static_cast<double>(p[ping::LPG_RELEASE]) * 0.001);
    m_damping = 2.0f - 1.9f * clampv(p[ping::RESONANCE], 0.0f, 1.0f);
    m_bandHz = p[ping::BAND];
    m_bandK = 1.0f / std::max(0.2f, p[ping::BAND_Q]);
    m_bandMix = p[ping::BAND_MIX];
    m_sweepOct = p[ping::SWEEP];
    m_sweepRate = p[ping::SWEEP_RATE];
}

void Ping::noteOn(int pitch, float velocity, double late, uint32_t serial)
{
    Voice& v = m_v[m_next];
    m_next = (m_next + 1) % Voices;
    v = Voice{};
    v.m_active = true;
    v.m_f0 = midiToHz(pitch);
    v.m_vel = clampv(velocity, 0.0f, 1.0f);
    v.m_dp = std::exp(-1.0 / (m_pitchTau * m_sr));
    v.m_di = std::exp(-1.0 / (m_indexTau * m_sr));
    v.m_da = std::exp(-Ln1000 / (m_decayS * m_sr));
    v.m_ep = std::pow(v.m_dp, late);
    v.m_ei = std::pow(v.m_di, late);
    v.m_ea = std::pow(v.m_da, late);
    // The phases `late` samples in, at the starting frequency (close enough for the fraction of a sample).
    const double f = v.m_f0 * (1.0 + m_pitchAmt);
    v.m_pc = f * late / m_sr;
    v.m_pm = f * m_ratio * late / m_sr;
    // Its place: Pan, spread by Width, from the note's serial (the same every render).
    const uint64_t h = mixSeed(0x50494E47ull, serial);   // "PING"
    const float u = static_cast<float>((h >> 40) * (1.0 / 16777216.0));
    const float pos = clampv(m_pan + m_width * (2.0f * u - 1.0f), -1.0f, 1.0f);
    const float th = (pos + 1.0f) * 0.25f * Pi;
    v.m_gl = std::cos(th) * 1.41421356f;
    v.m_gr = std::sin(th) * 1.41421356f;
    v.m_lpg.reset();
    v.m_lpg.setK(80.0f, m_damping, static_cast<float>(m_sr));
}

void Ping::process(float* L, float* R, int n, int64_t sample)
{
    for (int i = 0; i < n; ++i) { L[i] = 0.0f; R[i] = 0.0f; }
    const float fs = static_cast<float>(m_sr);
    for (Voice& v : m_v) {
        if (!v.m_active) continue;
        for (int i = 0; i < n; ++i) {
            const double f = v.m_f0 * (1.0 + m_pitchAmt * v.m_ep);
            const double idx = m_index * v.m_vel * v.m_ei;
            const double y = std::sin(TwoPiD * v.m_pc + idx * std::sin(TwoPiD * v.m_pm));
            v.m_pc += f / m_sr;
            v.m_pm += f * m_ratio / m_sr;
            v.m_pc -= std::floor(v.m_pc);
            v.m_pm -= std::floor(v.m_pm);
            v.m_ep *= v.m_dp;
            v.m_ei *= v.m_di;
            v.m_ea *= v.m_da;
            // The vactrol: a millisecond up, then down with a time constant that grows as it darkens.
            if (v.m_age < static_cast<int>(0.002 * m_sr)) v.m_vac += (1.0 - v.m_vac) * m_vacAttack;
            else {
                const double dark = 1.0 - v.m_vac;
                const double tau = m_lpgRelease * (0.2 + 1.6 * dark * dark);
                v.m_vac -= v.m_vac / (tau * m_sr);
            }
            if ((v.m_age % CoefEvery) == 0) v.m_lpg.setK(static_cast<float>(80.0 * std::pow(225.0, v.m_vac)), m_damping, fs);
            const float gated = v.m_lpg.lp(static_cast<float>(y)) * static_cast<float>(std::pow(v.m_vac, 1.5));
            const float plain = static_cast<float>(y * v.m_ea);
            const float out = v.m_vel * (m_lpgMix * gated + (1.0f - m_lpgMix) * plain);
            L[i] += out * v.m_gl;
            R[i] += out * v.m_gr;
            ++v.m_age;
            // Silent: free at this very sample, not at the span's end (where it falls depends on the host's blocks).
            if (v.m_age > 256 && v.m_ea < 1.0e-4 && v.m_vac < 1.0e-4) { v.m_active = false; break; }
        }
    }
    // The bus: a band pass whose centre a slow sine moves, its phase from the absolute sample.
    for (int i = 0; i < n; ++i) {
        // On an absolute raster of 16 samples only, so the host's blocks cannot move a coefficient.
        if (((sample + i) & 15) == 0) {
            const double t = static_cast<double>(sample + i) / m_sr;
            const float centre = m_bandHz * static_cast<float>(std::pow(2.0, m_sweepOct * std::sin(TwoPiD * m_sweepRate * t)));
            m_bandL.setK(centre, m_bandK, fs);
            m_bandR.setK(centre, m_bandK, fs);
        }
        float lp, bp, hp;
        m_bandL.tick(L[i], lp, bp, hp);
        const float bl = bp * m_bandL.m_k;
        m_bandR.tick(R[i], lp, bp, hp);
        const float br = bp * m_bandR.m_k;
        L[i] = (L[i] * (1.0f - m_bandMix) + bl * m_bandMix) * m_level;
        R[i] = (R[i] * (1.0f - m_bandMix) + br * m_bandMix) * m_level;
    }
}

} // namespace tot
