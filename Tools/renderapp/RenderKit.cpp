/**
 * @file RenderKit.cpp
 * @brief The render app's files, renderer, queue, command line, self-test and application (RenderKit.h).
 * @author Rene Weller
 */
#include "RenderKit.h"

#include "Mp3Writer.h"
#include "RenderWindow.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace renderkit
{

namespace
{

constexpr int Block = 4096;   ///< samples a call of the rendering

/** @brief Where the samples go: a JUCE writer behind a background thread, or the MP3 writer. */
class Sink
{
public:
    /**
     * @brief Opens the file.
     * @param[in]  format      the format
     * @param[in]  file        the file
     * @param[in]  sampleRate  samples a second
     * @param[out] why         receives why it failed
     * @return true when it is open
     */
    bool open( FormatE format, const juce::File &file, int sampleRate, juce::String &why )
    {
        file.deleteFile();
        file.getParentDirectory().createDirectory();
        if ( format == FormatE::MP3 )
        {
            std::string w;
            m_mp3 = std::make_unique<Mp3Writer>();
            if ( !m_mp3->open( file.getFullPathName().toStdString(), sampleRate, 320, w ) )
            {
                why = w;
                m_mp3.reset();
                return false;
            }
            return true;
        }
        std::unique_ptr<juce::AudioFormat> fmt;
        int                                bits = 24, quality = 0;
        if ( format == FormatE::WAV ) fmt = std::make_unique<juce::WavAudioFormat>();
        if ( format == FormatE::FLAC )
        {
            fmt = std::make_unique<juce::FlacAudioFormat>();
            quality = 5;
        }
        if ( format == FormatE::OGG )
        {
            fmt = std::make_unique<juce::OggVorbisAudioFormat>();
            bits = 16;
            quality = std::max( 0, fmt->getQualityOptions().indexOf( "320 kbps" ) );
        }
        if ( !fmt )
        {
            why = "no such format";
            return false;
        }
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>( file );
        if ( !static_cast<juce::FileOutputStream *>( stream.get() )->openedOk() )
        {
            why = "cannot write " + file.getFullPathName();
            return false;
        }
        const auto                               options = juce::AudioFormatWriterOptions{}
                                                               .withSampleRate( sampleRate )
                                                               .withNumChannels( 2 )
                                                               .withBitsPerSample( bits )
                                                               .withQualityOptionIndex( quality );
        std::unique_ptr<juce::AudioFormatWriter> writer = fmt->createWriterFor( stream, options );
        if ( !writer )
        {
            why = juce::String( "no " ) + formatName( format ) + " writer for this file";
            return false;
        }
        m_thread.startThread();
        // encoding beside the rendering: eight seconds of room before write() has to wait
        m_writer =
            std::make_unique<juce::AudioFormatWriter::ThreadedWriter>( writer.release(), m_thread, 8 * sampleRate );
        return true;
    }
    /**
     * @brief Appends samples.
     * @param[in] L  left
     * @param[in] R  right
     * @param[in] n  samples
     * @return false when the file refused them
     */
    bool write( const float *L, const float *R, int n )
    {
        if ( m_mp3 ) return m_mp3->write( L, R, n );
        const float *channels[2] = { L, R };
        for ( int tries = 0; !m_writer->write( channels, n ); ++tries )
        {   // the encoder behind: give it a moment
            if ( tries > 20000 ) return false;
            juce::Thread::sleep( 1 );
        }
        return true;
    }
    /**
     * @brief Finishes the file.
     * @return true when it was finished cleanly
     */
    bool close()
    {
        bool ok = true;
        if ( m_mp3 ) ok = m_mp3->close();
        m_mp3.reset();
        m_writer.reset();   // flushes what is left and closes the file
        m_thread.stopThread( 10000 );
        return ok;
    }

private:
    std::unique_ptr<Mp3Writer>                               m_mp3;   ///< the MP3 writer, or null
    juce::TimeSliceThread                                    m_thread{ "render app writer" };   ///< encodes
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> m_writer;   ///< the JUCE writer behind it, or null
};

/**
 * @brief The command line's value after a flag.
 * @param[in] args  the arguments
 * @param[in] flag  the flag
 * @param[in] dflt  when it is not there
 * @return the value
 */
juce::String argument( const juce::StringArray &args, const juce::String &flag, const juce::String &dflt )
{
    const int i = args.indexOf( flag );
    return i >= 0 && i + 1 < args.size() ? args[i + 1] : dflt;
}

/**
 * @brief The request of the panel's first state: the dice of the command line, every option at its default, or as
 *        the command line names it (--KEY VALUE: a menu's entry by its text or its number, a toggle by on/off/1/0).
 * @param[in] source  the instrument
 * @param[in] args    the command line
 * @return the request
 */
Request requestFrom( const Source &source, const juce::StringArray &args )
{
    Request r;
    r.m_seed = static_cast<std::uint64_t>( argument( args, "--seed", "1" ).getLargeIntValue() );
    r.m_limit = argument( args, "--seconds", "0" ).getDoubleValue();
    for ( const Option &op : source.options() )
    {
        int                value = op.m_default;
        const juce::String v = argument( args, "--" + op.m_key, {} );
        if ( v.isNotEmpty() )
        {
            if ( op.m_choices.isEmpty() )
                value = v == "1" || v.equalsIgnoreCase( "on" ) || v.equalsIgnoreCase( "true" ) ||
                        v.equalsIgnoreCase( "yes" );
            else
            {
                const int k = op.m_choices.indexOf( v, true );
                value = k >= 0 ? k : std::clamp( v.getIntValue(), 0, op.m_choices.size() - 1 );
            }
        }
        r.m_options[op.m_key] = value;
    }
    return r;
}

/**
 * @brief The format a command line names.
 * @param[in] args  the command line
 * @return it (WAV when it names none)
 */
FormatE formatFrom( const juce::StringArray &args )
{
    const juce::String f = argument( args, "--format", "wav" ).toLowerCase();
    return f == "flac" ? FormatE::FLAC : f == "ogg" ? FormatE::OGG : f == "mp3" ? FormatE::MP3 : FormatE::WAV;
}

/**
 * @brief --list: every row, its kind and the length of its rendering.
 * @param[in,out] source  the instrument
 * @param[in]     args    the command line (the dice, the options)
 * @return 0
 */
int listCommand( Source &source, const juce::StringArray &args )
{
    const Request r = requestFrom( source, args );
    for ( const Item &it : source.list() )
        std::printf( "%-48s %-14s %10s\n", it.m_name.toRawUTF8(), it.m_kind.toRawUTF8(),
                     clockText( source.length( it, r ) ).toRawUTF8() );
    return 0;
}

/**
 * @brief --render: one row without the window, its progress printed.
 * @param[in,out] source  the instrument
 * @param[in]     args    the command line
 * @return 0 when it was written
 */
int renderCommand( Source &source, const juce::StringArray &args )
{
    const juce::String name = argument( args, "--render", {} );
    for ( const Item &item : source.list() )
    {
        if ( item.m_name != name ) continue;
        Job job;
        job.m_item = item;
        job.m_request = requestFrom( source, args );
        job.m_format = formatFrom( args );
        if ( !formatAvailable( job.m_format ) )
        {
            std::printf( "%s is not written on this system\n", formatName( job.m_format ) );
            return 1;
        }
        const juce::String out = argument( args, "--out", {} );
        if ( out.isNotEmpty() )
            job.m_file = juce::File::getCurrentWorkingDirectory().getChildFile( out );
        else
            job.m_folder = juce::File::getCurrentWorkingDirectory();
        std::printf( "%s: %s\n", name.toRawUTF8(), clockText( source.length( item, job.m_request ) ).toRawUTF8() );
        Renderer     r;
        juce::File   file;
        juce::String why;
        const bool   ok = r.render( source, job, [] { return false; }, file, why );
        if ( ok )
            std::printf( "%s written into %s, %.1fx real time\n", clockText( r.m_total.load() ).toRawUTF8(),
                         file.getFullPathName().toRawUTF8(), r.m_speed.load() );
        else
            std::printf( "failed: %s\n", why.toRawUTF8() );
        return ok ? 0 : 1;
    }
    std::printf( "no %s named %s (--list names them)\n", source.noun().toRawUTF8(), name.toRawUTF8() );
    return 1;
}

/**
 * @brief The self-test's row: the source's, else the first.
 * @param[in,out] source  the instrument
 * @param[out]    item    receives it
 * @return false when there is none
 */
bool testItem( Source &source, Item &item )
{
    const std::vector<Item> items = source.list();
    for ( const Item &it : items )
        if ( it.m_name == source.testItem() )
        {
            item = it;
            return true;
        }
    if ( items.empty() ) return false;
    item = items.front();
    return true;
}

/**
 * @brief Every format: a few seconds of the test row written, read back, its length and level checked.
 * @param[in,out] source   the instrument
 * @param[in]     seconds  how long each file
 * @return the number of formats that failed
 */
int formatsTest( Source &source, double seconds )
{
    Item item;
    if ( !testItem( source, item ) )
    {
        std::printf( "  FAIL  nothing to render\n" );
        return 1;
    }
    const juce::File dir =
        juce::File::getSpecialLocation( juce::File::tempDirectory ).getChildFile( source.product() + "RenderAppTest" );
    dir.deleteRecursively();
    dir.createDirectory();
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    int failures = 0;
    std::printf( "%s: %s, the test renders %s\n", source.title().toRawUTF8(), source.noun().toRawUTF8(),
                 item.m_name.toRawUTF8() );
    for ( int f = 0; f < static_cast<int>( FormatE::COUNT ); ++f )
    {
        if ( !formatAvailable( static_cast<FormatE>( f ) ) ) continue;
        Job job;
        job.m_item = item;
        job.m_request = requestFrom( source, {} );
        job.m_request.m_limit = seconds;
        job.m_format = static_cast<FormatE>( f );
        job.m_file = dir.getChildFile( juce::String( "test" ) + formatExtension( job.m_format ) );
        Renderer     r;
        juce::File   file;
        juce::String why;
        const bool   rendered = r.render( source, job, [] { return false; }, file, why );
        double       length = 0.0, db = -200.0;
        if ( rendered )
            if ( std::unique_ptr<juce::AudioFormatReader> reader{ formats.createReaderFor( file ) } )
            {
                length = static_cast<double>( reader->lengthInSamples ) / reader->sampleRate;
                juce::AudioBuffer<float> buf( 2, static_cast<int>( reader->lengthInSamples ) );
                reader->read( &buf, 0, buf.getNumSamples(), 0, true, true );
                const double rms = 0.5 * ( buf.getRMSLevel( 0, 0, buf.getNumSamples() ) +
                                           buf.getRMSLevel( 1, 0, buf.getNumSamples() ) );
                db = 20.0 * std::log10( std::max( rms, 1e-10 ) );
            }
        const bool ok = rendered && std::abs( length - seconds ) < 0.5 && db > -70.0;
        if ( !ok ) ++failures;
        std::printf( "  %s  %-26s %5.2f s, %6.1f dBFS, %.1fx real time%s%s\n", ok ? "ok  " : "FAIL",
                     formatName( job.m_format ), length, db, r.m_speed.load(), why.isEmpty() ? "" : ": ",
                     why.toRawUTF8() );
    }
    dir.deleteRecursively();
    return failures;
}

/**
 * @brief The window's queue as the window drives it: two renderings one after the other, then one cancelled under way
 *        (its unfinished file removed).
 * @param[in,out] source  the instrument
 * @return 0 when the queue did all three right
 */
int queueTest( Source &source )
{
    Item item;
    if ( !testItem( source, item ) ) return 1;
    const juce::File dir =
        juce::File::getSpecialLocation( juce::File::tempDirectory ).getChildFile( source.product() + "RenderAppQueue" );
    dir.deleteRecursively();
    dir.createDirectory();
    std::vector<Job> jobs( 2 );
    for ( size_t k = 0; k < jobs.size(); ++k )
    {
        jobs[k].m_item = item;
        jobs[k].m_request = requestFrom( source, {} );
        jobs[k].m_request.m_limit = 3.0;
        jobs[k].m_file = dir.getChildFile( "q" + juce::String( static_cast<int>( k ) ) + ".wav" );
    }
    RenderQueue  q( source );
    juce::String name, message;
    int          waiting = 0, finished = 0;
    q.add( jobs );
    for ( int t = 0; t < 2400 && ( q.status( name, waiting, finished ) || waiting > 0 || finished < 2 ); ++t )
        juce::Thread::sleep( 50 );
    message = q.takeMessage();
    const bool both = finished == 2 && jobs[0].m_file.existsAsFile() && jobs[1].m_file.existsAsFile();
    Job        long_ = jobs[0];
    long_.m_request.m_limit = 600.0;
    long_.m_file = dir.getChildFile( "cancelled.wav" );
    q.add( { long_ } );
    for ( int t = 0; t < 2400 && ( q.m_renderer.m_total.load() < 100.0 || q.m_renderer.m_done.load() < 1.0 ); ++t )
        juce::Thread::sleep( 25 );   // until the long one is a second in
    q.cancelAll();
    for ( int t = 0; t < 400 && q.status( name, waiting, finished ); ++t ) juce::Thread::sleep( 25 );
    const juce::String last = q.takeMessage();
    const bool         cancelled = !q.status( name, waiting, finished ) && last.endsWith( "cancelled" ) &&
                                   !long_.m_file.existsAsFile() && finished == 2;
    std::printf( "  %s  queue: two after each other (%s), one cancelled under way (%s)\n",
                 both && cancelled ? "ok  " : "FAIL", message.toRawUTF8(), last.toRawUTF8() );
    dir.deleteRecursively();
    return both && cancelled ? 0 : 1;
}

/** @brief The window. */
class AppWindow : public juce::DocumentWindow
{
public:
    /**
     * @brief The window for an instrument.
     * @param[in,out] source  the instrument
     */
    explicit AppWindow( Source &source )
        : juce::DocumentWindow( source.title(), source.palette().m_back, juce::DocumentWindow::allButtons )
    {
        setUsingNativeTitleBar( true );
        setContentOwned( new RenderWindow( source ), true );
        setResizable( true, true );
        setResizeLimits( 960, 680, 4000, 3000 );
        centreWithSize( getWidth(), getHeight() );
        setVisible( true );
    }
    /** @brief The close button quits. */
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

/** @brief The application: the window, or a command line's work. */
class RenderApplication : public juce::JUCEApplication
{
public:
    /** @brief Its name. @return the instrument's title */
    const juce::String getApplicationName() override { return source().title(); }
    /** @brief Its version. @return the instrument's */
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    /**
     * @brief Several at once: a command line's rendering beside an open window (else it would hand its arguments to
     *        the window and quit).
     * @return true
     */
    bool moreThanOneInstanceAllowed() override { return true; }
    /**
     * @brief Opens the window, or does what the command line asks and quits.
     * @param[in] commandLine  the arguments
     */
    void initialise( const juce::String &commandLine ) override
    {
        juce::StringArray args = juce::StringArray::fromTokens( commandLine, true );
        for ( juce::String &a : args ) a = a.unquoted();   // a name with spaces comes in its quotes
        if ( args.contains( "--selftest" ) )
        {
            const double seconds = argument( args, "--selftest", "6" ).getDoubleValue();
            const int    formats = formatsTest( source(), seconds > 0.0 ? seconds : 6.0 );
            const int    queue = queueTest( source() );
            std::printf( formats + queue == 0 ? "render app passed\n" : "render app: %d FAILED\n", formats + queue );
            setApplicationReturnValue( formats + queue == 0 ? 0 : 1 );
            quit();
            return;
        }
        if ( args.contains( "--list" ) )
        {
            setApplicationReturnValue( listCommand( source(), args ) );
            quit();
            return;
        }
        if ( args.contains( "--render" ) )
        {
            setApplicationReturnValue( renderCommand( source(), args ) );
            quit();
            return;
        }
        if ( args.contains( "--shot" ) )
        {   // the window as a picture, a row chosen (the manual's)
            RenderWindow w( source() );
            w.choose( argument( args, "--choose", {} ) );
            w.settle();
            const juce::Image shot = w.createComponentSnapshot( w.getLocalBounds() );
            juce::File        out =
                juce::File::getCurrentWorkingDirectory().getChildFile( argument( args, "--shot", "app.png" ) );
            out.deleteFile();
            juce::FileOutputStream s( out );
            juce::PNGImageFormat   png;
            setApplicationReturnValue( s.openedOk() && png.writeImageToStream( shot, s ) ? 0 : 1 );
            quit();
            return;
        }
        m_window = std::make_unique<AppWindow>( source() );
    }
    /** @brief Closes the window. */
    void shutdown() override
    {
        m_window.reset();
        m_source.reset();
    }
    /** @brief Quits. */
    void systemRequestedQuit() override { quit(); }

private:
    /** @brief The instrument, made the first time it is asked for. @return it */
    Source &source()
    {
        if ( !m_source ) m_source = makeSource();
        return *m_source;
    }
    std::unique_ptr<Source>    m_source;   ///< the instrument
    std::unique_ptr<AppWindow> m_window;   ///< the window, when there is one
};

}   // namespace

const char *formatName( FormatE f )
{
    switch ( f )
    {
    case FormatE::WAV: return "WAV (24 bit)";
    case FormatE::FLAC: return "FLAC (24 bit, lossless)";
    case FormatE::OGG: return "Ogg Vorbis (320 kbit/s)";
    case FormatE::MP3: return "MP3 (320 kbit/s)";
    default: return "?";
    }
}

const char *formatExtension( FormatE f )
{
    switch ( f )
    {
    case FormatE::WAV: return ".wav";
    case FormatE::FLAC: return ".flac";
    case FormatE::OGG: return ".ogg";
    case FormatE::MP3: return ".mp3";
    default: return ".wav";
    }
}

bool formatAvailable( FormatE f )
{
    return f != FormatE::COUNT && ( f != FormatE::MP3 || Mp3Writer::available() );
}

juce::String clockText( double seconds )
{
    const long long s = static_cast<long long>( std::llround( std::max( seconds, 0.0 ) ) );
    const long long h = s / 3600, m = ( s / 60 ) % 60, r = s % 60;
    return h > 0 ? juce::String::formatted( "%lld:%02lld:%02lld", h, m, r )
                 : juce::String::formatted( "%lld:%02lld", m, r );
}

std::uint64_t seedFor( const Item &item, const Request &r )
{
    std::uint64_t h = 1469598103934665603ull;
    for ( const char *c = item.m_name.toRawUTF8(); *c != 0; ++c )
        h = ( h ^ static_cast<unsigned char>( *c ) ) * 1099511628211ull;
    const std::uint64_t s = ( r.m_seed * 0x9E3779B97F4A7C15ull ) ^ h;
    return s != 0 ? s : 1;
}

juce::String fileNameFor( const juce::String &name, double seconds, FormatE format )
{
    return juce::File::createLegalFileName( name + " (" + clockText( seconds ).replace( ":", "-" ) + ")" ) +
           formatExtension( format );
}

// ---- the renderer

juce::String Renderer::now() const
{
    const std::lock_guard<std::mutex> hold( m_lock );
    return m_now;
}

bool Renderer::render( Source &source, const Job &job, const std::function<bool()> &cancelled, juce::File &file,
                       juce::String &why )
{
    m_done.store( 0.0 );
    m_total.store( 0.0 );
    m_speed.store( 0.0 );
    m_setup.store( true );
    {
        const std::lock_guard<std::mutex> hold( m_lock );
        m_now.clear();
    }
    std::unique_ptr<Rendering> rendering = source.open( job.m_item, job.m_request, why );
    m_setup.store( false );
    if ( !rendering )
    {
        if ( why.isEmpty() ) why = "it cannot be rendered";
        return false;
    }
    const double whole = rendering->seconds();
    const double total = job.m_request.m_limit > 0.0 ? std::min( job.m_request.m_limit, whole ) : whole;
    if ( total <= 0.0 )
    {
        why = "it lasts no time";
        return false;
    }
    m_total.store( total );
    file = job.m_file != juce::File()
               ? job.m_file
               : job.m_folder.getChildFile( fileNameFor( job.m_item.m_name, total, job.m_format ) )
                     .getNonexistentSibling( false );
    const double rate = source.sampleRate();
    Sink         sink;
    if ( !sink.open( job.m_format, file, source.sampleRate(), why ) ) return false;
    const long long frames = static_cast<long long>( std::llround( total * rate ) );
    const long long fadeFrames =
        static_cast<long long>( std::max( 0.0, std::min( rendering->fadeSeconds(), total * 0.5 ) ) * rate );
    std::vector<float> L( Block ), R( Block );
    const auto         t0 = std::chrono::steady_clock::now();
    auto               told = t0;
    long long          done = 0;
    bool               ok = true;
    while ( done < frames )
    {
        if ( cancelled() )
        {
            why = "cancelled";
            ok = false;
            break;
        }
        const int n = static_cast<int>( std::min<long long>( Block, frames - done ) );
        rendering->render( L.data(), R.data(), n );
        for ( int i = 0; i < n; ++i )
        {   // nothing that is not a number reaches a file
            if ( !std::isfinite( L[static_cast<size_t>( i )] ) ) L[static_cast<size_t>( i )] = 0.0f;
            if ( !std::isfinite( R[static_cast<size_t>( i )] ) ) R[static_cast<size_t>( i )] = 0.0f;
        }
        if ( done + n > frames - fadeFrames )
            for ( int i = 0; i < n; ++i )
            {   // the end faded out on a raised cosine
                const double u = std::clamp( static_cast<double>( frames - ( done + i ) ) /
                                                 static_cast<double>( std::max<long long>( fadeFrames, 1 ) ),
                                             0.0, 1.0 );
                const float  g = static_cast<float>( 0.5 - 0.5 * std::cos( 3.14159265358979 * u ) );
                L[static_cast<size_t>( i )] *= g;
                R[static_cast<size_t>( i )] *= g;
            }
        if ( !sink.write( L.data(), R.data(), n ) )
        {
            why = "the file could not be written (is the disk full?)";
            ok = false;
            break;
        }
        done += n;
        const auto   t = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double>( t - t0 ).count();
        m_done.store( static_cast<double>( done ) / rate );
        m_speed.store( elapsed > 0.0 ? static_cast<double>( done ) / rate / elapsed : 0.0 );
        if ( std::chrono::duration<double>( t - told ).count() > 0.25 )
        {   // what plays now, four times a second
            told = t;
            const juce::String                now = rendering->now();
            const std::lock_guard<std::mutex> hold( m_lock );
            m_now = now;
        }
    }
    if ( !sink.close() && ok )
    {
        why = "the file could not be finished";
        ok = false;
    }
    return ok;
}

// ---- the queue

RenderQueue::RenderQueue( Source &source ) : juce::Thread( "render app", 64u << 20 ), m_source( source ) {}

RenderQueue::~RenderQueue()
{
    cancelAll();
    signalThreadShouldExit();
    m_wake.signal();
    stopThread( 60000 );
}

void RenderQueue::add( const std::vector<Job> &jobs )
{
    {
        const std::lock_guard<std::mutex> hold( m_lock );
        for ( const Job &j : jobs ) m_jobs.push_back( j );
    }
    if ( !isThreadRunning() ) startThread();   // once: it then waits for work until the window closes
    m_wake.signal();
}

void RenderQueue::cancelAll()
{
    const std::lock_guard<std::mutex> hold( m_lock );
    m_jobs.clear();
    m_cancel.store( true );   // the one under way (a job taken later clears it, under the same lock)
}

bool RenderQueue::status( juce::String &name, int &waiting, int &finished )
{
    const std::lock_guard<std::mutex> hold( m_lock );
    name = m_current;
    waiting = static_cast<int>( m_jobs.size() );
    finished = m_finished;
    return m_current.isNotEmpty();
}

juce::String RenderQueue::takeMessage()
{
    const std::lock_guard<std::mutex> hold( m_lock );
    juce::String                      m = m_message;
    m_message.clear();
    return m;
}

void RenderQueue::run()
{
    while ( !threadShouldExit() )
    {
        Job  job;
        bool taken = false;
        {
            const std::lock_guard<std::mutex> hold( m_lock );
            if ( m_jobs.empty() )
                m_current.clear();
            else
            {
                job = m_jobs.front();
                m_jobs.erase( m_jobs.begin() );
                m_current = job.m_item.m_name;
                m_cancel.store( false );
                taken = true;
            }
        }
        if ( !taken )
        {   // nothing to do: wait for add() (or the window closing)
            m_wake.wait( 500 );
            continue;
        }
        juce::File   file;
        juce::String why;
        const bool   ok =
            m_renderer.render( m_source, job, [this] { return m_cancel.load() || threadShouldExit(); }, file, why );
        const std::lock_guard<std::mutex> hold( m_lock );
        if ( ok )
        {
            ++m_finished;
            m_message = "Written: " + file.getFileName() + " (" + clockText( m_renderer.m_total.load() ) + ", " +
                        juce::String( m_renderer.m_speed.load(), 1 ) + "x real time)";
        }
        else
        {
            m_message = job.m_item.m_name + ": " + why;
            if ( why == "cancelled" && file != juce::File() )
                file.deleteFile();   // a part of a rendering is no rendering of it
        }
        m_current.clear();
    }
}

}   // namespace renderkit

START_JUCE_APPLICATION( renderkit::RenderApplication )
