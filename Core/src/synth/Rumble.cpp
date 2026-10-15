/**
 * @file Rumble.cpp
 * @brief The rumble: split, hall, clip, band filters, the phase-continuing sub, the duck.
 */
#include "tot/synth/Rumble.h"
#include "tot/Params.h"
#include <algorithm>
#include <cmath>

namespace tot {

namespace {
constexpr double TwoPiD = 6.283185307179586;   ///< 2 pi
/** @brief Gain on the hall's return before the clip (calibrated, see Rumble.h). */
constexpr float HallGain = 3.98f;   // +12 dB: the return's peaks at the defaults near -1 dBFS
/** @brief Gain on the band after its filters (calibrated, see Rumble.h). */
constexpr float BandGain = 1.80f;   // +5.1 dB: at Level 0 dB the rumble (band and sub) as loud as the kick, RMS over a beat
}

void Rumble::prepare(double sampleRate, int maxBlock)
{
    m_sr = sampleRate;
    m_hall.prepare(sampleRate);
    m_duck.prepare(sampleRate);
    const size_t n = static_cast<size_t>(std::max(1, maxBlock));
    m_in.assign(n, 0.0f);
    m_hallL.assign(n, 0.0f);
    m_hallR.assign(n, 0.0f);
    m_band.assign(n, 0.0f);
    m_pre.assign(n, 0.0f);
    m_subOut.assign(n, 0.0f);
    reset();
}

void Rumble::reset()
{
    m_split1.reset();
    m_split2.reset();
    m_hall.reset();
    m_os.reset();
    m_postHp.reset();
    m_postLp.reset();
    m_duck.reset();
    m_env = 0.0f;
    m_subPhase = 0.0;
    m_resetIn = -1;
    m_sinceKick = 0;
}

void Rumble::update(const float* v, float f0)
{
    const float fs = static_cast<float>(m_sr);
    m_level = v[rumble::LEVEL] <= -59.9f ? 0.0f : dbToGain(v[rumble::LEVEL]);
    m_subGain = v[rumble::SUB] <= -59.9f ? 0.0f : dbToGain(v[rumble::SUB]);
    const float split = v[rumble::SPLIT];
    m_split1.setK(split, 1.41421356f, fs);
    m_split2.setK(split, 1.41421356f, fs);
    m_hall.set(v[rumble::SIZE], v[rumble::DECAY], v[rumble::DAMPING], v[rumble::PRE_DELAY] * 0.001f * fs, 20.0f, 4000.0f);
    m_clipGain = dbToGain(v[rumble::DRIVE]);
    m_postHp.setK(split, 1.41421356f, fs);
    m_postLp.set(std::max(split * 1.2f, f0 * v[rumble::RATIO]), v[rumble::RESONANCE], fs);
    m_envAtt = 1.0f - std::exp(-1.0f / (v[rumble::SUB_ATTACK] * 0.001f * fs));
    m_envRel = 1.0f - std::exp(-1.0f / (v[rumble::SUB_RELEASE] * 0.001f * fs));
    m_duckDepth = 1.0f - dbToGain(-v[rumble::DUCK]);
    m_duckHoldMs = v[rumble::DUCK_HOLD];
    m_duckReleaseMs = v[rumble::DUCK_RELEASE];
    m_duck.set(m_duckDepth, DuckAttackMs, m_duckHoldMs, m_duckReleaseMs);
    // The sub's own pitch changes only with a kick (kick()), so a retuned kick and its sub never disagree.
}

void Rumble::kick(double late, double asymptote, double f0)
{
    m_duck.trigger(late);
    m_kickLate = late;
    m_kickC = asymptote;
    m_kickF0 = f0;
    m_sinceKick = 0;
    // The sub keeps its old phase until the gate has shut, then continues the new kick (Rumble.h).
    m_resetIn = std::max(1, static_cast<int>(std::ceil(DuckAttackMs * 0.001 * m_sr)));
}

void Rumble::process(const float* kickBody, float* out, int n)
{
    // The split and the hall.
    for (int i = 0; i < n; ++i) {
        float lp, bp, hp;
        m_split1.tick(kickBody[i], lp, bp, hp);
        m_split2.tick(hp, lp, bp, hp);
        m_in[static_cast<size_t>(i)] = hp;
    }
    m_hall.process(m_in.data(), m_in.data(), m_hallL.data(), m_hallR.data(), n);

    const float g = m_clipGain, ig = 1.0f / m_clipGain;
    for (int i = 0; i < n; ++i) {
        const size_t k = static_cast<size_t>(i);
        const float x = 0.5f * (m_hallL[k] + m_hallR[k]) * HallGain;
        m_pre[k] = x;
        float y = m_oversample ? m_os.process(x, [g, ig](float s) { return std::tanh(g * s) * ig; }) : std::tanh(g * x) * ig;
        float lp, bp, hp;
        m_postHp.tick(y, lp, bp, hp);
        y = m_postLp.lp(hp) * BandGain;
        // The band's envelope (full-wave, one pole up and down), which the sub follows.
        const float a = std::fabs(y);
        m_env += (a - m_env) * (a > m_env ? m_envAtt : m_envRel);
        // The duck: the band by its depth, the sub all the way.
        const float duckGain = m_duck.next();
        const float gate = 1.0f - m_duck.amount();
        m_band[k] = y * duckGain;
        if (m_resetIn >= 0 && --m_resetIn < 0) {
            // Continue the kick: this sample lies t = (samples since the trigger + late) / fs after its ideal start,
            // where its phase is f0 t + c.
            const double t = (static_cast<double>(m_sinceKick) + m_kickLate) / m_sr;
            const double ph = m_kickF0 * t + m_kickC;
            m_subPhase = ph - std::floor(ph);
            m_f0 = m_kickF0;
        }
        // A sine's mean absolute value is 2/pi of its peak: the sub stands at the band's peak level times Sub.
        m_subOut[k] = static_cast<float>(std::sin(TwoPiD * m_subPhase)) * m_env * 1.5707963f * m_subGain * gate;
        out[i] = (m_band[k] + m_subOut[k]) * m_level;
        m_subInc = m_f0 / m_sr;
        m_subPhase += m_subInc;
        m_subPhase -= std::floor(m_subPhase);
        ++m_sinceKick;
    }
}

} // namespace tot
