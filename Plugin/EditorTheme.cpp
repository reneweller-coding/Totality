/**
 * @file EditorTheme.cpp
 * @brief The palette, the families, the modules' panels and the look and feel (EditorTheme.h).
 */
#include "EditorTheme.h"
#include "TotalityData.h"
#include <cmath>

void drawLogo(juce::Graphics& g, juce::Rectangle<float> r);   ///< PluginEditor.cpp

namespace totui {

using namespace colour;

juce::Colour familyColour(Family f)
{
    switch (f) {
    case Family::Source:   return juce::Colour(0xffe6c178);   // the corona's gold
    case Family::Filter:   return juce::Colour(0xffd9825b);   // copper
    case Family::Envelope: return juce::Colour(0xff9fbf6f);   // sage
    case Family::Motion:   return juce::Colour(0xff6fb8ae);   // teal
    default:               return juce::Colour(0xff6f8fb8);   // steel blue
    }
}

juce::Colour deckColour(int d)
{
    static const juce::uint32 kDeck[3] = { 0xffe6c178, 0xff6fb8ae, 0xffc47fb0 };
    return juce::Colour(kDeck[static_cast<size_t>(juce::jlimit(0, 2, d))]);
}

const std::vector<GroupSpec>& layoutOf(tot::Module m)
{
    using F = Family;
    using M = tot::Module;
    static const std::vector<GroupSpec> none;
    static const std::vector<GroupSpec> kick = {
        { "Engine", F::Source, { "~engine", "*tune", "level", "drive", "clip" } },
        { "Pitch", F::Envelope, { "pitch_start", "pitch_end", "pitch_decay" } },
        { "Punch and Amp", F::Envelope, { "punch", "punch_decay", "amp_attack", "amp_hold", "*amp_decay", "tail_limit" } },
        { "Click", F::Source, { "click_level", "click_tone", "click_decay" } },
        { "Tone", F::Filter, { "tone", "low_cut", "dip_freq", "dip" } },
        { "Top Layer", F::Source, { "top_level", "top_pitch", "top_decay", "top_drive", "top_cut" } },
    };
    static const std::vector<GroupSpec> rumble = {
        { "Rumble", F::Source, { "*level", "*split", "drive", "resonance" } },
        { "Room", F::Space, { "decay", "size", "damping", "predelay", "ratio" } },
        { "Sub", F::Envelope, { "sub", "sub_attack", "sub_release" } },
        { "Duck", F::Motion, { "duck", "duck_hold", "duck_release" } },
    };
    static const std::vector<GroupSpec> sub = {
        { "Sub", F::Source, { "*level", "octave", "drive", "low_pass", "lock" } },
        { "Envelope", F::Envelope, { "attack", "decay", "sustain", "release" } },
        { "Duck", F::Motion, { "duck", "duck_hold", "duck_release" } },
    };
    static const std::vector<GroupSpec> perc = {
        { "Lane", F::Source, { "active", "~role", "~engine", "*level", "pan", "choke", "density" } },
        { "Pitch", F::Source, { "*pitch", "tune", "pitch_amount", "pitch_decay" } },
        { "FM, Modes, Metal", F::Source, { "fm_ratio", "fm_index", "~mode_set", "mode_damp", "metal_scale" } },
        { "Noise", F::Source, { "noise", "~noise_type", "noise_decay", "bursts", "burst_spacing" } },
        { "Shape", F::Envelope, { "*decay", "shift" } },
        { "Filter", F::Filter, { "~filter", "*cutoff", "resonance", "low_cut", "drive", "cut_track" } },
        { "Space", F::Space, { "pan_depth", "pan_bars" } },
    };
    static const std::vector<GroupSpec> mix = {
        { "Hats", F::Space, { "*hats_level", "hats_cut" } },
        { "Perc", F::Space, { "*perc_level", "perc_cut" } },
        { "Drum Bus", F::Source, { "drum_sat", "low_cut" } },
        { "Ducking", F::Motion, { "duck_low", "duck_mid" } },
    };
    static const std::vector<GroupSpec> master = {
        { "Tone", F::Filter, { "*tilt", "mono_below", "cut" } },
        { "Glue", F::Envelope, { "threshold", "ratio" } },
        { "Out", F::Space, { "*level", "clip", "ceiling" } },
    };
    static const std::vector<GroupSpec> ping = {
        { "Voice", F::Source, { "*level", "pan", "width", "ratio", "*index", "index_decay", "pitch_amount", "pitch_decay" } },
        { "Gate", F::Envelope, { "decay", "lpg", "lpg_release", "resonance" } },
        { "Band", F::Filter, { "band", "band_q", "band_mix", "sweep", "sweep_rate" } },
    };
    static const std::vector<GroupSpec> synth = {
        { "Oscillator", F::Source, { "*level", "~wave", "pulse_width", "sub_osc", "pan", "glide", "drive" } },
        { "Filter", F::Filter, { "~filter", "*cutoff", "*resonance", "env_amount", "decay", "accent", "key_track", "low_cut", "high_cut" } },
        { "Amp", F::Envelope, { "amp_attack", "amp_decay", "amp_sustain", "amp_release" } },
        { "Sends and Duck", F::Space, { "dub_send", "room_send", "duck", "duck_release" } },
    };
    static const std::vector<GroupSpec> chord = {
        { "Stab", F::Source, { "*level", "width", "detune", "octave", "bright" } },
        { "Envelope", F::Envelope, { "attack", "decay", "sustain", "release", "env_amount" } },
        { "Band", F::Filter, { "*band", "~band_mode", "band_q", "sweep", "sweep_rate", "dip" } },
        { "Colour", F::Motion, { "crush", "crush_mix", "phaser", "phaser_rate" } },
        { "Sends", F::Space, { "dub_send", "plate_send" } },
    };
    static const std::vector<GroupSpec> drone = {
        { "Drone", F::Source, { "*level", "octave", "detune" } },
        { "Filter", F::Filter, { "*cutoff", "resonance", "sweep", "sweep_bars" } },
        { "Envelope", F::Envelope, { "attack", "release" } },
        { "Sends", F::Space, { "plate_send", "room_send" } },
    };
    static const std::vector<GroupSpec> texture = {
        { "Texture", F::Source, { "*level", "crackle", "hum", "hum_hz", "erosion", "width" } },
    };
    static const std::vector<GroupSpec> dub = {
        { "Tape Echo", F::Motion, { "~echo_time", "*feedback", "tone", "low_cut", "wow", "flutter", "drive", "echo_return" } },
        { "Spring", F::Space, { "spring", "spring_decay" } },
        { "Plate", F::Space, { "plate_decay", "plate_damping", "plate_predelay", "plate_low_cut", "plate_return" } },
        { "Sends", F::Space, { "ping_send", "hats_send", "perc_send" } },
    };
    static const std::vector<GroupSpec> space = {
        { "Room", F::Space, { "*level", "size", "*decay", "damping", "predelay", "low_cut", "high_cut" } },
        { "Sends", F::Space, { "hats_send", "perc_send", "ping_send" } },
    };
    static const std::vector<GroupSpec> cloud = {
        { "Grains", F::Motion, { "*level", "density", "size", "pitch", "spray" } },
        { "Sends", F::Space, { "ping_send", "chord_send", "plate_send" } },
    };
    static const std::vector<GroupSpec> motion = {
        { "Motion", F::Motion, { "*amount", "hats_cut", "hats_level", "hat_decay", "perc_cut", "rumble_drive" } },
    };
    static const std::vector<GroupSpec> compose = {
        { "Track", F::Source, { "~style", "~archetype", "~key", "~scale", "*bpm", "minutes", "~form", "auto" } },
        { "Style Morph", F::Motion, { "~morph_to", "morph", "dub_share", "hypnotic_share" } },
        { "Feel", F::Envelope, { "swing", "humanize", "~low_owner" } },
    };
    static const std::vector<GroupSpec> set = {
        { "Set", F::Source, { "*minutes", "~dramaturgy", "~journey", "~blend", "track_minutes" } },
        { "Live", F::Motion, { "loops", "fx_breaks", "dj_hand" } },
    };
    static const std::vector<GroupSpec> deck = {
        { "Channel", F::Space, { "*fader", "low", "mid", "high", "*filter", "fx_send" } },
    };
    static const std::vector<GroupSpec> djfx = {
        { "Mixer Effects", F::Motion, { "~echo_time", "feedback", "echo_return", "hall_decay", "hall_return" } },
    };
    static const std::vector<GroupSpec> cue = {
        { "OSC Cues", F::Motion, { "enabled", "port" } },
    };
    static const std::vector<GroupSpec> custom = {
        { "Use", F::Source, { "use" } },
        { "Tempo and Length", F::Source, { "bpm_low", "bpm_high", "blocks_low", "blocks_high" } },
        { "Form", F::Envelope, { "arc", "peak", "endless", "max_reduction", "edge" } },
        { "Rack", F::Motion, { "mutation", "reroll", "polymeter", "fill", "swing_low", "swing_high", "density_cap" } },
        { "Events and Corridor", F::Motion, { "event_rate", "throw_share", "similarity", "sub_chance", "peak_lufs" } },
    };
    switch (m) {
    case M::Kick: return kick;
    case M::Rumble: return rumble;
    case M::Sub: return sub;
    case M::Perc: return perc;
    case M::Mix: return mix;
    case M::Master: return master;
    case M::Ping: return ping;
    case M::Motion: return motion;
    case M::Space: return space;
    case M::Bass: case M::Acid: return synth;
    case M::Chord: return chord;
    case M::Drone: return drone;
    case M::Texture: return texture;
    case M::Dub: return dub;
    case M::Cloud: return cloud;
    case M::Compose: return compose;
    case M::Set: return set;
    case M::Deck: return deck;
    case M::DjFx: return djfx;
    case M::Cue: return cue;
    case M::Custom: return custom;
    default: return none;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

const frame::Skin& skin()
{
    static const frame::Skin s = [] {
        frame::Skin k;
        k.name = "Totality";
        k.bg = bg; k.panel = panel; k.group = group; k.raised = raised; k.edge = edge;
        k.ink = ink; k.dim = dim; k.faint = faint; k.accent = accent; k.onset = onset; k.good = green; k.bad = red;
        for (int f = 0; f < 5; ++f) k.families[f] = familyColour(static_cast<Family>(f));
        for (int d = 0; d < 3; ++d) k.decks[d] = deckColour(d);
        k.radius = 2.0f;               // concrete: hard corners
        k.tracking = 0.32f;            // T O T A L I T Y
        k.typeface = "Bahnschrift";    // a condensed DIN, as on Berlin's signs
        k.titleBold = false;
        k.backdropData = TotalityData::backdrop_jpg;
        k.backdropSize = TotalityData::backdrop_jpgSize;
        k.backdropTop = 0.85f;
        k.backdropPage = 0.5f;
        k.logo = [](juce::Graphics& g, juce::Rectangle<float> r) { drawLogo(g, r); };
        return k;
    }();
    return s;
}

} // namespace totui
