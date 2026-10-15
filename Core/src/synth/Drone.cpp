/**
 * @file Drone.cpp
 * @brief The drone and the texture.
 * @author Rene Weller
 */
#include "tot/synth/Drone.h"
#include "tot/Params.h"
#include <algorithm>
#include <cmath>

namespace tot
{

namespace
{
constexpr double TwoPiD = 6.283185307179586;   ///< 2 pi
}

// ------------------------------------------------------------------------------------------------------------ drone

void Drone::prepare( double sampleRate )
{
    m_sr = sampleRate;
    m_amp.setSampleRate( sampleRate );
    reset();
}

void Drone::reset()
{
    m_amp.kill();
    m_lpL.reset();
    m_lpR.reset();
}

void Drone::update( const float *v )
{
    m_level = v[drone::LEVEL] <= -59.9f ? 0.0f : dbToGain( v[drone::LEVEL] );
    m_octave = static_cast<int>( std::lround( v[drone::OCTAVE] ) );
    m_detune = v[drone::DETUNE];
    m_cutoff = v[drone::CUTOFF];
    m_res = v[drone::RESONANCE];
    m_sweep = v[drone::SWEEP];
    m_sweepBeats = 4.0 * v[drone::SWEEP_BARS];
    m_attack = v[drone::ATTACK];
    m_release = v[drone::RELEASE];
}

void Drone::noteOn( int pitch, float velocity )
{
    m_hz = midiToHz( pitch + 12 * m_octave );
    while ( m_hz < 150.0 ) m_hz *= 2.0;   // above the band the kick and the rumble own
    m_vel = clampv( velocity, 0.0f, 1.0f );
    const double spread = std::pow( 2.0, m_detune / 1200.0 );
    m_a.set( m_hz / spread, m_sr, 0.0f, 0.5f );
    m_b.set( m_hz * spread, m_sr, 0.0f, 0.5f );
    m_amp.setTimes( m_attack, 0.1f, 1.0f, m_release );
    m_amp.noteOn();
}

void Drone::noteOff()
{
    m_amp.noteOff();
}

void Drone::at( double beat )
{
    const float fs = static_cast<float>( m_sr );
    const float fc = std::min( 0.45f * fs, m_cutoff * static_cast<float>( std::pow(
                                                          2.0, m_sweep * std::sin( TwoPiD * beat / m_sweepBeats ) ) ) );
    m_lpL.set( fc, m_res, fs );
    m_lpR.copyCoefficients( m_lpL );
}

void Drone::process( float *L, float *R, int n )
{
    for ( int i = 0; i < n; ++i )
    {
        if ( !m_amp.isActive() )
        {
            for ( ; i < n; ++i ) L[i] = R[i] = 0.0f;
            break;
        }   // resting, from the sample it ended
        const float e = m_amp.process() * m_vel * m_level;
        L[i] = m_lpL.lp( m_a.next() ) * e;
        R[i] = m_lpR.lp( m_b.next() ) * e;
    }
}

// ---------------------------------------------------------------------------------------------------------- texture

void Texture::prepare( double sampleRate )
{
    m_sr = sampleRate;
    m_gainStep = static_cast<float>( 1.0 / ( 2.0 * sampleRate ) );
    reset();
}

void Texture::reset()
{
    for ( int c = 0; c < 2; ++c )
    {
        Side &s = m_s[c];
        s.m_rng.seed( 0x5445585455524500ull + static_cast<uint64_t>( c ) );   // "TEXTURE"
        s.m_burst = 0.0f;
        s.m_walk = 0.0f;
        s.m_crackleBand.reset();
        s.m_erosionBand.reset();
        s.m_hissLp.reset();
        const float fs = static_cast<float>( m_sr );
        s.m_crackleBand.setQ( 2500.0f, 1.2f, fs );
        s.m_erosionBand.setQ( 900.0f, 0.8f, fs );
        s.m_hissLp.setQ( 6000.0f, 0.7071f, fs );
        s.m_burstDecay = static_cast<float>( std::exp( -1.0 / ( 0.0015 * m_sr ) ) );
    }
    m_humPhase = 0.0;
    m_gain = 0.0f;
    m_on = false;
}

void Texture::update( const float *v )
{
    m_level = v[texture::LEVEL] <= -59.9f ? 0.0f : dbToGain( v[texture::LEVEL] );
    m_crackle = v[texture::CRACKLE];
    m_hum = v[texture::HUM];
    m_humHz = v[texture::HUM_HZ];
    m_erosion = v[texture::EROSION];
    m_width = v[texture::WIDTH];
}

void Texture::process( float *L, float *R, int n )
{
    const float rate = static_cast<float>( 10.0 * m_crackle / m_sr );   // clicks per sample
    const float walkStep = static_cast<float>( 1.0 / m_sr );
    float       out[2];
    for ( int i = 0; i < n; ++i )
    {
        if ( !m_on && m_gain <= 0.0f )
        {
            for ( ; i < n; ++i ) L[i] = R[i] = 0.0f;
            break;
        }   // faded out: resting
        m_gain = m_on ? std::min( 1.0f, m_gain + m_gainStep ) : std::max( 0.0f, m_gain - m_gainStep );
        m_humPhase += m_humHz / m_sr;
        m_humPhase -= std::floor( m_humPhase );
        const double p = TwoPiD * m_humPhase;
        const float  hum = static_cast<float>( std::sin( p ) + 0.5 * std::sin( 2 * p ) + 0.3 * std::sin( 3 * p ) +
                                               0.15 * std::sin( 5 * p ) ) *
                           0.35f * m_hum;
        for ( int c = 0; c < 2; ++c )
        {
            Side &s = m_s[c];
            // A click: a burst whose size is heavy-tailed (the cube of a uniform: many ticks, a few pops).
            if ( s.m_rng.uniform() < rate )
            {
                const float u = s.m_rng.uniform();
                s.m_burst += 0.2f + 2.5f * u * u * u;
            }
            const float nz = s.m_rng.bipolar();
            float       lp, bp, hp;
            s.m_crackleBand.tick( nz * s.m_burst, lp, bp, hp );
            const float crackle = bp * s.m_crackleBand.m_k;
            s.m_burst *= s.m_burstDecay;
            const float hiss = s.m_hissLp.lp( nz ) * 0.02f * m_crackle;
            // Erosion: band noise under a slow random walk of its level.
            s.m_walk += walkStep * ( s.m_rng.bipolar() * 3.0f - s.m_walk * 0.3f );
            s.m_erosionBand.tick( s.m_rng.bipolar(), lp, bp, hp );
            const float erosion =
                bp * s.m_erosionBand.m_k * m_erosion * 0.25f * ( 0.6f + 0.4f * std::tanh( s.m_walk * 4.0f ) );
            out[c] = crackle * 0.5f + hiss + erosion + hum;
        }
        const float mid = 0.5f * ( out[0] + out[1] ), side = 0.5f * ( out[0] - out[1] ) * m_width;
        L[i] = ( mid + side ) * m_level * m_gain;
        R[i] = ( mid - side ) * m_level * m_gain;
    }
}

}   // namespace tot
