/**
 * @file main.cpp
 * @brief Umbra for Meta Quest: the whole generator on the headset, played with the hands (PLAN 10.2).
 *
 * No game engine -- `NativeActivity` + `android_native_app_glue`, EGL, GLES 3, the Khronos OpenXR loader,
 * `XR_EXT_hand_tracking`, Oboe for audio, and the unchanged core from `../Core`. The frame of the app -- the OpenXR
 * session, the swapchains, the point renderer, the font, the hands and the audio stream -- is Ephemeris' Quest app
 * (after Phosphene's); the player, the gestures, the panel and the Eclipse in the room are Umbra's.
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
 * **The Eclipse in the room.** Above the player, ahead and tilted towards them, the Patterns page's Eclipse (the
 * plugin's EditorEclipse.h) at the size of a room: the kick a dark disc -- no light at all -- in its corona, which
 * swells with every kick; every other part a ring of beads around it, a bar once round, the playhead's hand sweeping
 * them; where three rings meet on a sixteenth, a ray. As the Kaleidoscope rules ask: nothing about the camera moves
 * with the audio, and every brightness is a continuous function of the bar position (the next bar's beads fade in as
 * the last ones fade out).
 *
 * **`umb.cfg`** in `<externalDataPath>` (`/sdcard/Android/data/com.reneweller.umbra.quest/files`):
 * @code
 *   mute=1              start silent (the test rule; the engine still runs)
 *   seed=2026           the first track's seed; the next takes the next seed
 *   minutes=7           length of a track
 *   set_minutes=60      a set of so many minutes instead of single tracks
 *   style=Hypnotic      Hypnotic, Ostgut, Dub or Raw
 *   osc_host=192.168.1.20   the score cues (Cue.h) to a visualiser such as Kaleidoscope; empty = off
 *   osc_port=9000
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

#include "umb/Cue.h"
#include "umb/Engine.h"
#include "umb/Leveler.h"
#include "umb/compose/Composer.h"
#include "umb/compose/Set.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "Umbra", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "Umbra", __VA_ARGS__)

using namespace umb;

namespace {

// ---------------------------------------------------------------- small math

/** @brief Column-major 4x4 matrix, the layout GL wants. */
struct Mat4 { float m[16]; };
/** @brief A point or direction. */
struct Vec3 { float x, y, z; };

Mat4 identity() { Mat4 r{}; r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f; return r; }

Mat4 multiply(const Mat4& a, const Mat4& b)
{
    Mat4 r{};
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr)
            r.m[c * 4 + rr] = a.m[0 * 4 + rr] * b.m[c * 4 + 0] + a.m[1 * 4 + rr] * b.m[c * 4 + 1]
                            + a.m[2 * 4 + rr] * b.m[c * 4 + 2] + a.m[3 * 4 + rr] * b.m[c * 4 + 3];
    return r;
}

/** @brief Asymmetric projection from OpenXR's four half-angles. */
Mat4 projectionFromFov(const XrFovf& fov, float nearZ, float farZ)
{
    const float l = std::tan(fov.angleLeft), r = std::tan(fov.angleRight);
    const float u = std::tan(fov.angleUp), d = std::tan(fov.angleDown);
    const float w = r - l, h = u - d;
    Mat4 p{};
    p.m[0] = 2.0f / w;  p.m[8] = (r + l) / w;
    p.m[5] = 2.0f / h;  p.m[9] = (u + d) / h;
    p.m[10] = -(farZ + nearZ) / (farZ - nearZ);
    p.m[11] = -1.0f;
    p.m[14] = -(2.0f * farZ * nearZ) / (farZ - nearZ);
    return p;
}

Mat4 rotationFromQuat(const XrQuaternionf& q)
{
    const float x = q.x, y = q.y, z = q.z, w = q.w;
    Mat4 r = identity();
    r.m[0] = 1 - 2 * (y * y + z * z); r.m[4] = 2 * (x * y - z * w);     r.m[8] = 2 * (x * z + y * w);
    r.m[1] = 2 * (x * y + z * w);     r.m[5] = 1 - 2 * (x * x + z * z); r.m[9] = 2 * (y * z - x * w);
    r.m[2] = 2 * (x * z - y * w);     r.m[6] = 2 * (y * z + x * w);     r.m[10] = 1 - 2 * (x * x + y * y);
    return r;
}

Vec3 rotate(const XrQuaternionf& q, Vec3 v)
{
    const Mat4 r = rotationFromQuat(q);
    return { r.m[0] * v.x + r.m[4] * v.y + r.m[8] * v.z,
             r.m[1] * v.x + r.m[5] * v.y + r.m[9] * v.z,
             r.m[2] * v.x + r.m[6] * v.y + r.m[10] * v.z };
}

/** @brief The view matrix of an eye pose: the inverse of the pose. */
Mat4 viewFromPose(const XrPosef& pose)
{
    const Mat4 r = rotationFromQuat(pose.orientation);
    Mat4 rt = identity();
    for (int c = 0; c < 3; ++c) for (int rr = 0; rr < 3; ++rr) rt.m[c * 4 + rr] = r.m[rr * 4 + c];
    Mat4 t = identity();
    t.m[12] = -pose.position.x; t.m[13] = -pose.position.y; t.m[14] = -pose.position.z;
    return multiply(rt, t);
}

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
    static const unsigned char kFont[][7] = {
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
    if (c >= 'A' && c <= 'Z') return kFont[c - 'A'];
    if (c >= '0' && c <= '9') return kFont[26 + (c - '0')];
    switch (c) {
    case ' ': return kFont[36]; case '-': return kFont[37]; case '.': return kFont[38];
    case ':': return kFont[39]; case '/': return kFont[40]; case '<': return kFont[41];
    case '>': return kFont[42]; case '+': return kFont[43]; case '=': return kFont[44];
    case '%': return kFont[45]; case '#': return kFont[46]; case '*': return kFont[47];
    default: break;
    }
    return kFont[48];
}


// ---------------------------------------------------------------- config

/** @brief What `umb.cfg` can say. */
struct Config {
    bool mute = false;              ///< start silent (test rule); the engine still runs
    uint64_t seed = 1;              ///< the first track's seed
    double minutes = 0.0;           ///< length of a track (0: compose.minutes)
    double setMinutes = 0.0;        ///< a set of so many minutes instead of single tracks
    std::string oscHost;            ///< cue target (Cue.h), empty = off
    int oscPort = 9000;             ///< cue port
    std::string knobs;              ///< knob assignments, "key=value" separated by ';' or newlines
    bool quest = true;              ///< the Quest's quality (Engine::Quality): the grain cloud rests; `quality=desktop` plays all
};

/** @brief Reads `<dir>/umb.cfg`; every key is optional. */
Config readConfig(const char* dir)
{
    Config c;
    if (dir == nullptr) return c;
    const std::string path = std::string(dir) + "/umb.cfg";
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
        if (k == "mute") c.mute = v != "0";
        else if (k == "seed") c.seed = std::strtoull(v.c_str(), nullptr, 10);
        else if (k == "minutes") c.minutes = std::max(3.0, std::atof(v.c_str()));
        else if (k == "set_minutes") c.setMinutes = std::clamp(std::atof(v.c_str()), 0.0, 240.0);
        else if (k == "osc_host") c.oscHost = v;
        else if (k == "osc_port") c.oscPort = std::atoi(v.c_str());
        else if (k == "quality") c.quest = v != "desktop";
        else if (k == "style") { c.knobs += "compose.style=" + v; c.knobs += ";"; }
        else if (k == "knobs") { c.knobs += v; c.knobs += ";"; }
        else LOGE("umb.cfg: unknown key %s", k.c_str());
    }
    std::fclose(f);
    LOGI("config: mute %d, seed %llu, set %.0f min", c.mute ? 1 : 0, static_cast<unsigned long long>(c.seed), c.setMinutes);
    return c;
}

// ---------------------------------------------------------------- the track player

/** @brief What plays, for the panel: a set, or a track as deck A of a one-track set, with where its tracks lie. */
struct Now {
    SetScore set;
    bool isSet = false;
    std::vector<TrackInfo> tracks;
    std::vector<double> starts, swaps;   ///< set beats of each track's first bar and of its swap
    std::vector<int> decks;
    /** @brief The track that owns the low end at @p beat. */
    int trackAt(double beat) const
    {
        int t = 0;
        for (size_t i = 0; i < tracks.size(); ++i) if (beat >= (isSet && i > 0 ? swaps[i] : starts[i])) t = static_cast<int>(i);
        return t;
    }
};

/**
 * @brief Engine and composer, with a play/stop fade and a swap to the next track or set.
 *
 * **Handover.** `state_` is the only synchronisation between the audio thread and the composer thread. The audio thread
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
        muted_ = cfg.mute;
        seed_ = cfg.seed;
        setMinutes_ = cfg.setMinutes;
        engine_.setLive(true);   // the perform module acts, the mixer is in a track's path (Engine.h)
        engine_.setQuality(cfg.quest ? Engine::Quality::Quest : Engine::Quality::Desktop);
        if (!cfg.knobs.empty()) {
            std::string err;
            if (!engine_.params().parseText(cfg.knobs, &err)) LOGE("umb.cfg knobs: %s", err.c_str());
        }
        if (cfg.minutes > 0.0) engine_.params().set(engine_.params().id(Module::Compose, 0, compose::Minutes), static_cast<float>(cfg.minutes));
        sr_ = static_cast<double>(sampleRate);
        gainCoef_ = static_cast<float>(1.0 - std::exp(-1.0 / (0.015 * sampleRate)));   // 15 ms
        levelCoef_ = static_cast<float>(1.0 - std::exp(-1.0 / (0.3 * sampleRate)));    // 300 ms
        engine_.prepare(sampleRate, block);
        auto now = std::make_shared<Now>(composeFor(seed_));
        load(*now);
        publish(now);
        levelDue_ = true;   // measured once the stream runs (pump)
        ready_.store(true, std::memory_order_release);
        LOGI("ready: seed %llu, %.0f s", static_cast<unsigned long long>(seed_), engine_.lengthSeconds());
    }

    /** @brief Renders one block. Audio thread only. */
    void process(float* L, float* R, int n)
    {
        int expected = kIdle;
        if (!ready_.load(std::memory_order_acquire) || !state_.compare_exchange_strong(expected, kProcessing)) {
            std::fill(L, L + n, 0.0f);
            std::fill(R, R + n, 0.0f);
            return;
        }
        if (trimsReady_.load(std::memory_order_acquire)) {   // the loudness corrections, measured while it plays
            for (int d = 0; d < kDecks; ++d) if (!trimsIn_[d].empty()) engine_.setLevelTrims(d, trimsIn_[d]);
            trimsReady_.store(false, std::memory_order_release);
        }
        const float target = playing_.load(std::memory_order_relaxed) ? 1.0f : 0.0f;
        if (target <= 0.0f && gain_ < 1.0e-4f) {
            // Faded out: the music waits where it is instead of running on silently.
            gain_ = 0.0f;
            std::fill(L, L + n, 0.0f);
            std::fill(R, R + n, 0.0f);
        } else {
            const double from = engine_.beat();
            engine_.process(L, R, n);
            if (cues_ != nullptr && cues_->running()) {
                // The cues of this block (Cue.h), stamped with the moment the block is heard.
                const double blockSeconds = static_cast<double>(n) / sr_, to = engine_.beat();
                const float bpm = static_cast<float>((to - from) / blockSeconds * 60.0);
                tap_.scan(engine_.cueMarks(), from, to, bpm, CueSender::nowNanos(), static_cast<int64_t>(2.0 * blockSeconds * 1.0e9),
                          static_cast<int64_t>(blockSeconds * 1.0e9), cues_->ring());
            }
            const float out = muted_ ? 0.0f : 1.0f;
            float level = level_.load(std::memory_order_relaxed);
            for (int i = 0; i < n; ++i) {
                gain_ += (target - gain_) * gainCoef_;   // one pole: no step, no click
                level += (0.5f * (L[i] * L[i] + R[i] * R[i]) * gain_ * gain_ - level) * levelCoef_;
                L[i] *= gain_ * out;
                R[i] *= gain_ * out;
            }
            level_.store(level, std::memory_order_relaxed);
            beat_.store(engine_.beat(), std::memory_order_relaxed);
            seconds_.store(engine_.seconds(), std::memory_order_relaxed);
            if (engine_.seconds() > engine_.lengthSeconds() + 8.0) nextRequest_.store(true, std::memory_order_relaxed);
        }
        state_.store(kIdle, std::memory_order_release);
    }

    /** @brief Binds the cue sender (Cue.h); null leaves the cues off. Call before the audio stream starts. */
    void setCueSender(CueSender* sender) { cues_ = sender; }
    /** @brief Play or stop (any thread; takes effect over the fade). */
    void setPlaying(bool on) { playing_.store(on, std::memory_order_relaxed); }
    /** @brief Whether play is on. */
    bool playing() const { return playing_.load(std::memory_order_relaxed); }
    /** @brief Asks for the next track or set (any thread). */
    void requestNext() { nextRequest_.store(true, std::memory_order_relaxed); }
    /** @brief One round of composer work: a pending next track, else the loudness of the one playing. Composer thread. */
    void pump()
    {
        if (nextRequest_.exchange(false, std::memory_order_relaxed)) next();
        else if (levelDue_) { levelDue_ = false; level(); }
    }
    /** @brief Breaks off the composer's work (the app closes). */
    void quit() { quit_.store(true, std::memory_order_relaxed); }

    /** @brief Where the audio thread is, in beats and seconds (render thread). */
    double beat() const { return beat_.load(std::memory_order_relaxed); }
    double seconds() const { return seconds_.load(std::memory_order_relaxed); }   ///< @copydoc beat
    /** @brief The output's mean square, smoothed over 300 ms, as dBFS (render thread). */
    float levelDb() const { const float l = level_.load(std::memory_order_relaxed); return l > 1.0e-10f ? 10.0f * std::log10(l) : -100.0f; }
    /** @brief What plays (render thread): an immutable copy. */
    std::shared_ptr<const Now> now() const { std::lock_guard<std::mutex> lock(nowMutex_); return now_; }
    /** @brief Which track or set plays, from 1. */
    int number() const { return number_.load(std::memory_order_relaxed); }
    /** @brief The knobs, for the hand controls (atomics: any thread). */
    ParamStore& params() { return engine_.params(); }
    /** @brief Whether prepare() has finished. */
    bool ready() const { return ready_.load(std::memory_order_acquire); }
    /** @brief Whether the output is muted by the config. */
    bool muted() const { return muted_; }

private:
    enum : int { kIdle = 0, kProcessing, kBlocked };

    /** @brief Composes the track or set after this one and swaps it in behind a fade. Composer thread. */
    void next()
    {
        const bool wasPlaying = playing();
        setPlaying(false);
        const int n = number_.load(std::memory_order_relaxed) + 1;
        // Composed while the fade runs and the old one still waits: nothing is taken from the audio thread yet.
        auto now = std::make_shared<Now>(composeFor(seed_ + static_cast<uint64_t>(n - 1)));
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        int expected = kIdle;
        while (!state_.compare_exchange_weak(expected, kBlocked)) {
            expected = kIdle;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        load(*now);                // allocates: here, while the audio thread writes silence
        trimsReady_.store(false, std::memory_order_relaxed);
        tap_.reset();
        gain_ = 0.0f;
        beat_.store(0.0, std::memory_order_relaxed);
        seconds_.store(0.0, std::memory_order_relaxed);
        state_.store(kIdle, std::memory_order_release);
        number_.store(n, std::memory_order_relaxed);
        publish(now);
        setPlaying(wasPlaying);
        levelDue_ = true;
        LOGI("%s %d: seed %llu", now->isSet ? "set" : "track", n, static_cast<unsigned long long>(seed_ + static_cast<uint64_t>(n - 1)));
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
        SetScore s = playing->set;
        auto stop = [this] { return nextRequest_.load(std::memory_order_relaxed) || quit_.load(std::memory_order_relaxed); };
        const bool done = playing->isSet ? !levelSet(s, engine_.params(), 20.0, stop).empty() : !levelScore(s.decks[0], engine_.params(), 20.0, stop).empty();
        if (!done || trimsReady_.load(std::memory_order_acquire)) return;
        for (int d = 0; d < kDecks; ++d) {
            trimsIn_[d].clear();
            for (const LevelMark& m : s.decks[d].levels) trimsIn_[d].push_back(m.trimDb);
        }
        trimsReady_.store(true, std::memory_order_release);
        LOGI("%d: loudness corrected", number());
    }

    void publish(std::shared_ptr<const Now> s) { std::lock_guard<std::mutex> lock(nowMutex_); now_ = std::move(s); }

    void load(const Now& now)
    {
        if (now.isSet) engine_.loadSet(now.set);
        else engine_.load(now.set.decks[0]);
    }

    /** @brief A track, or with set_minutes a set. Composer thread. */
    Now composeFor(uint64_t seed)
    {
        const ParamStore& p = engine_.params();
        Now out;
        if (setMinutes_ > 0.0) {
            SetInfo info;
            out.isSet = true;
            out.set = composeSet(p, seed, setMinutes_, nullptr, &info);
            for (const SetTrack& t : info.tracks) {
                out.tracks.push_back(t.info);
                out.starts.push_back(t.start);
                out.swaps.push_back(t.swapIn);
                out.decks.push_back(t.deck);
            }
        } else {
            TrackInfo info;
            out.set.decks[0] = composeTrack(p, seed, TrackRequest{}, nullptr, std::string(), &info);
            out.set.lengthBeats = out.set.decks[0].lengthBeats;
            out.tracks.push_back(info);
            out.starts.push_back(0.0);
            out.swaps.push_back(0.0);
            out.decks.push_back(0);
        }
        return out;
    }

    Engine engine_;
    uint64_t seed_ = 1;
    double setMinutes_ = 0.0;
    std::atomic<int> state_{ kIdle };
    std::atomic<bool> ready_{ false }, playing_{ false }, nextRequest_{ false }, quit_{ false };
    bool levelDue_ = false;                      ///< what plays is still to be measured (composer thread)
    std::vector<float> trimsIn_[kDecks];         ///< its corrections, for the audio thread once trimsReady_ says so
    std::atomic<bool> trimsReady_{ false };
    std::atomic<int> number_{ 1 };
    std::atomic<double> beat_{ 0.0 }, seconds_{ 0.0 };
    std::atomic<float> level_{ 0.0f };
    bool muted_ = false;
    double sr_ = 48000.0;
    CueSender* cues_ = nullptr;   ///< the cue bridge, owned by the app; null = off
    CueTap tap_;                  ///< audio thread: beat range -> cues
    float gain_ = 0.0f, gainCoef_ = 0.002f, levelCoef_ = 0.0001f;
    mutable std::mutex nowMutex_;
    std::shared_ptr<const Now> now_;
};

// ---------------------------------------------------------------- audio

/** @brief Oboe output stream: a low-latency float stream straight into TrackPlayer::process. */
class Audio : public oboe::AudioStreamDataCallback {
public:
    explicit Audio(TrackPlayer& p) : player_(p) {}

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
        if (b.openStream(stream_) != oboe::Result::OK) { LOGE("Oboe: cannot open stream"); return false; }
        sampleRate_ = stream_->getSampleRate();
        burst_ = stream_->getFramesPerBurst();
        stream_->setBufferSizeInFrames(burst_ * 2);
        bufL_.assign(4096, 0.0f);
        bufR_.assign(4096, 0.0f);
        LOGI("Oboe: %d Hz, burst %d", sampleRate_, burst_);
        return true;
    }
    bool start() { return stream_ && stream_->requestStart() == oboe::Result::OK; }
    void stop() { if (stream_) { stream_->requestStop(); stream_->close(); stream_.reset(); } }
    int sampleRate() const { return sampleRate_; }
    int burst() const { return burst_; }

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream*, void* data, int32_t frames) override
    {
        float* out = static_cast<float*>(data);
        int done = 0;
        while (done < frames) {
            const int n = std::min(frames - done, static_cast<int32_t>(bufL_.size()));
            player_.process(bufL_.data(), bufR_.data(), n);
            for (int i = 0; i < n; ++i) {
                out[(done + i) * 2] = bufL_[static_cast<size_t>(i)];
                out[(done + i) * 2 + 1] = bufR_[static_cast<size_t>(i)];
            }
            done += n;
        }
        return oboe::DataCallbackResult::Continue;
    }

private:
    TrackPlayer& player_;
    std::shared_ptr<oboe::AudioStream> stream_;
    std::vector<float> bufL_, bufR_;
    int sampleRate_ = 48000, burst_ = 256;
};

// ---------------------------------------------------------------- GL scene

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
struct Point { float x, y, z; float r, g, b, a; float size; };

/** @brief A head-locked plane to lay text out on: origin plus a right and an up axis. */
struct Panel { Vec3 origin, right, up; };

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
    static constexpr float kPixelsPerRadian = 1150.0f;

    bool init()
    {
        program_ = glCreateProgram();
        glAttachShader(program_, compile(GL_VERTEX_SHADER, kVertexShader));
        glAttachShader(program_, compile(GL_FRAGMENT_SHADER, kFragmentShader));
        glLinkProgram(program_);
        GLint ok = 0;
        glGetProgramiv(program_, GL_LINK_STATUS, &ok);
        if (!ok) { LOGE("program link failed"); return false; }
        uVP_ = glGetUniformLocation(program_, "uVP");
        glGenBuffers(1, &vbo_);
        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Point), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Point), reinterpret_cast<void*>(12));
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Point), reinterpret_cast<void*>(28));
        glBindVertexArray(0);
        return true;
    }

    void begin() { points_.clear(); }

    /** @brief One point in world space. */
    void add(float x, float y, float z, float r, float g, float b, float a, float size)
    {
        points_.push_back({ x, y, z, r, g, b, a, size });
    }

    /** @brief One point on a panel, at panel coordinates @p u (right) and @p v (up). */
    void addOn(const Panel& p, float u, float v, float r, float g, float b, float a, float size)
    {
        add(p.origin.x + p.right.x * u + p.up.x * v,
            p.origin.y + p.right.y * u + p.up.y * v,
            p.origin.z + p.right.z * u + p.up.z * v, r, g, b, a, size);
    }

    /**
     * @brief Text on a panel, one point per lit pixel of the 5x7 font; @p cell is the pixel pitch.
     *
     * The point size is `cell * kPixelsPerRadian / w`: a cell of `cell` metres at `w` metres
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
                              r, g, b, a, cell * kPixelsPerRadian);
            u += 6.0f * cell;
        }
    }

    void upload()
    {
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(points_.size() * sizeof(Point)), points_.data(), GL_DYNAMIC_DRAW);
    }

    void draw(const Mat4& vp, int width, int height)
    {
        glViewport(0, 0, width, height);
        glClearColor(0.008f, 0.008f, 0.010f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);            // additive: points add light, they never occlude
        glUseProgram(program_);
        glUniformMatrix4fv(uVP_, 1, GL_FALSE, vp.m);
        glBindVertexArray(vao_);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(points_.size()));
        glBindVertexArray(0);
    }

private:
    GLuint program_ = 0, vbo_ = 0, vao_ = 0;
    GLint uVP_ = -1;
    std::vector<Point> points_;
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
        bool valid = false;
        XrVector3f palm{};
        float pinch = 0.0f;     ///< 0 open .. 1 closed
        bool closed = false;    ///< after the Schmitt trigger
        float height = 0.5f;    ///< 0..1 against the head
        float macro = 0.5f;     ///< the smoothed value the macro follows
        bool everSeen = false;  ///< this hand has been tracked at least once
    };

    /** @brief Feeds one hand; @p headY is the head's height in the same space. */
    void setHand(int h, const XrVector3f& palm, float thumbToIndex, float headY)
    {
        Hand& s = hand_[h];
        s.valid = true;
        s.everSeen = true;
        s.palm = palm;
        s.pinch = clamp01(1.0f - (thumbToIndex - 0.015f) / 0.035f);
        if (thumbToIndex < 0.022f) s.closed = true;
        else if (thumbToIndex > 0.038f) s.closed = false;
        s.height = clamp01((palm.y - (headY - 1.0f)) / 0.8f);
    }
    /** @brief Marks a hand as not tracked this frame (its macro then holds its value). */
    void lost(int h) { hand_[h].valid = false; hand_[h].closed = false; }

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
            Hand& s = hand_[h];
            if (s.valid && !s.closed) s.macro += (s.height - s.macro) * k;
        }
        leftPinch = hand_[0].closed && !wasClosed_[0];
        rightPinch = hand_[1].closed && !wasClosed_[1];
        wasClosed_[0] = hand_[0].closed;
        wasClosed_[1] = hand_[1].closed;
    }

    const Hand& hand(int h) const { return hand_[h]; }

private:
    Hand hand_[2];
    bool wasClosed_[2] = { false, false };
};

// ---------------------------------------------------------------- the app

/** @brief One eye's swapchain and the framebuffer it is rendered through. */
struct SwapchainTarget {
    XrSwapchain swapchain = XR_NULL_HANDLE;
    int width = 0, height = 0;
    std::vector<XrSwapchainImageOpenGLESKHR> images;
    GLuint fbo = 0, depth = 0;
};

class App {
public:
    explicit App(android_app* app) : app_(app) {}

    bool init()
    {
        dataDir_ = app_->activity->externalDataPath ? app_->activity->externalDataPath : "";
        config_ = readConfig(dataDir_.c_str());
        // The cue bridge of PLAN 10.3, off unless umb.cfg names a host: a visualiser that is not there changes nothing.
        if (!config_.oscHost.empty()) {
            if (cues_.start(config_.oscHost, config_.oscPort)) {
                LOGI("cues: OSC to %s:%d", config_.oscHost.c_str(), config_.oscPort);
                player_.setCueSender(&cues_);
            } else LOGE("cues: cannot reach %s:%d", config_.oscHost.c_str(), config_.oscPort);
        }
        if (!initLoader()) return false;
        if (!initInstance()) return false;
        if (!initEgl()) return false;
        if (!initSession()) return false;
        if (!initHands()) LOGE("hand tracking unavailable: the hands will not play");
        if (!scene_.init()) return false;
        // The stream is opened first so the engine is prepared for the rate the device really gives.
        if (!audio_.open()) return false;
        composerThread_ = std::thread([this] { composerLoop(); });
        return true;
    }

    void run()
    {
        while (!app_->destroyRequested) {
            int events;
            android_poll_source* source;
            const int timeout = (sessionRunning_ || app_->window == nullptr) ? 0 : -1;
            while (ALooper_pollOnce(timeout, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0) {
                if (source) source->process(app_, source);
                if (app_->destroyRequested) break;
            }
            pollXrEvents();
            if (quit_) break;
            if (sessionRunning_) frame();
        }
    }

    void shutdown()
    {
        stopComposer_.store(true);
        player_.quit();
        if (composerThread_.joinable()) composerThread_.join();
        audio_.stop();
        cues_.stop();   // after the stream: the sender's thread reads the ring the audio thread fills
        for (SwapchainTarget& t : targets_) if (t.swapchain != XR_NULL_HANDLE) xrDestroySwapchain(t.swapchain);
        for (int h = 0; h < 2; ++h)
            if (handTracker_[h] != XR_NULL_HANDLE && pfnDestroyHandTracker_) pfnDestroyHandTracker_(handTracker_[h]);
        if (viewSpace_ != XR_NULL_HANDLE) xrDestroySpace(viewSpace_);
        if (stageSpace_ != XR_NULL_HANDLE) xrDestroySpace(stageSpace_);
        if (session_ != XR_NULL_HANDLE) xrDestroySession(session_);
        if (instance_ != XR_NULL_HANDLE) xrDestroyInstance(instance_);
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
        player_.prepare(audio_.sampleRate(), audio_.burst() * 2, config_);
        const ParamStore& p = player_.params();
        filterId_ = p.id(Module::Perform, 0, perform::Filter);
        throwId_ = p.id(Module::Perform, 0, perform::Throw);
        muteKickId_ = p.id(Module::Perform, 0, perform::MuteKick);
        if (!audio_.start()) LOGE("Oboe: cannot start");
        else LOGI("audio started%s", config_.mute ? " (muted)" : "");
        while (!stopComposer_.load()) {
            player_.pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    // ------------------------------------------------ OpenXR setup

    bool initLoader()
    {
        PFN_xrInitializeLoaderKHR initLoader = nullptr;
        xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", reinterpret_cast<PFN_xrVoidFunction*>(&initLoader));
        if (initLoader == nullptr) { LOGE("no xrInitializeLoaderKHR"); return false; }
        XrLoaderInitInfoAndroidKHR li{ XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR };
        li.applicationVM = app_->activity->vm;
        li.applicationContext = app_->activity->clazz;
        return XR_SUCCEEDED(initLoader(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&li)));
    }

    bool initInstance()
    {
        const char* exts[] = { XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
                               XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
                               XR_EXT_HAND_TRACKING_EXTENSION_NAME };
        XrInstanceCreateInfoAndroidKHR android{ XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR };
        android.applicationVM = app_->activity->vm;
        android.applicationActivity = app_->activity->clazz;
        XrInstanceCreateInfo ci{ XR_TYPE_INSTANCE_CREATE_INFO, &android };
        std::strncpy(ci.applicationInfo.applicationName, "Umbra", XR_MAX_APPLICATION_NAME_SIZE - 1);
        ci.applicationInfo.applicationVersion = 1;
        std::strncpy(ci.applicationInfo.engineName, "UmbraCore", XR_MAX_ENGINE_NAME_SIZE - 1);
        ci.applicationInfo.apiVersion = XR_API_VERSION_1_0;
        ci.enabledExtensionCount = 3;
        ci.enabledExtensionNames = exts;
        if (XR_FAILED(xrCreateInstance(&ci, &instance_))) { LOGE("xrCreateInstance failed"); return false; }

        XrSystemGetInfo sgi{ XR_TYPE_SYSTEM_GET_INFO };
        sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        if (XR_FAILED(xrGetSystem(instance_, &sgi, &system_))) { LOGE("xrGetSystem failed"); return false; }
        XrSystemHandTrackingPropertiesEXT ht{ XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT };
        XrSystemProperties sp{ XR_TYPE_SYSTEM_PROPERTIES, &ht };
        xrGetSystemProperties(instance_, system_, &sp);
        handsSupported_ = ht.supportsHandTracking == XR_TRUE;
        LOGI("system: %s, hand tracking %d", sp.systemName, handsSupported_ ? 1 : 0);
        return true;
    }

    bool initEgl()
    {
        PFN_xrGetOpenGLESGraphicsRequirementsKHR getReq = nullptr;
        xrGetInstanceProcAddr(instance_, "xrGetOpenGLESGraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&getReq));
        XrGraphicsRequirementsOpenGLESKHR req{ XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR };
        if (getReq == nullptr || XR_FAILED(getReq(instance_, system_, &req))) { LOGE("GLES requirements failed"); return false; }

        display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        EGLint major = 0, minor = 0;
        if (!eglInitialize(display_, &major, &minor)) { LOGE("eglInitialize failed"); return false; }
        const EGLint attribs[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                                   EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                                   EGL_DEPTH_SIZE, 0, EGL_NONE };
        EGLint count = 0;
        if (!eglChooseConfig(display_, attribs, &eglConfig_, 1, &count) || count == 0) { LOGE("eglChooseConfig failed"); return false; }
        const EGLint pbuf[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
        surface_ = eglCreatePbufferSurface(display_, eglConfig_, pbuf);
        const EGLint ctx[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
        context_ = eglCreateContext(display_, eglConfig_, EGL_NO_CONTEXT, ctx);
        if (context_ == EGL_NO_CONTEXT || !eglMakeCurrent(display_, surface_, surface_, context_)) { LOGE("EGL context failed"); return false; }
        LOGI("EGL %d.%d, GL %s", major, minor, glGetString(GL_VERSION));
        return true;
    }

    bool initSession()
    {
        XrGraphicsBindingOpenGLESAndroidKHR gb{ XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR };
        gb.display = display_; gb.config = eglConfig_; gb.context = context_;
        XrSessionCreateInfo sci{ XR_TYPE_SESSION_CREATE_INFO, &gb };
        sci.systemId = system_;
        if (XR_FAILED(xrCreateSession(instance_, &sci, &session_))) { LOGE("xrCreateSession failed"); return false; }

        XrReferenceSpaceCreateInfo rs{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
        rs.poseInReferenceSpace.orientation.w = 1.0f;
        rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
        if (XR_FAILED(xrCreateReferenceSpace(session_, &rs, &stageSpace_))) {
            rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
            if (XR_FAILED(xrCreateReferenceSpace(session_, &rs, &stageSpace_))) { LOGE("no reference space"); return false; }
        }
        rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        xrCreateReferenceSpace(session_, &rs, &viewSpace_);

        uint32_t viewCount = 0;
        xrEnumerateViewConfigurationViews(instance_, system_, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
        std::vector<XrViewConfigurationView> cfg(viewCount, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
        xrEnumerateViewConfigurationViews(instance_, system_, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount, cfg.data());
        views_.assign(viewCount, { XR_TYPE_VIEW });

        uint32_t fmtCount = 0;
        xrEnumerateSwapchainFormats(session_, 0, &fmtCount, nullptr);
        std::vector<int64_t> formats(fmtCount);
        xrEnumerateSwapchainFormats(session_, fmtCount, &fmtCount, formats.data());
        int64_t format = formats.empty() ? GL_RGBA8 : formats[0];
        for (int64_t f : formats) if (f == GL_SRGB8_ALPHA8) { format = f; break; }

        targets_.resize(viewCount);
        for (uint32_t v = 0; v < viewCount; ++v) {
            SwapchainTarget& t = targets_[v];
            XrSwapchainCreateInfo sc{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
            sc.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
            sc.format = format;
            sc.sampleCount = 1;
            sc.width = cfg[v].recommendedImageRectWidth;
            sc.height = cfg[v].recommendedImageRectHeight;
            sc.faceCount = 1; sc.arraySize = 1; sc.mipCount = 1;
            if (XR_FAILED(xrCreateSwapchain(session_, &sc, &t.swapchain))) { LOGE("xrCreateSwapchain failed"); return false; }
            t.width = static_cast<int>(sc.width);
            t.height = static_cast<int>(sc.height);
            uint32_t imgCount = 0;
            xrEnumerateSwapchainImages(t.swapchain, 0, &imgCount, nullptr);
            t.images.assign(imgCount, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR });
            xrEnumerateSwapchainImages(t.swapchain, imgCount, &imgCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(t.images.data()));
            glGenFramebuffers(1, &t.fbo);
            glGenRenderbuffers(1, &t.depth);
            glBindRenderbuffer(GL_RENDERBUFFER, t.depth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, t.width, t.height);
            LOGI("view %u: %dx%d, %u images", v, t.width, t.height, imgCount);
        }
        return true;
    }

    bool initHands()
    {
        if (!handsSupported_) return false;
        xrGetInstanceProcAddr(instance_, "xrCreateHandTrackerEXT", reinterpret_cast<PFN_xrVoidFunction*>(&pfnCreateHandTracker_));
        xrGetInstanceProcAddr(instance_, "xrLocateHandJointsEXT", reinterpret_cast<PFN_xrVoidFunction*>(&pfnLocateHandJoints_));
        xrGetInstanceProcAddr(instance_, "xrDestroyHandTrackerEXT", reinterpret_cast<PFN_xrVoidFunction*>(&pfnDestroyHandTracker_));
        if (!pfnCreateHandTracker_ || !pfnLocateHandJoints_) return false;
        for (int h = 0; h < 2; ++h) {
            XrHandTrackerCreateInfoEXT hci{ XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT };
            hci.hand = (h == 0) ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT;
            hci.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
            if (XR_FAILED(pfnCreateHandTracker_(session_, &hci, &handTracker_[h]))) return false;
        }
        return true;
    }

    void pollXrEvents()
    {
        XrEventDataBuffer ev{ XR_TYPE_EVENT_DATA_BUFFER };
        while (xrPollEvent(instance_, &ev) == XR_SUCCESS) {
            if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                const auto* sc = reinterpret_cast<const XrEventDataSessionStateChanged*>(&ev);
                sessionState_ = sc->state;
                switch (sessionState_) {
                case XR_SESSION_STATE_READY: {
                    XrSessionBeginInfo bi{ XR_TYPE_SESSION_BEGIN_INFO };
                    bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    if (XR_SUCCEEDED(xrBeginSession(session_, &bi))) sessionRunning_ = true;
                    break;
                }
                case XR_SESSION_STATE_STOPPING:
                    xrEndSession(session_);
                    sessionRunning_ = false;
                    break;
                case XR_SESSION_STATE_EXITING:
                case XR_SESSION_STATE_LOSS_PENDING:
                    quit_ = true;
                    break;
                default: break;
                }
            } else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
                quit_ = true;
            }
            ev = { XR_TYPE_EVENT_DATA_BUFFER };
        }
    }

    // ------------------------------------------------ per frame

    void updateHead(XrTime time)
    {
        headValid_ = false;
        if (viewSpace_ == XR_NULL_HANDLE) return;
        XrSpaceLocation loc{ XR_TYPE_SPACE_LOCATION };
        if (XR_FAILED(xrLocateSpace(viewSpace_, stageSpace_, time, &loc))) return;
        const XrSpaceLocationFlags need = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        if ((loc.locationFlags & need) != need) return;
        headPose_ = loc.pose;
        headValid_ = true;
    }

    void updateHands(XrTime time)
    {
        for (int h = 0; h < 2; ++h) {
            if (handTracker_[h] == XR_NULL_HANDLE) { hands_.lost(h); continue; }
            XrHandJointLocationEXT joints[XR_HAND_JOINT_COUNT_EXT];
            XrHandJointLocationsEXT locs{ XR_TYPE_HAND_JOINT_LOCATIONS_EXT };
            locs.jointCount = XR_HAND_JOINT_COUNT_EXT;
            locs.jointLocations = joints;
            XrHandJointsLocateInfoEXT li{ XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT };
            li.baseSpace = stageSpace_;
            li.time = time;
            if (XR_FAILED(pfnLocateHandJoints_(handTracker_[h], &li, &locs)) || !locs.isActive) { hands_.lost(h); continue; }
            const XrHandJointLocationEXT& palm = joints[XR_HAND_JOINT_PALM_EXT];
            const XrHandJointLocationEXT& thumb = joints[XR_HAND_JOINT_THUMB_TIP_EXT];
            const XrHandJointLocationEXT& index = joints[XR_HAND_JOINT_INDEX_TIP_EXT];
            const XrSpaceLocationFlags need = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
            if ((palm.locationFlags & need) != need) { hands_.lost(h); continue; }
            const float dx = thumb.pose.position.x - index.pose.position.x;
            const float dy = thumb.pose.position.y - index.pose.position.y;
            const float dz = thumb.pose.position.z - index.pose.position.z;
            hands_.setHand(h, palm.pose.position, std::sqrt(dx * dx + dy * dy + dz * dz),
                           headValid_ ? headPose_.position.y : 1.6f);
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
        if (!player_.ready() || filterId_ < 0) return;
        ParamStore& p = player_.params();
        // A hand that has never been tracked writes nothing: without hand tracking the controls keep what umb.cfg set.
        const Hands::Hand& left = hands_.hand(0);
        const Hands::Hand& right = hands_.hand(1);
        if (left.everSeen) {
            // A dead zone round the middle, so a resting hand leaves the filter open.
            const float x = 2.0f * (left.macro - 0.5f);
            const float y = std::fabs(x) < 0.1f ? 0.0f : (x - (x > 0.0f ? 0.1f : -0.1f)) / 0.9f;
            p.set(filterId_, std::clamp(y, -1.0f, 1.0f));
        }
        if (right.everSeen) p.set(throwId_, clamp01(2.0f * (right.macro - 0.5f)));
    }

    /**
     * @brief Geometry of the panel.
     *
     * A glyph is 7 cells high and a character 6 cells wide, so at kCell and kPanelDistance a line of
     * 20 characters spans 0.50 m -- about 29 degrees -- and a glyph stands 1.7 degrees tall, roughly
     * print at reading distance. Every line drawn below stays inside 20 characters. These are design
     * values and untuned: nobody has put the headset on yet.
     */
    static constexpr float kCell = 0.0042f;         ///< pitch of one font pixel, metres
    static constexpr float kRow = 0.040f;           ///< distance between text rows, metres
    static constexpr float kPanelDistance = 1.0f;   ///< how far in front of the eyes the panel sits

    /** @brief The head-locked panel: in front of the eyes, following the yaw only. */
    Panel headPanel() const
    {
        Panel p;
        const Vec3 fwd = rotate(headPose_.orientation, { 0.0f, 0.0f, -1.0f });
        const float len = std::fmax(std::sqrt(fwd.x * fwd.x + fwd.z * fwd.z), 1.0e-3f);
        const Vec3 f{ fwd.x / len, 0.0f, fwd.z / len };
        p.right = { -f.z, 0.0f, f.x };
        p.up = { 0.0f, 1.0f, 0.0f };
        // Shifted half a 16-character line to the left (the lines are left aligned and 13 to 20
        // characters long), so the block of text sits roughly centred in front of the eyes.
        const float half = 0.5f * 16.0f * 6.0f * kCell;
        p.origin = { headPose_.position.x + f.x * kPanelDistance - p.right.x * half,
                     headPose_.position.y + 0.12f,
                     headPose_.position.z + f.z * kPanelDistance - p.right.z * half };
        return p;
    }

    /** @brief The block at @p beat as the composer named it ("Intro", "Block 3", "Reduction", a set's track), on @p deck. */
    static const char* sectionAt(const Score& s, double beat)
    {
        const char* text = "";
        for (const Marker& m : s.markers) { if (m.beat > beat) break; text = m.text.c_str(); }
        return text;
    }

    /** @brief One part's onsets in a bar: position in the bar 0..1 and velocity. */
    struct Beads { int part = 0; std::vector<std::pair<float, float>> onsets; };

    /** @brief The onsets of bar @p bar of @p s, per part (the kick apart), in part order. */
    static void gatherBar(const Score& s, int bar, std::vector<Beads>& rings, std::vector<float>& kick)
    {
        rings.clear();
        kick.clear();
        if (bar < 0) return;
        const double b0 = 4.0 * bar, b1 = b0 + 4.0;
        auto it = std::lower_bound(s.notes.begin(), s.notes.end(), b0 - 0.1, [](const NoteEvent& n, double b) { return n.beat < b; });
        int index[64];
        for (int& i : index) i = -1;
        for (; it != s.notes.end() && it->beat < b1 - 0.1; ++it) {
            if (it->velocity <= 0.0f) continue;
            const float pos = static_cast<float>(std::clamp((it->beat - b0) / 4.0, 0.0, 0.9999));
            if (it->part == Part::Kick) { kick.push_back(pos); continue; }
            const int p = static_cast<int>(it->part);
            if (p < 0 || p >= 64) continue;
            if (index[p] < 0) { index[p] = static_cast<int>(rings.size()); rings.push_back({ p, {} }); }
            rings[static_cast<size_t>(index[p])].onsets.push_back({ pos, it->velocity });
        }
        std::sort(rings.begin(), rings.end(), [](const Beads& a, const Beads& b) { return a.part < b.part; });
    }

    /** @brief A part's colour in the room: the drums the corona's warm white, the tones their families. */
    static void partColour(int part, float& r, float& g, float& b)
    {
        switch (static_cast<Part>(part)) {
        case Part::Sub: r = 0.44f; g = 0.56f; b = 0.72f; return;
        case Part::Ping: r = 0.44f; g = 0.72f; b = 0.68f; return;
        case Part::Bass: r = 0.85f; g = 0.51f; b = 0.36f; return;
        case Part::Acid: r = 0.84f; g = 0.33f; b = 0.25f; return;
        case Part::Chord: case Part::Drone: case Part::Texture: r = 0.62f; g = 0.75f; b = 0.44f; return;
        default: r = 0.93f; g = 0.84f; b = 0.66f; return;
        }
    }

    /**
     * @brief The logo (Deploy/make_icon.py) in points of light: the corona round a dark centre and a ring with its beads,
     *        centred at (@p u, @p v) on the panel, @p size across.
     */
    void addLogo(const Panel& p, float u, float v, float size)
    {
        const float disc = 0.2f * size;
        for (int k = 0; k < 3; ++k) {
            const float r = disc * (1.08f + 0.14f * static_cast<float>(k));
            const int dots = 28 + 6 * k;
            for (int i = 0; i < dots; ++i) {
                const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(dots);
                scene_.addOn(p, u + r * std::cos(a), v + r * std::sin(a), 0.93f, 0.84f, 0.66f, 0.75f - 0.22f * static_cast<float>(k), 22.0f);
            }
        }
        const float ring = 0.42f * size;
        for (int i = 0; i < 40; ++i) {
            const float a = 6.2831853f * static_cast<float>(i) / 40.0f;
            scene_.addOn(p, u + ring * std::cos(a), v + ring * std::sin(a), 0.47f, 0.44f, 0.41f, 0.35f, 14.0f);
        }
        for (int i = 0; i < 16; i += 3) {
            const float a = 1.5707963f - 6.2831853f * static_cast<float>(i) / 16.0f;
            scene_.addOn(p, u + ring * std::cos(a), v + ring * std::sin(a), i == 0 ? 0.84f : 0.93f, i == 0 ? 0.33f : 0.84f,
                         i == 0 ? 0.25f : 0.66f, 1.0f, 70.0f);
        }
        // The diamond ring: a bead of light on the rim, upper right.
        scene_.addOn(p, u + disc * 0.7071f, v + disc * 0.7071f, 1.0f, 0.96f, 0.88f, 1.0f, 60.0f);
    }

    /**
     * @brief The Eclipse in the room (the file comment): ahead of the player and above, tilted towards them, anchored
     *        where the head was when the session began -- it never moves with the head or the audio.
     */
    void addEclipse(const Now& now, double beat)
    {
        if (!eclipseAnchored_) return;
        const int t = now.trackAt(beat);
        const Score& s = now.set.decks[now.decks.empty() ? 0 : now.decks[static_cast<size_t>(t)]];
        const int bar = static_cast<int>(std::floor(beat / 4.0));
        const float phase = static_cast<float>(beat / 4.0 - std::floor(beat / 4.0));
        // The bar that plays and the one before, crossfaded over the first eighth of the bar: no bead pops in.
        std::vector<Beads> rings[2];
        std::vector<float> kicks[2];
        gatherBar(s, bar, rings[0], kicks[0]);
        gatherBar(s, bar - 1, rings[1], kicks[1]);
        const float fadeIn = std::min(1.0f, phase / 0.125f);
        const float weight[2] = { 0.5f - 0.5f * std::cos(3.14159265f * fadeIn), 0.5f + 0.5f * std::cos(3.14159265f * fadeIn) };
        const Panel& e = eclipse_;
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
                scene_.addOn(e, r * std::cos(a), r * std::sin(a), 0.93f, 0.84f, 0.66f, (0.55f - 0.12f * static_cast<float>(k)) * (0.6f + 0.4f * swell), 34.0f);
            }
        }
        const auto angleOf = [](float pos) { return 1.5707963f - 6.2831853f * pos; };   // bar start at the top, clockwise
        // Conjunctions of the bar that plays: three rings or more on one sixteenth, a ray.
        int count[16] = {};
        for (const Beads& r : rings[0]) {
            bool seen[16] = {};
            for (const auto& o : r.onsets) {
                const int step = std::clamp(static_cast<int>(std::lround(o.first * 16.0f)) % 16, 0, 15);
                if (!seen[step]) { seen[step] = true; ++count[step]; }
            }
        }
        for (int st = 0; st < 16; ++st) {
            if (count[st] < 3) continue;
            const float a = angleOf(static_cast<float>(st) / 16.0f);
            for (int i = 0; i < 18; ++i) {
                const float r = disc * 1.5f + (outer - disc * 1.5f) * static_cast<float>(i) / 17.0f;
                scene_.addOn(e, r * std::cos(a), r * std::sin(a), 0.93f, 0.84f, 0.66f, 0.06f * static_cast<float>(count[st]) * weight[0], 60.0f);
            }
        }
        // The rings and their beads.
        for (int w = 0; w < 2; ++w) {
            const int n = static_cast<int>(rings[w].size());
            for (int i = 0; i < n; ++i) {
                const Beads& r = rings[w][static_cast<size_t>(i)];
                const float rad = inner + (outer - inner) * (n <= 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(n - 1));
                float cr, cg, cb;
                partColour(r.part, cr, cg, cb);
                if (w == 0)
                    for (int d = 0; d < 72; ++d) {
                        const float a = 6.2831853f * static_cast<float>(d) / 72.0f;
                        scene_.addOn(e, rad * std::cos(a), rad * std::sin(a), cr, cg, cb, 0.07f, 18.0f);
                    }
                for (const auto& o : r.onsets) {
                    const float a = angleOf(o.first);
                    const float d = w == 0 ? phase - o.first : 1.0f;
                    const float lit = d >= 0.0f && d < 0.1f ? 0.5f + 0.5f * std::cos(3.14159265f * d / 0.1f) : 0.0f;
                    scene_.addOn(e, rad * std::cos(a), rad * std::sin(a), cr, cg, cb, weight[w] * (0.35f + 0.3f * o.second + 0.35f * lit),
                                 60.0f + 50.0f * o.second + 60.0f * lit);
                }
            }
        }
        // The playhead's hand.
        const float ha = angleOf(phase);
        for (int i = 0; i < 24; ++i) {
            const float r = disc * 1.3f + (outer - disc * 1.3f) * static_cast<float>(i) / 23.0f;
            scene_.addOn(e, r * std::cos(ha), r * std::sin(ha), 0.84f, 0.33f, 0.25f, 0.35f, 26.0f);
        }
    }

    /** @brief Anchors the Eclipse at the first head pose: 3 m ahead, 1.3 m above the eyes, tilted 40 degrees down. */
    void anchorEclipse()
    {
        if (eclipseAnchored_ || !headValid_) return;
        const Vec3 fwd = rotate(headPose_.orientation, { 0.0f, 0.0f, -1.0f });
        const float len = std::fmax(std::sqrt(fwd.x * fwd.x + fwd.z * fwd.z), 1.0e-3f);
        const Vec3 f{ fwd.x / len, 0.0f, fwd.z / len };
        const float tilt = 0.70f;   // radians the disc leans towards the player
        eclipse_.right = { -f.z, 0.0f, f.x };
        // "Up" on the disc: up, leaning back away from the player, so its face looks down at them.
        eclipse_.up = { f.x * std::sin(tilt), std::cos(tilt), f.z * std::sin(tilt) };
        eclipse_.origin = { headPose_.position.x + f.x * 3.0f, headPose_.position.y + 1.3f, headPose_.position.z + f.z * 3.0f };
        eclipseAnchored_ = true;
    }

    /** @brief The performer panel (PLAN 10.2): the track, its block and key, the time, the level, the hands' controls. */
    void buildScene()
    {
        // The floor ring: a fixed horizon, never moved by the audio.
        for (int i = 0; i < 72; ++i) {
            const float a = static_cast<float>(i) / 72.0f * 6.2831853f;
            scene_.add(2.5f * std::sin(a), 0.02f, -2.5f * std::cos(a), 0.30f, 0.27f, 0.22f, 0.4f, 40.0f);
        }
        for (int h = 0; h < 2; ++h) {
            const Hands::Hand& s = hands_.hand(h);
            if (!s.valid) continue;
            const float t = s.pinch;
            scene_.add(s.palm.x, s.palm.y, s.palm.z, 0.93f, 0.84f - 0.4f * t, 0.66f - 0.4f * t, 1.0f, 130.0f);
        }
        if (!headValid_) return;
        anchorEclipse();

        const Panel p = headPanel();
        char line[64];
        if (!player_.ready()) {
            addLogo(p, 0.08f, 0.10f, 0.12f);
            scene_.addText(p, 0.0f, 0.0f, kCell * 1.8f, "UMBRA", 0.93f, 0.84f, 0.66f, 1.0f);
            scene_.addText(p, 0.0f, -0.08f, kCell, "COMPOSING", 0.6f, 0.7f, 0.9f, 0.8f);
            return;
        }
        const std::shared_ptr<const Now> np = player_.now();
        if (!np) return;
        const Now& now = *np;
        const double beat = player_.beat();
        const ParamStore& par = player_.params();
        addEclipse(now, beat);

        // The title line above the rest: the logo and the name.
        addLogo(p, 0.012f, kRow + 0.006f, 0.034f);
        scene_.addText(p, 0.036f, kRow, kCell, "UMBRA", 0.93f, 0.84f, 0.66f, 0.85f);
        float row = 0.0f;
        auto text = [&](const char* t, float r, float g, float b, float a) {
            scene_.addText(p, 0.0f, row, kCell, t, r, g, b, a);
            row -= kRow;
        };
        const int t = now.trackAt(beat);
        const TrackInfo& info = now.tracks[static_cast<size_t>(t)];
        if (now.isSet) std::snprintf(line, sizeof(line), "SET %d  T%d/%d", player_.number(), t + 1, static_cast<int>(now.tracks.size()));
        else std::snprintf(line, sizeof(line), "TRACK %d", player_.number());
        text(line, 0.93f, 0.84f, 0.66f, 1.0f);
        std::snprintf(line, sizeof(line), "%s %s", info.style.c_str(), kFormNames[static_cast<int>(info.form)]);
        text(line, 0.93f, 0.84f, 0.66f, 0.9f);
        const Score& deck = now.set.decks[now.decks[static_cast<size_t>(t)]];
        text(sectionAt(deck, beat), 0.93f, 0.78f, 0.55f, 0.9f);
        std::snprintf(line, sizeof(line), "%s %s %s", kKeyNames[info.key], kScaleNames[info.scale], info.camelot.c_str());
        text(line, 0.70f, 0.85f, 1.00f, 0.9f);
        const TempoMap& tm = now.set.decks[0].tempo;
        const double sec = player_.seconds(), total = tm.secondsAt(now.set.lengthBeats);
        std::snprintf(line, sizeof(line), "%02d:%02d/%02d:%02d %.0f BPM", static_cast<int>(sec) / 60, static_cast<int>(sec) % 60,
                      static_cast<int>(total) / 60, static_cast<int>(total) % 60, tm.bpmAt(beat));
        text(line, 0.93f, 0.84f, 0.60f, 0.9f);
        const float db = player_.levelDb();
        std::snprintf(line, sizeof(line), "%+.1f DB  %s", static_cast<double>(std::max(db, -99.0f)),
                      player_.muted() ? "MUTED" : (player_.playing() ? "PLAY" : "STOP"));
        text(line, 0.93f, 0.84f, 0.60f, 0.9f);
        const float f = par.get(filterId_);
        if (std::fabs(f) < 0.01f) std::snprintf(line, sizeof(line), "FILTER OPEN");
        else std::snprintf(line, sizeof(line), "FILTER %s %d%%", f < 0.0f ? "LP" : "HP", static_cast<int>(std::lround(std::fabs(f) * 100.0f)));
        text(line, 0.60f, 0.75f, 0.95f, 0.75f);
        std::snprintf(line, sizeof(line), "THROW %d%%", static_cast<int>(std::lround(par.get(throwId_) * 100.0f)));
        text(line, 0.60f, 0.75f, 0.95f, 0.75f);
        if (par.getBool(muteKickId_)) text("KICK OUT", 0.84f, 0.33f, 0.25f, 1.0f);

        row -= kRow * 0.3f;
        // Four beat lamps: a raised cosine of the distance to the beat, a continuous pulse.
        const double inBar = beat / 4.0 - std::floor(beat / 4.0);
        const float phase = static_cast<float>(inBar) * 4.0f;
        for (int b = 0; b < 4; ++b) {
            float dist = phase - static_cast<float>(b);
            if (dist < 0.0f) dist += 4.0f;
            const float x = dist < 1.0f ? 0.5f + 0.5f * std::cos(3.14159265f * dist) : 0.0f;
            const float level = player_.playing() ? 0.22f + 0.78f * x : 0.18f;
            scene_.addOn(p, 0.026f * static_cast<float>(b), row, 1.0f, 0.55f + 0.35f * x, 0.25f, level, 150.0f);
        }
    }

    /**
     * @brief The pinches: a pinch acts when it opens again, so both hands closed together can mean something else --
     *        the next track, once, while neither of them then counts as a single pinch. Left alone: play / stop;
     *        right alone: kick out / kick in.
     */
    void gestures()
    {
        const bool closed[2] = { hands_.hand(0).closed, hands_.hand(1).closed };
        if (closed[0] && closed[1]) {
            spoiled_[0] = spoiled_[1] = true;
            if (!bothFired_) { player_.requestNext(); bothFired_ = true; }
        }
        for (int h = 0; h < 2; ++h) {
            if (closed[h] && !handWas_[h] && !closed[1 - h]) spoiled_[h] = false;
            if (!closed[h] && handWas_[h] && !spoiled_[h]) {
                if (h == 0) player_.setPlaying(!player_.playing());
                else player_.params().set(muteKickId_, player_.params().getBool(muteKickId_) ? 0.0f : 1.0f);
            }
            handWas_[h] = closed[h];
        }
        if (!closed[0] && !closed[1]) bothFired_ = false;
    }

    void frame()
    {
        XrFrameWaitInfo wi{ XR_TYPE_FRAME_WAIT_INFO };
        XrFrameState fs{ XR_TYPE_FRAME_STATE };
        if (XR_FAILED(xrWaitFrame(session_, &wi, &fs))) return;
        XrFrameBeginInfo bi{ XR_TYPE_FRAME_BEGIN_INFO };
        xrBeginFrame(session_, &bi);

        const double dt = lastTime_ == 0 ? 1.0 / 72.0 : static_cast<double>(fs.predictedDisplayTime - lastTime_) * 1.0e-9;
        lastTime_ = fs.predictedDisplayTime;
        updateHead(fs.predictedDisplayTime);
        updateHands(fs.predictedDisplayTime);
        bool leftPinch = false, rightPinch = false;
        hands_.update(dt, leftPinch, rightPinch);
        if (player_.ready()) {
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
            vli.space = stageSpace_;
            XrViewState vs{ XR_TYPE_VIEW_STATE };
            uint32_t viewCount = 0;
            xrLocateViews(session_, &vli, &vs, static_cast<uint32_t>(views_.size()), &viewCount, views_.data());
            if ((vs.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) && (vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT)) {
                scene_.begin();
                buildScene();
                scene_.upload();
                projViews.resize(viewCount, { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW });
                for (uint32_t v = 0; v < viewCount; ++v) {
                    SwapchainTarget& t = targets_[v];
                    XrSwapchainImageAcquireInfo ai{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
                    uint32_t index = 0;
                    xrAcquireSwapchainImage(t.swapchain, &ai, &index);
                    XrSwapchainImageWaitInfo swi{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
                    swi.timeout = XR_INFINITE_DURATION;
                    xrWaitSwapchainImage(t.swapchain, &swi);

                    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.images[index].image, 0);
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, t.depth);
                    scene_.draw(multiply(projectionFromFov(views_[v].fov, 0.05f, 100.0f), viewFromPose(views_[v].pose)), t.width, t.height);
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);

                    XrSwapchainImageReleaseInfo ri{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
                    xrReleaseSwapchainImage(t.swapchain, &ri);

                    projViews[v].pose = views_[v].pose;
                    projViews[v].fov = views_[v].fov;
                    projViews[v].subImage.swapchain = t.swapchain;
                    projViews[v].subImage.imageRect = { { 0, 0 }, { t.width, t.height } };
                    projViews[v].subImage.imageArrayIndex = 0;
                }
                layer.space = stageSpace_;
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
        xrEndFrame(session_, &ei);
    }

    android_app* app_;
    std::string dataDir_;
    Config config_;
    TrackPlayer player_;
    Audio audio_{ player_ };
    std::thread composerThread_;
    std::atomic<bool> stopComposer_{ false };
    Scene scene_;
    Hands hands_;
    int filterId_ = -1, throwId_ = -1, muteKickId_ = -1;   ///< perform.filter, .throw and .mute_kick: the hands' controls
    bool handWas_[2] = {}, spoiled_[2] = {}, bothFired_ = false;   ///< gestures(): the pinches as they were
    Panel eclipse_{};                    ///< the Eclipse's plane in the room (anchorEclipse)
    bool eclipseAnchored_ = false;
    CueSender cues_;                     ///< the cue bridge's socket and thread (Cue.h)

    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLConfig eglConfig_ = nullptr;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;

    XrInstance instance_ = XR_NULL_HANDLE;
    XrSystemId system_ = XR_NULL_SYSTEM_ID;
    XrSession session_ = XR_NULL_HANDLE;
    XrSpace stageSpace_ = XR_NULL_HANDLE, viewSpace_ = XR_NULL_HANDLE;
    XrSessionState sessionState_ = XR_SESSION_STATE_UNKNOWN;
    bool sessionRunning_ = false, quit_ = false, handsSupported_ = false, headValid_ = false;
    std::vector<XrView> views_;
    std::vector<SwapchainTarget> targets_;
    XrPosef headPose_{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
    XrTime lastTime_ = 0;

    PFN_xrCreateHandTrackerEXT pfnCreateHandTracker_ = nullptr;
    PFN_xrLocateHandJointsEXT pfnLocateHandJoints_ = nullptr;
    PFN_xrDestroyHandTrackerEXT pfnDestroyHandTracker_ = nullptr;
    XrHandTrackerEXT handTracker_[2] = { XR_NULL_HANDLE, XR_NULL_HANDLE };
};

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
