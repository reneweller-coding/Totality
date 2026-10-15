/**
 * @file Chord.cpp
 * @brief The dub chord's voices and bus.
 * @author Rene Weller
 */
#include "tot/synth/Chord.h"
#include "tot/Params.h"
#include <algorithm>
#include <cmath>

namespace tot
{

namespace
{
constexpr double TwoPiD = 6.283185307179586;   ///< 2 pi
constexpr double Ln1000 = 6.907755278982137;   ///< ln 1000: a decay of 60 dB
}   // namespace

void ChordSynth::prepare( double sampleRate )
{
    m_sr = sampleRate;
    for ( Voice &v : m_v ) v.m_amp.setSampleRate( sampleRate );
    reset();
}

void ChordSynth::reset()
{
    for ( Voice &v : m_v )
    {
        v.m_active = false;
        v.m_amp.kill();
        v.m_lpA.reset();
        v.m_lpB.reset();
        v.m_pitch = -1;
    }
    m_bandL.reset();
    m_bandR.reset();
    m_dipL.reset();
    m_dipR.reset();
    for ( int i = 0; i < 4; ++i ) m_apL[i] = m_apR[i] = 0.0f;
    setBus( 0 );
}

void ChordSynth::update( const float *p )
{
    m_detune = p[chord::DETUNE];
    m_width = p[chord::WIDTH];
    m_octave = static_cast<int>( std::lround( p[chord::OCTAVE] ) );
    m_attack = p[chord::ATTACK] * 0.001f;
    m_decay = p[chord::DECAY] * 0.001f;
    m_sustain = p[chord::SUSTAIN];
    m_release = p[chord::RELEASE] * 0.001f;
    m_bright = p[chord::BRIGHT];
    m_envAmt = p[chord::ENV_AMOUNT];
    m_fenvDecay = std::exp( -Ln1000 / ( std::max( 0.01, static_cast<double>( m_decay ) ) * m_sr ) );
    m_bandHz = p[chord::BAND];
    m_highPass = std::lround( p[chord::BAND_MODE] ) == 1;
    m_bandK = 1.0f / std::max( 0.2f, p[chord::BAND_Q] );
    m_sweep = p[chord::SWEEP];
    m_sweepRate = p[chord::SWEEP_RATE];
    m_bits = p[chord::CRUSH];
    m_crushMix = p[chord::CRUSH_MIX];
    m_phaser = p[chord::PHASER];
    m_phaserRate = p[chord::PHASER_RATE];
    m_dipGain = dbToGain( p[chord::DIP] );
}

void ChordSynth::noteOn( int pitch, float velocity, double late )
{
    // A free voice, else the oldest.
    int      pick = 0;
    uint32_t oldest = 0;
    for ( int i = 0; i < Voices; ++i )
    {
        if ( !m_v[i].m_active )
        {
            pick = i;
            break;
        }
        const uint32_t age = m_clock - m_v[i].m_age;
        if ( age >= oldest )
        {
            oldest = age;
            pick = i;
        }
    }
    Voice &v = m_v[pick];
    v.m_active = true;
    v.m_pitch = pitch;
    v.m_vel = clampv( velocity, 0.0f, 1.0f );
    v.m_age = m_clock++;
    v.m_n = 0;
    const double hz = midiToHz( pitch + 12 * m_octave );
    const double spread = std::pow( 2.0, m_detune / 1200.0 );
    v.m_a.set( hz / spread, m_sr, 0.0f, 0.5f );
    v.m_b.set( hz * spread, m_sr, 0.0f, 0.5f );
    v.m_a.restart( 0.0, late );
    v.m_b.restart( 0.37, late );
    v.m_amp.setTimes( m_attack, m_decay, m_sustain, m_release );
    v.m_amp.noteOn();
    v.m_amp.advanceAttack( late );
    v.m_fenv = std::pow( m_fenvDecay, late );
}

void ChordSynth::noteOff( int pitch )
{
    for ( Voice &v : m_v )
        if ( v.m_active && v.m_pitch == pitch ) v.m_amp.noteOff();
}

void ChordSynth::setBus( int64_t sample )
{
    const float  fs = static_cast<float>( m_sr );
    const double t = static_cast<double>( sample ) / m_sr;
    const float  centre =
        m_bandHz * static_cast<float>( std::pow( 2.0, m_sweep * std::sin( TwoPiD * m_sweepRate * t ) ) );
    m_bandL.setK( centre, m_bandK, fs );
    m_bandR.setK( centre, m_bandK, fs );
    m_dipL.setQ( 600.0f, 0.7f, fs );
    m_dipR.setQ( 600.0f, 0.7f, fs );
    // The phaser's notches sweep 300 Hz .. 3 kHz; one first-order all-pass coefficient for all four stages.
    const double ph = 0.5 + 0.5 * std::sin( TwoPiD * m_phaserRate * t + 1.1 );
    const double fc = 300.0 * std::pow( 10.0, ph );
    const double w = std::tan( TwoPiD * 0.5 * std::min( fc, 0.45 * m_sr ) / m_sr );
    m_apCoef = static_cast<float>( ( w - 1.0 ) / ( w + 1.0 ) );
}

void ChordSynth::process( float *L, float *R, int n, int64_t sample )
{
    const float fs = static_cast<float>( m_sr );
    // Width 1: oscillator a hard left, b hard right; width 0: both in the middle (constant power).
    const float near = std::cos( 0.25f * Pi * ( 1.0f - m_width ) ), far = std::sin( 0.25f * Pi * ( 1.0f - m_width ) );
    for ( int i = 0; i < n; ++i )
    {
        L[i] = 0.0f;
        R[i] = 0.0f;
    }
    for ( Voice &v : m_v )
    {
        if ( !v.m_active ) continue;
        for ( int i = 0; i < n; ++i )
        {
            // The voice's low pass follows its envelope every eighth sample of the note (its own clock, not the host's).
            if ( ( v.m_n & 7u ) == 0 )
            {
                const float fc =
                    std::min( 0.45f * fs, m_bright * static_cast<float>( std::pow( 2.0, m_envAmt * v.m_fenv ) ) );
                v.m_lpA.setQ( fc, 0.7071f, fs );
                v.m_lpB.copyCoefficients( v.m_lpA );
            }
            ++v.m_n;
            const float e = v.m_amp.process() * v.m_vel;
            // Its envelope ended: free at this very sample. (Freed at the span's end, it ran its filters on for a
            // host-dependent while, and a voice taken again started from a state the blocks had chosen.)
            if ( !v.m_amp.isActive() )
            {
                v.m_active = false;
                break;
            }
            const float a = v.m_lpA.lp( v.m_a.next() ) * e, b = v.m_lpB.lp( v.m_b.next() ) * e;
            v.m_fenv *= m_fenvDecay;
            L[i] += a * near + b * far;
            R[i] += b * near + a * far;
        }
    }
    const float levels = std::pow( 2.0f, m_bits - 1.0f );
    for ( int i = 0; i < n; ++i )
    {
        if ( ( ( sample + i ) & 15 ) == 0 ) setBus( sample + i );
        float lp, bp, hp;
        m_bandL.tick( L[i], lp, bp, hp );
        float l = m_highPass ? hp : bp * m_bandL.m_k;
        m_bandR.tick( R[i], lp, bp, hp );
        float r = m_highPass ? hp : bp * m_bandR.m_k;
        // A few bits, mixed in.
        l += m_crushMix * ( std::round( l * levels ) / levels - l );
        r += m_crushMix * ( std::round( r * levels ) / levels - r );
        // The phaser: four first-order all-passes, their sum with the dry signal (notches), at the depth Phaser.
        float xl = l, xr = r;
        for ( int s = 0; s < 4; ++s )
        {
            const float yl = m_apCoef * xl + m_apL[s];
            m_apL[s] = xl - m_apCoef * yl;
            xl = yl;
            const float yr = m_apCoef * xr + m_apR[s];
            m_apR[s] = xr - m_apCoef * yr;
            xr = yr;
        }
        l = l + m_phaser * 0.5f * ( xl - l );
        r = r + m_phaser * 0.5f * ( xr - r );
        // The dip at 600 Hz.
        m_dipL.tick( l, lp, bp, hp );
        l += ( m_dipGain - 1.0f ) * bp * m_dipL.m_k;
        m_dipR.tick( r, lp, bp, hp );
        r += ( m_dipGain - 1.0f ) * bp * m_dipR.m_k;
        L[i] = l;
        R[i] = r;
    }
}

}   // namespace tot
