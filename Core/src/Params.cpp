/**
 * @file Params.cpp
 * @brief Module descriptor tables and the parameter store.
 * @note The store below the tables is copied from Ephemeris `Core/src/Params.cpp` at d047d79 (27.09.2026); the
 *       tables are Totality's own.
 * @author Rene Weller
 */
#include "tot/Params.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace tot
{

const char *const KeyNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const char *const ScaleNames[] = { "Aeolian", "Dorian", "Phrygian", "Hexachord", "Minor Pentatonic" };
const char *const StyleNames[] = { "Hypnotic", "Ostgut", "Dub", "Raw Peak" };
const char *const PercRoleNames[NumPercRoles] = { "Closed Hat", "Rolling Hat", "Open Hat", "Ride",
                                                  "Clap",       "Clap Ghost",  "Snare",    "Rim",
                                                  "Shaker",     "Tom",         "Conga",    "Noise" };

namespace
{

const char *const LowOwnerNames[] = { "Rumble", "Sub" };   ///< compose.low_owner: who owns the low end
const char *const FormChoiceNames[] = { "Auto", "Arc", "Peak", "Endless" };   ///< compose.form: the track's form
const char *const MorphToNames[] = { "Off", "Hypnotic", "Ostgut", "Dub",
                                     "Raw Peak" };   ///< compose.morph_to: the style a track morphs into
const char *const KeyboardPartNames[] = {
    "Off", "Kit", "Bass", "303", "Ping", "Chord", "Drone", "By channel"
};   ///< perform::keys
const char *const KeyboardModeNames[] = { "Replace", "Layer" };              ///< perform.keyboard_mode
const char *const KeyboardSplitNames[] = { "C1", "C2", "C3", "C4", "C5" };   ///< perform.keyboard_split: 36 .. 84
const char *const KeyboardVelocityNames[] = { "As Played", "Soft", "Hard", "Fixed" };   ///< perform.keyboard_velocity
const char *const KickEngineNames[] = { "Sweep", "Resonator", "909" };                  ///< kick.engine
const char *const KickTuneNames[] = { "Free", "Key", "Fifth", "Flat Seventh" };         ///< kick.tune (KickTune)
const char *const KickClipNames[] = { "Soft", "Hard" };                                 ///< kick.clip
const char *const LockNames[] = { "Off", "Kick" };   ///< sub.lock: the sub's phase locked to the kick or not
const char *const PercEngineNames[] = { "Noise", "Metal", "Modal", "Tone", "FM" };   ///< perc.engine (PercEngine)
const char *const ModeSetNames[] = { "Membrane", "Bar", "Harmonic" };   ///< perc.modes: the modal engine's mode sets
const char *const PercFilterNames[] = { "Low Pass", "Band Pass",
                                        "High Pass" };           ///< perc.filter: the lane filter's output
const char *const NoiseTypeNames[] = { "White", "909 Metal" };   ///< perc.noise_type
/** @brief synth.model (FilterModel). */
const char *const FilterModelNames[] = {
    "Moog Ladder",        "Prophet (SSM2040)", "Juno (IR3109)", "Oberheim SEM", "Xpander",
    "Diode Ladder (303)", "Korg35 (MS-20)",    "Polivoks",      "EDP Wasp",     "Comb"
};
const char *const BandModeNames[] = { "Band Pass", "High Pass" };   ///< chord.band_mode: the bus's band or high pass
const char *const EchoTimeNames[] = { "1/16", "1/8", "3/16", "1/4", "3/8", "1/2" };   ///< the echo times, in notes

/**
 * Aeolian 0.6, Dorian 0.15, the Aeolian-Dorian hexachord (no sixth) 0.1, Phrygian 0.1, minor pentatonic 0.05 is the
 * prior of Dok. 8.1; harmonic minor is absent from the genre (Faraldo 2017) and not offered.
 */
const ScaleDef Scales[static_cast<int>( ScaleE::COUNT )] = {
    { 7, { 0, 2, 3, 5, 7, 8, 10 } },   // Aeolian
    { 7, { 0, 2, 3, 5, 7, 9, 10 } },   // Dorian
    { 7, { 0, 1, 3, 5, 7, 8, 10 } },   // Phrygian
    { 6, { 0, 2, 3, 5, 7, 10, 0 } },   // Aeolian-Dorian hexachord: the sixth left out
    { 5, { 0, 3, 5, 7, 10, 0, 0 } },   // minor pentatonic
};

/** Phase 13: the archetypes (Composer.h), Auto first. */
const char *const ArchetypeChoiceNames[] = { "Auto", "Tool", "Roller", "Stab", "Acid", "Bleep", "Dub Chord", "Tribal" };
/** @brief The compose module: what the composer is asked for. */
const ParamDesc ComposeParams[compose::COUNT] = {
    { "bpm", "Tempo", "BPM", 100.0f, 160.0f, 130.0f, CurveE::LINEAR },
    { "key", "Key", "", 0.0f, 11.0f, 9.0f, CurveE::CHOICE, KeyNames },
    { "scale", "Scale", "", 0.0f, 4.0f, 0.0f, CurveE::CHOICE, ScaleNames },
    { "style", "Style", "", 0.0f, 3.0f, 0.0f, CurveE::CHOICE, StyleNames },
    { "minutes", "Length", "min", 2.0f, 16.0f, 7.0f, CurveE::LINEAR },
    { "swing", "Swing", "%", 50.0f, 66.0f, 53.0f, CurveE::LINEAR },   // MPC scale, Dok. 8.3
    { "humanize", "Humanize", "ms", 0.0f, 10.0f, 3.0f, CurveE::LINEAR },
    { "low_owner", "Low End", "", 0.0f, 1.0f, 0.0f, CurveE::CHOICE, LowOwnerNames },
    { "auto", "Auto", "", 0.0f, 1.0f, 1.0f, CurveE::TOGGLE },
    { "form", "Form", "", 0.0f, 3.0f, 0.0f, CurveE::CHOICE, FormChoiceNames },
    { "morph_to", "Morph To", "", 0.0f, 4.0f, 0.0f, CurveE::CHOICE, MorphToNames },
    { "morph", "Morph", "", 0.0f, 1.0f, 0.5f, CurveE::LINEAR },
    { "dub_share", "Dub Share", "", 0.0f, 1.0f, 0.0f, CurveE::LINEAR },
    { "hypnotic_share", "Hypnotic Share", "", 0.0f, 1.0f, 0.0f, CurveE::LINEAR },
    { "pick_sounds", "Composer's Sounds", "", 0.0f, 1.0f, 1.0f, CurveE::TOGGLE },
    { "archetype", "Archetype", "", 0.0f, 7.0f, 0.0f, CurveE::CHOICE, ArchetypeChoiceNames },
    { "use_ratings", "Favor Ratings", "", 0.0f, 1.0f, 0.0f, CurveE::TOGGLE },
};

/**
 * The kick (PLAN 5.1, Dok. 8.4). Phosphene's ranges, with Totality's defaults: an end pitch tuned to the key in 41 to
 * 62 Hz, a start about 2.5 octaves up that settles in some 20 ms, a decay of 380 ms between the peak-time and the
 * rolling recipe, the click band 2 to 5 kHz. The top layer is the 909 engine eight semitones down, high-passed at
 * 400 Hz, about 12 dB under the body (the Berghain recipe says -8 st and a mid layer; the level is [I]).
 */
const ParamDesc KickParams[kick::COUNT] = {
    { "engine", "Engine", "", 0.0f, 2.0f, 0.0f, CurveE::CHOICE, KickEngineNames },
    { "tune", "Tune", "", 0.0f, 3.0f, 1.0f, CurveE::CHOICE, KickTuneNames },
    { "pitch_end", "Pitch End", "Hz", 30.0f, 120.0f, 52.0f, CurveE::LOG },
    { "pitch_start", "Pitch Start", "Hz", 60.0f, 1500.0f, 300.0f, CurveE::LOG },
    { "pitch_decay", "Body Decay", "ms", 5.0f, 300.0f, 18.0f, CurveE::LOG },
    { "punch_decay", "Punch Decay", "ms", 0.5f, 20.0f, 3.0f, CurveE::LOG },
    { "punch", "Punch", "", 0.0f, 1.0f, 0.45f, CurveE::LINEAR },
    { "amp_attack", "Attack", "ms", 0.0f, 10.0f, 0.2f, CurveE::LINEAR },
    { "amp_hold", "Hold", "ms", 0.0f, 150.0f, 25.0f, CurveE::LINEAR },
    { "amp_decay", "Decay", "ms", 20.0f, 1500.0f, 380.0f, CurveE::LOG },
    { "drive", "Drive", "", 0.0f, 1.0f, 0.35f, CurveE::LINEAR },
    { "clip", "Clip", "", 0.0f, 1.0f, 0.0f, CurveE::CHOICE, KickClipNames },
    { "click_level", "Click", "", 0.0f, 1.0f, 0.35f, CurveE::LINEAR },
    { "click_tone", "Click Tone", "Hz", 500.0f, 12000.0f, 3000.0f, CurveE::LOG },
    { "click_decay", "Click Decay", "ms", 0.5f, 30.0f, 6.0f, CurveE::LOG },
    { "tone", "Tone", "Hz", 200.0f, 20000.0f, 6000.0f, CurveE::LOG },
    { "level", "Level", "dB", -36.0f, 6.0f, -3.0f, CurveE::LINEAR },
    { "tail_limit", "Tail Limit", "dB", -60.0f, 0.0f, 0.0f, CurveE::LINEAR },   // 0: off
    { "low_cut", "Low Cut", "Hz", 15.0f, 60.0f, 30.0f, CurveE::LOG },
    { "dip_freq", "Dip Freq", "Hz", 200.0f, 1500.0f, 500.0f, CurveE::LOG },
    { "dip", "Dip", "dB", -12.0f, 0.0f, -3.0f, CurveE::LINEAR },
    { "top_level", "Top Level", "dB", -60.0f, 0.0f, -12.0f, CurveE::LINEAR },   // -60: off
    { "top_pitch", "Top Pitch", "st", -24.0f, 0.0f, -8.0f, CurveE::LINEAR },
    { "top_decay", "Top Decay", "ms", 20.0f, 400.0f, 90.0f, CurveE::LOG },
    { "top_drive", "Top Drive", "", 0.0f, 1.0f, 0.60f, CurveE::LINEAR },
    { "top_cut", "Top Cut", "Hz", 150.0f, 2000.0f, 400.0f, CurveE::LOG },
};

/**
 * The rumble (PLAN 5.2, Dok. 8.4). The hall 1 to 4 s (TrackSensei 1 to 2 s, Dark Cinematic 3.79 s), a clip of
 * 4 to 6 dB, a low pass at two to four times the kick's pitch; the sine under the split a few dB under the band
 * above; the duck 4 to 8 dB with a hold about the kick's body and a release of 150 to 350 ms.
 */
const ParamDesc RumbleParams[rumble::COUNT] = {
    { "level", "Level", "dB", -60.0f, 6.0f, -9.0f, CurveE::LINEAR },   // -60: off
    { "split", "Split", "Hz", 50.0f, 150.0f, 80.0f, CurveE::LOG },
    { "decay", "Decay", "s", 0.5f, 6.0f, 1.8f, CurveE::LOG },
    { "size", "Size", "", 0.3f, 3.0f, 1.0f, CurveE::LOG },
    { "damping", "Damping", "", 0.0f, 1.0f, 0.5f, CurveE::LINEAR },
    { "predelay", "Pre-Delay", "ms", 0.0f, 60.0f, 0.0f, CurveE::LINEAR },
    { "drive", "Drive", "dB", 0.0f, 18.0f, 6.0f, CurveE::LINEAR },
    { "ratio", "Low Pass", "x f0", 1.5f, 6.0f, 3.0f, CurveE::LOG },
    { "resonance", "Resonance", "", 0.0f, 1.0f, 0.15f, CurveE::LINEAR },
    { "sub", "Sub", "dB", -60.0f, 6.0f, 0.0f,
      CurveE::LINEAR },   // -60: off; 0 dB since the reference measurement (sub share)
    { "sub_attack", "Sub Attack", "ms", 5.0f, 200.0f, 25.0f, CurveE::LOG },
    { "sub_release", "Sub Release", "ms", 50.0f, 2000.0f, 300.0f, CurveE::LOG },
    { "duck", "Duck", "dB", 0.0f, 24.0f, 7.0f, CurveE::LINEAR },
    { "duck_hold", "Duck Hold", "ms", 0.0f, 250.0f, 80.0f, CurveE::LINEAR },
    { "duck_release", "Duck Release", "ms", 30.0f, 600.0f, 220.0f, CurveE::LOG },
};

/** The sub bass (PLAN 5.3): a sine, a low pass at 80 to 120 Hz, the kick's phase, a duck of 6 to 12 dB. */
const ParamDesc SubParams[sub::COUNT] = {
    { "level", "Level", "dB", -60.0f, 6.0f, -6.0f, CurveE::LINEAR },
    { "octave", "Octave", "", -2.0f, 2.0f, 0.0f, CurveE::INT },
    { "attack", "Attack", "ms", 0.5f, 50.0f, 3.0f, CurveE::LOG },
    { "decay", "Decay", "ms", 20.0f, 2000.0f, 250.0f, CurveE::LOG },
    { "sustain", "Sustain", "", 0.0f, 1.0f, 0.7f, CurveE::LINEAR },
    { "release", "Release", "ms", 10.0f, 800.0f, 90.0f, CurveE::LOG },
    { "low_pass", "Low Pass", "Hz", 40.0f, 300.0f, 110.0f, CurveE::LOG },
    { "drive", "Drive", "", 0.0f, 1.0f, 0.1f, CurveE::LINEAR },
    { "lock", "Kick Lock", "", 0.0f, 1.0f, 1.0f, CurveE::CHOICE, LockNames },
    { "duck", "Duck", "dB", 0.0f, 24.0f, 9.0f, CurveE::LINEAR },
    { "duck_hold", "Duck Hold", "ms", 0.0f, 250.0f, 40.0f, CurveE::LINEAR },
    { "duck_release", "Duck Release", "ms", 30.0f, 600.0f, 180.0f, CurveE::LOG },
};

/** A percussion lane: Phosphene's table (Perc.h there), then the noise type. */
const ParamDesc PercParams[perc::COUNT] = {
    { "active", "Active", "", 0.0f, 1.0f, 1.0f, CurveE::TOGGLE },
    { "role", "Role", "", 0.0f, 11.0f, 0.0f, CurveE::CHOICE, PercRoleNames },
    { "engine", "Engine", "", 0.0f, 4.0f, 0.0f, CurveE::CHOICE, PercEngineNames },
    { "pitch", "Pitch", "Hz", 40.0f, 12000.0f, 400.0f, CurveE::LOG },
    { "pitch_amount", "Pitch Amount", "x", 1.0f, 16.0f, 1.0f, CurveE::LOG },
    { "pitch_decay", "Pitch Decay", "ms", 0.5f, 300.0f, 10.0f, CurveE::LOG },
    { "fm_ratio", "FM Ratio", "", 0.25f, 8.0f, 1.41f, CurveE::LINEAR },
    { "fm_index", "FM Index", "", 0.0f, 8.0f, 0.0f, CurveE::LINEAR },
    { "mode_set", "Modes", "", 0.0f, 2.0f, 0.0f, CurveE::CHOICE, ModeSetNames },
    { "mode_damp", "Mode Damping", "", 0.0f, 1.0f, 0.5f, CurveE::LINEAR },
    { "metal_scale", "Metal Scale", "x", 0.25f, 4.0f, 1.0f, CurveE::LOG },
    { "noise", "Noise", "", 0.0f, 1.0f, 0.0f, CurveE::LINEAR },
    { "noise_decay", "Noise Decay", "ms", 2.0f, 3000.0f, 60.0f, CurveE::LOG },
    { "bursts", "Bursts", "", 1.0f, 6.0f, 1.0f, CurveE::INT },
    { "burst_spacing", "Burst Spacing", "ms", 2.0f, 40.0f, 10.0f, CurveE::LINEAR },
    { "decay", "Decay", "ms", 2.0f, 3000.0f, 120.0f, CurveE::LOG },
    { "filter", "Filter", "", 0.0f, 2.0f, 2.0f, CurveE::CHOICE, PercFilterNames },
    { "cutoff", "Cutoff", "Hz", 100.0f, 18000.0f, 8000.0f, CurveE::LOG },
    { "resonance", "Resonance", "", 0.0f, 1.0f, 0.2f, CurveE::LINEAR },
    { "low_cut", "Low Cut", "Hz", 150.0f, 8000.0f, 150.0f, CurveE::LOG },
    { "drive", "Drive", "", 0.0f, 1.0f, 0.0f, CurveE::LINEAR },
    { "level", "Level", "dB", -36.0f, 6.0f, -12.0f, CurveE::LINEAR },
    { "pan", "Pan", "", -1.0f, 1.0f, 0.0f, CurveE::LINEAR },
    { "choke", "Choke Group", "", 0.0f, 4.0f, 0.0f, CurveE::INT },
    { "shift", "Shift", "ms", -10.0f, 10.0f, 0.0f, CurveE::LINEAR },
    { "density", "Density", "", 0.0f, 1.0f, 0.5f, CurveE::LINEAR },
    { "tune", "Tune to Key", "", 0.0f, 1.0f, 0.0f, CurveE::TOGGLE },
    { "pan_depth", "Pan Depth", "", 0.0f, 1.0f, 0.0f, CurveE::LINEAR },
    { "pan_bars", "Pan Period", "bars", 0.0625f, 16.0f, 0.1875f, CurveE::LOG },
    { "cut_track", "Cut Tracks Pitch", "", 0.0f, 2.0f, 0.0f, CurveE::LINEAR },
    { "noise_type", "Noise Type", "", 0.0f, 1.0f, 0.0f, CurveE::CHOICE, NoiseTypeNames },
};

/**
 * The buses (PLAN 8.1). The levels of the hats' and the percussion's buses, like the room's return, the pads' levels,
 * the ping's and the master's tilt, are fitted to the references (27.09.2026, PLAN 13.4): the stems of a 10-minute study
 * per style, their gains and the tilt chosen so the sum meets the refs' band balance (250 Hz .. 16 kHz against 40 .. 140
 * Hz) and width in one least-squares fit over the four styles -- hats -3.5 dB, perc -6.7, ping +1.7, room +9.9, chord
 * and drone +6.7, tilt +10.5 dB.
 */
const ParamDesc MixParams[mix::COUNT] = {
    { "hats_level", "Hats Level", "dB", -24.0f, 12.0f, -3.5f, CurveE::LINEAR },
    { "hats_cut", "Hats Cut", "Hz", 200.0f, 20000.0f, 20000.0f, CurveE::LOG },
    { "perc_level", "Perc Level", "dB", -24.0f, 12.0f, -6.5f, CurveE::LINEAR },
    { "perc_cut", "Perc Cut", "Hz", 200.0f, 20000.0f, 20000.0f, CurveE::LOG },
    { "drum_sat", "Drum Saturation", "", 0.0f, 1.0f, 0.35f, CurveE::LINEAR },
    { "duck_low", "Duck Low", "dB", 0.0f, 18.0f, 10.0f, CurveE::LINEAR },
    { "duck_mid", "Duck Mid", "dB", 0.0f, 12.0f, 3.0f, CurveE::LINEAR },
    { "low_cut", "Group Low Cut", "Hz", 20.0f, 500.0f, 20.0f, CurveE::LOG },   // off at 20 Hz
};

/**
 * The master (PLAN 8.4, 8.5): a glue compressor (threshold -16 dB and a gentle ratio after Dok. 8.7), a soft
 * clipper at four times the rate, a true-peak limiter at -1 dBTP (Dok.: at most -0.5), the side mono under 120 Hz.
 */
const ParamDesc MasterParams[master::COUNT] = {
    { "level", "Level", "dB", -24.0f, 12.0f, 0.0f, CurveE::LINEAR },
    { "threshold", "Glue Threshold", "dB", -40.0f, 0.0f, -16.0f, CurveE::LINEAR },
    { "ratio", "Glue Ratio", "", 1.0f, 10.0f, 2.0f, CurveE::LOG },
    { "clip", "Clip Drive", "dB", 0.0f, 9.0f, 2.0f, CurveE::LINEAR },
    { "ceiling", "Ceiling", "dBTP", -6.0f, 0.0f, -1.0f, CurveE::LINEAR },
    { "mono_below", "Mono Below", "Hz", 40.0f, 250.0f, 120.0f, CurveE::LOG },
    { "tilt", "Tilt", "dB", -12.0f, 12.0f, 10.0f, CurveE::LINEAR },   // fitted: +10.5 (Erg. 2, MixParams)
    { "cut", "Vinyl Cut", "", 0.0f, 1.0f, 0.0f, CurveE::TOGGLE },
};

/**
 * A synth voice (PLAN 5.3). The defaults are the bass's (Dok. 8.4: saw or square in the SH-101/303 idiom, the SH-101's
 * IR3109 cascade, high pass 65 Hz, low pass 350 Hz, release about 300 ms, a duck of 6 to 12 dB); the 303 line's differ
 * (DefaultAcid).
 */
const ParamDesc SynthParams[synth::COUNT] = {
    { "level", "Level", "dB", -60.0f, 6.0f, -9.0f, CurveE::LINEAR },
    { "pan", "Pan", "", -1.0f, 1.0f, 0.0f, CurveE::LINEAR },
    { "wave", "Wave", "", 0.0f, 1.0f, 0.0f, CurveE::LINEAR },
    { "pulse_width", "Pulse Width", "", 0.05f, 0.95f, 0.5f, CurveE::LINEAR },
    { "sub_osc", "Sub Osc", "", 0.0f, 1.0f, 0.3f, CurveE::LINEAR },
    { "filter", "Filter", "", 0.0f, 9.0f, 2.0f, CurveE::CHOICE, FilterModelNames },
    { "cutoff", "Cutoff", "Hz", 40.0f, 12000.0f, 350.0f, CurveE::LOG },
    { "resonance", "Resonance", "", 0.0f, 1.0f, 0.2f, CurveE::LINEAR },
    { "env_amount", "Env Amount", "oct", 0.0f, 6.0f, 2.0f, CurveE::LINEAR },
    { "decay", "Decay", "ms", 5.0f, 3000.0f, 180.0f, CurveE::LOG },
    { "accent", "Accent", "", 0.0f, 1.0f, 0.3f, CurveE::LINEAR },
    { "amp_attack", "Attack", "ms", 0.5f, 500.0f, 3.0f, CurveE::LOG },
    { "amp_decay", "Amp Decay", "ms", 5.0f, 3000.0f, 250.0f, CurveE::LOG },
    { "amp_sustain", "Sustain", "", 0.0f, 1.0f, 0.6f, CurveE::LINEAR },
    { "amp_release", "Release", "ms", 5.0f, 3000.0f, 300.0f, CurveE::LOG },
    { "glide", "Glide", "ms", 0.0f, 500.0f, 0.0f, CurveE::LINEAR },
    { "drive", "Drive", "", 0.0f, 1.0f, 0.2f, CurveE::LINEAR },
    { "key_track", "Key Track", "", 0.0f, 1.0f, 0.5f, CurveE::LINEAR },
    { "low_cut", "Low Cut", "Hz", 20.0f, 500.0f, 65.0f, CurveE::LOG },
    { "high_cut", "High Cut", "Hz", 500.0f, 20000.0f, 5000.0f, CurveE::LOG },
    { "dub_send", "Dub Send", "", 0.0f, 1.0f, 0.0f, CurveE::LINEAR },
    { "room_send", "Room Send", "", 0.0f, 1.0f, 0.0f, CurveE::LINEAR },
    { "duck", "Duck", "dB", 0.0f, 24.0f, 8.0f, CurveE::LINEAR },
    { "duck_release", "Duck Release", "ms", 30.0f, 600.0f, 200.0f, CurveE::LOG },
};

/**
 * The dub chord (PLAN 5.6, Dok. 8.4): two saws per tone +-10 cents, amp A 10 / D 300 / S 15 %, a per-voice low pass with
 * an envelope, then on the bus a band pass at 380 Hz with a sine of 0.2 Hz, +-0.5 octaves, eight bits at 20 %, a
 * four-stage phaser at 0.5 Hz, the dip of -7 dB at 600 Hz, sends into the dub echo and the plate.
 */
const ParamDesc ChordParams[chord::COUNT] = {
    { "level", "Level", "dB", -60.0f, 6.0f, -5.0f, CurveE::LINEAR },   // fitted (MixParams)
    { "width", "Width", "", 0.0f, 1.0f, 0.6f, CurveE::LINEAR },
    { "detune", "Detune", "ct", 0.0f, 30.0f, 10.0f, CurveE::LINEAR },
    { "octave", "Octave", "", -2.0f, 2.0f, 0.0f, CurveE::INT },
    { "attack", "Attack", "ms", 0.5f, 500.0f, 10.0f, CurveE::LOG },
    { "decay", "Decay", "ms", 20.0f, 3000.0f, 300.0f, CurveE::LOG },
    { "sustain", "Sustain", "", 0.0f, 1.0f, 0.15f, CurveE::LINEAR },
    { "release", "Release", "ms", 10.0f, 3000.0f, 400.0f, CurveE::LOG },
    { "bright", "Brightness", "Hz", 200.0f, 12000.0f, 2500.0f, CurveE::LOG },
    { "env_amount", "Env Amount", "oct", 0.0f, 5.0f, 1.5f, CurveE::LINEAR },
    { "band", "Band", "Hz", 100.0f, 3000.0f, 380.0f, CurveE::LOG },
    { "band_mode", "Band Mode", "", 0.0f, 1.0f, 0.0f, CurveE::CHOICE, BandModeNames },
    { "band_q", "Band Q", "", 0.2f, 4.0f, 0.7f, CurveE::LOG },
    { "sweep", "Sweep", "oct", 0.0f, 2.0f, 0.5f, CurveE::LINEAR },
    { "sweep_rate", "Sweep Rate", "Hz", 0.01f, 2.0f, 0.2f, CurveE::LOG },
    { "crush", "Bits", "", 4.0f, 16.0f, 8.0f, CurveE::LINEAR },
    { "crush_mix", "Crush Mix", "", 0.0f, 1.0f, 0.2f, CurveE::LINEAR },
    { "phaser", "Phaser", "", 0.0f, 1.0f, 0.5f, CurveE::LINEAR },
    { "phaser_rate", "Phaser Rate", "Hz", 0.05f, 4.0f, 0.5f, CurveE::LOG },
    { "dip", "Dip 600 Hz", "dB", -18.0f, 0.0f, -7.0f, CurveE::LINEAR },
    { "dub_send", "Dub Send", "", 0.0f, 1.0f, 0.5f, CurveE::LINEAR },
    { "plate_send", "Plate Send", "", 0.0f, 1.0f, 0.35f, CurveE::LINEAR },
};

/** The drone (PLAN 5.7): two detuned saws on the tonic or fifth above 150 Hz, a slow sweep over 32 bars, long envelopes. */
const ParamDesc DroneParams[drone::COUNT] = {
    { "level", "Level", "dB", -60.0f, 6.0f, -13.0f, CurveE::LINEAR },   // fitted (MixParams)
    { "octave", "Octave", "", -2.0f, 2.0f, 0.0f, CurveE::INT },
    { "detune", "Detune", "ct", 0.0f, 30.0f, 8.0f, CurveE::LINEAR },
    { "cutoff", "Cutoff", "Hz", 80.0f, 8000.0f, 700.0f, CurveE::LOG },
    { "resonance", "Resonance", "", 0.0f, 1.0f, 0.25f, CurveE::LINEAR },
    { "sweep", "Sweep", "oct", 0.0f, 4.0f, 1.5f, CurveE::LINEAR },
    { "sweep_bars", "Sweep Bars", "bars", 4.0f, 128.0f, 32.0f, CurveE::LOG },
    { "attack", "Attack", "s", 0.01f, 20.0f, 4.0f, CurveE::LOG },
    { "release", "Release", "s", 0.01f, 20.0f, 6.0f, CurveE::LOG },
    { "plate_send", "Plate Send", "", 0.0f, 1.0f, 0.4f, CurveE::LINEAR },
    // A sustained voice fills the room's FDN far more than a hit: with the room's +22 dB (fitted on the hats, the perc and
    // the ping, Engine) a send of 0.2 put the room 6 dB over the drone itself (a set, 27.09.2026). 0.02: 14 dB under it.
    { "room_send", "Room Send", "", 0.0f, 1.0f, 0.02f, CurveE::LINEAR },
};

/** The texture (Dok. 8.4: vinyl crackle, mains hum, noise erosion at -20 to -30 dB). */
const ParamDesc TextureParams[texture::COUNT] = {
    { "level", "Level", "dB", -60.0f, 0.0f, -26.0f, CurveE::LINEAR },
    { "crackle", "Crackle", "", 0.0f, 1.0f, 0.5f, CurveE::LINEAR },
    { "hum", "Hum", "", 0.0f, 1.0f, 0.2f, CurveE::LINEAR },
    { "hum_hz", "Hum Freq", "Hz", 40.0f, 70.0f, 50.0f, CurveE::LINEAR },
    { "erosion", "Erosion", "", 0.0f, 1.0f, 0.4f, CurveE::LINEAR },
    { "width", "Width", "", 0.0f, 1.0f, 0.8f, CurveE::LINEAR },
};

/**
 * The dub chain (PLAN 5.9, Dok. 8.4): a dotted eighth with 25 % feedback, the loop high-passed at 200 Hz and low-passed at
 * 4.5 kHz, wow and flutter of a worn tape, the springs of an RE-201 beside it, a plate of 4.2 s at 20 ms pre-delay whose
 * return is cut under 250 Hz.
 */
const ParamDesc DubParams[dub::COUNT] = {
    { "echo_time", "Echo Time", "", 0.0f, 5.0f, 2.0f, CurveE::CHOICE, EchoTimeNames },
    { "feedback", "Feedback", "", 0.0f, 1.2f, 0.25f, CurveE::LINEAR },
    { "tone", "Echo Tone", "Hz", 800.0f, 12000.0f, 4500.0f, CurveE::LOG },
    { "low_cut", "Echo Low Cut", "Hz", 40.0f, 800.0f, 200.0f, CurveE::LOG },
    { "wow", "Wow", "ms", 0.0f, 3.0f, 0.5f, CurveE::LINEAR },
    { "flutter", "Flutter", "ms", 0.0f, 0.5f, 0.05f, CurveE::LINEAR },
    { "drive", "Echo Drive", "dB", 0.0f, 18.0f, 4.0f, CurveE::LINEAR },
    { "echo_return", "Echo Return", "dB", -60.0f, 6.0f, -3.0f, CurveE::LINEAR },
    { "spring", "Spring Return", "dB", -60.0f, 6.0f, -9.0f, CurveE::LINEAR },
    { "spring_decay", "Spring Decay", "s", 0.3f, 6.0f, 2.0f, CurveE::LOG },
    { "plate_decay", "Plate Decay", "s", 0.5f, 12.0f, 4.2f, CurveE::LOG },
    { "plate_damping", "Plate Damping", "", 0.0f, 1.0f, 0.4f, CurveE::LINEAR },
    { "plate_predelay", "Plate Pre-Delay", "ms", 0.0f, 100.0f, 20.0f, CurveE::LINEAR },
    { "plate_low_cut", "Plate Low Cut", "Hz", 60.0f, 800.0f, 250.0f, CurveE::LOG },
    { "plate_return", "Plate Return", "dB", -60.0f, 18.0f, 6.0f, CurveE::LINEAR },
    // The echo's sends from the ping and the kit buses: the throws of the form (PLAN 7.4) open them for a hit or a bar.
    { "ping_send", "Ping to Echo", "", 0.0f, 1.0f, 0.12f, CurveE::LINEAR },
    { "hats_send", "Hats to Echo", "", 0.0f, 1.0f, 0.0f, CurveE::LINEAR },
    { "perc_send", "Perc to Echo", "", 0.0f, 1.0f, 0.0f, CurveE::LINEAR },
};

/**
 * The granular cloud (PLAN 5.7): silent at its knob, the form raises it through a reduction (Study.cpp); twelve grains a
 * second of 180 ms from the last two seconds of the ping and the chord, a third of them transposed, half into the plate.
 */
const ParamDesc CloudParams[cloud::COUNT] = {
    { "level", "Level", "dB", -60.0f, 6.0f, -60.0f, CurveE::LINEAR },
    { "density", "Density", "/s", 0.5f, 60.0f, 12.0f, CurveE::LOG },
    { "size", "Grain Size", "ms", 20.0f, 1000.0f, 180.0f, CurveE::LOG },
    { "pitch", "Pitch", "", 0.0f, 1.0f, 0.3f, CurveE::LINEAR },
    { "spray", "Spray", "s", 0.05f, 4.5f, 2.0f, CurveE::LOG },
    { "ping_send", "From Ping", "", 0.0f, 1.0f, 0.8f, CurveE::LINEAR },
    { "chord_send", "From Chord", "", 0.0f, 1.0f, 0.6f, CurveE::LINEAR },
    { "plate_send", "Plate Send", "", 0.0f, 1.0f, 0.5f, CurveE::LINEAR },
};

/**
 * A DJ mixer channel (PLAN 7.7): the isolator's bands split at 200 Hz and 2.5 kHz (Linkwitz-Riley), each from a kill at
 * -60 dB to +6; the filter a low pass below 0 (20 kHz down to 20 Hz at -1), a high pass above (20 Hz up to 20 kHz at 1).
 */
const ParamDesc DeckParams[deck::COUNT] = {
    { "fader", "Fader", "dB", -60.0f, 6.0f, 0.0f, CurveE::LINEAR },
    { "low", "Low", "dB", -60.0f, 6.0f, 0.0f, CurveE::LINEAR },
    { "mid", "Mid", "dB", -60.0f, 6.0f, 0.0f, CurveE::LINEAR },
    { "high", "High", "dB", -60.0f, 6.0f, 0.0f, CurveE::LINEAR },
    { "filter", "Filter", "", -1.0f, 1.0f, 0.0f, CurveE::LINEAR },
    { "fx_send", "FX Send", "", 0.0f, 1.0f, 0.0f, CurveE::LINEAR },
};

/** The DJ mixer's effects: a dotted-eighth tape echo and a long hall on one send (breaks where a track has none). */
const ParamDesc DjFxParams[djfx::COUNT] = {
    { "echo_time", "Echo Time", "", 0.0f, 5.0f, 2.0f, CurveE::CHOICE, EchoTimeNames },
    { "feedback", "Feedback", "", 0.0f, 1.0f, 0.45f, CurveE::LINEAR },
    { "echo_return", "Echo Return", "dB", -60.0f, 6.0f, -3.0f, CurveE::LINEAR },
    { "hall_decay", "Hall Decay", "s", 0.5f, 12.0f, 5.0f, CurveE::LOG },
    { "hall_return", "Hall Return", "dB", -60.0f, 12.0f, 6.0f, CurveE::LINEAR },
};

const char *const DramaturgyChoiceNames[] = {
    "Warm-up", "Peak", "Closing", "Sunday", "Flat", "Cruise", "Marathon"
};   ///< set.dramaturgy: the set's arc
const char *const JourneyNames[] = { "Stay", "Wander" };     ///< set.journey: the key stays or wanders
const char *const BlendNames[] = { "16 bars", "32 bars" };   ///< set.blend: how long two tracks overlap
/** The set (PLAN 7.1, 7.7). */
const ParamDesc SetParams[set::COUNT] = {
    { "dramaturgy", "Dramaturgy", "", 0.0f, 6.0f, 1.0f, CurveE::CHOICE, DramaturgyChoiceNames },
    { "journey", "Styles", "", 0.0f, 1.0f, 1.0f, CurveE::CHOICE, JourneyNames },
    { "loops", "Live Loops", "", 0.0f, 1.0f, 0.5f, CurveE::LINEAR },
    { "fx_breaks", "FX Breaks", "", 0.0f, 1.0f, 0.3f, CurveE::LINEAR },
    { "blend", "Blend", "", 0.0f, 1.0f, 1.0f, CurveE::CHOICE, BlendNames },
    { "minutes", "Set Length", "min", 0.0f, 720.0f, 0.0f, CurveE::LINEAR },
    { "track_minutes", "Track Time", "min", 2.0f, 6.0f, 3.5f,
      CurveE::LINEAR },   // Berghain 04, 06, Fabric 66: 3.0 .. 4.0 min a track
    { "dj_hand", "DJ Moves", "", 0.0f, 1.0f, 0.5f, CurveE::LINEAR },
};

/** The performer's controls (live only). */
const ParamDesc PerformParams[perform::COUNT] = {
    { "filter", "Master Filter", "", -1.0f, 1.0f, 0.0f, CurveE::LINEAR },
    { "throw", "Echo Throw", "", 0.0f, 1.0f, 0.0f, CurveE::LINEAR },
    { "mute_kick", "Mute Kick", "", 0.0f, 1.0f, 0.0f, CurveE::TOGGLE },
    { "mute_sub", "Mute Sub", "", 0.0f, 1.0f, 0.0f, CurveE::TOGGLE },
    { "mute_hats", "Mute Hats", "", 0.0f, 1.0f, 0.0f, CurveE::TOGGLE },
    { "mute_perc", "Mute Perc", "", 0.0f, 1.0f, 0.0f, CurveE::TOGGLE },
    { "mute_ping", "Mute Ping", "", 0.0f, 1.0f, 0.0f, CurveE::TOGGLE },
    { "mute_bass", "Mute Bass", "", 0.0f, 1.0f, 0.0f, CurveE::TOGGLE },
    { "mute_pads", "Mute Pads", "", 0.0f, 1.0f, 0.0f, CurveE::TOGGLE },
    // 01.10.2026: a MIDI keyboard plays a voice (Engine::queueLive); the composer can be switched off.
    { "keyboard_part", "Keyboard Plays", "", 0.0f, 7.0f, 0.0f, CurveE::CHOICE, KeyboardPartNames },
    { "keyboard_mode", "Keyboard Mode", "", 0.0f, 1.0f, 0.0f, CurveE::CHOICE, KeyboardModeNames },
    { "composer", "Composer", "", 0.0f, 1.0f, 1.0f, CurveE::TOGGLE },
    // The keyboard's options (02.10.2026), all off by default: a split with a second voice under it, Scale Lock, the
    // velocity curve. The lower keys choose among the voices (not by channel).
    { "keyboard_lower", "Lower Keys Play", "", 0.0f, 6.0f, 0.0f, CurveE::CHOICE, KeyboardPartNames },
    { "keyboard_split", "Split At", "", 0.0f, 4.0f, 2.0f, CurveE::CHOICE, KeyboardSplitNames },
    { "keyboard_scale", "Scale Lock", "", 0.0f, 1.0f, 0.0f, CurveE::TOGGLE },
    { "keyboard_velocity", "Velocity Curve", "", 0.0f, 3.0f, 0.0f, CurveE::CHOICE, KeyboardVelocityNames },
};

/** The OSC cues (Cue.h). */
const ParamDesc CueParams[cue::COUNT] = {
    { "enabled", "OSC Cues", "", 0.0f, 1.0f, 0.0f, CurveE::TOGGLE },
    { "port", "OSC Port", "", 1024.0f, 65535.0f, 9000.0f, CurveE::INT },
};

/** A style of the user's own (Style tab): the defaults are the Hypnotic profile's numbers. */
const ParamDesc CustomParams[custom::COUNT] = {
    { "use", "Use Custom Style", "", 0.0f, 1.0f, 0.0f, CurveE::TOGGLE },
    { "bpm_low", "Tempo Low", "BPM", 120.0f, 140.0f, 128.0f, CurveE::LINEAR },
    { "bpm_high", "Tempo High", "BPM", 120.0f, 140.0f, 133.0f, CurveE::LINEAR },
    { "arc", "Arc Weight", "", 0.0f, 1.0f, 0.5f, CurveE::LINEAR },
    { "peak", "Peak Weight", "", 0.0f, 1.0f, 0.2f, CurveE::LINEAR },
    { "endless", "Endless Weight", "", 0.0f, 1.0f, 0.3f, CurveE::LINEAR },
    { "sub_chance", "Sub Owns Low", "", 0.0f, 1.0f, 0.3f, CurveE::LINEAR },
    { "blocks_low", "Blocks Low", "", 4.0f, 12.0f, 6.0f, CurveE::INT },
    { "blocks_high", "Blocks High", "", 4.0f, 12.0f, 8.0f, CurveE::INT },
    { "mutation", "Mutation", "", 0.0f, 1.0f, 0.25f, CurveE::LINEAR },
    { "reroll", "Reroll", "", 0.0f, 1.0f, 0.15f, CurveE::LINEAR },
    { "polymeter", "Polymeter", "", 0.0f, 1.0f, 0.8f, CurveE::LINEAR },
    { "fill", "Fills", "", 0.0f, 1.0f, 0.15f, CurveE::LINEAR },
    { "edge", "Filtered Edges", "", 0.0f, 1.0f, 0.5f, CurveE::LINEAR },
    { "max_reduction", "Longest Kick-out", "bars", 4.0f, 32.0f, 32.0f, CurveE::INT },
    { "swing_low", "Swing Low", "%", 50.0f, 62.0f, 51.0f, CurveE::LINEAR },
    { "swing_high", "Swing High", "%", 50.0f, 62.0f, 56.0f, CurveE::LINEAR },
    { "event_rate", "Events", "", 0.0f, 1.0f, 0.35f, CurveE::LINEAR },
    { "throw_share", "Throw Share", "", 0.0f, 1.0f, 0.5f, CurveE::LINEAR },
    { "density_cap", "Density Cap", "", 3.0f, 12.0f, 9.0f, CurveE::INT },
    { "similarity", "Bar Similarity", "", 0.7f, 1.0f, 0.90f, CurveE::LINEAR },
    { "peak_lufs", "Target LUFS", "LUFS", -14.0f, -7.0f, -10.0f, CurveE::LINEAR },
};

/** The 303 line's own defaults on the synth table (PLAN 5.3): the diode ladder, resonant, a longer accent, a slide. */
const char *const DefaultAcid =
    "acid.level=-14; acid.filter=Diode Ladder (303); acid.cutoff=600; acid.resonance=0.72; acid.env_amount=2.5; "
    "acid.decay=220;"
    "acid.accent=0.6; acid.sub_osc=0; acid.amp_sustain=0.8; acid.amp_release=60; acid.glide=60; acid.drive=0.5; "
    "acid.key_track=0.3;"
    "acid.low_cut=120; acid.high_cut=9000; acid.dub_send=0.25; acid.room_send=0.01; acid.duck=4; acid.pan=0.1\n";

/**
 * The ping (PLAN 5.5, Erg. 1): a carrier of 200 to 600 Hz (the notes decide), a modulator at sqrt 2 times it, an index of
 * 0.5 to 3 by velocity, a pitch drop of 30 to 90 ms, 40 to 180 ms of decay, the gate's longer tail, a band pass that
 * wanders by some half an octave at 0.07 Hz (Dok. 8.4's free filter LFOs, 0.05 .. 0.08 Hz).
 */
const ParamDesc PingParams[ping::COUNT] = {
    { "level", "Level", "dB", -60.0f, 6.0f, -12.5f, CurveE::LINEAR },   // fitted (MixParams)
    { "pan", "Pan", "", -1.0f, 1.0f, 0.0f, CurveE::LINEAR },
    { "width", "Width", "", 0.0f, 1.0f, 0.5f, CurveE::LINEAR },
    { "ratio", "Ratio", "", 0.25f, 8.0f, 1.414f, CurveE::LOG },
    { "index", "Index", "", 0.0f, 6.0f, 2.0f, CurveE::LINEAR },
    { "index_decay", "Index Decay", "ms", 2.0f, 500.0f, 60.0f, CurveE::LOG },
    { "pitch_amount", "Pitch Amount", "", 0.0f, 4.0f, 0.8f, CurveE::LINEAR },
    { "pitch_decay", "Pitch Decay", "ms", 2.0f, 300.0f, 45.0f, CurveE::LOG },
    { "decay", "Decay", "ms", 10.0f, 2000.0f, 140.0f, CurveE::LOG },
    { "lpg", "Low-Pass Gate", "", 0.0f, 1.0f, 0.6f, CurveE::LINEAR },
    { "lpg_release", "Gate Release", "ms", 10.0f, 2000.0f, 150.0f, CurveE::LOG },
    { "resonance", "Resonance", "", 0.0f, 1.0f, 0.35f, CurveE::LINEAR },
    { "band", "Band", "Hz", 150.0f, 6000.0f, 900.0f, CurveE::LOG },
    { "band_q", "Band Q", "", 0.2f, 8.0f, 0.8f, CurveE::LOG },
    { "band_mix", "Band Mix", "", 0.0f, 1.0f, 0.5f, CurveE::LINEAR },
    { "sweep", "Sweep", "oct", 0.0f, 2.0f, 0.6f, CurveE::LINEAR },
    { "sweep_rate", "Sweep Rate", "Hz", 0.01f, 2.0f, 0.07f, CurveE::LOG },
};

/**
 * The global motion (Dok. 8.4): cutoff LFOs of +-10 to 20 % (here in octaves), a hat volume sine of 10 to 20 % (dB), the
 * hat decay, the rumble's drive. Defaults inside those ranges; the reference measurement asks for about half a dB of
 * change from bar to bar in every band (PLAN 13.4).
 */
const ParamDesc MotionParams[motion::COUNT] = {
    { "amount", "Amount", "", 0.0f, 1.0f, 1.0f, CurveE::LINEAR },
    { "hats_cut", "Hats Cut", "oct", 0.0f, 1.5f, 0.4f, CurveE::LINEAR },
    { "hats_level", "Hats Level", "dB", 0.0f, 6.0f, 1.5f, CurveE::LINEAR },
    { "hat_decay", "Hat Decay", "", 0.0f, 0.5f, 0.2f, CurveE::LINEAR },
    { "perc_cut", "Perc Cut", "oct", 0.0f, 1.5f, 0.4f, CurveE::LINEAR },
    { "rumble_drive", "Rumble Drive", "dB", 0.0f, 6.0f, 2.0f, CurveE::LINEAR },
};

/**
 * The room (PLAN 5.9): a short plate-like hall, 1.2 s, 15 ms pre-delay, its return cut under 250 Hz and over 6 kHz
 * (Dok. 8.7: returns high-passed at 200 to 400 Hz; Dok. 8.4: "kurz/Plate auf Hats/Claps bei 10 bis 22 %"). Sends: the
 * hats 25 %, the percussion 30 %, the ping 40 %, the return +12 dB: the FDN returns quietly, and so the room lies some
 * 17 dB under the dry hats (measured on the study, 27.09.2026).
 */
const ParamDesc SpaceParams[space::COUNT] = {
    { "level", "Return", "dB", -60.0f, 12.0f, 0.0f, CurveE::LINEAR },   // over the engine's +22 dB (Engine.cpp)
    { "size", "Size", "", 0.3f, 3.0f, 0.9f, CurveE::LOG },
    { "decay", "Decay", "s", 0.2f, 12.0f, 1.2f, CurveE::LOG },
    { "damping", "Damping", "", 0.0f, 1.0f, 0.5f, CurveE::LINEAR },
    { "predelay", "Pre-Delay", "ms", 0.0f, 100.0f, 15.0f, CurveE::LINEAR },
    { "low_cut", "Low Cut", "Hz", 60.0f, 800.0f, 250.0f, CurveE::LOG },
    { "high_cut", "High Cut", "Hz", 1000.0f, 18000.0f, 6000.0f, CurveE::LOG },
    { "hats_send", "Hats Send", "", 0.0f, 1.0f, 0.25f, CurveE::LINEAR },
    { "perc_send", "Perc Send", "", 0.0f, 1.0f, 0.30f, CurveE::LINEAR },
    { "ping_send", "Ping Send", "", 0.0f, 1.0f, 0.40f, CurveE::LINEAR },
};

/**
 * The default kit (PLAN 5.4): what each of the twelve lanes is and how it sounds before a preset touches it. The
 * hats play the 909's metal table (Kit.h), 50 to 80 ms closed and 200 to 600 open, high-passed at 7 kHz or
 * band-passed at 10 kHz (Dok. 8.4); the ride the 808's six squares; the clap four bursts 10 to 20 ms apart through a
 * band pass at 1.1 kHz; the tom an octave above the old recipe's eight semitones over the kick (the low-end rule,
 * PLAN 5.4). Levels: a full-velocity hit of each lane peaks against the kick's peak where Dok. 8.7's reference levels
 * put it -- CH -8, OH -10, perc -15, FX -10 dB, the rest inferred (selftest testKitLevels, which holds them to 1.5 dB).
 */
const char *const DefaultKit =
    "perc1.role=Closed Hat; perc1.engine=Noise; perc1.noise_type=909 Metal; perc1.noise=1; perc1.noise_decay=70; "
    "perc1.decay=70;"
    "perc1.filter=High Pass; perc1.cutoff=7000; perc1.resonance=0.15; perc1.low_cut=3000; perc1.drive=0.1; "
    "perc1.level=-0.7;"
    "perc1.pan=0.1; perc1.choke=1; perc1.pan_depth=0.3\n"
    "perc2.role=Rolling Hat; perc2.engine=Noise; perc2.noise_type=909 Metal; perc2.metal_scale=1.15; perc2.noise=1;"
    "perc2.noise_decay=38; perc2.decay=38; perc2.filter=Band Pass; perc2.cutoff=10000; perc2.resonance=0.2; "
    "perc2.low_cut=5000;"
    "perc2.level=-4.2; perc2.pan=-0.15; perc2.choke=1; perc2.pan_depth=0.4\n"
    "perc3.role=Open Hat; perc3.engine=Noise; perc3.noise_type=909 Metal; perc3.metal_scale=0.95; perc3.noise=1;"
    "perc3.noise_decay=320; perc3.decay=320; perc3.filter=High Pass; perc3.cutoff=6500; perc3.resonance=0.1; "
    "perc3.low_cut=3000;"
    "perc3.level=-2.7; perc3.pan=0.1; perc3.choke=1\n"
    "perc4.role=Ride; perc4.engine=Metal; perc4.metal_scale=2.2; perc4.noise=0.5; perc4.noise_decay=700; "
    "perc4.decay=700;"
    "perc4.filter=High Pass; perc4.cutoff=4000; perc4.low_cut=1000; perc4.level=-14.0; perc4.pan=-0.3\n"
    "perc5.role=Clap; perc5.engine=Noise; perc5.noise_decay=160; perc5.bursts=4; perc5.burst_spacing=12; "
    "perc5.filter=Band Pass;"
    "perc5.cutoff=1150; perc5.resonance=0.55; perc5.low_cut=300; perc5.drive=0.2; perc5.level=-1.7\n"
    "perc6.role=Clap Ghost; perc6.engine=Noise; perc6.noise_decay=120; perc6.bursts=3; perc6.burst_spacing=11; "
    "perc6.filter=Band Pass;"
    "perc6.cutoff=1250; perc6.resonance=0.5; perc6.low_cut=300; perc6.drive=0.2; perc6.level=-9.7; perc6.pan=0.2\n"
    "perc7.role=Snare; perc7.engine=Tone; perc7.pitch=190; perc7.pitch_amount=1.6; perc7.pitch_decay=20; "
    "perc7.decay=110;"
    "perc7.noise=0.5; perc7.noise_decay=90; perc7.filter=Low Pass; perc7.cutoff=2500; perc7.level=-14\n"
    "perc8.role=Rim; perc8.engine=Tone; perc8.pitch=1700; perc8.pitch_amount=1.2; perc8.pitch_decay=3; perc8.decay=25;"
    "perc8.noise=0.15; perc8.noise_decay=8; perc8.filter=Band Pass; perc8.cutoff=1900; perc8.resonance=0.35; "
    "perc8.low_cut=400;"
    "perc8.drive=0.6; perc8.level=-5.5; perc8.tune=1; perc8.pan=-0.2\n"
    "perc9.role=Shaker; perc9.engine=Noise; perc9.noise_decay=65; perc9.filter=High Pass; perc9.cutoff=6000; "
    "perc9.resonance=0.3;"
    "perc9.low_cut=2000; perc9.drive=0.3; perc9.level=-10.1; perc9.pan=0.35; perc9.pan_depth=0.5\n"
    "perc10.role=Tom; perc10.engine=Tone; perc10.pitch=175; perc10.pitch_amount=1.5; perc10.pitch_decay=60; "
    "perc10.decay=350;"
    "perc10.noise=0.05; perc10.noise_decay=20; perc10.filter=Low Pass; perc10.cutoff=3000; perc10.level=-15; "
    "perc10.tune=1;"
    "perc10.pan=-0.25\n"
    "perc11.role=Conga; perc11.engine=Modal; perc11.mode_set=Harmonic; perc11.pitch=320; perc11.decay=260; "
    "perc11.filter=Low Pass;"
    "perc11.cutoff=5000; perc11.level=-16; perc11.tune=1; perc11.pan=0.3\n"
    "perc12.role=Noise; perc12.engine=Noise; perc12.noise_decay=2500; perc12.filter=Band Pass; perc12.cutoff=2000;"
    "perc12.resonance=0.4; perc12.low_cut=300; perc12.level=-7.0\n";

/** @brief A module of the store: its key prefix, its parameters, how many instances. */
struct ModuleSpec
{
    const char      *m_prefix;      ///< the key prefix ("kick", "perc" ...)
    const ParamDesc *m_descs;       ///< its parameters
    int              m_count;       ///< how many
    int              m_instances;   ///< how many instances
};

/** @brief The modules, in the order of Module. */
const ModuleSpec Modules[static_cast<int>( ModuleE::COUNT )] = {
    { "compose", ComposeParams, compose::COUNT, 1 }, { "kick", KickParams, kick::COUNT, 1 },
    { "rumble", RumbleParams, rumble::COUNT, 1 },    { "sub", SubParams, sub::COUNT, 1 },
    { "perc", PercParams, perc::COUNT, PercLanes },  { "mix", MixParams, mix::COUNT, 1 },
    { "master", MasterParams, master::COUNT, 1 },    { "ping", PingParams, ping::COUNT, 1 },
    { "motion", MotionParams, motion::COUNT, 1 },    { "space", SpaceParams, space::COUNT, 1 },
    { "bass", SynthParams, synth::COUNT, 1 },        { "acid", SynthParams, synth::COUNT, 1 },
    { "chord", ChordParams, chord::COUNT, 1 },       { "drone", DroneParams, drone::COUNT, 1 },
    { "texture", TextureParams, texture::COUNT, 1 }, { "dub", DubParams, dub::COUNT, 1 },
    { "cloud", CloudParams, cloud::COUNT, 1 },       { "deck", DeckParams, deck::COUNT, Decks },
    { "djfx", DjFxParams, djfx::COUNT, 1 },          { "set", SetParams, set::COUNT, 1 },
    { "perform", PerformParams, perform::COUNT, 1 }, { "cue", CueParams, cue::COUNT, 1 },
    { "custom", CustomParams, custom::COUNT, 1 },
};

/**
 * @brief Whether a curve takes whole steps (Int, Choice, Toggle).
 * @param[in] c  the curve
 * @return true when it takes whole steps
 */
bool isDiscrete( CurveE c )
{
    return c == CurveE::INT || c == CurveE::CHOICE || c == CurveE::TOGGLE;
}

/**
 * @brief @p s without spaces, tabs and carriage returns at either end.
 * @param[in] s  the text
 * @return the text without spaces, tabs and carriage returns at either end
 */
std::string_view trim( std::string_view s )
{
    while ( !s.empty() && ( s.front() == ' ' || s.front() == '\t' || s.front() == '\r' ) ) s.remove_prefix( 1 );
    while ( !s.empty() && ( s.back() == ' ' || s.back() == '\t' || s.back() == '\r' ) ) s.remove_suffix( 1 );
    return s;
}

/**
 * @brief Lower case without spaces, for matching choice names ("909 Metal" = "909metal").
 * @param[in] s  the name
 * @return the name folded
 */
std::string foldName( std::string_view s )
{
    std::string out;
    for ( char ch : s )
    {
        if ( ch == ' ' || ch == '_' || ch == '-' ) continue;
        out += ( ch >= 'A' && ch <= 'Z' ) ? static_cast<char>( ch - 'A' + 'a' ) : ch;
    }
    return out;
}

}   // namespace

const ScaleDef &scaleDef( int scale )
{
    return Scales[scale >= 0 && scale < static_cast<int>( ScaleE::COUNT ) ? scale : 0];
}

bool inScale( int scale, int semitones )
{
    const ScaleDef &d = scaleDef( scale );
    const int       pc = ( ( semitones % 12 ) + 12 ) % 12;
    for ( int i = 0; i < d.m_size; ++i )
        if ( d.m_steps[i] == pc ) return true;
    return false;
}

ParamStore::ParamStore()
{
    for ( auto &row : m_bases )
        for ( int &b : row ) b = -1;
    int total = 0;
    for ( const ModuleSpec &m : Modules ) total += m.m_count * m.m_instances;
    m_entries.reserve( static_cast<size_t>( total ) );
    for ( int mi = 0; mi < static_cast<int>( ModuleE::COUNT ); ++mi )
    {
        const ModuleSpec &m = Modules[mi];
        for ( int inst = 0; inst < m.m_instances && inst < MaxInstances; ++inst )
        {
            m_bases[mi][inst] = static_cast<int>( m_entries.size() );
            std::string prefix = m.m_prefix;
            if ( m.m_instances > 1 ) prefix += std::to_string( inst + 1 );
            for ( int p = 0; p < m.m_count; ++p )
            {
                Entry e{ &m.m_descs[p], prefix + "." + m.m_descs[p].m_key, static_cast<ModuleE>( mi ), inst };
                m_index.emplace( e.m_key, static_cast<int>( m_entries.size() ) );
                m_entries.push_back( std::move( e ) );
            }
        }
    }
    m_values = std::make_unique<std::atomic<float>[]>( m_entries.size() );
    // Defaults: the descriptors', then the lanes' own on top.
    m_defaults.resize( m_entries.size() );
    for ( int i = 0; i < count(); ++i )
    {
        m_defaults[static_cast<size_t>( i )] = desc( i ).m_defValue;
        m_values[static_cast<size_t>( i )].store( desc( i ).m_defValue, std::memory_order_relaxed );
    }
    parseText( DefaultKit );
    parseText( DefaultAcid );
    for ( int i = 0; i < count(); ++i ) m_defaults[static_cast<size_t>( i )] = get( i );
}

int ParamStore::base( ModuleE m, int instance ) const
{
    const int mi = static_cast<int>( m );
    if ( mi < 0 || mi >= static_cast<int>( ModuleE::COUNT ) || instance < 0 || instance >= MaxInstances ) return -1;
    return m_bases[mi][instance];
}

int ParamStore::find( std::string_view key ) const
{
    const auto it = m_index.find( std::string( key ) );
    return it == m_index.end() ? -1 : it->second;
}

int ParamStore::getInt( int id ) const
{
    return static_cast<int>( std::lround( get( id ) ) );
}

void ParamStore::set( int id, float value )
{
    if ( id < 0 || id >= count() ) return;
    const ParamDesc &d = desc( id );
    if ( !( value == value ) )
        value = m_defaults.empty() ? d.m_defValue : m_defaults[static_cast<size_t>( id )];   // NaN
    float v = value < d.m_minValue ? d.m_minValue : ( value > d.m_maxValue ? d.m_maxValue : value );
    if ( isDiscrete( d.m_curve ) ) v = std::round( v );
    m_values[static_cast<size_t>( id )].store( v, std::memory_order_relaxed );
}

float ParamStore::toNormalised( int id, float value ) const
{
    const ParamDesc &d = desc( id );
    if ( d.m_maxValue <= d.m_minValue ) return 0.0f;
    float n;
    if ( d.m_curve == CurveE::LOG )
        n = std::log( value / d.m_minValue ) / std::log( d.m_maxValue / d.m_minValue );
    else
        n = ( value - d.m_minValue ) / ( d.m_maxValue - d.m_minValue );
    return n < 0.0f ? 0.0f : ( n > 1.0f ? 1.0f : n );
}

float ParamStore::fromNormalised( int id, float norm ) const
{
    const ParamDesc &d = desc( id );
    const float      n = norm < 0.0f ? 0.0f : ( norm > 1.0f ? 1.0f : norm );
    float            v;
    if ( d.m_curve == CurveE::LOG )
        v = d.m_minValue * std::pow( d.m_maxValue / d.m_minValue, n );
    else
        v = d.m_minValue + n * ( d.m_maxValue - d.m_minValue );
    if ( isDiscrete( d.m_curve ) ) v = std::round( v );
    return v;
}

void ParamStore::resetDefaults()
{
    for ( int i = 0; i < count(); ++i )
        m_values[static_cast<size_t>( i )].store( m_defaults[static_cast<size_t>( i )], std::memory_order_relaxed );
}

int ParamStore::moduleCount( ModuleE m )
{
    const int mi = static_cast<int>( m );
    return mi >= 0 && mi < static_cast<int>( ModuleE::COUNT ) ? Modules[mi].m_count : 0;
}

void ParamStore::readModule( ModuleE m, int instance, float *out ) const
{
    const int b = base( m, instance );
    if ( b < 0 ) return;
    const int n = moduleCount( m );
    for ( int i = 0; i < n; ++i ) out[i] = get( b + i );
}

void ParamStore::copyValuesFrom( const ParamStore &other )
{
    const int n = count() < other.count() ? count() : other.count();
    for ( int i = 0; i < n; ++i ) m_values[static_cast<size_t>( i )].store( other.get( i ), std::memory_order_relaxed );
}

bool ParamStore::parseText( std::string_view text, std::string *error )
{
    // Split into assignments. Newlines and ';' always separate; whitespace separates only where the next word
    // contains '=' -- so a choice name with a space ("perc1.noise_type=909 Metal") stays one value while "a=1 b=2"
    // is still two assignments. '#' starts a comment to the end of the line.
    std::vector<std::string> items;
    size_t                   pos = 0;
    bool                     newItem = true;
    while ( pos < text.size() )
    {
        const char ch = text[pos];
        if ( ch == '\n' || ch == ';' || ch == '\r' )
        {
            newItem = true;
            ++pos;
            continue;
        }
        if ( ch == ' ' || ch == '\t' )
        {
            ++pos;
            continue;
        }
        if ( ch == '#' )
        {
            while ( pos < text.size() && text[pos] != '\n' ) ++pos;
            continue;
        }
        size_t end = pos;
        while ( end < text.size() && text[end] != '\n' && text[end] != ';' && text[end] != '\r' && text[end] != ' ' &&
                text[end] != '\t' )
            ++end;
        const std::string_view word = text.substr( pos, end - pos );
        pos = end;
        if ( newItem || word.find( '=' ) != std::string_view::npos || items.empty() )
            items.emplace_back( word );
        else
        {
            items.back() += ' ';
            items.back() += word;
        }
        newItem = false;
    }

    bool ok = true;
    for ( const std::string &item : items )
    {
        const std::string_view tok = trim( item );
        const size_t           eq = tok.find( '=' );
        if ( eq == std::string_view::npos )
        {
            if ( error && ok ) *error = "missing '=' in \"" + std::string( tok ) + "\"";
            ok = false;
            continue;
        }
        const std::string_view k = trim( tok.substr( 0, eq ) ), v = trim( tok.substr( eq + 1 ) );
        const int              id = find( k );
        if ( id < 0 )
        {
            if ( error && ok ) *error = "unknown parameter \"" + std::string( k ) + "\"";
            ok = false;
            continue;
        }
        const ParamDesc &d = desc( id );
        bool             matched = false;
        if ( d.m_choices != nullptr || d.m_curve == CurveE::TOGGLE )
        {
            const std::string fv = foldName( v );
            if ( d.m_curve == CurveE::TOGGLE && ( fv == "on" || fv == "off" ) )
            {
                set( id, fv == "on" ? 1.0f : 0.0f );
                matched = true;
            }
            for ( int c = 0; !matched && d.m_choices != nullptr && c <= static_cast<int>( d.m_maxValue ); ++c )
            {
                if ( fv == foldName( d.m_choices[c] ) )
                {
                    set( id, static_cast<float>( c ) );
                    matched = true;
                }
            }
        }
        if ( !matched )
        {
            const std::string vs( v );
            char             *stop = nullptr;
            const double      x = std::strtod( vs.c_str(), &stop );
            if ( vs.empty() || stop == nullptr || *stop != 0 )
            {
                if ( error && ok ) *error = "bad value \"" + vs + "\" for " + std::string( k );
                ok = false;
                continue;
            }
            set( id, static_cast<float>( x ) );
        }
    }
    return ok;
}

std::string ParamStore::toText( bool onlyChanged ) const
{
    std::string out;
    char        buf[64];
    for ( int i = 0; i < count(); ++i )
    {
        const float v = get( i );
        if ( onlyChanged && v == m_defaults[static_cast<size_t>( i )] ) continue;
        // %.9g round-trips every float exactly.
        std::snprintf( buf, sizeof( buf ), "%.9g", static_cast<double>( v ) );
        out += key( i );
        out += '=';
        out += buf;
        out += '\n';
    }
    return out;
}

std::string ParamStore::format( int id ) const
{
    const ParamDesc &d = desc( id );
    const float      v = get( id );
    if ( d.m_curve == CurveE::CHOICE && d.m_choices != nullptr ) return d.m_choices[getInt( id )];
    if ( d.m_curve == CurveE::TOGGLE ) return v >= 0.5f ? "On" : "Off";
    char buf[64];
    if ( d.m_curve == CurveE::INT )
        std::snprintf( buf, sizeof( buf ), "%d", getInt( id ) );
    else if ( std::fabs( v ) >= 100.0f )
        std::snprintf( buf, sizeof( buf ), "%.0f", static_cast<double>( v ) );
    else if ( std::fabs( v ) >= 10.0f )
        std::snprintf( buf, sizeof( buf ), "%.1f", static_cast<double>( v ) );
    else
        std::snprintf( buf, sizeof( buf ), "%.2f", static_cast<double>( v ) );
    std::string s = buf;
    if ( d.m_unit != nullptr && d.m_unit[0] != 0 )
    {
        s += ' ';
        s += d.m_unit;
    }
    return s;
}

}   // namespace tot
