/**
 * @file Profile.h
 * @brief What each stage of the engine costs (PLAN 11): with the CMake option UMB_PROFILE, the stages of Deck::render and
 *        of the engine's mixer and master add their wall time to a slot each, and umb_render prints the table. Without
 *        it the scopes compile to nothing.
 */
#pragma once

namespace umb::prof {

/** @brief The stages timed. */
enum Slot : int { Kick, Rumble, Sub, Kit, Ping, Bass, Acid, Chord, Drone, Texture, Buses, Cloud, Room, Dub, Ducks, TrackBus, Glue,
                  Mixer, MasterTone, Clipper, Limiter, Count };
/** @brief Their names. */
inline const char* const kNames[Count] = { "kick", "rumble", "sub", "kit (12 lanes)", "ping", "bass", "acid", "chord", "drone",
                                           "texture", "buses and sends", "cloud", "room", "dub chain", "ducks", "track bus",
                                           "glue and trim", "mixer", "master tone", "clipper (4x)", "limiter" };

} // namespace umb::prof

#ifdef UMB_PROFILE
#include <chrono>
namespace umb::prof {
/** @brief Nanoseconds spent per slot. */
inline double ns[Count] = {};
/** @brief Adds the time from its construction to its end to a slot. */
struct Scope {
    int slot;
    std::chrono::steady_clock::time_point t0;
    explicit Scope(int s) : slot(s), t0(std::chrono::steady_clock::now()) {}
    ~Scope() { ns[slot] += std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count(); }
};
} // namespace umb::prof
#define UMB_PROF_CAT2(a, b) a##b
#define UMB_PROF_CAT(a, b) UMB_PROF_CAT2(a, b)
#define UMB_PROF(slot) const ::umb::prof::Scope UMB_PROF_CAT(umbProf, __LINE__)(::umb::prof::slot)
/** @brief A stage that is not a block of its own: its start, and its end in the same scope. */
#define UMB_PROF_BEGIN(slot) const auto umbProfT_##slot = std::chrono::steady_clock::now()
#define UMB_PROF_END(slot)     (::umb::prof::ns[::umb::prof::slot] += std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - umbProfT_##slot).count())
#else
#define UMB_PROF(slot) ((void)0)
#define UMB_PROF_BEGIN(slot) ((void)0)
#define UMB_PROF_END(slot) ((void)0)
#endif
