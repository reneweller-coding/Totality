/**
 * @file Study.cpp
 * @brief The study: blocks, operations per style, the reduction, micro-automation, delay throws, the loudness mark.
 * @author Rene Weller
 */
#include "tot/compose/Study.h"
#include "tot/Dsp.h"
#include "tot/Leveler.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace tot
{

namespace
{

/**
 * @brief An automation curve from @p fromValue to @p toValue (real units) as offsets from the knob's position.
 * @param[in] p          the knobs
 * @param[in] id         the knob
 * @param[in] beat       where the ramp begins
 * @param[in] length     its length in beats
 * @param[in] fromValue  the value it starts at
 * @param[in] toValue    the value it ends at
 * @param[in] shape      the curve's shape
 * @param[in] hand       the hand that makes the move (0: none)
 * @return the gesture
 */
Gesture ramp( const ParamStore &p, int id, double beat, double length, float fromValue, float toValue,
              GestureShapeE shape = GestureShapeE::LINEAR, uint8_t hand = 0 )
{
    const float knob = p.toNormalised( id, p.get( id ) );
    Gesture     g;
    g.m_param = id;
    g.m_beat = beat;
    g.m_length = length;
    g.m_from = p.toNormalised( id, fromValue ) - knob;
    g.m_to = p.toNormalised( id, toValue ) - knob;
    g.m_shape = shape;
    g.m_hand = hand;
    return g;
}

/**
 * @brief Back to the knob at @p beat.
 * @param[in] id    the knob
 * @param[in] beat  where it returns
 * @return the gesture
 */
Gesture home( int id, double beat )
{
    Gesture g;
    g.m_param = id;
    g.m_beat = beat;
    g.m_length = 0.0;
    g.m_shape = GestureShapeE::STEP;
    return g;
}

/**
 * @brief The index of layer @p id.
 * @param[in] id  the layer
 * @return its index
 */
int L( LayerIdE id )
{
    return static_cast<int>( id );
}

/**
 * @brief Whether a layer is tonal (none in the first and last 32 bars, Dok. 8.5).
 * @param[in] id  the layer
 * @return true when it is tonal
 */
bool tonal( LayerIdE id )
{
    return id == LayerIdE::BASS || id == LayerIdE::ACID || id == LayerIdE::CHORD || id == LayerIdE::DRONE ||
           id == LayerIdE::TEXTURE || id == LayerIdE::PING;
}

/**
 * @brief Accepts every layer.
 * @return true
 */
bool anyLayer( LayerIdE )
{
    return true;
}

}   // namespace

Score composeStudy( const ParamStore &p, uint64_t seed )
{
    const float     bpm = p.get( p.id( ModuleE::COMPOSE, 0, compose::BPM ) );
    const float     minutes = p.get( p.id( ModuleE::COMPOSE, 0, compose::MINUTES ) );
    const LowOwnerE owner = static_cast<LowOwnerE>( p.getInt( p.id( ModuleE::COMPOSE, 0, compose::LOW_OWNER ) ) );
    const bool      subOwns = owner == LowOwnerE::SUB;
    const StyleE    style = static_cast<StyleE>( p.getInt( p.id( ModuleE::COMPOSE, 0, compose::STYLE ) ) );

    Score sc;
    sc.clear( bpm );
    sc.m_seed = seed;
    sc.m_keyRoot = p.getInt( p.id( ModuleE::COMPOSE, 0, compose::KEY ) );
    sc.m_scale = p.getInt( p.id( ModuleE::COMPOSE, 0, compose::SCALE ) );

    const int blocks = std::max( 4, static_cast<int>( std::lround( minutes * bpm / 4.0 / 32.0 ) ) );
    const int bars = blocks * 32;
    const int outroBlocks = blocks >= 7 ? 2 : 1;
    const int bodyFirst = 1, bodyLast = blocks - 1 - outroBlocks;
    const int bodyCount = bodyLast - bodyFirst + 1;
    const int reductionBlock = bodyFirst + static_cast<int>( std::lround( 0.55 * ( bodyCount - 1 ) ) );

    const RackPlan plan = makeRackPlan( p, mixSeed( seed, 1 ) );
    Rng            rng;
    rng.seed( mixSeed( seed, 2 ) );
    auto shuffled = [&rng]( std::vector<LayerIdE> v )
    {
        for ( int i = static_cast<int>( v.size() ) - 1; i > 0; --i )
            std::swap( v[static_cast<size_t>( i )], v[static_cast<size_t>( rng.below( i + 1 ) )] );
        return v;
    };
    auto append = []( std::vector<LayerIdE> &to, const std::vector<LayerIdE> &from )
    { to.insert( to.end(), from.begin(), from.end() ); };

    // The layers a body block may add, in order, per style (PLAN 2.8; Dok. 8.5's order: hats and ride, clap and perc,
    // bass, stab, pad and texture -- each style bringing its signature early).
    std::vector<LayerIdE> queue;
    switch ( style )
    {
    case StyleE::HYPNOTIC:   // the ping's cyclic figure, a drone; clap and chord seldom
        queue = { LayerIdE::OPEN_HAT, LayerIdE::PING, LayerIdE::DRONE, LayerIdE::ROLLING_HAT };
        append( queue, shuffled( { LayerIdE::SHAKER, LayerIdE::TOM_CONGA, LayerIdE::RIM, LayerIdE::RIDE } ) );
        queue.push_back( LayerIdE::TEXTURE );
        if ( rng.uniform() < 0.3f ) queue.push_back( LayerIdE::CLAP_A );
        if ( rng.uniform() < 0.2f ) queue.push_back( LayerIdE::CHORD );
        break;
    case StyleE::DUB:   // the chord with its full chain first; the hats stay reduced (the rolling hat at half)
        queue = { LayerIdE::CHORD, LayerIdE::OPEN_HAT, LayerIdE::TEXTURE };
        append( queue, shuffled( { LayerIdE::SHAKER, LayerIdE::RIM } ) );
        if ( rng.uniform() < 0.3f ) queue.push_back( LayerIdE::DRONE );
        break;
    case StyleE::RAW_PEAK:   // dense: sixteenth hats, toms, clap; the 303 now and then
        queue = { LayerIdE::OPEN_HAT, LayerIdE::ROLLING_HAT, LayerIdE::CLAP_A, LayerIdE::TOM_CONGA };
        if ( rng.uniform() < 0.4f ) queue.push_back( LayerIdE::ACID );
        append( queue, shuffled( { LayerIdE::RIM, LayerIdE::SHAKER, LayerIdE::RIDE } ) );
        break;
    default:   // Ostgut: clap on 2 and 4, the ride's eighths; stabs sparingly, a 303 seldom
        queue = { LayerIdE::OPEN_HAT, LayerIdE::CLAP_A, LayerIdE::ROLLING_HAT, LayerIdE::RIDE };
        if ( rng.uniform() < 0.5f )
            queue.push_back( LayerIdE::CHORD );
        else if ( rng.uniform() < 0.5f )
            queue.push_back( LayerIdE::ACID );
        append( queue, shuffled( { LayerIdE::SHAKER, LayerIdE::TOM_CONGA, LayerIdE::RIM } ) );
        break;
    }

    bool  active[NumLayers] = {};
    float density[NumLayers];
    for ( float &d : density ) d = 1.0f;
    std::vector<LayerIdE> entered;   // in the order they came, for the outro's mirror

    const int hatsCut = p.id( ModuleE::MIX, 0, mix::HATS_CUT );
    const int rollDecay = p.id( ModuleE::PERC, 1, perc::NOISE_DECAY );
    const int ohDecay = p.id( ModuleE::PERC, 2, perc::NOISE_DECAY );
    const int noiseLevel = p.id( ModuleE::PERC, 11, perc::LEVEL );
    const int noiseCut = p.id( ModuleE::PERC, 11, perc::CUTOFF );
    const int rumbleDecay = p.id( ModuleE::RUMBLE, 0, rumble::DECAY );
    const int chordLevel = p.id( ModuleE::CHORD, 0, chord::LEVEL );
    const int chordBand = p.id( ModuleE::CHORD, 0, chord::BAND );
    const int chordDub = p.id( ModuleE::CHORD, 0, chord::DUB_SEND );
    const int acidCut = p.id( ModuleE::ACID, 0, synth::CUTOFF );
    const int feedback = p.id( ModuleE::DUB, 0, dub::FEEDBACK );
    const int pingEcho = p.id( ModuleE::DUB, 0, dub::PING_SEND );
    const int hatsEcho = p.id( ModuleE::DUB, 0, dub::HATS_SEND );
    const int cloudLevel = p.id( ModuleE::CLOUD, 0, cloud::LEVEL );
    float     band = p.get( chordBand ), acid = p.get( acidCut );

    auto beatOf = []( int bar ) { return 4.0 * bar; };
    auto op = [&]( int bar, OpKindE kind, LayerIdE layer, LayerIdE other = LayerIdE::COUNT )
    {
        BlockOp o;
        o.m_beat = beatOf( bar );
        o.m_kind = kind;
        o.m_layer = layer == LayerIdE::COUNT ? -1 : L( layer );
        o.m_other = other == LayerIdE::COUNT ? -1 : L( other );
        sc.m_ops.push_back( o );
    };
    // What enters, and the curves that come with it.
    auto enter = [&]( int bar, LayerIdE add )
    {
        if ( add == LayerIdE::ROLLING_HAT )
            density[L( add )] = 1.0f;
        else
            active[L( add )] = true;
        entered.push_back( add );
        op( bar, OpKindE::ADD, add );
        if ( add == LayerIdE::OPEN_HAT )
            sc.m_gestures.push_back(
                ramp( p, ohDecay, beatOf( bar ), beatOf( 16 ), 100.0f, 400.0f, GestureShapeE::LINEAR, 1 ) );
        if ( add == LayerIdE::CHORD )
        {
            // "Nudging louder into the mix every few bars" (Dok. 5): 9 dB under the knob, a step up every four bars.
            const float k = p.get( chordLevel );
            for ( int s = 0; s < 4; ++s )
            {
                const float v = k - 9.0f + 3.0f * static_cast<float>( s );
                sc.m_gestures.push_back(
                    ramp( p, chordLevel, beatOf( bar + 4 * s ), 0.0, v, v, GestureShapeE::STEP, 0 ) );
            }
        }
    };
    // Removes the latest-entered active layer that @p pick accepts; false if there is none.
    auto leave = [&]( int bar, bool ( *pick )( LayerIdE ) )
    {
        for ( auto it = entered.rbegin(); it != entered.rend(); ++it )
        {
            const LayerIdE id = *it;
            if ( id == LayerIdE::KICK || id == LayerIdE::CLOSED_HAT || id == LayerIdE::ROLLING_HAT ||
                 !active[L( id )] || !pick( id ) )
                continue;
            active[L( id )] = false;
            op( bar, OpKindE::REMOVE, id );
            return true;
        }
        return false;
    };

    // Whole-track automation: the rumble's hall grows by a fifth (Dok. 8.5, "Track-Drift").
    {
        const float d0 = p.get( rumbleDecay );
        sc.m_gestures.push_back( ramp( p, rumbleDecay, 0.0, beatOf( bars ), d0, d0 * 1.2f, GestureShapeE::LINEAR, 1 ) );
    }
    // The intro: the hats' bus opens over bars 8 .. 32.
    sc.m_gestures.push_back(
        ramp( p, hatsCut, beatOf( 8 ), beatOf( 24 ), 1500.0f, 20000.0f, GestureShapeE::LINEAR, 0 ) );

    // The loudness mark: the track's loudest part is the block after the reduction (the densest), else the body's last.
    {
        const int peakBlock = reductionBlock + 1 <= bodyLast ? reductionBlock + 1 : bodyLast;
        LevelMark lm{ 0.0, beatOf( peakBlock * 32 ), styleTargetLufs( static_cast<int>( style ) ), 0.0f };
        lm.m_styleMix[static_cast<size_t>( std::clamp( static_cast<int>( style ), 0, 3 ) )] = 1.0f;
        sc.m_levels.push_back( lm );
    }

    const float throwChance = style == StyleE::DUB ? 0.6f : 0.3f;
    int         queuePos = 0;
    for ( int bar = 0; bar < bars; ++bar )
    {
        const int  block = bar / 32, inBlock = bar % 32;
        const bool outro = block >= blocks - outroBlocks;
        const bool body = block >= bodyFirst && !outro;
        // --- the form's decisions at the block's first bar ---
        if ( inBlock == 0 )
        {
            sc.m_markers.push_back( Marker{ beatOf( bar ), block == 0 ? std::string( "Intro" )
                                                           : outro    ? std::string( "Outro" )
                                                                      : "Block " + std::to_string( block + 1 ) } );
            if ( block == 0 )
            {
                active[L( LayerIdE::KICK )] = true;
                entered.push_back( LayerIdE::KICK );
                op( bar, OpKindE::START, LayerIdE::KICK );
            }
            else if ( block == bodyFirst )
            {
                enter( bar, subOwns ? LayerIdE::BASS : LayerIdE::GHOST_KICK );
            }
            else if ( block == reductionBlock )
            {
                op( bar, OpKindE::HOLD, LayerIdE::COUNT );
            }
            else if ( !outro )
            {
                if ( queuePos < static_cast<int>( queue.size() ) )
                    enter( bar, queue[static_cast<size_t>( queuePos++ )] );
                else
                    op( bar, OpKindE::HOLD, LayerIdE::COUNT );
            }
            else if ( block == blocks - 1 )
            {
                // The last block: kick and hats; the rolling hat thins, the hats' bus closes.
                bool any = false;
                for ( int i = 0; i < NumLayers; ++i )
                {
                    if ( i == L( LayerIdE::KICK ) || i == L( LayerIdE::CLOSED_HAT ) ||
                         i == L( LayerIdE::ROLLING_HAT ) || !active[i] )
                        continue;
                    if ( !any ) op( bar, OpKindE::REMOVE, static_cast<LayerIdE>( i ) );
                    active[i] = false;
                    any = true;
                }
                if ( !any ) op( bar, OpKindE::HOLD, LayerIdE::COUNT );
                density[L( LayerIdE::ROLLING_HAT )] = 0.5f;
                sc.m_gestures.push_back(
                    ramp( p, hatsCut, beatOf( bar + 16 ), beatOf( 16 ), 20000.0f, 2500.0f, GestureShapeE::LINEAR, 0 ) );
            }
        }
        // The outro's blocks before the last: the layers leave in the reverse order of their entry, the tonal ones first
        // (none in the last 32 bars, Dok. 8.5), one every eight bars.
        if ( outro && block < blocks - 1 && inBlock % 8 == 0 )
        {
            if ( !leave( bar, tonal ) && !leave( bar, anyLayer ) && inBlock == 0 )
                op( bar, OpKindE::HOLD, LayerIdE::COUNT );
        }
        // --- inside the intro ---
        if ( bar == 8 )
        {
            active[L( LayerIdE::CLOSED_HAT )] = true;
            entered.push_back( LayerIdE::CLOSED_HAT );
            op( bar, OpKindE::ADD, LayerIdE::CLOSED_HAT );
        }
        if ( bar == 16 )
        {
            active[L( LayerIdE::ROLLING_HAT )] = true;
            density[L( LayerIdE::ROLLING_HAT )] = 0.5f;
            op( bar, OpKindE::ADD, LayerIdE::ROLLING_HAT );
        }
        // --- the last 16 bars: kick and the offbeat hat ---
        if ( bar == bars - 16 )
        {
            active[L( LayerIdE::ROLLING_HAT )] = false;
            op( bar, OpKindE::REMOVE, LayerIdE::ROLLING_HAT );
        }
        // --- micro-events: every 16 bars in the body the rolling hat's decay moves a few per cent ---
        if ( inBlock % 16 == 0 && body )
        {
            const float d0 = p.get( rollDecay );
            const float from = d0 * ( 1.0f + 0.08f * ( 2.0f * rng.uniform() - 1.0f ) );
            const float to = d0 * ( 1.0f + 0.08f * ( 2.0f * rng.uniform() - 1.0f ) );
            sc.m_gestures.push_back(
                ramp( p, rollDecay, beatOf( bar ), beatOf( 16 ), from, to, GestureShapeE::MINIMUM_JERK, 1 ) );
        }
        // --- the meso level (Dok. 8.5's table): the stab's band wanders up to an octave either way of its knob over 16
        //     or 32 bars; the 303's cutoff makes its trips ---
        if ( inBlock % 16 == 0 && body && active[L( LayerIdE::CHORD )] && ( inBlock == 0 || rng.uniform() < 0.5f ) )
        {
            const float to = p.get( chordBand ) * std::pow( 2.0f, 2.0f * rng.uniform() - 1.0f );
            const int   len = inBlock == 0 && rng.uniform() < 0.5f ? 32 : 16;
            sc.m_gestures.push_back(
                ramp( p, chordBand, beatOf( bar ), beatOf( len ), band, to, GestureShapeE::MINIMUM_JERK, 1 ) );
            band = to;
        }
        if ( inBlock % 16 == 0 && body && active[L( LayerIdE::ACID )] )
        {
            const float to = 350.0f * std::pow( 2.0f, 2.5f * rng.uniform() );
            sc.m_gestures.push_back(
                ramp( p, acidCut, beatOf( bar ), beatOf( 16 ), acid, to, GestureShapeE::MINIMUM_JERK, 1 ) );
            acid = to;
        }
        // --- a delay throw at the end of a 16-bar phrase (Dok. 8.5, PLAN 7.4): from the third beat of its last bar a
        //     ping, a stab or the hats open into the echo, the feedback climbs to the edge and falls back over two bars ---
        if ( body && inBlock % 16 == 15 && rng.uniform() < throwChance )
        {
            const double at = beatOf( bar ) + 2.0;
            LayerIdE     from = LayerIdE::CLOSED_HAT;
            const float  u = rng.uniform();
            if ( active[L( LayerIdE::PING )] && u < 0.5f )
                from = LayerIdE::PING;
            else if ( active[L( LayerIdE::CHORD )] && u < 0.8f )
                from = LayerIdE::CHORD;
            const float fb = p.get( feedback );
            sc.m_gestures.push_back( ramp( p, feedback, at, 2.0, fb, 0.85f, GestureShapeE::EASE_IN, 0 ) );
            sc.m_gestures.push_back( ramp( p, feedback, at + 2.0, 8.0, 0.85f, fb, GestureShapeE::EASE_OUT, 0 ) );
            sc.m_gestures.push_back( home( feedback, at + 10.0 ) );
            const int   send = from == LayerIdE::PING ? pingEcho : from == LayerIdE::CHORD ? chordDub : hatsEcho;
            const float open = from == LayerIdE::CLOSED_HAT ? 0.5f : 0.9f;
            sc.m_gestures.push_back( ramp( p, send, at, 0.0, open, open, GestureShapeE::STEP, 1 ) );
            sc.m_gestures.push_back( home( send, at + 2.0 ) );
        }

        BarSpec spec = emptyBar( bar, beatOf( bar ), bpm );
        for ( int i = 0; i < NumLayers; ++i )
        {
            spec.m_active[i] = active[i];
            spec.m_density[i] = density[i];
        }
        // --- the reduction: kick out for bars 16 .. 23, a swell, a one-bar dropout, the return ---
        if ( block == reductionBlock )
        {
            if ( inBlock == 16 )
            {
                op( bar, OpKindE::KICK_OUT, LayerIdE::KICK );
                sc.m_markers.push_back( Marker{ beatOf( bar ), "Reduction" } );
                sc.m_gestures.push_back(
                    ramp( p, noiseLevel, beatOf( bar ), beatOf( 7 ), -36.0f, -12.0f, GestureShapeE::EASE_IN, 0 ) );
                sc.m_gestures.push_back(
                    ramp( p, noiseCut, beatOf( bar ), beatOf( 7 ), 600.0f, 6000.0f, GestureShapeE::EASE_IN, 1 ) );
                // The cloud of the ping's and the chord's last seconds rises through the kick's absence (PLAN 5.7).
                if ( active[L( LayerIdE::PING )] || active[L( LayerIdE::CHORD )] )
                    sc.m_gestures.push_back(
                        ramp( p, cloudLevel, beatOf( bar ), beatOf( 8 ), -60.0f, -3.0f, GestureShapeE::EASE_IN, 0 ) );
            }
            if ( inBlock >= 16 && inBlock < 24 )
            {
                spec.m_active[L( LayerIdE::KICK )] = false;
                spec.m_active[L( LayerIdE::GHOST_KICK )] = false;
                spec.m_active[L( LayerIdE::BASS )] = false;
                spec.m_active[L( LayerIdE::ACID )] = false;
                if ( inBlock < 23 )
                {
                    NoteEvent n;
                    n.m_beat = beatOf( bar );
                    n.m_length = 4.0;
                    n.m_part = percPart( 11 );
                    n.m_pitch = 49;
                    n.m_velocity = 0.9f;
                    sc.m_notes.push_back( n );
                }
                else
                {
                    for ( bool &a : spec.m_active ) a = false;   // the cut: one bar of nothing but the tails
                }
            }
            if ( inBlock == 24 )
            {
                op( bar, OpKindE::RETURN, LayerIdE::KICK );
                sc.m_markers.push_back( Marker{ beatOf( bar ), "Return" } );
                sc.m_gestures.push_back( home( noiseLevel, beatOf( bar ) ) );
                sc.m_gestures.push_back( home( noiseCut, beatOf( bar ) ) );
                if ( active[L( LayerIdE::PING )] || active[L( LayerIdE::CHORD )] )
                {
                    sc.m_gestures.push_back(
                        ramp( p, cloudLevel, beatOf( bar ), beatOf( 8 ), -3.0f, -60.0f, GestureShapeE::EASE_OUT, 0 ) );
                    sc.m_gestures.push_back( home( cloudLevel, beatOf( bar + 8 ) ) );
                }
            }
        }
        // "Mute one expected hit" before the first block's change.
        if ( bar == 31 ) spec.m_muteStep[14] = true;
        realizeBar( plan, spec, sc.m_notes );
    }
    op( bars, OpKindE::END, LayerIdE::COUNT );
    sc.m_lengthBeats = beatOf( bars );
    sc.sort();
    return sc;
}

}   // namespace tot
