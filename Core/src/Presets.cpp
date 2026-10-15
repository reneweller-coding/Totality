/**
 * @file Presets.cpp
 * @brief The factory presets: sixteen groups per engine, sixty-four presets per group (Presets.h).
 * @note The builder is Ephemeris' (Core/src/Presets.cpp at d047d79); the groups, their ranges and their styles are Totality's.
 * @author Rene Weller
 */
#include "tot/Presets.h"
#include <algorithm>
#include <cmath>
#include <mutex>

namespace tot {

namespace {

/** @brief A knob's range in a group and the axis that moves it: 'A' the adjective's, 'B' the noun's, 'R' a draw. */
struct Axis {
    int m_k;   ///< the knob (its index in the module)
    float m_lo;   ///< the low end of its range in the group
    float m_hi;   ///< the high end
    char m_axis;   ///< what moves it: 'A' the adjective, 'B' the noun, 'R' a draw
};

/** @brief A group: its name, which of the engine's four adjective rows it uses, its nouns, its knobs, its styles. */
struct Group {
    const char* m_name;   ///< the group's name
    int m_adjectives;   ///< which of the engine's four adjective rows it uses
    const char* m_nouns[8];   ///< its eight nouns
    std::vector<Axis> m_axes;   ///< its knobs
    float m_styles[4];      ///< Hypnotic, Ostgut, Dub, Raw
    uint32_t m_roles = 0;   ///< a kit lane's roles (bit PercRole), 0: any
};

/** @brief An engine: four rows of adjectives, each from dark to bright, and its sixteen groups. */
struct Engine {
    const char* m_adjectives[4][8];   ///< four rows of eight adjectives, each from dark to bright
    std::vector<Group> m_groups;   ///< its sixteen groups
};

/**
 * @brief The bit of role @p r in Group::roles.
 * @param[in] r  the role
 * @return its bit
 */
constexpr uint32_t bit(PercRoleE r) { return 1u << static_cast<int>(r); }

// --- The kick ----------------------------------------------------------------------------------------------------------
/**
 * @brief The kick's names and groups.
 * @return the kick's names and groups
 */
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
              { { k::ENGINE, 2, 2, 'R' }, { k::PITCH_END, 46, 56, 'R' }, { k::PITCH_START, 220, 360, 'R' }, { k::PITCH_DECAY, 14, 24, 'B' },
                { k::AMP_DECAY, 300, 450, 'B' }, { k::DRIVE, 0.25f, 0.5f, 'R' }, { k::TONE, 3000, 9000, 'A' }, { k::CLICK_LEVEL, 0.2f, 0.5f, 'A' },
                { k::CLICK_TONE, 2000, 6000, 'A' }, { k::PUNCH, 0.35f, 0.6f, 'R' }, { k::TOP_LEVEL, -18, -10, 'R' }, { k::TOP_PITCH, -10, -6, 'R' },
                { k::TOP_DECAY, 60, 120, 'B' } },
              { 1.0f, 0.9f, 0.2f, 0.6f } },
            { "Hypnotic Thud", 1, { "Pulse", "Heart", "Engine Room", "Piston", "Loom", "Mill", "Pump", "Drum Hall" },
              { { k::ENGINE, 2, 2, 'R' }, { k::PITCH_END, 44, 50, 'R' }, { k::PITCH_START, 200, 320, 'R' }, { k::PITCH_DECAY, 16, 24, 'B' },
                { k::AMP_DECAY, 380, 450, 'B' }, { k::DRIVE, 0.25f, 0.4f, 'R' }, { k::TONE, 2500, 6000, 'A' }, { k::CLICK_LEVEL, 0.1f, 0.3f, 'A' },
                { k::PUNCH, 0.3f, 0.5f, 'R' }, { k::TOP_LEVEL, -20, -12, 'R' }, { k::DIP, -5, -2, 'R' } },
              { 1.0f, 0.6f, 0.3f, 0.2f } },
            { "Ostgut Punch", 2, { "Concrete", "Girder", "Rivet", "Anvil", "Forge", "Crane", "Foundry", "Steelworks" },
              { { k::ENGINE, 2, 2, 'R' }, { k::PITCH_END, 50, 58, 'R' }, { k::PITCH_START, 260, 420, 'R' }, { k::PITCH_DECAY, 14, 20, 'B' },
                { k::AMP_DECAY, 250, 380, 'B' }, { k::DRIVE, 0.35f, 0.55f, 'R' }, { k::PUNCH, 0.5f, 0.7f, 'R' }, { k::TONE, 4000, 10000, 'A' },
                { k::CLICK_LEVEL, 0.3f, 0.55f, 'A' }, { k::TOP_LEVEL, -14, -8, 'R' }, { k::TOP_DRIVE, 0.5f, 0.8f, 'R' } },
              { 0.5f, 1.0f, 0.1f, 0.5f } },
            { "Warehouse Boom", 3, { "Hangar", "Depot", "Dock", "Arch", "Viaduct", "Gasometer", "Rail Yard", "Freight" },
              { { k::ENGINE, 2, 2, 'R' }, { k::PITCH_END, 42, 50, 'R' }, { k::PITCH_DECAY, 16, 24, 'R' }, { k::AMP_DECAY, 380, 450, 'B' },
                { k::DRIVE, 0.3f, 0.5f, 'R' }, { k::DIP, -6, -2, 'B' }, { k::DIP_FREQ, 300, 800, 'R' }, { k::TONE, 2500, 7000, 'A' },
                { k::CLICK_LEVEL, 0.15f, 0.4f, 'A' } },
              { 0.6f, 0.6f, 0.1f, 0.4f } },
            { "Dub Sine", 0, { "Lagoon", "Estuary", "Harbour", "Reef", "Mooring", "Buoy", "Jetty", "Lighthouse" },
              { { k::ENGINE, 0, 0, 'R' }, { k::PITCH_END, 44, 52, 'R' }, { k::PITCH_START, 150, 260, 'R' }, { k::PITCH_DECAY, 18, 26, 'B' },
                { k::AMP_DECAY, 380, 550, 'B' }, { k::DRIVE, 0.15f, 0.3f, 'R' }, { k::CLICK_LEVEL, 0.05f, 0.25f, 'A' }, { k::TONE, 1500, 4000, 'A' },
                { k::PUNCH, 0.2f, 0.4f, 'R' }, { k::TOP_LEVEL, -30, -18, 'R' } },
              { 0.2f, 0.1f, 1.0f, 0.0f } },
            { "Soft Chamber", 1, { "Alcove", "Parlour", "Chapel", "Crypt", "Cloister", "Rotunda", "Nave", "Apse" },
              { { k::ENGINE, 0, 0, 'R' }, { k::PITCH_END, 44, 50, 'R' }, { k::PITCH_DECAY, 20, 26, 'B' }, { k::AMP_DECAY, 400, 550, 'B' },
                { k::DRIVE, 0.15f, 0.25f, 'R' }, { k::CLICK_LEVEL, 0.0f, 0.15f, 'A' }, { k::TONE, 1200, 3000, 'A' }, { k::TOP_LEVEL, -36, -20, 'R' } },
              { 0.3f, 0.1f, 0.9f, 0.0f } },
            { "Round Sub Kick", 2, { "Boulder", "Monolith", "Menhir", "Dolmen", "Cairn", "Tor", "Barrow", "Henge" },
              { { k::ENGINE, 0, 0, 'R' }, { k::PITCH_END, 40, 48, 'R' }, { k::PITCH_START, 120, 220, 'R' }, { k::PITCH_DECAY, 18, 26, 'R' },
                { k::AMP_DECAY, 400, 550, 'B' }, { k::DRIVE, 0.15f, 0.3f, 'R' }, { k::CLICK_LEVEL, 0.05f, 0.2f, 'A' }, { k::TONE, 1500, 3500, 'A' } },
              { 0.4f, 0.2f, 0.6f, 0.1f } },
            { "Resonant Body", 3, { "Bell Jar", "Kettle", "Cauldron", "Cistern", "Tank", "Barrel", "Drum Shell", "Casket" },
              { { k::ENGINE, 1, 1, 'R' }, { k::PITCH_END, 46, 56, 'R' }, { k::PITCH_DECAY, 14, 20, 'R' }, { k::AMP_DECAY, 300, 450, 'B' },
                { k::DRIVE, 0.25f, 0.45f, 'R' }, { k::TONE, 3000, 7000, 'A' }, { k::CLICK_LEVEL, 0.15f, 0.4f, 'A' }, { k::PUNCH, 0.35f, 0.55f, 'R' } },
              { 0.4f, 0.3f, 0.3f, 0.3f } },
            { "Wooden Knock", 0, { "Plank", "Crate", "Pallet", "Beam", "Rafter", "Joist", "Timber", "Lintel" },
              { { k::ENGINE, 1, 1, 'R' }, { k::PITCH_END, 50, 58, 'R' }, { k::AMP_DECAY, 250, 320, 'B' }, { k::PITCH_DECAY, 12, 16, 'R' },
                { k::CLICK_LEVEL, 0.3f, 0.55f, 'A' }, { k::TONE, 5000, 12000, 'A' }, { k::PUNCH, 0.5f, 0.75f, 'B' }, { k::DRIVE, 0.3f, 0.5f, 'R' } },
              { 0.3f, 0.5f, 0.1f, 0.3f } },
            { "Raw Distorted", 1, { "Grinder", "Saw Mill", "Crusher", "Shredder", "Chainsaw", "Jackhammer", "Drill", "Rivet Gun" },
              { { k::ENGINE, 2, 2, 'R' }, { k::CLIP, 1, 1, 'R' }, { k::DRIVE, 0.55f, 0.85f, 'B' }, { k::PITCH_DECAY, 12, 20, 'R' },
                { k::AMP_DECAY, 250, 380, 'R' }, { k::TONE, 5000, 12000, 'A' }, { k::CLICK_LEVEL, 0.35f, 0.6f, 'A' }, { k::TOP_DRIVE, 0.6f, 0.9f, 'R' },
                { k::TOP_LEVEL, -14, -8, 'R' } },
              { 0.0f, 0.3f, 0.0f, 1.0f } },
            { "Industrial Hammer", 2, { "Stamp", "Press", "Pile Driver", "Ram", "Mallet", "Sledge", "Maul", "Trip Hammer" },
              { { k::ENGINE, 0, 0, 'R' }, { k::CLIP, 1, 1, 'R' }, { k::DRIVE, 0.6f, 0.85f, 'R' }, { k::PITCH_START, 400, 900, 'B' },
                { k::PITCH_DECAY, 12, 18, 'R' }, { k::AMP_DECAY, 250, 340, 'R' }, { k::CLICK_LEVEL, 0.4f, 0.65f, 'A' }, { k::TONE, 6000, 14000, 'A' } },
              { 0.0f, 0.2f, 0.0f, 0.9f } },
            { "Peak Stomp", 3, { "Stomp", "Stampede", "March", "Parade", "Charge", "Surge", "Onslaught", "Rally" },
              { { k::ENGINE, 2, 2, 'R' }, { k::PUNCH, 0.6f, 0.85f, 'B' }, { k::DRIVE, 0.5f, 0.75f, 'R' }, { k::AMP_DECAY, 280, 380, 'R' },
                { k::PITCH_DECAY, 12, 18, 'R' }, { k::TOP_LEVEL, -12, -6, 'R' }, { k::TOP_DRIVE, 0.7f, 0.9f, 'R' }, { k::TONE, 4000, 11000, 'A' } },
              { 0.1f, 0.5f, 0.0f, 0.8f } },
            { "Tight Tool", 0, { "Tool", "Lever", "Wrench", "Clamp", "Chisel", "Gauge", "Caliper", "Spanner" },
              { { k::ENGINE, 2, 2, 'R' }, { k::AMP_DECAY, 250, 320, 'B' }, { k::PITCH_DECAY, 14, 18, 'R' }, { k::CLICK_LEVEL, 0.3f, 0.5f, 'A' },
                { k::TONE, 5000, 10000, 'A' }, { k::DRIVE, 0.3f, 0.5f, 'R' }, { k::PUNCH, 0.4f, 0.6f, 'R' } },
              { 0.5f, 0.7f, 0.1f, 0.4f } },
            { "Long Tail", 1, { "Comet Tail", "Wake", "Trail", "Echo Line", "Afterglow", "Drift", "Contrail", "Slipstream" },
              { { k::ENGINE, 1, 1, 'R' }, { k::AMP_DECAY, 400, 450, 'B' }, { k::PITCH_END, 40, 48, 'R' }, { k::PITCH_DECAY, 18, 24, 'R' },
                { k::DRIVE, 0.25f, 0.35f, 'R' }, { k::TONE, 2000, 5000, 'A' }, { k::CLICK_LEVEL, 0.1f, 0.3f, 'A' } },
              { 0.6f, 0.2f, 0.4f, 0.1f } },
            { "Click Top", 2, { "Needle", "Pin", "Stylus", "Tack", "Nail", "Awl", "Spike", "Barb" },
              { { k::ENGINE, 2, 2, 'R' }, { k::CLICK_LEVEL, 0.45f, 0.7f, 'A' }, { k::CLICK_TONE, 3000, 9000, 'A' }, { k::CLICK_DECAY, 3, 10, 'B' },
                { k::TOP_LEVEL, -14, -8, 'R' }, { k::TOP_CUT, 300, 700, 'B' }, { k::PITCH_DECAY, 14, 20, 'R' }, { k::AMP_DECAY, 280, 400, 'R' } },
              { 0.4f, 0.5f, 0.1f, 0.5f } },
            { "Dark Rumble Feeder", 3, { "Undertow", "Riptide", "Current", "Eddy", "Whirlpool", "Maelstrom", "Swell", "Groundswell" },
              { { k::ENGINE, 0, 0, 'R' }, { k::PITCH_END, 42, 48, 'R' }, { k::PITCH_DECAY, 20, 26, 'B' }, { k::AMP_DECAY, 350, 450, 'R' },
                { k::TONE, 1500, 3500, 'A' }, { k::DIP, -8, -3, 'B' }, { k::CLICK_LEVEL, 0.05f, 0.2f, 'A' }, { k::DRIVE, 0.2f, 0.35f, 'R' } },
              { 0.8f, 0.3f, 0.5f, 0.3f } },
        } };
    return e;
}

// --- The rumble ----------------------------------------------------------------------------------------------------------
/**
 * @brief The rumble's names and groups.
 * @return the rumble's names and groups
 */
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
              { { r::DECAY, 1.5f, 2.8f, 'B' }, { r::SIZE, 0.8f, 1.4f, 'R' }, { r::DAMPING, 0.7f, 0.35f, 'A' }, { r::SPLIT, 70, 100, 'A' },
                { r::DRIVE, 4, 8, 'R' }, { r::RATIO, 2.5f, 3.5f, 'R' }, { r::RESONANCE, 0.1f, 0.25f, 'R' } },
              { 1.0f, 0.8f, 0.3f, 0.6f } },
            { "Dark Hall", 1, { "Hall", "Gallery", "Atrium", "Foyer", "Lobby", "Corridor", "Aisle", "Passage" },
              { { r::DECAY, 1.8f, 3.0f, 'B' }, { r::SIZE, 1.0f, 2.0f, 'B' }, { r::DAMPING, 0.8f, 0.5f, 'A' }, { r::SPLIT, 60, 90, 'A' },
                { r::DRIVE, 3, 6, 'R' }, { r::PRE_DELAY, 0, 20, 'R' } },
              { 0.9f, 0.5f, 0.4f, 0.3f } },
            { "Tight Rumble", 2, { "Knot", "Coil", "Spring", "Clench", "Grip", "Vice", "Brace", "Tension" },
              { { r::DECAY, 0.8f, 1.5f, 'B' }, { r::SIZE, 0.4f, 0.8f, 'R' }, { r::DAMPING, 0.6f, 0.3f, 'A' }, { r::SPLIT, 80, 120, 'A' },
                { r::DRIVE, 5, 9, 'R' }, { r::RATIO, 3.0f, 4.5f, 'R' } },
              { 0.5f, 0.9f, 0.1f, 0.7f } },
            { "Sub Swell", 3, { "Tide", "Flood", "Surge", "Wave", "Breaker", "Roller", "Crest", "Billow" },
              { { r::DECAY, 1.6f, 2.8f, 'R' }, { r::SUB_ATTACK, 40, 150, 'B' }, { r::SUB_RELEASE, 400, 1200, 'B' }, { r::DAMPING, 0.7f, 0.4f, 'A' },
                { r::SPLIT, 60, 90, 'A' }, { r::DRIVE, 3, 6, 'R' } },
              { 0.8f, 0.4f, 0.6f, 0.2f } },
            { "Distorted Roll", 0, { "Static", "Crackle", "Sizzle", "Fizz", "Buzz", "Rasp", "Snarl", "Growl" },
              { { r::DRIVE, 8, 12, 'B' }, { r::DECAY, 1.4f, 2.4f, 'R' }, { r::DAMPING, 0.6f, 0.3f, 'A' }, { r::SPLIT, 80, 120, 'A' },
                { r::RESONANCE, 0.15f, 0.35f, 'R' }, { r::RATIO, 3.0f, 4.5f, 'R' } },
              { 0.2f, 0.4f, 0.0f, 1.0f } },
            { "Long Cave", 1, { "Cave", "Grotto", "Cavern", "Hollow", "Pit", "Shaft", "Mine", "Quarry" },
              { { r::DECAY, 2.2f, 3.0f, 'B' }, { r::SIZE, 1.4f, 2.4f, 'R' }, { r::DAMPING, 0.85f, 0.55f, 'A' }, { r::SPLIT, 55, 80, 'A' },
                { r::DRIVE, 3, 6, 'R' }, { r::PRE_DELAY, 5, 30, 'R' } },
              { 0.8f, 0.2f, 0.6f, 0.1f } },
            { "Short Plate", 2, { "Plate", "Sheet", "Panel", "Tile", "Slab", "Board", "Leaf", "Foil" },
              { { r::DECAY, 0.8f, 1.4f, 'B' }, { r::SIZE, 0.3f, 0.6f, 'R' }, { r::DAMPING, 0.5f, 0.2f, 'A' }, { r::SPLIT, 90, 130, 'A' },
                { r::DRIVE, 4, 8, 'R' } },
              { 0.3f, 0.8f, 0.1f, 0.6f } },
            { "Wide Chamber", 3, { "Chamber", "Vault", "Dome", "Cupola", "Arcade", "Basilica", "Hangar", "Stadium" },
              { { r::DECAY, 1.6f, 2.6f, 'R' }, { r::SIZE, 1.2f, 2.2f, 'B' }, { r::DAMPING, 0.7f, 0.4f, 'A' }, { r::SPLIT, 65, 95, 'A' },
                { r::DRIVE, 4, 7, 'R' }, { r::RATIO, 2.0f, 3.0f, 'R' } },
              { 0.7f, 0.6f, 0.4f, 0.3f } },
            { "Soft Undertow", 0, { "Murmur", "Hum", "Drone", "Purr", "Whir", "Thrum", "Breath", "Sigh" },
              { { r::DRIVE, 2, 4, 'R' }, { r::DECAY, 1.4f, 2.2f, 'B' }, { r::DAMPING, 0.85f, 0.6f, 'A' }, { r::SPLIT, 60, 85, 'A' },
                { r::RESONANCE, 0.05f, 0.15f, 'R' } },
              { 0.6f, 0.3f, 0.9f, 0.1f } },
            { "Resonant Boom", 1, { "Gong", "Timpani", "Kettle Drum", "Tabla", "Taiko", "Bodhran", "Dhol", "Surdo" },
              { { r::RESONANCE, 0.3f, 0.55f, 'B' }, { r::RATIO, 2.0f, 4.0f, 'R' }, { r::DECAY, 1.4f, 2.4f, 'R' }, { r::DAMPING, 0.7f, 0.4f, 'A' },
                { r::SPLIT, 70, 100, 'A' }, { r::DRIVE, 4, 8, 'R' } },
              { 0.6f, 0.5f, 0.2f, 0.5f } },
            { "Gritty Floor", 2, { "Gravel", "Grit", "Sand", "Shale", "Scree", "Rubble", "Cinder", "Clinker" },
              { { r::DRIVE, 6, 10, 'B' }, { r::DECAY, 1.2f, 2.0f, 'R' }, { r::DAMPING, 0.6f, 0.3f, 'A' }, { r::SPLIT, 85, 125, 'A' },
                { r::RATIO, 3.5f, 5.0f, 'R' } },
              { 0.3f, 0.6f, 0.0f, 0.9f } },
            { "Cathedral Low", 3, { "Cathedral", "Minster", "Abbey", "Priory", "Sanctum", "Chancel", "Transept", "Belfry" },
              { { r::DECAY, 2.4f, 3.0f, 'B' }, { r::SIZE, 1.8f, 3.0f, 'R' }, { r::DAMPING, 0.8f, 0.5f, 'A' }, { r::SPLIT, 55, 75, 'A' },
                { r::PRE_DELAY, 10, 40, 'R' }, { r::DRIVE, 3, 5, 'R' } },
              { 0.7f, 0.2f, 0.5f, 0.1f } },
            { "Pulse Rumble", 0, { "Pulse", "Throb", "Beat", "Thump", "Pound", "Knock", "Tap", "Tick" },
              { { r::DECAY, 1.0f, 1.8f, 'B' }, { r::SIZE, 0.6f, 1.0f, 'R' }, { r::DAMPING, 0.6f, 0.35f, 'A' }, { r::SPLIT, 75, 110, 'A' },
                { r::DRIVE, 5, 8, 'R' }, { r::RATIO, 2.8f, 3.8f, 'R' } },
              { 0.9f, 0.7f, 0.2f, 0.5f } },
            { "Deep Tunnel", 1, { "Tunnel", "Tube", "Sewer", "Culvert", "Conduit", "Pipeline", "Aqueduct", "Catacomb" },
              { { r::DECAY, 1.8f, 2.8f, 'B' }, { r::SIZE, 0.9f, 1.6f, 'R' }, { r::DAMPING, 0.9f, 0.6f, 'A' }, { r::SPLIT, 50, 75, 'A' },
                { r::DRIVE, 4, 7, 'R' } },
              { 0.9f, 0.4f, 0.5f, 0.3f } },
            { "Dub Floor", 2, { "Floorboard", "Parquet", "Terrace", "Veranda", "Patio", "Deck", "Porch", "Balcony" },
              { { r::DECAY, 1.2f, 2.2f, 'B' }, { r::DRIVE, 3, 6, 'R' }, { r::DAMPING, 0.8f, 0.55f, 'A' }, { r::SPLIT, 60, 85, 'A' },
                { r::SUB_RELEASE, 300, 800, 'R' } },
              { 0.3f, 0.2f, 1.0f, 0.0f } },
            { "Raw Grind", 3, { "Millstone", "Quern", "Crank", "Gearbox", "Sprocket", "Cog", "Ratchet", "Flywheel" },
              { { r::DRIVE, 8, 12, 'B' }, { r::DECAY, 1.4f, 2.4f, 'R' }, { r::RESONANCE, 0.2f, 0.4f, 'R' }, { r::DAMPING, 0.55f, 0.3f, 'A' },
                { r::SPLIT, 90, 130, 'A' } },
              { 0.1f, 0.3f, 0.0f, 1.0f } },
        } };
    return e;
}

// --- The sub bass ----------------------------------------------------------------------------------------------------------
/**
 * @brief The sub bass's names and groups.
 * @return the sub bass's names and groups
 */
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
              { { s::LOW_PASS, 80, 160, 'A' }, { s::DRIVE, 0.0f, 0.1f, 'R' }, { s::ATTACK, 2, 8, 'R' }, { s::DECAY, 200, 600, 'B' },
                { s::SUSTAIN, 0.6f, 0.9f, 'R' }, { s::RELEASE, 60, 200, 'B' } },
              { 0.7f, 0.5f, 1.0f, 0.3f } },
            { "Warm Sine", 1, { "Hearth", "Ember", "Glow", "Coal", "Kiln", "Stove", "Oven", "Furnace" },
              { { s::LOW_PASS, 90, 180, 'A' }, { s::DRIVE, 0.15f, 0.35f, 'B' }, { s::ATTACK, 2, 6, 'R' }, { s::DECAY, 250, 700, 'R' },
                { s::SUSTAIN, 0.6f, 0.85f, 'R' }, { s::RELEASE, 80, 200, 'R' } },
              { 0.7f, 0.6f, 0.9f, 0.4f } },
            { "Pluck Sub", 2, { "Pluck", "Pick", "Twang", "Snap", "Flick", "Pop", "Tap", "Nudge" },
              { { s::DECAY, 80, 250, 'B' }, { s::SUSTAIN, 0.1f, 0.4f, 'R' }, { s::RELEASE, 40, 120, 'R' }, { s::LOW_PASS, 100, 220, 'A' },
                { s::ATTACK, 1, 4, 'R' }, { s::DRIVE, 0.05f, 0.25f, 'R' } },
              { 0.6f, 0.7f, 0.4f, 0.5f } },
            { "Held Sub", 3, { "Anchor", "Keel", "Ballast", "Plinth", "Pedestal", "Foundation", "Bedrock", "Cornerstone" },
              { { s::SUSTAIN, 0.8f, 1.0f, 'R' }, { s::DECAY, 400, 1200, 'B' }, { s::RELEASE, 100, 300, 'B' }, { s::LOW_PASS, 80, 150, 'A' },
                { s::ATTACK, 3, 10, 'R' } },
              { 0.5f, 0.3f, 1.0f, 0.2f } },
            { "Driven Sub", 0, { "Motor", "Dynamo", "Generator", "Turbine", "Rotor", "Engine", "Compressor", "Reactor" },
              { { s::DRIVE, 0.35f, 0.7f, 'B' }, { s::LOW_PASS, 100, 220, 'A' }, { s::DECAY, 200, 600, 'R' }, { s::SUSTAIN, 0.5f, 0.8f, 'R' },
                { s::RELEASE, 60, 160, 'R' } },
              { 0.3f, 0.5f, 0.2f, 1.0f } },
            { "Round Sub", 1, { "Sphere", "Orb", "Globe", "Pearl", "Marble", "Bubble", "Dome", "Ball" },
              { { s::LOW_PASS, 70, 130, 'A' }, { s::DRIVE, 0.05f, 0.15f, 'R' }, { s::DECAY, 300, 800, 'B' }, { s::SUSTAIN, 0.6f, 0.9f, 'R' },
                { s::RELEASE, 80, 250, 'R' }, { s::ATTACK, 3, 12, 'R' } },
              { 0.6f, 0.4f, 0.9f, 0.2f } },
            { "Short Sub", 2, { "Blip", "Dot", "Dash", "Spark", "Flash", "Beat", "Stab", "Jab" },
              { { s::DECAY, 60, 180, 'B' }, { s::SUSTAIN, 0.0f, 0.3f, 'R' }, { s::RELEASE, 30, 90, 'R' }, { s::LOW_PASS, 110, 240, 'A' },
                { s::ATTACK, 1, 3, 'R' } },
              { 0.5f, 0.8f, 0.2f, 0.6f } },
            { "Dub Sub", 3, { "Bassline", "Riddim", "Dubplate", "Sound System", "Stack", "Scoop", "Horn", "Bin" },
              { { s::LOW_PASS, 80, 140, 'A' }, { s::DRIVE, 0.05f, 0.2f, 'R' }, { s::DECAY, 400, 1000, 'B' }, { s::SUSTAIN, 0.7f, 0.95f, 'R' },
                { s::RELEASE, 120, 350, 'B' }, { s::ATTACK, 4, 15, 'R' } },
              { 0.3f, 0.2f, 1.0f, 0.1f } },
            { "Soft Swell", 0, { "Swell", "Rise", "Bloom", "Fade", "Wash", "Surge", "Wax", "Welling" },
              { { s::ATTACK, 15, 50, 'B' }, { s::DECAY, 400, 1200, 'R' }, { s::SUSTAIN, 0.7f, 0.95f, 'R' }, { s::RELEASE, 150, 400, 'R' },
                { s::LOW_PASS, 80, 150, 'A' } },
              { 0.6f, 0.2f, 0.8f, 0.1f } },
            { "Tight Sub", 1, { "Latch", "Lock", "Bolt", "Hinge", "Clasp", "Buckle", "Catch", "Pin" },
              { { s::ATTACK, 1, 3, 'R' }, { s::DECAY, 120, 300, 'B' }, { s::SUSTAIN, 0.4f, 0.7f, 'R' }, { s::RELEASE, 40, 100, 'R' },
                { s::LOW_PASS, 100, 200, 'A' }, { s::DRIVE, 0.05f, 0.2f, 'R' } },
              { 0.8f, 0.8f, 0.4f, 0.5f } },
            { "Dark Sub", 2, { "Shadow", "Umbra", "Eclipse", "Nightfall", "Gloom", "Murk", "Void", "Abyss" },
              { { s::LOW_PASS, 60, 100, 'A' }, { s::DRIVE, 0.0f, 0.1f, 'R' }, { s::DECAY, 300, 900, 'B' }, { s::SUSTAIN, 0.6f, 0.9f, 'R' },
                { s::RELEASE, 100, 300, 'R' } },
              { 0.9f, 0.4f, 0.7f, 0.2f } },
            { "Grit Sub", 3, { "Grain", "Sandpaper", "Emery", "Rasp", "File", "Burr", "Whetstone", "Grindstone" },
              { { s::DRIVE, 0.25f, 0.5f, 'B' }, { s::LOW_PASS, 120, 260, 'A' }, { s::DECAY, 150, 500, 'R' }, { s::SUSTAIN, 0.4f, 0.8f, 'R' },
                { s::RELEASE, 50, 150, 'R' } },
              { 0.3f, 0.6f, 0.1f, 0.9f } },
            { "Hypnotic Sub", 0, { "Trance", "Spell", "Charm", "Mantra", "Chant", "Loop", "Cycle", "Orbit" },
              { { s::LOW_PASS, 90, 160, 'A' }, { s::DECAY, 250, 700, 'B' }, { s::SUSTAIN, 0.6f, 0.85f, 'R' }, { s::RELEASE, 70, 180, 'R' },
                { s::DRIVE, 0.05f, 0.2f, 'R' }, { s::ATTACK, 2, 6, 'R' } },
              { 1.0f, 0.5f, 0.5f, 0.3f } },
            { "Ostgut Sub", 1, { "Pylon", "Mast", "Tower", "Chimney", "Stack", "Spire", "Column", "Pillar" },
              { { s::LOW_PASS, 100, 200, 'A' }, { s::DECAY, 150, 450, 'B' }, { s::SUSTAIN, 0.4f, 0.75f, 'R' }, { s::RELEASE, 50, 130, 'R' },
                { s::DRIVE, 0.1f, 0.3f, 'R' } },
              { 0.5f, 1.0f, 0.2f, 0.5f } },
            { "Long Release Sub", 2, { "Echo", "Linger", "Remnant", "Residue", "Afterimage", "Ripple", "Wake", "Trace" },
              { { s::RELEASE, 250, 700, 'B' }, { s::DECAY, 400, 1200, 'R' }, { s::SUSTAIN, 0.6f, 0.9f, 'R' }, { s::LOW_PASS, 80, 150, 'A' } },
              { 0.5f, 0.2f, 0.9f, 0.1f } },
            { "Raw Sub", 3, { "Iron Bar", "Rebar", "Girder", "Rail", "Pipe", "Cable", "Chain", "Hook" },
              { { s::DRIVE, 0.4f, 0.8f, 'B' }, { s::LOW_PASS, 140, 300, 'A' }, { s::DECAY, 120, 400, 'R' }, { s::SUSTAIN, 0.3f, 0.7f, 'R' },
                { s::RELEASE, 40, 120, 'R' } },
              { 0.1f, 0.4f, 0.0f, 1.0f } },
        } };
    return e;
}

// --- A lane of the kit ----------------------------------------------------------------------------------------------------------
/**
 * @brief A kit lane's names and groups.
 * @return a kit lane's names and groups
 */
const Engine& percEngine()
{
    namespace p = perc;
    using RT = PercRoleE;
    static const Engine e{
        { { "Dull", "Soft", "Muted", "Dry", "Crisp", "Bright", "Sizzling", "Glittering" },
          { "Wooden", "Leathery", "Papery", "Brittle", "Glassy", "Steely", "Tinny", "Icy" },
          { "Dark", "Shadowy", "Dusty", "Grainy", "Clean", "Shiny", "Sparkling", "Blinding" },
          { "Distant", "Hushed", "Close", "Tight", "Snappy", "Sharp", "Piercing", "Searing" } },
        {
            { "909 Closed Hat", 0, { "Tick", "Chip", "Sliver", "Shard", "Splinter", "Flake", "Fleck", "Speck" },
              { { p::ENGINE, 0, 0, 'R' }, { p::NOISE_TYPE, 1, 1, 'R' }, { p::NOISE, 1, 1, 'R' }, { p::NOISE_DECAY, 35, 90, 'B' },
                { p::DECAY, 35, 90, 'B' }, { p::FILTER, 2, 2, 'R' }, { p::CUTOFF, 5000, 9000, 'A' }, { p::LOW_CUT, 2500, 5000, 'A' },
                { p::RESONANCE, 0.05f, 0.3f, 'R' }, { p::METAL_SCALE, 0.85f, 1.2f, 'R' }, { p::DRIVE, 0.0f, 0.3f, 'R' } },
              { 1.0f, 1.0f, 0.6f, 1.0f }, bit(RT::CLOSED_HAT) | bit(RT::ROLLING_HAT) },
            { "White Tick Hat", 1, { "Grain", "Seed", "Crumb", "Mote", "Dust", "Pollen", "Spore", "Ash" },
              { { p::ENGINE, 0, 0, 'R' }, { p::NOISE_TYPE, 0, 0, 'R' }, { p::NOISE, 1, 1, 'R' }, { p::NOISE_DECAY, 20, 60, 'B' },
                { p::DECAY, 20, 60, 'B' }, { p::FILTER, 1, 2, 'R' }, { p::CUTOFF, 7000, 12000, 'A' }, { p::LOW_CUT, 3000, 6000, 'A' },
                { p::RESONANCE, 0.1f, 0.35f, 'R' } },
              { 0.8f, 0.7f, 0.8f, 0.6f }, bit(RT::CLOSED_HAT) | bit(RT::ROLLING_HAT) },
            { "Metal Hat", 2, { "Rivet", "Washer", "Nut", "Screw", "Staple", "Tack", "Clip", "Link" },
              { { p::ENGINE, 1, 1, 'R' }, { p::METAL_SCALE, 0.8f, 1.6f, 'A' }, { p::NOISE, 0.2f, 0.6f, 'R' }, { p::NOISE_DECAY, 30, 120, 'B' },
                { p::DECAY, 30, 120, 'B' }, { p::FILTER, 2, 2, 'R' }, { p::CUTOFF, 6000, 10000, 'A' }, { p::LOW_CUT, 3000, 5000, 'R' } },
              { 0.7f, 0.8f, 0.3f, 0.9f }, bit(RT::CLOSED_HAT) | bit(RT::ROLLING_HAT) | bit(RT::OPEN_HAT) },
            { "Rolling Shaker Hat", 3, { "Rattle", "Tremolo", "Flutter", "Shiver", "Quiver", "Jitter", "Twitch", "Tremor" },
              { { p::ENGINE, 0, 0, 'R' }, { p::NOISE_TYPE, 0, 1, 'R' }, { p::NOISE, 1, 1, 'R' }, { p::NOISE_DECAY, 25, 60, 'B' },
                { p::DECAY, 25, 60, 'B' }, { p::FILTER, 1, 1, 'R' }, { p::CUTOFF, 6000, 11000, 'A' }, { p::LOW_CUT, 3000, 6000, 'R' },
                { p::BURSTS, 1, 2, 'R' }, { p::BURST_SPACING, 6, 14, 'R' } },
              { 1.0f, 0.7f, 0.5f, 0.6f }, bit(RT::ROLLING_HAT) | bit(RT::SHAKER) },
            { "Open Hat Wash", 0, { "Wash", "Spray", "Mist", "Spume", "Foam", "Surf", "Froth", "Drizzle" },
              { { p::ENGINE, 0, 0, 'R' }, { p::NOISE_TYPE, 1, 1, 'R' }, { p::NOISE, 1, 1, 'R' }, { p::NOISE_DECAY, 200, 500, 'B' },
                { p::DECAY, 200, 500, 'B' }, { p::FILTER, 2, 2, 'R' }, { p::CUTOFF, 5500, 8500, 'A' }, { p::LOW_CUT, 2500, 4000, 'A' },
                { p::METAL_SCALE, 0.85f, 1.1f, 'R' } },
              { 1.0f, 1.0f, 0.7f, 0.8f }, bit(RT::OPEN_HAT) },
            { "Metal Open Hat", 1, { "Cymbal", "Gong", "Chime", "Plate", "Disc", "Shield", "Lid", "Pan" },
              { { p::ENGINE, 1, 1, 'R' }, { p::METAL_SCALE, 0.9f, 1.4f, 'A' }, { p::NOISE, 0.3f, 0.7f, 'R' }, { p::NOISE_DECAY, 250, 600, 'B' },
                { p::DECAY, 250, 600, 'B' }, { p::FILTER, 2, 2, 'R' }, { p::CUTOFF, 5000, 8000, 'A' } },
              { 0.8f, 0.9f, 0.5f, 0.8f }, bit(RT::OPEN_HAT) },
            { "Ride Bell", 2, { "Bell", "Carillon", "Campana", "Tocsin", "Knell", "Peal", "Toll", "Clang" },
              { { p::ENGINE, 1, 1, 'R' }, { p::METAL_SCALE, 1.8f, 2.8f, 'A' }, { p::NOISE, 0.3f, 0.6f, 'R' }, { p::NOISE_DECAY, 500, 1200, 'B' },
                { p::DECAY, 500, 1200, 'B' }, { p::FILTER, 2, 2, 'R' }, { p::CUTOFF, 3000, 6000, 'A' }, { p::LOW_CUT, 800, 1500, 'R' } },
              { 0.6f, 1.0f, 0.4f, 0.6f }, bit(RT::RIDE) },
            { "Dry Ride", 3, { "Ride", "Canter", "Trot", "Gallop", "Stride", "Pace", "Glide", "Cruise" },
              { { p::ENGINE, 1, 1, 'R' }, { p::METAL_SCALE, 1.6f, 2.4f, 'R' }, { p::NOISE, 0.5f, 0.8f, 'R' }, { p::NOISE_DECAY, 300, 700, 'B' },
                { p::DECAY, 300, 700, 'B' }, { p::FILTER, 2, 2, 'R' }, { p::CUTOFF, 3500, 7000, 'A' } },
              { 0.6f, 1.0f, 0.5f, 0.5f }, bit(RT::RIDE) },
            { "909 Clap", 0, { "Clap", "Slap", "Smack", "Whack", "Crack", "Snap", "Spank", "Swat" },
              { { p::ENGINE, 0, 0, 'R' }, { p::NOISE_TYPE, 0, 0, 'R' }, { p::NOISE, 1, 1, 'R' }, { p::BURSTS, 3, 5, 'R' }, { p::BURST_SPACING, 8, 14, 'R' },
                { p::NOISE_DECAY, 120, 260, 'B' }, { p::DECAY, 120, 260, 'B' }, { p::FILTER, 1, 1, 'R' }, { p::CUTOFF, 900, 2000, 'A' },
                { p::RESONANCE, 0.2f, 0.45f, 'R' } },
              { 0.8f, 1.0f, 0.6f, 0.9f }, bit(RT::CLAP) | bit(RT::CLAP_GHOST) },
            { "Room Clap", 1, { "Applause", "Ovation", "Crowd", "Audience", "Throng", "Gathering", "Assembly", "Congregation" },
              { { p::ENGINE, 0, 0, 'R' }, { p::NOISE_TYPE, 0, 0, 'R' }, { p::NOISE, 1, 1, 'R' }, { p::BURSTS, 4, 6, 'R' }, { p::BURST_SPACING, 10, 18, 'R' },
                { p::NOISE_DECAY, 200, 400, 'B' }, { p::DECAY, 200, 400, 'B' }, { p::FILTER, 1, 1, 'R' }, { p::CUTOFF, 1000, 2500, 'A' } },
              { 0.6f, 0.8f, 0.8f, 0.6f }, bit(RT::CLAP) | bit(RT::CLAP_GHOST) },
            { "Rimshot", 2, { "Rim", "Edge", "Brim", "Lip", "Hoop", "Border", "Margin", "Verge" },
              { { p::ENGINE, 2, 2, 'R' }, { p::MODE_SET, 1, 1, 'R' }, { p::PITCH, 800, 1800, 'A' }, { p::DECAY, 20, 60, 'B' }, { p::NOISE_DECAY, 10, 30, 'R' },
                { p::NOISE, 0.1f, 0.3f, 'R' }, { p::MODE_DAMP, 0.3f, 0.7f, 'R' }, { p::FILTER, 2, 2, 'R' }, { p::CUTOFF, 400, 1200, 'R' } },
              { 0.9f, 0.8f, 0.6f, 0.6f }, bit(RT::RIM) },
            { "Wood Block", 3, { "Block", "Clave", "Log", "Stick", "Peg", "Dowel", "Spindle", "Baton" },
              { { p::ENGINE, 2, 2, 'R' }, { p::MODE_SET, 1, 1, 'R' }, { p::PITCH, 500, 1500, 'A' }, { p::DECAY, 30, 90, 'B' }, { p::NOISE, 0.0f, 0.15f, 'R' },
                { p::MODE_DAMP, 0.4f, 0.8f, 'R' }, { p::FILTER, 2, 2, 'R' }, { p::CUTOFF, 300, 900, 'R' } },
              { 0.8f, 0.5f, 0.7f, 0.4f }, bit(RT::RIM) | bit(RT::CONGA) },
            { "Shaker", 0, { "Maraca", "Cabasa", "Rainstick", "Egg", "Tambourine", "Ganza", "Caxixi", "Shekere" },
              { { p::ENGINE, 0, 0, 'R' }, { p::NOISE_TYPE, 0, 0, 'R' }, { p::NOISE, 1, 1, 'R' }, { p::NOISE_DECAY, 30, 80, 'B' }, { p::DECAY, 30, 80, 'B' },
                { p::FILTER, 1, 1, 'R' }, { p::CUTOFF, 5000, 10000, 'A' }, { p::BURSTS, 1, 2, 'R' }, { p::BURST_SPACING, 15, 30, 'R' } },
              { 1.0f, 0.6f, 0.6f, 0.5f }, bit(RT::SHAKER) },
            { "Tom Membrane", 1, { "Tom", "Floor Tom", "Rack Tom", "Kettle", "Hand Drum", "Frame Drum", "Tabor", "Snare Drum" },
              { { p::ENGINE, 2, 2, 'R' }, { p::MODE_SET, 0, 0, 'R' }, { p::PITCH, 80, 220, 'A' }, { p::PITCH_AMOUNT, 1.5f, 3.0f, 'R' },
                { p::PITCH_DECAY, 20, 80, 'R' }, { p::DECAY, 150, 400, 'B' }, { p::NOISE, 0.0f, 0.2f, 'R' }, { p::FILTER, 0, 0, 'R' },
                { p::CUTOFF, 1500, 5000, 'R' }, { p::LOW_CUT, 150, 300, 'R' } },
              { 0.8f, 0.6f, 0.4f, 0.9f }, bit(RT::TOM) | bit(RT::CONGA) },
            { "Conga Skin", 2, { "Conga", "Bongo", "Djembe", "Cajon", "Tumba", "Quinto", "Darbuka", "Udu" },
              { { p::ENGINE, 2, 2, 'R' }, { p::MODE_SET, 0, 0, 'R' }, { p::PITCH, 180, 400, 'A' }, { p::PITCH_AMOUNT, 1.2f, 2.0f, 'R' },
                { p::PITCH_DECAY, 10, 40, 'R' }, { p::DECAY, 80, 220, 'B' }, { p::NOISE, 0.0f, 0.15f, 'R' }, { p::FILTER, 0, 0, 'R' },
                { p::CUTOFF, 2000, 6000, 'R' }, { p::LOW_CUT, 150, 300, 'R' } },
              { 0.9f, 0.5f, 0.6f, 0.5f }, bit(RT::CONGA) | bit(RT::TOM) },
            { "Snare Crack", 3, { "Crack", "Rimfire", "Report", "Shot", "Burst", "Blast", "Bang", "Salvo" },
              { { p::ENGINE, 3, 3, 'R' }, { p::PITCH, 180, 260, 'R' }, { p::NOISE, 0.5f, 0.85f, 'R' }, { p::NOISE_TYPE, 0, 0, 'R' },
                { p::NOISE_DECAY, 100, 250, 'B' }, { p::DECAY, 60, 160, 'B' }, { p::FILTER, 1, 1, 'R' }, { p::CUTOFF, 1500, 4000, 'A' },
                { p::DRIVE, 0.1f, 0.5f, 'R' } },
              { 0.4f, 0.7f, 0.4f, 1.0f }, bit(RT::SNARE) | bit(RT::NOISE) },
        } };
    return e;
}

// --- The ping ----------------------------------------------------------------------------------------------------------
/**
 * @brief The ping's names and groups.
 * @return the ping's names and groups
 */
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
              { { g::RATIO, 1.4f, 2.0f, 'R' }, { g::INDEX, 1.0f, 2.5f, 'A' }, { g::INDEX_DECAY, 30, 90, 'B' }, { g::DECAY, 90, 220, 'B' },
                { g::BAND, 800, 2000, 'A' }, { g::BAND_Q, 0.6f, 1.5f, 'R' }, { g::LPG, 0.4f, 0.8f, 'R' }, { g::RESONANCE, 0.2f, 0.5f, 'R' } },
              { 1.0f, 0.6f, 0.6f, 0.5f } },
            { "Metal Ping", 1, { "Anvil", "Tine", "Fork", "Wire", "String", "Rod", "Tube", "Pipe" },
              { { g::RATIO, 2.0f, 3.5f, 'R' }, { g::INDEX, 2.0f, 4.0f, 'A' }, { g::INDEX_DECAY, 40, 120, 'B' }, { g::DECAY, 80, 180, 'B' },
                { g::BAND, 1000, 3000, 'A' }, { g::BAND_Q, 0.8f, 2.0f, 'R' } },
              { 0.5f, 0.5f, 0.2f, 1.0f } },
            { "Wood Ping", 2, { "Marimba", "Xylophone", "Kalimba", "Mbira", "Balafon", "Slit Drum", "Temple Block", "Gamelan" },
              { { g::RATIO, 1.0f, 1.5f, 'R' }, { g::INDEX, 0.5f, 1.5f, 'A' }, { g::INDEX_DECAY, 20, 60, 'B' }, { g::DECAY, 90, 200, 'B' },
                { g::BAND, 500, 1200, 'A' }, { g::LPG, 0.6f, 0.9f, 'R' }, { g::LPG_RELEASE, 80, 200, 'R' } },
              { 0.9f, 0.6f, 0.7f, 0.3f } },
            { "Bell Ping", 3, { "Bell", "Chime", "Gong", "Cowbell", "Handbell", "Singing Bowl", "Glockenspiel", "Celesta" },
              { { g::RATIO, 2.4f, 3.5f, 'R' }, { g::INDEX, 1.5f, 3.0f, 'A' }, { g::INDEX_DECAY, 60, 200, 'B' }, { g::DECAY, 150, 220, 'B' },
                { g::BAND, 1000, 2500, 'A' }, { g::BAND_MIX, 0.3f, 0.6f, 'R' } },
              { 0.6f, 0.5f, 0.5f, 0.6f } },
            { "Dub Drop", 0, { "Drop", "Drip", "Plop", "Splash", "Ripple", "Puddle", "Spring", "Fountain" },
              { { g::RATIO, 1.2f, 1.8f, 'R' }, { g::PITCH_AMOUNT, 1.0f, 2.5f, 'B' }, { g::PITCH_DECAY, 30, 120, 'B' }, { g::DECAY, 120, 220, 'R' },
                { g::BAND, 600, 1400, 'A' }, { g::SWEEP, 0.5f, 1.2f, 'R' } },
              { 0.6f, 0.3f, 1.0f, 0.2f } },
            { "Sonar", 1, { "Sonar", "Radar", "Beacon", "Signal", "Transponder", "Echo Sounder", "Pulse", "Probe" },
              { { g::RATIO, 1.0f, 1.2f, 'R' }, { g::INDEX, 0.3f, 1.2f, 'A' }, { g::DECAY, 150, 220, 'B' }, { g::LPG_RELEASE, 150, 400, 'B' },
                { g::BAND, 500, 1500, 'A' }, { g::RESONANCE, 0.3f, 0.6f, 'R' }, { g::SWEEP, 0.2f, 0.8f, 'R' } },
              { 1.0f, 0.4f, 0.7f, 0.3f } },
            { "Water Drip", 2, { "Rain", "Dew", "Brook", "Rill", "Creek", "Stream", "Rivulet", "Trickle" },
              { { g::PITCH_AMOUNT, 1.5f, 3.0f, 'B' }, { g::PITCH_DECAY, 10, 40, 'B' }, { g::RATIO, 1.4f, 2.2f, 'R' }, { g::DECAY, 90, 160, 'R' },
                { g::BAND, 800, 2000, 'A' } },
              { 0.8f, 0.4f, 0.8f, 0.3f } },
            { "Hollow Ping", 3, { "Flute", "Ocarina", "Whistle", "Pipe Organ", "Recorder", "Panpipe", "Reed", "Horn" },
              { { g::RATIO, 0.5f, 1.0f, 'R' }, { g::INDEX, 0.5f, 1.5f, 'A' }, { g::BAND_Q, 1.5f, 4.0f, 'B' }, { g::BAND_MIX, 0.5f, 0.9f, 'R' },
                { g::BAND, 600, 1500, 'A' }, { g::DECAY, 100, 220, 'R' } },
              { 0.9f, 0.5f, 0.6f, 0.3f } },
            { "Bright Blip", 0, { "Blip", "Bleep", "Chirp", "Tweet", "Peep", "Beep", "Ping", "Pip" },
              { { g::RATIO, 1.5f, 3.0f, 'R' }, { g::INDEX, 1.5f, 3.5f, 'A' }, { g::INDEX_DECAY, 15, 50, 'B' }, { g::DECAY, 60, 120, 'B' },
                { g::BAND, 1500, 4000, 'A' }, { g::LPG, 0.3f, 0.6f, 'R' } },
              { 0.7f, 0.7f, 0.3f, 0.8f } },
            { "Dark Blip", 1, { "Mole", "Beetle", "Cricket", "Moth", "Bat", "Owl", "Raven", "Crow" },
              { { g::RATIO, 1.0f, 2.0f, 'R' }, { g::INDEX, 0.5f, 2.0f, 'A' }, { g::INDEX_DECAY, 20, 60, 'B' }, { g::DECAY, 80, 160, 'B' },
                { g::BAND, 400, 900, 'A' }, { g::LPG, 0.6f, 0.9f, 'R' } },
              { 1.0f, 0.6f, 0.6f, 0.5f } },
            { "FM Bell", 2, { "Clock", "Carillon", "Tower Bell", "Sleigh Bell", "Ship Bell", "Doorbell", "Bicycle Bell", "School Bell" },
              { { g::RATIO, 2.8f, 3.5f, 'R' }, { g::INDEX, 2.5f, 5.0f, 'A' }, { g::INDEX_DECAY, 80, 250, 'B' }, { g::DECAY, 150, 220, 'R' },
                { g::BAND, 1500, 3500, 'A' } },
              { 0.4f, 0.4f, 0.4f, 0.8f } },
            { "Soft Pluck", 3, { "Harp", "Lute", "Zither", "Koto", "Sitar", "Dulcimer", "Lyre", "Oud" },
              { { g::RATIO, 1.0f, 2.0f, 'R' }, { g::INDEX, 0.5f, 1.5f, 'A' }, { g::INDEX_DECAY, 30, 100, 'R' }, { g::DECAY, 120, 220, 'B' },
                { g::LPG, 0.5f, 0.8f, 'R' }, { g::LPG_RELEASE, 100, 300, 'B' }, { g::BAND, 600, 1400, 'A' } },
              { 0.8f, 0.5f, 0.8f, 0.2f } },
            { "Resonant Knock", 0, { "Knock", "Rap", "Tap", "Thud", "Bump", "Thump", "Clunk", "Clonk" },
              { { g::RESONANCE, 0.5f, 0.8f, 'B' }, { g::BAND_Q, 2.0f, 5.0f, 'B' }, { g::BAND, 500, 1500, 'A' }, { g::RATIO, 1.0f, 2.0f, 'R' },
                { g::DECAY, 80, 160, 'R' } },
              { 0.8f, 0.7f, 0.4f, 0.6f } },
            { "Sweeping Ping", 1, { "Pendulum", "Swing", "Sway", "Arc", "Orbit", "Spiral", "Vortex", "Carousel" },
              { { g::SWEEP, 0.8f, 1.8f, 'B' }, { g::SWEEP_RATE, 0.02f, 0.2f, 'B' }, { g::BAND, 600, 1600, 'A' }, { g::RATIO, 1.2f, 2.2f, 'R' },
                { g::DECAY, 100, 200, 'R' }, { g::BAND_Q, 0.8f, 2.0f, 'R' } },
              { 1.0f, 0.4f, 0.6f, 0.4f } },
            { "Tight Tick", 2, { "Tick", "Click", "Tock", "Clack", "Chink", "Clink", "Plink", "Tink" },
              { { g::DECAY, 30, 80, 'B' }, { g::INDEX_DECAY, 5, 30, 'B' }, { g::INDEX, 1.5f, 3.5f, 'A' }, { g::RATIO, 1.5f, 3.0f, 'R' },
                { g::BAND, 1200, 3500, 'A' } },
              { 0.7f, 0.8f, 0.3f, 0.7f } },
            { "Long Drop", 3, { "Well", "Cistern", "Reservoir", "Basin", "Pond", "Lake", "Fjord", "Ocean" },
              { { g::DECAY, 180, 220, 'R' }, { g::LPG_RELEASE, 300, 800, 'B' }, { g::PITCH_AMOUNT, 0.5f, 1.5f, 'R' }, { g::PITCH_DECAY, 60, 200, 'B' },
                { g::BAND, 500, 1300, 'A' }, { g::RATIO, 1.2f, 1.8f, 'R' } },
              { 0.8f, 0.3f, 0.9f, 0.2f } },
        } };
    return e;
}

// --- The bass synth ----------------------------------------------------------------------------------------------------------
/**
 * @brief The bass synth's names and groups.
 * @return the bass synth's names and groups
 */
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
              { { s::FILTER, 0, 0, 'R' }, { s::WAVE, 0.0f, 0.2f, 'R' }, { s::CUTOFF, 180, 700, 'A' }, { s::RESONANCE, 0.1f, 0.35f, 'B' },
                { s::ENV_AMOUNT, 1.5f, 3.5f, 'R' }, { s::DECAY, 120, 400, 'B' }, { s::SUB_OSC, 0.2f, 0.5f, 'R' }, { s::DRIVE, 0.1f, 0.35f, 'R' } },
              { 1.0f, 0.8f, 0.4f, 0.6f } },
            { "Deep Sub Saw", 1, { "Trench", "Rift", "Chasm", "Canyon", "Gorge", "Ravine", "Gully", "Gulf" },
              { { s::FILTER, 0, 0, 'R' }, { s::WAVE, 0.0f, 0.1f, 'R' }, { s::CUTOFF, 120, 400, 'A' }, { s::RESONANCE, 0.05f, 0.2f, 'R' },
                { s::ENV_AMOUNT, 0.8f, 2.0f, 'B' }, { s::DECAY, 200, 600, 'B' }, { s::SUB_OSC, 0.4f, 0.7f, 'R' } },
              { 0.9f, 0.6f, 0.7f, 0.4f } },
            { "Pulse Bass", 2, { "Pulse", "Square", "Block", "Brick", "Cube", "Tile", "Crate", "Box" },
              { { s::FILTER, 2, 2, 'R' }, { s::WAVE, 0.8f, 1.0f, 'R' }, { s::PULSE_WIDTH, 0.2f, 0.5f, 'B' }, { s::CUTOFF, 250, 900, 'A' },
                { s::RESONANCE, 0.1f, 0.3f, 'R' }, { s::ENV_AMOUNT, 1.0f, 2.5f, 'R' }, { s::DECAY, 100, 300, 'R' } },
              { 0.7f, 0.8f, 0.4f, 0.5f } },
            { "Dub Bass", 3, { "Dubwise", "Skank", "Version", "Steppa", "Rockers", "Roots", "Toast", "Echo Chamber" },
              { { s::FILTER, 3, 3, 'R' }, { s::WAVE, 0.0f, 0.3f, 'R' }, { s::CUTOFF, 150, 450, 'A' }, { s::RESONANCE, 0.05f, 0.25f, 'R' },
                { s::ENV_AMOUNT, 0.5f, 1.5f, 'B' }, { s::AMP_SUSTAIN, 0.6f, 0.9f, 'R' }, { s::AMP_RELEASE, 150, 400, 'B' }, { s::SUB_OSC, 0.3f, 0.6f, 'R' } },
              { 0.3f, 0.2f, 1.0f, 0.1f } },
            { "Plucked Bass", 0, { "Pluck", "Flick", "Twang", "Strum", "Pick", "Plectrum", "Snap", "Fret" },
              { { s::FILTER, 0, 2, 'R' }, { s::WAVE, 0.0f, 0.5f, 'R' }, { s::CUTOFF, 200, 800, 'A' }, { s::ENV_AMOUNT, 2.0f, 4.0f, 'R' },
                { s::DECAY, 60, 200, 'B' }, { s::AMP_DECAY, 80, 250, 'B' }, { s::AMP_SUSTAIN, 0.1f, 0.4f, 'R' } },
              { 0.8f, 0.9f, 0.3f, 0.6f } },
            { "Warm Juno", 1, { "Hearth", "Lantern", "Candle", "Ember", "Glow", "Torch", "Beacon", "Flare" },
              { { s::FILTER, 2, 2, 'R' }, { s::WAVE, 0.0f, 0.6f, 'R' }, { s::CUTOFF, 300, 1000, 'A' }, { s::RESONANCE, 0.1f, 0.3f, 'B' },
                { s::ENV_AMOUNT, 1.0f, 2.5f, 'R' }, { s::DECAY, 150, 400, 'R' }, { s::SUB_OSC, 0.2f, 0.5f, 'R' } },
              { 0.7f, 0.7f, 0.6f, 0.3f } },
            { "Prophet Growl", 2, { "Growl", "Snarl", "Roar", "Rumble", "Grumble", "Howl", "Bellow", "Bark" },
              { { s::FILTER, 1, 1, 'R' }, { s::WAVE, 0.0f, 0.3f, 'R' }, { s::CUTOFF, 200, 800, 'A' }, { s::RESONANCE, 0.2f, 0.5f, 'B' },
                { s::ENV_AMOUNT, 1.5f, 3.5f, 'R' }, { s::DRIVE, 0.3f, 0.6f, 'R' }, { s::DECAY, 150, 400, 'R' } },
              { 0.5f, 0.7f, 0.2f, 0.8f } },
            { "SEM Round", 3, { "Round", "Circle", "Loop", "Ring", "Halo", "Hoop", "Wreath", "Band" },
              { { s::FILTER, 3, 3, 'R' }, { s::WAVE, 0.0f, 0.4f, 'R' }, { s::CUTOFF, 200, 700, 'A' }, { s::RESONANCE, 0.1f, 0.3f, 'B' },
                { s::ENV_AMOUNT, 1.0f, 2.5f, 'R' }, { s::DECAY, 150, 450, 'R' } },
              { 0.8f, 0.6f, 0.6f, 0.4f } },
            { "Wasp Buzz", 0, { "Wasp", "Hornet", "Bee", "Gnat", "Midge", "Fly", "Mosquito", "Cicada" },
              { { s::FILTER, 8, 8, 'R' }, { s::WAVE, 0.0f, 0.6f, 'R' }, { s::CUTOFF, 250, 900, 'A' }, { s::RESONANCE, 0.2f, 0.5f, 'B' },
                { s::ENV_AMOUNT, 1.5f, 3.0f, 'R' }, { s::DRIVE, 0.3f, 0.6f, 'R' } },
              { 0.3f, 0.6f, 0.1f, 1.0f } },
            { "MS-20 Bite", 1, { "Bite", "Fang", "Tooth", "Tusk", "Claw", "Talon", "Beak", "Sting" },
              { { s::FILTER, 6, 6, 'R' }, { s::WAVE, 0.0f, 0.4f, 'R' }, { s::CUTOFF, 250, 1000, 'A' }, { s::RESONANCE, 0.3f, 0.6f, 'B' },
                { s::ENV_AMOUNT, 1.5f, 3.5f, 'R' }, { s::DRIVE, 0.2f, 0.5f, 'R' }, { s::DECAY, 100, 300, 'R' } },
              { 0.4f, 0.7f, 0.1f, 0.9f } },
            { "Hollow Square", 2, { "Hollow", "Cavity", "Void", "Well", "Chamber", "Shell", "Husk", "Pod" },
              { { s::FILTER, 0, 3, 'R' }, { s::WAVE, 1.0f, 1.0f, 'R' }, { s::PULSE_WIDTH, 0.35f, 0.5f, 'R' }, { s::CUTOFF, 200, 700, 'A' },
                { s::RESONANCE, 0.05f, 0.25f, 'R' }, { s::ENV_AMOUNT, 0.5f, 2.0f, 'B' } },
              { 0.8f, 0.5f, 0.7f, 0.3f } },
            { "Driven Tool", 3, { "Hammer", "Chisel", "Mallet", "Pliers", "Clamp", "Saw", "Drill", "Rasp" },
              { { s::FILTER, 0, 1, 'R' }, { s::DRIVE, 0.4f, 0.8f, 'B' }, { s::CUTOFF, 250, 900, 'A' }, { s::RESONANCE, 0.15f, 0.4f, 'R' },
                { s::ENV_AMOUNT, 1.5f, 3.0f, 'R' }, { s::DECAY, 100, 300, 'R' } },
              { 0.4f, 0.8f, 0.1f, 0.9f } },
            { "Short Blip Bass", 0, { "Blip", "Dot", "Pip", "Point", "Tick", "Jot", "Nib", "Stub" },
              { { s::FILTER, 0, 2, 'R' }, { s::AMP_DECAY, 40, 120, 'B' }, { s::AMP_SUSTAIN, 0.0f, 0.2f, 'R' }, { s::CUTOFF, 250, 900, 'A' },
                { s::ENV_AMOUNT, 2.0f, 4.0f, 'R' }, { s::DECAY, 40, 120, 'B' } },
              { 0.8f, 0.9f, 0.2f, 0.6f } },
            { "Filtered Drone Bass", 1, { "Drone", "Hum", "Buzz", "Whir", "Moan", "Groan", "Murmur", "Mumble" },
              { { s::FILTER, 0, 3, 'R' }, { s::CUTOFF, 120, 400, 'A' }, { s::RESONANCE, 0.2f, 0.45f, 'B' }, { s::ENV_AMOUNT, 0.3f, 1.0f, 'R' },
                { s::AMP_SUSTAIN, 0.8f, 1.0f, 'R' }, { s::AMP_RELEASE, 200, 600, 'R' }, { s::GLIDE, 30, 120, 'R' } },
              { 0.9f, 0.4f, 0.8f, 0.3f } },
            { "Rubber Bass", 2, { "Rubber", "Latex", "Elastic", "Bungee", "Spring", "Sponge", "Gel", "Putty" },
              { { s::FILTER, 0, 5, 'R' }, { s::CUTOFF, 200, 700, 'A' }, { s::RESONANCE, 0.3f, 0.55f, 'B' }, { s::ENV_AMOUNT, 2.0f, 3.5f, 'R' },
                { s::DECAY, 100, 250, 'R' }, { s::GLIDE, 20, 80, 'R' }, { s::ACCENT, 0.3f, 0.6f, 'R' } },
              { 0.7f, 0.8f, 0.3f, 0.6f } },
            { "Detroit Low", 3, { "Motor City", "Assembly", "Belt Line", "Plant", "Works", "Transmission", "Chassis", "Axle" },
              { { s::FILTER, 1, 2, 'R' }, { s::WAVE, 0.0f, 0.5f, 'R' }, { s::CUTOFF, 250, 850, 'A' }, { s::RESONANCE, 0.15f, 0.35f, 'B' },
                { s::ENV_AMOUNT, 1.5f, 3.0f, 'R' }, { s::DECAY, 120, 350, 'R' }, { s::SUB_OSC, 0.2f, 0.4f, 'R' } },
              { 0.7f, 0.9f, 0.3f, 0.5f } },
        } };
    return e;
}

// --- The 303 ----------------------------------------------------------------------------------------------------------
/**
 * @brief The 303's names and groups.
 * @return the 303's names and groups
 */
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
              { { s::FILTER, 5, 5, 'R' }, { s::CUTOFF, 300, 1100, 'A' }, { s::RESONANCE, 0.6f, 0.85f, 'B' }, { s::ENV_AMOUNT, 2.0f, 3.5f, 'R' },
                { s::DECAY, 150, 350, 'B' }, { s::ACCENT, 0.5f, 0.75f, 'R' }, { s::GLIDE, 40, 90, 'R' }, { s::DRIVE, 0.3f, 0.6f, 'R' } },
              { 0.6f, 0.7f, 0.3f, 0.9f } },
            { "Squelch", 1, { "Squish", "Splat", "Slosh", "Splodge", "Smear", "Goo", "Ooze", "Slurp" },
              { { s::FILTER, 5, 5, 'R' }, { s::CUTOFF, 400, 1200, 'A' }, { s::RESONANCE, 0.7f, 0.9f, 'B' }, { s::ENV_AMOUNT, 2.5f, 4.0f, 'R' },
                { s::DECAY, 120, 300, 'R' }, { s::ACCENT, 0.6f, 0.85f, 'R' } },
              { 0.4f, 0.6f, 0.1f, 1.0f } },
            { "Rubber Acid", 2, { "Band", "Hose", "Tyre", "Tube", "Balloon", "Ball", "Duck", "Boot" },
              { { s::FILTER, 5, 5, 'R' }, { s::CUTOFF, 300, 900, 'A' }, { s::RESONANCE, 0.55f, 0.75f, 'B' }, { s::ENV_AMOUNT, 1.5f, 2.5f, 'R' },
                { s::DECAY, 200, 400, 'B' }, { s::GLIDE, 60, 120, 'R' } },
              { 0.7f, 0.6f, 0.4f, 0.6f } },
            { "Dark Acid", 3, { "Cellar", "Crypt", "Dungeon", "Vault", "Bunker", "Undercroft", "Tomb", "Hold" },
              { { s::FILTER, 5, 5, 'R' }, { s::CUTOFF, 250, 600, 'A' }, { s::RESONANCE, 0.6f, 0.8f, 'B' }, { s::ENV_AMOUNT, 1.5f, 3.0f, 'R' },
                { s::DECAY, 180, 400, 'R' }, { s::DRIVE, 0.4f, 0.7f, 'R' } },
              { 0.9f, 0.6f, 0.3f, 0.6f } },
            { "Screamer", 0, { "Siren", "Klaxon", "Alarm", "Whistle", "Shriek", "Wail", "Howl", "Scream" },
              { { s::FILTER, 5, 6, 'R' }, { s::CUTOFF, 600, 1500, 'A' }, { s::RESONANCE, 0.8f, 0.95f, 'B' }, { s::ENV_AMOUNT, 3.0f, 4.5f, 'R' },
                { s::DRIVE, 0.5f, 0.8f, 'R' }, { s::ACCENT, 0.7f, 0.9f, 'R' } },
              { 0.1f, 0.4f, 0.0f, 1.0f } },
            { "Soft Acid", 1, { "Pillow", "Cushion", "Quilt", "Duvet", "Blanket", "Mattress", "Hammock", "Cradle" },
              { { s::FILTER, 5, 5, 'R' }, { s::CUTOFF, 300, 800, 'A' }, { s::RESONANCE, 0.45f, 0.65f, 'B' }, { s::ENV_AMOUNT, 1.0f, 2.0f, 'R' },
                { s::DECAY, 200, 450, 'R' }, { s::DRIVE, 0.1f, 0.3f, 'R' } },
              { 0.8f, 0.5f, 0.7f, 0.2f } },
            { "Hypnotic Acid", 2, { "Spiral", "Helix", "Coil", "Vortex", "Eddy", "Swirl", "Whorl", "Gyre" },
              { { s::FILTER, 5, 5, 'R' }, { s::CUTOFF, 350, 900, 'A' }, { s::RESONANCE, 0.6f, 0.8f, 'B' }, { s::ENV_AMOUNT, 1.5f, 3.0f, 'R' },
                { s::DECAY, 150, 350, 'R' }, { s::GLIDE, 50, 100, 'R' }, { s::ACCENT, 0.4f, 0.7f, 'R' } },
              { 1.0f, 0.6f, 0.4f, 0.6f } },
            { "Detroit Line", 3, { "Line", "Track", "Circuit", "Grid", "Route", "Beltway", "Freeway", "Parkway" },
              { { s::FILTER, 5, 5, 'R' }, { s::CUTOFF, 400, 1000, 'A' }, { s::RESONANCE, 0.55f, 0.75f, 'B' }, { s::ENV_AMOUNT, 2.0f, 3.0f, 'R' },
                { s::DECAY, 150, 300, 'R' }, { s::DRIVE, 0.2f, 0.5f, 'R' } },
              { 0.6f, 0.9f, 0.3f, 0.5f } },
            { "Wasp Acid", 0, { "Swarm", "Hive", "Nest", "Comb", "Colony", "Cluster", "Brood", "Horde" },
              { { s::FILTER, 8, 8, 'R' }, { s::CUTOFF, 400, 1100, 'A' }, { s::RESONANCE, 0.6f, 0.85f, 'B' }, { s::ENV_AMOUNT, 2.0f, 3.5f, 'R' },
                { s::DRIVE, 0.4f, 0.7f, 'R' } },
              { 0.3f, 0.5f, 0.1f, 1.0f } },
            { "MS-20 Acid", 1, { "Patch", "Cable", "Jack", "Socket", "Plug", "Lead", "Cord", "Wire" },
              { { s::FILTER, 6, 6, 'R' }, { s::CUTOFF, 400, 1200, 'A' }, { s::RESONANCE, 0.65f, 0.9f, 'B' }, { s::ENV_AMOUNT, 2.0f, 4.0f, 'R' },
                { s::DRIVE, 0.3f, 0.6f, 'R' }, { s::DECAY, 120, 300, 'R' } },
              { 0.3f, 0.6f, 0.1f, 0.9f } },
            { "Ladder Acid", 2, { "Rung", "Step", "Stair", "Tread", "Riser", "Landing", "Flight", "Banister" },
              { { s::FILTER, 0, 0, 'R' }, { s::CUTOFF, 350, 1000, 'A' }, { s::RESONANCE, 0.6f, 0.85f, 'B' }, { s::ENV_AMOUNT, 2.0f, 3.5f, 'R' },
                { s::DECAY, 150, 350, 'R' }, { s::ACCENT, 0.5f, 0.8f, 'R' } },
              { 0.7f, 0.7f, 0.3f, 0.7f } },
            { "Pulse Acid", 3, { "Gate", "Valve", "Switch", "Relay", "Trigger", "Toggle", "Latch", "Shutter" },
              { { s::FILTER, 5, 5, 'R' }, { s::WAVE, 1.0f, 1.0f, 'R' }, { s::PULSE_WIDTH, 0.3f, 0.5f, 'R' }, { s::CUTOFF, 350, 1000, 'A' },
                { s::RESONANCE, 0.6f, 0.8f, 'B' }, { s::ENV_AMOUNT, 2.0f, 3.5f, 'R' } },
              { 0.6f, 0.7f, 0.2f, 0.7f } },
            { "Deep Acid", 0, { "Depth", "Fathom", "Plumb", "Sounding", "Keel", "Hull", "Bilge", "Bottom" },
              { { s::FILTER, 5, 5, 'R' }, { s::CUTOFF, 250, 700, 'A' }, { s::RESONANCE, 0.5f, 0.75f, 'B' }, { s::ENV_AMOUNT, 1.5f, 2.5f, 'R' },
                { s::DECAY, 250, 500, 'R' }, { s::SUB_OSC, 0.2f, 0.5f, 'R' } },
              { 0.9f, 0.5f, 0.6f, 0.4f } },
            { "Bright Acid", 1, { "Flash", "Spark", "Glint", "Gleam", "Flicker", "Twinkle", "Sparkle", "Blaze" },
              { { s::FILTER, 5, 5, 'R' }, { s::CUTOFF, 700, 1500, 'A' }, { s::RESONANCE, 0.6f, 0.85f, 'B' }, { s::ENV_AMOUNT, 2.0f, 3.5f, 'R' },
                { s::DECAY, 100, 250, 'R' } },
              { 0.4f, 0.7f, 0.1f, 0.8f } },
            { "Gliding Acid", 2, { "Glider", "Skater", "Surfer", "Sailor", "Diver", "Swimmer", "Flyer", "Drifter" },
              { { s::FILTER, 5, 5, 'R' }, { s::GLIDE, 80, 160, 'B' }, { s::CUTOFF, 350, 900, 'A' }, { s::RESONANCE, 0.6f, 0.8f, 'R' },
                { s::ENV_AMOUNT, 1.5f, 3.0f, 'R' }, { s::DECAY, 180, 350, 'R' } },
              { 0.8f, 0.6f, 0.4f, 0.6f } },
            { "Bitter Acid", 3, { "Vinegar", "Lemon", "Lime", "Quince", "Rhubarb", "Sorrel", "Tamarind", "Sloe" },
              { { s::FILTER, 5, 7, 'R' }, { s::CUTOFF, 400, 1100, 'A' }, { s::RESONANCE, 0.7f, 0.9f, 'B' }, { s::DRIVE, 0.5f, 0.8f, 'R' },
                { s::ENV_AMOUNT, 2.5f, 4.0f, 'R' } },
              { 0.2f, 0.5f, 0.0f, 1.0f } },
        } };
    return e;
}

// --- The dub chord ----------------------------------------------------------------------------------------------------------
/**
 * @brief The dub chord's names and groups.
 * @return the dub chord's names and groups
 */
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
              { { c::DETUNE, 6, 15, 'R' }, { c::BAND, 300, 480, 'A' }, { c::BRIGHT, 2000, 4000, 'A' }, { c::DECAY, 200, 450, 'B' },
                { c::SUSTAIN, 0.05f, 0.25f, 'R' }, { c::RELEASE, 300, 700, 'B' }, { c::CRUSH, 7, 10, 'R' }, { c::CRUSH_MIX, 0.1f, 0.3f, 'R' },
                { c::PHASER, 0.3f, 0.6f, 'R' }, { c::WIDTH, 0.6f, 0.9f, 'R' } },
              { 0.6f, 0.5f, 1.0f, 0.2f } },
            { "Deep Stab", 1, { "Stab", "Thrust", "Lunge", "Jab", "Prod", "Poke", "Plunge", "Dive" },
              { { c::DETUNE, 5, 12, 'R' }, { c::BAND, 250, 420, 'A' }, { c::BRIGHT, 1500, 3000, 'A' }, { c::DECAY, 120, 300, 'B' },
                { c::SUSTAIN, 0.0f, 0.15f, 'R' }, { c::RELEASE, 200, 500, 'R' }, { c::ENV_AMOUNT, 1.0f, 2.0f, 'R' } },
              { 0.8f, 0.8f, 0.7f, 0.4f } },
            { "Glass Stab", 2, { "Crystal", "Icicle", "Frost", "Glaze", "Sleet", "Hail", "Rime", "Quartz" },
              { { c::DETUNE, 4, 10, 'R' }, { c::BAND, 380, 480, 'A' }, { c::BRIGHT, 3500, 7000, 'A' }, { c::DECAY, 100, 250, 'B' },
                { c::SUSTAIN, 0.0f, 0.1f, 'R' }, { c::BAND_MODE, 1, 1, 'R' }, { c::PHASER, 0.2f, 0.5f, 'R' } },
              { 0.6f, 0.7f, 0.5f, 0.5f } },
            { "Dub Chord", 3, { "Dubplate", "Echo", "Delay", "Spring", "Tape", "Reverb", "Siren", "Chamber" },
              { { c::DETUNE, 8, 15, 'R' }, { c::BAND, 300, 450, 'A' }, { c::BRIGHT, 2500, 4500, 'A' }, { c::DECAY, 250, 500, 'B' },
                { c::SUSTAIN, 0.1f, 0.3f, 'R' }, { c::RELEASE, 400, 900, 'B' }, { c::CRUSH, 6, 9, 'R' }, { c::CRUSH_MIX, 0.15f, 0.35f, 'R' },
                { c::DIP, -5, -2, 'R' }, { c::WIDTH, 0.8f, 0.95f, 'R' } },
              { 0.3f, 0.2f, 1.0f, 0.1f } },
            { "Organ Chord", 0, { "Organ", "Harmonium", "Accordion", "Bandoneon", "Concertina", "Melodica", "Calliope", "Reed Organ" },
              { { c::DETUNE, 3, 8, 'R' }, { c::ATTACK, 5, 20, 'R' }, { c::SUSTAIN, 0.4f, 0.7f, 'B' }, { c::DECAY, 300, 700, 'R' },
                { c::BAND, 320, 480, 'A' }, { c::BRIGHT, 2000, 4000, 'A' }, { c::BAND_Q, 0.4f, 0.8f, 'R' } },
              { 0.5f, 0.4f, 0.7f, 0.2f } },
            { "Crushed Stab", 1, { "Crush", "Grind", "Mash", "Pulp", "Crumble", "Shatter", "Splinter", "Fracture" },
              { { c::CRUSH, 5, 8, 'B' }, { c::CRUSH_MIX, 0.3f, 0.6f, 'B' }, { c::DETUNE, 6, 14, 'R' }, { c::BAND, 300, 460, 'A' },
                { c::BRIGHT, 2000, 4500, 'A' }, { c::DECAY, 150, 350, 'R' } },
              { 0.4f, 0.5f, 0.8f, 0.6f } },
            { "Phased Chord", 2, { "Phase", "Crescent", "Gibbous", "Quarter", "Waning", "Waxing", "New Moon", "Full Moon" },
              { { c::PHASER, 0.6f, 0.95f, 'B' }, { c::PHASER_RATE, 0.1f, 1.0f, 'B' }, { c::DETUNE, 6, 14, 'R' }, { c::BAND, 300, 460, 'A' },
                { c::BRIGHT, 2000, 4000, 'A' }, { c::DECAY, 200, 450, 'R' } },
              { 0.7f, 0.4f, 0.9f, 0.3f } },
            { "Soft Pad Stab", 3, { "Cloud", "Fleece", "Down", "Feather", "Cotton", "Wool", "Silk", "Satin" },
              { { c::ATTACK, 20, 80, 'B' }, { c::DECAY, 400, 900, 'B' }, { c::SUSTAIN, 0.2f, 0.5f, 'R' }, { c::RELEASE, 500, 1200, 'R' },
                { c::BAND, 300, 460, 'A' }, { c::BRIGHT, 1500, 3500, 'A' }, { c::DETUNE, 8, 16, 'R' } },
              { 0.7f, 0.3f, 0.8f, 0.1f } },
            { "Bright Stab", 0, { "Beam", "Ray", "Shaft", "Glare", "Flare", "Burst", "Nova", "Corona" },
              { { c::BRIGHT, 4000, 8000, 'A' }, { c::BAND, 400, 480, 'R' }, { c::DECAY, 100, 250, 'B' }, { c::SUSTAIN, 0.0f, 0.15f, 'R' },
                { c::ENV_AMOUNT, 1.5f, 3.0f, 'R' }, { c::DETUNE, 5, 12, 'R' } },
              { 0.4f, 0.8f, 0.4f, 0.7f } },
            { "Filtered Minor", 1, { "Minor", "Lament", "Elegy", "Dirge", "Requiem", "Nocturne", "Reverie", "Threnody" },
              { { c::BAND, 280, 450, 'A' }, { c::BAND_Q, 0.8f, 2.0f, 'B' }, { c::BRIGHT, 1800, 3500, 'A' }, { c::DECAY, 200, 500, 'R' },
                { c::SWEEP, 0.3f, 0.9f, 'R' }, { c::SWEEP_RATE, 0.05f, 0.4f, 'R' }, { c::DETUNE, 6, 12, 'R' } },
              { 0.9f, 0.6f, 0.7f, 0.3f } },
            { "Short Blip Chord", 2, { "Blip", "Pip", "Dot", "Beep", "Tick", "Click", "Chirp", "Cheep" },
              { { c::DECAY, 60, 150, 'B' }, { c::SUSTAIN, 0.0f, 0.05f, 'R' }, { c::RELEASE, 100, 300, 'R' }, { c::BAND, 330, 480, 'A' },
                { c::BRIGHT, 2500, 5000, 'A' }, { c::DETUNE, 4, 10, 'R' } },
              { 0.7f, 0.8f, 0.5f, 0.5f } },
            { "Long Swell", 3, { "Swell", "Tide", "Bloom", "Dawn", "Sunrise", "Rising", "Ascent", "Crescendo" },
              { { c::ATTACK, 80, 400, 'B' }, { c::DECAY, 600, 1500, 'R' }, { c::SUSTAIN, 0.3f, 0.6f, 'R' }, { c::RELEASE, 700, 1800, 'B' },
                { c::BAND, 280, 440, 'A' }, { c::BRIGHT, 1500, 3500, 'A' } },
              { 0.8f, 0.2f, 0.8f, 0.1f } },
            { "Tape Chord", 0, { "Cassette", "Reel", "Spool", "Loop", "Splice", "Leader", "Capstan", "Pinch Roller" },
              { { c::DETUNE, 10, 20, 'B' }, { c::CRUSH, 8, 12, 'R' }, { c::CRUSH_MIX, 0.2f, 0.4f, 'R' }, { c::BAND, 300, 450, 'A' },
                { c::BRIGHT, 1800, 3500, 'A' }, { c::DECAY, 250, 550, 'R' } },
              { 0.5f, 0.3f, 0.9f, 0.2f } },
            { "Mellow Keys", 1, { "Rhodes", "Wurlitzer", "Clavinet", "Celeste", "Piano", "Vibraphone", "Keyboard", "Keys" },
              { { c::DECAY, 300, 700, 'B' }, { c::SUSTAIN, 0.1f, 0.3f, 'R' }, { c::BAND, 320, 470, 'A' }, { c::BRIGHT, 2000, 3800, 'A' },
                { c::DETUNE, 3, 8, 'R' }, { c::PHASER, 0.2f, 0.5f, 'R' } },
              { 0.6f, 0.6f, 0.7f, 0.2f } },
            { "Metallic Stab", 2, { "Steel", "Chrome", "Nickel", "Cobalt", "Titanium", "Tungsten", "Platinum", "Iridium" },
              { { c::BAND_MODE, 1, 1, 'R' }, { c::BAND_Q, 1.0f, 2.5f, 'B' }, { c::BAND, 380, 480, 'A' }, { c::BRIGHT, 3000, 6000, 'A' },
                { c::DECAY, 120, 280, 'R' }, { c::CRUSH, 6, 10, 'R' }, { c::CRUSH_MIX, 0.2f, 0.4f, 'R' } },
              { 0.3f, 0.7f, 0.3f, 0.8f } },
            { "Warm Minor Pad", 3, { "Twilight", "Evensong", "Vesper", "Gloaming", "Sundown", "Eventide", "Moonrise", "Starlight" },
              { { c::ATTACK, 30, 120, 'R' }, { c::DECAY, 500, 1200, 'B' }, { c::SUSTAIN, 0.3f, 0.5f, 'R' }, { c::BAND, 280, 420, 'A' },
                { c::BRIGHT, 1500, 3000, 'A' }, { c::DETUNE, 8, 15, 'R' }, { c::WIDTH, 0.7f, 0.95f, 'R' } },
              { 0.9f, 0.3f, 0.8f, 0.1f } },
        } };
    return e;
}

// --- The drone ----------------------------------------------------------------------------------------------------------
/**
 * @brief The drone's names and groups.
 * @return the drone's names and groups
 */
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
              { { d::DETUNE, 4, 10, 'R' }, { d::CUTOFF, 200, 600, 'A' }, { d::RESONANCE, 0.1f, 0.3f, 'B' }, { d::SWEEP, 0.8f, 2.0f, 'R' },
                { d::SWEEP_BARS, 32, 64, 'R' }, { d::ATTACK, 3, 8, 'R' }, { d::RELEASE, 4, 10, 'R' } },
              { 1.0f, 0.5f, 0.7f, 0.4f } },
            { "Warm Drone", 1, { "Hearth", "Glow", "Ember", "Kindling", "Tinder", "Candle", "Lamp", "Lantern" },
              { { d::DETUNE, 6, 14, 'R' }, { d::CUTOFF, 400, 1200, 'A' }, { d::RESONANCE, 0.1f, 0.25f, 'R' }, { d::SWEEP, 1.0f, 2.0f, 'B' },
                { d::SWEEP_BARS, 32, 64, 'R' } },
              { 0.8f, 0.4f, 0.9f, 0.2f } },
            { "Glass Drone", 2, { "Pane", "Window", "Crystal", "Prism", "Lens", "Mirror", "Chandelier", "Goblet" },
              { { d::DETUNE, 3, 8, 'R' }, { d::CUTOFF, 1000, 3000, 'A' }, { d::RESONANCE, 0.3f, 0.5f, 'B' }, { d::SWEEP, 0.5f, 1.5f, 'R' } },
              { 0.7f, 0.5f, 0.5f, 0.3f } },
            { "Sweeping Drone", 3, { "Wind", "Gust", "Breeze", "Gale", "Zephyr", "Draught", "Current", "Flow" },
              { { d::SWEEP, 2.0f, 3.5f, 'B' }, { d::SWEEP_BARS, 16, 64, 'B' }, { d::CUTOFF, 300, 1000, 'A' }, { d::RESONANCE, 0.2f, 0.4f, 'R' },
                { d::DETUNE, 5, 12, 'R' } },
              { 0.9f, 0.4f, 0.7f, 0.4f } },
            { "Resonant Drone", 0, { "Resonance", "Overtone", "Harmonic", "Partial", "Formant", "Vowel", "Choir", "Chant" },
              { { d::RESONANCE, 0.45f, 0.75f, 'B' }, { d::CUTOFF, 300, 1200, 'A' }, { d::SWEEP, 1.0f, 2.5f, 'R' }, { d::DETUNE, 4, 10, 'R' } },
              { 0.8f, 0.6f, 0.4f, 0.6f } },
            { "Soft Bed", 1, { "Bed", "Mattress", "Pillow", "Blanket", "Quilt", "Cushion", "Nest", "Hammock" },
              { { d::CUTOFF, 250, 700, 'A' }, { d::RESONANCE, 0.0f, 0.15f, 'R' }, { d::ATTACK, 6, 15, 'B' }, { d::RELEASE, 8, 16, 'B' },
                { d::DETUNE, 6, 12, 'R' }, { d::SWEEP, 0.5f, 1.2f, 'R' } },
              { 0.8f, 0.3f, 0.9f, 0.1f } },
            { "Tape Drone", 2, { "Tape", "Loop", "Reel", "Spool", "Cassette", "Cartridge", "Wire", "Drum" },
              { { d::DETUNE, 12, 22, 'B' }, { d::CUTOFF, 300, 900, 'A' }, { d::RESONANCE, 0.1f, 0.3f, 'R' }, { d::SWEEP, 0.8f, 1.8f, 'R' } },
              { 0.6f, 0.3f, 0.9f, 0.2f } },
            { "Cold Drone", 3, { "Glacier", "Tundra", "Permafrost", "Floe", "Berg", "Frost", "Snowfield", "Ice Shelf" },
              { { d::CUTOFF, 600, 2000, 'A' }, { d::RESONANCE, 0.25f, 0.45f, 'B' }, { d::DETUNE, 2, 6, 'R' }, { d::SWEEP, 1.0f, 2.0f, 'R' } },
              { 0.8f, 0.7f, 0.3f, 0.5f } },
            { "Industrial Hum", 0, { "Transformer", "Substation", "Generator", "Dynamo", "Pylon", "Cable", "Grid", "Mains" },
              { { d::CUTOFF, 400, 1500, 'A' }, { d::RESONANCE, 0.3f, 0.6f, 'B' }, { d::DETUNE, 1, 4, 'R' }, { d::SWEEP, 0.3f, 1.0f, 'R' } },
              { 0.6f, 0.8f, 0.2f, 0.8f } },
            { "Slow Tide", 1, { "Ebb", "Flow", "Neap", "Spring Tide", "Undertow", "Swell", "Current", "Drift" },
              { { d::SWEEP_BARS, 64, 128, 'B' }, { d::SWEEP, 1.5f, 3.0f, 'R' }, { d::CUTOFF, 300, 900, 'A' }, { d::DETUNE, 5, 12, 'R' },
                { d::ATTACK, 6, 14, 'R' } },
              { 1.0f, 0.3f, 0.8f, 0.2f } },
            { "Choir Drone", 2, { "Choir", "Chorus", "Chant", "Hymn", "Psalm", "Anthem", "Canticle", "Motet" },
              { { d::RESONANCE, 0.35f, 0.6f, 'R' }, { d::CUTOFF, 500, 1600, 'A' }, { d::DETUNE, 8, 16, 'B' }, { d::SWEEP, 0.8f, 1.8f, 'R' } },
              { 0.8f, 0.4f, 0.6f, 0.2f } },
            { "Low Throb", 3, { "Throb", "Pulse", "Beat", "Heart", "Drum", "Pound", "Thud", "Boom" },
              { { d::CUTOFF, 150, 450, 'A' }, { d::RESONANCE, 0.2f, 0.45f, 'B' }, { d::SWEEP, 1.0f, 2.5f, 'R' }, { d::SWEEP_BARS, 16, 32, 'R' },
                { d::DETUNE, 3, 8, 'R' } },
              { 0.9f, 0.6f, 0.5f, 0.5f } },
            { "Airy Drone", 0, { "Air", "Ether", "Sky", "Heaven", "Firmament", "Vault", "Azure", "Empyrean" },
              { { d::CUTOFF, 1200, 3500, 'A' }, { d::RESONANCE, 0.05f, 0.2f, 'R' }, { d::DETUNE, 6, 12, 'B' }, { d::ATTACK, 5, 12, 'R' } },
              { 0.7f, 0.3f, 0.7f, 0.2f } },
            { "Rust Drone", 1, { "Rust", "Corrosion", "Patina", "Verdigris", "Oxide", "Scale", "Tarnish", "Decay" },
              { { d::CUTOFF, 300, 1000, 'A' }, { d::RESONANCE, 0.3f, 0.55f, 'B' }, { d::DETUNE, 8, 18, 'R' }, { d::SWEEP, 0.8f, 2.0f, 'R' } },
              { 0.6f, 0.6f, 0.4f, 0.7f } },
            { "Still Drone", 2, { "Lake", "Pond", "Pool", "Mere", "Tarn", "Loch", "Lagoon", "Marsh" },
              { { d::SWEEP, 0.1f, 0.5f, 'B' }, { d::CUTOFF, 300, 1000, 'A' }, { d::RESONANCE, 0.1f, 0.25f, 'R' }, { d::DETUNE, 3, 8, 'R' } },
              { 0.8f, 0.5f, 0.7f, 0.3f } },
            { "Deep Organ Drone", 3, { "Pedal", "Diapason", "Bourdon", "Open Flute", "Contra", "Subbass", "Principal", "Gedackt" },
              { { d::CUTOFF, 250, 800, 'A' }, { d::RESONANCE, 0.05f, 0.2f, 'R' }, { d::DETUNE, 2, 6, 'B' }, { d::ATTACK, 2, 6, 'R' },
                { d::RELEASE, 3, 8, 'R' } },
              { 0.7f, 0.5f, 0.6f, 0.3f } },
        } };
    return e;
}

// --- The texture (its amounts scaled so it stays about as loud as the default's: it sits under everything) ----------------------------------------------------------------------------------------------------------
/**
 * @brief The texture's names and groups.
 * @return the texture's names and groups
 */
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
              { { t::CRACKLE, 0.2f, 0.7f, 'A' }, { t::HUM, 0.0f, 0.2f, 'R' }, { t::EROSION, 0.1f, 0.4f, 'B' }, { t::WIDTH, 0.6f, 0.9f, 'R' } },
              { 0.8f, 0.4f, 1.0f, 0.3f } },
            { "Mains Hum", 1, { "Wire", "Socket", "Fuse", "Plug", "Cable", "Transformer", "Ballast", "Relay" },
              { { t::HUM, 0.13f, 0.35f, 'A' }, { t::HUM_HZ, 49, 51, 'R' }, { t::CRACKLE, 0.0f, 0.3f, 'R' }, { t::EROSION, 0.0f, 0.3f, 'B' } },
              { 0.4f, 0.5f, 0.8f, 0.5f } },
            { "Eroded Air", 2, { "Breath", "Sigh", "Whisper", "Draught", "Hiss", "Rustle", "Murmur", "Susurrus" },
              { { t::EROSION, 0.27f, 0.6f, 'A' }, { t::CRACKLE, 0.0f, 0.3f, 'R' }, { t::HUM, 0.0f, 0.1f, 'R' }, { t::WIDTH, 0.5f, 1.0f, 'B' } },
              { 0.9f, 0.5f, 0.7f, 0.4f } },
            { "Old Record", 3, { "Shellac", "Acetate", "Lacquer", "Dubplate", "Test Press", "White Label", "Promo", "Bootleg" },
              { { t::CRACKLE, 0.31f, 0.7f, 'A' }, { t::EROSION, 0.2f, 0.6f, 'B' }, { t::HUM, 0.1f, 0.3f, 'R' }, { t::WIDTH, 0.4f, 0.8f, 'R' } },
              { 0.5f, 0.3f, 1.0f, 0.2f } },
            { "Crackle Rain", 0, { "Drizzle", "Shower", "Downpour", "Squall", "Cloudburst", "Deluge", "Monsoon", "Torrent" },
              { { t::CRACKLE, 0.35f, 0.7f, 'A' }, { t::WIDTH, 0.7f, 1.0f, 'B' }, { t::EROSION, 0.1f, 0.3f, 'R' }, { t::HUM, 0.0f, 0.1f, 'R' } },
              { 0.6f, 0.4f, 0.8f, 0.4f } },
            { "Room Tone", 1, { "Room", "Studio", "Booth", "Cellar", "Attic", "Garage", "Basement", "Loft" },
              { { t::EROSION, 0.2f, 0.5f, 'A' }, { t::HUM, 0.05f, 0.25f, 'R' }, { t::CRACKLE, 0.0f, 0.15f, 'R' }, { t::WIDTH, 0.6f, 1.0f, 'B' } },
              { 0.8f, 0.6f, 0.7f, 0.3f } },
            { "Warm Hum", 2, { "Valve", "Tube", "Filament", "Heater", "Cathode", "Anode", "Grid", "Plate" },
              { { t::HUM, 0.12f, 0.35f, 'A' }, { t::HUM_HZ, 49, 51, 'R' }, { t::CRACKLE, 0.1f, 0.4f, 'B' }, { t::EROSION, 0.0f, 0.2f, 'R' } },
              { 0.5f, 0.4f, 0.9f, 0.3f } },
            { "Wide Dust", 3, { "Horizon", "Plain", "Steppe", "Prairie", "Savanna", "Desert", "Dune", "Mesa" },
              { { t::WIDTH, 0.85f, 1.0f, 'R' }, { t::CRACKLE, 0.2f, 0.6f, 'A' }, { t::EROSION, 0.2f, 0.5f, 'B' } },
              { 0.8f, 0.4f, 0.8f, 0.3f } },
            { "Narrow Crackle", 0, { "Needle", "Stylus", "Cartridge", "Tonearm", "Headshell", "Cantilever", "Groove", "Runout" },
              { { t::WIDTH, 0.1f, 0.4f, 'R' }, { t::CRACKLE, 0.26f, 0.7f, 'A' }, { t::EROSION, 0.0f, 0.3f, 'B' } },
              { 0.5f, 0.6f, 0.6f, 0.5f } },
            { "Tape Wear", 1, { "Oxide", "Dropout", "Wow", "Flutter", "Print Through", "Stretch", "Crease", "Splice" },
              { { t::EROSION, 0.3f, 0.6f, 'A' }, { t::HUM, 0.05f, 0.2f, 'R' }, { t::CRACKLE, 0.0f, 0.2f, 'R' }, { t::WIDTH, 0.4f, 0.8f, 'B' } },
              { 0.7f, 0.4f, 0.8f, 0.4f } },
            { "Static Field", 2, { "Static", "Ion", "Spark", "Arc", "Discharge", "Corona", "Plasma", "Lightning" },
              { { t::CRACKLE, 0.42f, 0.7f, 'A' }, { t::EROSION, 0.26f, 0.6f, 'B' }, { t::WIDTH, 0.6f, 1.0f, 'R' } },
              { 0.3f, 0.5f, 0.3f, 0.9f } },
            { "Ghost Hum", 3, { "Ghost", "Phantom", "Spectre", "Wraith", "Shade", "Spirit", "Apparition", "Revenant" },
              { { t::HUM, 0.09f, 0.35f, 'A' }, { t::HUM_HZ, 49, 51, 'R' }, { t::EROSION, 0.3f, 0.6f, 'B' }, { t::CRACKLE, 0.0f, 0.2f, 'R' } },
              { 0.9f, 0.4f, 0.6f, 0.3f } },
            { "Factory Floor", 0, { "Conveyor", "Lathe", "Press", "Loom", "Kiln", "Smelter", "Foundry", "Mill" },
              { { t::HUM, 0.15f, 0.35f, 'A' }, { t::EROSION, 0.26f, 0.6f, 'B' }, { t::CRACKLE, 0.1f, 0.4f, 'R' }, { t::WIDTH, 0.5f, 0.9f, 'R' } },
              { 0.4f, 0.7f, 0.2f, 0.8f } },
            { "Midnight Air", 1, { "Midnight", "Small Hours", "Witching Hour", "Night Shift", "Graveyard Shift", "Vigil", "Watch", "Lookout" },
              { { t::EROSION, 0.3f, 0.6f, 'A' }, { t::CRACKLE, 0.05f, 0.25f, 'R' }, { t::HUM, 0.0f, 0.15f, 'R' }, { t::WIDTH, 0.7f, 1.0f, 'B' } },
              { 1.0f, 0.5f, 0.6f, 0.3f } },
            { "Dirty Groove", 2, { "Groove", "Track", "Rut", "Furrow", "Trench", "Channel", "Gutter", "Ditch" },
              { { t::CRACKLE, 0.39f, 0.7f, 'A' }, { t::EROSION, 0.3f, 0.6f, 'B' }, { t::HUM, 0.1f, 0.3f, 'R' } },
              { 0.4f, 0.3f, 0.9f, 0.5f } },
            { "Clean Air", 3, { "Clearing", "Meadow", "Glade", "Dell", "Vale", "Heath", "Moor", "Fell" },
              { { t::CRACKLE, 0.0f, 0.2f, 'A' }, { t::EROSION, 0.05f, 0.25f, 'B' }, { t::HUM, 0.0f, 0.05f, 'R' }, { t::WIDTH, 0.6f, 0.9f, 'R' } },
              { 0.8f, 0.8f, 0.5f, 0.5f } },
        } };
    return e;
}

/**
 * @brief The names and groups of module @p m, null for a module without presets.
 * @param[in] m  the module
 * @return its names and groups, null for a module without presets
 */
const Engine* engineOf(ModuleE m)
{
    switch (m) {
    case ModuleE::KICK: return &kickEngine();
    case ModuleE::RUMBLE: return &rumbleEngine();
    case ModuleE::SUB: return &subEngine();
    case ModuleE::PERC: return &percEngine();
    case ModuleE::PING: return &pingEngine();
    case ModuleE::BASS: return &bassEngine();
    case ModuleE::ACID: return &acidEngine();
    case ModuleE::CHORD: return &chordEngine();
    case ModuleE::DRONE: return &droneEngine();
    case ModuleE::TEXTURE: return &textureEngine();
    default: return nullptr;
    }
}

/**
 * @brief The 1024 presets of module @p m: per group, the 64 of its adjectives and nouns, their knobs drawn along the
 *        axes.
 * @param[in] m  the module
 * @param[in] e  its names and groups
 * @return its 1024 presets
 */
std::vector<SoundPreset> build(ModuleE m, const Engine& e)
{
    ParamStore store;
    std::vector<SoundPreset> out;
    out.reserve(e.m_groups.size() * 64);
    for (size_t g = 0; g < e.m_groups.size(); ++g) {
        const Group& grp = e.m_groups[g];
        for (int v = 0; v < 64; ++v) {
            const int a = v / 8, n = v % 8;
            SoundPreset p;
            p.m_group = grp.m_name;
            p.m_name = std::string(e.m_adjectives[grp.m_adjectives][a]) + " " + grp.m_nouns[n];
            for (int s = 0; s < 4; ++s) p.m_styles[s] = grp.m_styles[s];
            p.m_roles = grp.m_roles;
            Rng rng;
            rng.seed(mixSeed(0x554D42u + 131u * static_cast<uint64_t>(m) + 17u * g, static_cast<uint64_t>(v)));   // "TOT"
            for (const Axis& ax : grp.m_axes) {
                // Where on the knob's range: the adjective's row or the noun's column (with a little of its own), or a draw.
                float t = ax.m_axis == 'A' ? a / 7.0f : (ax.m_axis == 'B' ? n / 7.0f : rng.uniform());
                if (ax.m_axis != 'R') t = std::clamp(t + 0.12f * (rng.uniform() - 0.5f), 0.0f, 1.0f);
                const ParamDesc& d = store.desc(store.id(m, 0, ax.m_k));
                float value;
                if (d.m_curve == CurveE::LOG && ax.m_lo > 0.0f && ax.m_hi > 0.0f) value = ax.m_lo * std::pow(ax.m_hi / ax.m_lo, t);
                else value = ax.m_lo + t * (ax.m_hi - ax.m_lo);
                if (d.m_curve == CurveE::CHOICE || d.m_curve == CurveE::INT || d.m_curve == CurveE::TOGGLE) {
                    // A choice between two values is drawn evenly (a plain round would favour neither end less).
                    value = ax.m_lo == ax.m_hi ? ax.m_lo : std::floor(std::min(ax.m_lo, ax.m_hi) + t * (std::fabs(ax.m_hi - ax.m_lo) + 1.0f) * 0.9999f);
                }
                p.m_values.push_back({ ax.m_k, std::clamp(value, d.m_minValue, d.m_maxValue) });
            }
            out.push_back(std::move(p));
        }
    }
    return out;
}

} // namespace

bool hasPresets(ModuleE module) { return engineOf(module) != nullptr; }

const std::vector<SoundPreset>& factoryPresets(ModuleE module)
{
    static std::mutex lock;
    static std::vector<SoundPreset> built[static_cast<int>(ModuleE::COUNT)];
    static bool done[static_cast<int>(ModuleE::COUNT)] = {};
    static const std::vector<SoundPreset> none;
    const Engine* e = engineOf(module);
    if (e == nullptr) return none;
    std::lock_guard<std::mutex> g(lock);
    const int i = static_cast<int>(module);
    if (!done[i]) { built[i] = build(module, *e); done[i] = true; }
    return built[i];
}

bool presetLeaves(ModuleE module, int k)
{
    switch (module) {
    case ModuleE::KICK: return k == kick::LEVEL || k == kick::TUNE || k == kick::TAIL_LIMIT || k == kick::LOW_CUT;
    case ModuleE::RUMBLE: return k == rumble::LEVEL || k == rumble::SUB || k == rumble::DUCK || k == rumble::DUCK_HOLD || k == rumble::DUCK_RELEASE;
    case ModuleE::SUB: return k == sub::LEVEL || k == sub::OCTAVE || k == sub::LOCK || k == sub::DUCK || k == sub::DUCK_HOLD || k == sub::DUCK_RELEASE;
    case ModuleE::PERC:
        return k == perc::ACTIVE || k == perc::ROLE || k == perc::LEVEL || k == perc::PAN || k == perc::CHOKE || k == perc::SHIFT
            || k == perc::DENSITY || k == perc::TUNE || k == perc::PAN_DEPTH || k == perc::PAN_BARS || k == perc::CUT_TRACK;
    case ModuleE::PING: return k == ping::LEVEL || k == ping::PAN;
    case ModuleE::BASS: case ModuleE::ACID:
        return k == synth::LEVEL || k == synth::PAN || k == synth::LOW_CUT || k == synth::DUB_SEND || k == synth::ROOM_SEND || k == synth::DUCK
            || k == synth::DUCK_RELEASE;
    case ModuleE::CHORD: return k == chord::LEVEL || k == chord::OCTAVE || k == chord::DUB_SEND || k == chord::PLATE_SEND;
    case ModuleE::DRONE: return k == drone::LEVEL || k == drone::OCTAVE || k == drone::PLATE_SEND || k == drone::ROOM_SEND;
    case ModuleE::TEXTURE: return k == texture::LEVEL;
    default: return true;
    }
}

std::vector<std::pair<int, float>> presetKnobs(ModuleE module, int instance, const SoundPreset& preset)
{
    static const ParamStore defaults;
    std::vector<std::pair<int, float>> out;
    const int count = ParamStore::moduleCount(module);
    for (int k = 0; k < count; ++k) {
        if (presetLeaves(module, k)) continue;
        float v = defaults.defaultValue(defaults.id(module, instance, k));
        for (const auto& e : preset.m_values) if (e.first == k) v = e.second;
        out.push_back({ k, v });
    }
    return out;
}

void applyPreset(ParamStore& params, ModuleE module, int instance, const SoundPreset& preset)
{
    for (const auto& e : presetKnobs(module, instance, preset)) params.set(params.id(module, instance, e.first), e.second);
}

int pickPreset(ModuleE module, const float* styleMix, int role, Rng& rng, const Preferences* prefs)
{
    const std::vector<SoundPreset>& list = factoryPresets(module);
    if (list.empty()) return -1;
    // A group by its fit to the track's style mix (cubed, so the fitting groups dominate), then one of its presets.
    std::vector<std::pair<size_t, float>> groups;   // first preset of the group, weight
    float total = 0.0f;
    for (size_t i = 0; i < list.size(); i += 64) {
        const SoundPreset& p = list[i];
        if (role >= 0 && p.m_roles != 0 && (p.m_roles & (1u << role)) == 0) continue;
        float fit = 0.0f;
        for (int s = 0; s < 4; ++s) fit += p.m_styles[s] * styleMix[s];
        const float w = fit * fit * fit * (prefs != nullptr ? prefs->group(p.m_group) : 1.0f);   // (Phase 17: the ratings)
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

} // namespace tot
