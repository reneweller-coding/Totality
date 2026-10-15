/**
 * @file Presets.h
 * @brief Factory sound presets for every synth engine (28.09.2026, after Ephemeris' Presets.h): 1024 each for the kick,
 *        the rumble, the sub, a lane of the kit, the ping, the bass, the 303, the dub chord, the drone and the texture.
 *
 * **How they are made.** Each engine has sixteen groups -- "Berlin 909", "Dub Sine", "Ride Bell", "Classic 303",
 * "Basic Channel" ... -- and each group sixty-four presets on an eight by eight grid. The rows of the grid are an
 * adjective that runs from dark to bright, the columns a noun of the group's own, so every name is two words and unique
 * within its engine, and the name says where on the grid the sound lies. A group gives each knob it cares about a range
 * and an axis: the adjective's (mostly the brightness: tone, cutoff, band), the noun's (mostly the shape: decay,
 * resonance, drive), or a seeded draw; the rest keep their defaults. The same index is always the same sound.
 *
 * **The composer's choice.** A group says how well it suits each of the four styles (Hypnotic, Ostgut, Dub, Raw); the
 * composer draws a group by those weights against the style mix of the track's profile, then one of its presets
 * (compose.pick_sounds). A lane of the kit only takes a preset of a group made for its role (hats, rides, claps ...).
 *
 * **What they leave alone.** A preset is the sound, not the mix: levels, pans, sends, ducking, the kick's tuning mode,
 * the octaves, and a lane's role, pattern knobs and choke group stay where the mix and the composer put them. Nothing
 * a preset sets moves the pitch of a tonal voice.
 * @author Rene Weller
 */
#pragma once
#include "tot/Dsp.h"
#include "tot/Params.h"
#include "tot/Preferences.h"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace tot
{

/** @brief One preset: its group (a submenu), its name, and the values it sets (knob index in its module, value). */
struct SoundPreset
{
    std::string                        m_group;          ///< its group (a submenu)
    std::string                        m_name;           ///< its name
    std::vector<std::pair<int, float>> m_values;         ///< the values it sets: knob index in its module, value
    float    m_styles[4] = { 1.0f, 1.0f, 1.0f, 1.0f };   ///< its group's fit to Hypnotic, Ostgut, Dub, Raw (0..1)
    uint32_t m_roles = 0;                                ///< a kit lane's roles it is made for (bit PercRole), 0: any
};

/**
 * @brief The engines with factory presets: Kick, Rumble, Sub, Perc, Ping, Bass, Acid, Chord, Drone, Texture.
 * @param[in] module  the module
 * @return true when it has factory presets
 */
bool hasPresets( ModuleE module );
/**
 * @brief The factory presets of an engine (1024; empty for a module without). Built once, thread-safe.
 * @param[in] module  the module
 * @return its factory presets
 */
const std::vector<SoundPreset> &factoryPresets( ModuleE module );
/**
 * @brief Whether a preset leaves knob @p k of @p module alone (the mix, the composer's and the pattern's knobs).
 * @param[in] module  the module
 * @param[in] k       the knob's index within the module
 * @return true when presets leave it alone
 */
bool presetLeaves( ModuleE module, int k );
/**
 * @brief The values a preset gives every knob of its module it does not leave: its own, the defaults of instance
 *        @p instance for the rest -- the whole sound, so a preset sounds the same whatever was loaded before.
 * @param[in] module    the module
 * @param[in] instance  its instance
 * @param[in] preset    the preset
 * @return the knobs and their values
 */
std::vector<std::pair<int, float>> presetKnobs( ModuleE module, int instance, const SoundPreset &preset );
/**
 * @brief Applies @p preset to instance @p instance of @p module in @p params.
 * @param[in,out] params    the knobs, set here
 * @param[in]     module    the module
 * @param[in]     instance  its instance
 * @param[in]     preset    the preset
 */
void applyPreset( ParamStore &params, ModuleE module, int instance, const SoundPreset &preset );
/**
 * @brief The composer's choice: a preset of @p module for a track of style mix @p styleMix (Hypnotic, Ostgut, Dub,
 *        Raw; weights summing to 1), drawn from @p rng; for a kit lane only among presets made for @p role (-1: any).
 * @param[in]     module    the module
 * @param[in]     styleMix  the style mix
 * @param[in]     role      the role
 * @param[in,out] rng       the random stream
 * @param[in]     prefs     the ratings' weights, or null
 * @return its index in factoryPresets(module), -1 if the engine has none
 */
int pickPreset( ModuleE module, const float *styleMix, int role, Rng &rng, const Preferences *prefs = nullptr );

}   // namespace tot
