/**
 * @file main.cpp
 * @brief Totality for Meta Quest: the whole generator on the headset, played with the hands (PLAN 10.2).
 *
 * No game engine -- `NativeActivity` + `android_native_app_glue`, EGL, GLES 3, the Khronos OpenXR loader,
 * `XR_EXT_hand_tracking`, Oboe for audio, and the unchanged core from `../Core`. The frame of the app -- the OpenXR
 * session, the swapchains, the point renderer, the font, the hands and the audio stream -- is Ephemeris' Quest app
 * (after Phosphene's); the player, the gestures, the panel and the Eclipse in the room are Totality's.
 *
 * **Three threads.**
 *  - *Audio* (Oboe): `TrackPlayer::process` runs `Engine::process` and the play/stop fade. It never locks and never
 *    blocks: while the composer thread holds the player it writes silence and returns. It publishes the position and
 *    the level for the panel.
 *  - *Composer*: composes a whole track (or set) and loads it into the engine before the audio stream starts; "next"
 *    composes the next one here and swaps it in behind a fade. The engine allocates when it loads, so the load happens
 *    on this thread while the audio thread writes silence. Then the loudness, measured while it plays (Leveler.h).
 *  - *Render* (the glue thread): OpenXR frame loop, hands, gestures, the picture.
 *
 * **The hands are the hands at the mixer** (PLAN 10.2). The engine plays live (Engine::setLive):
 *
 * | Gesture | Effect |
 * |---|---|
 * | left pinch | play / stop (a 15 ms fade, the music pauses where it is) |
 * | right pinch | kick out / kick in (perform.mute_kick) -- the DJ's move |
 * | both hands pinched together | the next track (or set): composed from the next seed, swapped in behind a fade |
 * | left hand height | the master filter (perform.filter): low pass below mid height, high pass above, open at the middle |
 * | right hand height | the echo throw (perform.throw), from mid height up |
 *
 * A hand only moves its control while it is *not* pinching, and both are smoothed with a 0.15 s one-pole: a hand at
 * mid height plays exactly what the composer wrote, and nothing ever jumps.
 *
 * The grammar every generator's headset shares (01.10.2026): the desktop's frame (Plugin/Frame.h) reads the same hands
 * from the bridge with the same rules and the same numbers.
 *
 * **The Eclipse in the room.** Above the player, ahead and tilted towards them, the Patterns page's Eclipse (the
 * plugin's EditorEclipse.h) at the size of a room: the kick a dark disc -- no light at all -- in its corona, which
 * swells with every kick; every other part a ring of beads around it, a bar once round, the playhead's hand sweeping
 * them; where three rings meet on a sixteenth, a ray. As the Kaleidoscope rules ask: nothing about the camera moves
 * with the audio, and every brightness is a continuous function of the bar position (the next bar's beads fade in as
 * the last ones fade out).
 *
 * **`tot.cfg`** in `<externalDataPath>` (`/sdcard/Android/data/com.reneweller.totality.quest/files`):
 * @code
 *   mute=1              start silent (the test rule; the engine still runs)
 *   seed=2026           the first track's seed; the next takes the next seed
 *   minutes=7           length of a track
 *   set_minutes=60      a set of so many minutes instead of single tracks
 *   style=Hypnotic      Hypnotic, Ostgut, Dub or Raw
 *   osc_host=192.168.1.20   the score cues (Cue.h) to a visualiser such as Kaleidoscope; empty = off
 *   osc_port=9000
 *   bridge_host=192.168.1.20   the hands to the desktop's Totality (01.10.2026): played from here, the same gestures
 *   bridge_port=9103     its headset port (the desktop's settings, Headset)
 *   audio=0             no sound here, the desktop plays (the same as mute=1)
 *   quality=desktop     everything, the grain cloud too (default here: quest, PLAN 11)
 *   knobs=compose.key=D;perform.throw=0     any knobs, repeatable
 * @endcode
 */

#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <jni.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <oboe/Oboe.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "tot/Cue.h"
#include "tot/Engine.h"
#include "tot/Leveler.h"
#include "tot/Presets.h"
#include "tot/compose/Composer.h"
#include "tot/compose/Set.h"

/** @brief An info line in the Android log, tag "Totality" (printf style). */
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "Totality", __VA_ARGS__)
/** @brief An error line in the Android log, tag "Totality" (printf style). */
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "Totality", __VA_ARGS__)

using namespace tot;

namespace {

// ---------------------------------------------------------------- small math

/** @brief Column-major 4x4 matrix, the layout GL wants. */
struct Mat4 {
    float m_m[16];   ///< the sixteen values, column by column
};
/** @brief A point or direction. */
struct Vec3 {
    float m_x;   ///< right
    float m_y;   ///< up
    float m_z;   ///< back (towards the viewer)
};

/** @brief The identity matrix. */
Mat4 identity() { Mat4 r{}; r.m_m[0] = r.m_m[5] = r.m_m[10] = r.m_m[15] = 1.0f; return r; }

/** @brief The product @p a times @p b (b applied first). */
Mat4 multiply(const Mat4& a, const Mat4& b)
{
    Mat4 r{};
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr)
            r.m_m[c * 4 + rr] = a.m_m[0 * 4 + rr] * b.m_m[c * 4 + 0] + a.m_m[1 * 4 + rr] * b.m_m[c * 4 + 1]
                            + a.m_m[2 * 4 + rr] * b.m_m[c * 4 + 2] + a.m_m[3 * 4 + rr] * b.m_m[c * 4 + 3];
    return r;
}

/** @brief Asymmetric projection from OpenXR's four half-angles. */
Mat4 projectionFromFov(const XrFovf& fov, float nearZ, float farZ)
{
    const float l = std::tan(fov.angleLeft), r = std::tan(fov.angleRight);
    const float u = std::tan(fov.angleUp), d = std::tan(fov.angleDown);
    const float w = r - l, h = u - d;
    Mat4 p{};
    p.m_m[0] = 2.0f / w;  p.m_m[8] = (r + l) / w;
    p.m_m[5] = 2.0f / h;  p.m_m[9] = (u + d) / h;
    p.m_m[10] = -(farZ + nearZ) / (farZ - nearZ);
    p.m_m[11] = -1.0f;
    p.m_m[14] = -(2.0f * farZ * nearZ) / (farZ - nearZ);
    return p;
}

/** @brief The rotation of unit quaternion @p q as a matrix. */
Mat4 rotationFromQuat(const XrQuaternionf& q)
{
    const float x = q.x, y = q.y, z = q.z, w = q.w;
    Mat4 r = identity();
    r.m_m[0] = 1 - 2 * (y * y + z * z); r.m_m[4] = 2 * (x * y - z * w);     r.m_m[8] = 2 * (x * z + y * w);
    r.m_m[1] = 2 * (x * y + z * w);     r.m_m[5] = 1 - 2 * (x * x + z * z); r.m_m[9] = 2 * (y * z - x * w);
    r.m_m[2] = 2 * (x * z - y * w);     r.m_m[6] = 2 * (y * z + x * w);     r.m_m[10] = 1 - 2 * (x * x + y * y);
    return r;
}

/** @brief Vector @p v rotated by @p q. */
Vec3 rotate(const XrQuaternionf& q, Vec3 v)
{
    const Mat4 r = rotationFromQuat(q);
    return { r.m_m[0] * v.m_x + r.m_m[4] * v.m_y + r.m_m[8] * v.m_z,
             r.m_m[1] * v.m_x + r.m_m[5] * v.m_y + r.m_m[9] * v.m_z,
             r.m_m[2] * v.m_x + r.m_m[6] * v.m_y + r.m_m[10] * v.m_z };
}

/** @brief The view matrix of an eye pose: the inverse of the pose. */
Mat4 viewFromPose(const XrPosef& pose)
{
    const Mat4 r = rotationFromQuat(pose.orientation);
    Mat4 rt = identity();
    for (int c = 0; c < 3; ++c) for (int rr = 0; rr < 3; ++rr) rt.m_m[c * 4 + rr] = r.m_m[rr * 4 + c];
    Mat4 t = identity();
    t.m_m[12] = -pose.position.x; t.m_m[13] = -pose.position.y; t.m_m[14] = -pose.position.z;
    return multiply(rt, t);
}

/** @brief @p x limited to 0 .. 1. */
float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

// ---------------------------------------------------------------- 5x7 point font

/**
 * @brief One glyph as seven rows of five bits (bit 4 = left column), or the blank for the unknown.
 *
 * Text on the panel is drawn as points, like everything else: no texture, no atlas, one draw call
 * for the whole picture. (Table taken from Noctuary's Quest app, where it was measured legible at
 * arm's length; extended by '%', '#' and '*'.)
 */
const unsigned char* glyph(char c)
{
    static const unsigned char Font[][7] = {
        {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, // A B C
        {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}, {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, // D E F
        {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, // G H I
        {0x01,0x01,0x01,0x01,0x11,0x11,0x0E}, {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, // J K L
        {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, {0x11,0x19,0x15,0x13,0x11,0x11,0x11}, {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, // M N O
        {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, // P Q R
        {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, // S T U
        {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}, {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, // V W X
        {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},                                        // Y Z
        {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, // 0 1 2
        {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}, {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, // 3 4 5
        {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, // 6 7 8
        {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},                                                                              // 9
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}, // space - .
        {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00}, {0x01,0x02,0x04,0x08,0x10,0x00,0x00}, {0x02,0x04,0x08,0x04,0x02,0x00,0x00}, // : / <
        {0x08,0x04,0x02,0x04,0x08,0x00,0x00}, {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}, {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}, // > + =
        {0x18,0x19,0x02,0x04,0x08,0x13,0x03}, {0x0A,0x1F,0x0A,0x0A,0x0A,0x1F,0x0A}, {0x00,0x15,0x0E,0x1F,0x0E,0x15,0x00}, // % # *
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00},                                                                              // unknown
    };
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return Font[c - 'A'];
    if (c >= '0' && c <= '9') return Font[26 + (c - '0')];
    switch (c) {
    case ' ': return Font[36]; case '-': return Font[37]; case '.': return Font[38];
    case ':': return Font[39]; case '/': return Font[40]; case '<': return Font[41];
    case '>': return Font[42]; case '+': return Font[43]; case '=': return Font[44];
    case '%': return Font[45]; case '#': return Font[46]; case '*': return Font[47];
    default: break;
    }
    return Font[48];
}


// ---------------------------------------------------------------- config

/** @brief What `tot.cfg` can say. */
struct Config {
    bool m_mute = false;              ///< start silent (test rule); the engine still runs
    uint64_t m_seed = 1;              ///< the first track's seed
    double m_minutes = 0.0;           ///< length of a track (0: compose.minutes)
    double m_setMinutes = 0.0;        ///< a set of so many minutes instead of single tracks
    std::string m_oscHost;            ///< cue target (Cue.h), empty = off
    int m_oscPort = 9000;             ///< cue port
    std::string m_bridgeHost;         ///< the bridge (01.10.2026): the hands to the desktop's Totality there; empty = off
    int m_bridgePort = 9103;          ///< its headset port (the desktop's settings, Headset)
    std::string m_knobs;              ///< knob assignments, "key=value" separated by ';' or newlines
    bool m_quest = true;              ///< the Quest's quality (Engine::Quality): the grain cloud rests; `quality=desktop` plays all
};

/** @brief Reads `<dir>/tot.cfg`; every key is optional. */
Config readConfig(const char* dir)
{
    Config c;
    if (dir == nullptr) return c;
    const std::string path = std::string(dir) + "/tot.cfg";
    FILE* f = std::fopen(path.c_str(), "r");
    if (f == nullptr) { LOGI("no config at %s", path.c_str()); return c; }
    char line[512];
    while (std::fgets(line, sizeof(line), f)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
        if (s.empty() || s[0] == '#') continue;
        const size_t eq = s.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = s.substr(0, eq), v = s.substr(eq + 1);
        if (k == "mute") c.m_mute = v != "0";
        else if (k == "seed") c.m_seed = std::strtoull(v.c_str(), nullptr, 10);
        else if (k == "minutes") c.m_minutes = std::max(3.0, std::atof(v.c_str()));
        else if (k == "set_minutes") c.m_setMinutes = std::clamp(std::atof(v.c_str()), 0.0, 240.0);
        else if (k == "osc_host") c.m_oscHost = v;
        else if (k == "osc_port") c.m_oscPort = std::atoi(v.c_str());
        else if (k == "bridge_host") c.m_bridgeHost = v;
        else if (k == "bridge_port") c.m_bridgePort = std::atoi(v.c_str());
        else if (k == "audio") { if (v == "0") c.m_mute = true; }   // no sound here: the desktop plays (the bridge)
        else if (k == "quality") c.m_quest = v != "desktop";
        else if (k == "style") { c.m_knobs += "compose.style=" + v; c.m_knobs += ";"; }
        else if (k == "knobs") { c.m_knobs += v; c.m_knobs += ";"; }
        else LOGE("tot.cfg: unknown key %s", k.c_str());
    }
    std::fclose(f);
    LOGI("config: mute %d, seed %llu, set %.0f min", c.m_mute ? 1 : 0, static_cast<unsigned long long>(c.m_seed), c.m_setMinutes);
    return c;
}

// ---------------------------------------------------------------- the track player

/** @brief What plays, for the panel: a set, or a track as deck A of a one-track set, with where its tracks lie. */
struct Now {
    SetScore m_set;   ///< the set, or the one track as deck A
    bool m_isSet = false;   ///< a set plays (set_minutes), not a single track
    std::vector<TrackInfo> m_tracks;   ///< the tracks' descriptions, in playing order
    /** @brief set beat of each track's first bar */
    std::vector<double> m_starts, m_swaps;   ///< set beats of each track's first bar and of its swap
    std::vector<int> m_decks;   ///< the deck each track plays on
    /** @brief The track that owns the low end at @p beat. */
    int trackAt(double beat) const
    {
        int t = 0;
        for (size_t i = 0; i < m_tracks.size(); ++i) if (beat >= (m_isSet && i > 0 ? m_swaps[i] : m_starts[i])) t = static_cast<int>(i);
        return t;
    }
};

/**
 * @brief Engine and composer, with a play/stop fade and a swap to the next track or set.
 *
 * **Handover.** `m_state` is the only synchronisation between the audio thread and the composer thread. The audio thread
 * takes it from Idle to Processing with one compare-and-exchange and puts it back; if it cannot (the composer holds it
 * for a load), it writes silence and returns. The composer waits for Idle and takes it to Blocked. The audio thread
 * therefore never blocks.
 *
 * **What the panel reads.** Never the engine: its score and tempo map change when a track is loaded. The audio thread
 * publishes beat, seconds and level as atomics after every block, and the composer publishes what plays as an
 * immutable shared copy under a mutex the render thread takes for a pointer copy.
 */
class TrackPlayer {
public:
    /** @brief Composes the first track and loads it. Composer thread, before the audio stream starts. */
    void prepare(int sampleRate, int block, const Config& cfg)
    {
        m_muted = cfg.m_mute;
        m_seed = cfg.m_seed;
        m_setMinutes = cfg.m_setMinutes;
        m_engine.setLive(true);   // the perform module acts, the mixer is in a track's path (Engine.h)
        m_engine.setQuality(cfg.m_quest ? Engine::QualityE::QUEST : Engine::QualityE::DESKTOP);
        if (!cfg.m_knobs.empty()) {
            std::string err;
            if (!m_engine.params().parseText(cfg.m_knobs, &err)) LOGE("tot.cfg knobs: %s", err.c_str());
        }
        if (cfg.m_minutes > 0.0) m_engine.params().set(m_engine.params().id(ModuleE::COMPOSE, 0, compose::MINUTES), static_cast<float>(cfg.m_minutes));
        m_sr = static_cast<double>(sampleRate);
        m_gainCoef = static_cast<float>(1.0 - std::exp(-1.0 / (0.015 * sampleRate)));   // 15 ms
        m_levelCoef = static_cast<float>(1.0 - std::exp(-1.0 / (0.3 * sampleRate)));    // 300 ms
        m_engine.prepare(sampleRate, block);
        auto now = std::make_shared<Now>(composeFor(m_seed));
        load(*now);
        publish(now);
        m_levelDue = true;   // measured once the stream runs (pump)
        m_ready.store(true, std::memory_order_release);
        LOGI("ready: seed %llu, %.0f s", static_cast<unsigned long long>(m_seed), m_engine.lengthSeconds());
    }

    /** @brief Renders one block. Audio thread only. */
    void process(float* L, float* R, int n)
    {
        int expected = IDLE;
        if (!m_ready.load(std::memory_order_acquire) || !m_state.compare_exchange_strong(expected, PROCESSING)) {
            std::fill(L, L + n, 0.0f);
            std::fill(R, R + n, 0.0f);
            return;
        }
        if (m_trimsReady.load(std::memory_order_acquire)) {   // the loudness corrections, measured while it plays
            for (int d = 0; d < Decks; ++d) {
                if (!m_trimsIn[d].empty()) m_engine.setLevelTrims(d, m_trimsIn[d]);
                if (!m_balIn[d].empty()) m_engine.setLevelBalance(d, m_balIn[d]);
            }
            m_trimsReady.store(false, std::memory_order_release);
        }
        const float target = m_playing.load(std::memory_order_relaxed) ? 1.0f : 0.0f;
        if (target <= 0.0f && m_gain < 1.0e-4f) {
            // Faded out: the music waits where it is instead of running on silently.
            m_gain = 0.0f;
            std::fill(L, L + n, 0.0f);
            std::fill(R, R + n, 0.0f);
        } else {
            const double from = m_engine.beat();
            m_engine.process(L, R, n);
            if (m_cues != nullptr && m_cues->running()) {
                // The cues of this block (Cue.h), stamped with the moment the block is heard.
                const double blockSeconds = static_cast<double>(n) / m_sr, to = m_engine.beat();
                const float bpm = static_cast<float>((to - from) / blockSeconds * 60.0);
                m_tap.scan(m_engine.cueMarks(), from, to, bpm, CueSender::nowNanos(), static_cast<int64_t>(2.0 * blockSeconds * 1.0e9),
                          static_cast<int64_t>(blockSeconds * 1.0e9), m_cues->ring());
            }
            const float out = m_muted ? 0.0f : 1.0f;
            float level = m_level.load(std::memory_order_relaxed);
            for (int i = 0; i < n; ++i) {
                m_gain += (target - m_gain) * m_gainCoef;   // one pole: no step, no click
                level += (0.5f * (L[i] * L[i] + R[i] * R[i]) * m_gain * m_gain - level) * m_levelCoef;
                L[i] *= m_gain * out;
                R[i] *= m_gain * out;
            }
            m_level.store(level, std::memory_order_relaxed);
            m_beat.store(m_engine.beat(), std::memory_order_relaxed);
            m_seconds.store(m_engine.seconds(), std::memory_order_relaxed);
            if (m_engine.seconds() > m_engine.lengthSeconds() + 8.0) m_nextRequest.store(true, std::memory_order_relaxed);
        }
        m_state.store(IDLE, std::memory_order_release);
    }

    /** @brief Binds the cue sender (Cue.h); null leaves the cues off. Call before the audio stream starts. */
    void setCueSender(CueSender* sender) { m_cues = sender; }
    /** @brief Play or stop (any thread; takes effect over the fade). */
    void setPlaying(bool on) { m_playing.store(on, std::memory_order_relaxed); }
    /** @brief Whether play is on. */
    bool playing() const { return m_playing.load(std::memory_order_relaxed); }
    /** @brief Asks for the next track or set (any thread). */
    void requestNext() { m_nextRequest.store(true, std::memory_order_relaxed); }
    /** @brief One round of composer work: a pending next track, else the loudness of the one playing. Composer thread. */
    void pump()
    {
        if (m_nextRequest.exchange(false, std::memory_order_relaxed)) next();
        else if (m_levelDue) { m_levelDue = false; level(); }
    }
    /** @brief Breaks off the composer's work (the app closes). */
    void quit() { m_quit.store(true, std::memory_order_relaxed); }

    /** @brief Where the audio thread is, in beats and seconds (render thread). */
    double beat() const { return m_beat.load(std::memory_order_relaxed); }
    double seconds() const { return m_seconds.load(std::memory_order_relaxed); }   ///< @copydoc beat
    /** @brief The output's mean square, smoothed over 300 ms, as dBFS (render thread). */
    float levelDb() const { const float l = m_level.load(std::memory_order_relaxed); return l > 1.0e-10f ? 10.0f * std::log10(l) : -100.0f; }
    /** @brief What plays (render thread): an immutable copy. */
    std::shared_ptr<const Now> now() const { std::lock_guard<std::mutex> lock(m_nowMutex); return m_now; }
    /** @brief Which track or set plays, from 1. */
    int number() const { return m_number.load(std::memory_order_relaxed); }
    /** @brief The knobs, for the hand controls (atomics: any thread). */
    ParamStore& params() { return m_engine.params(); }
    /** @brief Whether prepare() has finished. */
    bool ready() const { return m_ready.load(std::memory_order_acquire); }
    /** @brief Whether the output is muted by the config. */
    bool muted() const { return m_muted; }

private:
    enum : int { IDLE = 0, PROCESSING, BLOCKED };

    /** @brief Composes the track or set after this one and swaps it in behind a fade. Composer thread. */
    void next()
    {
        const bool wasPlaying = playing();
        setPlaying(false);
        const int n = m_number.load(std::memory_order_relaxed) + 1;
        // Composed while the fade runs and the old one still waits: nothing is taken from the audio thread yet.
        auto now = std::make_shared<Now>(composeFor(m_seed + static_cast<uint64_t>(n - 1)));
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        int expected = IDLE;
        while (!m_state.compare_exchange_weak(expected, BLOCKED)) {
            expected = IDLE;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        load(*now);                // allocates: here, while the audio thread writes silence
        m_trimsReady.store(false, std::memory_order_relaxed);
        m_tap.reset();
        m_gain = 0.0f;
        m_beat.store(0.0, std::memory_order_relaxed);
        m_seconds.store(0.0, std::memory_order_relaxed);
        m_state.store(IDLE, std::memory_order_release);
        m_number.store(n, std::memory_order_relaxed);
        publish(now);
        setPlaying(wasPlaying);
        m_levelDue = true;
        LOGI("%s %d: seed %llu", now->m_isSet ? "set" : "track", n, static_cast<unsigned long long>(m_seed + static_cast<uint64_t>(n - 1)));
    }

    /**
     * @brief The loudness of what plays (Leveler.h), measured while it plays -- on the headset's cores a good many
     *        seconds: the corrections go to the audio thread, which glides them in (Engine::setLevelTrims). A request
     *        for the next track breaks it off.
     */
    void level()
    {
        const std::shared_ptr<const Now> playing = now();
        if (!playing) return;
        SetScore s = playing->m_set;
        auto stop = [this] { return m_nextRequest.load(std::memory_order_relaxed) || m_quit.load(std::memory_order_relaxed); };
        const bool done = playing->m_isSet ? !levelSet(s, m_engine.params(), 20.0, stop).empty() : !levelScore(s.m_decks[0], m_engine.params(), 20.0, stop).empty();
        if (!done || m_trimsReady.load(std::memory_order_acquire)) return;
        for (int d = 0; d < Decks; ++d) {
            m_trimsIn[d].clear();
            m_balIn[d].clear();
            for (const LevelMark& m : s.m_decks[d].m_levels) { m_trimsIn[d].push_back(m.m_trimDb); m_balIn[d].push_back(m.m_balDb); }
        }
        m_trimsReady.store(true, std::memory_order_release);
        LOGI("%d: loudness corrected", number());
    }

    /** @brief Hands @p s to the render thread (now()) under the mutex. */
    void publish(std::shared_ptr<const Now> s) { std::lock_guard<std::mutex> lock(m_nowMutex); m_now = std::move(s); }

    /** @brief Loads @p now into the engine: a set, or the one track. Only while the audio thread is kept out (m_state). */
    void load(const Now& now)
    {
        if (now.m_isSet) m_engine.loadSet(now.m_set);
        else m_engine.load(now.m_set.m_decks[0]);
    }

    /** @brief A track, or with set_minutes a set. Composer thread. */
    Now composeFor(uint64_t seed)
    {
        const ParamStore& p = m_engine.params();
        Now out;
        if (m_setMinutes > 0.0) {
            SetInfo info;
            out.m_isSet = true;
            out.m_set = composeSet(p, seed, m_setMinutes, nullptr, &info);
            for (const SetTrack& t : info.m_tracks) {
                out.m_tracks.push_back(t.m_info);
                out.m_starts.push_back(t.m_start);
                out.m_swaps.push_back(t.m_swapIn);
                out.m_decks.push_back(t.m_deck);
            }
        } else {
            TrackInfo info;
            out.m_set.m_decks[0] = composeTrack(p, seed, TrackRequest{}, nullptr, std::string(), &info);
            out.m_set.m_lengthBeats = out.m_set.m_decks[0].m_lengthBeats;
            out.m_tracks.push_back(info);
            out.m_starts.push_back(0.0);
            out.m_swaps.push_back(0.0);
            out.m_decks.push_back(0);
        }
        return out;
    }

    Engine m_engine;   ///< the engine: the decks, the mixer, the perform controls
    uint64_t m_seed = 1;   ///< the first track's seed; the n-th takes m_seed + n - 1
    double m_setMinutes = 0.0;   ///< the length of a set, minutes; 0: single tracks
    std::atomic<int> m_state{ IDLE };   ///< the handover between the audio thread and the composer thread (IDLE, PROCESSING, BLOCKED)
    std::atomic<bool> m_ready{ false };   ///< prepare() has finished
    std::atomic<bool> m_playing{ false };   ///< play is on (the fade follows it)
    std::atomic<bool> m_nextRequest{ false };   ///< the next track or set is asked for
    std::atomic<bool> m_quit{ false };   ///< the app closes: the composer's work breaks off
    bool m_levelDue = false;                      ///< what plays is still to be measured (composer thread)
    std::vector<float> m_trimsIn[Decks];         ///< its corrections, for the audio thread once m_trimsReady says so
    std::vector<BalanceDbT> m_balIn[Decks];       ///< Phase 18: the parts' corrections found with them
    std::atomic<bool> m_trimsReady{ false };   ///< m_trimsIn and m_balIn hold corrections the audio thread has not yet taken
    std::atomic<int> m_number{ 1 };   ///< which track or set plays, from 1
    std::atomic<double> m_beat{ 0.0 };   ///< where the audio thread is, in beats, after its last block
    std::atomic<double> m_seconds{ 0.0 };   ///< where the audio thread is, in seconds, after its last block
    std::atomic<float> m_level{ 0.0f };   ///< the output's mean square, smoothed over 300 ms
    bool m_muted = false;   ///< the config mutes the output (the engine still runs)
    double m_sr = 48000.0;   ///< the sample rate, Hz
    CueSender* m_cues = nullptr;   ///< the cue bridge, owned by the app; null = off
    CueTap m_tap;                  ///< audio thread: beat range -> cues
    float m_gain = 0.0f;   ///< the play/stop fade, 0 .. 1
    float m_gainCoef = 0.002f;   ///< the fade's one-pole coefficient (15 ms)
    float m_levelCoef = 0.0001f;   ///< the level meter's one-pole coefficient (300 ms)
    mutable std::mutex m_nowMutex;   ///< guards m_now
    std::shared_ptr<const Now> m_now;   ///< what plays, as the render thread reads it
};

// ---------------------------------------------------------------- audio

/** @brief Oboe output stream: a low-latency float stream straight into TrackPlayer::process. */
class Audio : public oboe::AudioStreamDataCallback {
public:
    /** @brief A stream that will play @p p (open() and start() it). */
    explicit Audio(TrackPlayer& p) : m_player(p) {}

    /** @brief Opens a low-latency stereo float stream at 48 kHz (or what the device gives); false if it cannot. */
    bool open()
    {
        oboe::AudioStreamBuilder b;
        b.setDirection(oboe::Direction::Output)
         ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
         ->setSharingMode(oboe::SharingMode::Exclusive)
         ->setFormat(oboe::AudioFormat::Float)
         ->setChannelCount(2)
         ->setSampleRate(48000)
         ->setDataCallback(this);
        if (b.openStream(m_stream) != oboe::Result::OK) { LOGE("Oboe: cannot open stream"); return false; }
        m_sampleRate = m_stream->getSampleRate();
        m_burst = m_stream->getFramesPerBurst();
        m_stream->setBufferSizeInFrames(m_burst * 2);
        m_bufL.assign(4096, 0.0f);
        m_bufR.assign(4096, 0.0f);
        LOGI("Oboe: %d Hz, burst %d", m_sampleRate, m_burst);
        return true;
    }
    /** @brief Starts the stream; false if it does not. */
    bool start() { return m_stream && m_stream->requestStart() == oboe::Result::OK; }
    /** @brief Stops and closes the stream. */
    void stop() { if (m_stream) { m_stream->requestStop(); m_stream->close(); m_stream.reset(); } }
    /** @brief The rate the device gave, Hz. */
    int sampleRate() const { return m_sampleRate; }
    /** @brief The device's burst, frames. */
    int burst() const { return m_burst; }

    /**
     * @brief Oboe's callback: renders @p frames through the player in pieces of at most 4096 and interleaves them into @p
     *        data.
     */
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream*, void* data, int32_t frames) override
    {
        float* out = static_cast<float*>(data);
        int done = 0;
        while (done < frames) {
            const int n = std::min(frames - done, static_cast<int32_t>(m_bufL.size()));
            m_player.process(m_bufL.data(), m_bufR.data(), n);
            for (int i = 0; i < n; ++i) {
                out[(done + i) * 2] = m_bufL[static_cast<size_t>(i)];
                out[(done + i) * 2 + 1] = m_bufR[static_cast<size_t>(i)];
            }
            done += n;
        }
        return oboe::DataCallbackResult::Continue;
    }

private:
    TrackPlayer& m_player;   ///< what renders the audio
    std::shared_ptr<oboe::AudioStream> m_stream;   ///< the open stream, or null
    std::vector<float> m_bufL;   ///< the left channel of one piece
    std::vector<float> m_bufR;   ///< the right channel of one piece
    int m_sampleRate = 48000;   ///< the rate the device gave, Hz
    int m_burst = 256;   ///< the device's burst, frames
};

// ---------------------------------------------------------------- GL scene

/** @brief The vertex shader: a point at its position, sized by its distance. */
const char* kVertexShader = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aCol;
layout(location = 2) in float aSize;
uniform mat4 uVP;
out vec4 vCol;
void main() {
    vec4 p = uVP * vec4(aPos, 1.0);
    gl_Position = p;
    gl_PointSize = aSize / max(p.w, 0.2);
    vCol = aCol;
})";

/** @brief The fragment shader: a soft round dot, added to what is there. */
const char* kFragmentShader = R"(#version 300 es
precision mediump float;
in vec4 vCol;
out vec4 o;
void main() {
    vec2 d = gl_PointCoord - vec2(0.5);
    float r = length(d) * 2.0;
    float a = smoothstep(1.0, 0.15, r);
    o = vec4(vCol.rgb * a * vCol.a, 1.0);
})";

/** @brief One soft round point: the only primitive the scene has. */
struct Point {
    float m_x;      ///< position in the room, metres
    float m_y;      ///< @copydoc x
    float m_z;      ///< @copydoc x
    float m_r;      ///< red 0..1
    float m_g;      ///< green 0..1
    float m_b;      ///< blue 0..1
    float m_a;      ///< brightness 0..1
    float m_size;   ///< diameter, pixels at one metre
};

/** @brief A head-locked plane to lay text out on: origin plus a right and an up axis. */
struct Panel {
    Vec3 m_origin;   ///< the top left of the text, in the room
    Vec3 m_right;    ///< one metre to the right along the panel
    Vec3 m_up;       ///< one metre up along the panel
};

/** @brief A shader of @p type compiled from @p src; a failure is logged. */
GLuint compile(GLenum type, const char* src)
{
    const GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[1024]; glGetShaderInfoLog(s, sizeof(log), nullptr, log); LOGE("shader: %s", log); }
    return s;
}

/**
 * @brief Everything visible, as additive points.
 *
 * The Kaleidoscope rules apply (docs/PLAN.md 8.2): nothing about the camera moves with the audio,
 * and every brightness is a continuous function of the bar position -- no step, no flash.
 */
class Scene {
public:
    /** @brief Display pixels per radian on the Quest 2's render target (about 1830 px over 90 deg). */
    static constexpr float PixelsPerRadian = 1150.0f;

    /** @brief Links the shaders and sets up the point buffer; false if the program does not link. */
    bool init()
    {
        m_program = glCreateProgram();
        glAttachShader(m_program, compile(GL_VERTEX_SHADER, kVertexShader));
        glAttachShader(m_program, compile(GL_FRAGMENT_SHADER, kFragmentShader));
        glLinkProgram(m_program);
        GLint ok = 0;
        glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
        if (!ok) { LOGE("program link failed"); return false; }
        m_uVP = glGetUniformLocation(m_program, "uVP");
        glGenBuffers(1, &m_vbo);
        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Point), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Point), reinterpret_cast<void*>(12));
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Point), reinterpret_cast<void*>(28));
        glBindVertexArray(0);
        return true;
    }

    /** @brief Starts a new picture: forgets the points of the last frame. */
    void begin() { m_points.clear(); }

    /** @brief One point in world space. */
    void add(float x, float y, float z, float r, float g, float b, float a, float size)
    {
        m_points.push_back({ x, y, z, r, g, b, a, size });
    }

    /** @brief One point on a panel, at panel coordinates @p u (right) and @p v (up). */
    void addOn(const Panel& p, float u, float v, float r, float g, float b, float a, float size)
    {
        add(p.m_origin.m_x + p.m_right.m_x * u + p.m_up.m_x * v,
            p.m_origin.m_y + p.m_right.m_y * u + p.m_up.m_y * v,
            p.m_origin.m_z + p.m_right.m_z * u + p.m_up.m_z * v, r, g, b, a, size);
    }

    /**
     * @brief Text on a panel, one point per lit pixel of the 5x7 font; @p cell is the pixel pitch.
     *
     * The point size is `cell * PixelsPerRadian / w`: a cell of `cell` metres at `w` metres
     * subtends `cell / w` radians, and the Quest 2 renders about 1150 pixels per radian (roughly
     * 1830 pixels over 90 degrees per eye), so the dots of a glyph just touch at any distance.
     */
    void addText(const Panel& p, float u, float v, float cell, const char* text,
                 float r, float g, float b, float a)
    {
        for (const char* c = text; *c; ++c) {
            const unsigned char* gl = glyph(*c);
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col)
                    if (gl[row] & (0x10 >> col))
                        addOn(p, u + static_cast<float>(col) * cell, v - static_cast<float>(row) * cell,
                              r, g, b, a, cell * PixelsPerRadian);
            u += 6.0f * cell;
        }
    }

    /** @brief Sends this frame's points to the GPU. */
    void upload()
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_points.size() * sizeof(Point)), m_points.data(), GL_DYNAMIC_DRAW);
    }

    /** @brief Draws the points for one eye with view-projection @p vp into a @p width by @p height target. */
    void draw(const Mat4& vp, int width, int height)
    {
        glViewport(0, 0, width, height);
        glClearColor(0.008f, 0.008f, 0.010f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);            // additive: points add light, they never occlude
        glUseProgram(m_program);
        glUniformMatrix4fv(m_uVP, 1, GL_FALSE, vp.m_m);
        glBindVertexArray(m_vao);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_points.size()));
        glBindVertexArray(0);
    }

private:
    GLuint m_program = 0;   ///< the shader program
    GLuint m_vbo = 0;   ///< the points' buffer
    GLuint m_vao = 0;   ///< the points' vertex layout
    GLint m_uVP = -1;   ///< the location of the view-projection uniform
    std::vector<Point> m_points;   ///< this frame's points
};

// ---------------------------------------------------------------- hands

/**
 * @brief Hand state and the mapping to the two macros and the two buttons.
 *
 * Height is measured against the head, not the floor: `(palm.y - (head.y - 1.0)) / 0.8`, so the
 * mapping is the same whether the runtime gave us a STAGE space (floor at y = 0) or a LOCAL one
 * (origin wherever the session started), and it is the same for a tall and a short player.
 *
 * The pinch is the thumb tip to index tip distance with a Schmitt trigger (closed under 22 mm, open
 * over 38 mm), so a hand held near the threshold does not rattle between the two states.
 */
class Hands {
public:
    /** @brief One hand's measurement of this frame. */
    struct Hand {
        bool m_valid = false;   ///< tracked this frame
        XrVector3f m_palm{};   ///< the palm's position in the room
        float m_pinch = 0.0f;     ///< 0 open .. 1 closed
        bool m_closed = false;    ///< after the Schmitt trigger
        float m_height = 0.5f;    ///< 0..1 against the head
        float m_macro = 0.5f;     ///< the smoothed value the macro follows
        bool m_everSeen = false;  ///< this hand has been tracked at least once
    };

    /** @brief Feeds one hand; @p headY is the head's height in the same space. */
    void setHand(int h, const XrVector3f& palm, float thumbToIndex, float headY)
    {
        Hand& s = m_hand[h];
        s.m_valid = true;
        s.m_everSeen = true;
        s.m_palm = palm;
        s.m_pinch = clamp01(1.0f - (thumbToIndex - 0.015f) / 0.035f);
        if (thumbToIndex < 0.022f) s.m_closed = true;
        else if (thumbToIndex > 0.038f) s.m_closed = false;
        s.m_height = clamp01((palm.y - (headY - 1.0f)) / 0.8f);
    }
    /** @brief Marks a hand as not tracked this frame (its macro then holds its value). */
    void lost(int h) { m_hand[h].m_valid = false; m_hand[h].m_closed = false; }

    /**
     * @brief Advances the smoothing and reports the two rising pinch edges.
     * @param dt         seconds since the last frame
     * @param leftPinch  receives true on the frame the left hand closes
     * @param rightPinch receives true on the frame the right hand closes
     *
     * A macro follows its hand only while that hand is open: the pinch that starts a track must not
     * also drag the gain with it.
     */
    void update(double dt, bool& leftPinch, bool& rightPinch)
    {
        // 0.15 s one pole. dt is capped at 0.1 s so that a long frame -- the first one after the
        // session resumes, say -- cannot make the coefficient 1 and snap the macro to the hand.
        const float k = 1.0f - std::exp(-static_cast<float>(std::min(dt, 0.1)) / 0.15f);
        for (int h = 0; h < 2; ++h) {
            Hand& s = m_hand[h];
            if (s.m_valid && !s.m_closed) s.m_macro += (s.m_height - s.m_macro) * k;
        }
        leftPinch = m_hand[0].m_closed && !m_wasClosed[0];
        rightPinch = m_hand[1].m_closed && !m_wasClosed[1];
        m_wasClosed[0] = m_hand[0].m_closed;
        m_wasClosed[1] = m_hand[1].m_closed;
    }

    /** @brief Hand @p h: 0 left, 1 right. */
    const Hand& hand(int h) const { return m_hand[h]; }

private:
    Hand m_hand[2];   ///< left, right
    bool m_wasClosed[2] = { false, false };   ///< left, right: closed in the last update (the rising edges)
};

// ---------------------------------------------------------------- the bridge

/**
 * @brief The bridge mode (01.10.2026): the hands to the desktop's Totality over OSC (UDP), which then plays as if they were
 *        its own -- the same gestures, read by its frame (Plugin/Frame.h, Headset) with the same rules as here.
 *
 * "/hands" with six floats: left height, right height, left pinch, right pinch, left tracked, right tracked (the heights
 * 0..1 against the head and unsmoothed, the desktop smooths them; the others 0 or 1), about 30 times a second. Fire and
 * forget: a desktop that is not there changes nothing. After Noctuary's bridge (AmbientSynth's Quest/src/main.cpp, OscOut).
 */
class HandBridge {
public:
    HandBridge() = default;
    HandBridge(const HandBridge&) = delete;
    HandBridge& operator=(const HandBridge&) = delete;
    ~HandBridge() { close(); }
    /** @brief Opens the UDP socket to @p host (a dotted IPv4 address, no names) and @p port; false if it cannot. */
    bool open(const std::string& host, int port)
    {
        close();
        m_sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_sock < 0) return false;
        std::memset(&m_to, 0, sizeof(m_to));
        m_to.sin_family = AF_INET;
        m_to.sin_port = htons(static_cast<uint16_t>(port));
        if (inet_pton(AF_INET, host.c_str(), &m_to.sin_addr) != 1) { close(); return false; }
        return true;
    }
    /** @brief Closes the socket; send() then does nothing. */
    void close() { if (m_sock >= 0) ::close(m_sock); m_sock = -1; }
    /** @brief Whether the bridge is on. */
    bool ok() const { return m_sock >= 0; }
    /** @brief Sends the hands, at most 30 times a second; @p dt the seconds since the last frame (render thread). */
    void send(const Hands& hands, double dt)
    {
        if (m_sock < 0) return;
        m_since += dt;
        if (m_since < 1.0 / 30.0) return;
        m_since = 0.0;
        float a[6];
        for (int h = 0; h < 2; ++h) {
            const Hands::Hand& s = hands.hand(h);
            a[h] = s.m_height;
            a[2 + h] = s.m_valid && s.m_closed ? 1.0f : 0.0f;
            a[4 + h] = s.m_valid ? 1.0f : 0.0f;
        }
        char buf[64];
        size_t pos = 0;
        auto putStr = [&](const char* t) {
            const size_t l = std::strlen(t) + 1;
            std::memcpy(buf + pos, t, l);
            pos += l;
            while (pos & 3) buf[pos++] = 0;
        };
        putStr("/hands");
        putStr(",ffffff");
        for (const float f : a) {   // big-endian 32-bit words
            uint32_t u;
            std::memcpy(&u, &f, 4);
            buf[pos++] = static_cast<char>(u >> 24);
            buf[pos++] = static_cast<char>(u >> 16);
            buf[pos++] = static_cast<char>(u >> 8);
            buf[pos++] = static_cast<char>(u);
        }
        ::sendto(m_sock, buf, pos, 0, reinterpret_cast<const sockaddr*>(&m_to), sizeof(m_to));
    }

private:
    int m_sock = -1;        ///< the UDP socket, or -1 while closed
    sockaddr_in m_to{};     ///< the desktop's address and port
    double m_since = 0.0;   ///< seconds since the last datagram
};

// ---------------------------------------------------------------- the app

/** @brief One eye's swapchain and the framebuffer it is rendered through. */
struct SwapchainTarget {
    XrSwapchain m_swapchain = XR_NULL_HANDLE;   ///< the eye's swapchain
    int m_width = 0;   ///< its width, pixels
    int m_height = 0;   ///< its height, pixels
    std::vector<XrSwapchainImageOpenGLESKHR> m_images;   ///< the swapchain's images
    GLuint m_fbo = 0;   ///< the framebuffer drawn through
    GLuint m_depth = 0;   ///< its depth buffer
};

/** @brief The Quest app: OpenXR session, hands, panel, and the track player with its composer thread and audio stream. */
class App {
public:
    /** @brief The app on the native activity @p app. */
    explicit App(android_app* app) : m_app(app) {}

    /**
     * @brief Reads tot.cfg, opens the cue sender and the bridge, sets up OpenXR, EGL, the hands, the scene and the audio
     *        stream, and starts the composer thread; false where something essential failed.
     */
    bool init()
    {
        m_dataDir = m_app->activity->externalDataPath ? m_app->activity->externalDataPath : "";
        m_config = readConfig(m_dataDir.c_str());
        // The cue bridge of PLAN 10.3, off unless tot.cfg names a host: a visualiser that is not there changes nothing.
        if (!m_config.m_oscHost.empty()) {
            if (m_cues.start(m_config.m_oscHost, m_config.m_oscPort)) {
                LOGI("cues: OSC to %s:%d", m_config.m_oscHost.c_str(), m_config.m_oscPort);
                m_player.setCueSender(&m_cues);
            } else LOGE("cues: cannot reach %s:%d", m_config.m_oscHost.c_str(), m_config.m_oscPort);
        }
        // The bridge (01.10.2026): the hands to the desktop's Totality, off unless tot.cfg names a host.
        if (!m_config.m_bridgeHost.empty()) {
            if (m_bridge.open(m_config.m_bridgeHost, m_config.m_bridgePort))
                LOGI("bridge: the hands to %s:%d", m_config.m_bridgeHost.c_str(), m_config.m_bridgePort);
            else LOGE("bridge: bad host %s (a dotted IPv4 address)", m_config.m_bridgeHost.c_str());
        }
        if (!initLoader()) return false;
        if (!initInstance()) return false;
        if (!initEgl()) return false;
        if (!initSession()) return false;
        if (!initHands()) LOGE("hand tracking unavailable: the hands will not play");
        if (!m_scene.init()) return false;
        // The stream is opened first so the engine is prepared for the rate the device really gives.
        if (!m_audio.open()) return false;
        m_composerThread = std::thread([this] { composerLoop(); });
        return true;
    }

    /** @brief The main loop: Android's events, OpenXR's events, a frame while the session runs; until the app is closed. */
    void run()
    {
        while (!m_app->destroyRequested) {
            int events;
            android_poll_source* source;
            const int timeout = (m_sessionRunning || m_app->window == nullptr) ? 0 : -1;
            while (ALooper_pollOnce(timeout, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0) {
                if (source) source->process(m_app, source);
                if (m_app->destroyRequested) break;
            }
            pollXrEvents();
            if (m_quit) break;
            if (m_sessionRunning) frame();
        }
    }

    /** @brief Stops the composer thread and the stream, then destroys what OpenXR made. */
    void shutdown()
    {
        m_stopComposer.store(true);
        m_player.quit();
        if (m_composerThread.joinable()) m_composerThread.join();
        m_audio.stop();
        m_cues.stop();   // after the stream: the sender's thread reads the ring the audio thread fills
        for (SwapchainTarget& t : m_targets) if (t.m_swapchain != XR_NULL_HANDLE) xrDestroySwapchain(t.m_swapchain);
        for (int h = 0; h < 2; ++h)
            if (m_handTracker[h] != XR_NULL_HANDLE && m_pfnDestroyHandTracker) m_pfnDestroyHandTracker(m_handTracker[h]);
        if (m_viewSpace != XR_NULL_HANDLE) xrDestroySpace(m_viewSpace);
        if (m_stageSpace != XR_NULL_HANDLE) xrDestroySpace(m_stageSpace);
        if (m_session != XR_NULL_HANDLE) xrDestroySession(m_session);
        if (m_instance != XR_NULL_HANDLE) xrDestroyInstance(m_instance);
    }

private:
    // ------------------------------------------------ the composer thread

    /**
     * @brief Composes and loads the first track (or set), starts the stream, then waits for "next".
     *
     * A track is composed whole before it plays (Composer.h): a few seconds on the headset's small cores,
     * during which the panel says COMPOSING and the audio stream is not yet started.
     */
    void composerLoop()
    {
        m_player.prepare(m_audio.sampleRate(), m_audio.burst() * 2, m_config);
        const ParamStore& p = m_player.params();
        m_filterId = p.id(ModuleE::PERFORM, 0, perform::FILTER);
        m_throwId = p.id(ModuleE::PERFORM, 0, perform::THROW);
        m_muteKickId = p.id(ModuleE::PERFORM, 0, perform::MUTE_KICK);
        if (!m_audio.start()) LOGE("Oboe: cannot start");
        else LOGI("audio started%s", m_config.m_mute ? " (muted)" : "");
        while (!m_stopComposer.load()) {
            m_player.pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    // ------------------------------------------------ OpenXR setup

    /** @brief Initialises Android's OpenXR loader. */
    bool initLoader()
    {
        PFN_xrInitializeLoaderKHR initLoader = nullptr;
        xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", reinterpret_cast<PFN_xrVoidFunction*>(&initLoader));
        if (initLoader == nullptr) { LOGE("no xrInitializeLoaderKHR"); return false; }
        XrLoaderInitInfoAndroidKHR li{ XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR };
        li.applicationVM = m_app->activity->vm;
        li.applicationContext = m_app->activity->clazz;
        return XR_SUCCEEDED(initLoader(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&li)));
    }

    /** @brief Creates the OpenXR instance with GLES and hand tracking and finds the headset; notes whether it tracks hands. */
    bool initInstance()
    {
        const char* exts[] = { XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
                               XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
                               XR_EXT_HAND_TRACKING_EXTENSION_NAME };
        XrInstanceCreateInfoAndroidKHR android{ XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR };
        android.applicationVM = m_app->activity->vm;
        android.applicationActivity = m_app->activity->clazz;
        XrInstanceCreateInfo ci{ XR_TYPE_INSTANCE_CREATE_INFO, &android };
        std::strncpy(ci.applicationInfo.applicationName, "Totality", XR_MAX_APPLICATION_NAME_SIZE - 1);
        ci.applicationInfo.applicationVersion = 1;
        std::strncpy(ci.applicationInfo.engineName, "TotalityCore", XR_MAX_ENGINE_NAME_SIZE - 1);
        ci.applicationInfo.apiVersion = XR_API_VERSION_1_0;
        ci.enabledExtensionCount = 3;
        ci.enabledExtensionNames = exts;
        if (XR_FAILED(xrCreateInstance(&ci, &m_instance))) { LOGE("xrCreateInstance failed"); return false; }

        XrSystemGetInfo sgi{ XR_TYPE_SYSTEM_GET_INFO };
        sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        if (XR_FAILED(xrGetSystem(m_instance, &sgi, &m_system))) { LOGE("xrGetSystem failed"); return false; }
        XrSystemHandTrackingPropertiesEXT ht{ XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT };
        XrSystemProperties sp{ XR_TYPE_SYSTEM_PROPERTIES, &ht };
        xrGetSystemProperties(m_instance, m_system, &sp);
        m_handsSupported = ht.supportsHandTracking == XR_TRUE;
        LOGI("system: %s, hand tracking %d", sp.systemName, m_handsSupported ? 1 : 0);
        return true;
    }

    /** @brief An EGL context for GLES 3 on a small pbuffer, as OpenXR's GLES binding needs. */
    bool initEgl()
    {
        PFN_xrGetOpenGLESGraphicsRequirementsKHR getReq = nullptr;
        xrGetInstanceProcAddr(m_instance, "xrGetOpenGLESGraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&getReq));
        XrGraphicsRequirementsOpenGLESKHR req{ XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR };
        if (getReq == nullptr || XR_FAILED(getReq(m_instance, m_system, &req))) { LOGE("GLES requirements failed"); return false; }

        m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        EGLint major = 0, minor = 0;
        if (!eglInitialize(m_display, &major, &minor)) { LOGE("eglInitialize failed"); return false; }
        const EGLint attribs[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                                   EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                                   EGL_DEPTH_SIZE, 0, EGL_NONE };
        EGLint count = 0;
        if (!eglChooseConfig(m_display, attribs, &m_eglConfig, 1, &count) || count == 0) { LOGE("eglChooseConfig failed"); return false; }
        const EGLint pbuf[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
        m_surface = eglCreatePbufferSurface(m_display, m_eglConfig, pbuf);
        const EGLint ctx[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
        m_context = eglCreateContext(m_display, m_eglConfig, EGL_NO_CONTEXT, ctx);
        if (m_context == EGL_NO_CONTEXT || !eglMakeCurrent(m_display, m_surface, m_surface, m_context)) { LOGE("EGL context failed"); return false; }
        LOGI("EGL %d.%d, GL %s", major, minor, glGetString(GL_VERSION));
        return true;
    }

    /** @brief The session, the room (stage, else local) and head spaces, and a swapchain with framebuffer and depth per eye. */
    bool initSession()
    {
        XrGraphicsBindingOpenGLESAndroidKHR gb{ XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR };
        gb.display = m_display; gb.config = m_eglConfig; gb.context = m_context;
        XrSessionCreateInfo sci{ XR_TYPE_SESSION_CREATE_INFO, &gb };
        sci.systemId = m_system;
        if (XR_FAILED(xrCreateSession(m_instance, &sci, &m_session))) { LOGE("xrCreateSession failed"); return false; }

        XrReferenceSpaceCreateInfo rs{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
        rs.poseInReferenceSpace.orientation.w = 1.0f;
        rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
        if (XR_FAILED(xrCreateReferenceSpace(m_session, &rs, &m_stageSpace))) {
            rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
            if (XR_FAILED(xrCreateReferenceSpace(m_session, &rs, &m_stageSpace))) { LOGE("no reference space"); return false; }
        }
        rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        xrCreateReferenceSpace(m_session, &rs, &m_viewSpace);

        uint32_t viewCount = 0;
        xrEnumerateViewConfigurationViews(m_instance, m_system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
        std::vector<XrViewConfigurationView> cfg(viewCount, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
        xrEnumerateViewConfigurationViews(m_instance, m_system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount, cfg.data());
        m_views.assign(viewCount, { XR_TYPE_VIEW });

        uint32_t fmtCount = 0;
        xrEnumerateSwapchainFormats(m_session, 0, &fmtCount, nullptr);
        std::vector<int64_t> formats(fmtCount);
        xrEnumerateSwapchainFormats(m_session, fmtCount, &fmtCount, formats.data());
        int64_t format = formats.empty() ? GL_RGBA8 : formats[0];
        for (int64_t f : formats) if (f == GL_SRGB8_ALPHA8) { format = f; break; }

        m_targets.resize(viewCount);
        for (uint32_t v = 0; v < viewCount; ++v) {
            SwapchainTarget& t = m_targets[v];
            XrSwapchainCreateInfo sc{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
            sc.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
            sc.format = format;
            sc.sampleCount = 1;
            sc.width = cfg[v].recommendedImageRectWidth;
            sc.height = cfg[v].recommendedImageRectHeight;
            sc.faceCount = 1; sc.arraySize = 1; sc.mipCount = 1;
            if (XR_FAILED(xrCreateSwapchain(m_session, &sc, &t.m_swapchain))) { LOGE("xrCreateSwapchain failed"); return false; }
            t.m_width = static_cast<int>(sc.width);
            t.m_height = static_cast<int>(sc.height);
            uint32_t imgCount = 0;
            xrEnumerateSwapchainImages(t.m_swapchain, 0, &imgCount, nullptr);
            t.m_images.assign(imgCount, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR });
            xrEnumerateSwapchainImages(t.m_swapchain, imgCount, &imgCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(t.m_images.data()));
            glGenFramebuffers(1, &t.m_fbo);
            glGenRenderbuffers(1, &t.m_depth);
            glBindRenderbuffer(GL_RENDERBUFFER, t.m_depth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, t.m_width, t.m_height);
            LOGI("view %u: %dx%d, %u images", v, t.m_width, t.m_height, imgCount);
        }
        return true;
    }

    /** @brief A hand tracker per hand; false where the headset or runtime does not track hands. */
    bool initHands()
    {
        if (!m_handsSupported) return false;
        xrGetInstanceProcAddr(m_instance, "xrCreateHandTrackerEXT", reinterpret_cast<PFN_xrVoidFunction*>(&m_pfnCreateHandTracker));
        xrGetInstanceProcAddr(m_instance, "xrLocateHandJointsEXT", reinterpret_cast<PFN_xrVoidFunction*>(&m_pfnLocateHandJoints));
        xrGetInstanceProcAddr(m_instance, "xrDestroyHandTrackerEXT", reinterpret_cast<PFN_xrVoidFunction*>(&m_pfnDestroyHandTracker));
        if (!m_pfnCreateHandTracker || !m_pfnLocateHandJoints) return false;
        for (int h = 0; h < 2; ++h) {
            XrHandTrackerCreateInfoEXT hci{ XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT };
            hci.hand = (h == 0) ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT;
            hci.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
            if (XR_FAILED(m_pfnCreateHandTracker(m_session, &hci, &m_handTracker[h]))) return false;
        }
        return true;
    }

    /** @brief OpenXR's events: begins and ends the session as its state asks, and notes when the app is to quit. */
    void pollXrEvents()
    {
        XrEventDataBuffer ev{ XR_TYPE_EVENT_DATA_BUFFER };
        while (xrPollEvent(m_instance, &ev) == XR_SUCCESS) {
            if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                const auto* sc = reinterpret_cast<const XrEventDataSessionStateChanged*>(&ev);
                m_sessionState = sc->state;
                switch (m_sessionState) {
                case XR_SESSION_STATE_READY: {
                    XrSessionBeginInfo bi{ XR_TYPE_SESSION_BEGIN_INFO };
                    bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    if (XR_SUCCEEDED(xrBeginSession(m_session, &bi))) m_sessionRunning = true;
                    break;
                }
                case XR_SESSION_STATE_STOPPING:
                    xrEndSession(m_session);
                    m_sessionRunning = false;
                    break;
                case XR_SESSION_STATE_EXITING:
                case XR_SESSION_STATE_LOSS_PENDING:
                    m_quit = true;
                    break;
                default: break;
                }
            } else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
                m_quit = true;
            }
            ev = { XR_TYPE_EVENT_DATA_BUFFER };
        }
    }

    // ------------------------------------------------ per frame

    /** @brief The head pose at @p time (m_headPose, m_headValid). */
    void updateHead(XrTime time)
    {
        m_headValid = false;
        if (m_viewSpace == XR_NULL_HANDLE) return;
        XrSpaceLocation loc{ XR_TYPE_SPACE_LOCATION };
        if (XR_FAILED(xrLocateSpace(m_viewSpace, m_stageSpace, time, &loc))) return;
        const XrSpaceLocationFlags need = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        if ((loc.locationFlags & need) != need) return;
        m_headPose = loc.pose;
        m_headValid = true;
    }

    /** @brief Locates both hands at @p time and feeds their palms and pinches to m_hands. */
    void updateHands(XrTime time)
    {
        for (int h = 0; h < 2; ++h) {
            if (m_handTracker[h] == XR_NULL_HANDLE) { m_hands.lost(h); continue; }
            XrHandJointLocationEXT joints[XR_HAND_JOINT_COUNT_EXT];
            XrHandJointLocationsEXT locs{ XR_TYPE_HAND_JOINT_LOCATIONS_EXT };
            locs.jointCount = XR_HAND_JOINT_COUNT_EXT;
            locs.jointLocations = joints;
            XrHandJointsLocateInfoEXT li{ XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT };
            li.baseSpace = m_stageSpace;
            li.time = time;
            if (XR_FAILED(m_pfnLocateHandJoints(m_handTracker[h], &li, &locs)) || !locs.isActive) { m_hands.lost(h); continue; }
            const XrHandJointLocationEXT& palm = joints[XR_HAND_JOINT_PALM_EXT];
            const XrHandJointLocationEXT& thumb = joints[XR_HAND_JOINT_THUMB_TIP_EXT];
            const XrHandJointLocationEXT& index = joints[XR_HAND_JOINT_INDEX_TIP_EXT];
            const XrSpaceLocationFlags need = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
            if ((palm.locationFlags & need) != need) { m_hands.lost(h); continue; }
            const float dx = thumb.pose.position.x - index.pose.position.x;
            const float dy = thumb.pose.position.y - index.pose.position.y;
            const float dz = thumb.pose.position.z - index.pose.position.z;
            m_hands.setHand(h, palm.pose.position, std::sqrt(dx * dx + dy * dy + dz * dz),
                           m_headValid ? m_headPose.position.y : 1.6f);
        }
    }

    /**
     * @brief Turns the hands' heights into the perform controls (Params.h, perform).
     *
     * Left height is the master filter over its whole range: mid height open (the composed sound), lower a low pass,
     * higher a high pass, as a DJ mixer's filter knob. Right height is the echo throw: nothing below mid height, all of
     * it with the hand raised.
     */
    void applyMacros()
    {
        if (!m_player.ready() || m_filterId < 0) return;
        ParamStore& p = m_player.params();
        // A hand that has never been tracked writes nothing: without hand tracking the controls keep what tot.cfg set.
        const Hands::Hand& left = m_hands.hand(0);
        const Hands::Hand& right = m_hands.hand(1);
        if (left.m_everSeen) {
            // A dead zone round the middle, so a resting hand leaves the filter open.
            const float x = 2.0f * (left.m_macro - 0.5f);
            const float y = std::fabs(x) < 0.1f ? 0.0f : (x - (x > 0.0f ? 0.1f : -0.1f)) / 0.9f;
            p.set(m_filterId, std::clamp(y, -1.0f, 1.0f));
        }
        if (right.m_everSeen) p.set(m_throwId, clamp01(2.0f * (right.m_macro - 0.5f)));
    }

    /**
     * @brief Geometry of the panel.
     *
     * A glyph is 7 cells high and a character 6 cells wide, so at Cell and PanelDistance a line of
     * 20 characters spans 0.50 m -- about 29 degrees -- and a glyph stands 1.7 degrees tall, roughly
     * print at reading distance. Every line drawn below stays inside 20 characters. These are design
     * values and untuned: nobody has put the headset on yet.
     */
    static constexpr float Cell = 0.0042f;         ///< pitch of one font pixel, metres
    static constexpr float Row = 0.040f;           ///< distance between text rows, metres
    static constexpr float PanelDistance = 1.0f;   ///< how far in front of the eyes the panel sits

    /** @brief The head-locked panel: in front of the eyes, following the yaw only. */
    Panel headPanel() const
    {
        Panel p;
        const Vec3 fwd = rotate(m_headPose.orientation, { 0.0f, 0.0f, -1.0f });
        const float len = std::fmax(std::sqrt(fwd.m_x * fwd.m_x + fwd.m_z * fwd.m_z), 1.0e-3f);
        const Vec3 f{ fwd.m_x / len, 0.0f, fwd.m_z / len };
        p.m_right = { -f.m_z, 0.0f, f.m_x };
        p.m_up = { 0.0f, 1.0f, 0.0f };
        // Shifted half a 16-character line to the left (the lines are left aligned and 13 to 20
        // characters long), so the block of text sits roughly centred in front of the eyes.
        const float half = 0.5f * 16.0f * 6.0f * Cell;
        p.m_origin = { m_headPose.position.x + f.m_x * PanelDistance - p.m_right.m_x * half,
                     m_headPose.position.y + 0.12f,
                     m_headPose.position.z + f.m_z * PanelDistance - p.m_right.m_z * half };
        return p;
    }

    /** @brief The block at @p beat as the composer named it ("Intro", "Block 3", "Reduction", a set's track), on @p deck. */
    static const char* sectionAt(const Score& s, double beat)
    {
        const char* text = "";
        for (const Marker& m : s.m_markers) { if (m.m_beat > beat) break; text = m.m_text.c_str(); }
        return text;
    }

    /** @brief One part's onsets in a bar: position in the bar 0..1 and velocity. */
    struct Beads {
        int m_part = 0;                                    ///< the part (Part)
        std::vector<std::pair<float, float>> m_onsets;     ///< position in the bar 0..1 and velocity of each onset
    };

    /** @brief The onsets of bar @p bar of @p s, per part (the kick apart), in part order. */
    static void gatherBar(const Score& s, int bar, std::vector<Beads>& rings, std::vector<float>& kick)
    {
        rings.clear();
        kick.clear();
        if (bar < 0) return;
        const double b0 = 4.0 * bar, b1 = b0 + 4.0;
        auto it = std::lower_bound(s.m_notes.begin(), s.m_notes.end(), b0 - 0.1, [](const NoteEvent& n, double b) { return n.m_beat < b; });
        int index[64];
        for (int& i : index) i = -1;
        for (; it != s.m_notes.end() && it->m_beat < b1 - 0.1; ++it) {
            if (it->m_velocity <= 0.0f) continue;
            const float pos = static_cast<float>(std::clamp((it->m_beat - b0) / 4.0, 0.0, 0.9999));
            if (it->m_part == PartE::KICK) { kick.push_back(pos); continue; }
            const int p = static_cast<int>(it->m_part);
            if (p < 0 || p >= 64) continue;
            if (index[p] < 0) { index[p] = static_cast<int>(rings.size()); rings.push_back({ p, {} }); }
            rings[static_cast<size_t>(index[p])].m_onsets.push_back({ pos, it->m_velocity });
        }
        std::sort(rings.begin(), rings.end(), [](const Beads& a, const Beads& b) { return a.m_part < b.m_part; });
    }

    /** @brief A part's colour in the room: the drums the corona's warm white, the tones their families. */
    static void partColour(int part, float& r, float& g, float& b)
    {
        switch (static_cast<PartE>(part)) {
        case PartE::SUB: r = 0.44f; g = 0.56f; b = 0.72f; return;
        case PartE::PING: r = 0.44f; g = 0.72f; b = 0.68f; return;
        case PartE::BASS: r = 0.85f; g = 0.51f; b = 0.36f; return;
        case PartE::ACID: r = 0.84f; g = 0.33f; b = 0.25f; return;
        case PartE::CHORD: case PartE::DRONE: case PartE::TEXTURE: r = 0.62f; g = 0.75f; b = 0.44f; return;
        default: r = 0.93f; g = 0.84f; b = 0.66f; return;
        }
    }

    /**
     * @brief The logo (Deploy/make_icon.py) in points of light: the rim round the dark disc and the corona's streamers,
     *        long along the equator, each a line of points fading outwards; centred at (@p u, @p v), @p size across.
     */
    void addLogo(const Panel& p, float u, float v, float size)
    {
        const float disc = 0.2f * size;
        for (int i = 0; i < 40; ++i) {
            const float a = 6.2831853f * static_cast<float>(i) / 40.0f;
            m_scene.addOn(p, u + 1.06f * disc * std::cos(a), v + 1.06f * disc * std::sin(a), 0.93f, 0.84f, 0.66f, 0.95f, 26.0f);
        }
        for (int i = 0; i < 32; ++i) {
            const float a = 6.2831853f * static_cast<float>(i) / 32.0f;
            const float w = 0.55f + 0.45f * std::sin(7.0f * a + 1.3f) * std::sin(3.0f * a);
            const float e = std::pow(std::abs(std::cos(a)), 3.0f);
            const float length = disc * (0.18f + 0.95f * e * (0.6f + 0.4f * w) + 0.12f * w);
            const float bright = 0.45f + 0.5f * std::abs(std::cos(a));
            const int steps = 1 + static_cast<int>(length / (0.12f * disc));
            for (int k = 0; k < steps; ++k) {
                const float t = (static_cast<float>(k) + 0.5f) / static_cast<float>(steps);
                const float r = disc * 1.14f + length * t;
                m_scene.addOn(p, u + r * std::cos(a), v + r * std::sin(a), 0.93f, 0.84f, 0.66f, bright * (1.0f - 0.6f * t), 18.0f);
            }
        }
    }

    /**
     * @brief The Eclipse in the room (the file comment): ahead of the player and above, tilted towards them, anchored
     *        where the head was when the session began -- it never moves with the head or the audio.
     */
    void addEclipse(const Now& now, double beat)
    {
        if (!m_eclipseAnchored) return;
        const int t = now.trackAt(beat);
        const Score& s = now.m_set.m_decks[now.m_decks.empty() ? 0 : now.m_decks[static_cast<size_t>(t)]];
        const int bar = static_cast<int>(std::floor(beat / 4.0));
        const float phase = static_cast<float>(beat / 4.0 - std::floor(beat / 4.0));
        // The bar that plays and the one before, crossfaded over the first eighth of the bar: no bead pops in.
        std::vector<Beads> rings[2];
        std::vector<float> kicks[2];
        gatherBar(s, bar, rings[0], kicks[0]);
        gatherBar(s, bar - 1, rings[1], kicks[1]);
        const float fadeIn = std::min(1.0f, phase / 0.125f);
        const float weight[2] = { 0.5f - 0.5f * std::cos(3.14159265f * fadeIn), 0.5f + 0.5f * std::cos(3.14159265f * fadeIn) };
        const Panel& e = m_eclipse;
        const float disc = 0.34f, inner = 0.62f, outer = 1.45f;
        // The corona swells after every kick of the bar: a decaying raised cosine of the distance to it, continuous.
        float swell = 0.0f;
        for (float k : kicks[0]) {
            const float d = phase - k;
            if (d >= 0.0f && d < 0.2f) swell = std::max(swell, 0.5f + 0.5f * std::cos(3.14159265f * d / 0.2f));
        }
        for (int k = 0; k < 4; ++k) {
            const float r = disc * (1.04f + 0.12f * static_cast<float>(k)) + 0.03f * swell;
            const int dots = 120;
            for (int i = 0; i < dots; ++i) {
                const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(dots);
                m_scene.addOn(e, r * std::cos(a), r * std::sin(a), 0.93f, 0.84f, 0.66f, (0.55f - 0.12f * static_cast<float>(k)) * (0.6f + 0.4f * swell), 34.0f);
            }
        }
        const auto angleOf = [](float pos) { return 1.5707963f - 6.2831853f * pos; };   // bar start at the top, clockwise
        // Conjunctions of the bar that plays: three rings or more on one sixteenth, a ray.
        int count[16] = {};
        for (const Beads& r : rings[0]) {
            bool seen[16] = {};
            for (const auto& o : r.m_onsets) {
                const int step = std::clamp(static_cast<int>(std::lround(o.first * 16.0f)) % 16, 0, 15);
                if (!seen[step]) { seen[step] = true; ++count[step]; }
            }
        }
        for (int st = 0; st < 16; ++st) {
            if (count[st] < 3) continue;
            const float a = angleOf(static_cast<float>(st) / 16.0f);
            for (int i = 0; i < 18; ++i) {
                const float r = disc * 1.5f + (outer - disc * 1.5f) * static_cast<float>(i) / 17.0f;
                m_scene.addOn(e, r * std::cos(a), r * std::sin(a), 0.93f, 0.84f, 0.66f, 0.06f * static_cast<float>(count[st]) * weight[0], 60.0f);
            }
        }
        // The rings and their beads.
        for (int w = 0; w < 2; ++w) {
            const int n = static_cast<int>(rings[w].size());
            for (int i = 0; i < n; ++i) {
                const Beads& r = rings[w][static_cast<size_t>(i)];
                const float rad = inner + (outer - inner) * (n <= 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(n - 1));
                float cr, cg, cb;
                partColour(r.m_part, cr, cg, cb);
                if (w == 0)
                    for (int d = 0; d < 72; ++d) {
                        const float a = 6.2831853f * static_cast<float>(d) / 72.0f;
                        m_scene.addOn(e, rad * std::cos(a), rad * std::sin(a), cr, cg, cb, 0.07f, 18.0f);
                    }
                for (const auto& o : r.m_onsets) {
                    const float a = angleOf(o.first);
                    const float d = w == 0 ? phase - o.first : 1.0f;
                    const float lit = d >= 0.0f && d < 0.1f ? 0.5f + 0.5f * std::cos(3.14159265f * d / 0.1f) : 0.0f;
                    m_scene.addOn(e, rad * std::cos(a), rad * std::sin(a), cr, cg, cb, weight[w] * (0.35f + 0.3f * o.second + 0.35f * lit),
                                 60.0f + 50.0f * o.second + 60.0f * lit);
                }
            }
        }
        // The playhead's hand.
        const float ha = angleOf(phase);
        for (int i = 0; i < 24; ++i) {
            const float r = disc * 1.3f + (outer - disc * 1.3f) * static_cast<float>(i) / 23.0f;
            m_scene.addOn(e, r * std::cos(ha), r * std::sin(ha), 0.84f, 0.33f, 0.25f, 0.35f, 26.0f);
        }
    }

    /** @brief Anchors the Eclipse at the first head pose: 3 m ahead, 1.3 m above the eyes, tilted 40 degrees down. */
    void anchorEclipse()
    {
        if (m_eclipseAnchored || !m_headValid) return;
        const Vec3 fwd = rotate(m_headPose.orientation, { 0.0f, 0.0f, -1.0f });
        const float len = std::fmax(std::sqrt(fwd.m_x * fwd.m_x + fwd.m_z * fwd.m_z), 1.0e-3f);
        const Vec3 f{ fwd.m_x / len, 0.0f, fwd.m_z / len };
        const float tilt = 0.70f;   // radians the disc leans towards the player
        m_eclipse.m_right = { -f.m_z, 0.0f, f.m_x };
        // "Up" on the disc: up, leaning back away from the player, so its face looks down at them.
        m_eclipse.m_up = { f.m_x * std::sin(tilt), std::cos(tilt), f.m_z * std::sin(tilt) };
        m_eclipse.m_origin = { m_headPose.position.x + f.m_x * 3.0f, m_headPose.position.y + 1.3f, m_headPose.position.z + f.m_z * 3.0f };
        m_eclipseAnchored = true;
    }

    /** @brief The performer panel (PLAN 10.2): the track, its block and key, the time, the level, the hands' controls. */
    void buildScene()
    {
        // The floor ring: a fixed horizon, never moved by the audio.
        for (int i = 0; i < 72; ++i) {
            const float a = static_cast<float>(i) / 72.0f * 6.2831853f;
            m_scene.add(2.5f * std::sin(a), 0.02f, -2.5f * std::cos(a), 0.30f, 0.27f, 0.22f, 0.4f, 40.0f);
        }
        for (int h = 0; h < 2; ++h) {
            const Hands::Hand& s = m_hands.hand(h);
            if (!s.m_valid) continue;
            const float t = s.m_pinch;
            m_scene.add(s.m_palm.x, s.m_palm.y, s.m_palm.z, 0.93f, 0.84f - 0.4f * t, 0.66f - 0.4f * t, 1.0f, 130.0f);
        }
        if (!m_headValid) return;
        anchorEclipse();

        const Panel p = headPanel();
        char line[64];
        if (!m_player.ready()) {
            addLogo(p, 0.178f, 0.10f, 0.12f);         // centred over the name, 0.355 m wide at 1.8 cells
            m_scene.addText(p, 0.0f, 0.0f, Cell * 1.8f, "TOTALITY", 0.93f, 0.84f, 0.66f, 1.0f);
            m_scene.addText(p, 0.0f, -0.08f, Cell, "COMPOSING", 0.6f, 0.7f, 0.9f, 0.8f);
            return;
        }
        const std::shared_ptr<const Now> np = m_player.now();
        if (!np) return;
        const Now& now = *np;
        const double beat = m_player.beat();
        const ParamStore& par = m_player.params();
        addEclipse(now, beat);

        // The title line above the rest: the logo and the name.
        addLogo(p, 0.012f, Row + 0.006f, 0.034f);
        m_scene.addText(p, 0.036f, Row, Cell, "TOTALITY", 0.93f, 0.84f, 0.66f, 0.85f);
        float row = 0.0f;
        auto text = [&](const char* t, float r, float g, float b, float a) {
            m_scene.addText(p, 0.0f, row, Cell, t, r, g, b, a);
            row -= Row;
        };
        const int t = now.trackAt(beat);
        const TrackInfo& info = now.m_tracks[static_cast<size_t>(t)];
        if (now.m_isSet) std::snprintf(line, sizeof(line), "SET %d  T%d/%d", m_player.number(), t + 1, static_cast<int>(now.m_tracks.size()));
        else std::snprintf(line, sizeof(line), "TRACK %d", m_player.number());
        text(line, 0.93f, 0.84f, 0.66f, 1.0f);
        std::snprintf(line, sizeof(line), "%s %s", info.m_style.c_str(), FormNames[static_cast<int>(info.m_form)]);
        text(line, 0.93f, 0.84f, 0.66f, 0.9f);
        const Score& deck = now.m_set.m_decks[now.m_decks[static_cast<size_t>(t)]];
        text(sectionAt(deck, beat), 0.93f, 0.78f, 0.55f, 0.9f);
        std::snprintf(line, sizeof(line), "%s %s %s", KeyNames[info.m_key], ScaleNames[info.m_scale], info.m_camelot.c_str());
        text(line, 0.70f, 0.85f, 1.00f, 0.9f);
        // The composer's presets (Presets.h) of the kick and the ping, as the plugin's pages name them.
        for (const auto& [m, label] : { std::pair<ModuleE, const char*>{ ModuleE::KICK, "KICK" }, std::pair<ModuleE, const char*>{ ModuleE::PING, "PING" } }) {
            int index = -1;
            for (const SoundPick& k : deck.m_sounds)
                if (k.m_module == static_cast<int>(m) && k.m_instance == 0 && k.m_beat <= beat + 1.0) index = k.m_preset;
            if (index < 0) continue;
            std::snprintf(line, sizeof(line), "%s %s", label, factoryPresets(m)[static_cast<size_t>(index)].m_name.c_str());
            text(line, 0.62f, 0.75f, 0.44f, 0.8f);
        }
        const TempoMap& tm = now.m_set.m_decks[0].m_tempo;
        const double sec = m_player.seconds(), total = tm.secondsAt(now.m_set.m_lengthBeats);
        std::snprintf(line, sizeof(line), "%02d:%02d/%02d:%02d %.0f BPM", static_cast<int>(sec) / 60, static_cast<int>(sec) % 60,
                      static_cast<int>(total) / 60, static_cast<int>(total) % 60, tm.bpmAt(beat));
        text(line, 0.93f, 0.84f, 0.60f, 0.9f);
        const float db = m_player.levelDb();
        std::snprintf(line, sizeof(line), "%+.1f DB  %s", static_cast<double>(std::max(db, -99.0f)),
                      m_player.muted() ? "MUTED" : (m_player.playing() ? "PLAY" : "STOP"));
        text(line, 0.93f, 0.84f, 0.60f, 0.9f);
        const float f = par.get(m_filterId);
        if (std::fabs(f) < 0.01f) std::snprintf(line, sizeof(line), "FILTER OPEN");
        else std::snprintf(line, sizeof(line), "FILTER %s %d%%", f < 0.0f ? "LP" : "HP", static_cast<int>(std::lround(std::fabs(f) * 100.0f)));
        text(line, 0.60f, 0.75f, 0.95f, 0.75f);
        std::snprintf(line, sizeof(line), "THROW %d%%", static_cast<int>(std::lround(par.get(m_throwId) * 100.0f)));
        text(line, 0.60f, 0.75f, 0.95f, 0.75f);
        if (par.getBool(m_muteKickId)) text("KICK OUT", 0.84f, 0.33f, 0.25f, 1.0f);

        row -= Row * 0.3f;
        // Four beat lamps: a raised cosine of the distance to the beat, a continuous pulse.
        const double inBar = beat / 4.0 - std::floor(beat / 4.0);
        const float phase = static_cast<float>(inBar) * 4.0f;
        for (int b = 0; b < 4; ++b) {
            float dist = phase - static_cast<float>(b);
            if (dist < 0.0f) dist += 4.0f;
            const float x = dist < 1.0f ? 0.5f + 0.5f * std::cos(3.14159265f * dist) : 0.0f;
            const float level = m_player.playing() ? 0.22f + 0.78f * x : 0.18f;
            m_scene.addOn(p, 0.026f * static_cast<float>(b), row, 1.0f, 0.55f + 0.35f * x, 0.25f, level, 150.0f);
        }
    }

    /**
     * @brief The pinches: a pinch acts when it opens again, so both hands closed together can mean something else --
     *        the next track, once, while neither of them then counts as a single pinch. Left alone: play / stop;
     *        right alone: kick out / kick in.
     */
    void gestures()
    {
        const bool closed[2] = { m_hands.hand(0).m_closed, m_hands.hand(1).m_closed };
        if (closed[0] && closed[1]) {
            m_spoiled[0] = m_spoiled[1] = true;
            if (!m_bothFired) { m_player.requestNext(); m_bothFired = true; }
        }
        for (int h = 0; h < 2; ++h) {
            if (closed[h] && !m_handWas[h] && !closed[1 - h]) m_spoiled[h] = false;
            if (!closed[h] && m_handWas[h] && !m_spoiled[h]) {
                if (h == 0) m_player.setPlaying(!m_player.playing());
                else m_player.params().set(m_muteKickId, m_player.params().getBool(m_muteKickId) ? 0.0f : 1.0f);
            }
            m_handWas[h] = closed[h];
        }
        if (!closed[0] && !closed[1]) m_bothFired = false;
    }

    /**
     * @brief One frame: head and hands, the gestures and controls, then the scene drawn for both eyes and handed to the
     *        compositor.
     */
    void frame()
    {
        XrFrameWaitInfo wi{ XR_TYPE_FRAME_WAIT_INFO };
        XrFrameState fs{ XR_TYPE_FRAME_STATE };
        if (XR_FAILED(xrWaitFrame(m_session, &wi, &fs))) return;
        XrFrameBeginInfo bi{ XR_TYPE_FRAME_BEGIN_INFO };
        xrBeginFrame(m_session, &bi);

        const double dt = m_lastTime == 0 ? 1.0 / 72.0 : static_cast<double>(fs.predictedDisplayTime - m_lastTime) * 1.0e-9;
        m_lastTime = fs.predictedDisplayTime;
        updateHead(fs.predictedDisplayTime);
        updateHands(fs.predictedDisplayTime);
        bool leftPinch = false, rightPinch = false;
        m_hands.update(dt, leftPinch, rightPinch);
        m_bridge.send(m_hands, dt);   // the bridge: the hands to the desktop (01.10.2026)
        if (m_player.ready()) {
            gestures();
            applyMacros();
        }

        std::vector<XrCompositionLayerProjectionView> projViews;
        XrCompositionLayerProjection layer{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
        const XrCompositionLayerBaseHeader* layers[1] = { reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer) };
        uint32_t layerCount = 0;

        if (fs.shouldRender) {
            XrViewLocateInfo vli{ XR_TYPE_VIEW_LOCATE_INFO };
            vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            vli.displayTime = fs.predictedDisplayTime;
            vli.space = m_stageSpace;
            XrViewState vs{ XR_TYPE_VIEW_STATE };
            uint32_t viewCount = 0;
            xrLocateViews(m_session, &vli, &vs, static_cast<uint32_t>(m_views.size()), &viewCount, m_views.data());
            if ((vs.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) && (vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT)) {
                m_scene.begin();
                buildScene();
                m_scene.upload();
                projViews.resize(viewCount, { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW });
                for (uint32_t v = 0; v < viewCount; ++v) {
                    SwapchainTarget& t = m_targets[v];
                    XrSwapchainImageAcquireInfo ai{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
                    uint32_t index = 0;
                    xrAcquireSwapchainImage(t.m_swapchain, &ai, &index);
                    XrSwapchainImageWaitInfo swi{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
                    swi.timeout = XR_INFINITE_DURATION;
                    xrWaitSwapchainImage(t.m_swapchain, &swi);

                    glBindFramebuffer(GL_FRAMEBUFFER, t.m_fbo);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.m_images[index].image, 0);
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, t.m_depth);
                    m_scene.draw(multiply(projectionFromFov(m_views[v].fov, 0.05f, 100.0f), viewFromPose(m_views[v].pose)), t.m_width, t.m_height);
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);

                    XrSwapchainImageReleaseInfo ri{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
                    xrReleaseSwapchainImage(t.m_swapchain, &ri);

                    projViews[v].pose = m_views[v].pose;
                    projViews[v].fov = m_views[v].fov;
                    projViews[v].subImage.swapchain = t.m_swapchain;
                    projViews[v].subImage.imageRect = { { 0, 0 }, { t.m_width, t.m_height } };
                    projViews[v].subImage.imageArrayIndex = 0;
                }
                layer.space = m_stageSpace;
                layer.viewCount = viewCount;
                layer.views = projViews.data();
                layerCount = 1;
            }
        }

        XrFrameEndInfo ei{ XR_TYPE_FRAME_END_INFO };
        ei.displayTime = fs.predictedDisplayTime;
        ei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        ei.layerCount = layerCount;
        ei.layers = layers;
        xrEndFrame(m_session, &ei);
    }

    android_app* m_app;   ///< the native activity
    std::string m_dataDir;   ///< the app's external data folder (tot.cfg)
    Config m_config;   ///< what tot.cfg says
    TrackPlayer m_player;   ///< the engine and the composer
    Audio m_audio{ m_player };   ///< the audio stream
    std::thread m_composerThread;   ///< composes and loads tracks, measures their loudness
    std::atomic<bool> m_stopComposer{ false };   ///< ends the composer thread
    Scene m_scene;   ///< what is drawn
    Hands m_hands;   ///< the hands as tracked
    HandBridge m_bridge;                  ///< the bridge mode: the hands to the desktop's Totality (01.10.2026)
    int m_filterId = -1;   ///< perform.filter: the left hand
    int m_throwId = -1;   ///< perform.throw: the right hand
    int m_muteKickId = -1;   ///< perform.mute_kick: a pinch of the right hand
    bool m_handWas[2] = {};   ///< gestures(): the pinches as they were
    bool m_spoiled[2] = {};   ///< gestures(): per hand, the pinch was part of a two-hand gesture and does not count alone
    bool m_bothFired = false;   ///< gestures(): both hands closed and asked for the next track; once until both open
    Panel m_eclipse{};                    ///< the Eclipse's plane in the room (anchorEclipse)
    bool m_eclipseAnchored = false;   ///< the Eclipse's plane has been placed (anchorEclipse)
    CueSender m_cues;                     ///< the cue bridge's socket and thread (Cue.h)

    EGLDisplay m_display = EGL_NO_DISPLAY;   ///< the EGL display
    EGLConfig m_eglConfig = nullptr;   ///< the EGL configuration
    EGLSurface m_surface = EGL_NO_SURFACE;   ///< the small pbuffer surface the context is current on
    EGLContext m_context = EGL_NO_CONTEXT;   ///< the GLES context

    XrInstance m_instance = XR_NULL_HANDLE;   ///< the OpenXR instance
    XrSystemId m_system = XR_NULL_SYSTEM_ID;   ///< the headset
    XrSession m_session = XR_NULL_HANDLE;   ///< the OpenXR session
    XrSpace m_stageSpace = XR_NULL_HANDLE;   ///< the room: stage, else local
    XrSpace m_viewSpace = XR_NULL_HANDLE;   ///< the head
    XrSessionState m_sessionState = XR_SESSION_STATE_UNKNOWN;   ///< the session's last state
    bool m_sessionRunning = false;   ///< the session runs: frames are drawn
    bool m_quit = false;   ///< the runtime asks the app to end
    bool m_handsSupported = false;   ///< the headset tracks hands
    bool m_headValid = false;   ///< m_headPose is valid this frame
    std::vector<XrView> m_views;   ///< the eyes' poses and fields of view this frame
    std::vector<SwapchainTarget> m_targets;   ///< per eye: its swapchain and framebuffer
    XrPosef m_headPose{ { 0, 0, 0, 1 }, { 0, 0, 0 } };   ///< the head in the room
    XrTime m_lastTime = 0;   ///< the last frame's display time (0 before the first)

    PFN_xrCreateHandTrackerEXT m_pfnCreateHandTracker = nullptr;   ///< xrCreateHandTrackerEXT
    PFN_xrLocateHandJointsEXT m_pfnLocateHandJoints = nullptr;   ///< xrLocateHandJointsEXT
    PFN_xrDestroyHandTrackerEXT m_pfnDestroyHandTracker = nullptr;   ///< xrDestroyHandTrackerEXT
    XrHandTrackerEXT m_handTracker[2] = { XR_NULL_HANDLE, XR_NULL_HANDLE };   ///< left, right
};

/** @brief The native app glue's command callback: nothing to do, OpenXR's events drive the app. */
void handleCmd(android_app*, int32_t) {}

} // namespace

/** @brief NativeActivity entry point (android_native_app_glue). */
void android_main(android_app* app)
{
    app->onAppCmd = handleCmd;
    JNIEnv* env = nullptr;
    app->activity->vm->AttachCurrentThread(&env, nullptr);
    {
        // On the heap: the engine and the composer's plans are far more than the glue thread's stack.
        auto a = std::make_unique<App>(app);
        if (a->init()) a->run();
        else LOGE("init failed");
        a->shutdown();
    }
    app->activity->vm->DetachCurrentThread();
}
