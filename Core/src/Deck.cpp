/**
 * @file Deck.cpp
 * @brief A deck: events on the sample grid, the parameter raster, the low end as one system, buses, sends, track bus.
 * @note The raster, the event splitting and the automation cursors follow Ephemeris `Core/src/Engine.cpp` at d047d79
 *       (27.09.2026); the signal flow is Totality's. Until Phase 4 this was the engine itself.
 * @author Rene Weller
 */
#include "tot/Deck.h"
#include "tot/Profile.h"
#include <algorithm>
#include <cmath>

namespace tot
{

// The tone trims' bands (20.10.2026), one per band the Leveler reads: a low shelf under the sub, a bell over the kick's
// fundamental, broad bells on the low mids and the mids (two octaves and more), a high shelf.
const ToneBand DeckToneBands[ToneBands] = { { ToneBandE::LOW_SHELF, 45.0f, 0.7071f },
                                            { ToneBandE::BELL, 160.0f, 1.0f },
                                            { ToneBandE::BELL, 500.0f, 0.67f },
                                            { ToneBandE::BELL, 2200.0f, 0.57f },
                                            { ToneBandE::HIGH_SHELF, 6500.0f, 0.7071f } };

namespace
{
constexpr double PiD = 3.141592653589793;   ///< pi
constexpr float  BassMinCut =
    100.0f;                        ///< the bass synth's and the 303's lowest low cut: under it only kick, rumble, sub
constexpr float GlueMix = 0.35f;   ///< the glue in parallel (PLAN 8.4)
/** The room's makeup: the FDN returns quietly (Ephemeris: +4 dB at sends of 0.3); +12 dB made it audible in Phase 2, the
 *  fit against the references' width another +10 dB (Params.cpp, MixParams), so space.level's 0 dB is the fitted room. */
constexpr float  RoomMakeupDb = 22.0f;
constexpr double LeadSeconds = 0.5;    ///< a deck starts playing this long before its first note
constexpr double TailSeconds = 20.0;   ///< and plays on this long after its last (rooms, echo, plate)
}   // namespace

void Deck::prepare( const ParamStore *params, double sampleRate, int index )
{
    m_params = params;
    m_sampleRate = sampleRate;
    m_index = index;
    m_kick.prepare( sampleRate );
    m_rumble.prepare( sampleRate, Raster );
    m_sub.prepare( sampleRate );
    m_kit.prepare( sampleRate );
    m_ping.prepare( sampleRate );
    m_bass.prepare( sampleRate );
    m_acid.prepare( sampleRate );
    m_chord.prepare( sampleRate );
    m_drone.prepare( sampleRate );
    m_texture.prepare( sampleRate );
    m_room.prepare( sampleRate );
    m_dub.prepare( sampleRate, Raster );
    m_cloud.prepare( sampleRate, 0x434C4F5544ull + static_cast<uint64_t>( index ) );   // "CLOUD"
    m_padsDuck.prepare( sampleRate );
    m_fxDuck.prepare( sampleRate );
    m_glue.prepare( sampleRate );
    const float fs = static_cast<float>( sampleRate );
    m_tiltCoef = 1.0f - std::exp( -2.0f * Pi * 1000.0f / fs );
    m_trimCoef = 1.0f - std::exp( -1.0f / ( 1.0f * fs ) );   // a correction glides in over about a second
    m_toneEq.prepare( sampleRate, DeckToneBands, ToneBands );
    m_toneCoef = m_trimCoef;
    m_toneNow.fill( 0.0f );
    m_toneTarget.fill( 0.0f );
    for ( int p = 0; p < BalParts; ++p ) m_balGain[p] = m_balTarget[p] = 1.0f;
    const size_t n = static_cast<size_t>( Raster );
    for ( std::vector<float> *b :
          { &m_kickBuf, &m_bodyBuf, &m_rumbleBuf, &m_subBuf, &m_pingL,    &m_pingR,    &m_bassL,    &m_bassR,
            &m_acidL,   &m_acidR,   &m_chordL,    &m_chordR, &m_droneL,   &m_droneR,   &m_texL,     &m_texR,
            &m_roomInL, &m_roomInR, &m_roomL,     &m_roomR,  &m_echoInL,  &m_echoInR,  &m_plateInL, &m_plateInR,
            &m_dubL,    &m_dubR,    &m_drumL,     &m_drumR,  &m_cloudInL, &m_cloudInR, &m_cloudL,   &m_cloudR } )
        b->assign( n, 0.0f );
    clear();
}

void Deck::clear()
{
    m_loaded = false;
    m_score = Score{};
    m_events.clear();
    m_tracks.clear();
    m_trackOf.assign( static_cast<size_t>( m_params != nullptr ? m_params->count() : 0 ), -1 );
    m_mixerSteps.clear();
    m_plays.clear();
    m_lateTrims.clear();
    m_lateBal.clear();
    m_lateTone.clear();
    m_base.assign( static_cast<size_t>( m_params != nullptr ? m_params->count() : 0 ),
                   std::numeric_limits<float>::quiet_NaN() );
    m_knobCursor = 0;
    m_knobGroup = -1.0;
    m_newGroup = false;
    m_evCursor = 0;
}

void Deck::load( const Score &score )
{
    clear();
    m_loaded = true;
    m_score = score;
    m_score.sort();
    m_lateTrims.reserve( m_score.m_levels.size() );
    m_lateBal.reserve( m_score.m_levels.size() );
    m_lateTone.reserve( m_score.m_levels.size() );
    // Events on the sample grid: the first sample at or after the ideal time, and how late that is.
    int id = 0;
    for ( const NoteEvent &note : m_score.m_notes )
    {
        const double x = m_score.m_tempo.secondsAt( note.m_beat ) * m_sampleRate;
        const double s = std::ceil( x );
        Ev           on{ static_cast<int64_t>( s ),
                         1,
                         static_cast<uint8_t>( note.m_part ),
                         note.m_accent,
                         note.m_slide,
                         note.m_pitch,
                         note.m_velocity,
                         note.m_shift,
                         s - x,
                         id };
        m_events.push_back( on );
        if ( !isOneShot( note.m_part ) )
        {
            const double xo = m_score.m_tempo.secondsAt( note.m_beat + note.m_length ) * m_sampleRate;
            const double so = std::ceil( xo );
            m_events.push_back(
                Ev{ static_cast<int64_t>( so ), 0, on.m_part, false, false, note.m_pitch, 0.0f, 0, so - xo, id } );
        }
        ++id;
    }
    std::stable_sort( m_events.begin(), m_events.end(), []( const Ev &a, const Ev &b )
                      { return a.m_sample != b.m_sample ? a.m_sample < b.m_sample : a.m_on < b.m_on; } );
    // The automation, per parameter; the steps on this deck's mixer channel, for the engine.
    const int mixFirst = m_params->base( ModuleE::DECK, m_index ), mixEnd = mixFirst + deck::COUNT;
    for ( const Gesture &g : m_score.m_gestures )
    {
        if ( g.m_param < 0 || g.m_param >= m_params->count() ) continue;
        int &t = m_trackOf[static_cast<size_t>( g.m_param )];
        if ( t < 0 )
        {
            t = static_cast<int>( m_tracks.size() );
            m_tracks.push_back( Track{ g.m_param, {}, 0, 0.0f } );
        }
        m_tracks[static_cast<size_t>( t )].m_gestures.push_back( g );
        if ( g.m_param >= mixFirst && g.m_param < mixEnd )
            m_mixerSteps.push_back(
                static_cast<int64_t>( std::ceil( m_score.m_tempo.secondsAt( g.m_beat ) * m_sampleRate ) ) );
    }
    std::sort( m_mixerSteps.begin(), m_mixerSteps.end() );
    m_mixerSteps.erase( std::unique( m_mixerSteps.begin(), m_mixerSteps.end() ), m_mixerSteps.end() );
    // Where it plays: around its notes, merged.
    const int64_t lead = static_cast<int64_t>( LeadSeconds * m_sampleRate ),
                  tail = static_cast<int64_t>( TailSeconds * m_sampleRate );
    for ( const Ev &e : m_events )
    {
        const int64_t from = std::max<int64_t>( 0, e.m_sample - lead ), to = e.m_sample + tail;
        if ( !m_plays.empty() && from <= m_plays.back().second )
            m_plays.back().second = std::max( m_plays.back().second, to );
        else
            m_plays.push_back( { from, to } );
    }
    seek( 0 );
}

bool Deck::playsAt( int64_t sample ) const
{
    for ( const auto &r : m_plays )
    {
        if ( sample < r.first ) return false;
        if ( sample < r.second ) return true;
    }
    return false;
}

int64_t Deck::nextRestChange( int64_t sample ) const
{
    for ( const auto &r : m_plays )
    {
        if ( r.first > sample ) return r.first;
        if ( r.second > sample ) return r.second;
    }
    return Never;
}

void Deck::seek( int64_t sample )
{
    m_evCursor = 0;
    while ( m_evCursor < m_events.size() && m_events[m_evCursor].m_sample < sample ) ++m_evCursor;
    // A jump back to the start plays what is due at it; elsewhere the voices fall silent.
    if ( sample == 0 ) m_evCursor = 0;
    for ( Track &t : m_tracks )
    {
        t.m_cursor = t.m_gestures.size();
        t.m_offset = 0.0f;
    }
    // The knob settings again from the start: the next cell takes those due where the deck now is.
    std::fill( m_base.begin(), m_base.end(), std::numeric_limits<float>::quiet_NaN() );
    m_knobCursor = 0;
    m_knobGroup = -1.0;
    m_newGroup = false;
    m_kick.reset();
    m_rumble.reset();
    m_sub.reset();
    m_kit.reset();
    m_ping.reset();
    m_bass.reset();
    m_acid.reset();
    m_chord.reset();
    m_drone.reset();
    m_texture.reset();
    m_room.reset();
    m_dub.reset();
    m_cloud.reset();
    m_padsDuck.reset();
    m_fxDuck.reset();
    m_glue.reset();
    resetStems();
    for ( Svf &f : m_hatsLp ) f.reset();
    for ( Svf &f : m_percLp ) f.reset();
    for ( auto &ch : m_groupHp )
        for ( Svf &f : ch ) f.reset();
    m_tiltState[0] = m_tiltState[1] = 0.0f;
    m_haveKick = false;
    m_subNote = m_bassNote = m_acidNote = m_droneNote = -1;
    // The loudness correction at the new place, at once.
    const double b = m_score.m_tempo.beatAt( static_cast<double>( sample ) / m_sampleRate );
    m_trimGain = m_trimTarget = dbToGain( m_score.trimAt( b ) );
    const BalanceDbT bal = m_score.balanceAt( b );
    for ( int p = 0; p < BalParts; ++p ) m_balGain[p] = m_balTarget[p] = dbToGain( bal[static_cast<size_t>( p )] );
    m_toneNow = m_toneTarget = m_score.toneAt( b );
    m_toneEq.setGains( m_toneNow.data() );
    m_toneState.reset();
}

float Deck::played( int id ) const
{
    const float knob = m_params->get( id );
    float       value = knob;
    if ( id >= 0 && static_cast<size_t>( id ) < m_base.size() )
    {
        const float b = m_base[static_cast<size_t>( id )];
        if ( b == b )
        {
            // The track's own value; where a hand has turned the knob away from what the engine wrote on it, by as much.
            value = b;
            const float s = m_shown != nullptr ? m_shown[id] : std::numeric_limits<float>::quiet_NaN();
            if ( s == s && knob != s )
                value =
                    m_params->fromNormalised( id, m_params->toNormalised( id, b ) + m_params->toNormalised( id, knob ) -
                                                      m_params->toNormalised( id, s ) );
        }
    }
    if ( id < 0 || id >= static_cast<int>( m_trackOf.size() ) || m_trackOf[static_cast<size_t>( id )] < 0 )
        return value;
    const float off = m_tracks[static_cast<size_t>( m_trackOf[static_cast<size_t>( id )] )].m_offset;
    if ( off == 0.0f ) return value;
    return m_params->fromNormalised( id, m_params->toNormalised( id, value ) + off );
}

void Deck::applyKnobs( double beat )
{
    // One beat early: a track's sounds are in place before its first note (its deck rests until then).
    while ( m_knobCursor < m_score.m_knobs.size() && m_score.m_knobs[m_knobCursor].m_beat <= beat + 1.0 )
    {
        const KnobSet &k = m_score.m_knobs[m_knobCursor++];
        if ( k.m_beat != m_knobGroup )
        {
            std::fill( m_base.begin(), m_base.end(), std::numeric_limits<float>::quiet_NaN() );
            m_knobGroup = k.m_beat;
            m_newGroup = true;
        }
        if ( k.m_param >= 0 && static_cast<size_t>( k.m_param ) < m_base.size() )
            m_base[static_cast<size_t>( k.m_param )] = k.m_value;
    }
}

void Deck::readPlayed( ModuleE m, int instance, float *out ) const
{
    const int b = m_params->base( m, instance );
    const int n = ParamStore::moduleCount( m );
    for ( int i = 0; i < n; ++i ) out[i] = played( b + i );
}

void Deck::updateCell( int64_t sample )
{
    const double seconds = static_cast<double>( sample ) / m_sampleRate;
    const double beat = m_score.m_tempo.beatAt( seconds );
    applyKnobs( beat );
    for ( Track &t : m_tracks )
    {
        // The latest curve that has started (curves are sorted by beat; cursor == size() means none yet).
        const size_t none = t.m_gestures.size();
        size_t       c = t.m_cursor, next = c == none ? 0 : c + 1;
        while ( next < t.m_gestures.size() && t.m_gestures[next].m_beat <= beat )
        {
            c = next;
            ++next;
        }
        t.m_cursor = c;
        t.m_offset = c == none ? 0.0f : gestureValue( t.m_gestures[c], beat );
    }
    // The performer's mutes (live only): a muted group's notes are not played, its tails ring out.
    m_mutes = 0;
    if ( m_live )
        for ( int k = 0; k < perform::Mutes; ++k )
            if ( m_params->getBool( m_params->id( ModuleE::PERFORM, 0, perform::MUTE_KICK + k ) ) ) m_mutes |= 1u << k;
    // The keyboard (live only, 01.10.2026): what it plays, whether that replaces the composer's notes, whether the
    // composer plays at all.
    m_keyTarget =
        m_live ? m_params->getInt( m_params->id( ModuleE::PERFORM, 0, perform::KEYBOARD_PART ) ) : perform::keys::OFF;
    m_keyReplace = !m_live || m_params->getInt( m_params->id( ModuleE::PERFORM, 0, perform::KEYBOARD_MODE ) ) == 0;
    m_composerOff = m_live && !m_params->getBool( m_params->id( ModuleE::PERFORM, 0, perform::COMPOSER ) );

    float c[64];
    readPlayed( ModuleE::COMPOSE, 0, c );
    m_keyRoot = static_cast<int>( std::lround( c[compose::KEY] ) );
    m_scale = static_cast<int>( std::lround( c[compose::SCALE] ) );
    m_subOwns = std::lround( c[compose::LOW_OWNER] ) == static_cast<long>( LowOwnerE::SUB );
    const double bpm = m_score.m_tempo.bpmAt( beat );

    // The global motion (Dok. 8.4): three LFOs of 7, 11 and 13 beats and a drift of 0.065 Hz, from the absolute beat
    // and second, so the phases do not depend on where a render starts. They step every 128 samples (an absolute
    // raster, 2.7 ms), so the kit recomputes its lanes a quarter as often.
    float mv[16];
    readPlayed( ModuleE::MOTION, 0, mv );
    const int64_t q = ( sample / 128 ) * 128;
    const double  qs = static_cast<double>( q ) / m_sampleRate, qb = m_score.m_tempo.beatAt( qs );
    const float   amt = mv[motion::AMOUNT];
    const float   lfo7 = amt * static_cast<float>( std::sin( 2.0 * PiD * qb / 7.0 ) );
    const float   lfo11 = amt * static_cast<float>( std::sin( 2.0 * PiD * qb / 11.0 + 1.3 ) );
    const float   lfo13 = amt * static_cast<float>( std::sin( 2.0 * PiD * qb / 13.0 + 2.1 ) );
    const float   drift = amt * static_cast<float>( std::sin( 2.0 * PiD * 0.065 * qs + 0.7 ) );
    const float   drift2 = amt * static_cast<float>( std::sin( 2.0 * PiD * 0.05 * qs + 2.9 ) );

    float v[64];
    readPlayed( ModuleE::KICK, 0, v );
    Kick::constrain( v, 0.0, m_keyRoot );
    m_kick.update( v, m_keyRoot );

    readPlayed( ModuleE::RUMBLE, 0, v );
    if ( m_subOwns ) v[rumble::SUB] = -60.0f;   // the sub bass owns the band under the split
    v[rumble::DRIVE] = std::max( 0.0f, v[rumble::DRIVE] + mv[motion::RUMBLE_DRIVE] * drift2 );
    m_rumble.update( v, m_kick.tunedEndHz() );

    readPlayed( ModuleE::SUB, 0, v );
    m_sub.update( v );

    m_kit.setTempo( bpm );
    for ( int l = 0; l < PercLanes; ++l )
    {
        readPlayed( ModuleE::PERC, l, v );
        const PercRoleE r = static_cast<PercRoleE>( std::lround( v[perc::ROLE] ) );
        if ( r == PercRoleE::CLOSED_HAT || r == PercRoleE::ROLLING_HAT || r == PercRoleE::OPEN_HAT )
        {
            const float k = 1.0f + mv[motion::HAT_DECAY] * lfo11;
            v[perc::NOISE_DECAY] *= k;
            v[perc::DECAY] *= k;
            // The hats' own filters wander, not the bus's low pass (which is open, and a ramp target of the form).
            v[perc::CUTOFF] *= std::pow( 2.0f, mv[motion::HATS_CUT] * drift );
        }
        else
        {
            v[perc::CUTOFF] *= std::pow( 2.0f, mv[motion::PERC_CUT] * lfo13 );
        }
        m_kit.update( l, v, m_keyRoot, m_scale );
        m_laneIsHat[l] = r == PercRoleE::CLOSED_HAT || r == PercRoleE::ROLLING_HAT || r == PercRoleE::OPEN_HAT ||
                         r == PercRoleE::RIDE || r == PercRoleE::SHAKER;
    }

    readPlayed( ModuleE::PING, 0, v );
    m_ping.update( v );

    readPlayed( ModuleE::BASS, 0, v );
    m_bass.update( v, BassMinCut );
    m_bassSends = Sends{ v[synth::ROOM_SEND], v[synth::DUB_SEND], 0.0f };
    readPlayed( ModuleE::ACID, 0, v );
    m_acid.update( v, BassMinCut );
    m_acidSends = Sends{ v[synth::ROOM_SEND], v[synth::DUB_SEND], 0.0f };
    readPlayed( ModuleE::CHORD, 0, v );
    m_chord.update( v );
    m_chordLevel = v[chord::LEVEL] <= -59.9f ? 0.0f : dbToGain( v[chord::LEVEL] );
    m_chordSends = Sends{ 0.0f, v[chord::DUB_SEND], v[chord::PLATE_SEND] };
    readPlayed( ModuleE::DRONE, 0, v );
    m_drone.update( v );
    m_drone.at( beat );
    m_droneSends = Sends{ v[drone::ROOM_SEND], 0.0f, v[drone::PLATE_SEND] };
    readPlayed( ModuleE::TEXTURE, 0, v );
    m_texture.update( v );
    readPlayed( ModuleE::DUB, 0, v );
    m_dub.update( v, bpm );
    m_pingEcho = v[dub::PING_SEND];
    m_hatsEcho = v[dub::HATS_SEND];
    m_percEcho = v[dub::PERC_SEND];
    readPlayed( ModuleE::CLOUD, 0, v );
    m_cloud.update( v );
    m_cloudPing = v[cloud::PING_SEND];
    m_cloudChord = v[cloud::CHORD_SEND];
    m_cloudPlate = v[cloud::PLATE_SEND];

    const float fs = static_cast<float>( m_sampleRate );
    readPlayed( ModuleE::MIX, 0, v );
    m_hatsGain = dbToGain( v[mix::HATS_LEVEL] + mv[motion::HATS_LEVEL] * lfo7 );
    m_percGain = dbToGain( v[mix::PERC_LEVEL] );
    for ( Svf &f : m_hatsLp ) f.setQ( std::min( v[mix::HATS_CUT], 0.45f * fs ), 0.7071f, fs );
    for ( Svf &f : m_percLp ) f.setQ( std::min( v[mix::PERC_CUT], 0.45f * fs ), 0.7071f, fs );
    m_drumSat = v[mix::DRUM_SAT];
    m_padsDuck.set( v[mix::DUCK_LOW], v[mix::DUCK_MID], 60.0f, 250.0f );
    // The group high pass: off at its floor (the sweep of the form raises it over 8 to 16 bars and lets it fall back).
    m_groupHpOn = v[mix::LOW_CUT] > 20.5f;
    if ( !m_groupHpOn )
    {   // back at the floor: no stale state later
        for ( auto &ch : m_groupHp )
            for ( Svf &f : ch ) f.reset();
        for ( StemBus &s : m_stemBus )
            for ( auto &ch : s.m_hp )
                for ( Svf &f : ch ) f.reset();
    }
    for ( auto &ch : m_groupHp )
    {
        ch[0].setK( v[mix::LOW_CUT], 1.8477590f, fs );
        ch[1].setK( v[mix::LOW_CUT], 0.7653669f, fs );
    }
    m_fxDuck.set( v[mix::DUCK_LOW], v[mix::DUCK_MID], 60.0f, 250.0f );

    readPlayed( ModuleE::SPACE, 0, v );
    m_room.set( v[space::SIZE], v[space::DECAY], v[space::DAMPING], v[space::PRE_DELAY] * 0.001f * fs,
                v[space::LOW_CUT], v[space::HIGH_CUT] );
    m_roomReturn = v[space::LEVEL] <= -59.9f ? 0.0f : dbToGain( v[space::LEVEL] + RoomMakeupDb );
    m_hatsSend = v[space::HATS_SEND];
    m_percSend = v[space::PERC_SEND];
    m_pingSend = v[space::PING_SEND];

    // The track bus: the tilt and the glue are the track's (a style's recipe sets them), the master the engine's.
    readPlayed( ModuleE::MASTER, 0, v );
    m_glue.set( v[master::THRESHOLD], v[master::RATIO], 6.0f, 20.0f, 200.0f );
    // The tilt: the highs against the lows by Tilt dB, the lows kept (a pivot at 1 kHz took the low end down by half the
    // tilt, and with it the level the Leveler had to win back).
    m_tiltHigh = dbToGain( v[master::TILT] );

    // The Leveler's correction of the track playing: the score's, or the one found while it plays.
    float trim = m_score.trimAt( beat );
    if ( !m_lateTrims.empty() )
    {
        size_t k = 0;
        for ( size_t i = 0; i < m_score.m_levels.size(); ++i )
            if ( m_score.m_levels[i].m_beat <= beat ) k = i;
        if ( k < m_lateTrims.size() ) trim = m_lateTrims[k];
    }
    m_trimTarget = dbToGain( trim );
    // Phase 18: the parts' corrections, the same way.
    BalanceDbT bal = m_score.balanceAt( beat );
    if ( !m_lateBal.empty() )
    {
        size_t k = 0;
        for ( size_t i = 0; i < m_score.m_levels.size(); ++i )
            if ( m_score.m_levels[i].m_beat <= beat ) k = i;
        if ( k < m_lateBal.size() ) bal = m_lateBal[k];
    }
    for ( int p = 0; p < BalParts; ++p ) m_balTarget[p] = dbToGain( bal[static_cast<size_t>( p )] );
    // 20.10.2026: the tone trims, the same way.
    m_toneTarget = m_score.toneAt( beat );
    if ( !m_lateTone.empty() )
    {
        size_t k = 0;
        for ( size_t i = 0; i < m_score.m_levels.size(); ++i )
            if ( m_score.m_levels[i].m_beat <= beat ) k = i;
        if ( k < m_lateTone.size() ) m_toneTarget = m_lateTone[k];
    }
}

void Deck::watchPeaks( bool on )
{
    m_watch = on;
    if ( !on ) return;
    m_kickPeak = 0.0f;
    for ( float &p : m_partPeak ) p = 0.0f;
}

void Deck::dispatchUntil( int64_t sample )
{
    while ( m_evCursor < m_events.size() && m_events[m_evCursor].m_sample <= sample )
        dispatch( m_events[m_evCursor++] );
}

void Deck::dispatch( const Ev &e )
{
    const PartE part = static_cast<PartE>( e.m_part );
    if ( e.m_on != 0 && !m_liveEvent && silenced( part ) ) return;   // the keyboard's or nobody's (01.10.2026)
    // The family jam (02.10.2026): a pitched part's composed note moves by the jam's transposition; its end by the shift
    // its start had, so a voice found by its pitch is found. The keys a player plays are their own.
    if ( !m_liveEvent )
    {
        switch ( part )
        {
        case PartE::SUB:
        case PartE::PING:
        case PartE::BASS:
        case PartE::ACID:
        case PartE::CHORD:
        case PartE::DRONE:
        case PartE::TEXTURE:
        {
            int8_t   &was = m_jamShift[static_cast<size_t>( e.m_part )][static_cast<size_t>( e.m_pitch & 127 )];
            const int shift = e.m_on != 0 ? m_jamTranspose : was;
            if ( e.m_on != 0 ) was = static_cast<int8_t>( m_jamTranspose );
            if ( shift != 0 )
            {
                Ev t = e;
                t.m_pitch = std::clamp( e.m_pitch + shift, 0, 127 );
                dispatchPlayed( t );
                return;
            }
            break;
        }
        default: break;
        }
    }
    dispatchPlayed( e );
}

void Deck::dispatchPlayed( const Ev &e )
{
    const PartE part = static_cast<PartE>( e.m_part );
    // MIDI out (02.10.2026): the composer's notes as they are played -- a muted group's note-ons are not, offs always.
    if ( m_noteTap != nullptr && !m_liveEvent && ( e.m_on == 0 || muteOf( part ) < 0 || !muted( muteOf( part ) ) ) )
        m_noteTap->add( e.m_sample, e.m_part, e.m_pitch, e.m_velocity, e.m_on != 0,
                        isOneShot( part ) ? e.m_sample + static_cast<int64_t>( m_sampleRate * 0.05 ) : -1 );
    switch ( part )
    {
    case PartE::KICK:
    {
        if ( muted( perform::MUTE_KICK ) ) return;
        m_kick.trigger( e.m_velocity, e.m_late );
        const double c = m_kick.asymptoticPhase(), f0 = m_kick.tunedEndHz();
        m_rumble.kick( e.m_late, c, f0 );
        m_sub.kick( e.m_late );
        m_bass.kick( e.m_late );
        m_acid.kick( e.m_late );
        m_padsDuck.trigger( e.m_late );
        m_fxDuck.trigger( e.m_late );
        m_haveKick = true;
        m_kickTime = static_cast<double>( e.m_sample ) - e.m_late;
        m_kickC = c;
        m_kickF0 = f0;
        return;
    }
    case PartE::SUB:
    case PartE::BASS:
    {
        if ( e.m_on == 0 )
        {
            if ( part == PartE::BASS && e.m_id == m_bassNote )
            {
                m_bass.noteOff();
                m_bassNote = -1;
            }
            if ( e.m_id == m_subNote )
            {
                m_sub.noteOff();
                m_subNote = -1;
            }
            return;
        }
        if ( part == PartE::BASS )
        {
            if ( !muted( perform::MUTE_BASS ) )
            {
                m_bass.noteOn( e.m_pitch, e.m_velocity, e.m_late, e.m_accent, e.m_slide );
                m_bassNote = e.m_id;
            }
            if ( !m_subOwns ) return;   // the sine plays the line only where the sub owns the low end
        }
        if ( muted( perform::MUTE_SUB ) ) return;
        double phase = -1.0;
        if ( m_sub.locked() && m_haveKick )
        {
            // The kick's phase at the note's ideal start, f0 dt + c, carried to the note's pitch.
            const double dt = ( static_cast<double>( e.m_sample ) - e.m_late - m_kickTime ) / m_sampleRate;
            const double hz = m_sub.noteHz( e.m_pitch );
            const double kickPhase = m_kickF0 * dt + m_kickC;
            const double p = hz / m_kickF0 * kickPhase - m_sub.chainPhase( hz );
            phase = p - std::floor( p );
        }
        m_sub.noteOn( e.m_pitch, e.m_velocity, e.m_late, phase );
        m_subNote = e.m_id;
        return;
    }
    case PartE::ACID:
        if ( e.m_on == 0 )
        {
            if ( e.m_id == m_acidNote )
            {
                m_acid.noteOff();
                m_acidNote = -1;
            }
            return;
        }
        if ( muted( perform::MUTE_BASS ) ) return;
        m_acid.noteOn( e.m_pitch, e.m_velocity, e.m_late, e.m_accent, e.m_slide );
        m_acidNote = e.m_id;
        return;
    case PartE::CHORD:
        if ( e.m_on == 0 )
            m_chord.noteOff( e.m_pitch );
        else if ( !muted( perform::MUTE_PADS ) )
            m_chord.noteOn( e.m_pitch, e.m_velocity, e.m_late );
        return;
    case PartE::DRONE:
        if ( e.m_on == 0 )
        {
            if ( e.m_id == m_droneNote )
            {
                m_drone.noteOff();
                m_droneNote = -1;
            }
            return;
        }
        if ( muted( perform::MUTE_PADS ) ) return;
        m_drone.noteOn( e.m_pitch, e.m_velocity );
        m_droneNote = e.m_id;
        return;
    case PartE::TEXTURE: m_texture.gate( e.m_on != 0 && !muted( perform::MUTE_PADS ) ); return;
    case PartE::PING:
        if ( muted( perform::MUTE_PING ) ) return;
        m_ping.noteOn( e.m_pitch, e.m_velocity, e.m_late, static_cast<uint32_t>( e.m_id ) );
        return;
    default:
    {
        const int lane = laneOf( part );
        if ( lane >= 0 && !muted( m_laneIsHat[lane] ? perform::MUTE_HATS : perform::MUTE_PERC ) )
            m_kit.trigger( lane, e.m_velocity, e.m_shift, e.m_late );
        return;
    }
    }
}

void Deck::resetStems()
{
    for ( StemBus &s : m_stemBus )
    {
        for ( auto &ch : s.m_hp )
            for ( Svf &f : ch ) f.reset();
        s.m_tilt[0] = s.m_tilt[1] = 0.0f;
        s.m_tone.reset();
    }
    for ( auto &r : m_stemPads ) r = m_padsDuck.replica();
    for ( auto &r : m_stemFx ) r = m_fxDuck.replica();
}

void Deck::render( int64_t sample, float *L, float *R, int n, float *const *stemL, float *const *stemR )
{
    const bool stems = stemL != nullptr;
    // The sources.
    {
        TOT_PROF( KICK );
        m_kick.process( m_kickBuf.data(), m_bodyBuf.data(), n );
    }
    {
        TOT_PROF( RUMBLE );
        m_rumble.process( m_bodyBuf.data(), m_rumbleBuf.data(), n );
    }
    {
        TOT_PROF( SUB );
        m_sub.process( m_subBuf.data(), n );
    }
    {
        TOT_PROF( KIT );
        m_kit.processLanes( n );
    }
    {
        TOT_PROF( PING );
        m_ping.process( m_pingL.data(), m_pingR.data(), n, sample );
    }
    {
        TOT_PROF( BASS );
        m_bass.process( m_bassL.data(), m_bassR.data(), n );
    }
    {
        TOT_PROF( ACID );
        m_acid.process( m_acidL.data(), m_acidR.data(), n );
    }
    {
        TOT_PROF( CHORD );
        m_chord.process( m_chordL.data(), m_chordR.data(), n, sample );
    }
    {
        TOT_PROF( DRONE );
        m_drone.process( m_droneL.data(), m_droneR.data(), n );
    }
    {
        TOT_PROF( TEXTURE );
        m_texture.process( m_texL.data(), m_texR.data(), n );
    }
    TOT_PROF_BEGIN( Buses );

    // The percussion buses, the drum bus, the pads, the sends.
    const float *ll = m_kit.laneL();
    const float *lr = m_kit.laneR();
    float        busH[2][Raster], busP[2][Raster];
    for ( int i = 0; i < n; ++i )
    {
        // Phase 18: the parts' gains against the kick, at the sources -- every lane of the kit, every tonal voice (the sends
        // follow them, as after a fader).
        for ( int p = 0; p < BalParts; ++p ) m_balGain[p] += ( m_balTarget[p] - m_balGain[p] ) * m_trimCoef;
        {
            const size_t k = static_cast<size_t>( i );
            const float  gp = m_balGain[static_cast<int>( BalPartE::PING )],
                         gb = m_balGain[static_cast<int>( BalPartE::BASS )];
            const float  ga = m_balGain[static_cast<int>( BalPartE::ACID )],
                         gd = m_balGain[static_cast<int>( BalPartE::DRONE )];
            const float  gt = m_balGain[static_cast<int>( BalPartE::TEXTURE )];
            m_pingL[k] *= gp;
            m_pingR[k] *= gp;
            m_bassL[k] *= gb;
            m_bassR[k] *= gb;
            m_acidL[k] *= ga;
            m_acidR[k] *= ga;
            m_droneL[k] *= gd;
            m_droneR[k] *= gd;
            m_texL[k] *= gt;
            m_texR[k] *= gt;
            if ( m_watch )
            {
                const auto most = []( float &m, float a, float b )
                { m = std::max( m, std::max( std::fabs( a ), std::fabs( b ) ) ); };
                most( m_kickPeak, m_kickBuf[k], m_kickBuf[k] );
                most( m_partPeak[static_cast<int>( BalPartE::PING )], m_pingL[k], m_pingR[k] );
                most( m_partPeak[static_cast<int>( BalPartE::BASS )], m_bassL[k], m_bassR[k] );
                most( m_partPeak[static_cast<int>( BalPartE::ACID )], m_acidL[k], m_acidR[k] );
                most( m_partPeak[static_cast<int>( BalPartE::CHORD )],
                      m_chordL[k] * m_chordLevel * m_balGain[static_cast<int>( BalPartE::CHORD )],
                      m_chordR[k] * m_chordLevel * m_balGain[static_cast<int>( BalPartE::CHORD )] );
                most( m_partPeak[static_cast<int>( BalPartE::DRONE )], m_droneL[k], m_droneR[k] );
                most( m_partPeak[static_cast<int>( BalPartE::TEXTURE )], m_texL[k], m_texR[k] );
            }
        }
        float hl = 0.0f, hr = 0.0f, pl = 0.0f, pr = 0.0f;
        for ( int l = 0; l < PercLanes; ++l )
        {
            const size_t k = static_cast<size_t>( i * PercKit::Stride + l );
            const float  a = ll[k] * m_balGain[l], b = lr[k] * m_balGain[l];
            if ( m_laneIsHat[l] )
            {
                hl += a;
                hr += b;
            }
            else
            {
                pl += a;
                pr += b;
            }
            if ( m_watch )
                m_partPeak[l] = std::max( m_partPeak[l], std::max( std::fabs( a ), std::fabs( b ) ) *
                                                             ( m_laneIsHat[l] ? m_hatsGain : m_percGain ) );
        }
        busH[0][i] = m_hatsLp[0].lp( hl ) * m_hatsGain;
        busH[1][i] = m_hatsLp[1].lp( hr ) * m_hatsGain;
        busP[0][i] = m_percLp[0].lp( pl ) * m_percGain;
        busP[1][i] = m_percLp[1].lp( pr ) * m_percGain;
        const size_t k = static_cast<size_t>( i );
        const float  gChord = m_chordLevel * m_balGain[static_cast<int>( BalPartE::CHORD )];
        m_chordL[k] *= gChord;
        m_chordR[k] *= gChord;
        m_roomInL[k] = busH[0][i] * m_hatsSend + busP[0][i] * m_percSend + m_pingL[k] * m_pingSend +
                       m_bassL[k] * m_bassSends.m_room + m_acidL[k] * m_acidSends.m_room +
                       m_droneL[k] * m_droneSends.m_room;
        m_roomInR[k] = busH[1][i] * m_hatsSend + busP[1][i] * m_percSend + m_pingR[k] * m_pingSend +
                       m_bassR[k] * m_bassSends.m_room + m_acidR[k] * m_acidSends.m_room +
                       m_droneR[k] * m_droneSends.m_room;
        m_echoInL[k] = m_bassL[k] * m_bassSends.m_dub + m_acidL[k] * m_acidSends.m_dub +
                       m_chordL[k] * m_chordSends.m_dub + m_pingL[k] * m_pingEcho + busH[0][i] * m_hatsEcho +
                       busP[0][i] * m_percEcho;
        m_echoInR[k] = m_bassR[k] * m_bassSends.m_dub + m_acidR[k] * m_acidSends.m_dub +
                       m_chordR[k] * m_chordSends.m_dub + m_pingR[k] * m_pingEcho + busH[1][i] * m_hatsEcho +
                       busP[1][i] * m_percEcho;
        m_plateInL[k] = m_chordL[k] * m_chordSends.m_plate + m_droneL[k] * m_droneSends.m_plate;
        m_plateInR[k] = m_chordR[k] * m_chordSends.m_plate + m_droneR[k] * m_droneSends.m_plate;
        // The drum bus: three stages of saturation mixed in (Dok. 8.7: "3 or 4 drum buss units at 10 or 20 % wet").
        const float mono = m_kickBuf[k] + m_rumbleBuf[k];
        float       dl = mono + busH[0][i] + busP[0][i], dr = mono + busH[1][i] + busP[1][i];
        const float inL = dl, inR = dr;
        if ( m_drumSat > 0.0f )
        {
            const float        m = 0.43f * m_drumSat;   // 15 % a stage at the default 0.35
            static const float Drive[3] = { 1.5f, 2.0f, 2.5f };
            for ( float d : Drive )
            {
                dl += m * ( std::tanh( d * dl ) / d - dl );
                dr += m * ( std::tanh( d * dr ) / d - dr );
            }
        }
        if ( stems )
        {   // the saturation's gain on the sum, for its parts
            m_satGain[0][i] = inL != 0.0f ? dl / inL : 1.0f;
            m_satGain[1][i] = inR != 0.0f ? dr / inR : 1.0f;
        }
        m_drumL[k] = dl;
        m_drumR[k] = dr;
    }
    TOT_PROF_END( BUSES );
    // The mixer page's meters, while it looks: the sources after their levels and corrections, the drums by bus.
    float      meterPk[MeterSink::STRIPS] = {};
    double     meterSs[MeterSink::STRIPS] = {};
    const auto measure = [&]( int strip, float l, float r )
    {
        meterPk[strip] = std::max( meterPk[strip], std::max( std::fabs( l ), std::fabs( r ) ) );
        meterSs[strip] += 0.5 * ( static_cast<double>( l ) * l + static_cast<double>( r ) * r );
    };
    if ( m_meter != nullptr )
    {
        for ( int i = 0; i < n; ++i )
        {
            const size_t k = static_cast<size_t>( i );
            measure( MeterSink::KICK, m_kickBuf[k], m_kickBuf[k] );
            measure( MeterSink::RUMBLE, m_rumbleBuf[k], m_rumbleBuf[k] );
            measure( MeterSink::SUB, m_subBuf[k], m_subBuf[k] );
            measure( MeterSink::HATS, busH[0][i], busH[1][i] );
            measure( MeterSink::PERC, busP[0][i], busP[1][i] );
            measure( MeterSink::PING, m_pingL[k], m_pingR[k] );
            measure( MeterSink::BASS, m_bassL[k], m_bassR[k] );
            measure( MeterSink::ACID, m_acidL[k], m_acidR[k] );
            measure( MeterSink::CHORD, m_chordL[k], m_chordR[k] );
            measure( MeterSink::DRONE, m_droneL[k], m_droneR[k] );
            measure( MeterSink::TEXTURE, m_texL[k], m_texR[k] );
        }
    }
    TOT_PROF_BEGIN( Cloud );
    // The cloud hears the ping and the chord; half of it goes into the plate.
    for ( int i = 0; i < n; ++i )
    {
        const size_t k = static_cast<size_t>( i );
        m_cloudInL[k] = m_pingL[k] * m_cloudPing + m_chordL[k] * m_cloudChord;
        m_cloudInR[k] = m_pingR[k] * m_cloudPing + m_chordR[k] * m_cloudChord;
    }
    if ( m_quest )
    {   // the Quest's quality (PLAN 11): no grains
        std::fill( m_cloudL.begin(), m_cloudL.begin() + n, 0.0f );
        std::fill( m_cloudR.begin(), m_cloudR.begin() + n, 0.0f );
    }
    else
    {
        m_cloud.process( m_cloudInL.data(), m_cloudInR.data(), m_cloudL.data(), m_cloudR.data(), n );
    }
    for ( int i = 0; i < n; ++i )
    {
        const size_t k = static_cast<size_t>( i );
        m_plateInL[k] += m_cloudL[k] * m_cloudPlate;
        m_plateInR[k] += m_cloudR[k] * m_cloudPlate;
    }
    TOT_PROF_END( CLOUD );
    {
        TOT_PROF( ROOM );
        m_room.process( m_roomInL.data(), m_roomInR.data(), m_roomL.data(), m_roomR.data(), n );
    }
    {
        TOT_PROF( DUB );
        m_dub.process( m_echoInL.data(), m_echoInR.data(), m_plateInL.data(), m_plateInR.data(), m_dubL.data(),
                       m_dubR.data(), n );
    }
    if ( m_meter != nullptr )
    {   // the returns
        const float gRoom = m_roomReturn * m_balGain[static_cast<int>( BalPartE::ROOM )];
        for ( int i = 0; i < n; ++i )
        {
            const size_t k = static_cast<size_t>( i );
            measure( MeterSink::ROOM, m_roomL[k] * gRoom, m_roomR[k] * gRoom );
            measure( MeterSink::DUB, m_dubL[k], m_dubR[k] );
            measure( MeterSink::CLOUD, m_cloudL[k], m_cloudR[k] );
        }
        m_meter->add( meterPk, meterSs, 0, MeterSink::STRIPS );
    }
    if ( stems )
    {
        // The parts of the pads and of the returns, before their ducks.
        for ( int i = 0; i < n; ++i )
        {
            const size_t k = static_cast<size_t>( i );
            stemL[STEM_CHORD][i] = m_chordL[k];
            stemR[STEM_CHORD][i] = m_chordR[k];
            stemL[STEM_DRONE][i] = m_droneL[k];
            stemR[STEM_DRONE][i] = m_droneR[k];
            const float gRoom = m_roomReturn * m_balGain[static_cast<int>( BalPartE::ROOM )];
            stemL[STEM_ROOM][i] = m_roomL[k] * gRoom;
            stemR[STEM_ROOM][i] = m_roomR[k] * gRoom;
            stemL[STEM_DUB][i] = m_dubL[k];
            stemR[STEM_DUB][i] = m_dubR[k];
            stemL[STEM_CLOUD][i] = m_cloudL[k];
            stemR[STEM_CLOUD][i] = m_cloudR[k];
        }
    }
    // The pads (chord and drone) and the returns step aside for the kick, band by band.
    for ( int i = 0; i < n; ++i )
    {
        const size_t k = static_cast<size_t>( i );
        m_chordL[k] += m_droneL[k];
        m_chordR[k] += m_droneR[k];
        const float gRoom = m_roomReturn * m_balGain[static_cast<int>( BalPartE::ROOM )];   // (Phase 18: the guard's)
        m_roomL[k] = m_roomL[k] * gRoom + m_dubL[k] + m_cloudL[k];
        m_roomR[k] = m_roomR[k] * gRoom + m_dubR[k] + m_cloudR[k];
    }
    TOT_PROF_BEGIN( Ducks );
    m_padsDuck.process( m_chordL.data(), m_chordR.data(), n, stems ? m_padsGains : nullptr );
    m_fxDuck.process( m_roomL.data(), m_roomR.data(), n, stems ? m_fxGains : nullptr );
    TOT_PROF_END( DUCKS );
    if ( stems )
    {
        MultibandDucker::apply( m_stemPads[0], m_padsGains, stemL[STEM_CHORD], stemR[STEM_CHORD], n );
        MultibandDucker::apply( m_stemPads[1], m_padsGains, stemL[STEM_DRONE], stemR[STEM_DRONE], n );
        MultibandDucker::apply( m_stemFx[0], m_fxGains, stemL[STEM_ROOM], stemR[STEM_ROOM], n );
        MultibandDucker::apply( m_stemFx[1], m_fxGains, stemL[STEM_DUB], stemR[STEM_DUB], n );
        MultibandDucker::apply( m_stemFx[2], m_fxGains, stemL[STEM_CLOUD], stemR[STEM_CLOUD], n );
        for ( int i = 0; i < n; ++i )
        {
            const size_t k = static_cast<size_t>( i );
            auto         put = [&]( int s, float l, float r )
            {
                stemL[s][i] = l;
                stemR[s][i] = r;
            };
            put( STEM_KICK, m_kickBuf[k] * m_satGain[0][i], m_kickBuf[k] * m_satGain[1][i] );
            put( STEM_RUMBLE, m_rumbleBuf[k] * m_satGain[0][i], m_rumbleBuf[k] * m_satGain[1][i] );
            put( STEM_SUB, m_subBuf[k], m_subBuf[k] );
            put( STEM_HATS, busH[0][i] * m_satGain[0][i], busH[1][i] * m_satGain[1][i] );
            put( STEM_PERC, busP[0][i] * m_satGain[0][i], busP[1][i] * m_satGain[1][i] );
            put( STEM_PING, m_pingL[k], m_pingR[k] );
            put( STEM_BASS, m_bassL[k], m_bassR[k] );
            put( STEM_ACID, m_acidL[k], m_acidR[k] );
            put( STEM_TEXTURE, m_texL[k], m_texR[k] );
        }
    }

    TOT_PROF_BEGIN( TrackBus );
    for ( int i = 0; i < n; ++i )
    {
        const size_t k = static_cast<size_t>( i );
        float        l =
            m_drumL[k] + m_subBuf[k] + m_pingL[k] + m_bassL[k] + m_acidL[k] + m_chordL[k] + m_texL[k] + m_roomL[k];
        float r =
            m_drumR[k] + m_subBuf[k] + m_pingR[k] + m_bassR[k] + m_acidR[k] + m_chordR[k] + m_texR[k] + m_roomR[k];
        if ( m_groupHpOn )
        {
            float lp, bp, hp;
            m_groupHp[0][0].tick( l, lp, bp, hp );
            m_groupHp[0][1].tick( hp, lp, bp, l );
            m_groupHp[1][0].tick( r, lp, bp, hp );
            m_groupHp[1][1].tick( hp, lp, bp, r );
        }
        // The tilt: a complementary one-pole split at 1 kHz, the highs apart.
        m_tiltState[0] += m_tiltCoef * ( l - m_tiltState[0] );
        m_tiltState[1] += m_tiltCoef * ( r - m_tiltState[1] );
        L[i] = m_tiltState[0] + m_tiltHigh * ( l - m_tiltState[0] );
        R[i] = m_tiltState[1] + m_tiltHigh * ( r - m_tiltState[1] );
        m_drumL[k] = L[i];   // the glue's dry signal (the drum bus is spent)
        m_drumR[k] = R[i];
    }
    TOT_PROF_END( TRACK_BUS );
    TOT_PROF_BEGIN( Glue );
    m_glue.process( L, R, n, stems ? m_glueGains : nullptr );
    // The glue in parallel, then the Leveler's trim: after the glue, which would otherwise halve every correction (a
    // ratio of 2 far over its threshold).
    for ( int i = 0; i < n; ++i )
    {
        const size_t k = static_cast<size_t>( i );
        m_trimGain += ( m_trimTarget - m_trimGain ) * m_trimCoef;
        m_trimGains[i] = m_trimGain;
        L[i] = ( m_drumL[k] + GlueMix * ( L[i] - m_drumL[k] ) ) * m_trimGain;
        R[i] = ( m_drumR[k] + GlueMix * ( R[i] - m_drumR[k] ) ) * m_trimGain;
    }
    // The tone trims (20.10.2026): gliding a block at a time (a hundredth of a dB a block), then the shelves and bells.
    bool toneMoved = false;
    for ( size_t b = 0; b < static_cast<size_t>( ToneBands ); ++b )
    {
        const float gap = m_toneTarget[b] - m_toneNow[b];
        if ( gap == 0.0f ) continue;
        m_toneNow[b] = std::fabs( gap ) < 0.002f
                           ? m_toneTarget[b]
                           : m_toneNow[b] + gap * std::min( 1.0f, m_toneCoef * static_cast<float>( n ) );
        toneMoved = true;
    }
    if ( toneMoved ) m_toneEq.setGains( m_toneNow.data() );
    m_toneEq.process( m_toneState, L, R, n );
    TOT_PROF_END( GLUE );
    if ( !stems ) return;
    // The stems through their own group high pass and tilt, then the glue's and the trim's gains.
    for ( int s = 0; s < STEMS; ++s )
    {
        StemBus &b = m_stemBus[s];
        for ( int c = 0; c < 2; ++c )
            for ( int q = 0; q < 2; ++q ) b.m_hp[c][q].copyCoefficients( m_groupHp[c][q] );
        float *ch[2] = { stemL[s], stemR[s] };
        for ( int c = 0; c < 2; ++c )
        {
            for ( int i = 0; i < n; ++i )
            {
                float x = ch[c][i];
                if ( m_groupHpOn )
                {
                    float lp, bp, hp;
                    b.m_hp[c][0].tick( x, lp, bp, hp );
                    b.m_hp[c][1].tick( hp, lp, bp, x );
                }
                b.m_tilt[c] += m_tiltCoef * ( x - b.m_tilt[c] );
                x = b.m_tilt[c] + m_tiltHigh * ( x - b.m_tilt[c] );
                ch[c][i] = x * ( 1.0f + GlueMix * ( m_glueGains[i] - 1.0f ) ) * m_trimGains[i];
            }
        }
        m_toneEq.process( b.m_tone, stemL[s], stemR[s], n );
    }
}

int Deck::targetOf( PartE part )
{
    switch ( part )
    {
    case PartE::KICK: return perform::keys::KIT;
    case PartE::SUB:
    case PartE::BASS: return perform::keys::BASS;
    case PartE::ACID: return perform::keys::ACID;
    case PartE::PING: return perform::keys::PING;
    case PartE::CHORD: return perform::keys::CHORD;
    case PartE::DRONE: return perform::keys::DRONE;
    default: return laneOf( part ) >= 0 ? perform::keys::KIT : perform::keys::OFF;
    }
}

int Deck::muteOf( PartE part ) const
{
    switch ( part )
    {
    case PartE::KICK: return perform::MUTE_KICK;
    case PartE::SUB: return perform::MUTE_SUB;
    case PartE::BASS:
    case PartE::ACID: return perform::MUTE_BASS;
    case PartE::CHORD:
    case PartE::DRONE:
    case PartE::TEXTURE: return perform::MUTE_PADS;
    case PartE::PING: return perform::MUTE_PING;
    default:
    {
        const int lane = laneOf( part );
        return lane < 0 ? -1 : m_laneIsHat[lane] ? perform::MUTE_HATS : perform::MUTE_PERC;
    }
    }
}

bool Deck::silenced( PartE part ) const
{
    if ( m_composerOff ) return true;
    if ( m_keyTarget == perform::keys::OFF || !m_keyReplace ) return false;
    const int t = targetOf( part );
    if ( t == perform::keys::OFF ) return false;
    // By channel, a part is the player's from its first played key on (until liveAllOff).
    if ( m_keyTarget == perform::keys::BY_CHANNEL ) return ( ( m_keyPlayed >> t ) & 1u ) != 0;
    return m_keyTarget == t;
}

void Deck::liveNote( int64_t sample, int target, int pitch, float velocity, bool on )
{
    if ( !m_loaded || target <= perform::keys::OFF || target >= perform::keys::BY_CHANNEL || pitch < 0 || pitch > 127 )
        return;
    Ev e{};
    e.m_sample = sample;
    e.m_on = on ? 1 : 0;
    e.m_pitch = pitch;
    e.m_velocity = std::clamp( velocity, 0.0f, 1.0f );
    e.m_accent = velocity > 0.86f;   // a hard key is the 303's accent
    e.m_id = 0x40000000 + pitch;     // a key's own id: its release finds its note
    switch ( target )
    {
    case perform::keys::KIT:
        if ( !on ) return;   // one-shots: a release does nothing
        if ( pitch == 36 )
            e.m_part = static_cast<uint8_t>( PartE::KICK );
        else if ( pitch > 36 && pitch <= 36 + PercLanes )
            e.m_part = static_cast<uint8_t>( percPart( pitch - 37 ) );
        else
            return;
        break;
    case perform::keys::BASS: e.m_part = static_cast<uint8_t>( PartE::BASS ); break;
    case perform::keys::ACID: e.m_part = static_cast<uint8_t>( PartE::ACID ); break;
    case perform::keys::PING:
        if ( !on ) return;
        e.m_part = static_cast<uint8_t>( PartE::PING );
        break;
    case perform::keys::CHORD:
        e.m_part = static_cast<uint8_t>( PartE::CHORD );
        m_liveChord[pitch] = on ? 1 : 0;
        break;
    case perform::keys::DRONE: e.m_part = static_cast<uint8_t>( PartE::DRONE ); break;
    default: return;
    }
    if ( target == perform::keys::BASS || target == perform::keys::ACID || target == perform::keys::DRONE )
    {
        // A mono voice: one id for all its keys; a key pressed while another is held slides there, the release of a
        // key that no longer sounds does nothing.
        int &held = m_liveHeld[target];
        e.m_id = 0x40000000;
        if ( on )
        {
            e.m_slide = held != 0;
            held = pitch + 1;
        }
        else if ( held != pitch + 1 )
            return;
        else
            held = 0;
    }
    m_liveEvent = true;
    dispatch( e );
    m_liveEvent = false;
    if ( on ) m_keyPlayed |= 1u << target;
}

void Deck::liveAllOff()
{
    for ( int t : { perform::keys::BASS, perform::keys::ACID, perform::keys::DRONE } )
        if ( m_liveHeld[t] != 0 ) liveNote( 0, t, m_liveHeld[t] - 1, 0.0f, false );
    for ( int k = 0; k < 128; ++k )
        if ( m_liveChord[k] != 0 ) liveNote( 0, perform::keys::CHORD, k, 0.0f, false );
    m_keyPlayed = 0;
}

}   // namespace tot
