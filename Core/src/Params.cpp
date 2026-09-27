/**
 * @file Params.cpp
 * @brief Module descriptor tables and the parameter store.
 * @note The store below the tables is copied from Ephemeris `Core/src/Params.cpp` at d047d79 (27.09.2026); the
 *       tables are Umbra's own.
 */
#include "umb/Params.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace umb {

const char* const kKeyNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const char* const kScaleNames[] = { "Aeolian", "Dorian", "Phrygian", "Hexachord", "Minor Pentatonic" };
const char* const kStyleNames[] = { "Hypnotic", "Ostgut", "Dub", "Raw Peak" };
const char* const kPercRoleNames[kNumPercRoles] = { "Closed Hat", "Rolling Hat", "Open Hat", "Ride", "Clap", "Clap Ghost",
                                                    "Snare", "Rim", "Shaker", "Tom", "Conga", "Noise" };

namespace {

const char* const kLowOwnerNames[] = { "Rumble", "Sub" };
const char* const kKickEngineNames[] = { "Sweep", "Resonator", "909" };
const char* const kKickTuneNames[] = { "Free", "Key", "Fifth", "Flat Seventh" };
const char* const kKickClipNames[] = { "Soft", "Hard" };
const char* const kLockNames[] = { "Off", "Kick" };
const char* const kPercEngineNames[] = { "Noise", "Metal", "Modal", "Tone", "FM" };
const char* const kModeSetNames[] = { "Membrane", "Bar", "Harmonic" };
const char* const kPercFilterNames[] = { "Low Pass", "Band Pass", "High Pass" };
const char* const kNoiseTypeNames[] = { "White", "909 Metal" };

/**
 * Aeolian 0.6, Dorian 0.15, the Aeolian-Dorian hexachord (no sixth) 0.1, Phrygian 0.1, minor pentatonic 0.05 is the
 * prior of Dok. 8.1; harmonic minor is absent from the genre (Faraldo 2017) and not offered.
 */
const ScaleDef kScales[static_cast<int>(Scale::Count)] = {
    { 7, { 0, 2, 3, 5, 7, 8, 10 } },   // Aeolian
    { 7, { 0, 2, 3, 5, 7, 9, 10 } },   // Dorian
    { 7, { 0, 1, 3, 5, 7, 8, 10 } },   // Phrygian
    { 6, { 0, 2, 3, 5, 7, 10, 0 } },   // Aeolian-Dorian hexachord: the sixth left out
    { 5, { 0, 3, 5, 7, 10, 0, 0 } },   // minor pentatonic
};

const ParamDesc kComposeParams[compose::Count] = {
    { "bpm",       "Tempo",     "BPM", 100.0f, 160.0f, 130.0f, Curve::Linear },
    { "key",       "Key",       "",      0.0f,  11.0f,   9.0f, Curve::Choice, kKeyNames },
    { "scale",     "Scale",     "",      0.0f,   4.0f,   0.0f, Curve::Choice, kScaleNames },
    { "style",     "Style",     "",      0.0f,   3.0f,   0.0f, Curve::Choice, kStyleNames },
    { "minutes",   "Length",    "min",   2.0f,  16.0f,   7.0f, Curve::Linear },
    { "swing",     "Swing",     "%",    50.0f,  66.0f,  53.0f, Curve::Linear },   // MPC scale, Dok. 8.3
    { "humanize",  "Humanize",  "ms",    0.0f,  10.0f,   3.0f, Curve::Linear },
    { "low_owner", "Low End",   "",      0.0f,   1.0f,   0.0f, Curve::Choice, kLowOwnerNames },
};

/**
 * The kick (PLAN 5.1, Dok. 8.4). Phosphene's ranges, with Umbra's defaults: an end pitch tuned to the key in 41 to
 * 62 Hz, a start about 2.5 octaves up that settles in some 20 ms, a decay of 380 ms between the peak-time and the
 * rolling recipe, the click band 2 to 5 kHz. The top layer is the 909 engine eight semitones down, high-passed at
 * 400 Hz, about 12 dB under the body (the Berghain recipe says -8 st and a mid layer; the level is [I]).
 */
const ParamDesc kKickParams[kick::Count] = {
    { "engine",      "Engine",       "",     0.0f,     2.0f,    0.0f, Curve::Choice, kKickEngineNames },
    { "tune",        "Tune",         "",     0.0f,     3.0f,    1.0f, Curve::Choice, kKickTuneNames },
    { "pitch_end",   "Pitch End",    "Hz",  30.0f,   120.0f,   52.0f, Curve::Log },
    { "pitch_start", "Pitch Start",  "Hz",  60.0f,  1500.0f,  300.0f, Curve::Log },
    { "pitch_decay", "Body Decay",   "ms",   5.0f,   300.0f,   18.0f, Curve::Log },
    { "punch_decay", "Punch Decay",  "ms",   0.5f,    20.0f,    3.0f, Curve::Log },
    { "punch",       "Punch",        "",     0.0f,     1.0f,   0.45f, Curve::Linear },
    { "amp_attack",  "Attack",       "ms",   0.0f,    10.0f,    0.2f, Curve::Linear },
    { "amp_hold",    "Hold",         "ms",   0.0f,   150.0f,   25.0f, Curve::Linear },
    { "amp_decay",   "Decay",        "ms",  20.0f,  1500.0f,  380.0f, Curve::Log },
    { "drive",       "Drive",        "",     0.0f,     1.0f,   0.35f, Curve::Linear },
    { "clip",        "Clip",         "",     0.0f,     1.0f,    0.0f, Curve::Choice, kKickClipNames },
    { "click_level", "Click",        "",     0.0f,     1.0f,   0.35f, Curve::Linear },
    { "click_tone",  "Click Tone",   "Hz", 500.0f, 12000.0f, 3000.0f, Curve::Log },
    { "click_decay", "Click Decay",  "ms",   0.5f,    30.0f,    6.0f, Curve::Log },
    { "tone",        "Tone",         "Hz", 200.0f, 20000.0f, 6000.0f, Curve::Log },
    { "level",       "Level",        "dB", -36.0f,     6.0f,   -3.0f, Curve::Linear },
    { "tail_limit",  "Tail Limit",   "dB", -60.0f,     0.0f,    0.0f, Curve::Linear },   // 0: off
    { "low_cut",     "Low Cut",      "Hz",  15.0f,    60.0f,   30.0f, Curve::Log },
    { "dip_freq",    "Dip Freq",     "Hz", 200.0f,  1500.0f,  500.0f, Curve::Log },
    { "dip",         "Dip",          "dB", -12.0f,     0.0f,   -3.0f, Curve::Linear },
    { "top_level",   "Top Level",    "dB", -60.0f,     0.0f,  -12.0f, Curve::Linear },   // -60: off
    { "top_pitch",   "Top Pitch",    "st", -24.0f,     0.0f,   -8.0f, Curve::Linear },
    { "top_decay",   "Top Decay",    "ms",  20.0f,   400.0f,   90.0f, Curve::Log },
    { "top_drive",   "Top Drive",    "",     0.0f,     1.0f,   0.60f, Curve::Linear },
    { "top_cut",     "Top Cut",      "Hz", 150.0f,  2000.0f,  400.0f, Curve::Log },
};

/**
 * The rumble (PLAN 5.2, Dok. 8.4). The hall 1 to 4 s (TrackSensei 1 to 2 s, Dark Cinematic 3.79 s), a clip of
 * 4 to 6 dB, a low pass at two to four times the kick's pitch; the sine under the split a few dB under the band
 * above; the duck 4 to 8 dB with a hold about the kick's body and a release of 150 to 350 ms.
 */
const ParamDesc kRumbleParams[rumble::Count] = {
    { "level",        "Level",        "dB", -60.0f,   6.0f,  -9.0f, Curve::Linear },   // -60: off
    { "split",        "Split",        "Hz",  50.0f, 150.0f,  80.0f, Curve::Log },
    { "decay",        "Decay",        "s",    0.5f,   6.0f,   1.8f, Curve::Log },
    { "size",         "Size",         "",     0.3f,   3.0f,   1.0f, Curve::Log },
    { "damping",      "Damping",      "",     0.0f,   1.0f,   0.5f, Curve::Linear },
    { "predelay",     "Pre-Delay",    "ms",   0.0f,  60.0f,   0.0f, Curve::Linear },
    { "drive",        "Drive",        "dB",   0.0f,  18.0f,   6.0f, Curve::Linear },
    { "ratio",        "Low Pass",     "x f0", 1.5f,   6.0f,   3.0f, Curve::Log },
    { "resonance",    "Resonance",    "",     0.0f,   1.0f,  0.15f, Curve::Linear },
    { "sub",          "Sub",          "dB", -60.0f,   6.0f,  -4.0f, Curve::Linear },   // -60: off
    { "sub_attack",   "Sub Attack",   "ms",   5.0f, 200.0f,  25.0f, Curve::Log },
    { "sub_release",  "Sub Release",  "ms",  50.0f, 2000.0f, 300.0f, Curve::Log },
    { "duck",         "Duck",         "dB",   0.0f,  24.0f,   7.0f, Curve::Linear },
    { "duck_hold",    "Duck Hold",    "ms",   0.0f, 250.0f,  80.0f, Curve::Linear },
    { "duck_release", "Duck Release", "ms",  30.0f, 600.0f, 220.0f, Curve::Log },
};

/** The sub bass (PLAN 5.3): a sine, a low pass at 80 to 120 Hz, the kick's phase, a duck of 6 to 12 dB. */
const ParamDesc kSubParams[sub::Count] = {
    { "level",        "Level",        "dB", -60.0f,   6.0f,  -6.0f, Curve::Linear },
    { "octave",       "Octave",       "",    -2.0f,   2.0f,   0.0f, Curve::Int },
    { "attack",       "Attack",       "ms",   0.5f,  50.0f,   3.0f, Curve::Log },
    { "decay",        "Decay",        "ms",  20.0f, 2000.0f, 250.0f, Curve::Log },
    { "sustain",      "Sustain",      "",     0.0f,   1.0f,   0.7f, Curve::Linear },
    { "release",      "Release",      "ms",  10.0f, 800.0f,  90.0f, Curve::Log },
    { "low_pass",     "Low Pass",     "Hz",  40.0f, 300.0f, 110.0f, Curve::Log },
    { "drive",        "Drive",        "",     0.0f,   1.0f,   0.1f, Curve::Linear },
    { "lock",         "Kick Lock",    "",     0.0f,   1.0f,   1.0f, Curve::Choice, kLockNames },
    { "duck",         "Duck",         "dB",   0.0f,  24.0f,   9.0f, Curve::Linear },
    { "duck_hold",    "Duck Hold",    "ms",   0.0f, 250.0f,  40.0f, Curve::Linear },
    { "duck_release", "Duck Release", "ms",  30.0f, 600.0f, 180.0f, Curve::Log },
};

/** A percussion lane: Phosphene's table (Perc.h there), then the noise type. */
const ParamDesc kPercParams[perc::Count] = {
    { "active",        "Active",        "",      0.0f,     1.0f,    1.0f, Curve::Toggle },
    { "role",          "Role",          "",      0.0f,    11.0f,    0.0f, Curve::Choice, kPercRoleNames },
    { "engine",        "Engine",        "",      0.0f,     4.0f,    0.0f, Curve::Choice, kPercEngineNames },
    { "pitch",         "Pitch",         "Hz",   40.0f, 12000.0f,  400.0f, Curve::Log },
    { "pitch_amount",  "Pitch Amount",  "x",     1.0f,    16.0f,    1.0f, Curve::Log },
    { "pitch_decay",   "Pitch Decay",   "ms",    0.5f,   300.0f,   10.0f, Curve::Log },
    { "fm_ratio",      "FM Ratio",      "",     0.25f,     8.0f,   1.41f, Curve::Linear },
    { "fm_index",      "FM Index",      "",      0.0f,     8.0f,    0.0f, Curve::Linear },
    { "mode_set",      "Modes",         "",      0.0f,     2.0f,    0.0f, Curve::Choice, kModeSetNames },
    { "mode_damp",     "Mode Damping",  "",      0.0f,     1.0f,    0.5f, Curve::Linear },
    { "metal_scale",   "Metal Scale",   "x",    0.25f,     4.0f,    1.0f, Curve::Log },
    { "noise",         "Noise",         "",      0.0f,     1.0f,    0.0f, Curve::Linear },
    { "noise_decay",   "Noise Decay",   "ms",    2.0f,  3000.0f,   60.0f, Curve::Log },
    { "bursts",        "Bursts",        "",      1.0f,     6.0f,    1.0f, Curve::Int },
    { "burst_spacing", "Burst Spacing", "ms",    2.0f,    40.0f,   10.0f, Curve::Linear },
    { "decay",         "Decay",         "ms",    2.0f,  3000.0f,  120.0f, Curve::Log },
    { "filter",        "Filter",        "",      0.0f,     2.0f,    2.0f, Curve::Choice, kPercFilterNames },
    { "cutoff",        "Cutoff",        "Hz",  100.0f, 18000.0f, 8000.0f, Curve::Log },
    { "resonance",     "Resonance",     "",      0.0f,     1.0f,    0.2f, Curve::Linear },
    { "low_cut",       "Low Cut",       "Hz",  150.0f,  8000.0f,  150.0f, Curve::Log },
    { "drive",         "Drive",         "",      0.0f,     1.0f,    0.0f, Curve::Linear },
    { "level",         "Level",         "dB",  -36.0f,     6.0f,  -12.0f, Curve::Linear },
    { "pan",           "Pan",           "",     -1.0f,     1.0f,    0.0f, Curve::Linear },
    { "choke",         "Choke Group",   "",      0.0f,     4.0f,    0.0f, Curve::Int },
    { "shift",         "Shift",         "ms",  -10.0f,    10.0f,    0.0f, Curve::Linear },
    { "density",       "Density",       "",      0.0f,     1.0f,    0.5f, Curve::Linear },
    { "tune",          "Tune to Key",   "",      0.0f,     1.0f,    0.0f, Curve::Toggle },
    { "pan_depth",     "Pan Depth",     "",      0.0f,     1.0f,    0.0f, Curve::Linear },
    { "pan_bars",      "Pan Period",    "bars",  0.0625f, 16.0f,  0.1875f, Curve::Log },
    { "cut_track",     "Cut Tracks Pitch","",    0.0f,     2.0f,    0.0f, Curve::Linear },
    { "noise_type",    "Noise Type",    "",      0.0f,     1.0f,    0.0f, Curve::Choice, kNoiseTypeNames },
};

const ParamDesc kMixParams[mix::Count] = {
    { "hats_level", "Hats Level", "dB", -24.0f,    12.0f,     0.0f, Curve::Linear },
    { "hats_cut",   "Hats Cut",   "Hz", 200.0f, 20000.0f, 20000.0f, Curve::Log },
    { "perc_level", "Perc Level", "dB", -24.0f,    12.0f,     0.0f, Curve::Linear },
    { "perc_cut",   "Perc Cut",   "Hz", 200.0f, 20000.0f, 20000.0f, Curve::Log },
};

/**
 * The master (PLAN 8.4, 8.5): a glue compressor (threshold -16 dB and a gentle ratio after Dok. 8.7), a soft
 * clipper at four times the rate, a true-peak limiter at -1 dBTP (Dok.: at most -0.5), the side mono under 120 Hz.
 */
const ParamDesc kMasterParams[master::Count] = {
    { "level",      "Level",      "dB", -24.0f,  12.0f,   0.0f, Curve::Linear },
    { "threshold",  "Glue Threshold", "dB", -40.0f, 0.0f, -16.0f, Curve::Linear },
    { "ratio",      "Glue Ratio", "",     1.0f,  10.0f,   2.0f, Curve::Log },
    { "clip",       "Clip Drive", "dB",   0.0f,   9.0f,   2.0f, Curve::Linear },
    { "ceiling",    "Ceiling",    "dBTP", -6.0f,  0.0f,  -1.0f, Curve::Linear },
    { "mono_below", "Mono Below", "Hz",  40.0f, 250.0f, 120.0f, Curve::Log },
};

/**
 * The default kit (PLAN 5.4): what each of the twelve lanes is and how it sounds before a preset touches it. The
 * hats play the 909's metal table (Kit.h), 50 to 80 ms closed and 200 to 600 open, high-passed at 7 kHz or
 * band-passed at 10 kHz (Dok. 8.4); the ride the 808's six squares; the clap four bursts 10 to 20 ms apart through a
 * band pass at 1.1 kHz; the tom an octave above the old recipe's eight semitones over the kick (the low-end rule,
 * PLAN 5.4). Levels: a full-velocity hit of each lane peaks against the kick's peak where Dok. 8.7's reference levels
 * put it -- CH -8, OH -10, perc -15, FX -10 dB, the rest inferred (selftest testKitLevels, which holds them to 1.5 dB).
 */
const char* const kDefaultKit =
    "perc1.role=Closed Hat; perc1.engine=Noise; perc1.noise_type=909 Metal; perc1.noise=1; perc1.noise_decay=70; perc1.decay=70;"
    "perc1.filter=High Pass; perc1.cutoff=7000; perc1.resonance=0.15; perc1.low_cut=3000; perc1.drive=0.1; perc1.level=-0.7;"
    "perc1.pan=0.1; perc1.choke=1; perc1.pan_depth=0.3\n"
    "perc2.role=Rolling Hat; perc2.engine=Noise; perc2.noise_type=909 Metal; perc2.metal_scale=1.15; perc2.noise=1;"
    "perc2.noise_decay=38; perc2.decay=38; perc2.filter=Band Pass; perc2.cutoff=10000; perc2.resonance=0.2; perc2.low_cut=5000;"
    "perc2.level=-4.2; perc2.pan=-0.15; perc2.choke=1; perc2.pan_depth=0.4\n"
    "perc3.role=Open Hat; perc3.engine=Noise; perc3.noise_type=909 Metal; perc3.metal_scale=0.95; perc3.noise=1;"
    "perc3.noise_decay=320; perc3.decay=320; perc3.filter=High Pass; perc3.cutoff=6500; perc3.resonance=0.1; perc3.low_cut=3000;"
    "perc3.level=-2.7; perc3.pan=0.1; perc3.choke=1\n"
    "perc4.role=Ride; perc4.engine=Metal; perc4.metal_scale=2.2; perc4.noise=0.5; perc4.noise_decay=700; perc4.decay=700;"
    "perc4.filter=High Pass; perc4.cutoff=4000; perc4.low_cut=1000; perc4.level=-14.0; perc4.pan=-0.3\n"
    "perc5.role=Clap; perc5.engine=Noise; perc5.noise_decay=160; perc5.bursts=4; perc5.burst_spacing=12; perc5.filter=Band Pass;"
    "perc5.cutoff=1150; perc5.resonance=0.55; perc5.low_cut=300; perc5.drive=0.2; perc5.level=-1.7\n"
    "perc6.role=Clap Ghost; perc6.engine=Noise; perc6.noise_decay=120; perc6.bursts=3; perc6.burst_spacing=11; perc6.filter=Band Pass;"
    "perc6.cutoff=1250; perc6.resonance=0.5; perc6.low_cut=300; perc6.drive=0.2; perc6.level=-9.7; perc6.pan=0.2\n"
    "perc7.role=Snare; perc7.engine=Tone; perc7.pitch=190; perc7.pitch_amount=1.6; perc7.pitch_decay=20; perc7.decay=110;"
    "perc7.noise=0.5; perc7.noise_decay=90; perc7.filter=Low Pass; perc7.cutoff=2500; perc7.level=-14\n"
    "perc8.role=Rim; perc8.engine=Tone; perc8.pitch=1700; perc8.pitch_amount=1.2; perc8.pitch_decay=3; perc8.decay=25;"
    "perc8.noise=0.15; perc8.noise_decay=8; perc8.filter=Band Pass; perc8.cutoff=1900; perc8.resonance=0.35; perc8.low_cut=400;"
    "perc8.drive=0.6; perc8.level=-5.5; perc8.tune=1; perc8.pan=-0.2\n"
    "perc9.role=Shaker; perc9.engine=Noise; perc9.noise_decay=65; perc9.filter=High Pass; perc9.cutoff=6000; perc9.resonance=0.3;"
    "perc9.low_cut=2000; perc9.drive=0.3; perc9.level=-10.1; perc9.pan=0.35; perc9.pan_depth=0.5\n"
    "perc10.role=Tom; perc10.engine=Tone; perc10.pitch=175; perc10.pitch_amount=1.5; perc10.pitch_decay=60; perc10.decay=350;"
    "perc10.noise=0.05; perc10.noise_decay=20; perc10.filter=Low Pass; perc10.cutoff=3000; perc10.level=-15; perc10.tune=1;"
    "perc10.pan=-0.25\n"
    "perc11.role=Conga; perc11.engine=Modal; perc11.mode_set=Harmonic; perc11.pitch=320; perc11.decay=260; perc11.filter=Low Pass;"
    "perc11.cutoff=5000; perc11.level=-16; perc11.tune=1; perc11.pan=0.3\n"
    "perc12.role=Noise; perc12.engine=Noise; perc12.noise_decay=2500; perc12.filter=Band Pass; perc12.cutoff=2000;"
    "perc12.resonance=0.4; perc12.low_cut=300; perc12.level=-7.0\n";

struct ModuleSpec {
    const char* prefix;
    const ParamDesc* descs;
    int count;
    int instances;
};

const ModuleSpec kModules[static_cast<int>(Module::Count)] = {
    { "compose", kComposeParams, compose::Count, 1 },
    { "kick",    kKickParams,    kick::Count,    1 },
    { "rumble",  kRumbleParams,  rumble::Count,  1 },
    { "sub",     kSubParams,     sub::Count,     1 },
    { "perc",    kPercParams,    perc::Count,    kPercLanes },
    { "mix",     kMixParams,     mix::Count,     1 },
    { "master",  kMasterParams,  master::Count,  1 },
};

bool isDiscrete(Curve c) { return c == Curve::Int || c == Curve::Choice || c == Curve::Toggle; }

std::string_view trim(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

/** @brief Lower case without spaces, for matching choice names ("909 Metal" = "909metal"). */
std::string foldName(std::string_view s)
{
    std::string out;
    for (char ch : s) {
        if (ch == ' ' || ch == '_' || ch == '-') continue;
        out += (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
    }
    return out;
}

} // namespace

const ScaleDef& scaleDef(int scale)
{
    return kScales[scale >= 0 && scale < static_cast<int>(Scale::Count) ? scale : 0];
}

bool inScale(int scale, int semitones)
{
    const ScaleDef& d = scaleDef(scale);
    const int pc = ((semitones % 12) + 12) % 12;
    for (int i = 0; i < d.size; ++i) if (d.steps[i] == pc) return true;
    return false;
}

ParamStore::ParamStore()
{
    for (auto& row : bases_) for (int& b : row) b = -1;
    int total = 0;
    for (const ModuleSpec& m : kModules) total += m.count * m.instances;
    entries_.reserve(static_cast<size_t>(total));
    for (int mi = 0; mi < static_cast<int>(Module::Count); ++mi) {
        const ModuleSpec& m = kModules[mi];
        for (int inst = 0; inst < m.instances && inst < kMaxInstances; ++inst) {
            bases_[mi][inst] = static_cast<int>(entries_.size());
            std::string prefix = m.prefix;
            if (m.instances > 1) prefix += std::to_string(inst + 1);
            for (int p = 0; p < m.count; ++p) {
                Entry e{ &m.descs[p], prefix + "." + m.descs[p].key, static_cast<Module>(mi), inst };
                index_.emplace(e.key, static_cast<int>(entries_.size()));
                entries_.push_back(std::move(e));
            }
        }
    }
    values_ = std::make_unique<std::atomic<float>[]>(entries_.size());
    // Defaults: the descriptors', then the lanes' own on top.
    defaults_.resize(entries_.size());
    for (int i = 0; i < count(); ++i) {
        defaults_[static_cast<size_t>(i)] = desc(i).defValue;
        values_[static_cast<size_t>(i)].store(desc(i).defValue, std::memory_order_relaxed);
    }
    parseText(kDefaultKit);
    for (int i = 0; i < count(); ++i) defaults_[static_cast<size_t>(i)] = get(i);
}

int ParamStore::base(Module m, int instance) const
{
    const int mi = static_cast<int>(m);
    if (mi < 0 || mi >= static_cast<int>(Module::Count) || instance < 0 || instance >= kMaxInstances) return -1;
    return bases_[mi][instance];
}

int ParamStore::find(std::string_view key) const
{
    const auto it = index_.find(std::string(key));
    return it == index_.end() ? -1 : it->second;
}

int ParamStore::getInt(int id) const
{
    return static_cast<int>(std::lround(get(id)));
}

void ParamStore::set(int id, float value)
{
    if (id < 0 || id >= count()) return;
    const ParamDesc& d = desc(id);
    if (!(value == value)) value = defaults_.empty() ? d.defValue : defaults_[static_cast<size_t>(id)];   // NaN
    float v = value < d.minValue ? d.minValue : (value > d.maxValue ? d.maxValue : value);
    if (isDiscrete(d.curve)) v = std::round(v);
    values_[static_cast<size_t>(id)].store(v, std::memory_order_relaxed);
}

float ParamStore::toNormalised(int id, float value) const
{
    const ParamDesc& d = desc(id);
    if (d.maxValue <= d.minValue) return 0.0f;
    float n;
    if (d.curve == Curve::Log) n = std::log(value / d.minValue) / std::log(d.maxValue / d.minValue);
    else n = (value - d.minValue) / (d.maxValue - d.minValue);
    return n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n);
}

float ParamStore::fromNormalised(int id, float norm) const
{
    const ParamDesc& d = desc(id);
    const float n = norm < 0.0f ? 0.0f : (norm > 1.0f ? 1.0f : norm);
    float v;
    if (d.curve == Curve::Log) v = d.minValue * std::pow(d.maxValue / d.minValue, n);
    else v = d.minValue + n * (d.maxValue - d.minValue);
    if (isDiscrete(d.curve)) v = std::round(v);
    return v;
}

void ParamStore::resetDefaults()
{
    for (int i = 0; i < count(); ++i) values_[static_cast<size_t>(i)].store(defaults_[static_cast<size_t>(i)], std::memory_order_relaxed);
}

int ParamStore::moduleCount(Module m)
{
    const int mi = static_cast<int>(m);
    return mi >= 0 && mi < static_cast<int>(Module::Count) ? kModules[mi].count : 0;
}

void ParamStore::readModule(Module m, int instance, float* out) const
{
    const int b = base(m, instance);
    if (b < 0) return;
    const int n = moduleCount(m);
    for (int i = 0; i < n; ++i) out[i] = get(b + i);
}

void ParamStore::copyValuesFrom(const ParamStore& other)
{
    const int n = count() < other.count() ? count() : other.count();
    for (int i = 0; i < n; ++i) values_[static_cast<size_t>(i)].store(other.get(i), std::memory_order_relaxed);
}

bool ParamStore::parseText(std::string_view text, std::string* error)
{
    // Split into assignments. Newlines and ';' always separate; whitespace separates only where the next word
    // contains '=' -- so a choice name with a space ("perc1.noise_type=909 Metal") stays one value while "a=1 b=2"
    // is still two assignments. '#' starts a comment to the end of the line.
    std::vector<std::string> items;
    size_t pos = 0;
    bool newItem = true;
    while (pos < text.size()) {
        const char ch = text[pos];
        if (ch == '\n' || ch == ';' || ch == '\r') { newItem = true; ++pos; continue; }
        if (ch == ' ' || ch == '\t') { ++pos; continue; }
        if (ch == '#') { while (pos < text.size() && text[pos] != '\n') ++pos; continue; }
        size_t end = pos;
        while (end < text.size() && text[end] != '\n' && text[end] != ';' && text[end] != '\r' && text[end] != ' ' && text[end] != '\t') ++end;
        const std::string_view word = text.substr(pos, end - pos);
        pos = end;
        if (newItem || word.find('=') != std::string_view::npos || items.empty()) items.emplace_back(word);
        else { items.back() += ' '; items.back() += word; }
        newItem = false;
    }

    bool ok = true;
    for (const std::string& item : items) {
        const std::string_view tok = trim(item);
        const size_t eq = tok.find('=');
        if (eq == std::string_view::npos) {
            if (error && ok) *error = "missing '=' in \"" + std::string(tok) + "\"";
            ok = false;
            continue;
        }
        const std::string_view k = trim(tok.substr(0, eq)), v = trim(tok.substr(eq + 1));
        const int id = find(k);
        if (id < 0) {
            if (error && ok) *error = "unknown parameter \"" + std::string(k) + "\"";
            ok = false;
            continue;
        }
        const ParamDesc& d = desc(id);
        bool matched = false;
        if (d.choices != nullptr || d.curve == Curve::Toggle) {
            const std::string fv = foldName(v);
            if (d.curve == Curve::Toggle && (fv == "on" || fv == "off")) { set(id, fv == "on" ? 1.0f : 0.0f); matched = true; }
            for (int c = 0; !matched && d.choices != nullptr && c <= static_cast<int>(d.maxValue); ++c) {
                if (fv == foldName(d.choices[c])) { set(id, static_cast<float>(c)); matched = true; }
            }
        }
        if (!matched) {
            const std::string vs(v);
            char* stop = nullptr;
            const double x = std::strtod(vs.c_str(), &stop);
            if (vs.empty() || stop == nullptr || *stop != 0) {
                if (error && ok) *error = "bad value \"" + vs + "\" for " + std::string(k);
                ok = false;
                continue;
            }
            set(id, static_cast<float>(x));
        }
    }
    return ok;
}

std::string ParamStore::toText(bool onlyChanged) const
{
    std::string out;
    char buf[64];
    for (int i = 0; i < count(); ++i) {
        const float v = get(i);
        if (onlyChanged && v == defaults_[static_cast<size_t>(i)]) continue;
        // %.9g round-trips every float exactly.
        std::snprintf(buf, sizeof(buf), "%.9g", static_cast<double>(v));
        out += key(i);
        out += '=';
        out += buf;
        out += '\n';
    }
    return out;
}

std::string ParamStore::format(int id) const
{
    const ParamDesc& d = desc(id);
    const float v = get(id);
    if (d.curve == Curve::Choice && d.choices != nullptr) return d.choices[getInt(id)];
    if (d.curve == Curve::Toggle) return v >= 0.5f ? "On" : "Off";
    char buf[64];
    if (d.curve == Curve::Int) std::snprintf(buf, sizeof(buf), "%d", getInt(id));
    else if (std::fabs(v) >= 100.0f) std::snprintf(buf, sizeof(buf), "%.0f", static_cast<double>(v));
    else if (std::fabs(v) >= 10.0f) std::snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(v));
    else std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(v));
    std::string s = buf;
    if (d.unit != nullptr && d.unit[0] != 0) { s += ' '; s += d.unit; }
    return s;
}

} // namespace umb
