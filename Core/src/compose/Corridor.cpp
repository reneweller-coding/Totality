/**
 * @file Corridor.cpp
 * @brief The hypnosis corridor's measures on the score.
 * @author Rene Weller
 */
#include "tot/compose/Corridor.h"
#include <algorithm>
#include <cmath>

namespace tot
{

namespace
{

/**
 * @brief The band layer @p id counts in: 0 low, 1 mid, 2 high.
 * @param[in] id  the layer
 * @return its band: 0 low, 1 mid, 2 high
 */
int bandOfLayer( LayerIdE id )
{
    switch ( id )
    {
    case LayerIdE::KICK:
    case LayerIdE::GHOST_KICK:
    case LayerIdE::BASS: return 0;
    case LayerIdE::CLOSED_HAT:
    case LayerIdE::ROLLING_HAT:
    case LayerIdE::OPEN_HAT:
    case LayerIdE::RIDE:
    case LayerIdE::SHAKER: return 2;
    case LayerIdE::DRONE:
    case LayerIdE::TEXTURE: return -1;
    default: return 1;
    }
}

/**
 * @brief Pearson correlation of two profiles over their 48 values; 1 for two silent bars, 0 for one silent.
 * @param[in] a  the first bar's profile
 * @param[in] b  the second bar's profile
 * @return their correlation
 */
float correlation( const BarProfile &a, const BarProfile &b )
{
    double ma = 0.0, mb = 0.0;
    for ( int k = 0; k < 3; ++k )
        for ( int s = 0; s < Steps; ++s )
        {
            ma += a.m_v[k][s];
            mb += b.m_v[k][s];
        }
    ma /= 48.0;
    mb /= 48.0;
    double sab = 0.0, saa = 0.0, sbb = 0.0;
    for ( int k = 0; k < 3; ++k )
        for ( int s = 0; s < Steps; ++s )
        {
            const double x = a.m_v[k][s] - ma, y = b.m_v[k][s] - mb;
            sab += x * y;
            saa += x * x;
            sbb += y * y;
        }
    if ( saa <= 1e-12 && sbb <= 1e-12 ) return 1.0f;
    if ( saa <= 1e-12 || sbb <= 1e-12 ) return 0.0f;
    return static_cast<float>( sab / std::sqrt( saa * sbb ) );
}

}   // namespace

void partBands( const RackPlan &plan, int *bandOfPart )
{
    for ( int i = 0; i < NumParts; ++i ) bandOfPart[i] = -1;
    bandOfPart[static_cast<int>( PartE::KICK )] = 0;
    bandOfPart[static_cast<int>( PartE::SUB )] = 0;
    bandOfPart[static_cast<int>( PartE::BASS )] = 0;
    bandOfPart[static_cast<int>( PartE::PING )] = 1;
    bandOfPart[static_cast<int>( PartE::CHORD )] = 1;
    bandOfPart[static_cast<int>( PartE::ACID )] = 1;
    for ( int l = 0; l < NumLayers; ++l )
    {
        const int lane = layerLane( plan, static_cast<LayerIdE>( l ) );
        if ( lane >= 0 ) bandOfPart[static_cast<int>( percPart( lane ) )] = bandOfLayer( static_cast<LayerIdE>( l ) );
    }
}

std::vector<BarProfile> barProfiles( const std::vector<NoteEvent> &notes, int firstBar, int count, const int *bands )
{
    std::vector<BarProfile> out( static_cast<size_t>( std::max( 0, count ) ) );
    for ( const NoteEvent &n : notes )
    {
        const int band = bands[static_cast<int>( n.m_part )];
        if ( band < 0 || n.m_velocity <= 0.0f ) continue;
        const double pos = n.m_beat * 4.0;                                  // sixteenths from beat 0
        const int    step = static_cast<int>( std::floor( pos + 0.25 ) );   // a late note stays on its step
        const int    bar = step / Steps - firstBar;
        if ( bar < 0 || bar >= count ) continue;
        BarProfile &b = out[static_cast<size_t>( bar )];
        b.m_v[band][step % Steps] += n.m_velocity;
        ++b.m_onsets;
    }
    return out;
}

CorridorStats corridorOf( const std::vector<BarProfile> &bars, int from, int to )
{
    CorridorStats st;
    from = std::max( 0, from );
    to = std::min( to, static_cast<int>( bars.size() ) );
    if ( to <= from ) return st;
    std::vector<float> sims;
    double             micro = 0.0, onsets = 0.0;
    int                microN = 0;
    for ( int i = from; i < to; ++i )
    {
        const BarProfile &b = bars[static_cast<size_t>( i )];
        onsets += b.m_onsets;
        float best = -1.0f;
        for ( int lag : { 1, 2, 4 } )
            if ( i - lag >= 0 ) best = std::max( best, correlation( b, bars[static_cast<size_t>( i - lag )] ) );
        if ( best > -1.0f ) sims.push_back( best );
        if ( i >= 1 )
        {
            const BarProfile &a = bars[static_cast<size_t>( i - 1 )];
            for ( int k = 0; k < 3; ++k )
            {
                double ea = 0.0, eb = 0.0;
                for ( int s = 0; s < Steps; ++s )
                {
                    ea += a.m_v[k][s] * a.m_v[k][s];
                    eb += b.m_v[k][s] * b.m_v[k][s];
                }
                if ( ea <= 1e-9 && eb <= 1e-9 ) continue;
                micro += std::min( 12.0, std::fabs( 10.0 * std::log10( ( eb + 1e-3 ) / ( ea + 1e-3 ) ) ) );
                ++microN;
            }
        }
    }
    if ( !sims.empty() )
    {
        std::nth_element( sims.begin(), sims.begin() + static_cast<long>( sims.size() / 2 ), sims.end() );
        st.m_similarity = sims[sims.size() / 2];
    }
    st.m_micro = microN > 0 ? static_cast<float>( micro / microN ) : 0.0f;
    st.m_density = static_cast<float>( onsets / ( to - from ) );
    return st;
}

}   // namespace tot
