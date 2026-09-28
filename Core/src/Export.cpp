/**
 * @file Export.cpp
 * @brief The cues and the DJ loops of an export (Export.h); moved here from tot_render.
 */
#include "tot/Export.h"
#include "tot/Engine.h"
#include "tot/WavWriter.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>

namespace tot {

namespace {

std::string jsonEscape(const std::string& s)
{
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (static_cast<unsigned char>(c) < 0x20) o += ' ';
        else o += c;
    }
    return o;
}

} // namespace

void trackCues(const TrackInfo& t, double at, const TempoMap& tempo, const std::string& prefix, std::vector<CueAt>& out)
{
    const auto sec = [&](int bar) { return tempo.secondsAt(at + 4.0 * bar); };
    if (t.bassBar > 0) out.push_back({ sec(t.bassBar), prefix + "Bass in" });
    for (size_t r = 0; r < t.reductions.size(); ++r) {
        out.push_back({ sec(t.reductions[r]), prefix + "Kick out" });
        out.push_back({ sec(t.returns[r]), prefix + "Return" });
    }
    if (t.outroBar < t.bars) out.push_back({ sec(t.outroBar), prefix + "Outro" });
}

std::vector<CueAt> setCues(const SetInfo& si, const TempoMap& tm)
{
    std::vector<CueAt> cues;
    for (size_t i = 0; i < si.tracks.size(); ++i) {
        const SetTrack& t = si.tracks[i];
        const std::string name = "T" + std::to_string(i + 1);
        cues.push_back({ tm.secondsAt(t.start), name + " " + t.info.style + " " + kFormNames[static_cast<int>(t.info.form)] + " " + t.info.camelot });
        if (i > 0) cues.push_back({ tm.secondsAt(t.swapIn), "Swap to " + name });
        for (size_t r = 0; r < t.info.reductions.size(); ++r) cues.push_back({ tm.secondsAt(t.start + 4.0 * t.info.reductions[r]), name + " kick out" });
    }
    for (const SetLoop& l : si.loops) {
        std::string kind = kLoopKindNames[static_cast<int>(l.kind)];
        kind[0] = static_cast<char>(kind[0] - 'a' + 'A');
        cues.push_back({ tm.secondsAt(l.start), kind + " of T" + std::to_string(l.from + 1) });
    }
    std::sort(cues.begin(), cues.end(), [](const CueAt& a, const CueAt& b) { return a.seconds < b.seconds; });
    return cues;
}

bool writeCuesJson(const std::string& path, const std::vector<CueAt>& cues, double rate)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) return false;
    std::fprintf(f, "[\n");
    for (size_t i = 0; i < cues.size(); ++i)
        std::fprintf(f, "{\"seconds\":%.4f,\"sample\":%lld,\"label\":\"%s\"}%s\n", cues[i].seconds,
                     static_cast<long long>(std::llround(cues[i].seconds * rate)), jsonEscape(cues[i].label).c_str(),
                     i + 1 < cues.size() ? "," : "");
    std::fprintf(f, "]\n");
    return std::fclose(f) == 0;
}

/*
 * DJ loops (PLAN 9): 4 and 8 bars of a track's loudest block, seamless -- the bars rendered three times over, the
 *        last pass kept, so its start carries the tails of the pass before it as a loop played round does -- as the mix
 *        and as kick, hats and perc alone (the stems, before the master).
 */
bool renderLoops(const ParamStore& knobs, const Score& track, const TrackInfo& info, const std::string& dir, double rate)
{
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    for (int bars : { 4, 8 }) {
        const double src = 4.0 * info.peakBar, len = 4.0 * bars;
        Score loop;
        loop.clear(track.tempo.bpmAt(src));
        loop.seed = track.seed;
        loop.keyRoot = track.keyRoot;
        loop.scale = track.scale;
        for (int pass = 0; pass < 3; ++pass)
            for (NoteEvent n : track.notes)
                if (n.beat >= src - 0.05 && n.beat < src + len - 0.05) { n.beat = n.beat - src + pass * len; loop.notes.push_back(n); }
        // Every knob where the track has it at the loop's start.
        std::vector<int> ids;
        for (const Gesture& g : track.gestures) ids.push_back(g.param);
        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        for (int id : ids) {
            Gesture g;
            g.param = id;
            g.beat = 0.0;
            g.length = 0.0;
            g.from = g.to = track.gestureOffset(id, src + 0.01);
            g.shape = GestureShape::Step;
            loop.gestures.push_back(g);
        }
        loop.levels.push_back(LevelMark{ 0.0, 0.0, -10.0f, track.trimAt(src) });
        for (KnobSet k : track.knobs) if (k.beat <= src + 1e-9) { k.beat = 0.0; loop.knobs.push_back(k); }
        loop.lengthBeats = 3.0 * len;
        loop.sort();
        auto e = std::make_unique<Engine>();
        e->params().copyValuesFrom(knobs);
        e->prepare(rate, 512);
        e->load(loop);
        const int64_t from = std::llround(loop.tempo.secondsAt(2.0 * len) * rate);
        const int64_t count = std::llround(loop.tempo.secondsAt(len) * rate);
        std::vector<std::vector<float>> sl(Engine::kStems, std::vector<float>(512)), sr = sl;
        std::vector<float*> pl, pr;
        for (int s = 0; s < Engine::kStems; ++s) { pl.push_back(sl[static_cast<size_t>(s)].data()); pr.push_back(sr[static_cast<size_t>(s)].data()); }
        e->setStems(pl.data(), pr.data());
        const std::string base = dir + "/loop" + std::to_string(bars);
        WavWriter mixW, kickW, hatsW, percW;
        if (!mixW.open((base + ".wav").c_str(), static_cast<int>(rate), 2, WavFormat::Pcm24)
            || !kickW.open((base + "_kick.wav").c_str(), static_cast<int>(rate), 2, WavFormat::Float32)
            || !hatsW.open((base + "_hats.wav").c_str(), static_cast<int>(rate), 2, WavFormat::Float32)
            || !percW.open((base + "_perc.wav").c_str(), static_cast<int>(rate), 2, WavFormat::Float32)) return false;
        std::vector<float> L(512), R(512);
        for (int64_t at = 0; at < from + count;) {
            const int m = static_cast<int>(std::min<int64_t>(512, from + count - at));
            e->process(L.data(), R.data(), m);
            // Only the last pass is written.
            const int skip = static_cast<int>(std::clamp<int64_t>(from - at, 0, m));
            if (skip < m) {
                mixW.write(L.data() + skip, R.data() + skip, m - skip);
                kickW.write(pl[Engine::kStemKick] + skip, pr[Engine::kStemKick] + skip, m - skip);
                hatsW.write(pl[Engine::kStemHats] + skip, pr[Engine::kStemHats] + skip, m - skip);
                percW.write(pl[Engine::kStemPerc] + skip, pr[Engine::kStemPerc] + skip, m - skip);
            }
            at += m;
        }
        mixW.close();
        kickW.close();
        hatsW.close();
        percW.close();
    }
    return true;
}

} // namespace tot
