/**
 * @file Synth.cpp
 * @brief The mono synth.
 * @author Rene Weller
 */
#include "tot/synth/Synth.h"
#include "tot/Params.h"
#include <algorithm>
#include <cmath>

namespace tot {

namespace {
constexpr double PiD = 3.141592653589793;   ///< pi
constexpr double Ln1000 = 6.907755278982137;   ///< ln 1000: a decay of 60 dB
}

void MonoSynth::prepare(double sampleRate)
{
    m_sr = sampleRate;
    m_amp.setSampleRate(sampleRate);
    m_duck.prepare(sampleRate);
    m_hb = designHalfband(90.0, 0.06);
    m_down.setup(m_hb);
    reset();
}

void MonoSynth::reset()
{
    m_amp.kill();
    m_filt.clear();
    m_down.reset();
    m_duck.reset();
    m_hp.reset();
    m_lp.reset();
    m_fenv = 0.0;
    m_subPhase = 0.0;
    m_slidePending = false;
}

void MonoSynth::update(const float* v, float minLowCut)
{
    const float fs = static_cast<float>(m_sr);
    m_level = v[synth::LEVEL] <= -59.9f ? 0.0f : dbToGain(v[synth::LEVEL]);
    const float th = (clampv(v[synth::PAN], -1.0f, 1.0f) + 1.0f) * 0.25f * Pi;
    m_gl = std::cos(th) * 1.41421356f;
    m_gr = std::sin(th) * 1.41421356f;
    m_wave = v[synth::WAVE];
    m_pw = v[synth::PULSE_WIDTH];
    m_sub = v[synth::SUB_OSC];
    m_model = static_cast<FilterModelE>(std::clamp(static_cast<int>(std::lround(v[synth::FILTER])), 0, FilterModels - 2));
    m_cutoff = v[synth::CUTOFF];
    m_res = v[synth::RESONANCE];
    m_envAmt = v[synth::ENV_AMOUNT];
    m_filtDecayS = std::max(0.002, static_cast<double>(v[synth::DECAY]) * 0.001);
    m_fenvDecay = std::exp(-Ln1000 / (m_filtDecayS * 2.0 * m_sr));
    m_accent = v[synth::ACCENT];
    m_attack = v[synth::AMP_ATTACK] * 0.001f;
    m_decay = v[synth::AMP_DECAY] * 0.001f;
    m_sustain = v[synth::AMP_SUSTAIN];
    m_release = v[synth::AMP_RELEASE] * 0.001f;
    m_glideMs = v[synth::GLIDE];
    m_drive = 0.5f + 5.0f * v[synth::DRIVE];
    m_driveNorm = 1.0f / std::tanh(m_drive);
    m_keyTrack = v[synth::KEY_TRACK];
    m_hp.setK(std::max(v[synth::LOW_CUT], minLowCut), 1.41421356f, fs);
    m_lp.setK(std::min(v[synth::HIGH_CUT], 0.45f * fs), 1.41421356f, fs);
    m_duck.set(1.0f - dbToGain(-v[synth::DUCK]), 1.0f, 30.0f, v[synth::DUCK_RELEASE]);
}

void MonoSynth::noteOn(int pitch, float velocity, double late, bool accent, bool slide)
{
    m_targetHz = midiToHz(pitch);
    const bool legato = m_slidePending && m_amp.isActive();
    const double glide = legato ? (m_glideMs > 0.0f ? m_glideMs : 60.0) : m_glideMs;
    // The glide: an exponential approach of the pitch, a time constant of a third of the glide time.
    m_glideCoef = glide > 0.0 ? 1.0 - std::exp(-1.0 / (glide * 0.001 / 3.0 * 2.0 * m_sr)) : 1.0;
    if (glide <= 0.0 || !m_amp.isActive()) m_hz = m_targetHz;
    m_velocity = clampv(velocity, 0.0f, 1.0f);
    m_noteAccent = accent;
    if (!legato) {
        m_amp.setTimes(m_attack, m_decay, m_sustain, std::max(m_release, static_cast<float>(0.5 / m_targetHz)));
        m_amp.noteOn();
        m_amp.advanceAttack(late);
        m_fenv = std::pow(m_fenvDecay, 2.0 * late);
    }
    m_slidePending = slide;
}

void MonoSynth::noteOff()
{
    // A sliding note's off comes after the next note has begun (the rack makes it 0.3 beats long), and the engine drops
    // an off whose note is no longer the latest: an off that arrives is a rest after the slide, so the gate closes.
    m_amp.noteOff();
}

void MonoSynth::process(float* L, float* R, int n)
{
    const double sr2 = 2.0 * m_sr;
    const float accentOct = m_noteAccent ? 1.5f * m_accent : 0.0f;
    const float accentGain = m_noteAccent ? dbToGain(4.0f * m_accent) : 1.0f;
    const float k = FilterVoicing::feedback(m_model, m_res);
    for (int i = 0; i < n; ++i) {
        if (!m_amp.isActive()) {
            // Silent from the sample its envelope ended: only the duck's curve runs on (sample-exact, so the host's
            // blocks cannot move where the voice rests).
            for (; i < n; ++i) { m_duck.next(); L[i] = 0.0f; R[i] = 0.0f; }
            break;
        }
        float pair[2];
        for (int h = 0; h < 2; ++h) {
            m_hz += (m_targetHz - m_hz) * m_glideCoef;
            m_osc.set(m_hz, sr2, m_wave, m_pw);
            float x = m_osc.next();
            // The sub: a square an octave down, naive but low (its first alias lies far under its own level at bass
            // pitches, and the filter follows).
            m_subPhase += 0.5 * m_hz / sr2;
            m_subPhase -= std::floor(m_subPhase);
            x += m_sub * (m_subPhase < 0.5 ? 1.0f : -1.0f);
            x = std::tanh(x * m_drive) * m_driveNorm;
            const double track = std::pow(m_hz / 110.0, static_cast<double>(m_keyTrack));
            const double fc = std::min(0.42 * sr2, m_cutoff * track * std::pow(2.0, (m_envAmt + accentOct) * m_fenv));
            const float g = static_cast<float>(std::tan(PiD * fc / sr2));
            // The knee: transparent under 1, then towards 2. A cutoff moved fast by the envelope pumps the integrators of
            // the diode ladder, the Polivoks and the Wasp at full resonance far beyond any input (peaks of 27 in the self
            // test); at 2x, so the knee's harmonics are filtered by the downsampler.
            const float f = m_filt.tick(m_model, x * 0.5f, g, k, 0.0f);
            const float a = std::fabs(f);
            pair[h] = a <= 1.0f ? f : std::copysign(1.0f + std::tanh(a - 1.0f), f);
            m_fenv *= m_fenvDecay;
        }
        float y = m_down.process(pair[0], pair[1]);
        float lpo, bp, hp;
        m_hp.tick(y, lpo, bp, hp);
        y = m_lp.lp(hp);
        y *= m_amp.process() * m_velocity * accentGain * m_duck.next() * m_level;
        L[i] = y * m_gl;
        R[i] = y * m_gr;
    }
}

} // namespace tot
