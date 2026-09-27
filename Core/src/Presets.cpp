/**
 * @file Presets.cpp
 * @brief The factory presets: sixteen groups per engine, sixty-four presets per group (Presets.h).
 * @note The builder is Ephemeris' (Core/src/Presets.cpp at d047d79); the groups, their ranges and their styles are Umbra's.
 */
#include "umb/Presets.h"
#include <algorithm>
#include <cmath>
#include <mutex>

namespace umb {

namespace {

/** @brief A knob's range in a group and the axis that moves it: 'A' the adjective's, 'B' the noun's, 'R' a draw. */
struct Axis {
    int k;
    float lo, hi;
    char axis;
};

/** @brief A group: its name, which of the engine's four adjective rows it uses, its nouns, its knobs, its styles. */
struct Group {
    const char* name;
    int adjectives;
    const char* nouns[8];
    std::vector<Axis> axes;
    float styles[4];      ///< Hypnotic, Ostgut, Dub, Raw
    uint32_t roles = 0;   ///< a kit lane's roles (bit PercRole), 0: any
};

/** @brief An engine: four rows of adjectives, each from dark to bright, and its sixteen groups. */
struct Engine {
    const char* adjectives[4][8];
    std::vector<Group> groups;
};

constexpr uint32_t bit(PercRole r) { return 1u << static_cast<int>(r); }

// --- The kick ----------------------------------------------------------------------------------------------------------
const Engine& kickEngine()
{
    namespace k = kick;
    static const Engine e{
        { { "Muffled", "Buried", "Dusky", "Round", "Firm", "Taut", "Crisp", "Cutting" },
          { "Velvet", "Padded", "Warm", "Solid", "Punchy", "Snappy", "Bright", "Glaring" },
          { "Sunken", "Dull", "Heavy", "Dense", "Driving", "Hard", "Sharp", "Piercing" },
          { "Distant", "Soft", "Deep", "Full", "Tight", "Clean", "Clear", "Brilliant" } },
        {
            { "Berlin 909", 0, { "Bunker", "Vault", "Cellar", "Tunnel", "Hall", "Turbine", "Boiler", "Silo" },
              { { k::Engine, 2, 2, 'R' }, { k::PitchEnd, 46, 56, 'R' }, { k::PitchStart, 220, 360, 'R' }, { k::PitchDecay, 14, 24, 'B' },
                { k::AmpDecay, 300, 450, 'B' }, { k::Drive, 0.25f, 0.5f, 'R' }, { k::Tone, 3000, 9000, 'A' }, { k::ClickLevel, 0.2f, 0.5f, 'A' },
                { k::ClickTone, 2000, 6000, 'A' }, { k::Punch, 0.35f, 0.6f, 'R' }, { k::TopLevel, -18, -10, 'R' }, { k::TopPitch, -10, -6, 'R' },
                { k::TopDecay, 60, 120, 'B' } },
              { 1.0f, 0.9f, 0.2f, 0.6f } },
            { "Hypnotic Thud", 1, { "Pulse", "Heart", "Engine Room", "Piston", "Loom", "Mill", "Pump", "Drum Hall" },
              { { k::Engine, 2, 2, 'R' }, { k::PitchEnd, 44, 50, 'R' }, { k::PitchStart, 200, 320, 'R' }, { k::PitchDecay, 16, 24, 'B' },
                { k::AmpDecay, 380, 450, 'B' }, { k::Drive, 0.25f, 0.4f, 'R' }, { k::Tone, 2500, 6000, 'A' }, { k::ClickLevel, 0.1f, 0.3f, 'A' },
                { k::Punch, 0.3f, 0.5f, 'R' }, { k::TopLevel, -20, -12, 'R' }, { k::Dip, -5, -2, 'R' } },
              { 1.0f, 0.6f, 0.3f, 0.2f } },
            { "Ostgut Punch", 2, { "Concrete", "Girder", "Rivet", "Anvil", "Forge", "Crane", "Foundry", "Steelworks" },
              { { k::Engine, 2, 2, 'R' }, { k::PitchEnd, 50, 58, 'R' }, { k::PitchStart, 260, 420, 'R' }, { k::PitchDecay, 14, 20, 'B' },
                { k::AmpDecay, 250, 380, 'B' }, { k::Drive, 0.35f, 0.55f, 'R' }, { k::Punch, 0.5f, 0.7f, 'R' }, { k::Tone, 4000, 10000, 'A' },
                { k::ClickLevel, 0.3f, 0.55f, 'A' }, { k::TopLevel, -14, -8, 'R' }, { k::TopDrive, 0.5f, 0.8f, 'R' } },
              { 0.5f, 1.0f, 0.1f, 0.5f } },
            { "Warehouse Boom", 3, { "Hangar", "Depot", "Dock", "Arch", "Viaduct", "Gasometer", "Rail Yard", "Freight" },
              { { k::Engine, 2, 2, 'R' }, { k::PitchEnd, 42, 50, 'R' }, { k::PitchDecay, 16, 24, 'R' }, { k::AmpDecay, 380, 450, 'B' },
                { k::Drive, 0.3f, 0.5f, 'R' }, { k::Dip, -6, -2, 'B' }, { k::DipFreq, 300, 800, 'R' }, { k::Tone, 2500, 7000, 'A' },
                { k::ClickLevel, 0.15f, 0.4f, 'A' } },
              { 0.6f, 0.6f, 0.1f, 0.4f } },
            { "Dub Sine", 0, { "Lagoon", "Estuary", "Harbour", "Reef", "Mooring", "Buoy", "Jetty", "Lighthouse" },
              { { k::Engine, 0, 0, 'R' }, { k::PitchEnd, 44, 52, 'R' }, { k::PitchStart, 150, 260, 'R' }, { k::PitchDecay, 18, 26, 'B' },
                { k::AmpDecay, 380, 550, 'B' }, { k::Drive, 0.15f, 0.3f, 'R' }, { k::ClickLevel, 0.05f, 0.25f, 'A' }, { k::Tone, 1500, 4000, 'A' },
                { k::Punch, 0.2f, 0.4f, 'R' }, { k::TopLevel, -30, -18, 'R' } },
              { 0.2f, 0.1f, 1.0f, 0.0f } },
            { "Soft Chamber", 1, { "Alcove", "Parlour", "Chapel", "Crypt", "Cloister", "Rotunda", "Nave", "Apse" },
              { { k::Engine, 0, 0, 'R' }, { k::PitchEnd, 44, 50, 'R' }, { k::PitchDecay, 20, 26, 'B' }, { k::AmpDecay, 400, 550, 'B' },
                { k::Drive, 0.15f, 0.25f, 'R' }, { k::ClickLevel, 0.0f, 0.15f, 'A' }, { k::Tone, 1200, 3000, 'A' }, { k::TopLevel, -36, -20, 'R' } },
              { 0.3f, 0.1f, 0.9f, 0.0f } },
            { "Round Sub Kick", 2, { "Boulder", "Monolith", "Menhir", "Dolmen", "Cairn", "Tor", "Barrow", "Henge" },
              { { k::Engine, 0, 0, 'R' }, { k::PitchEnd, 40, 48, 'R' }, { k::PitchStart, 120, 220, 'R' }, { k::PitchDecay, 18, 26, 'R' },
                { k::AmpDecay, 400, 550, 'B' }, { k::Drive, 0.15f, 0.3f, 'R' }, { k::ClickLevel, 0.05f, 0.2f, 'A' }, { k::Tone, 1500, 3500, 'A' } },
              { 0.4f, 0.2f, 0.6f, 0.1f } },
            { "Resonant Body", 3, { "Bell Jar", "Kettle", "Cauldron", "Cistern", "Tank", "Barrel", "Drum Shell", "Casket" },
              { { k::Engine, 1, 1, 'R' }, { k::PitchEnd, 46, 56, 'R' }, { k::PitchDecay, 14, 20, 'R' }, { k::AmpDecay, 300, 450, 'B' },
                { k::Drive, 0.25f, 0.45f, 'R' }, { k::Tone, 3000, 7000, 'A' }, { k::ClickLevel, 0.15f, 0.4f, 'A' }, { k::Punch, 0.35f, 0.55f, 'R' } },
              { 0.4f, 0.3f, 0.3f, 0.3f } },
            { "Wooden Knock", 0, { "Plank", "Crate", "Pallet", "Beam", "Rafter", "Joist", "Timber", "Lintel" },
              { { k::Engine, 1, 1, 'R' }, { k::PitchEnd, 50, 58, 'R' }, { k::AmpDecay, 250, 320, 'B' }, { k::PitchDecay, 12, 16, 'R' },
                { k::ClickLevel, 0.3f, 0.55f, 'A' }, { k::Tone, 5000, 12000, 'A' }, { k::Punch, 0.5f, 0.75f, 'B' }, { k::Drive, 0.3f, 0.5f, 'R' } },
              { 0.3f, 0.5f, 0.1f, 0.3f } },
            { "Raw Distorted", 1, { "Grinder", "Saw Mill", "Crusher", "Shredder", "Chainsaw", "Jackhammer", "Drill", "Rivet Gun" },
              { { k::Engine, 2, 2, 'R' }, { k::Clip, 1, 1, 'R' }, { k::Drive, 0.55f, 0.85f, 'B' }, { k::PitchDecay, 12, 20, 'R' },
                { k::AmpDecay, 250, 380, 'R' }, { k::Tone, 5000, 12000, 'A' }, { k::ClickLevel, 0.35f, 0.6f, 'A' }, { k::TopDrive, 0.6f, 0.9f, 'R' },
                { k::TopLevel, -14, -8, 'R' } },
              { 0.0f, 0.3f, 0.0f, 1.0f } },
            { "Industrial Hammer", 2, { "Stamp", "Press", "Pile Driver", "Ram", "Mallet", "Sledge", "Maul", "Trip Hammer" },
              { { k::Engine, 0, 0, 'R' }, { k::Clip, 1, 1, 'R' }, { k::Drive, 0.6f, 0.85f, 'R' }, { k::PitchStart, 400, 900, 'B' },
                { k::PitchDecay, 12, 18, 'R' }, { k::AmpDecay, 250, 340, 'R' }, { k::ClickLevel, 0.4f, 0.65f, 'A' }, { k::Tone, 6000, 14000, 'A' } },
              { 0.0f, 0.2f, 0.0f, 0.9f } },
            { "Peak Stomp", 3, { "Stomp", "Stampede", "March", "Parade", "Charge", "Surge", "Onslaught", "Rally" },
              { { k::Engine, 2, 2, 'R' }, { k::Punch, 0.6f, 0.85f, 'B' }, { k::Drive, 0.5f, 0.75f, 'R' }, { k::AmpDecay, 280, 380, 'R' },
                { k::PitchDecay, 12, 18, 'R' }, { k::TopLevel, -12, -6, 'R' }, { k::TopDrive, 0.7f, 0.9f, 'R' }, { k::Tone, 4000, 11000, 'A' } },
              { 0.1f, 0.5f, 0.0f, 0.8f } },
            { "Tight Tool", 0, { "Tool", "Lever", "Wrench", "Clamp", "Chisel", "Gauge", "Caliper", "Spanner" },
              { { k::Engine, 2, 2, 'R' }, { k::AmpDecay, 250, 320, 'B' }, { k::PitchDecay, 14, 18, 'R' }, { k::ClickLevel, 0.3f, 0.5f, 'A' },
                { k::Tone, 5000, 10000, 'A' }, { k::Drive, 0.3f, 0.5f, 'R' }, { k::Punch, 0.4f, 0.6f, 'R' } },
              { 0.5f, 0.7f, 0.1f, 0.4f } },
            { "Long Tail", 1, { "Comet Tail", "Wake", "Trail", "Echo Line", "Afterglow", "Drift", "Contrail", "Slipstream" },
              { { k::Engine, 1, 1, 'R' }, { k::AmpDecay, 400, 450, 'B' }, { k::PitchEnd, 40, 48, 'R' }, { k::PitchDecay, 18, 24, 'R' },
                { k::Drive, 0.25f, 0.35f, 'R' }, { k::Tone, 2000, 5000, 'A' }, { k::ClickLevel, 0.1f, 0.3f, 'A' } },
              { 0.6f, 0.2f, 0.4f, 0.1f } },
            { "Click Top", 2, { "Needle", "Pin", "Stylus", "Tack", "Nail", "Awl", "Spike", "Barb" },
              { { k::Engine, 2, 2, 'R' }, { k::ClickLevel, 0.45f, 0.7f, 'A' }, { k::ClickTone, 3000, 9000, 'A' }, { k::ClickDecay, 3, 10, 'B' },
                { k::TopLevel, -14, -8, 'R' }, { k::TopCut, 300, 700, 'B' }, { k::PitchDecay, 14, 20, 'R' }, { k::AmpDecay, 280, 400, 'R' } },
              { 0.4f, 0.5f, 0.1f, 0.5f } },
            { "Dark Rumble Feeder", 3, { "Undertow", "Riptide", "Current", "Eddy", "Whirlpool", "Maelstrom", "Swell", "Groundswell" },
              { { k::Engine, 0, 0, 'R' }, { k::PitchEnd, 42, 48, 'R' }, { k::PitchDecay, 20, 26, 'B' }, { k::AmpDecay, 350, 450, 'R' },
                { k::Tone, 1500, 3500, 'A' }, { k::Dip, -8, -3, 'B' }, { k::ClickLevel, 0.05f, 0.2f, 'A' }, { k::Drive, 0.2f, 0.35f, 'R' } },
              { 0.8f, 0.3f, 0.5f, 0.3f } },
        } };
    return e;
}

// --- The rumble ----------------------------------------------------------------------------------------------------------
const Engine& rumbleEngine()
{
    namespace r = rumble;
    static const Engine e{
        { { "Abyssal", "Murky", "Sombre", "Smoky", "Grey", "Pale", "Misty", "Airy" },
          { "Tarry", "Sooty", "Earthen", "Ashen", "Dusty", "Hazy", "Silty", "Chalky" },
          { "Black", "Charcoal", "Graphite", "Iron", "Lead", "Pewter", "Zinc", "Silver" },
          { "Midnight", "Nocturnal", "Twilit", "Dusk", "Dawn", "Morning", "Noon", "Glaring" } },
        {
            { "Berghain Roll", 0, { "Roll", "Tumble", "Churn", "Grind", "Rotation", "Revolution", "Wheel", "Drum" },
              { { r::Decay, 1.5f, 2.8f, 'B' }, { r::Size, 0.8f, 1.4f, 'R' }, { r::Damping, 0.7f, 0.35f, 'A' }, { r::Split, 70, 100, 'A' },
                { r::Drive, 4, 8, 'R' }, { r::Ratio, 2.5f, 3.5f, 'R' }, { r::Resonance, 0.1f, 0.25f, 'R' } },
              { 1.0f, 0.8f, 0.3f, 0.6f } },
            { "Dark Hall", 1, { "Hall", "Gallery", "Atrium", "Foyer", "Lobby", "Corridor", "Aisle", "Passage" },
              { { r::Decay, 1.8f, 3.0f, 'B' }, { r::Size, 1.0f, 2.0f, 'B' }, { r::Damping, 0.8f, 0.5f, 'A' }, { r::Split, 60, 90, 'A' },
                { r::Drive, 3, 6, 'R' }, { r::PreDelay, 0, 20, 'R' } },
              { 0.9f, 0.5f, 0.4f, 0.3f } },
            { "Tight Rumble", 2, { "Knot", "Coil", "Spring", "Clench", "Grip", "Vice", "Brace", "Tension" },
              { { r::Decay, 0.8f, 1.5f, 'B' }, { r::Size, 0.4f, 0.8f, 'R' }, { r::Damping, 0.6f, 0.3f, 'A' }, { r::Split, 80, 120, 'A' },
                { r::Drive, 5, 9, 'R' }, { r::Ratio, 3.0f, 4.5f, 'R' } },
              { 0.5f, 0.9f, 0.1f, 0.7f } },
            { "Sub Swell", 3, { "Tide", "Flood", "Surge", "Wave", "Breaker", "Roller", "Crest", "Billow" },
              { { r::Decay, 1.6f, 2.8f, 'R' }, { r::SubAttack, 40, 150, 'B' }, { r::SubRelease, 400, 1200, 'B' }, { r::Damping, 0.7f, 0.4f, 'A' },
                { r::Split, 60, 90, 'A' }, { r::Drive, 3, 6, 'R' } },
              { 0.8f, 0.4f, 0.6f, 0.2f } },
            { "Distorted Roll", 0, { "Static", "Crackle", "Sizzle", "Fizz", "Buzz", "Rasp", "Snarl", "Growl" },
              { { r::Drive, 8, 12, 'B' }, { r::Decay, 1.4f, 2.4f, 'R' }, { r::Damping, 0.6f, 0.3f, 'A' }, { r::Split, 80, 120, 'A' },
                { r::Resonance, 0.15f, 0.35f, 'R' }, { r::Ratio, 3.0f, 4.5f, 'R' } },
              { 0.2f, 0.4f, 0.0f, 1.0f } },
            { "Long Cave", 1, { "Cave", "Grotto", "Cavern", "Hollow", "Pit", "Shaft", "Mine", "Quarry" },
              { { r::Decay, 2.2f, 3.0f, 'B' }, { r::Size, 1.4f, 2.4f, 'R' }, { r::Damping, 0.85f, 0.55f, 'A' }, { r::Split, 55, 80, 'A' },
                { r::Drive, 3, 6, 'R' }, { r::PreDelay, 5, 30, 'R' } },
              { 0.8f, 0.2f, 0.6f, 0.1f } },
            { "Short Plate", 2, { "Plate", "Sheet", "Panel", "Tile", "Slab", "Board", "Leaf", "Foil" },
              { { r::Decay, 0.8f, 1.4f, 'B' }, { r::Size, 0.3f, 0.6f, 'R' }, { r::Damping, 0.5f, 0.2f, 'A' }, { r::Split, 90, 130, 'A' },
                { r::Drive, 4, 8, 'R' } },
              { 0.3f, 0.8f, 0.1f, 0.6f } },
            { "Wide Chamber", 3, { "Chamber", "Vault", "Dome", "Cupola", "Arcade", "Basilica", "Hangar", "Stadium" },
              { { r::Decay, 1.6f, 2.6f, 'R' }, { r::Size, 1.2f, 2.2f, 'B' }, { r::Damping, 0.7f, 0.4f, 'A' }, { r::Split, 65, 95, 'A' },
                { r::Drive, 4, 7, 'R' }, { r::Ratio, 2.0f, 3.0f, 'R' } },
              { 0.7f, 0.6f, 0.4f, 0.3f } },
            { "Soft Undertow", 0, { "Murmur", "Hum", "Drone", "Purr", "Whir", "Thrum", "Breath", "Sigh" },
              { { r::Drive, 2, 4, 'R' }, { r::Decay, 1.4f, 2.2f, 'B' }, { r::Damping, 0.85f, 0.6f, 'A' }, { r::Split, 60, 85, 'A' },
                { r::Resonance, 0.05f, 0.15f, 'R' } },
              { 0.6f, 0.3f, 0.9f, 0.1f } },
            { "Resonant Boom", 1, { "Gong", "Timpani", "Kettle Drum", "Tabla", "Taiko", "Bodhran", "Dhol", "Surdo" },
              { { r::Resonance, 0.3f, 0.55f, 'B' }, { r::Ratio, 2.0f, 4.0f, 'R' }, { r::Decay, 1.4f, 2.4f, 'R' }, { r::Damping, 0.7f, 0.4f, 'A' },
                { r::Split, 70, 100, 'A' }, { r::Drive, 4, 8, 'R' } },
              { 0.6f, 0.5f, 0.2f, 0.5f } },
            { "Gritty Floor", 2, { "Gravel", "Grit", "Sand", "Shale", "Scree", "Rubble", "Cinder", "Clinker" },
              { { r::Drive, 6, 10, 'B' }, { r::Decay, 1.2f, 2.0f, 'R' }, { r::Damping, 0.6f, 0.3f, 'A' }, { r::Split, 85, 125, 'A' },
                { r::Ratio, 3.5f, 5.0f, 'R' } },
              { 0.3f, 0.6f, 0.0f, 0.9f } },
            { "Cathedral Low", 3, { "Cathedral", "Minster", "Abbey", "Priory", "Sanctum", "Chancel", "Transept", "Belfry" },
              { { r::Decay, 2.4f, 3.0f, 'B' }, { r::Size, 1.8f, 3.0f, 'R' }, { r::Damping, 0.8f, 0.5f, 'A' }, { r::Split, 55, 75, 'A' },
                { r::PreDelay, 10, 40, 'R' }, { r::Drive, 3, 5, 'R' } },
              { 0.7f, 0.2f, 0.5f, 0.1f } },
            { "Pulse Rumble", 0, { "Pulse", "Throb", "Beat", "Thump", "Pound", "Knock", "Tap", "Tick" },
              { { r::Decay, 1.0f, 1.8f, 'B' }, { r::Size, 0.6f, 1.0f, 'R' }, { r::Damping, 0.6f, 0.35f, 'A' }, { r::Split, 75, 110, 'A' },
                { r::Drive, 5, 8, 'R' }, { r::Ratio, 2.8f, 3.8f, 'R' } },
              { 0.9f, 0.7f, 0.2f, 0.5f } },
            { "Deep Tunnel", 1, { "Tunnel", "Tube", "Sewer", "Culvert", "Conduit", "Pipeline", "Aqueduct", "Catacomb" },
              { { r::Decay, 1.8f, 2.8f, 'B' }, { r::Size, 0.9f, 1.6f, 'R' }, { r::Damping, 0.9f, 0.6f, 'A' }, { r::Split, 50, 75, 'A' },
                { r::Drive, 4, 7, 'R' } },
              { 0.9f, 0.4f, 0.5f, 0.3f } },
            { "Dub Floor", 2, { "Floorboard", "Parquet", "Terrace", "Veranda", "Patio", "Deck", "Porch", "Balcony" },
              { { r::Decay, 1.2f, 2.2f, 'B' }, { r::Drive, 3, 6, 'R' }, { r::Damping, 0.8f, 0.55f, 'A' }, { r::Split, 60, 85, 'A' },
                { r::SubRelease, 300, 800, 'R' } },
              { 0.3f, 0.2f, 1.0f, 0.0f } },
            { "Raw Grind", 3, { "Millstone", "Quern", "Crank", "Gearbox", "Sprocket", "Cog", "Ratchet", "Flywheel" },
              { { r::Drive, 8, 12, 'B' }, { r::Decay, 1.4f, 2.4f, 'R' }, { r::Resonance, 0.2f, 0.4f, 'R' }, { r::Damping, 0.55f, 0.3f, 'A' },
                { r::Split, 90, 130, 'A' } },
              { 0.1f, 0.3f, 0.0f, 1.0f } },
        } };
    return e;
}

// --- The sub bass ----------------------------------------------------------------------------------------------------------
const Engine& subEngine()
{
    namespace s = sub;
    static const Engine e{
        { { "Pure", "Still", "Calm", "Smooth", "Round", "Warm", "Glowing", "Radiant" },
          { "Deep", "Low", "Heavy", "Solid", "Firm", "Rich", "Full", "Bold" },
          { "Dark", "Shaded", "Dim", "Muted", "Plain", "Open", "Lit", "Bright" },
          { "Quiet", "Gentle", "Mellow", "Easy", "Steady", "Lively", "Keen", "Fierce" } },
        {
            { "Pure Sine", 0, { "Tone", "Hum", "Note", "Pitch", "Unison", "Root", "Fundamental", "Octave" },
              { { s::LowPass, 80, 160, 'A' }, { s::Drive, 0.0f, 0.1f, 'R' }, { s::Attack, 2, 8, 'R' }, { s::Decay, 200, 600, 'B' },
                { s::Sustain, 0.6f, 0.9f, 'R' }, { s::Release, 60, 200, 'B' } },
              { 0.7f, 0.5f, 1.0f, 0.3f } },
            { "Warm Sine", 1, { "Hearth", "Ember", "Glow", "Coal", "Kiln", "Stove", "Oven", "Furnace" },
              { { s::LowPass, 90, 180, 'A' }, { s::Drive, 0.15f, 0.35f, 'B' }, { s::Attack, 2, 6, 'R' }, { s::Decay, 250, 700, 'R' },
                { s::Sustain, 0.6f, 0.85f, 'R' }, { s::Release, 80, 200, 'R' } },
              { 0.7f, 0.6f, 0.9f, 0.4f } },
            { "Pluck Sub", 2, { "Pluck", "Pick", "Twang", "Snap", "Flick", "Pop", "Tap", "Nudge" },
              { { s::Decay, 80, 250, 'B' }, { s::Sustain, 0.1f, 0.4f, 'R' }, { s::Release, 40, 120, 'R' }, { s::LowPass, 100, 220, 'A' },
                { s::Attack, 1, 4, 'R' }, { s::Drive, 0.05f, 0.25f, 'R' } },
              { 0.6f, 0.7f, 0.4f, 0.5f } },
            { "Held Sub", 3, { "Anchor", "Keel", "Ballast", "Plinth", "Pedestal", "Foundation", "Bedrock", "Cornerstone" },
              { { s::Sustain, 0.8f, 1.0f, 'R' }, { s::Decay, 400, 1200, 'B' }, { s::Release, 100, 300, 'B' }, { s::LowPass, 80, 150, 'A' },
                { s::Attack, 3, 10, 'R' } },
              { 0.5f, 0.3f, 1.0f, 0.2f } },
            { "Driven Sub", 0, { "Motor", "Dynamo", "Generator", "Turbine", "Rotor", "Engine", "Compressor", "Reactor" },
              { { s::Drive, 0.35f, 0.7f, 'B' }, { s::LowPass, 100, 220, 'A' }, { s::Decay, 200, 600, 'R' }, { s::Sustain, 0.5f, 0.8f, 'R' },
                { s::Release, 60, 160, 'R' } },
              { 0.3f, 0.5f, 0.2f, 1.0f } },
            { "Round Sub", 1, { "Sphere", "Orb", "Globe", "Pearl", "Marble", "Bubble", "Dome", "Ball" },
              { { s::LowPass, 70, 130, 'A' }, { s::Drive, 0.05f, 0.15f, 'R' }, { s::Decay, 300, 800, 'B' }, { s::Sustain, 0.6f, 0.9f, 'R' },
                { s::Release, 80, 250, 'R' }, { s::Attack, 3, 12, 'R' } },
              { 0.6f, 0.4f, 0.9f, 0.2f } },
            { "Short Sub", 2, { "Blip", "Dot", "Dash", "Spark", "Flash", "Beat", "Stab", "Jab" },
              { { s::Decay, 60, 180, 'B' }, { s::Sustain, 0.0f, 0.3f, 'R' }, { s::Release, 30, 90, 'R' }, { s::LowPass, 110, 240, 'A' },
                { s::Attack, 1, 3, 'R' } },
              { 0.5f, 0.8f, 0.2f, 0.6f } },
            { "Dub Sub", 3, { "Bassline", "Riddim", "Dubplate", "Sound System", "Stack", "Scoop", "Horn", "Bin" },
              { { s::LowPass, 80, 140, 'A' }, { s::Drive, 0.05f, 0.2f, 'R' }, { s::Decay, 400, 1000, 'B' }, { s::Sustain, 0.7f, 0.95f, 'R' },
                { s::Release, 120, 350, 'B' }, { s::Attack, 4, 15, 'R' } },
              { 0.3f, 0.2f, 1.0f, 0.1f } },
            { "Soft Swell", 0, { "Swell", "Rise", "Bloom", "Fade", "Wash", "Surge", "Wax", "Welling" },
              { { s::Attack, 15, 50, 'B' }, { s::Decay, 400, 1200, 'R' }, { s::Sustain, 0.7f, 0.95f, 'R' }, { s::Release, 150, 400, 'R' },
                { s::LowPass, 80, 150, 'A' } },
              { 0.6f, 0.2f, 0.8f, 0.1f } },
            { "Tight Sub", 1, { "Latch", "Lock", "Bolt", "Hinge", "Clasp", "Buckle", "Catch", "Pin" },
              { { s::Attack, 1, 3, 'R' }, { s::Decay, 120, 300, 'B' }, { s::Sustain, 0.4f, 0.7f, 'R' }, { s::Release, 40, 100, 'R' },
                { s::LowPass, 100, 200, 'A' }, { s::Drive, 0.05f, 0.2f, 'R' } },
              { 0.8f, 0.8f, 0.4f, 0.5f } },
            { "Dark Sub", 2, { "Shadow", "Umbra", "Eclipse", "Nightfall", "Gloom", "Murk", "Void", "Abyss" },
              { { s::LowPass, 60, 100, 'A' }, { s::Drive, 0.0f, 0.1f, 'R' }, { s::Decay, 300, 900, 'B' }, { s::Sustain, 0.6f, 0.9f, 'R' },
                { s::Release, 100, 300, 'R' } },
              { 0.9f, 0.4f, 0.7f, 0.2f } },
            { "Grit Sub", 3, { "Grain", "Sandpaper", "Emery", "Rasp", "File", "Burr", "Whetstone", "Grindstone" },
              { { s::Drive, 0.25f, 0.5f, 'B' }, { s::LowPass, 120, 260, 'A' }, { s::Decay, 150, 500, 'R' }, { s::Sustain, 0.4f, 0.8f, 'R' },
                { s::Release, 50, 150, 'R' } },
              { 0.3f, 0.6f, 0.1f, 0.9f } },
            { "Hypnotic Sub", 0, { "Trance", "Spell", "Charm", "Mantra", "Chant", "Loop", "Cycle", "Orbit" },
              { { s::LowPass, 90, 160, 'A' }, { s::Decay, 250, 700, 'B' }, { s::Sustain, 0.6f, 0.85f, 'R' }, { s::Release, 70, 180, 'R' },
                { s::Drive, 0.05f, 0.2f, 'R' }, { s::Attack, 2, 6, 'R' } },
              { 1.0f, 0.5f, 0.5f, 0.3f } },
            { "Ostgut Sub", 1, { "Pylon", "Mast", "Tower", "Chimney", "Stack", "Spire", "Column", "Pillar" },
              { { s::LowPass, 100, 200, 'A' }, { s::Decay, 150, 450, 'B' }, { s::Sustain, 0.4f, 0.75f, 'R' }, { s::Release, 50, 130, 'R' },
                { s::Drive, 0.1f, 0.3f, 'R' } },
              { 0.5f, 1.0f, 0.2f, 0.5f } },
            { "Long Release Sub", 2, { "Echo", "Linger", "Remnant", "Residue", "Afterimage", "Ripple", "Wake", "Trace" },
              { { s::Release, 250, 700, 'B' }, { s::Decay, 400, 1200, 'R' }, { s::Sustain, 0.6f, 0.9f, 'R' }, { s::LowPass, 80, 150, 'A' } },
              { 0.5f, 0.2f, 0.9f, 0.1f } },
            { "Raw Sub", 3, { "Iron Bar", "Rebar", "Girder", "Rail", "Pipe", "Cable", "Chain", "Hook" },
              { { s::Drive, 0.4f, 0.8f, 'B' }, { s::LowPass, 140, 300, 'A' }, { s::Decay, 120, 400, 'R' }, { s::Sustain, 0.3f, 0.7f, 'R' },
                { s::Release, 40, 120, 'R' } },
              { 0.1f, 0.4f, 0.0f, 1.0f } },
        } };
    return e;
}

// --- A lane of the kit ----------------------------------------------------------------------------------------------------------
const Engine& percEngine()
{
    namespace p = perc;
    using R = PercRole;
    static const Engine e{
        { { "Dull", "Soft", "Muted", "Dry", "Crisp", "Bright", "Sizzling", "Glittering" },
          { "Wooden", "Leathery", "Papery", "Brittle", "Glassy", "Steely", "Tinny", "Icy" },
          { "Dark", "Shadowy", "Dusty", "Grainy", "Clean", "Shiny", "Sparkling", "Blinding" },
          { "Distant", "Hushed", "Close", "Tight", "Snappy", "Sharp", "Piercing", "Searing" } },
        {
            { "909 Closed Hat", 0, { "Tick", "Chip", "Sliver", "Shard", "Splinter", "Flake", "Fleck", "Speck" },
              { { p::Engine, 0, 0, 'R' }, { p::NoiseType, 1, 1, 'R' }, { p::Noise, 1, 1, 'R' }, { p::NoiseDecay, 35, 90, 'B' },
                { p::Decay, 35, 90, 'B' }, { p::Filter, 2, 2, 'R' }, { p::Cutoff, 5000, 9000, 'A' }, { p::LowCut, 2500, 5000, 'A' },
                { p::Resonance, 0.05f, 0.3f, 'R' }, { p::MetalScale, 0.85f, 1.2f, 'R' }, { p::Drive, 0.0f, 0.3f, 'R' } },
              { 1.0f, 1.0f, 0.6f, 1.0f }, bit(R::ClosedHat) | bit(R::RollingHat) },
            { "White Tick Hat", 1, { "Grain", "Seed", "Crumb", "Mote", "Dust", "Pollen", "Spore", "Ash" },
              { { p::Engine, 0, 0, 'R' }, { p::NoiseType, 0, 0, 'R' }, { p::Noise, 1, 1, 'R' }, { p::NoiseDecay, 20, 60, 'B' },
                { p::Decay, 20, 60, 'B' }, { p::Filter, 1, 2, 'R' }, { p::Cutoff, 7000, 12000, 'A' }, { p::LowCut, 3000, 6000, 'A' },
                { p::Resonance, 0.1f, 0.35f, 'R' } },
              { 0.8f, 0.7f, 0.8f, 0.6f }, bit(R::ClosedHat) | bit(R::RollingHat) },
            { "Metal Hat", 2, { "Rivet", "Washer", "Nut", "Screw", "Staple", "Tack", "Clip", "Link" },
              { { p::Engine, 1, 1, 'R' }, { p::MetalScale, 0.8f, 1.6f, 'A' }, { p::Noise, 0.2f, 0.6f, 'R' }, { p::NoiseDecay, 30, 120, 'B' },
                { p::Decay, 30, 120, 'B' }, { p::Filter, 2, 2, 'R' }, { p::Cutoff, 6000, 10000, 'A' }, { p::LowCut, 3000, 5000, 'R' } },
              { 0.7f, 0.8f, 0.3f, 0.9f }, bit(R::ClosedHat) | bit(R::RollingHat) | bit(R::OpenHat) },
            { "Rolling Shaker Hat", 3, { "Rattle", "Tremolo", "Flutter", "Shiver", "Quiver", "Jitter", "Twitch", "Tremor" },
              { { p::Engine, 0, 0, 'R' }, { p::NoiseType, 0, 1, 'R' }, { p::Noise, 1, 1, 'R' }, { p::NoiseDecay, 25, 60, 'B' },
                { p::Decay, 25, 60, 'B' }, { p::Filter, 1, 1, 'R' }, { p::Cutoff, 6000, 11000, 'A' }, { p::LowCut, 3000, 6000, 'R' },
                { p::Bursts, 1, 2, 'R' }, { p::BurstSpacing, 6, 14, 'R' } },
              { 1.0f, 0.7f, 0.5f, 0.6f }, bit(R::RollingHat) | bit(R::Shaker) },
            { "Open Hat Wash", 0, { "Wash", "Spray", "Mist", "Spume", "Foam", "Surf", "Froth", "Drizzle" },
              { { p::Engine, 0, 0, 'R' }, { p::NoiseType, 1, 1, 'R' }, { p::Noise, 1, 1, 'R' }, { p::NoiseDecay, 200, 500, 'B' },
                { p::Decay, 200, 500, 'B' }, { p::Filter, 2, 2, 'R' }, { p::Cutoff, 5500, 8500, 'A' }, { p::LowCut, 2500, 4000, 'A' },
                { p::MetalScale, 0.85f, 1.1f, 'R' } },
              { 1.0f, 1.0f, 0.7f, 0.8f }, bit(R::OpenHat) },
            { "Metal Open Hat", 1, { "Cymbal", "Gong", "Chime", "Plate", "Disc", "Shield", "Lid", "Pan" },
              { { p::Engine, 1, 1, 'R' }, { p::MetalScale, 0.9f, 1.4f, 'A' }, { p::Noise, 0.3f, 0.7f, 'R' }, { p::NoiseDecay, 250, 600, 'B' },
                { p::Decay, 250, 600, 'B' }, { p::Filter, 2, 2, 'R' }, { p::Cutoff, 5000, 8000, 'A' } },
              { 0.8f, 0.9f, 0.5f, 0.8f }, bit(R::OpenHat) },
            { "Ride Bell", 2, { "Bell", "Carillon", "Campana", "Tocsin", "Knell", "Peal", "Toll", "Clang" },
              { { p::Engine, 1, 1, 'R' }, { p::MetalScale, 1.8f, 2.8f, 'A' }, { p::Noise, 0.3f, 0.6f, 'R' }, { p::NoiseDecay, 500, 1200, 'B' },
                { p::Decay, 500, 1200, 'B' }, { p::Filter, 2, 2, 'R' }, { p::Cutoff, 3000, 6000, 'A' }, { p::LowCut, 800, 1500, 'R' } },
              { 0.6f, 1.0f, 0.4f, 0.6f }, bit(R::Ride) },
            { "Dry Ride", 3, { "Ride", "Canter", "Trot", "Gallop", "Stride", "Pace", "Glide", "Cruise" },
              { { p::Engine, 1, 1, 'R' }, { p::MetalScale, 1.6f, 2.4f, 'R' }, { p::Noise, 0.5f, 0.8f, 'R' }, { p::NoiseDecay, 300, 700, 'B' },
                { p::Decay, 300, 700, 'B' }, { p::Filter, 2, 2, 'R' }, { p::Cutoff, 3500, 7000, 'A' } },
              { 0.6f, 1.0f, 0.5f, 0.5f }, bit(R::Ride) },
            { "909 Clap", 0, { "Clap", "Slap", "Smack", "Whack", "Crack", "Snap", "Spank", "Swat" },
              { { p::Engine, 0, 0, 'R' }, { p::NoiseType, 0, 0, 'R' }, { p::Noise, 1, 1, 'R' }, { p::Bursts, 3, 5, 'R' }, { p::BurstSpacing, 8, 14, 'R' },
                { p::NoiseDecay, 120, 260, 'B' }, { p::Decay, 120, 260, 'B' }, { p::Filter, 1, 1, 'R' }, { p::Cutoff, 900, 2000, 'A' },
                { p::Resonance, 0.2f, 0.45f, 'R' } },
              { 0.8f, 1.0f, 0.6f, 0.9f }, bit(R::Clap) | bit(R::ClapGhost) },
            { "Room Clap", 1, { "Applause", "Ovation", "Crowd", "Audience", "Throng", "Gathering", "Assembly", "Congregation" },
              { { p::Engine, 0, 0, 'R' }, { p::NoiseType, 0, 0, 'R' }, { p::Noise, 1, 1, 'R' }, { p::Bursts, 4, 6, 'R' }, { p::BurstSpacing, 10, 18, 'R' },
                { p::NoiseDecay, 200, 400, 'B' }, { p::Decay, 200, 400, 'B' }, { p::Filter, 1, 1, 'R' }, { p::Cutoff, 1000, 2500, 'A' } },
              { 0.6f, 0.8f, 0.8f, 0.6f }, bit(R::Clap) | bit(R::ClapGhost) },
            { "Rimshot", 2, { "Rim", "Edge", "Brim", "Lip", "Hoop", "Border", "Margin", "Verge" },
              { { p::Engine, 2, 2, 'R' }, { p::ModeSet, 1, 1, 'R' }, { p::Pitch, 800, 1800, 'A' }, { p::Decay, 20, 60, 'B' }, { p::NoiseDecay, 10, 30, 'R' },
                { p::Noise, 0.1f, 0.3f, 'R' }, { p::ModeDamp, 0.3f, 0.7f, 'R' }, { p::Filter, 2, 2, 'R' }, { p::Cutoff, 400, 1200, 'R' } },
              { 0.9f, 0.8f, 0.6f, 0.6f }, bit(R::Rim) },
            { "Wood Block", 3, { "Block", "Clave", "Log", "Stick", "Peg", "Dowel", "Spindle", "Baton" },
              { { p::Engine, 2, 2, 'R' }, { p::ModeSet, 1, 1, 'R' }, { p::Pitch, 500, 1500, 'A' }, { p::Decay, 30, 90, 'B' }, { p::Noise, 0.0f, 0.15f, 'R' },
                { p::ModeDamp, 0.4f, 0.8f, 'R' }, { p::Filter, 2, 2, 'R' }, { p::Cutoff, 300, 900, 'R' } },
              { 0.8f, 0.5f, 0.7f, 0.4f }, bit(R::Rim) | bit(R::Conga) },
            { "Shaker", 0, { "Maraca", "Cabasa", "Rainstick", "Egg", "Tambourine", "Ganza", "Caxixi", "Shekere" },
              { { p::Engine, 0, 0, 'R' }, { p::NoiseType, 0, 0, 'R' }, { p::Noise, 1, 1, 'R' }, { p::NoiseDecay, 30, 80, 'B' }, { p::Decay, 30, 80, 'B' },
                { p::Filter, 1, 1, 'R' }, { p::Cutoff, 5000, 10000, 'A' }, { p::Bursts, 1, 2, 'R' }, { p::BurstSpacing, 15, 30, 'R' } },
              { 1.0f, 0.6f, 0.6f, 0.5f }, bit(R::Shaker) },
            { "Tom Membrane", 1, { "Tom", "Floor Tom", "Rack Tom", "Kettle", "Hand Drum", "Frame Drum", "Tabor", "Snare Drum" },
              { { p::Engine, 2, 2, 'R' }, { p::ModeSet, 0, 0, 'R' }, { p::Pitch, 80, 220, 'A' }, { p::PitchAmount, 1.5f, 3.0f, 'R' },
                { p::PitchDecay, 20, 80, 'R' }, { p::Decay, 150, 400, 'B' }, { p::Noise, 0.0f, 0.2f, 'R' }, { p::Filter, 0, 0, 'R' },
                { p::Cutoff, 1500, 5000, 'R' }, { p::LowCut, 150, 300, 'R' } },
              { 0.8f, 0.6f, 0.4f, 0.9f }, bit(R::Tom) | bit(R::Conga) },
            { "Conga Skin", 2, { "Conga", "Bongo", "Djembe", "Cajon", "Tumba", "Quinto", "Darbuka", "Udu" },
              { { p::Engine, 2, 2, 'R' }, { p::ModeSet, 0, 0, 'R' }, { p::Pitch, 180, 400, 'A' }, { p::PitchAmount, 1.2f, 2.0f, 'R' },
                { p::PitchDecay, 10, 40, 'R' }, { p::Decay, 80, 220, 'B' }, { p::Noise, 0.0f, 0.15f, 'R' }, { p::Filter, 0, 0, 'R' },
                { p::Cutoff, 2000, 6000, 'R' }, { p::LowCut, 150, 300, 'R' } },
              { 0.9f, 0.5f, 0.6f, 0.5f }, bit(R::Conga) | bit(R::Tom) },
            { "Snare Crack", 3, { "Crack", "Rimfire", "Report", "Shot", "Burst", "Blast", "Bang", "Salvo" },
              { { p::Engine, 3, 3, 'R' }, { p::Pitch, 180, 260, 'R' }, { p::Noise, 0.5f, 0.85f, 'R' }, { p::NoiseType, 0, 0, 'R' },
                { p::NoiseDecay, 100, 250, 'B' }, { p::Decay, 60, 160, 'B' }, { p::Filter, 1, 1, 'R' }, { p::Cutoff, 1500, 4000, 'A' },
                { p::Drive, 0.1f, 0.5f, 'R' } },
              { 0.4f, 0.7f, 0.4f, 1.0f }, bit(R::Snare) | bit(R::Noise) },
        } };
    return e;
}

// --- The ping ----------------------------------------------------------------------------------------------------------
const Engine& pingEngine()
{
    namespace g = ping;
    static const Engine e{
        { { "Deep", "Dim", "Veiled", "Soft", "Clear", "Glassy", "Shimmering", "Dazzling" },
          { "Sunken", "Murky", "Watery", "Liquid", "Crystal", "Frosted", "Gleaming", "Luminous" },
          { "Night", "Dusk", "Shade", "Grey", "Silver", "Chrome", "Neon", "Laser" },
          { "Hollow", "Wooden", "Tawny", "Amber", "Golden", "Brass", "Bronze", "Starlit" } },
        {
            { "Glass Ping", 0, { "Prism", "Lens", "Mirror", "Pane", "Vial", "Bead", "Marble", "Droplet" },
              { { g::Ratio, 1.4f, 2.0f, 'R' }, { g::Index, 1.0f, 2.5f, 'A' }, { g::IndexDecay, 30, 90, 'B' }, { g::Decay, 90, 220, 'B' },
                { g::Band, 800, 2000, 'A' }, { g::BandQ, 0.6f, 1.5f, 'R' }, { g::Lpg, 0.4f, 0.8f, 'R' }, { g::Resonance, 0.2f, 0.5f, 'R' } },
              { 1.0f, 0.6f, 0.6f, 0.5f } },
            { "Metal Ping", 1, { "Anvil", "Tine", "Fork", "Wire", "String", "Rod", "Tube", "Pipe" },
              { { g::Ratio, 2.0f, 3.5f, 'R' }, { g::Index, 2.0f, 4.0f, 'A' }, { g::IndexDecay, 40, 120, 'B' }, { g::Decay, 80, 180, 'B' },
                { g::Band, 1000, 3000, 'A' }, { g::BandQ, 0.8f, 2.0f, 'R' } },
              { 0.5f, 0.5f, 0.2f, 1.0f } },
            { "Wood Ping", 2, { "Marimba", "Xylophone", "Kalimba", "Mbira", "Balafon", "Slit Drum", "Temple Block", "Gamelan" },
              { { g::Ratio, 1.0f, 1.5f, 'R' }, { g::Index, 0.5f, 1.5f, 'A' }, { g::IndexDecay, 20, 60, 'B' }, { g::Decay, 90, 200, 'B' },
                { g::Band, 500, 1200, 'A' }, { g::Lpg, 0.6f, 0.9f, 'R' }, { g::LpgRelease, 80, 200, 'R' } },
              { 0.9f, 0.6f, 0.7f, 0.3f } },
            { "Bell Ping", 3, { "Bell", "Chime", "Gong", "Cowbell", "Handbell", "Singing Bowl", "Glockenspiel", "Celesta" },
              { { g::Ratio, 2.4f, 3.5f, 'R' }, { g::Index, 1.5f, 3.0f, 'A' }, { g::IndexDecay, 60, 200, 'B' }, { g::Decay, 150, 220, 'B' },
                { g::Band, 1000, 2500, 'A' }, { g::BandMix, 0.3f, 0.6f, 'R' } },
              { 0.6f, 0.5f, 0.5f, 0.6f } },
            { "Dub Drop", 0, { "Drop", "Drip", "Plop", "Splash", "Ripple", "Puddle", "Spring", "Fountain" },
              { { g::Ratio, 1.2f, 1.8f, 'R' }, { g::PitchAmount, 1.0f, 2.5f, 'B' }, { g::PitchDecay, 30, 120, 'B' }, { g::Decay, 120, 220, 'R' },
                { g::Band, 600, 1400, 'A' }, { g::Sweep, 0.5f, 1.2f, 'R' } },
              { 0.6f, 0.3f, 1.0f, 0.2f } },
            { "Sonar", 1, { "Sonar", "Radar", "Beacon", "Signal", "Transponder", "Echo Sounder", "Pulse", "Probe" },
              { { g::Ratio, 1.0f, 1.2f, 'R' }, { g::Index, 0.3f, 1.2f, 'A' }, { g::Decay, 150, 220, 'B' }, { g::LpgRelease, 150, 400, 'B' },
                { g::Band, 500, 1500, 'A' }, { g::Resonance, 0.3f, 0.6f, 'R' }, { g::Sweep, 0.2f, 0.8f, 'R' } },
              { 1.0f, 0.4f, 0.7f, 0.3f } },
            { "Water Drip", 2, { "Rain", "Dew", "Brook", "Rill", "Creek", "Stream", "Rivulet", "Trickle" },
              { { g::PitchAmount, 1.5f, 3.0f, 'B' }, { g::PitchDecay, 10, 40, 'B' }, { g::Ratio, 1.4f, 2.2f, 'R' }, { g::Decay, 90, 160, 'R' },
                { g::Band, 800, 2000, 'A' } },
              { 0.8f, 0.4f, 0.8f, 0.3f } },
            { "Hollow Ping", 3, { "Flute", "Ocarina", "Whistle", "Pipe Organ", "Recorder", "Panpipe", "Reed", "Horn" },
              { { g::Ratio, 0.5f, 1.0f, 'R' }, { g::Index, 0.5f, 1.5f, 'A' }, { g::BandQ, 1.5f, 4.0f, 'B' }, { g::BandMix, 0.5f, 0.9f, 'R' },
                { g::Band, 600, 1500, 'A' }, { g::Decay, 100, 220, 'R' } },
              { 0.9f, 0.5f, 0.6f, 0.3f } },
            { "Bright Blip", 0, { "Blip", "Bleep", "Chirp", "Tweet", "Peep", "Beep", "Ping", "Pip" },
              { { g::Ratio, 1.5f, 3.0f, 'R' }, { g::Index, 1.5f, 3.5f, 'A' }, { g::IndexDecay, 15, 50, 'B' }, { g::Decay, 60, 120, 'B' },
                { g::Band, 1500, 4000, 'A' }, { g::Lpg, 0.3f, 0.6f, 'R' } },
              { 0.7f, 0.7f, 0.3f, 0.8f } },
            { "Dark Blip", 1, { "Mole", "Beetle", "Cricket", "Moth", "Bat", "Owl", "Raven", "Crow" },
              { { g::Ratio, 1.0f, 2.0f, 'R' }, { g::Index, 0.5f, 2.0f, 'A' }, { g::IndexDecay, 20, 60, 'B' }, { g::Decay, 80, 160, 'B' },
                { g::Band, 400, 900, 'A' }, { g::Lpg, 0.6f, 0.9f, 'R' } },
              { 1.0f, 0.6f, 0.6f, 0.5f } },
            { "FM Bell", 2, { "Clock", "Carillon", "Tower Bell", "Sleigh Bell", "Ship Bell", "Doorbell", "Bicycle Bell", "School Bell" },
              { { g::Ratio, 2.8f, 3.5f, 'R' }, { g::Index, 2.5f, 5.0f, 'A' }, { g::IndexDecay, 80, 250, 'B' }, { g::Decay, 150, 220, 'R' },
                { g::Band, 1500, 3500, 'A' } },
              { 0.4f, 0.4f, 0.4f, 0.8f } },
            { "Soft Pluck", 3, { "Harp", "Lute", "Zither", "Koto", "Sitar", "Dulcimer", "Lyre", "Oud" },
              { { g::Ratio, 1.0f, 2.0f, 'R' }, { g::Index, 0.5f, 1.5f, 'A' }, { g::IndexDecay, 30, 100, 'R' }, { g::Decay, 120, 220, 'B' },
                { g::Lpg, 0.5f, 0.8f, 'R' }, { g::LpgRelease, 100, 300, 'B' }, { g::Band, 600, 1400, 'A' } },
              { 0.8f, 0.5f, 0.8f, 0.2f } },
            { "Resonant Knock", 0, { "Knock", "Rap", "Tap", "Thud", "Bump", "Thump", "Clunk", "Clonk" },
              { { g::Resonance, 0.5f, 0.8f, 'B' }, { g::BandQ, 2.0f, 5.0f, 'B' }, { g::Band, 500, 1500, 'A' }, { g::Ratio, 1.0f, 2.0f, 'R' },
                { g::Decay, 80, 160, 'R' } },
              { 0.8f, 0.7f, 0.4f, 0.6f } },
            { "Sweeping Ping", 1, { "Pendulum", "Swing", "Sway", "Arc", "Orbit", "Spiral", "Vortex", "Carousel" },
              { { g::Sweep, 0.8f, 1.8f, 'B' }, { g::SweepRate, 0.02f, 0.2f, 'B' }, { g::Band, 600, 1600, 'A' }, { g::Ratio, 1.2f, 2.2f, 'R' },
                { g::Decay, 100, 200, 'R' }, { g::BandQ, 0.8f, 2.0f, 'R' } },
              { 1.0f, 0.4f, 0.6f, 0.4f } },
            { "Tight Tick", 2, { "Tick", "Click", "Tock", "Clack", "Chink", "Clink", "Plink", "Tink" },
              { { g::Decay, 30, 80, 'B' }, { g::IndexDecay, 5, 30, 'B' }, { g::Index, 1.5f, 3.5f, 'A' }, { g::Ratio, 1.5f, 3.0f, 'R' },
                { g::Band, 1200, 3500, 'A' } },
              { 0.7f, 0.8f, 0.3f, 0.7f } },
            { "Long Drop", 3, { "Well", "Cistern", "Reservoir", "Basin", "Pond", "Lake", "Fjord", "Ocean" },
              { { g::Decay, 180, 220, 'R' }, { g::LpgRelease, 300, 800, 'B' }, { g::PitchAmount, 0.5f, 1.5f, 'R' }, { g::PitchDecay, 60, 200, 'B' },
                { g::Band, 500, 1300, 'A' }, { g::Ratio, 1.2f, 1.8f, 'R' } },
              { 0.8f, 0.3f, 0.9f, 0.2f } },
        } };
    return e;
}

// --- The bass synth ----------------------------------------------------------------------------------------------------------
const Engine& bassEngine()
{
    namespace s = synth;
    static const Engine e{
        { { "Sunken", "Buried", "Dark", "Round", "Firm", "Growling", "Snarling", "Biting" },
          { "Velvet", "Suede", "Felt", "Rubber", "Leather", "Vinyl", "Plastic", "Chrome" },
          { "Coal", "Soot", "Tar", "Clay", "Rust", "Copper", "Brass", "Steel" },
          { "Night", "Late", "Deep", "Low", "Middle", "Upper", "High", "Top" } },
        {
            { "Rolling Ladder", 0, { "Roller", "Wheel", "Cylinder", "Drum", "Spool", "Reel", "Bobbin", "Pulley" },
              { { s::Filter, 0, 0, 'R' }, { s::Wave, 0.0f, 0.2f, 'R' }, { s::Cutoff, 180, 700, 'A' }, { s::Resonance, 0.1f, 0.35f, 'B' },
                { s::EnvAmount, 1.5f, 3.5f, 'R' }, { s::Decay, 120, 400, 'B' }, { s::SubOsc, 0.2f, 0.5f, 'R' }, { s::Drive, 0.1f, 0.35f, 'R' } },
              { 1.0f, 0.8f, 0.4f, 0.6f } },
            { "Deep Sub Saw", 1, { "Trench", "Rift", "Chasm", "Canyon", "Gorge", "Ravine", "Gully", "Gulf" },
              { { s::Filter, 0, 0, 'R' }, { s::Wave, 0.0f, 0.1f, 'R' }, { s::Cutoff, 120, 400, 'A' }, { s::Resonance, 0.05f, 0.2f, 'R' },
                { s::EnvAmount, 0.8f, 2.0f, 'B' }, { s::Decay, 200, 600, 'B' }, { s::SubOsc, 0.4f, 0.7f, 'R' } },
              { 0.9f, 0.6f, 0.7f, 0.4f } },
            { "Pulse Bass", 2, { "Pulse", "Square", "Block", "Brick", "Cube", "Tile", "Crate", "Box" },
              { { s::Filter, 2, 2, 'R' }, { s::Wave, 0.8f, 1.0f, 'R' }, { s::PulseWidth, 0.2f, 0.5f, 'B' }, { s::Cutoff, 250, 900, 'A' },
                { s::Resonance, 0.1f, 0.3f, 'R' }, { s::EnvAmount, 1.0f, 2.5f, 'R' }, { s::Decay, 100, 300, 'R' } },
              { 0.7f, 0.8f, 0.4f, 0.5f } },
            { "Dub Bass", 3, { "Dubwise", "Skank", "Version", "Steppa", "Rockers", "Roots", "Toast", "Echo Chamber" },
              { { s::Filter, 3, 3, 'R' }, { s::Wave, 0.0f, 0.3f, 'R' }, { s::Cutoff, 150, 450, 'A' }, { s::Resonance, 0.05f, 0.25f, 'R' },
                { s::EnvAmount, 0.5f, 1.5f, 'B' }, { s::AmpSustain, 0.6f, 0.9f, 'R' }, { s::AmpRelease, 150, 400, 'B' }, { s::SubOsc, 0.3f, 0.6f, 'R' } },
              { 0.3f, 0.2f, 1.0f, 0.1f } },
            { "Plucked Bass", 0, { "Pluck", "Flick", "Twang", "Strum", "Pick", "Plectrum", "Snap", "Fret" },
              { { s::Filter, 0, 2, 'R' }, { s::Wave, 0.0f, 0.5f, 'R' }, { s::Cutoff, 200, 800, 'A' }, { s::EnvAmount, 2.0f, 4.0f, 'R' },
                { s::Decay, 60, 200, 'B' }, { s::AmpDecay, 80, 250, 'B' }, { s::AmpSustain, 0.1f, 0.4f, 'R' } },
              { 0.8f, 0.9f, 0.3f, 0.6f } },
            { "Warm Juno", 1, { "Hearth", "Lantern", "Candle", "Ember", "Glow", "Torch", "Beacon", "Flare" },
              { { s::Filter, 2, 2, 'R' }, { s::Wave, 0.0f, 0.6f, 'R' }, { s::Cutoff, 300, 1000, 'A' }, { s::Resonance, 0.1f, 0.3f, 'B' },
                { s::EnvAmount, 1.0f, 2.5f, 'R' }, { s::Decay, 150, 400, 'R' }, { s::SubOsc, 0.2f, 0.5f, 'R' } },
              { 0.7f, 0.7f, 0.6f, 0.3f } },
            { "Prophet Growl", 2, { "Growl", "Snarl", "Roar", "Rumble", "Grumble", "Howl", "Bellow", "Bark" },
              { { s::Filter, 1, 1, 'R' }, { s::Wave, 0.0f, 0.3f, 'R' }, { s::Cutoff, 200, 800, 'A' }, { s::Resonance, 0.2f, 0.5f, 'B' },
                { s::EnvAmount, 1.5f, 3.5f, 'R' }, { s::Drive, 0.3f, 0.6f, 'R' }, { s::Decay, 150, 400, 'R' } },
              { 0.5f, 0.7f, 0.2f, 0.8f } },
            { "SEM Round", 3, { "Round", "Circle", "Loop", "Ring", "Halo", "Hoop", "Wreath", "Band" },
              { { s::Filter, 3, 3, 'R' }, { s::Wave, 0.0f, 0.4f, 'R' }, { s::Cutoff, 200, 700, 'A' }, { s::Resonance, 0.1f, 0.3f, 'B' },
                { s::EnvAmount, 1.0f, 2.5f, 'R' }, { s::Decay, 150, 450, 'R' } },
              { 0.8f, 0.6f, 0.6f, 0.4f } },
            { "Wasp Buzz", 0, { "Wasp", "Hornet", "Bee", "Gnat", "Midge", "Fly", "Mosquito", "Cicada" },
              { { s::Filter, 8, 8, 'R' }, { s::Wave, 0.0f, 0.6f, 'R' }, { s::Cutoff, 250, 900, 'A' }, { s::Resonance, 0.2f, 0.5f, 'B' },
                { s::EnvAmount, 1.5f, 3.0f, 'R' }, { s::Drive, 0.3f, 0.6f, 'R' } },
              { 0.3f, 0.6f, 0.1f, 1.0f } },
            { "MS-20 Bite", 1, { "Bite", "Fang", "Tooth", "Tusk", "Claw", "Talon", "Beak", "Sting" },
              { { s::Filter, 6, 6, 'R' }, { s::Wave, 0.0f, 0.4f, 'R' }, { s::Cutoff, 250, 1000, 'A' }, { s::Resonance, 0.3f, 0.6f, 'B' },
                { s::EnvAmount, 1.5f, 3.5f, 'R' }, { s::Drive, 0.2f, 0.5f, 'R' }, { s::Decay, 100, 300, 'R' } },
              { 0.4f, 0.7f, 0.1f, 0.9f } },
            { "Hollow Square", 2, { "Hollow", "Cavity", "Void", "Well", "Chamber", "Shell", "Husk", "Pod" },
              { { s::Filter, 0, 3, 'R' }, { s::Wave, 1.0f, 1.0f, 'R' }, { s::PulseWidth, 0.35f, 0.5f, 'R' }, { s::Cutoff, 200, 700, 'A' },
                { s::Resonance, 0.05f, 0.25f, 'R' }, { s::EnvAmount, 0.5f, 2.0f, 'B' } },
              { 0.8f, 0.5f, 0.7f, 0.3f } },
            { "Driven Tool", 3, { "Hammer", "Chisel", "Mallet", "Pliers", "Clamp", "Saw", "Drill", "Rasp" },
              { { s::Filter, 0, 1, 'R' }, { s::Drive, 0.4f, 0.8f, 'B' }, { s::Cutoff, 250, 900, 'A' }, { s::Resonance, 0.15f, 0.4f, 'R' },
                { s::EnvAmount, 1.5f, 3.0f, 'R' }, { s::Decay, 100, 300, 'R' } },
              { 0.4f, 0.8f, 0.1f, 0.9f } },
            { "Short Blip Bass", 0, { "Blip", "Dot", "Pip", "Point", "Tick", "Jot", "Nib", "Stub" },
              { { s::Filter, 0, 2, 'R' }, { s::AmpDecay, 40, 120, 'B' }, { s::AmpSustain, 0.0f, 0.2f, 'R' }, { s::Cutoff, 250, 900, 'A' },
                { s::EnvAmount, 2.0f, 4.0f, 'R' }, { s::Decay, 40, 120, 'B' } },
              { 0.8f, 0.9f, 0.2f, 0.6f } },
            { "Filtered Drone Bass", 1, { "Drone", "Hum", "Buzz", "Whir", "Moan", "Groan", "Murmur", "Mumble" },
              { { s::Filter, 0, 3, 'R' }, { s::Cutoff, 120, 400, 'A' }, { s::Resonance, 0.2f, 0.45f, 'B' }, { s::EnvAmount, 0.3f, 1.0f, 'R' },
                { s::AmpSustain, 0.8f, 1.0f, 'R' }, { s::AmpRelease, 200, 600, 'R' }, { s::Glide, 30, 120, 'R' } },
              { 0.9f, 0.4f, 0.8f, 0.3f } },
            { "Rubber Bass", 2, { "Rubber", "Latex", "Elastic", "Bungee", "Spring", "Sponge", "Gel", "Putty" },
              { { s::Filter, 0, 5, 'R' }, { s::Cutoff, 200, 700, 'A' }, { s::Resonance, 0.3f, 0.55f, 'B' }, { s::EnvAmount, 2.0f, 3.5f, 'R' },
                { s::Decay, 100, 250, 'R' }, { s::Glide, 20, 80, 'R' }, { s::Accent, 0.3f, 0.6f, 'R' } },
              { 0.7f, 0.8f, 0.3f, 0.6f } },
            { "Detroit Low", 3, { "Motor City", "Assembly", "Belt Line", "Plant", "Works", "Transmission", "Chassis", "Axle" },
              { { s::Filter, 1, 2, 'R' }, { s::Wave, 0.0f, 0.5f, 'R' }, { s::Cutoff, 250, 850, 'A' }, { s::Resonance, 0.15f, 0.35f, 'B' },
                { s::EnvAmount, 1.5f, 3.0f, 'R' }, { s::Decay, 120, 350, 'R' }, { s::SubOsc, 0.2f, 0.4f, 'R' } },
              { 0.7f, 0.9f, 0.3f, 0.5f } },
        } };
    return e;
}

// --- The 303 ----------------------------------------------------------------------------------------------------------
const Engine& acidEngine()
{
    namespace s = synth;
    static const Engine e{
        { { "Sour", "Bitter", "Tart", "Sharp", "Tangy", "Zesty", "Acrid", "Caustic" },
          { "Slimy", "Oily", "Waxy", "Glossy", "Slick", "Wet", "Dripping", "Molten" },
          { "Swampy", "Boggy", "Marshy", "Muddy", "Silty", "Brackish", "Briny", "Toxic" },
          { "Late", "Midnight", "Warehouse", "Rave", "Strobe", "Laser", "Sunrise", "Daylight" } },
        {
            { "Classic 303", 0, { "Squelch", "Chirp", "Wobble", "Warble", "Gurgle", "Bubble", "Burble", "Twitter" },
              { { s::Filter, 5, 5, 'R' }, { s::Cutoff, 300, 1100, 'A' }, { s::Resonance, 0.6f, 0.85f, 'B' }, { s::EnvAmount, 2.0f, 3.5f, 'R' },
                { s::Decay, 150, 350, 'B' }, { s::Accent, 0.5f, 0.75f, 'R' }, { s::Glide, 40, 90, 'R' }, { s::Drive, 0.3f, 0.6f, 'R' } },
              { 0.6f, 0.7f, 0.3f, 0.9f } },
            { "Squelch", 1, { "Squish", "Splat", "Slosh", "Splodge", "Smear", "Goo", "Ooze", "Slurp" },
              { { s::Filter, 5, 5, 'R' }, { s::Cutoff, 400, 1200, 'A' }, { s::Resonance, 0.7f, 0.9f, 'B' }, { s::EnvAmount, 2.5f, 4.0f, 'R' },
                { s::Decay, 120, 300, 'R' }, { s::Accent, 0.6f, 0.85f, 'R' } },
              { 0.4f, 0.6f, 0.1f, 1.0f } },
            { "Rubber Acid", 2, { "Band", "Hose", "Tyre", "Tube", "Balloon", "Ball", "Duck", "Boot" },
              { { s::Filter, 5, 5, 'R' }, { s::Cutoff, 300, 900, 'A' }, { s::Resonance, 0.55f, 0.75f, 'B' }, { s::EnvAmount, 1.5f, 2.5f, 'R' },
                { s::Decay, 200, 400, 'B' }, { s::Glide, 60, 120, 'R' } },
              { 0.7f, 0.6f, 0.4f, 0.6f } },
            { "Dark Acid", 3, { "Cellar", "Crypt", "Dungeon", "Vault", "Bunker", "Undercroft", "Tomb", "Hold" },
              { { s::Filter, 5, 5, 'R' }, { s::Cutoff, 250, 600, 'A' }, { s::Resonance, 0.6f, 0.8f, 'B' }, { s::EnvAmount, 1.5f, 3.0f, 'R' },
                { s::Decay, 180, 400, 'R' }, { s::Drive, 0.4f, 0.7f, 'R' } },
              { 0.9f, 0.6f, 0.3f, 0.6f } },
            { "Screamer", 0, { "Siren", "Klaxon", "Alarm", "Whistle", "Shriek", "Wail", "Howl", "Scream" },
              { { s::Filter, 5, 6, 'R' }, { s::Cutoff, 600, 1500, 'A' }, { s::Resonance, 0.8f, 0.95f, 'B' }, { s::EnvAmount, 3.0f, 4.5f, 'R' },
                { s::Drive, 0.5f, 0.8f, 'R' }, { s::Accent, 0.7f, 0.9f, 'R' } },
              { 0.1f, 0.4f, 0.0f, 1.0f } },
            { "Soft Acid", 1, { "Pillow", "Cushion", "Quilt", "Duvet", "Blanket", "Mattress", "Hammock", "Cradle" },
              { { s::Filter, 5, 5, 'R' }, { s::Cutoff, 300, 800, 'A' }, { s::Resonance, 0.45f, 0.65f, 'B' }, { s::EnvAmount, 1.0f, 2.0f, 'R' },
                { s::Decay, 200, 450, 'R' }, { s::Drive, 0.1f, 0.3f, 'R' } },
              { 0.8f, 0.5f, 0.7f, 0.2f } },
            { "Hypnotic Acid", 2, { "Spiral", "Helix", "Coil", "Vortex", "Eddy", "Swirl", "Whorl", "Gyre" },
              { { s::Filter, 5, 5, 'R' }, { s::Cutoff, 350, 900, 'A' }, { s::Resonance, 0.6f, 0.8f, 'B' }, { s::EnvAmount, 1.5f, 3.0f, 'R' },
                { s::Decay, 150, 350, 'R' }, { s::Glide, 50, 100, 'R' }, { s::Accent, 0.4f, 0.7f, 'R' } },
              { 1.0f, 0.6f, 0.4f, 0.6f } },
            { "Detroit Line", 3, { "Line", "Track", "Circuit", "Grid", "Route", "Beltway", "Freeway", "Parkway" },
              { { s::Filter, 5, 5, 'R' }, { s::Cutoff, 400, 1000, 'A' }, { s::Resonance, 0.55f, 0.75f, 'B' }, { s::EnvAmount, 2.0f, 3.0f, 'R' },
                { s::Decay, 150, 300, 'R' }, { s::Drive, 0.2f, 0.5f, 'R' } },
              { 0.6f, 0.9f, 0.3f, 0.5f } },
            { "Wasp Acid", 0, { "Swarm", "Hive", "Nest", "Comb", "Colony", "Cluster", "Brood", "Horde" },
              { { s::Filter, 8, 8, 'R' }, { s::Cutoff, 400, 1100, 'A' }, { s::Resonance, 0.6f, 0.85f, 'B' }, { s::EnvAmount, 2.0f, 3.5f, 'R' },
                { s::Drive, 0.4f, 0.7f, 'R' } },
              { 0.3f, 0.5f, 0.1f, 1.0f } },
            { "MS-20 Acid", 1, { "Patch", "Cable", "Jack", "Socket", "Plug", "Lead", "Cord", "Wire" },
              { { s::Filter, 6, 6, 'R' }, { s::Cutoff, 400, 1200, 'A' }, { s::Resonance, 0.65f, 0.9f, 'B' }, { s::EnvAmount, 2.0f, 4.0f, 'R' },
                { s::Drive, 0.3f, 0.6f, 'R' }, { s::Decay, 120, 300, 'R' } },
              { 0.3f, 0.6f, 0.1f, 0.9f } },
            { "Ladder Acid", 2, { "Rung", "Step", "Stair", "Tread", "Riser", "Landing", "Flight", "Banister" },
              { { s::Filter, 0, 0, 'R' }, { s::Cutoff, 350, 1000, 'A' }, { s::Resonance, 0.6f, 0.85f, 'B' }, { s::EnvAmount, 2.0f, 3.5f, 'R' },
                { s::Decay, 150, 350, 'R' }, { s::Accent, 0.5f, 0.8f, 'R' } },
              { 0.7f, 0.7f, 0.3f, 0.7f } },
            { "Pulse Acid", 3, { "Gate", "Valve", "Switch", "Relay", "Trigger", "Toggle", "Latch", "Shutter" },
              { { s::Filter, 5, 5, 'R' }, { s::Wave, 1.0f, 1.0f, 'R' }, { s::PulseWidth, 0.3f, 0.5f, 'R' }, { s::Cutoff, 350, 1000, 'A' },
                { s::Resonance, 0.6f, 0.8f, 'B' }, { s::EnvAmount, 2.0f, 3.5f, 'R' } },
              { 0.6f, 0.7f, 0.2f, 0.7f } },
            { "Deep Acid", 0, { "Depth", "Fathom", "Plumb", "Sounding", "Keel", "Hull", "Bilge", "Bottom" },
              { { s::Filter, 5, 5, 'R' }, { s::Cutoff, 250, 700, 'A' }, { s::Resonance, 0.5f, 0.75f, 'B' }, { s::EnvAmount, 1.5f, 2.5f, 'R' },
                { s::Decay, 250, 500, 'R' }, { s::SubOsc, 0.2f, 0.5f, 'R' } },
              { 0.9f, 0.5f, 0.6f, 0.4f } },
            { "Bright Acid", 1, { "Flash", "Spark", "Glint", "Gleam", "Flicker", "Twinkle", "Sparkle", "Blaze" },
              { { s::Filter, 5, 5, 'R' }, { s::Cutoff, 700, 1500, 'A' }, { s::Resonance, 0.6f, 0.85f, 'B' }, { s::EnvAmount, 2.0f, 3.5f, 'R' },
                { s::Decay, 100, 250, 'R' } },
              { 0.4f, 0.7f, 0.1f, 0.8f } },
            { "Gliding Acid", 2, { "Glider", "Skater", "Surfer", "Sailor", "Diver", "Swimmer", "Flyer", "Drifter" },
              { { s::Filter, 5, 5, 'R' }, { s::Glide, 80, 160, 'B' }, { s::Cutoff, 350, 900, 'A' }, { s::Resonance, 0.6f, 0.8f, 'R' },
                { s::EnvAmount, 1.5f, 3.0f, 'R' }, { s::Decay, 180, 350, 'R' } },
              { 0.8f, 0.6f, 0.4f, 0.6f } },
            { "Bitter Acid", 3, { "Vinegar", "Lemon", "Lime", "Quince", "Rhubarb", "Sorrel", "Tamarind", "Sloe" },
              { { s::Filter, 5, 7, 'R' }, { s::Cutoff, 400, 1100, 'A' }, { s::Resonance, 0.7f, 0.9f, 'B' }, { s::Drive, 0.5f, 0.8f, 'R' },
                { s::EnvAmount, 2.5f, 4.0f, 'R' } },
              { 0.2f, 0.5f, 0.0f, 1.0f } },
        } };
    return e;
}

// --- The dub chord ----------------------------------------------------------------------------------------------------------
const Engine& chordEngine()
{
    namespace c = chord;
    static const Engine e{
        { { "Foggy", "Hazy", "Murky", "Soft", "Clear", "Airy", "Bright", "Sparkling" },
          { "Dusty", "Faded", "Worn", "Weathered", "Polished", "Fresh", "Gleaming", "Radiant" },
          { "Submerged", "Drowned", "Sunken", "Floating", "Drifting", "Rising", "Soaring", "Beaming" },
          { "Smoky", "Ashen", "Charred", "Amber", "Honeyed", "Golden", "Silvered", "Crystalline" } },
        {
            { "Basic Channel", 0, { "Channel", "Canal", "Waterway", "Inlet", "Sound", "Strait", "Firth", "Delta" },
              { { c::Detune, 6, 15, 'R' }, { c::Band, 300, 480, 'A' }, { c::Bright, 2000, 4000, 'A' }, { c::Decay, 200, 450, 'B' },
                { c::Sustain, 0.05f, 0.25f, 'R' }, { c::Release, 300, 700, 'B' }, { c::Crush, 7, 10, 'R' }, { c::CrushMix, 0.1f, 0.3f, 'R' },
                { c::Phaser, 0.3f, 0.6f, 'R' }, { c::Width, 0.6f, 0.9f, 'R' } },
              { 0.6f, 0.5f, 1.0f, 0.2f } },
            { "Deep Stab", 1, { "Stab", "Thrust", "Lunge", "Jab", "Prod", "Poke", "Plunge", "Dive" },
              { { c::Detune, 5, 12, 'R' }, { c::Band, 250, 420, 'A' }, { c::Bright, 1500, 3000, 'A' }, { c::Decay, 120, 300, 'B' },
                { c::Sustain, 0.0f, 0.15f, 'R' }, { c::Release, 200, 500, 'R' }, { c::EnvAmount, 1.0f, 2.0f, 'R' } },
              { 0.8f, 0.8f, 0.7f, 0.4f } },
            { "Glass Stab", 2, { "Crystal", "Icicle", "Frost", "Glaze", "Sleet", "Hail", "Rime", "Quartz" },
              { { c::Detune, 4, 10, 'R' }, { c::Band, 380, 480, 'A' }, { c::Bright, 3500, 7000, 'A' }, { c::Decay, 100, 250, 'B' },
                { c::Sustain, 0.0f, 0.1f, 'R' }, { c::BandMode, 1, 1, 'R' }, { c::Phaser, 0.2f, 0.5f, 'R' } },
              { 0.6f, 0.7f, 0.5f, 0.5f } },
            { "Dub Chord", 3, { "Dubplate", "Echo", "Delay", "Spring", "Tape", "Reverb", "Siren", "Chamber" },
              { { c::Detune, 8, 15, 'R' }, { c::Band, 300, 450, 'A' }, { c::Bright, 2500, 4500, 'A' }, { c::Decay, 250, 500, 'B' },
                { c::Sustain, 0.1f, 0.3f, 'R' }, { c::Release, 400, 900, 'B' }, { c::Crush, 6, 9, 'R' }, { c::CrushMix, 0.15f, 0.35f, 'R' },
                { c::Dip, -5, -2, 'R' }, { c::Width, 0.8f, 0.95f, 'R' } },
              { 0.3f, 0.2f, 1.0f, 0.1f } },
            { "Organ Chord", 0, { "Organ", "Harmonium", "Accordion", "Bandoneon", "Concertina", "Melodica", "Calliope", "Reed Organ" },
              { { c::Detune, 3, 8, 'R' }, { c::Attack, 5, 20, 'R' }, { c::Sustain, 0.4f, 0.7f, 'B' }, { c::Decay, 300, 700, 'R' },
                { c::Band, 320, 480, 'A' }, { c::Bright, 2000, 4000, 'A' }, { c::BandQ, 0.4f, 0.8f, 'R' } },
              { 0.5f, 0.4f, 0.7f, 0.2f } },
            { "Crushed Stab", 1, { "Crush", "Grind", "Mash", "Pulp", "Crumble", "Shatter", "Splinter", "Fracture" },
              { { c::Crush, 5, 8, 'B' }, { c::CrushMix, 0.3f, 0.6f, 'B' }, { c::Detune, 6, 14, 'R' }, { c::Band, 300, 460, 'A' },
                { c::Bright, 2000, 4500, 'A' }, { c::Decay, 150, 350, 'R' } },
              { 0.4f, 0.5f, 0.8f, 0.6f } },
            { "Phased Chord", 2, { "Phase", "Crescent", "Gibbous", "Quarter", "Waning", "Waxing", "New Moon", "Full Moon" },
              { { c::Phaser, 0.6f, 0.95f, 'B' }, { c::PhaserRate, 0.1f, 1.0f, 'B' }, { c::Detune, 6, 14, 'R' }, { c::Band, 300, 460, 'A' },
                { c::Bright, 2000, 4000, 'A' }, { c::Decay, 200, 450, 'R' } },
              { 0.7f, 0.4f, 0.9f, 0.3f } },
            { "Soft Pad Stab", 3, { "Cloud", "Fleece", "Down", "Feather", "Cotton", "Wool", "Silk", "Satin" },
              { { c::Attack, 20, 80, 'B' }, { c::Decay, 400, 900, 'B' }, { c::Sustain, 0.2f, 0.5f, 'R' }, { c::Release, 500, 1200, 'R' },
                { c::Band, 300, 460, 'A' }, { c::Bright, 1500, 3500, 'A' }, { c::Detune, 8, 16, 'R' } },
              { 0.7f, 0.3f, 0.8f, 0.1f } },
            { "Bright Stab", 0, { "Beam", "Ray", "Shaft", "Glare", "Flare", "Burst", "Nova", "Corona" },
              { { c::Bright, 4000, 8000, 'A' }, { c::Band, 400, 480, 'R' }, { c::Decay, 100, 250, 'B' }, { c::Sustain, 0.0f, 0.15f, 'R' },
                { c::EnvAmount, 1.5f, 3.0f, 'R' }, { c::Detune, 5, 12, 'R' } },
              { 0.4f, 0.8f, 0.4f, 0.7f } },
            { "Filtered Minor", 1, { "Minor", "Lament", "Elegy", "Dirge", "Requiem", "Nocturne", "Reverie", "Threnody" },
              { { c::Band, 280, 450, 'A' }, { c::BandQ, 0.8f, 2.0f, 'B' }, { c::Bright, 1800, 3500, 'A' }, { c::Decay, 200, 500, 'R' },
                { c::Sweep, 0.3f, 0.9f, 'R' }, { c::SweepRate, 0.05f, 0.4f, 'R' }, { c::Detune, 6, 12, 'R' } },
              { 0.9f, 0.6f, 0.7f, 0.3f } },
            { "Short Blip Chord", 2, { "Blip", "Pip", "Dot", "Beep", "Tick", "Click", "Chirp", "Cheep" },
              { { c::Decay, 60, 150, 'B' }, { c::Sustain, 0.0f, 0.05f, 'R' }, { c::Release, 100, 300, 'R' }, { c::Band, 330, 480, 'A' },
                { c::Bright, 2500, 5000, 'A' }, { c::Detune, 4, 10, 'R' } },
              { 0.7f, 0.8f, 0.5f, 0.5f } },
            { "Long Swell", 3, { "Swell", "Tide", "Bloom", "Dawn", "Sunrise", "Rising", "Ascent", "Crescendo" },
              { { c::Attack, 80, 400, 'B' }, { c::Decay, 600, 1500, 'R' }, { c::Sustain, 0.3f, 0.6f, 'R' }, { c::Release, 700, 1800, 'B' },
                { c::Band, 280, 440, 'A' }, { c::Bright, 1500, 3500, 'A' } },
              { 0.8f, 0.2f, 0.8f, 0.1f } },
            { "Tape Chord", 0, { "Cassette", "Reel", "Spool", "Loop", "Splice", "Leader", "Capstan", "Pinch Roller" },
              { { c::Detune, 10, 20, 'B' }, { c::Crush, 8, 12, 'R' }, { c::CrushMix, 0.2f, 0.4f, 'R' }, { c::Band, 300, 450, 'A' },
                { c::Bright, 1800, 3500, 'A' }, { c::Decay, 250, 550, 'R' } },
              { 0.5f, 0.3f, 0.9f, 0.2f } },
            { "Mellow Keys", 1, { "Rhodes", "Wurlitzer", "Clavinet", "Celeste", "Piano", "Vibraphone", "Keyboard", "Keys" },
              { { c::Decay, 300, 700, 'B' }, { c::Sustain, 0.1f, 0.3f, 'R' }, { c::Band, 320, 470, 'A' }, { c::Bright, 2000, 3800, 'A' },
                { c::Detune, 3, 8, 'R' }, { c::Phaser, 0.2f, 0.5f, 'R' } },
              { 0.6f, 0.6f, 0.7f, 0.2f } },
            { "Metallic Stab", 2, { "Steel", "Chrome", "Nickel", "Cobalt", "Titanium", "Tungsten", "Platinum", "Iridium" },
              { { c::BandMode, 1, 1, 'R' }, { c::BandQ, 1.0f, 2.5f, 'B' }, { c::Band, 380, 480, 'A' }, { c::Bright, 3000, 6000, 'A' },
                { c::Decay, 120, 280, 'R' }, { c::Crush, 6, 10, 'R' }, { c::CrushMix, 0.2f, 0.4f, 'R' } },
              { 0.3f, 0.7f, 0.3f, 0.8f } },
            { "Warm Minor Pad", 3, { "Twilight", "Evensong", "Vesper", "Gloaming", "Sundown", "Eventide", "Moonrise", "Starlight" },
              { { c::Attack, 30, 120, 'R' }, { c::Decay, 500, 1200, 'B' }, { c::Sustain, 0.3f, 0.5f, 'R' }, { c::Band, 280, 420, 'A' },
                { c::Bright, 1500, 3000, 'A' }, { c::Detune, 8, 15, 'R' }, { c::Width, 0.7f, 0.95f, 'R' } },
              { 0.9f, 0.3f, 0.8f, 0.1f } },
        } };
    return e;
}

// --- The drone ----------------------------------------------------------------------------------------------------------
const Engine& droneEngine()
{
    namespace d = drone;
    static const Engine e{
        { { "Abyssal", "Nocturnal", "Brooding", "Solemn", "Calm", "Serene", "Luminous", "Celestial" },
          { "Frozen", "Icy", "Cold", "Cool", "Mild", "Warm", "Hot", "Molten" },
          { "Lead", "Iron", "Stone", "Clay", "Wood", "Amber", "Gold", "Crystal" },
          { "Silent", "Still", "Hushed", "Quiet", "Murmuring", "Singing", "Ringing", "Shining" } },
        {
            { "Dark Drone", 0, { "Void", "Abyss", "Depth", "Deep", "Underworld", "Netherworld", "Chthonic", "Tartarus" },
              { { d::Detune, 4, 10, 'R' }, { d::Cutoff, 200, 600, 'A' }, { d::Resonance, 0.1f, 0.3f, 'B' }, { d::Sweep, 0.8f, 2.0f, 'R' },
                { d::SweepBars, 32, 64, 'R' }, { d::Attack, 3, 8, 'R' }, { d::Release, 4, 10, 'R' } },
              { 1.0f, 0.5f, 0.7f, 0.4f } },
            { "Warm Drone", 1, { "Hearth", "Glow", "Ember", "Kindling", "Tinder", "Candle", "Lamp", "Lantern" },
              { { d::Detune, 6, 14, 'R' }, { d::Cutoff, 400, 1200, 'A' }, { d::Resonance, 0.1f, 0.25f, 'R' }, { d::Sweep, 1.0f, 2.0f, 'B' },
                { d::SweepBars, 32, 64, 'R' } },
              { 0.8f, 0.4f, 0.9f, 0.2f } },
            { "Glass Drone", 2, { "Pane", "Window", "Crystal", "Prism", "Lens", "Mirror", "Chandelier", "Goblet" },
              { { d::Detune, 3, 8, 'R' }, { d::Cutoff, 1000, 3000, 'A' }, { d::Resonance, 0.3f, 0.5f, 'B' }, { d::Sweep, 0.5f, 1.5f, 'R' } },
              { 0.7f, 0.5f, 0.5f, 0.3f } },
            { "Sweeping Drone", 3, { "Wind", "Gust", "Breeze", "Gale", "Zephyr", "Draught", "Current", "Flow" },
              { { d::Sweep, 2.0f, 3.5f, 'B' }, { d::SweepBars, 16, 64, 'B' }, { d::Cutoff, 300, 1000, 'A' }, { d::Resonance, 0.2f, 0.4f, 'R' },
                { d::Detune, 5, 12, 'R' } },
              { 0.9f, 0.4f, 0.7f, 0.4f } },
            { "Resonant Drone", 0, { "Resonance", "Overtone", "Harmonic", "Partial", "Formant", "Vowel", "Choir", "Chant" },
              { { d::Resonance, 0.45f, 0.75f, 'B' }, { d::Cutoff, 300, 1200, 'A' }, { d::Sweep, 1.0f, 2.5f, 'R' }, { d::Detune, 4, 10, 'R' } },
              { 0.8f, 0.6f, 0.4f, 0.6f } },
            { "Soft Bed", 1, { "Bed", "Mattress", "Pillow", "Blanket", "Quilt", "Cushion", "Nest", "Hammock" },
              { { d::Cutoff, 250, 700, 'A' }, { d::Resonance, 0.0f, 0.15f, 'R' }, { d::Attack, 6, 15, 'B' }, { d::Release, 8, 16, 'B' },
                { d::Detune, 6, 12, 'R' }, { d::Sweep, 0.5f, 1.2f, 'R' } },
              { 0.8f, 0.3f, 0.9f, 0.1f } },
            { "Tape Drone", 2, { "Tape", "Loop", "Reel", "Spool", "Cassette", "Cartridge", "Wire", "Drum" },
              { { d::Detune, 12, 22, 'B' }, { d::Cutoff, 300, 900, 'A' }, { d::Resonance, 0.1f, 0.3f, 'R' }, { d::Sweep, 0.8f, 1.8f, 'R' } },
              { 0.6f, 0.3f, 0.9f, 0.2f } },
            { "Cold Drone", 3, { "Glacier", "Tundra", "Permafrost", "Floe", "Berg", "Frost", "Snowfield", "Ice Shelf" },
              { { d::Cutoff, 600, 2000, 'A' }, { d::Resonance, 0.25f, 0.45f, 'B' }, { d::Detune, 2, 6, 'R' }, { d::Sweep, 1.0f, 2.0f, 'R' } },
              { 0.8f, 0.7f, 0.3f, 0.5f } },
            { "Industrial Hum", 0, { "Transformer", "Substation", "Generator", "Dynamo", "Pylon", "Cable", "Grid", "Mains" },
              { { d::Cutoff, 400, 1500, 'A' }, { d::Resonance, 0.3f, 0.6f, 'B' }, { d::Detune, 1, 4, 'R' }, { d::Sweep, 0.3f, 1.0f, 'R' } },
              { 0.6f, 0.8f, 0.2f, 0.8f } },
            { "Slow Tide", 1, { "Ebb", "Flow", "Neap", "Spring Tide", "Undertow", "Swell", "Current", "Drift" },
              { { d::SweepBars, 64, 128, 'B' }, { d::Sweep, 1.5f, 3.0f, 'R' }, { d::Cutoff, 300, 900, 'A' }, { d::Detune, 5, 12, 'R' },
                { d::Attack, 6, 14, 'R' } },
              { 1.0f, 0.3f, 0.8f, 0.2f } },
            { "Choir Drone", 2, { "Choir", "Chorus", "Chant", "Hymn", "Psalm", "Anthem", "Canticle", "Motet" },
              { { d::Resonance, 0.35f, 0.6f, 'R' }, { d::Cutoff, 500, 1600, 'A' }, { d::Detune, 8, 16, 'B' }, { d::Sweep, 0.8f, 1.8f, 'R' } },
              { 0.8f, 0.4f, 0.6f, 0.2f } },
            { "Low Throb", 3, { "Throb", "Pulse", "Beat", "Heart", "Drum", "Pound", "Thud", "Boom" },
              { { d::Cutoff, 150, 450, 'A' }, { d::Resonance, 0.2f, 0.45f, 'B' }, { d::Sweep, 1.0f, 2.5f, 'R' }, { d::SweepBars, 16, 32, 'R' },
                { d::Detune, 3, 8, 'R' } },
              { 0.9f, 0.6f, 0.5f, 0.5f } },
            { "Airy Drone", 0, { "Air", "Ether", "Sky", "Heaven", "Firmament", "Vault", "Azure", "Empyrean" },
              { { d::Cutoff, 1200, 3500, 'A' }, { d::Resonance, 0.05f, 0.2f, 'R' }, { d::Detune, 6, 12, 'B' }, { d::Attack, 5, 12, 'R' } },
              { 0.7f, 0.3f, 0.7f, 0.2f } },
            { "Rust Drone", 1, { "Rust", "Corrosion", "Patina", "Verdigris", "Oxide", "Scale", "Tarnish", "Decay" },
              { { d::Cutoff, 300, 1000, 'A' }, { d::Resonance, 0.3f, 0.55f, 'B' }, { d::Detune, 8, 18, 'R' }, { d::Sweep, 0.8f, 2.0f, 'R' } },
              { 0.6f, 0.6f, 0.4f, 0.7f } },
            { "Still Drone", 2, { "Lake", "Pond", "Pool", "Mere", "Tarn", "Loch", "Lagoon", "Marsh" },
              { { d::Sweep, 0.1f, 0.5f, 'B' }, { d::Cutoff, 300, 1000, 'A' }, { d::Resonance, 0.1f, 0.25f, 'R' }, { d::Detune, 3, 8, 'R' } },
              { 0.8f, 0.5f, 0.7f, 0.3f } },
            { "Deep Organ Drone", 3, { "Pedal", "Diapason", "Bourdon", "Open Flute", "Contra", "Subbass", "Principal", "Gedackt" },
              { { d::Cutoff, 250, 800, 'A' }, { d::Resonance, 0.05f, 0.2f, 'R' }, { d::Detune, 2, 6, 'B' }, { d::Attack, 2, 6, 'R' },
                { d::Release, 3, 8, 'R' } },
              { 0.7f, 0.5f, 0.6f, 0.3f } },
        } };
    return e;
}

// --- The texture (its amounts scaled so it stays about as loud as the default's: it sits under everything) ----------------------------------------------------------------------------------------------------------
const Engine& textureEngine()
{
    namespace t = texture;
    static const Engine e{
        { { "Faint", "Dim", "Light", "Gentle", "Present", "Heavy", "Dense", "Thick" },
          { "Old", "Worn", "Aged", "Used", "Scuffed", "Scratched", "Battered", "Ruined" },
          { "Narrow", "Close", "Tight", "Near", "Open", "Broad", "Wide", "Vast" },
          { "Clean", "Tidy", "Plain", "Rough", "Coarse", "Gritty", "Grimy", "Filthy" } },
        {
            { "Vinyl Dust", 0, { "Dust", "Lint", "Fluff", "Fibre", "Speck", "Mote", "Flake", "Crumb" },
              { { t::Crackle, 0.2f, 0.7f, 'A' }, { t::Hum, 0.0f, 0.2f, 'R' }, { t::Erosion, 0.1f, 0.4f, 'B' }, { t::Width, 0.6f, 0.9f, 'R' } },
              { 0.8f, 0.4f, 1.0f, 0.3f } },
            { "Mains Hum", 1, { "Wire", "Socket", "Fuse", "Plug", "Cable", "Transformer", "Ballast", "Relay" },
              { { t::Hum, 0.13f, 0.35f, 'A' }, { t::HumHz, 49, 51, 'R' }, { t::Crackle, 0.0f, 0.3f, 'R' }, { t::Erosion, 0.0f, 0.3f, 'B' } },
              { 0.4f, 0.5f, 0.8f, 0.5f } },
            { "Eroded Air", 2, { "Breath", "Sigh", "Whisper", "Draught", "Hiss", "Rustle", "Murmur", "Susurrus" },
              { { t::Erosion, 0.27f, 0.6f, 'A' }, { t::Crackle, 0.0f, 0.3f, 'R' }, { t::Hum, 0.0f, 0.1f, 'R' }, { t::Width, 0.5f, 1.0f, 'B' } },
              { 0.9f, 0.5f, 0.7f, 0.4f } },
            { "Old Record", 3, { "Shellac", "Acetate", "Lacquer", "Dubplate", "Test Press", "White Label", "Promo", "Bootleg" },
              { { t::Crackle, 0.31f, 0.7f, 'A' }, { t::Erosion, 0.2f, 0.6f, 'B' }, { t::Hum, 0.1f, 0.3f, 'R' }, { t::Width, 0.4f, 0.8f, 'R' } },
              { 0.5f, 0.3f, 1.0f, 0.2f } },
            { "Crackle Rain", 0, { "Drizzle", "Shower", "Downpour", "Squall", "Cloudburst", "Deluge", "Monsoon", "Torrent" },
              { { t::Crackle, 0.35f, 0.7f, 'A' }, { t::Width, 0.7f, 1.0f, 'B' }, { t::Erosion, 0.1f, 0.3f, 'R' }, { t::Hum, 0.0f, 0.1f, 'R' } },
              { 0.6f, 0.4f, 0.8f, 0.4f } },
            { "Room Tone", 1, { "Room", "Studio", "Booth", "Cellar", "Attic", "Garage", "Basement", "Loft" },
              { { t::Erosion, 0.2f, 0.5f, 'A' }, { t::Hum, 0.05f, 0.25f, 'R' }, { t::Crackle, 0.0f, 0.15f, 'R' }, { t::Width, 0.6f, 1.0f, 'B' } },
              { 0.8f, 0.6f, 0.7f, 0.3f } },
            { "Warm Hum", 2, { "Valve", "Tube", "Filament", "Heater", "Cathode", "Anode", "Grid", "Plate" },
              { { t::Hum, 0.12f, 0.35f, 'A' }, { t::HumHz, 49, 51, 'R' }, { t::Crackle, 0.1f, 0.4f, 'B' }, { t::Erosion, 0.0f, 0.2f, 'R' } },
              { 0.5f, 0.4f, 0.9f, 0.3f } },
            { "Wide Dust", 3, { "Horizon", "Plain", "Steppe", "Prairie", "Savanna", "Desert", "Dune", "Mesa" },
              { { t::Width, 0.85f, 1.0f, 'R' }, { t::Crackle, 0.2f, 0.6f, 'A' }, { t::Erosion, 0.2f, 0.5f, 'B' } },
              { 0.8f, 0.4f, 0.8f, 0.3f } },
            { "Narrow Crackle", 0, { "Needle", "Stylus", "Cartridge", "Tonearm", "Headshell", "Cantilever", "Groove", "Runout" },
              { { t::Width, 0.1f, 0.4f, 'R' }, { t::Crackle, 0.26f, 0.7f, 'A' }, { t::Erosion, 0.0f, 0.3f, 'B' } },
              { 0.5f, 0.6f, 0.6f, 0.5f } },
            { "Tape Wear", 1, { "Oxide", "Dropout", "Wow", "Flutter", "Print Through", "Stretch", "Crease", "Splice" },
              { { t::Erosion, 0.3f, 0.6f, 'A' }, { t::Hum, 0.05f, 0.2f, 'R' }, { t::Crackle, 0.0f, 0.2f, 'R' }, { t::Width, 0.4f, 0.8f, 'B' } },
              { 0.7f, 0.4f, 0.8f, 0.4f } },
            { "Static Field", 2, { "Static", "Ion", "Spark", "Arc", "Discharge", "Corona", "Plasma", "Lightning" },
              { { t::Crackle, 0.42f, 0.7f, 'A' }, { t::Erosion, 0.26f, 0.6f, 'B' }, { t::Width, 0.6f, 1.0f, 'R' } },
              { 0.3f, 0.5f, 0.3f, 0.9f } },
            { "Ghost Hum", 3, { "Ghost", "Phantom", "Spectre", "Wraith", "Shade", "Spirit", "Apparition", "Revenant" },
              { { t::Hum, 0.09f, 0.35f, 'A' }, { t::HumHz, 49, 51, 'R' }, { t::Erosion, 0.3f, 0.6f, 'B' }, { t::Crackle, 0.0f, 0.2f, 'R' } },
              { 0.9f, 0.4f, 0.6f, 0.3f } },
            { "Factory Floor", 0, { "Conveyor", "Lathe", "Press", "Loom", "Kiln", "Smelter", "Foundry", "Mill" },
              { { t::Hum, 0.15f, 0.35f, 'A' }, { t::Erosion, 0.26f, 0.6f, 'B' }, { t::Crackle, 0.1f, 0.4f, 'R' }, { t::Width, 0.5f, 0.9f, 'R' } },
              { 0.4f, 0.7f, 0.2f, 0.8f } },
            { "Midnight Air", 1, { "Midnight", "Small Hours", "Witching Hour", "Night Shift", "Graveyard Shift", "Vigil", "Watch", "Lookout" },
              { { t::Erosion, 0.3f, 0.6f, 'A' }, { t::Crackle, 0.05f, 0.25f, 'R' }, { t::Hum, 0.0f, 0.15f, 'R' }, { t::Width, 0.7f, 1.0f, 'B' } },
              { 1.0f, 0.5f, 0.6f, 0.3f } },
            { "Dirty Groove", 2, { "Groove", "Track", "Rut", "Furrow", "Trench", "Channel", "Gutter", "Ditch" },
              { { t::Crackle, 0.39f, 0.7f, 'A' }, { t::Erosion, 0.3f, 0.6f, 'B' }, { t::Hum, 0.1f, 0.3f, 'R' } },
              { 0.4f, 0.3f, 0.9f, 0.5f } },
            { "Clean Air", 3, { "Clearing", "Meadow", "Glade", "Dell", "Vale", "Heath", "Moor", "Fell" },
              { { t::Crackle, 0.0f, 0.2f, 'A' }, { t::Erosion, 0.05f, 0.25f, 'B' }, { t::Hum, 0.0f, 0.05f, 'R' }, { t::Width, 0.6f, 0.9f, 'R' } },
              { 0.8f, 0.8f, 0.5f, 0.5f } },
        } };
    return e;
}

const Engine* engineOf(Module m)
{
    switch (m) {
    case Module::Kick: return &kickEngine();
    case Module::Rumble: return &rumbleEngine();
    case Module::Sub: return &subEngine();
    case Module::Perc: return &percEngine();
    case Module::Ping: return &pingEngine();
    case Module::Bass: return &bassEngine();
    case Module::Acid: return &acidEngine();
    case Module::Chord: return &chordEngine();
    case Module::Drone: return &droneEngine();
    case Module::Texture: return &textureEngine();
    default: return nullptr;
    }
}

std::vector<SoundPreset> build(Module m, const Engine& e)
{
    ParamStore store;
    std::vector<SoundPreset> out;
    out.reserve(e.groups.size() * 64);
    for (size_t g = 0; g < e.groups.size(); ++g) {
        const Group& grp = e.groups[g];
        for (int v = 0; v < 64; ++v) {
            const int a = v / 8, n = v % 8;
            SoundPreset p;
            p.group = grp.name;
            p.name = std::string(e.adjectives[grp.adjectives][a]) + " " + grp.nouns[n];
            for (int s = 0; s < 4; ++s) p.styles[s] = grp.styles[s];
            p.roles = grp.roles;
            Rng rng;
            rng.seed(mixSeed(0x554D42u + 131u * static_cast<uint64_t>(m) + 17u * g, static_cast<uint64_t>(v)));   // "UMB"
            for (const Axis& ax : grp.axes) {
                // Where on the knob's range: the adjective's row or the noun's column (with a little of its own), or a draw.
                float t = ax.axis == 'A' ? a / 7.0f : (ax.axis == 'B' ? n / 7.0f : rng.uniform());
                if (ax.axis != 'R') t = std::clamp(t + 0.12f * (rng.uniform() - 0.5f), 0.0f, 1.0f);
                const ParamDesc& d = store.desc(store.id(m, 0, ax.k));
                float value;
                if (d.curve == Curve::Log && ax.lo > 0.0f && ax.hi > 0.0f) value = ax.lo * std::pow(ax.hi / ax.lo, t);
                else value = ax.lo + t * (ax.hi - ax.lo);
                if (d.curve == Curve::Choice || d.curve == Curve::Int || d.curve == Curve::Toggle) {
                    // A choice between two values is drawn evenly (a plain round would favour neither end less).
                    value = ax.lo == ax.hi ? ax.lo : std::floor(std::min(ax.lo, ax.hi) + t * (std::fabs(ax.hi - ax.lo) + 1.0f) * 0.9999f);
                }
                p.values.push_back({ ax.k, std::clamp(value, d.minValue, d.maxValue) });
            }
            out.push_back(std::move(p));
        }
    }
    return out;
}

} // namespace

bool hasPresets(Module module) { return engineOf(module) != nullptr; }

const std::vector<SoundPreset>& factoryPresets(Module module)
{
    static std::mutex lock;
    static std::vector<SoundPreset> built[static_cast<int>(Module::Count)];
    static bool done[static_cast<int>(Module::Count)] = {};
    static const std::vector<SoundPreset> none;
    const Engine* e = engineOf(module);
    if (e == nullptr) return none;
    std::lock_guard<std::mutex> g(lock);
    const int i = static_cast<int>(module);
    if (!done[i]) { built[i] = build(module, *e); done[i] = true; }
    return built[i];
}

bool presetLeaves(Module module, int k)
{
    switch (module) {
    case Module::Kick: return k == kick::Level || k == kick::Tune || k == kick::TailLimit || k == kick::LowCut;
    case Module::Rumble: return k == rumble::Level || k == rumble::Sub || k == rumble::Duck || k == rumble::DuckHold || k == rumble::DuckRelease;
    case Module::Sub: return k == sub::Level || k == sub::Octave || k == sub::Lock || k == sub::Duck || k == sub::DuckHold || k == sub::DuckRelease;
    case Module::Perc:
        return k == perc::Active || k == perc::Role || k == perc::Level || k == perc::Pan || k == perc::Choke || k == perc::Shift
            || k == perc::Density || k == perc::Tune || k == perc::PanDepth || k == perc::PanBars || k == perc::CutTrack;
    case Module::Ping: return k == ping::Level || k == ping::Pan;
    case Module::Bass: case Module::Acid:
        return k == synth::Level || k == synth::Pan || k == synth::LowCut || k == synth::DubSend || k == synth::RoomSend || k == synth::Duck
            || k == synth::DuckRelease;
    case Module::Chord: return k == chord::Level || k == chord::Octave || k == chord::DubSend || k == chord::PlateSend;
    case Module::Drone: return k == drone::Level || k == drone::Octave || k == drone::PlateSend || k == drone::RoomSend;
    case Module::Texture: return k == texture::Level;
    default: return true;
    }
}

std::vector<std::pair<int, float>> presetKnobs(Module module, int instance, const SoundPreset& preset)
{
    static const ParamStore defaults;
    std::vector<std::pair<int, float>> out;
    const int count = ParamStore::moduleCount(module);
    for (int k = 0; k < count; ++k) {
        if (presetLeaves(module, k)) continue;
        float v = defaults.defaultValue(defaults.id(module, instance, k));
        for (const auto& e : preset.values) if (e.first == k) v = e.second;
        out.push_back({ k, v });
    }
    return out;
}

void applyPreset(ParamStore& params, Module module, int instance, const SoundPreset& preset)
{
    for (const auto& e : presetKnobs(module, instance, preset)) params.set(params.id(module, instance, e.first), e.second);
}

int pickPreset(Module module, const float* styleMix, int role, Rng& rng)
{
    const std::vector<SoundPreset>& list = factoryPresets(module);
    if (list.empty()) return -1;
    // A group by its fit to the track's style mix (cubed, so the fitting groups dominate), then one of its presets.
    std::vector<std::pair<size_t, float>> groups;   // first preset of the group, weight
    float total = 0.0f;
    for (size_t i = 0; i < list.size(); i += 64) {
        const SoundPreset& p = list[i];
        if (role >= 0 && p.roles != 0 && (p.roles & (1u << role)) == 0) continue;
        float fit = 0.0f;
        for (int s = 0; s < 4; ++s) fit += p.styles[s] * styleMix[s];
        const float w = fit * fit * fit;
        if (w <= 0.0f) continue;
        groups.push_back({ i, w });
        total += w;
    }
    if (groups.empty()) return static_cast<int>(rng.below(static_cast<int>(list.size())));
    float u = rng.uniform() * total;
    size_t first = groups.back().first;
    for (const auto& g : groups) { if (u < g.second) { first = g.first; break; } u -= g.second; }
    // In the group: the adjective's row (dark to bright) leaning to the middle -- the extremes are there, the composer
    // takes them less often (a triangle: two draws averaged) -- the noun's column even.
    const int row = (rng.below(8) + rng.below(8) + 1) / 2;
    return static_cast<int>(first) + 8 * row + rng.below(8);
}

} // namespace umb
