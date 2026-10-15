/**
 * @file Export.cpp
 * @brief The cues and the DJ loops of an export (Export.h); moved here from tot_render.
 */
#include "tot/Export.h"
#include "tot/Engine.h"
#include "tot/WavWriter.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>

namespace tot {

namespace {

/** @brief @p s escaped for a JSON string. */
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
    if (t.m_bassBar > 0) out.push_back({ sec(t.m_bassBar), prefix + "Bass in" });
    for (size_t r = 0; r < t.m_reductions.size(); ++r) {
        out.push_back({ sec(t.m_reductions[r]), prefix + "Kick out" });
        out.push_back({ sec(t.m_returns[r]), prefix + "Return" });
    }
    if (t.m_outroBar < t.m_bars) out.push_back({ sec(t.m_outroBar), prefix + "Outro" });
}

std::vector<CueAt> setCues(const SetInfo& si, const TempoMap& tm)
{
    std::vector<CueAt> cues;
    for (size_t i = 0; i < si.m_tracks.size(); ++i) {
        const SetTrack& t = si.m_tracks[i];
        const std::string name = "T" + std::to_string(i + 1);
        cues.push_back({ tm.secondsAt(t.m_start), name + " " + t.m_info.m_style + " " + FormNames[static_cast<int>(t.m_info.m_form)] + " " + t.m_info.m_camelot });
        if (i > 0) cues.push_back({ tm.secondsAt(t.m_swapIn), "Swap to " + name });
        for (size_t r = 0; r < t.m_info.m_reductions.size(); ++r) cues.push_back({ tm.secondsAt(t.m_start + 4.0 * t.m_info.m_reductions[r]), name + " kick out" });
    }
    for (const SetLoop& l : si.m_loops) {
        std::string kind = LoopKindNames[static_cast<int>(l.m_kind)];
        kind[0] = static_cast<char>(kind[0] - 'a' + 'A');
        cues.push_back({ tm.secondsAt(l.m_start), kind + " of T" + std::to_string(l.m_from + 1) });
    }
    std::sort(cues.begin(), cues.end(), [](const CueAt& a, const CueAt& b) { return a.m_seconds < b.m_seconds; });
    return cues;
}

bool writeCuesJson(const std::string& path, const std::vector<CueAt>& cues, double rate)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) return false;
    std::fprintf(f, "[\n");
    for (size_t i = 0; i < cues.size(); ++i)
        std::fprintf(f, "{\"seconds\":%.4f,\"sample\":%lld,\"label\":\"%s\"}%s\n", cues[i].m_seconds,
                     static_cast<long long>(std::llround(cues[i].m_seconds * rate)), jsonEscape(cues[i].m_label).c_str(),
                     i + 1 < cues.size() ? "," : "");
    std::fprintf(f, "]\n");
    return std::fclose(f) == 0;
}

namespace {

/** @brief @p s escaped for XML. */
std::string xmlEscape(const std::string& s)
{
    std::string o;
    for (char c : s) {
        switch (c) {
        case '&': o += "&amp;"; break;
        case '<': o += "&lt;"; break;
        case '>': o += "&gt;"; break;
        case '"': o += "&quot;"; break;
        default: o += c; break;
        }
    }
    return o;
}

/** @brief A file URI as rekordbox writes them: file://localhost/G:/dir/name.wav, reserved characters percent-encoded. */
std::string fileUri(const std::string& path)
{
    std::error_code ec;
    std::string p = std::filesystem::absolute(std::filesystem::path(path), ec).generic_string();
    if (ec) p = path;
    std::string o = "file://localhost/";
    for (unsigned char c : p) {
        if (std::isalnum(c) || c == '/' || c == ':' || c == '-' || c == '_' || c == '.' || c == '~') o += static_cast<char>(c);
        else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            o += buf;
        }
    }
    return o;
}

} // namespace

bool writeRekordboxXml(const std::string& path, const std::string& wavPath, const std::string& title, const std::string& tonality,
                       const std::vector<CueAt>& cues, const TempoMap& tempo, double lengthBeats, double rate)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) return false;
    const double seconds = tempo.secondsAt(lengthBeats);
    std::fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<DJ_PLAYLISTS Version=\"1.0.0\">\n");
    std::fprintf(f, "  <PRODUCT Name=\"Totality\" Version=\"%s\" Company=\"Rene Weller\"/>\n", TOT_VERSION);
    std::fprintf(f, "  <COLLECTION Entries=\"1\">\n");
    std::fprintf(f, "    <TRACK TrackID=\"1\" Name=\"%s\" Artist=\"Totality\" Genre=\"Techno\" Kind=\"WAV File\" TotalTime=\"%d\" "
                    "AverageBpm=\"%.2f\" SampleRate=\"%d\" BitRate=\"%d\" Tonality=\"%s\" Location=\"%s\">\n",
                 xmlEscape(title).c_str(), static_cast<int>(std::lround(seconds)), tempo.bpmAt(0.0), static_cast<int>(rate),
                 static_cast<int>(rate * 2 * 24 / 1000), xmlEscape(tonality).c_str(), xmlEscape(fileUri(wavPath)).c_str());
    // The beat grid: a TEMPO from every bar whose tempo differs from the last one written (a set's ramps bar by bar).
    double last = -1.0;
    for (double beat = 0.0; beat < lengthBeats - 1e-9; beat += 4.0) {
        const double a = tempo.secondsAt(beat), b = tempo.secondsAt(std::min(lengthBeats, beat + 4.0));
        const double beats = std::min(4.0, lengthBeats - beat);
        const double bpm = b > a ? beats * 60.0 / (b - a) : tempo.bpmAt(beat);
        if (std::fabs(bpm - last) < 0.005) continue;
        std::fprintf(f, "      <TEMPO Inizio=\"%.3f\" Bpm=\"%.2f\" Metro=\"4/4\" Battito=\"1\"/>\n", a, bpm);
        last = bpm;
    }
    // Every cue a memory cue; the first eight also hot cues A to H.
    for (const CueAt& c : cues)
        std::fprintf(f, "      <POSITION_MARK Name=\"%s\" Type=\"0\" Start=\"%.3f\" Num=\"-1\"/>\n", xmlEscape(c.m_label).c_str(), c.m_seconds);
    for (size_t i = 0; i < cues.size() && i < 8; ++i)
        std::fprintf(f, "      <POSITION_MARK Name=\"%s\" Type=\"0\" Start=\"%.3f\" Num=\"%zu\" Red=\"40\" Green=\"226\" Blue=\"20\"/>\n",
                     xmlEscape(cues[i].m_label).c_str(), cues[i].m_seconds, i);
    std::fprintf(f, "    </TRACK>\n  </COLLECTION>\n  <PLAYLISTS>\n    <NODE Type=\"0\" Name=\"ROOT\" Count=\"1\">\n"
                    "      <NODE Name=\"Totality\" Type=\"1\" KeyType=\"0\" Entries=\"1\">\n        <TRACK Key=\"1\"/>\n"
                    "      </NODE>\n    </NODE>\n  </PLAYLISTS>\n</DJ_PLAYLISTS>\n");
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
        const double src = 4.0 * info.m_peakBar, len = 4.0 * bars;
        Score loop;
        loop.clear(track.m_tempo.bpmAt(src));
        loop.m_seed = track.m_seed;
        loop.m_keyRoot = track.m_keyRoot;
        loop.m_scale = track.m_scale;
        for (int pass = 0; pass < 3; ++pass)
            for (NoteEvent n : track.m_notes)
                if (n.m_beat >= src - 0.05 && n.m_beat < src + len - 0.05) { n.m_beat = n.m_beat - src + pass * len; loop.m_notes.push_back(n); }
        // Every knob where the track has it at the loop's start.
        std::vector<int> ids;
        for (const Gesture& g : track.m_gestures) ids.push_back(g.m_param);
        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        for (int id : ids) {
            Gesture g;
            g.m_param = id;
            g.m_beat = 0.0;
            g.m_length = 0.0;
            g.m_from = g.m_to = track.gestureOffset(id, src + 0.01);
            g.m_shape = GestureShapeE::STEP;
            loop.m_gestures.push_back(g);
        }
        loop.m_levels.push_back(LevelMark{ 0.0, 0.0, -10.0f, track.trimAt(src) });
        for (KnobSet k : track.m_knobs) if (k.m_beat <= src + 1e-9) { k.m_beat = 0.0; loop.m_knobs.push_back(k); }
        loop.m_lengthBeats = 3.0 * len;
        loop.sort();
        auto e = std::make_unique<Engine>();
        e->params().copyValuesFrom(knobs);
        e->prepare(rate, 512);
        e->load(loop);
        const int64_t from = std::llround(loop.m_tempo.secondsAt(2.0 * len) * rate);
        const int64_t count = std::llround(loop.m_tempo.secondsAt(len) * rate);
        std::vector<std::vector<float>> sl(Engine::STEMS, std::vector<float>(512)), sr = sl;
        std::vector<float*> pl, pr;
        for (int s = 0; s < Engine::STEMS; ++s) { pl.push_back(sl[static_cast<size_t>(s)].data()); pr.push_back(sr[static_cast<size_t>(s)].data()); }
        e->setStems(pl.data(), pr.data());
        const std::string base = dir + "/loop" + std::to_string(bars);
        WavWriter mixW, kickW, hatsW, percW;
        if (!mixW.open((base + ".wav").c_str(), static_cast<int>(rate), 2, WavFormatE::PCM24)
            || !kickW.open((base + "_kick.wav").c_str(), static_cast<int>(rate), 2, WavFormatE::FLOAT32)
            || !hatsW.open((base + "_hats.wav").c_str(), static_cast<int>(rate), 2, WavFormatE::FLOAT32)
            || !percW.open((base + "_perc.wav").c_str(), static_cast<int>(rate), 2, WavFormatE::FLOAT32)) return false;
        std::vector<float> L(512), R(512);
        for (int64_t at = 0; at < from + count;) {
            const int m = static_cast<int>(std::min<int64_t>(512, from + count - at));
            e->process(L.data(), R.data(), m);
            // Only the last pass is written.
            const int skip = static_cast<int>(std::clamp<int64_t>(from - at, 0, m));
            if (skip < m) {
                mixW.write(L.data() + skip, R.data() + skip, m - skip);
                kickW.write(pl[Engine::STEM_KICK] + skip, pr[Engine::STEM_KICK] + skip, m - skip);
                hatsW.write(pl[Engine::STEM_HATS] + skip, pr[Engine::STEM_HATS] + skip, m - skip);
                percW.write(pl[Engine::STEM_PERC] + skip, pr[Engine::STEM_PERC] + skip, m - skip);
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
