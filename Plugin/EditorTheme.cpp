/**
 * @file EditorTheme.cpp
 * @brief The palette, the families, the modules' panels and the look and feel (EditorTheme.h).
 * @author Rene Weller
 */
#include "EditorTheme.h"
#include "PluginEditor.h"   // drawLogo
#include "TotalityData.h"
#include <cmath>

namespace totui
{

using namespace colour;

juce::Colour familyColour( Family f )
{
    switch ( f )
    {
    case Family::SOURCE: return juce::Colour( 0xffe6c178 );     // the corona's gold
    case Family::FILTER: return juce::Colour( 0xffd9825b );     // copper
    case Family::ENVELOPE: return juce::Colour( 0xff9fbf6f );   // sage
    case Family::MOTION: return juce::Colour( 0xff6fb8ae );     // teal
    default: return juce::Colour( 0xff6f8fb8 );                 // steel blue
    }
}

juce::Colour deckColour( int d )
{
    static const juce::uint32 Deck[3] = { 0xffe6c178, 0xff6fb8ae, 0xffc47fb0 };
    return juce::Colour( Deck[static_cast<size_t>( juce::jlimit( 0, 2, d ) )] );
}

const std::vector<GroupSpec> &layoutOf( tot::ModuleE m )
{
    using F = Family;
    using MT = tot::ModuleE;
    static const std::vector<GroupSpec> none;
    static const std::vector<GroupSpec> kick = {
        { "Engine", F::SOURCE, { "~engine", "*tune", "level", "drive", "clip" } },
        { "Pitch", F::ENVELOPE, { "pitch_start", "pitch_end", "pitch_decay" } },
        { "Punch and Amp",
          F::ENVELOPE,
          { "punch", "punch_decay", "amp_attack", "amp_hold", "*amp_decay", "tail_limit" } },
        { "Click", F::SOURCE, { "click_level", "click_tone", "click_decay" } },
        { "Tone", F::FILTER, { "tone", "low_cut", "dip_freq", "dip" } },
        { "Top Layer", F::SOURCE, { "top_level", "top_pitch", "top_decay", "top_drive", "top_cut" } },
    };
    static const std::vector<GroupSpec> rumble = {
        { "Rumble", F::SOURCE, { "*level", "*split", "drive", "resonance" } },
        { "Room", F::SPACE, { "decay", "size", "damping", "predelay", "ratio" } },
        { "Sub", F::ENVELOPE, { "sub", "sub_attack", "sub_release" } },
        { "Duck", F::MOTION, { "duck", "duck_hold", "duck_release" } },
    };
    static const std::vector<GroupSpec> sub = {
        { "Sub", F::SOURCE, { "*level", "octave", "drive", "low_pass", "lock" } },
        { "Envelope", F::ENVELOPE, { "attack", "decay", "sustain", "release" } },
        { "Duck", F::MOTION, { "duck", "duck_hold", "duck_release" } },
    };
    static const std::vector<GroupSpec> perc = {
        { "Lane", F::SOURCE, { "active", "~role", "~engine", "*level", "pan", "choke", "density" } },
        { "Pitch", F::SOURCE, { "*pitch", "tune", "pitch_amount", "pitch_decay" } },
        { "FM, Modes, Metal", F::SOURCE, { "fm_ratio", "fm_index", "~mode_set", "mode_damp", "metal_scale" } },
        { "Noise", F::SOURCE, { "noise", "~noise_type", "noise_decay", "bursts", "burst_spacing" } },
        { "Shape", F::ENVELOPE, { "*decay", "shift" } },
        { "Filter", F::FILTER, { "~filter", "*cutoff", "resonance", "low_cut", "drive", "cut_track" } },
        { "Space", F::SPACE, { "pan_depth", "pan_bars" } },
    };
    static const std::vector<GroupSpec> mix = {
        { "Hats", F::SPACE, { "*hats_level", "hats_cut" } },
        { "Perc", F::SPACE, { "*perc_level", "perc_cut" } },
        { "Drum Bus", F::SOURCE, { "drum_sat", "low_cut" } },
        { "Ducking", F::MOTION, { "duck_low", "duck_mid" } },
    };
    static const std::vector<GroupSpec> master = {
        { "Tone", F::FILTER, { "*tilt", "mono_below", "cut" } },
        { "Glue", F::ENVELOPE, { "threshold", "ratio" } },
        { "Out", F::SPACE, { "*level", "clip", "ceiling" } },
    };
    static const std::vector<GroupSpec> ping = {
        { "Voice",
          F::SOURCE,
          { "*level", "pan", "width", "ratio", "*index", "index_decay", "pitch_amount", "pitch_decay" } },
        { "Gate", F::ENVELOPE, { "decay", "lpg", "lpg_release", "resonance" } },
        { "Band", F::FILTER, { "band", "band_q", "band_mix", "sweep", "sweep_rate" } },
    };
    static const std::vector<GroupSpec> synth = {
        { "Oscillator", F::SOURCE, { "*level", "~wave", "pulse_width", "sub_osc", "pan", "glide", "drive" } },
        { "Filter",
          F::FILTER,
          { "~filter", "*cutoff", "*resonance", "env_amount", "decay", "accent", "key_track", "low_cut", "high_cut" } },
        { "Amp", F::ENVELOPE, { "amp_attack", "amp_decay", "amp_sustain", "amp_release" } },
        { "Sends and Duck", F::SPACE, { "dub_send", "room_send", "duck", "duck_release" } },
    };
    static const std::vector<GroupSpec> chord = {
        { "Stab", F::SOURCE, { "*level", "width", "detune", "octave", "bright" } },
        { "Envelope", F::ENVELOPE, { "attack", "decay", "sustain", "release", "env_amount" } },
        { "Band", F::FILTER, { "*band", "~band_mode", "band_q", "sweep", "sweep_rate", "dip" } },
        { "Colour", F::MOTION, { "crush", "crush_mix", "phaser", "phaser_rate" } },
        { "Sends", F::SPACE, { "dub_send", "plate_send" } },
    };
    static const std::vector<GroupSpec> drone = {
        { "Drone", F::SOURCE, { "*level", "octave", "detune" } },
        { "Filter", F::FILTER, { "*cutoff", "resonance", "sweep", "sweep_bars" } },
        { "Envelope", F::ENVELOPE, { "attack", "release" } },
        { "Sends", F::SPACE, { "plate_send", "room_send" } },
    };
    static const std::vector<GroupSpec> texture = {
        { "Texture", F::SOURCE, { "*level", "crackle", "hum", "hum_hz", "erosion", "width" } },
    };
    static const std::vector<GroupSpec> dub = {
        { "Tape Echo",
          F::MOTION,
          { "~echo_time", "*feedback", "tone", "low_cut", "wow", "flutter", "drive", "echo_return" } },
        { "Spring", F::SPACE, { "spring", "spring_decay" } },
        { "Plate", F::SPACE, { "plate_decay", "plate_damping", "plate_predelay", "plate_low_cut", "plate_return" } },
        { "Sends", F::SPACE, { "ping_send", "hats_send", "perc_send" } },
    };
    static const std::vector<GroupSpec> space = {
        { "Room", F::SPACE, { "*level", "size", "*decay", "damping", "predelay", "low_cut", "high_cut" } },
        { "Sends", F::SPACE, { "hats_send", "perc_send", "ping_send" } },
    };
    static const std::vector<GroupSpec> cloud = {
        { "Grains", F::MOTION, { "*level", "density", "size", "pitch", "spray" } },
        { "Sends", F::SPACE, { "ping_send", "chord_send", "plate_send" } },
    };
    static const std::vector<GroupSpec> motion = {
        { "Motion", F::MOTION, { "*amount", "hats_cut", "hats_level", "hat_decay", "perc_cut", "rumble_drive" } },
    };
    static const std::vector<GroupSpec> compose = {
        { "Track", F::SOURCE, { "~style", "~archetype", "~key", "~scale", "*bpm", "minutes", "~form", "auto" } },
        { "Style Morph", F::MOTION, { "~morph_to", "morph", "dub_share", "hypnotic_share" } },
        { "Feel", F::ENVELOPE, { "swing", "humanize", "~low_owner" } },
    };
    static const std::vector<GroupSpec> set = {
        { "Set", F::SOURCE, { "*minutes", "~dramaturgy", "~journey", "~blend", "track_minutes" } },
        { "Live", F::MOTION, { "loops", "fx_breaks", "dj_hand" } },
    };
    static const std::vector<GroupSpec> deck = {
        { "Channel", F::SPACE, { "*fader", "low", "mid", "high", "*filter", "fx_send" } },
    };
    static const std::vector<GroupSpec> djfx = {
        { "Mixer Effects", F::MOTION, { "~echo_time", "feedback", "echo_return", "hall_decay", "hall_return" } },
    };
    static const std::vector<GroupSpec> cue = {
        { "OSC Cues", F::MOTION, { "enabled", "port" } },
    };
    static const std::vector<GroupSpec> custom = {
        { "Use", F::SOURCE, { "use" } },
        { "Tempo and Length", F::SOURCE, { "bpm_low", "bpm_high", "blocks_low", "blocks_high" } },
        { "Form", F::ENVELOPE, { "arc", "peak", "endless", "max_reduction", "edge" } },
        { "Rack", F::MOTION, { "mutation", "reroll", "polymeter", "fill", "swing_low", "swing_high", "density_cap" } },
        { "Events and Corridor", F::MOTION, { "event_rate", "throw_share", "similarity", "sub_chance", "peak_lufs" } },
    };
    switch ( m )
    {
    case MT::KICK: return kick;
    case MT::RUMBLE: return rumble;
    case MT::SUB: return sub;
    case MT::PERC: return perc;
    case MT::MIX: return mix;
    case MT::MASTER: return master;
    case MT::PING: return ping;
    case MT::MOTION: return motion;
    case MT::SPACE: return space;
    case MT::BASS:
    case MT::ACID: return synth;
    case MT::CHORD: return chord;
    case MT::DRONE: return drone;
    case MT::TEXTURE: return texture;
    case MT::DUB: return dub;
    case MT::CLOUD: return cloud;
    case MT::COMPOSE: return compose;
    case MT::SET: return set;
    case MT::DECK: return deck;
    case MT::DJ_FX: return djfx;
    case MT::CUE: return cue;
    case MT::CUSTOM: return custom;
    default: return none;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

const frame::Skin &skin()
{
    static const frame::Skin s = []
    {
        frame::Skin k;
        k.m_name = "Totality";
        k.m_bg = Bg;
        k.m_panel = Panel;
        k.m_group = Group;
        k.m_raised = Raised;
        k.m_edge = Edge;
        k.m_ink = Ink;
        k.m_dim = Dim;
        k.m_faint = Faint;
        k.m_accent = Accent;
        k.m_onset = Onset;
        k.m_good = Green;
        k.m_bad = Red;
        for ( int f = 0; f < 5; ++f ) k.m_families[f] = familyColour( static_cast<Family>( f ) );
        for ( int d = 0; d < 3; ++d ) k.m_decks[d] = deckColour( d );
        k.m_radius = 2.0f;              // concrete: hard corners
        k.m_tracking = 0.32f;           // T O T A L I T Y
        k.m_typeface = "Bahnschrift";   // a condensed DIN, as on Berlin's signs
        k.m_titleBold = false;
        k.m_backdropData = TotalityData::backdrop_jpg;
        k.m_backdropSize = TotalityData::backdrop_jpgSize;
        k.m_backdropTop = 0.85f;
        k.m_backdropPage = 0.5f;
        k.m_logo = []( juce::Graphics &g, juce::Rectangle<float> r ) { drawLogo( g, r ); };
        return k;
    }();
    return s;
}

}   // namespace totui
