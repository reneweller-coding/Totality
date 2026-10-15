/**
 * @file Dub.cpp
 * @brief The dub chain and the multiband duck.
 * @author Rene Weller
 */
#include "tot/fx/Dub.h"
#include "tot/Params.h"
#include <algorithm>
#include <cmath>

namespace tot {

namespace {
constexpr float Sqrt2 = 1.41421356f;   ///< sqrt 2
const double EchoBeats[] = { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };   ///< dub.echo_time: 1/16, 1/8, 3/16, 1/4, 3/8, 1/2
}

void DubChain::prepare(double sampleRate, int maxBlock)
{
    m_sr = sampleRate;
    m_echo.prepare(sampleRate, 2.5, 0x4543484Full);   // "ECHO"
    m_spring.prepare(sampleRate);
    m_plate.prepare(sampleRate);
    const size_t n = static_cast<size_t>(std::max(1, maxBlock));
    m_eL.assign(n, 0.0f); m_eR.assign(n, 0.0f);
    m_sL.assign(n, 0.0f); m_sR.assign(n, 0.0f);
    m_pL.assign(n, 0.0f); m_pR.assign(n, 0.0f);
    reset();
}

void DubChain::reset()
{
    m_echo.reset();
    m_plate.reset();
    m_spring.reset();
}

void DubChain::update(const float* v, double bpm)
{
    EchoSettings e;
    const int t = std::clamp(static_cast<int>(std::lround(v[dub::ECHO_TIME])), 0, 5);
    e.m_delaySeconds = EchoBeats[t] * 60.0 / std::max(20.0, bpm);
    e.m_feedback = v[dub::FEEDBACK];
    e.m_toneHz = v[dub::TONE];
    e.m_lowCutHz = v[dub::LOW_CUT];
    e.m_wowMs = v[dub::WOW];
    e.m_flutterMs = v[dub::FLUTTER];
    e.m_driveDb = v[dub::DRIVE];
    e.m_pingPong = true;
    m_echo.set(e);
    m_spring.set(v[dub::SPRING_DECAY], 4000.0f);
    m_plate.set(v[dub::PLATE_DECAY], v[dub::PLATE_DAMPING], v[dub::PLATE_PRE_DELAY] * 0.001f * static_cast<float>(m_sr),
               v[dub::PLATE_LOW_CUT], 8000.0f);
    m_echoGain = v[dub::ECHO_RETURN] <= -59.9f ? 0.0f : dbToGain(v[dub::ECHO_RETURN]);
    m_springGain = v[dub::SPRING] <= -59.9f ? 0.0f : dbToGain(v[dub::SPRING]);
    m_plateGain = v[dub::PLATE_RETURN] <= -59.9f ? 0.0f : dbToGain(v[dub::PLATE_RETURN]);
}

void DubChain::process(const float* echoL, const float* echoR, const float* plateL, const float* plateR, float* L, float* R, int n)
{
    for (int i = 0; i < n; ++i) { m_eL[i] = m_eR[i] = m_sL[i] = m_sR[i] = m_pL[i] = m_pR[i] = 0.0f; }
    m_echo.process(echoL, echoR, m_eL.data(), m_eR.data(), n);
    m_spring.process(echoL, echoR, m_sL.data(), m_sR.data(), n);
    m_plate.process(plateL, plateR, m_pL.data(), m_pR.data(), n);
    for (int i = 0; i < n; ++i) {
        const size_t k = static_cast<size_t>(i);
        L[i] = m_eL[k] * m_echoGain + m_sL[k] * m_springGain + m_pL[k] * m_plateGain;
        R[i] = m_eR[k] * m_echoGain + m_sR[k] * m_springGain + m_pR[k] * m_plateGain;
    }
}

void MultibandDucker::prepare(double sampleRate)
{
    m_duck.prepare(sampleRate);
    const float fs = static_cast<float>(sampleRate);
    for (Bands& b : m_b) {
        for (Svf* f : { &b.m_split1, &b.m_low2, &b.m_high2 }) f->setK(200.0f, Sqrt2, fs);
        for (Svf* f : { &b.m_split2, &b.m_mid2, &b.m_top2, &b.m_ap }) f->setK(2000.0f, Sqrt2, fs);
    }
    reset();
}

void MultibandDucker::reset()
{
    m_duck.reset();
    for (Bands& b : m_b)
        for (Svf* f : { &b.m_split1, &b.m_low2, &b.m_high2, &b.m_split2, &b.m_mid2, &b.m_top2, &b.m_ap }) f->reset();
}

void MultibandDucker::set(float lowDb, float midDb, float holdMs, float releaseMs)
{
    m_lowDepth = 1.0f - dbToGain(-lowDb);
    m_midDepth = 1.0f - dbToGain(-midDb);
    m_duck.set(1.0f, 1.0f, holdMs, releaseMs);
}

float MultibandDucker::band(Bands& b, float x, float gl, float gm)
{
    float lp, bp, hp, l1, h1;
    b.m_split1.tick(x, l1, bp, h1);
    const float low = b.m_low2.lp(l1);
    b.m_high2.tick(h1, lp, bp, hp);
    const float high = hp;
    b.m_split2.tick(high, l1, bp, h1);
    const float mid = b.m_mid2.lp(l1);
    b.m_top2.tick(h1, lp, bp, hp);
    const float top = hp;
    // The low band through the 2 kHz crossover's allpass (LP2^2 + HP2^2 = x - 2 k bp), in phase with the others.
    b.m_ap.tick(low, lp, bp, hp);
    const float lowAp = low - 2.0f * Sqrt2 * bp;
    return lowAp * gl + mid * gm + top;
}

void MultibandDucker::process(float* L, float* R, int n, float* gains)
{
    float* ch[2] = { L, R };
    for (int i = 0; i < n; ++i) {
        m_duck.next();
        const float a = m_duck.amount();
        const float gl = 1.0f - m_lowDepth * a, gm = 1.0f - m_midDepth * a;
        if (gains != nullptr) { gains[2 * i] = gl; gains[2 * i + 1] = gm; }
        for (int c = 0; c < 2; ++c) ch[c][i] = band(m_b[c], ch[c][i], gl, gm);
    }
}

MultibandDucker::Replica MultibandDucker::replica() const
{
    Replica r{ { m_b[0], m_b[1] } };
    for (Bands& b : r.m_b)
        for (Svf* f : { &b.m_split1, &b.m_low2, &b.m_high2, &b.m_split2, &b.m_mid2, &b.m_top2, &b.m_ap }) f->reset();
    return r;
}

void MultibandDucker::apply(Replica& r, const float* gains, float* L, float* R, int n)
{
    for (int i = 0; i < n; ++i) {
        L[i] = band(r.m_b[0], L[i], gains[2 * i], gains[2 * i + 1]);
        R[i] = band(r.m_b[1], R[i], gains[2 * i], gains[2 * i + 1]);
    }
}

} // namespace tot
