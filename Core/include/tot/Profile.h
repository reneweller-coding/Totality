/**
 * @file Profile.h
 * @brief What each stage of the engine costs (PLAN 11): with the CMake option TOT_PROFILE, the stages of Deck::render and
 *        of the engine's mixer and master add their wall time to a slot each, and tot_render prints the table. Without
 *        it the scopes compile to nothing.
 */
#pragma once

namespace tot::prof {

/** @brief The stages timed. */
enum Slot : int { Kick, Rumble, Sub, Kit, Ping, Bass, Acid, Chord, Drone, Texture, Buses, Cloud, Room, Dub, Ducks, TrackBus, Glue,
                  Mixer, MasterTone, Clipper, Limiter, Count };
/** @brief Their names. */
inline const char* const kNames[Count] = { "kick", "rumble", "sub", "kit (12 lanes)", "ping", "bass", "acid", "chord", "drone",
                                           "texture", "buses and sends", "cloud", "room", "dub chain", "ducks", "track bus",
                                           "glue and trim", "mixer", "master tone", "clipper (4x)", "limiter" };

} // namespace tot::prof

#ifdef TOT_PROFILE
#include <chrono>
namespace tot::prof {
/** @brief Nanoseconds spent per slot. */
inline double ns[Count] = {};
/** @brief Adds the time from its construction to its end to a slot. */
struct Scope {
    int slot;
    std::chrono::steady_clock::time_point t0;
    explicit Scope(int s) : slot(s), t0(std::chrono::steady_clock::now()) {}
    ~Scope() { ns[slot] += std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count(); }
};
} // namespace tot::prof
#define TOT_PROF_CAT2(a, b) a##b
#define TOT_PROF_CAT(a, b) TOT_PROF_CAT2(a, b)
#define TOT_PROF(slot) const ::tot::prof::Scope TOT_PROF_CAT(totProf, __LINE__)(::tot::prof::slot)
/** @brief A stage that is not a block of its own: its start, and its end in the same scope. */
#define TOT_PROF_BEGIN(slot) const auto totProfT_##slot = std::chrono::steady_clock::now()
#define TOT_PROF_END(slot)     (::tot::prof::ns[::tot::prof::slot] += std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - totProfT_##slot).count())
#else
#define TOT_PROF(slot) ((void)0)
#define TOT_PROF_BEGIN(slot) ((void)0)
#define TOT_PROF_END(slot) ((void)0)
#endif
