/**
 * @file Source.cpp
 * @brief Totality Sets: the render app's source for Totality -- its four styles, each composed into a track or a DJ set
 *        with the dice of the moment, and the sets the player saved (.totset files in Documents/Totality, its
 *        subfolders too), each played back exactly: its seed, its lengths, its rerolls, its knobs.
 *
 * A rendering is what tot_render writes for the same seed and knobs: composed (composeTrack, or composeSet for a set),
 * levelled (levelScore, levelSet), the engine prepared at 48 kHz with blocks of 512 and loaded, two seconds after the
 * end for the rooms. Composing is quick (a set of an hour in a fifth of a second), so the lengths in the table are the
 * composed ones; the levelling renders the loudest part of every track first and takes a few seconds.
 * @author Rene Weller
 */
#include "RenderKit.h"
#include "tot/Engine.h"
#include "tot/FactorySets.h"
#include "tot/Leveler.h"
#include "tot/Params.h"
#include "tot/Preferences.h"
#include "tot/SetFile.h"
#include "tot/compose/Composer.h"
#include "tot/compose/Set.h"
#include "tot/compose/Style.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>

namespace
{

constexpr double Rate = 48000.0;      ///< the files' rate
constexpr int    EngineBlock = 512;   ///< the engine's block (tot_render's)
constexpr double Tail = 2.0;          ///< s after the end, for the rooms (tot_render's)
/** @brief The panel's "What": a track, or a DJ set of so many minutes. */
const double SetMinutes[] = { 0.0, 30.0, 60.0, 90.0, 120.0, 180.0 };
/** @brief The panel's track lengths, minutes (compose.minutes). */
const double TrackMinutes[] = { 5.0, 7.0, 10.0, 13.0, 16.0 };

/** @brief A saved set as the table shows it. */
struct Saved
{
    juce::File    m_file;       ///< the .totset
    std::string   m_style;      ///< its style
    double        m_set = 0;    ///< its set length, minutes (0: a track)
    std::uint64_t m_seed = 1;   ///< its seed
};

/** @brief What a row is: a style, a factory set, or a saved set. */
struct Row
{
    int   m_style = -1;     ///< the style (StyleE), -1 for a set
    int   m_factory = -1;   ///< the factory set (tot/FactorySets.h), -1 for none
    Saved m_saved;          ///< the saved set
};

/**
 * @brief The player's ratings' weights when the knobs ask for them (compose.use_ratings), as the plugin composes with
 *        them: read from Documents/Totality/ratings.tsv.
 * @param[in]  p    the knobs
 * @param[out] use  receives whether they are asked for
 * @return the weights
 */
tot::Preferences preferencesOf( const tot::ParamStore &p, bool &use )
{
    use = p.getBool( p.id( tot::ModuleE::COMPOSE, 0, tot::compose::USE_RATINGS ) );
    if ( !use ) return {};
    const juce::File f = juce::File::getSpecialLocation( juce::File::userDocumentsDirectory )
                             .getChildFile( "Totality" )
                             .getChildFile( "ratings.tsv" );
    return tot::preferencesFrom( tot::loadRatings( f.getFullPathName().toStdString() ) );
}

/**
 * @brief A track's sections on the time line, as Kaleidoscope's cues know them (renderkit::CueSheet): the intro, the
 *        bass's entry a groove, every kick-out a break and its return a drop -- a build eight bars before it --, the
 *        outro; only those between @p from and @p to (a set's track before its swap still belongs to the one before).
 * @param[in,out] sheet     the time line
 * @param[in]     t         what the track's composer told
 * @param[in]     at        the track's first beat in the score
 * @param[in]     tm        the score's tempo map
 * @param[in]     energy    the set's energy where the track begins (0 .. 1)
 * @param[in]     from      s: the first moment that is the track's
 * @param[in]     to        s: the moment it hands over
 * @param[in]     chapters  each section a chapter as well (a track alone)
 */
void trackTimeline( renderkit::CueSheet &sheet, const tot::TrackInfo &t, double at, const tot::TempoMap &tm,
                    double energy, double from, double to, bool chapters )
{
    const auto sec = [&]( int bar ) { return tm.secondsAt( at + 4.0 * bar ); };
    const auto put = [&]( double when, const char *kind, double e, bool drop, const juce::String &title )
    {
        if ( when < from || when >= to ) return;
        sheet.section( when, kind, e, drop );
        if ( chapters ) sheet.chapter( when, title );
    };
    put( sec( 0 ), "Intro", 0.3, false, "Intro" );
    if ( t.m_bassBar > 0 ) put( sec( t.m_bassBar ), "Groove", 0.4 + 0.4 * energy, false, "Bass in" );
    for ( size_t r = 0; r < t.m_reductions.size() && r < t.m_returns.size(); ++r )
    {
        put( sec( t.m_reductions[r] ), "Break", 0.35, false, "Kick out " + juce::String( static_cast<int>( r + 1 ) ) );
        if ( t.m_returns[r] - 8 > t.m_reductions[r] ) put( sec( t.m_returns[r] - 8 ), "Build", 0.6, false, {} );
        put( sec( t.m_returns[r] ), "Drop", 0.6 + 0.4 * energy, true,
             "Return " + juce::String( static_cast<int>( r + 1 ) ) );
    }
    if ( t.m_outroBar > 0 && t.m_outroBar < t.m_bars ) put( sec( t.m_outroBar ), "Outro", 0.3, false, "Outro" );
}

/**
 * @brief A key as the cues name it.
 * @param[in] t  what the track's composer told
 * @return "A Aeolian"
 */
juce::String keyName( const tot::TrackInfo &t )
{
    return juce::String( tot::KeyNames[t.m_key] ) + " " + tot::ScaleNames[t.m_scale];
}

/** @brief A score composed: a track or a set, its length, and what plays when. */
struct Composed
{
    tot::SetScore                                m_set;             ///< the score (a track on deck A)
    bool                                         m_isSet = false;   ///< a DJ set
    double                                       m_seconds = 0.0;   ///< the length, the tail included
    std::vector<std::pair<double, juce::String>> m_marks;           ///< s -> what begins there
};

/**
 * @brief The knobs of a row: a style with the panel's lengths, or a set's own (the factory's, or a saved one).
 * @param[in,out] p     the knobs (fresh)
 * @param[in]     row   the row
 * @param[in]     r     the request
 * @param[in]     seed  the dice for a style (renderkit::seedFor)
 * @param[out]    out   receives the seed to compose with (a saved set's own)
 * @param[out]    cur   receives the rerolls
 * @param[out]    why   receives why a saved set cannot be read
 * @return false when it cannot be set up
 */
bool knobsOf( tot::ParamStore &p, const Row &row, const renderkit::Request &r, std::uint64_t seed, std::uint64_t &out,
              tot::Curation &cur, juce::String &why )
{
    using namespace tot;
    if ( row.m_style >= 0 )
    {
        p.set( p.id( ModuleE::COMPOSE, 0, compose::STYLE ), static_cast<float>( row.m_style ) );
        p.set( p.id( ModuleE::COMPOSE, 0, compose::MINUTES ),
               static_cast<float>( TrackMinutes[std::clamp( r.option( "track", 1 ), 0, 4 )] ) );
        const double minutes = SetMinutes[std::clamp( r.option( "what" ), 0, 5 )];
        p.set( p.id( ModuleE::SET, 0, set::MINUTES ), static_cast<float>( minutes ) );
        p.set( p.id( ModuleE::SET, 0, set::DRAMATURGY ), static_cast<float>( r.option( "dramaturgy", 1 ) ) );
        out = seed;
        return true;
    }
    SetFile     sf;
    std::string error;
    const bool  read = row.m_factory >= 0 ? loadSetText( factorySet( row.m_factory ).m_text, sf, p, &error )
                                          : loadSet( row.m_saved.m_file.getFullPathName().toRawUTF8(), sf, p, &error );
    if ( !read )
    {
        why = error;
        return false;
    }
    if ( sf.m_minutes > 0.0 )
        p.set( p.id( ModuleE::COMPOSE, 0, compose::MINUTES ), static_cast<float>( sf.m_minutes ) );
    // as the plugin's loadSet: the set's length always (0: a track), whatever the knob said when it was saved
    p.set( p.id( ModuleE::SET, 0, set::MINUTES ), static_cast<float>( sf.m_set ) );
    cur = sf.m_curation;
    out = sf.m_seed;
    return true;
}

/**
 * @brief Composes what the knobs ask for, as the plugin does: a set when set.minutes is above 0, else a track.
 * @param[in]  p      the knobs
 * @param[in]  seed   the seed
 * @param[in]  cur    the rerolls
 * @param[out] sheet  receives the time line -- a set's tracks as chapters, a track's sections -- (null: not asked)
 * @return the score, its length and its marks
 */
Composed compose( const tot::ParamStore &p, std::uint64_t seed, const tot::Curation &cur,
                  renderkit::CueSheet *sheet = nullptr )
{
    using namespace tot;
    Composed          out;
    bool              use = false;
    const Preferences prefs = preferencesOf( p, use );
    const double      minutes = p.get( p.id( ModuleE::SET, 0, set::MINUTES ) );
    if ( minutes > 0.0 )
    {
        SetInfo si;
        out.m_set = composeSet( p, seed, minutes, &cur, &si, use ? &prefs : nullptr );
        out.m_isSet = true;
        const TempoMap &tm = out.m_set.m_decks[0].m_tempo;
        out.m_seconds = tm.secondsAt( out.m_set.m_lengthBeats ) + Tail;
        for ( size_t i = 0; sheet != nullptr && i < si.m_tracks.size(); ++i )
        {   // each track from its swap (the first from the start) to the next one's: its key, its sections, a chapter
            const SetTrack &t = si.m_tracks[i];
            const double    from = i == 0 ? 0.0 : tm.secondsAt( t.m_swapIn );
            const double    to = i + 1 < si.m_tracks.size() ? tm.secondsAt( si.m_tracks[i + 1].m_swapIn ) : 1.0e12;
            sheet->key( from, keyName( t.m_info ) );
            sheet->chapter( from, "Track " + juce::String( static_cast<int>( i + 1 ) ) + ": " +
                                      juce::String( t.m_info.m_style ) + ", " +
                                      juce::String( std::lround( t.m_info.m_bpm ) ) + " BPM, " + keyName( t.m_info ) );
            if ( i > 0 ) sheet->section( from, "Groove", 0.4 + 0.4 * t.m_energy );
            trackTimeline( *sheet, t.m_info, t.m_start, tm, t.m_energy, from, to, false );
        }
        if ( sheet != nullptr ) sheet->grid( [&tm]( double b ) { return tm.secondsAt( b ); }, out.m_set.m_lengthBeats );
        for ( size_t i = 0; i < si.m_tracks.size(); ++i )
        {
            const SetTrack &t = si.m_tracks[i];
            out.m_marks.push_back( { tm.secondsAt( t.m_start ),
                                     "track " + juce::String( static_cast<int>( i + 1 ) ) + " of " +
                                         juce::String( static_cast<int>( si.m_tracks.size() ) ) + ": " +
                                         juce::String( t.m_info.m_style ) + ", " + juce::String( t.m_info.m_bpm, 1 ) +
                                         " BPM, " + KeyNames[t.m_info.m_key] + " " + ScaleNames[t.m_info.m_scale] } );
        }
    }
    else
    {
        TrackInfo    info;
        Score       &score = out.m_set.m_decks[0];
        TrackRequest req;
        req.m_prefs = use ? &prefs : nullptr;
        score = composeTrack( p, seed, req, &cur, std::string(), &info );
        out.m_seconds = score.m_tempo.secondsAt( score.m_lengthBeats ) + Tail;
        if ( sheet != nullptr )
        {   // a track alone: its sections as chapters
            sheet->key( 0.0, keyName( info ) );
            trackTimeline( *sheet, info, 0.0, score.m_tempo, 0.7, 0.0, 1.0e12, true );
            const TempoMap &tm = score.m_tempo;
            sheet->grid( [&tm]( double b ) { return tm.secondsAt( b ); }, score.m_lengthBeats );
        }
        out.m_marks.push_back( { 0.0, juce::String( info.m_style ) + ", " + juce::String( info.m_bpm, 1 ) + " BPM, " +
                                          KeyNames[info.m_key] + " " + ScaleNames[info.m_scale] + " (" +
                                          juce::String( info.m_camelot ) + ")" } );
    }
    return out;
}

/** @brief A track or a set being rendered: the engine offline, levelled first. */
class TotalityRendering : public renderkit::Rendering
{
public:
    /**
     * @brief Composes, levels and loads.
     * @param[in]  row   the row
     * @param[in]  r     the request
     * @param[in]  seed  the dice for a style (renderkit::seedFor)
     * @param[out] why   receives why it cannot be rendered
     */
    TotalityRendering( const Row &row, const renderkit::Request &r, std::uint64_t seed, juce::String &why )
    {
        using namespace tot;
        m_engine = std::make_unique<Engine>();
        ParamStore   &p = m_engine->params();
        Curation      cur;
        std::uint64_t s = seed;
        if ( !knobsOf( p, row, r, seed, s, cur, why ) )
        {
            m_engine.reset();
            return;
        }
        Composed c = compose( p, s, cur, &m_sheet );
        // The Leveler: the loudest part of every track to the style's target, before a sample is written (tot_render).
        if ( c.m_isSet )
            levelSet( c.m_set, p );
        else
            levelScore( c.m_set.m_decks[0], p );
        m_engine->prepare( Rate, EngineBlock );
        if ( c.m_isSet )
            m_engine->loadSet( c.m_set );
        else
            m_engine->load( c.m_set.m_decks[0] );
        m_seconds = m_engine->lengthSeconds() + Tail;
        m_marks = std::move( c.m_marks );
    }
    /** @brief Whether it could be set up. @return true when it renders */
    bool ok() const { return m_engine != nullptr; }
    double seconds() const override { return m_seconds; }
    void render( float *left, float *right, int frames ) override
    {
        for ( int done = 0; done < frames; )
        {   // in the engine's own blocks, as tot_render renders
            const int n = std::min( EngineBlock, frames - done );
            m_engine->process( left + done, right + done, n );
            done += n;
        }
        const std::lock_guard<std::mutex> hold( m_lock );
        m_time += frames / Rate;
    }
    juce::String now() const override
    {
        const std::lock_guard<std::mutex> hold( m_lock );
        juce::String                      s;
        for ( const auto &[at, what] : m_marks )
            if ( at <= m_time ) s = what;
        return s;
    }
    void timeline( renderkit::CueSheet &sheet ) const override { sheet = m_sheet; }

private:
    renderkit::CueSheet                          m_sheet;           ///< its time line, as composed
    std::unique_ptr<tot::Engine>                 m_engine;          ///< the instrument offline
    double                                       m_seconds = 0.0;   ///< the length
    std::vector<std::pair<double, juce::String>> m_marks;           ///< s -> what begins there
    mutable std::mutex                           m_lock;            ///< guards m_time
    double                                       m_time = 0.0;      ///< s rendered
};

/** @brief Totality's styles and saved sets for the render app. */
class TotalitySource : public renderkit::Source
{
public:
    juce::String product() const override { return "Totality"; }
    juce::String title() const override { return "Totality Sets"; }
    juce::String subtitle() const override { return "Render tracks and DJ sets into files, faster than real time"; }
    juce::String noun() const override { return "set"; }
    renderkit::Palette palette() const override
    {
        renderkit::Palette c;
        c.m_back = juce::Colour( 0xff08080a );
        c.m_panel = juce::Colour( 0xff141519 );
        c.m_row = juce::Colour( 0xff0f1013 );
        c.m_raised = juce::Colour( 0xff1e1f24 );
        c.m_edge = juce::Colour( 0xff2c2d33 );
        c.m_chosen = juce::Colour( 0xff3a2c29 );
        c.m_text = juce::Colour( 0xffe8e5de );
        c.m_dim = juce::Colour( 0xff8c8b91 );
        c.m_accent = juce::Colour( 0xffeae3d2 );
        return c;
    }
    std::vector<renderkit::Column> columns() const override
    {
        return { { "Kind", 90, false }, { "Style", 110, false }, { "Tempo", 110, true } };
    }
    std::vector<renderkit::Item> list() override
    {
        using namespace tot;
        std::vector<renderkit::Item> out;
        auto                         add = [&out, this]( renderkit::Item &&item, Row &&row )
        {
            const std::lock_guard<std::mutex> hold( m_lock );
            item.m_index = static_cast<int>( m_store.size() );
            m_store.push_back( std::make_shared<const Row>( std::move( row ) ) );
            out.push_back( std::move( item ) );
        };
        for ( int s = 0; s < static_cast<int>( StyleE::COUNT ); ++s )
        {
            const StyleProfile &prof = styleProfile( static_cast<StyleE>( s ) );
            renderkit::Item     item;
            item.m_name = StyleNames[s];
            item.m_kind = "Style";
            item.m_cells.add( "Style" );
            item.m_cells.add( StyleNames[s] );
            item.m_cells.add( juce::String( prof.m_bpmLow, 0 ) + " .. " + juce::String( prof.m_bpmHigh, 0 ) + " BPM" );
            item.m_keys = { std::nan( "" ), std::nan( "" ), static_cast<double>( prof.m_bpmLow ) };
            Row row;
            row.m_style = s;
            add( std::move( item ), std::move( row ) );
        }
        // the factory's DJ sets (tot/FactorySets.h): played as they are, the dice their own
        for ( int i = 0; i < numFactorySets(); ++i )
        {
            const FactorySet &f = factorySet( i );
            renderkit::Item   item;
            item.m_name = f.m_name;
            item.m_kind = "Factory set";
            item.m_dice = false;
            item.m_cells.add( juce::String( f.m_group ) + " set" );
            item.m_cells.add( f.m_section );
            item.m_cells.add( {} );
            item.m_keys = { std::nan( "" ), std::nan( "" ), std::nan( "" ) };
            item.m_detail = juce::String( f.m_note ) + " " + f.m_group + " " + f.m_section;
            Row row;
            row.m_factory = i;
            add( std::move( item ), std::move( row ) );
        }
        const juce::File docs =
            juce::File::getSpecialLocation( juce::File::userDocumentsDirectory ).getChildFile( "Totality" );
        juce::Array<juce::File> files = docs.findChildFiles( juce::File::findFiles, true, "*.totset" );
        files.sort();
        for ( const juce::File &f : files )
        {
            ParamStore  p;
            SetFile     sf;
            std::string error;
            if ( !loadSet( f.getFullPathName().toRawUTF8(), sf, p, &error ) ) continue;
            Row row;
            row.m_saved.m_file = f;
            row.m_saved.m_style =
                StyleNames[std::clamp( p.getInt( p.id( ModuleE::COMPOSE, 0, compose::STYLE ) ), 0, 3 )];
            row.m_saved.m_set = sf.m_set;
            row.m_saved.m_seed = sf.m_seed;
            renderkit::Item item;
            item.m_name = f.getFileNameWithoutExtension();
            item.m_kind = "Yours";
            item.m_dice = false;
            item.m_cells.add( row.m_saved.m_set > 0.0 ? "Your DJ set" : "Your track" );
            item.m_cells.add( juce::String( row.m_saved.m_style ) );
            item.m_cells.add( {} );
            item.m_keys = { std::nan( "" ), std::nan( "" ), std::nan( "" ) };
            item.m_detail = f.getRelativePathFrom( docs );
            add( std::move( item ), std::move( row ) );
        }
        return out;
    }
    std::vector<renderkit::Option> options() const override
    {
        renderkit::Option dram{
            "dramaturgy", "Arc", "A DJ set's arc: how its energy goes over the night.", {}, 1, true
        };
        for ( const char *n : { "Warm-up", "Peak", "Closing", "Sunday", "Flat", "Cruise", "Marathon" } )
            dram.m_choices.add( n );
        return { { "what",
                   "What",
                   "A single track, or a DJ set: tracks on two decks, blended, with the DJ's hand.",
                   { "A track", "DJ set, 30 min", "DJ set, 1 hour", "DJ set, 90 min", "DJ set, 2 hours",
                     "DJ set, 3 hours" },
                   0,
                   true },
                 { "track",
                   "Track",
                   "A single track's length (compose.minutes); a set's tracks take their own time.",
                   { "5 min", "7 min", "10 min", "13 min", "16 min" },
                   1,
                   true },
                 dram };
    }
    bool applies( const renderkit::Item &item, const juce::String &key ) const override
    {
        juce::ignoreUnused( key );
        return row( item )->m_style >= 0;   // a saved set brings its own
    }
    double length( const renderkit::Item &item, const renderkit::Request &r ) override
    {
        if ( const int f = row( item )->m_factory; f >= 0 && tot::factorySet( f ).m_seconds > 0.0 )
            return tot::factorySet( f ).m_seconds;   // measured when the bank was made (Tools/sets/make_sets.py)
        tot::ParamStore p;
        tot::Curation   cur;
        std::uint64_t   seed = 1;
        juce::String    why;
        if ( !knobsOf( p, *row( item ), r, renderkit::seedFor( item, r ), seed, cur, why ) ) return 0.0;
        return compose( p, seed, cur ).m_seconds;
    }
    juce::String describe( const renderkit::Item &item, const renderkit::Request &r ) override
    {
        const std::shared_ptr<const Row> rw = row( item );
        if ( rw->m_factory >= 0 )
        {
            const tot::FactorySet &f = tot::factorySet( rw->m_factory );
            return juce::String( "A DJ set of the factory: " ) + f.m_group + ", from " + f.m_section + "\n" + f.m_note;
        }
        if ( rw->m_style < 0 )
        {
            const Saved &s = rw->m_saved;
            return juce::String( "Your " ) +
                   ( s.m_set > 0.0 ? "DJ set of " + juce::String( s.m_set, 0 ) + " min" : "track" ) + ", " +
                   juce::String( s.m_style ) + "\nseed " + juce::String( static_cast<juce::int64>( s.m_seed ) ) +
                   ", played as you saved it";
        }
        const tot::StyleProfile &prof = tot::styleProfile( static_cast<tot::StyleE>( rw->m_style ) );
        const int                what = std::clamp( r.option( "what" ), 0, 5 );
        juce::String s = juce::String( tot::StyleNames[rw->m_style] ) + ", " + juce::String( prof.m_bpmLow, 0 ) +
                         " .. " + juce::String( prof.m_bpmHigh, 0 ) + " BPM\n";
        if ( what == 0 )
            s << "A track of about " << juce::String( TrackMinutes[std::clamp( r.option( "track", 1 ), 0, 4 )], 0 )
              << " minutes";
        else
            s << "A DJ set of " << juce::String( SetMinutes[what], 0 ) << " minutes, from "
              << tot::StyleNames[rw->m_style];
        return s;
    }
    std::unique_ptr<renderkit::Rendering> open( const renderkit::Item &item, const renderkit::Request &r,
                                                juce::String &why ) override
    {
        auto rendering = std::make_unique<TotalityRendering>( *row( item ), r, renderkit::seedFor( item, r ), why );
        if ( !rendering->ok() ) return nullptr;
        return rendering;
    }
    juce::String testItem() const override { return "Hypnotic"; }

private:
    /**
     * @brief A row's style or saved set.
     * @param[in] item  the row
     * @return it (shared: a rendering may outlive a Refresh)
     */
    std::shared_ptr<const Row> row( const renderkit::Item &item ) const
    {
        const std::lock_guard<std::mutex> hold( m_lock );
        return m_store[static_cast<size_t>( item.m_index )];
    }
    mutable std::mutex                      m_lock;    ///< guards m_store
    std::vector<std::shared_ptr<const Row>> m_store;   ///< every row listed (only ever added to)
};

}   // namespace

std::unique_ptr<renderkit::Source> renderkit::makeSource()
{
    return std::make_unique<TotalitySource>();
}
