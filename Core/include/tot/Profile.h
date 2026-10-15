/**
 * @file Profile.h
 * @brief What each stage of the engine costs (PLAN 11): with the CMake option TOT_PROFILE, the stages of Deck::render and
 *        of the engine's mixer and master add their wall time to a slot each, and tot_render prints the table. Without
 *        it the scopes compile to nothing.
 * @author Rene Weller
 *
 * @par Compile Flags:
 *   @arg @c TOT_PROFILE - times the stages of the engine into a slot each (the CMake option of the same name); without
 *                         it the scopes compile to nothing
 */
#pragma once

namespace tot::prof
{

/** @brief The stages timed. */
enum SlotE : int
{
    KICK,
    RUMBLE,
    SUB,
    KIT,
    PING,
    BASS,
    ACID,
    CHORD,
    DRONE,
    TEXTURE,
    BUSES,
    CLOUD,
    ROOM,
    DUB,
    DUCKS,
    TRACK_BUS,
    GLUE,
    MIXER,
    MASTER_TONE,
    CLIPPER,
    LIMITER,
    COUNT
};
/** @brief Their names. */
inline const char *const Names[COUNT] = {
    "kick",  "rumble",    "sub",           "kit (12 lanes)",  "ping",        "bass",         "acid",
    "chord", "drone",     "texture",       "buses and sends", "cloud",       "room",         "dub chain",
    "ducks", "track bus", "glue and trim", "mixer",           "master tone", "clipper (4x)", "limiter"
};

}   // namespace tot::prof

#ifdef TOT_PROFILE
  #include <chrono>
namespace tot::prof
{
/** @brief Nanoseconds spent per slot. */
inline double ns[COUNT] = {};
/** @brief Adds the time from its construction to its end to a slot. */
struct Scope
{
    int                                   m_slot;
    std::chrono::steady_clock::time_point m_t0;
    explicit Scope( int s ) : m_slot( s ), m_t0( std::chrono::steady_clock::now() ) {}
    ~Scope()
    {
        ns[m_slot] += std::chrono::duration<double, std::nano>( std::chrono::steady_clock::now() - m_t0 ).count();
    }
};
}   // namespace tot::prof
  #define TOT_PROF_CAT2( a, b ) a##b
  #define TOT_PROF_CAT( a, b ) TOT_PROF_CAT2( a, b )
  #define TOT_PROF( slot ) const ::tot::prof::Scope TOT_PROF_CAT( totProf, __LINE__ )( ::tot::prof::slot )
  /** @brief A stage that is not a block of its own: its start, and its end in the same scope. */
  #define TOT_PROF_BEGIN( slot ) const auto totProfT_##slot = std::chrono::steady_clock::now()
  #define TOT_PROF_END( slot )                                                                                         \
      ( ::tot::prof::ns[::tot::prof::slot] +=                                                                          \
        std::chrono::duration<double, std::nano>( std::chrono::steady_clock::now() - totProfT_##slot ).count() )
#else
  /**
 * @brief Times the enclosing scope into @p slot (nothing without TOT_PROFILE).
 * @param[in] slot  the profile slot (prof::SlotE)
 */
  #define TOT_PROF( slot ) ( (void)0 )
  /**
 * @brief Starts timing into @p slot (nothing without TOT_PROFILE).
 * @param[in] slot  the profile slot (prof::SlotE)
 */
  #define TOT_PROF_BEGIN( slot ) ( (void)0 )
  /**
 * @brief Ends timing into @p slot (nothing without TOT_PROFILE).
 * @param[in] slot  the profile slot (prof::SlotE)
 */
  #define TOT_PROF_END( slot ) ( (void)0 )
#endif
