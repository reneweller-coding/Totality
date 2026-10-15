/**
 * @file Rack.cpp
 * @brief The layer tables of Dok. 8.2 and 8.3, the rolls, the cycles, the ghost chains and the rules.
 * @author Rene Weller
 */
#include "tot/pattern/Rack.h"
#include "tot/compose/Style.h"
#include "tot/Dsp.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <vector>

namespace tot
{

const char *const LayerNames[NumLayers] = { "kick", "ghost kick", "closed hat", "rolling hat", "open hat",  "ride",
                                            "clap", "clap b",     "clap ghost", "shaker",      "tom/conga", "rim",
                                            "bass", "ping",       "chord",      "drone",       "acid",      "texture" };

namespace
{

/**
 * @brief A MIDI velocity as 0..1.
 * @param[in] midi  the MIDI velocity, 0 .. 127
 * @return the velocity, 0..1
 */
constexpr float V( int midi )
{
    return static_cast<float>( midi ) / 127.0f;
}

/**
 * The matrices of Dok. 8.2 (steps 1 .. 16 there are indices 0 .. 15 here) and the velocities, ranges, offsets and
 * swing shares of Dok. 8.3. Velocities: the middle of the range, with the random spread covering the range.
 */
const LayerDef Layers[NumLayers] = {
    // Kick: the quarters, 127, on the grid, never swung.
    { LayerKindE::ANCHOR,
      { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 },
      { V( 127 ), 0, 0, 0, V( 127 ), 0, 0, 0, V( 127 ), 0, 0, 0, V( 127 ), 0, 0, 0 },
      0.0f,
      0.0f,
      0.0f,
      0.0f,
      0.25,
      false },
    // Ghost kick: before beats 2 and 4, rarely the last sixteenth; 40 .. 60.
    { LayerKindE::MOTION,
      { 0, 0, 0, .25f, 0, 0, 0, 0, 0, 0, 0, .25f, 0, 0, 0, .10f },
      { 0, 0, 0, V( 50 ), 0, 0, 0, 0, 0, 0, 0, V( 50 ), 0, 0, 0, V( 50 ) },
      V( 10 ),
      0.0f,
      0.0f,
      0.0f,
      0.25,
      false },
    // Closed hat on the offbeat: 80 .. 90 (capped at 89), pushed 0 .. 9 ms.
    { LayerKindE::ANCHOR,
      { 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0 },
      { 0, 0, V( 84 ), 0, 0, 0, V( 84 ), 0, 0, 0, V( 84 ), 0, 0, 0, V( 84 ), 0 },
      V( 5 ),
      0.0f,
      9.0f,
      1.0f,
      0.1,
      false },
    // Rolling hat: .9 / .7; base 64, accents 100 on the quarters and 80 before them; swung. Motion, as Dok. 8.2 says: the
    // reference measurement (PLAN 13.4, 27.09.2026) puts the hypnotic records' bar similarity at 0.87, and the study with
    // this layer rolled anew each bar at 0.87 too; as a loop it read 0.98 (steadier than any profile's references).
    { LayerKindE::MOTION,
      { .9f, .7f, .9f, .7f, .9f, .7f, .9f, .7f, .9f, .7f, .9f, .7f, .9f, .7f, .9f, .7f },
      { V( 100 ), V( 64 ), V( 64 ), V( 80 ), V( 100 ), V( 64 ), V( 64 ), V( 80 ), V( 100 ), V( 64 ), V( 64 ), V( 80 ),
        V( 100 ), V( 64 ), V( 64 ), V( 80 ) },
      V( 12 ),
      0.0f,
      0.0f,
      1.0f,
      0.1,
      false },
    // Open hat on the offbeats, 64 .. 90 (the one on beat 2 at 64), pushed 5 .. 15 ms.
    { LayerKindE::LOOP,
      { 0, 0, .6f, 0, 0, 0, .9f, 0, 0, 0, .6f, 0, 0, 0, .9f, 0 },
      { 0, 0, V( 77 ), 0, 0, 0, V( 64 ), 0, 0, 0, V( 77 ), 0, 0, 0, V( 77 ), 0 },
      V( 8 ),
      5.0f,
      15.0f,
      1.0f,
      0.5,
      false },
    // Ride on the eighths, 100 and 70 alternating, half swung.
    { LayerKindE::LOOP,
      { .8f, 0, .8f, 0, .8f, 0, .8f, 0, .8f, 0, .8f, 0, .8f, 0, .8f, 0 },
      { V( 100 ), 0, V( 70 ), 0, V( 100 ), 0, V( 70 ), 0, V( 100 ), 0, V( 70 ), 0, V( 100 ), 0, V( 70 ), 0 },
      V( 5 ),
      0.0f,
      5.0f,
      0.5f,
      0.5,
      false },
    // Clap A: 2 and 4, 110 .. 127, straight.
    { LayerKindE::LOOP,
      { 0, 0, 0, 0, .9f, 0, 0, 0, 0, 0, 0, 0, .9f, 0, 0, 0 },
      { 0, 0, 0, 0, V( 118 ), 0, 0, 0, 0, 0, 0, 0, V( 118 ), 0, 0, 0 },
      V( 8 ),
      0.0f,
      0.0f,
      0.0f,
      0.25,
      false },
    // Clap B: displaced.
    { LayerKindE::LOOP,
      { 0, 0, 0, 0, .8f, 0, 0, .5f, 0, .5f, 0, 0, .8f, 0, 0, .5f },
      { 0, 0, 0, 0, V( 118 ), 0, 0, V( 105 ), 0, V( 105 ), 0, 0, V( 118 ), 0, 0, V( 105 ) },
      V( 8 ),
      0.0f,
      0.0f,
      0.0f,
      0.25,
      false },
    // Clap ghosts: 50 .. 70, pushed 5 .. 12 ms, swung.
    { LayerKindE::MOTION,
      { 0, .15f, 0, .15f, 0, .15f, 0, .2f, 0, .15f, 0, .15f, 0, .15f, 0, .25f },
      { 0, V( 60 ), 0, V( 60 ), 0, V( 60 ), 0, V( 60 ), 0, V( 60 ), 0, V( 60 ), 0, V( 60 ), 0, V( 60 ) },
      V( 10 ),
      5.0f,
      12.0f,
      1.0f,
      0.25,
      true },
    // Shaker: 70 .. 100, pushed 5 .. 15 ms, swung.
    { LayerKindE::MOTION,
      { 0, 0, .7f, 0, 0, 0, .4f, 0, 0, 0, .7f, 0, 0, 0, .4f, 0 },
      { 0, 0, V( 85 ), 0, 0, 0, V( 85 ), 0, 0, 0, V( 85 ), 0, 0, 0, V( 85 ), 0 },
      V( 14 ),
      5.0f,
      15.0f,
      1.0f,
      0.25,
      true },
    // Tom/conga in the gaps.
    { LayerKindE::LOOP,
      { 0, .3f, 0, .3f, 0, .3f, 0, .3f, 0, .3f, 0, .3f, 0, .4f, .2f, .5f },
      { 0, V( 85 ), 0, V( 85 ), 0, V( 85 ), 0, V( 85 ), 0, V( 85 ), 0, V( 85 ), 0, V( 85 ), V( 85 ), V( 85 ) },
      V( 14 ),
      5.0f,
      15.0f,
      1.0f,
      0.25,
      true },
    // Rim: 70 .. 90, straight; 4 ms early where it meets the kick.
    { LayerKindE::LOOP,
      { .4f, 0, 0, 0, 0, 0, .2f, 0, 0, 0, 0, 0, 0, 0, .2f, 0 },
      { V( 80 ), 0, 0, 0, 0, 0, V( 80 ), 0, 0, 0, 0, 0, 0, 0, V( 80 ), 0 },
      V( 5 ),
      0.0f,
      0.0f,
      0.0f,
      0.1,
      true },
    // Bass: the sixteenth ostinato around the kicks, 90 .. 110, half swung.
    { LayerKindE::LOOP,
      { 0, .5f, .8f, .5f, 0, .5f, .8f, .5f, 0, .5f, .8f, .5f, 0, .5f, .8f, .6f },
      { 0, V( 100 ), V( 100 ), V( 100 ), 0, V( 100 ), V( 100 ), V( 100 ), 0, V( 100 ), V( 100 ), V( 100 ), 0, V( 100 ),
        V( 100 ), V( 100 ) },
      V( 5 ),
      0.0f,
      5.0f,
      0.5f,
      0.2,
      false },
    // Ping: its figure is cyclic (RackPlan::cycle); the table only gives its velocity, spread and feel.
    { LayerKindE::LOOP, { 0 }, { V( 95 ) }, V( 14 ), 0.0f, 6.0f, 0.5f, 0.25, true },
    // Chord: Dok. 8.2's stab -- the downbeat .6, the third eighth .3, beat 3's offbeat .4; 79 % velocity with a wide spread
    // (Brootle's "Random Velocity 79 %"), straight, an eighth long.
    { LayerKindE::LOOP,
      { .6f, 0, 0, 0, 0, 0, .3f, 0, 0, 0, .4f, 0, 0, 0, 0, 0 },
      { V( 100 ), 0, 0, 0, 0, 0, V( 100 ), 0, 0, 0, V( 100 ), 0, 0, 0, 0, 0 },
      V( 20 ),
      0.0f,
      0.0f,
      0.0f,
      0.5,
      false },
    // Drone: a note on the downbeat of every eighth bar, held for them (realizeBar).
    { LayerKindE::ANCHOR,
      { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { V( 90 ), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0.0f,
      0.0f,
      0.0f,
      0.0f,
      32.0,
      false },
    // Acid: the 303's sixteenths, three in four sounding; pitches, accents and slides rolled per block (realizeBar).
    { LayerKindE::LOOP,
      { .75f, .75f, .75f, .75f, .75f, .75f, .75f, .75f, .75f, .75f, .75f, .75f, .75f, .75f, .75f, .75f },
      { V( 96 ), V( 96 ), V( 96 ), V( 96 ), V( 96 ), V( 96 ), V( 96 ), V( 96 ), V( 96 ), V( 96 ), V( 96 ), V( 96 ),
        V( 96 ), V( 96 ), V( 96 ), V( 96 ) },
      V( 6 ),
      0.0f,
      3.0f,
      0.5f,
      0.18,
      false },
    // Texture: a note a bar, which gates it (Drone.h).
    { LayerKindE::ANCHOR,
      { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { V( 100 ), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      0.0f,
      0.0f,
      0.0f,
      0.0f,
      4.0,
      false },
};

/** @brief General MIDI notes of the layers (the ping's is its pitch, set from the plan). */
constexpr int LayerNote[NumLayers] = { 36, 36, 42, 44, 46, 51, 39, 39, 39, 70, 45, 37, 45, 60, 57, 57, 45, 60 };

/**
 * @brief A uniform number for (seed, a, b, c), the same whatever else was rolled.
 * @param[in] seed  the seed
 * @param[in] a     the first coordinate
 * @param[in] b     the second coordinate
 * @param[in] c     the third coordinate
 * @return a uniform number in [0, 1)
 */
float roll( uint64_t seed, uint64_t a, uint64_t b, uint64_t c )
{
    const uint64_t h = mixSeed( mixSeed( mixSeed( seed, a ), b ), c );
    return static_cast<float>( ( h >> 40 ) * ( 1.0 / 16777216.0 ) );
}

/** @brief The salts of the rack's random streams, one per kind of decision. */
enum SaltE : uint64_t
{
    BASE = 11,
    MUT_A = 12,
    MUT_B = 13,
    MOTION = 14,
    VEL = 15,
    JITTER = 16,
    CYCLE = 17,
    FILL = 18,
    ACID = 19
};

/**
 * @brief Whether layer @p li plays a cycle of its own period (not the sixteen-step matrix).
 * @param[in] plan  the rack plan
 * @param[in] li    the layer's index
 * @return true when it plays a cycle of its own period
 */
bool isCyclic( const RackPlan &plan, int li )
{
    return plan.m_period[li] > 0;
}

/**
 * @brief The seed of layer @p li's rolls in a block of candidate @p variant (0: the layer's own seed).
 * @param[in] plan     the rack plan
 * @param[in] li       the layer's index
 * @param[in] variant  the candidate (0: the layer's own seed)
 * @return the seed of the layer's rolls
 */
uint64_t seedOf( const RackPlan &plan, int li, uint32_t variant )
{
    return variant == 0 ? plan.m_layerSeed[li]
                        : mixSeed( plan.m_layerSeed[li], 0x5641524900000000ull + variant );   // "VARI"
}

/**
 * @brief Base velocity of a layer: its loudest step's.
 * @param[in] d  the layer
 * @return its loudest step's velocity
 */
float baseVelocity( const LayerDef &d )
{
    float v = 0.0f;
    for ( float x : d.m_vel ) v = std::max( v, x );
    return v > 0.0f ? v : 0.7f;
}

/**
 * @brief Position of sixteenth @p s of bar @p bar in a cyclic layer's period.
 * @param[in] plan  the rack plan
 * @param[in] li    the layer's index
 * @param[in] bar   the bar
 * @param[in] s     the sixteenth within it
 * @return the position in the layer's period
 */
int cyclePos( const RackPlan &plan, int li, int bar, int s )
{
    const int     period = std::max( 1, plan.m_period[li] );
    const int     reset = std::max( 1, plan.m_resetBars[li] );
    const int64_t first = static_cast<int64_t>( bar / reset ) * reset * Steps;
    return static_cast<int>( ( static_cast<int64_t>( bar ) * Steps + s - first ) % period );
}

}   // namespace

const LayerDef &layerDef( LayerIdE id )
{
    return Layers[static_cast<int>( id )];
}

int layerLane( const RackPlan &plan, LayerIdE id )
{
    switch ( id )
    {
    case LayerIdE::CLOSED_HAT: return 0;
    case LayerIdE::ROLLING_HAT: return 1;
    case LayerIdE::OPEN_HAT: return 2;
    case LayerIdE::RIDE: return 3;
    case LayerIdE::CLAP_A:
    case LayerIdE::CLAP_B: return 4;
    case LayerIdE::CLAP_GHOST: return 5;
    case LayerIdE::RIM: return 7;
    case LayerIdE::SHAKER: return 8;
    case LayerIdE::TOM_CONGA: return plan.m_tomLane;
    default: return -1;
    }
}

uint64_t euclidMask( int k, int n )
{
    // Bresenham's form of the Euclidean rhythm: step i sounds where (i k mod n) < k. Maximally even, the first onset on
    // step 0 -- a rotation of Bjorklund's (Toussaint 2005).
    uint64_t m = 0;
    for ( int i = 0; i < n; ++i )
        if ( ( i * k ) % n < k ) m |= uint64_t( 1 ) << i;
    return m;
}

uint64_t rotateMask( uint64_t mask, int n, int r )
{
    r = ( ( r % n ) + n ) % n;
    uint64_t out = 0;
    for ( int i = 0; i < n; ++i )
        if ( mask & ( uint64_t( 1 ) << i ) ) out |= uint64_t( 1 ) << ( ( i + r ) % n );
    return out;
}

uint16_t offQuarterMask( int k, int r )
{
    const uint64_t slots = rotateMask( euclidMask( k, 12 ), 12, r );
    uint16_t       m = 0;
    for ( int j = 0, step = 0; step < Steps; ++step )
    {
        if ( step % 4 == 0 ) continue;   // the quarters are the kick's
        if ( ( slots >> j++ ) & 1u ) m = static_cast<uint16_t>( m | ( 1u << step ) );
    }
    return m;
}

RackSettings rackSettings( const ParamStore &p )
{
    RackSettings        s;
    const StyleProfile &prof =
        styleProfile( static_cast<StyleE>( p.getInt( p.id( ModuleE::COMPOSE, 0, compose::STYLE ) ) ) );
    s.m_keyRoot = p.getInt( p.id( ModuleE::COMPOSE, 0, compose::KEY ) );
    s.m_scale = p.getInt( p.id( ModuleE::COMPOSE, 0, compose::SCALE ) );
    s.m_swing = prof.m_swingHigh <= 51.0f
                    ? 50.0f
                    : p.get( p.id( ModuleE::COMPOSE, 0, compose::SWING ) );   // Dub plays straight
    s.m_humanizeMs = p.get( p.id( ModuleE::COMPOSE, 0, compose::HUMANIZE ) );
    s.m_mutation = prof.m_mutation;
    s.m_motionScale = prof.m_motionScale;
    s.m_reroll = prof.m_reroll;
    s.m_polymeterChance = prof.m_polymeterChance;
    s.m_fillChance = prof.m_fillChance;
    return s;
}

RackPlan makeRackPlan( const ParamStore &p, uint64_t seed )
{
    return makeRackPlan( rackSettings( p ), seed );
}

RackPlan makeRackPlan( const RackSettings &st, uint64_t seed )
{
    RackPlan plan;
    plan.m_seed = seed;
    for ( uint64_t &s : plan.m_layerSeed ) s = seed;
    Rng rng;
    rng.seed( mixSeed( seed, 0x5241434Bull ) );   // "RACK"
    // The style's restlessness (RackPlan, Style.h), after the reference measurement's bar similarity (PLAN 13.4):
    // Hypnotic 0.87, Raw 0.92 and Dub 0.93, Ostgut 0.955 (most of the motion rolled per block). Hypnotic rolled every
    // bar while the mix was dull (Phase 2); with the balance fitted to the references (Phase 3: highs +10 dB) the hats and
    // the ping weigh more in the onset profiles, and 0.15 meets 0.87 (0.84 .. 0.87 over three seeds): its difference is
    // the ping's cycles.
    plan.m_mutation = st.m_mutation;
    plan.m_motionScale = st.m_motionScale;
    plan.m_reroll = st.m_reroll;
    plan.m_fillChance = st.m_fillChance;
    for ( int i = 0; i < NumLayers; ++i )
    {
        const LayerDef &d = Layers[i];
        const float     u = rng.uniform();
        // Dok. 8.2: one bar 0.10, two 0.55, four 0.35; the style may ask for more one-bar loops, the rest keeping 11 : 7.
        const float two = plan.m_oneBarLoops + ( 1.0f - plan.m_oneBarLoops ) * ( 0.55f / 0.90f );
        plan.m_loopBars[i] = d.m_kind != LayerKindE::LOOP ? 1 : ( u < plan.m_oneBarLoops ? 1 : ( u < two ? 2 : 4 ) );
        plan.m_offsetMs[i] = d.m_offsetLoMs + rng.uniform() * ( d.m_offsetHiMs - d.m_offsetLoMs );
    }
    plan.m_clapB = rng.uniform() < 0.3f;
    plan.m_tomLane = rng.uniform() < 0.5f ? 9 : 10;
    plan.m_keyRoot = st.m_keyRoot;
    plan.m_scale = st.m_scale;
    plan.m_swing = st.m_swing;
    plan.m_humanizeMs = st.m_humanizeMs;
    // The bass's root: the octave of the key's root between 41 and 82 Hz (E1 .. E2), never under 35 Hz (Dok. 8.4).
    plan.m_bassRoot = 24 + plan.m_keyRoot;
    if ( midiToHz( plan.m_bassRoot ) < 41.0 ) plan.m_bassRoot += 12;
    // The bass alphabet (Dok. 8.6): {1} 0.3, {1,b7} 0.15, {1,5} 0.15, {1,b3} 0.1, {1,b2} 0.1, {1,4} 0.08, {1,b5} 0.07,
    // {1,b3,5,b7} 0.05.
    struct Set
    {
        float m_w;
        int   m_n;
        int   m_s[4];
    };
    static const Set Sets[] = {
        { 0.30f, 1, { 0 } },    { 0.15f, 2, { 0, 10 } }, { 0.15f, 2, { 0, 7 } }, { 0.10f, 2, { 0, 3 } },
        { 0.10f, 2, { 0, 1 } }, { 0.08f, 2, { 0, 5 } },  { 0.07f, 2, { 0, 6 } }, { 0.05f, 4, { 0, 3, 7, 10 } },
    };
    float      u = rng.uniform(), acc = 0.0f;
    const Set *pick = &Sets[0];
    for ( const Set &s : Sets )
    {
        acc += s.m_w;
        if ( u < acc )
        {
            pick = &s;
            break;
        }
    }
    plan.m_bassSize = pick->m_n;
    for ( int i = 0; i < 4; ++i ) plan.m_bassSet[i] = i < pick->m_n ? pick->m_s[i] : 0;

    // Phase 2. The ping's figure: a polymeter of 5 or 7 sixteenths (p 0.35 each) or a slipping loop of 15 or 17 (p 0.15
    // each), Euclidean, rotated; one or two pitch classes around the root in 220 .. 415 Hz (PLAN 5.5).
    {
        const int   li = static_cast<int>( LayerIdE::PING );
        const float w = rng.uniform();
        const int   period = w < 0.35f ? 5 : ( w < 0.70f ? 7 : ( w < 0.85f ? 15 : 17 ) );
        const int   k =
            period == 5 ? 2 : ( period == 7 ? 3 : ( period == 15 ? 5 + rng.below( 2 ) : 6 + rng.below( 2 ) ) );
        plan.m_period[li] = period;
        plan.m_resetBars[li] = period > 12 ? 32 : 16;
        plan.m_cycle[li] = rotateMask( euclidMask( k, period ), period, rng.below( period ) );
        plan.m_pingRoot = 57 + ( ( plan.m_keyRoot - 9 + 12 ) % 12 );   // A3 = 57 .. G#4 = 68
        static const int   Second[] = { 7, 10, 12, 3, 5 };
        static const float SecondW[] = { 0.35f, 0.25f, 0.20f, 0.10f, 0.10f };
        const float        v = rng.uniform();
        float              a2 = 0.0f;
        int                second = 7;
        for ( int i = 0; i < 5; ++i )
        {
            a2 += SecondW[i];
            if ( v < a2 )
            {
                second = Second[i];
                break;
            }
        }
        if ( !inScale( plan.m_scale, second ) ) second = 7;
        const bool twoTones = rng.uniform() < 0.7f;
        for ( int pos = 0; pos < period; ++pos )
            plan.m_pingNote[pos] = plan.m_pingRoot + ( ( twoTones && rng.uniform() < 0.35f ) ? second : 0 );
    }
    // Polymeter: one percussion layer with p 0.25, two with p 0.25, of period 3, 5, 6, 7 or 12, reset every 16 bars
    // (Dok. 8.2: "1-2 Perc-Layer ... p 0.5 pro Track"); the others Euclidean with p 0.4 -- E(3,8) twice, or five or
    // seven onsets off the quarters -- or displaced with p 0.3.
    {
        LayerIdE candidates[] = { LayerIdE::TOM_CONGA, LayerIdE::RIM, LayerIdE::SHAKER };
        for ( int i = 2; i > 0; --i ) std::swap( candidates[i], candidates[rng.below( i + 1 )] );
        static const int Periods[] = { 3, 5, 6, 7, 12 };
        const int        polys = rng.uniform() < st.m_polymeterChance ? ( rng.uniform() < 0.5f ? 2 : 1 ) : 0;
        for ( int c = 0; c < 3; ++c )
        {
            const int li = static_cast<int>( candidates[c] );
            if ( c < polys )
            {
                const int period = Periods[rng.below( 5 )];
                const int k = std::max( 1, static_cast<int>( std::lround( period / 3.0 ) ) );
                plan.m_period[li] = period;
                plan.m_resetBars[li] = 16;
                plan.m_cycle[li] = rotateMask( euclidMask( k, period ), period, rng.below( period ) );
            }
            else if ( rng.uniform() < 0.4f )
            {
                const int kind = rng.below( 3 );
                if ( kind == 0 )
                {
                    // E(3,8) twice in a bar, at a rotation that keeps every onset off the quarters, searched from a
                    // random start.
                    const uint64_t base = euclidMask( 3, 8 ) | ( euclidMask( 3, 8 ) << 8 );
                    const int      start = rng.below( 16 );
                    for ( int r = 0; r < 16; ++r )
                    {
                        const uint64_t m = rotateMask( base, 16, start + r );
                        if ( ( m & 0x1111u ) == 0 )
                        {
                            plan.m_euclid[li] = static_cast<uint16_t>( m );
                            break;
                        }
                    }
                }
                else
                {
                    // Five or seven onsets: E(5,16) and E(7,16) have one on every place of the beat, no rotation
                    // keeps them off the quarters -- until 01.10.2026 these two of the three draws fell back to the
                    // matrix unseen (the rim's .4, .2, .2: under one hit a bar). E(5,12) and E(7,12) on the twelve
                    // sixteenths beside the quarters instead (one draw for the rotation, as before).
                    plan.m_euclid[li] = offQuarterMask( kind == 1 ? 5 : 7, rng.below( 12 ) );
                }
            }
            else if ( rng.uniform() < 0.3f )
            {
                plan.m_displace[li] = 1 + rng.below( 3 );
            }
        }
        if ( rng.uniform() < 0.3f ) plan.m_displace[static_cast<int>( LayerIdE::CLAP_GHOST )] = 1 + rng.below( 3 );
    }
    // Phase 3: the harmony (Dok. 8.6). The chord in close position from the root in 220 .. 415 Hz.
    {
        plan.m_chordRoot = 57 + ( ( plan.m_keyRoot - 9 + 12 ) % 12 );
        const float c = rng.uniform();
        const bool  noNinth = !inScale( plan.m_scale, 2 );   // Phrygian and the pentatonic
        if ( c < 0.5f )
        {
            plan.m_nChordTones = 3;
            plan.m_chordTones[0] = 0;
            plan.m_chordTones[1] = 3;
            plan.m_chordTones[2] = 7;
        }
        else if ( c < 0.8f || ( c < 0.9f && noNinth ) )
        {
            plan.m_nChordTones = 4;
            plan.m_chordTones[0] = 0;
            plan.m_chordTones[1] = 3;
            plan.m_chordTones[2] = 7;
            plan.m_chordTones[3] = 10;
        }
        else if ( c < 0.9f )
        {   // add9 (where the scale has no ninth, the seventh above)
            plan.m_nChordTones = 4;
            plan.m_chordTones[0] = 0;
            plan.m_chordTones[1] = 3;
            plan.m_chordTones[2] = 7;
            plan.m_chordTones[3] = 14;
        }
        else
        {   // the fourth in the bass
            plan.m_nChordTones = 4;
            plan.m_chordTones[0] = -7;
            plan.m_chordTones[1] = 0;
            plan.m_chordTones[2] = 3;
            plan.m_chordTones[3] = 7;
        }
        if ( rng.uniform() < 0.2f )
        {
            const float s = rng.uniform();
            int         shuttle = s < 0.3f ? 1 : ( s < 0.55f ? 10 : ( s < 0.8f ? 5 : 3 ) );   // bII, bVII, iv, bIII
            if ( !inScale( plan.m_scale, shuttle ) ) shuttle = 10;
            plan.m_shuttle = shuttle > 6 ? shuttle - 12 : shuttle;
            plan.m_shuttleBars = rng.uniform() < 0.5f ? 2 : 4;
            // A triad in the scale's thirds on the shuttle's degree: every second degree up (a quartal shape in the
            // pentatonic), so the second chord stays in the key and carries no major seventh.
            const ScaleDef &d = scaleDef( plan.m_scale );
            int             deg = 0;
            while ( deg < d.m_size && d.m_steps[deg] != shuttle ) ++deg;
            for ( int t = 0; t < 3; ++t )
            {
                const int k = deg + 2 * t;
                plan.m_shuttleTones[t] =
                    plan.m_shuttle + d.m_steps[k % d.m_size] + 12 * ( k / d.m_size ) - d.m_steps[deg];
            }
        }
        plan.m_droneNote = plan.m_chordRoot + ( rng.uniform() < 0.7f ? 0 : 7 );
        plan.m_acidRoot = plan.m_bassRoot + 12;
        if ( plan.m_acidRoot < 45 )
            plan.m_acidRoot += 12;   // from A2 on: the 303 speaks through its harmonics, over the sub
    }
    // Trig conditions (Dok. 8.2): an extra open hat in the second of four bars, a single shaker in the third.
    plan.m_conds[plan.m_nConds++] = TrigCond{ LayerIdE::OPEN_HAT, rng.uniform() < 0.5f ? 14 : 10, 2, 4 };
    plan.m_conds[plan.m_nConds++] = TrigCond{ LayerIdE::SHAKER, rng.uniform() < 0.5f ? 13 : 5, 3, 4 };
    return plan;
}

void makeFigure( RackPlan &plan, LayerIdE voice, uint64_t seed )
{
    plan.m_figure = voice;
    plan.m_figBars = 1;
    plan.m_figOn = plan.m_figAccent = plan.m_figSlide = 0;
    plan.m_figLength = 0.5;
    for ( int8_t &p : plan.m_figPitch ) p = 0;
    if ( voice == LayerIdE::COUNT ) return;
    Rng rng;
    rng.seed( mixSeed( seed, 0x464947555245ull ) );   // "FIGURE"
    const auto weighted = [&]( const float *w, int n )
    {
        float sum = 0.0f;
        for ( int i = 0; i < n; ++i ) sum += w[i];
        float u = rng.uniform() * sum;
        for ( int i = 0; i < n; ++i )
        {
            if ( u < w[i] ) return i;
            u -= w[i];
        }
        return n - 1;
    };
    const auto set = []( uint32_t &mask, int pos, bool on )
    {
        if ( on )
            mask |= 1u << pos;
        else
            mask &= ~( 1u << pos );
    };
    const auto has = []( uint32_t mask, int pos ) { return ( ( mask >> pos ) & 1u ) != 0; };
    const auto inKey = [&]( int iv ) { return inScale( plan.m_scale, ( ( iv % 12 ) + 12 ) % 12 ); };

    switch ( voice )
    {
    case LayerIdE::ACID:
    {
        // A 303 line on the off-quarter sixteenths (the quarters are the kick's): its alphabet, accents, slides.
        plan.m_figBars = rng.uniform() < 0.45f ? 1 : 2;
        const float density = 0.5f + 0.25f * rng.uniform();
        const auto  pitch = [&]()
        {
            const float u = rng.uniform();
            const int   iv = u < 0.45f ? 0 : u < 0.65f ? 12 : u < 0.75f ? 7 : u < 0.85f ? 10 : u < 0.95f ? 3 : 5;
            return inKey( iv ) ? iv : 0;
        };
        const auto rollPlace = [&]( int pos )
        {
            set( plan.m_figOn, pos, rng.uniform() < density );
            plan.m_figPitch[pos] = static_cast<int8_t>( pitch() );
            set( plan.m_figAccent, pos, rng.uniform() < 0.3f );
        };
        for ( int s = 0; s < Steps; ++s )
            if ( s % 4 != 0 ) rollPlace( s );
        for ( int tries = 0; std::popcount( plan.m_figOn & 0xFFFFu ) < 5 && tries < 64; ++tries )
        {
            const int s = 1 + rng.below( 15 );
            if ( s % 4 != 0 ) set( plan.m_figOn, s, true );
        }
        if ( plan.m_figBars == 2 )
        {
            // The answer: the first bar again, two to four places rolled anew.
            for ( int s = 0; s < Steps; ++s )
            {
                set( plan.m_figOn, 16 + s, has( plan.m_figOn, s ) );
                set( plan.m_figAccent, 16 + s, has( plan.m_figAccent, s ) );
                plan.m_figPitch[16 + s] = plan.m_figPitch[s];
            }
            const int changes = 2 + rng.below( 3 );
            for ( int c = 0; c < changes; ++c )
            {
                int s = 1 + rng.below( 15 );
                if ( s % 4 == 0 ) ++s;
                rollPlace( 16 + s );
            }
        }
        // The line starts from the root, wherever its first sixteenth falls.
        for ( int pos = 0; pos < 16; ++pos )
            if ( has( plan.m_figOn, pos ) )
            {
                plan.m_figPitch[pos] = 0;
                break;
            }
        // Slides into a sounding sixteenth (the gate holds over).
        const int places = plan.m_figBars * Steps;
        for ( int pos = 0; pos < places; ++pos )
        {
            const int next = ( pos + 1 ) % places;
            if ( has( plan.m_figOn, pos ) && has( plan.m_figOn, next ) )
                set( plan.m_figSlide, pos, rng.uniform() < 0.2f );
        }
        break;
    }
    case LayerIdE::CHORD:
    {
        // The stab's rhythm (the steps of one or two bars) and its length in beats.
        struct Rhythm
        {
            float    m_w;
            int      m_bars;
            uint32_t m_on;
            double   m_length;
        };
        static const Rhythm Rhythms[] = {
            { 0.20f, 1, ( 1u << 2 ) | ( 1u << 6 ) | ( 1u << 10 ) | ( 1u << 14 ), 0.5 },   // the offbeat eighths
            { 0.15f, 1, ( 1u << 0 ) | ( 1u << 10 ), 0.5 },                                // Dok. 8.2's stab
            { 0.15f, 1, ( 1u << 0 ) | ( 1u << 3 ) | ( 1u << 6 ) | ( 1u << 8 ) | ( 1u << 11 ) | ( 1u << 14 ),
              0.4 },                                         // 3-3-2 twice
            { 0.10f, 1, ( 1u << 3 ) | ( 1u << 10 ), 0.5 },   // a late pair
            { 0.15f, 1, ( 1u << 0 ), 3.0 },                  // one long chord
            { 0.15f, 2, ( 1u << 0 ) | ( 1u << 6 ) | ( 1u << 10 ) | ( 1u << 19 ) | ( 1u << 26 ), 0.5 },   // call, answer
            { 0.10f, 1, 0, 0.4 },                                                                        // E(5,16)
        };
        float w[7];
        for ( int i = 0; i < 7; ++i ) w[i] = Rhythms[i].m_w;
        const Rhythm &r = Rhythms[weighted( w, 7 )];
        plan.m_figBars = r.m_bars;
        plan.m_figLength = r.m_length;
        plan.m_figOn = r.m_on;
        if ( plan.m_figOn == 0 )
        {
            // E(5,16) rotated off the downbeat, from a random start.
            const uint64_t e = euclidMask( 5, 16 );
            const int      start = 1 + rng.below( 15 );
            for ( int k = 0; k < 16 && plan.m_figOn == 0; ++k )
            {
                const uint64_t m = rotateMask( e, 16, start + k );
                if ( ( m & 1u ) == 0 ) plan.m_figOn = static_cast<uint32_t>( m );
            }
        }
        for ( int pos = 0; pos < plan.m_figBars * Steps; ++pos )
            if ( has( plan.m_figOn, pos ) )
            {
                set( plan.m_figAccent, pos, true );
                break;
            }
        break;
    }
    case LayerIdE::BASS:
    {
        struct Riff
        {
            float    m_w;
            uint32_t m_on;
        };
        static const Riff Riffs[] = {
            { 0.30f, ( 1u << 2 ) | ( 1u << 6 ) | ( 1u << 10 ) | ( 1u << 14 ) },   // offbeat
            { 0.20f, ( 1u << 2 ) | ( 1u << 3 ) | ( 1u << 6 ) | ( 1u << 7 ) | ( 1u << 10 ) | ( 1u << 11 ) |
                         ( 1u << 14 ) | ( 1u << 15 ) },                           // gallop
            { 0.20f, 0xEEEEu },                                                   // rolling
            { 0.15f, ( 1u << 3 ) | ( 1u << 6 ) | ( 1u << 10 ) | ( 1u << 13 ) },   // syncopated
            { 0.15f, ( 1u << 2 ) | ( 1u << 7 ) | ( 1u << 10 ) },                  // sparse
        };
        float w[5];
        for ( int i = 0; i < 5; ++i ) w[i] = Riffs[i].m_w;
        const uint32_t riff = Riffs[weighted( w, 5 )].m_on;
        plan.m_figBars = rng.uniform() < 0.3f ? 2 : 1;
        plan.m_figOn = riff | ( plan.m_figBars == 2 ? riff << 16 : 0u );
        std::vector<int> onsets;
        for ( int pos = 0; pos < plan.m_figBars * Steps; ++pos )
            if ( has( plan.m_figOn, pos ) ) onsets.push_back( pos );
        // The octave on one or two places, the alphabet's second tone on the motif's last onset.
        for ( int k = 0; k < 2; ++k )
            if ( rng.uniform() < 0.3f )
                plan.m_figPitch[onsets[static_cast<size_t>( rng.below( static_cast<int>( onsets.size() ) ) )]] = 12;
        // (The alphabet's b2 and b5 only where the scale has them; else the fifth.)
        if ( plan.m_bassSize > 1 )
            plan.m_figPitch[onsets.back()] = static_cast<int8_t>( inKey( plan.m_bassSet[1] ) ? plan.m_bassSet[1] : 7 );
        for ( int pos : onsets )
            if ( pos % Steps == 2 || pos % Steps == 3 ) set( plan.m_figAccent, pos, true );
        break;
    }
    case LayerIdE::PING:
    {
        const int   li = static_cast<int>( LayerIdE::PING );
        const float u = rng.uniform();
        const int   period = u < 0.35f ? 16 : u < 0.55f ? 12 : u < 0.8f ? 7 : 5;
        const int   k = period == 16 ? 5 + rng.below( 3 ) : period == 12 ? 4 + rng.below( 2 ) : period == 7 ? 3 : 2;
        plan.m_period[li] = period;
        plan.m_resetBars[li] = 16;
        plan.m_cycle[li] = rotateMask( euclidMask( k, period ), period, rng.below( period ) );
        static const int   Second[] = { 7, 10, 12, 3, 5 };
        static const float SecondW[] = { 0.35f, 0.25f, 0.20f, 0.10f, 0.10f };
        int                second = Second[weighted( SecondW, 5 )];
        if ( !inKey( second ) ) second = 7;
        static const int Third[] = { 3, 5, 7, 10, 12, 15 };
        int              third = Third[rng.below( 6 )];
        if ( third == second || !inKey( third ) ) third = second == 12 ? 7 : 12;
        bool first = true;
        for ( int pos = 0; pos < period; ++pos )
        {
            const float v = rng.uniform();
            const bool  onset = ( ( plan.m_cycle[li] >> pos ) & 1u ) != 0;
            const int   iv = ( onset && first ) ? 0 : v < 0.45f ? 0 : v < 0.8f ? second : third;
            if ( onset ) first = false;
            plan.m_pingNote[pos] = plan.m_pingRoot + iv;
        }
        break;
    }
    default: plan.m_figure = LayerIdE::COUNT; break;
    }
}

BarSpec emptyBar( int bar, double beat, double bpm )
{
    BarSpec s;
    s.m_bar = bar;
    s.m_block = bar / 32;
    s.m_barInBlock = bar % 32;
    s.m_beat = beat;
    s.m_bpm = bpm;
    for ( int i = 0; i < NumLayers; ++i )
    {
        s.m_active[i] = false;
        s.m_density[i] = 1.0f;
    }
    return s;
}

void rollSteps( const RackPlan &plan, LayerIdE id, int bar, int block, float density, bool *on, const bool *busy,
                uint32_t variant )
{
    const int       li = static_cast<int>( id );
    const uint64_t  sd = seedOf( plan, li, variant );
    const LayerDef &d = Layers[li];
    for ( int s = 0; s < Steps; ++s ) on[s] = false;
    if ( density <= 0.0f ) return;

    // Phase 8: the figure plays its motif, the same in every pass (the ping's figure is its cycle, below).
    if ( id == plan.m_figure && id != LayerIdE::PING && plan.m_figOn != 0 )
    {
        const int base = ( bar % std::max( 1, plan.m_figBars ) ) * Steps;
        for ( int s = 0; s < Steps; ++s ) on[s] = ( ( plan.m_figOn >> ( base + s ) ) & 1u ) != 0;
        return;
    }
    if ( isCyclic( plan, li ) )
    {
        // The cycle runs against the bar from its last reset; density thins it the same way in every pass.
        for ( int s = 0; s < Steps; ++s )
        {
            const int pos = cyclePos( plan, li, bar, s );
            if ( !( plan.m_cycle[li] & ( uint64_t( 1 ) << pos ) ) ) continue;
            on[s] = density >= 1.0f || roll( sd, li, CYCLE, static_cast<uint64_t>( block ) * 64 + pos ) < density;
        }
        return;
    }
    if ( plan.m_euclid[li] != 0 )
    {
        for ( int s = 0; s < Steps; ++s )
        {
            if ( !( plan.m_euclid[li] & ( 1u << s ) ) ) continue;
            on[s] = density >= 1.0f || roll( sd, li, BASE, static_cast<uint64_t>( block ) * 16 + s ) < density;
        }
    }
    else
    {
        bool prev1 = false, prev2 = false;
        for ( int s = 0; s < Steps; ++s )
        {
            const float p = d.m_p[s];
            if ( p <= 0.0f )
            {
                prev2 = prev1;
                prev1 = false;
                continue;
            }
            if ( p >= 1.0f )
            {
                on[s] = true;
                prev2 = prev1;
                prev1 = true;
                continue;
            }
            float u, q = p * std::min( density, 1.0f );
            switch ( d.m_kind )
            {
            case LayerKindE::ANCHOR: u = roll( sd, li, BASE, static_cast<uint64_t>( block ) * 16 + s ); break;
            case LayerKindE::LOOP:
            {
                // Bar 1 (and 3) of the loop is the base roll; bars 2 and 4 re-roll the style's share of the open steps
                // (RackPlan::mutation; Dok. 8.2's "Mutation nur in Takt 2 bzw. 2/4").
                const int pos = ( bar % 32 ) % std::max( 1, plan.m_loopBars[li] );
                u = roll( sd, li, BASE, static_cast<uint64_t>( block ) * 16 + s );
                if ( pos == 1 || pos == 3 )
                {
                    const uint64_t salt = pos == 1 ? MUT_A : MUT_B;
                    if ( roll( sd, li, salt, static_cast<uint64_t>( block ) * 64 + s ) < plan.m_mutation )
                        u = roll( sd, li, salt, static_cast<uint64_t>( block ) * 64 + 16 + s );
                }
                break;
            }
            default:
                // The ghost chain (Erg. 7): the layer's own last two steps, the others' ghosts, the half bar before. A step
                // is rolled anew this bar with the style's reroll share, else it keeps its roll of the block.
                u = roll( sd, li, MOTION, static_cast<uint64_t>( bar ) * 16 + s );
                if ( plan.m_reroll < 1.0f &&
                     roll( sd, li, MUT_B, static_cast<uint64_t>( bar ) * 16 + s ) >= plan.m_reroll )
                    u = roll( sd, li, MOTION, 0x100000000ull + static_cast<uint64_t>( block ) * 16 + s );
                q *= plan.m_motionScale;
                if ( prev1 )
                    q *= 0.5f;
                else if ( prev2 )
                    q *= 0.75f;
                if ( busy != nullptr && busy[s] ) q *= 0.5f;
                if ( s >= 8 && d.m_p[s - 8] > 0.0f && !on[s - 8] ) q = std::min( 1.0f, q * 1.5f );
                break;
            }
            on[s] = u < q;
            prev2 = prev1;
            prev1 = on[s];
        }
        // Phase 10: a loop that plays is heard -- a loop bar its rolls left empty keeps its likeliest step (the rim's
        // three chances of .4, .2 and .2 left it silent for whole blocks, 28.09.2026).
        if ( d.m_kind == LayerKindE::LOOP && density >= 1.0f )
        {
            bool any = false;
            for ( int s = 0; s < Steps; ++s ) any = any || on[s];
            int best = -1;
            for ( int s = 0; s < Steps && !any; ++s )
                if ( d.m_p[s] > 0.0f && ( best < 0 || d.m_p[s] > d.m_p[best] ) ) best = s;
            if ( best >= 0 ) on[best] = true;
        }
    }
    if ( plan.m_displace[li] > 0 )
    {
        bool tmp[Steps];
        for ( int s = 0; s < Steps; ++s ) tmp[( s + plan.m_displace[li] ) % Steps] = on[s];
        for ( int s = 0; s < Steps; ++s ) on[s] = tmp[s];
    }
}

void realizeBar( const RackPlan &plan, const BarSpec &spec, std::vector<NoteEvent> &out )
{
    bool on[NumLayers][Steps] = {};
    auto L = []( LayerIdE id ) { return static_cast<int>( id ); };
    // The layers in order; the ghosts see what the ghosts and the percussion before them took (the chain's cross factor).
    bool busy[Steps] = {};
    for ( int i = 0; i < NumLayers; ++i )
    {
        const LayerIdE id = static_cast<LayerIdE>( i );
        if ( id == LayerIdE::CLAP_B || !spec.m_active[i] ) continue;
        const LayerIdE  table = ( id == LayerIdE::CLAP_A && plan.m_clapB ) ? LayerIdE::CLAP_B : id;
        const LayerDef &d = Layers[static_cast<int>( table )];
        const bool      motion = d.m_kind == LayerKindE::MOTION;
        rollSteps( plan, table, spec.m_bar, spec.m_block, spec.m_density[i], on[i], motion ? busy : nullptr,
                   spec.m_variant );
        if ( motion || d.m_perc )
            for ( int s = 0; s < Steps; ++s ) busy[s] = busy[s] || on[i][s];
    }
    // Trig conditions.
    for ( int c = 0; c < plan.m_nConds; ++c )
    {
        const TrigCond &t = plan.m_conds[c];
        if ( spec.m_active[L( t.m_layer )] && ( spec.m_bar % t.m_b ) == t.m_a - 1 ) on[L( t.m_layer )][t.m_step] = true;
    }
    // The rules. Gap reservation: the quarters belong to the kick, the clap and the rim; a cyclic layer crosses them.
    for ( int i = 0; i < NumLayers; ++i )
    {
        const LayerIdE id = static_cast<LayerIdE>( i );
        if ( id == LayerIdE::KICK || id == LayerIdE::CLAP_A || id == LayerIdE::RIM || isCyclic( plan, i ) ) continue;
        // The tonal layers that are no bass may sit on the quarters: the chord's stab on the downbeat, the drone, the texture.
        if ( id == LayerIdE::CHORD || id == LayerIdE::DRONE || id == LayerIdE::TEXTURE ) continue;
        for ( int s = 0; s < Steps; s += 4 ) on[i][s] = false;
    }
    // The drone speaks every eighth bar.
    if ( ( spec.m_bar % 8 ) != 0 )
        for ( int s = 0; s < Steps; ++s ) on[L( LayerIdE::DRONE )][s] = false;
    // One hat per step.
    for ( int s = 0; s < Steps; ++s )
    {
        if ( on[L( LayerIdE::OPEN_HAT )][s] )
        {
            on[L( LayerIdE::CLOSED_HAT )][s] = false;
            on[L( LayerIdE::ROLLING_HAT )][s] = false;
        }
        if ( on[L( LayerIdE::CLOSED_HAT )][s] ) on[L( LayerIdE::ROLLING_HAT )][s] = false;
    }
    // A fill: the tom or conga rolls through the sixteenths after the last quarter of an eight-bar phrase (p fillChance;
    // the quarter stays the kick's), never a snare roll.
    bool fill = false;
    if ( spec.m_fills && spec.m_active[L( LayerIdE::TOM_CONGA )] && ( spec.m_bar % 8 ) == 7 &&
         roll( seedOf( plan, L( LayerIdE::TOM_CONGA ), spec.m_variant ), L( LayerIdE::TOM_CONGA ), FILL,
               static_cast<uint64_t>( spec.m_bar ) ) < plan.m_fillChance )
    {
        fill = true;
        for ( int s = 13; s < Steps; ++s ) on[L( LayerIdE::TOM_CONGA )][s] = true;
    }
    // "Mute one expected hit": everything but the kick falls silent on the marked steps.
    for ( int s = 0; s < Steps; ++s )
    {
        if ( !spec.m_muteStep[s] ) continue;
        for ( int i = 0; i < NumLayers; ++i )
            if ( i != L( LayerIdE::KICK ) ) on[i][s] = false;
    }

    const double msToBeats = spec.m_bpm / 60000.0;
    const double swingBeats =
        ( plan.m_swing - 50.0 ) / 100.0 * 0.5;   // Linn: the even sixteenth late by (S - 50) % of an eighth
    for ( int i = 0; i < NumLayers; ++i )
    {
        const LayerIdE  id = static_cast<LayerIdE>( i );
        const LayerIdE  table = ( id == LayerIdE::CLAP_A && plan.m_clapB ) ? LayerIdE::CLAP_B : id;
        const LayerDef &d = Layers[static_cast<int>( table )];
        const bool      kickLike = id == LayerIdE::KICK || id == LayerIdE::GHOST_KICK;
        const bool      pattern = isCyclic( plan, i ) || plan.m_euclid[i] != 0;
        // Phase 18: the layers no rule ties to the kit (kick, bass, ping, chord, 303, drone) draw their spread and jitter
        // apart from the block's candidate, so a kit layer drawn again leaves them bit for bit (it may pick another
        // candidate). Every track has such a voice now.
        const bool     untied = id == LayerIdE::KICK || id == LayerIdE::BASS || id == LayerIdE::PING ||
                                id == LayerIdE::CHORD || id == LayerIdE::ACID || id == LayerIdE::DRONE;
        const uint32_t variant = untied ? 0u : spec.m_variant;
        for ( int s = 0; s < Steps; ++s )
        {
            if ( !on[i][s] ) continue;
            NoteEvent n;
            // Velocity: the step's (a cyclic or Euclidean layer: its loudest step's), a random spread, the collision dip.
            const float base = pattern || d.m_vel[s] <= 0.0f ? baseVelocity( d ) : d.m_vel[s];
            float       vel = base + d.m_velRandom * ( 2.0f * roll( seedOf( plan, i, variant ), i, VEL,
                                                                    static_cast<uint64_t>( spec.m_bar ) * 16 + s ) -
                                                       1.0f );
            double      offMs = plan.m_offsetMs[i];
            const bool  kickHere = on[L( LayerIdE::KICK )][s] || on[L( LayerIdE::GHOST_KICK )][s];
            const bool  hatHere =
                on[L( LayerIdE::CLOSED_HAT )][s] || on[L( LayerIdE::ROLLING_HAT )][s] || on[L( LayerIdE::OPEN_HAT )][s];
            if ( d.m_perc && ( kickHere || hatHere ) )
            {
                if ( id == LayerIdE::RIM && kickHere )
                    offMs = -4.0;   // the sidestick 4 ms before the kick (Dok. 8.3)
                else
                    vel *= 0.75f;
            }
            if ( id == LayerIdE::CLOSED_HAT ) vel = std::min( vel, V( 89 ) );
            if ( fill && id == LayerIdE::TOM_CONGA && s >= 13 )
            {
                vel = 0.6f + 0.12f * static_cast<float>( s - 13 );
                n.m_shift = 2 * ( s - 12 );
            }
            // Timing: swing on the even sixteenths, the feel offset, a late jitter (never on the kick).
            double beat = spec.m_beat + 0.25 * s;
            if ( ( s & 1 ) == 1 ) beat += swingBeats * d.m_swing;
            if ( !kickLike && plan.m_humanizeMs > 0.0f )
            {
                // Half-normal from two uniforms (Box-Muller's radius), never early.
                const double u1 =
                    std::max( 1.0e-9, static_cast<double>( roll( seedOf( plan, i, variant ), i, JITTER,
                                                                 static_cast<uint64_t>( spec.m_bar ) * 32 + s ) ) );
                const double u2 =
                    roll( seedOf( plan, i, variant ), i, JITTER, static_cast<uint64_t>( spec.m_bar ) * 32 + 16 + s );
                const double g = std::sqrt( -2.0 * std::log( u1 ) ) * std::cos( 6.283185307179586 * u2 );
                offMs += std::min( 10.0, std::fabs( g ) * plan.m_humanizeMs );
            }
            if ( !kickLike ) beat += offMs * msToBeats;
            n.m_beat = beat;
            n.m_length = d.m_length;
            // Phase 8: the figure's place in its motif; its accents lift the velocity.
            const bool figure = id == plan.m_figure && id != LayerIdE::PING && plan.m_figOn != 0;
            const int  fig = ( spec.m_bar % std::max( 1, plan.m_figBars ) ) * Steps + s;
            if ( figure )
                vel = baseVelocity( d ) * ( ( ( plan.m_figAccent >> fig ) & 1u ) ? 1.12f : 0.9f ) +
                      0.5f * d.m_velRandom *
                          ( 2.0f * roll( seedOf( plan, i, 0 ), i, VEL, static_cast<uint64_t>( spec.m_bar ) * 16 + s ) -
                            1.0f );
            n.m_velocity = clampv( vel, 0.05f, 1.0f );
            if ( figure && id == LayerIdE::BASS )
            {
                n.m_part = PartE::BASS;
                n.m_pitch = plan.m_bassRoot + plan.m_figPitch[fig];
            }
            else if ( figure && id == LayerIdE::ACID )
            {
                n.m_part = PartE::ACID;
                int iv = plan.m_figPitch[fig];
                if ( !inScale( plan.m_scale, iv % 12 ) || !( plan.m_acidMask & ( 1u << ( iv % 12 ) ) ) )
                    iv = iv >= 12 ? 12 : 0;
                n.m_pitch = plan.m_acidRoot + iv;
                n.m_accent = ( ( plan.m_figAccent >> fig ) & 1u ) != 0;
                n.m_slide = ( ( plan.m_figSlide >> fig ) & 1u ) != 0;
                if ( n.m_slide ) n.m_length = 0.3;
            }
            else if ( figure && id == LayerIdE::CHORD )
            {
                const bool other =
                    plan.m_shuttleBars > 0 && ( spec.m_bar % plan.m_shuttleBars ) == plan.m_shuttleBars - 1;
                const int nt = other ? 3 : plan.m_nChordTones;
                for ( int t = 0; t < nt; ++t )
                {
                    NoteEvent c = n;
                    c.m_part = PartE::CHORD;
                    c.m_length = plan.m_figLength;
                    c.m_pitch = plan.m_chordRoot + ( other ? plan.m_shuttleTones[t] : plan.m_chordTones[t] );
                    out.push_back( c );
                }
                continue;
            }
            else if ( kickLike )
            {
                n.m_part = PartE::KICK;
                n.m_pitch = 36;
            }
            else if ( id == LayerIdE::BASS )
            {
                n.m_part = PartE::BASS;   // the synth; where the sub owns the low end the engine plays the sine too
                // Root ostinato; the alphabet's other tones only on the last two sixteenths (Dok. 8.6).
                int interval = 0;
                if ( plan.m_bassSize > 1 && s >= 14 )
                {
                    const int k =
                        1 + static_cast<int>( roll( seedOf( plan, i, spec.m_variant ), i, BASE,
                                                    1000 + static_cast<uint64_t>( spec.m_bar ) * 2 + ( s - 14 ) ) *
                                              static_cast<float>( plan.m_bassSize - 1 ) );
                    interval = plan.m_bassSet[std::min( k, plan.m_bassSize - 1 )];
                    if ( !inScale( plan.m_scale, interval % 12 ) )
                        interval = 7;   // (b2, b5 only in a scale that has them)
                }
                n.m_pitch = plan.m_bassRoot + interval;
            }
            else if ( id == LayerIdE::PING )
            {
                n.m_part = PartE::PING;
                n.m_pitch = plan.m_pingNote[cyclePos( plan, i, spec.m_bar, s )];
            }
            else if ( id == LayerIdE::CHORD )
            {
                // Every tone of the chord (or of the shuttle's, in its bars), each a note of its own.
                const bool other =
                    plan.m_shuttleBars > 0 && ( spec.m_bar % plan.m_shuttleBars ) == plan.m_shuttleBars - 1;
                const int nt = other ? 3 : plan.m_nChordTones;
                for ( int t = 0; t < nt; ++t )
                {
                    NoteEvent c = n;
                    c.m_part = PartE::CHORD;
                    c.m_pitch = plan.m_chordRoot + ( other ? plan.m_shuttleTones[t] : plan.m_chordTones[t] );
                    out.push_back( c );
                }
                continue;
            }
            else if ( id == LayerIdE::DRONE )
            {
                n.m_part = PartE::DRONE;
                n.m_pitch = plan.m_droneNote;
                n.m_length = 32.0;
            }
            else if ( id == LayerIdE::TEXTURE )
            {
                n.m_part = PartE::TEXTURE;
                n.m_pitch = 60;
                n.m_length = 4.0;
            }
            else if ( id == LayerIdE::ACID )
            {
                // Pitch, accent and slide of this step, rolled once per block: the root 0.45, its octave 0.2, the fifth,
                // the flat seventh and the minor third 0.1 each, within the scale (Dok. 8.6's alphabet, a 303 idiom).
                n.m_part = PartE::ACID;
                const uint64_t key = static_cast<uint64_t>( spec.m_block ) * 16 + s;
                const float    u = roll( seedOf( plan, i, spec.m_variant ), i, ACID, key );
                int            iv =
                    u < 0.45f ? 0 : ( u < 0.65f ? 12 : ( u < 0.75f ? 7 : ( u < 0.85f ? 10 : ( u < 0.95f ? 3 : 5 ) ) ) );
                if ( !inScale( plan.m_scale, iv ) || !( plan.m_acidMask & ( 1u << ( iv % 12 ) ) ) ) iv = 0;
                n.m_pitch = plan.m_acidRoot + iv;
                n.m_accent = roll( seedOf( plan, i, spec.m_variant ), i, ACID, 1000 + key ) < 0.3f;
                n.m_slide = roll( seedOf( plan, i, spec.m_variant ), i, ACID, 2000 + key ) < 0.2f;
                if ( n.m_slide ) n.m_length = 0.3;   // the gate holds into the next sixteenth
            }
            else
            {
                n.m_part = percPart( layerLane( plan, id ) );
                n.m_pitch = LayerNote[i];
            }
            out.push_back( n );
        }
    }
}

std::string miniNotation( const RackPlan &plan, LayerIdE id, int bar, const char *name )
{
    const int   li = static_cast<int>( id );
    std::string out;
    if ( isCyclic( plan, li ) )
    {
        out = "{";
        for ( int pos = 0; pos < plan.m_period[li]; ++pos )
        {
            if ( pos > 0 ) out += ' ';
            out += ( plan.m_cycle[li] & ( uint64_t( 1 ) << pos ) ) ? name : "~";
        }
        return out + "}%16";
    }
    bool on[Steps];
    rollSteps( plan, id, bar, bar / 32, 1.0f, on );
    for ( int s = 0; s < Steps; ++s )
    {
        if ( s > 0 ) out += ' ';
        out += on[s] ? name : "~";
    }
    return out;
}

}   // namespace tot
