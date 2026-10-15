/**
 * @file main.cpp
 * @brief tot_render: composes a track from a seed and renders it offline -- the determinism oracle, the MIDI export,
 *        the stems, the loudness report.
 *
 * @code
 *   tot_render [--seed N] [--minutes M] [--bpm B] [--low rumble|sub] [--set "key=value; ..."]
 *              [--out track.wav] [--midi track.mid] [--stems dir] [--rate 48000] [--block 512] [--bench] [--list]
 * @endcode
 * Without --out nothing is written and the render only measures (the loudness report, the time it took).
 */
#include "tot/Engine.h"
#include "tot/Export.h"
#include "tot/Profile.h"
#include "tot/Presets.h"
#include "tot/Leveler.h"
#include "tot/Loudness.h"
#include "tot/Midi.h"
#include "tot/WavWriter.h"
#include "tot/SetFile.h"
#include "tot/compose/Composer.h"
#include "tot/compose/Set.h"
#include "tot/compose/Study.h"
#include "tot/pattern/Rack.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace tot;

namespace {

/** @brief Prints the command line. */
void usage()
{
    std::printf("tot_render [--seed N] [--minutes M] [--bpm B] [--low rumble|sub] [--form arc|peak|endless]\n"
                "           [--set \"key=value; ...\"] [--reroll unit] [--save-set f.totset] [--load-set f.totset] [--study]\n"
                "           [--set-minutes M]  (a DJ set of M minutes on two decks)  [--seconds S]  (its first S seconds only)\n"
                "           [--loops dir] [--score-json f.json] [--decks dir] [--plan]\n"
                "           [--out track.wav] [--midi track.mid] [--stems dir] [--rate 48000] [--block 512]\n"
                "           [--bench] [--quality desktop|quest] [--list] [--dump-params f.json] [--version] [--stats] [--patterns]\n");
}


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

/** @brief A track's info as JSON. */
std::string infoJson(const TrackInfo& t)
{
    std::string s = "{\"style\":\"" + jsonEscape(t.m_style) + "\",\"form\":\"" + FormNames[static_cast<int>(t.m_form)] + "\"";
    char buf[512];
    std::snprintf(buf, sizeof(buf), ",\"bpm\":%.2f,\"key\":%d,\"scale\":%d,\"camelot\":\"%s\",\"monotonic\":%s,\"sub\":%s,\"bars\":%d,"
                  "\"intro\":%d,\"bass\":%d,\"outro\":%d,\"peak\":%d,\"similarity\":%.4f,\"micro\":%.3f,\"density\":%.2f",
                  t.m_bpm, t.m_key, t.m_scale, t.m_camelot.c_str(), t.m_monotonic ? "true" : "false", t.m_subOwns ? "true" : "false", t.m_bars,
                  t.m_introBars, t.m_bassBar, t.m_outroBar, t.m_peakBar, t.m_similarity, t.m_micro, t.m_density);
    s += buf;
    s += ",\"reductions\":[";
    for (size_t i = 0; i < t.m_reductions.size(); ++i) s += (i ? "," : "") + std::to_string(t.m_reductions[i]);
    s += "],\"returns\":[";
    for (size_t i = 0; i < t.m_returns.size(); ++i) s += (i ? "," : "") + std::to_string(t.m_returns[i]);
    s += "],\"layers\":[";
    for (size_t i = 0; i < t.m_layers.size(); ++i) s += (i ? ",\"" : "\"") + jsonEscape(LayerNames[t.m_layers[i]]) + "\"";
    return s + "]}";
}

/**
 * @brief The score for the evaluation (Tools/eval_report.py): the tracks (with their set beat and deck), every note
 *        (beat, part, pitch, velocity), the operations, the tempo points.
 */
bool writeScoreJson(const std::string& path, const Score& notes, const std::vector<std::pair<double, int>>& where,
                    const std::vector<TrackInfo>& tracks, const std::vector<std::pair<BlockOp, int>>& ops,
                    const std::vector<int>& noteDecks)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) return false;
    std::fprintf(f, "{\n\"tracks\":[\n");
    for (size_t i = 0; i < tracks.size(); ++i) {
        const double b0 = where[i].first;
        std::fprintf(f, "{\"start\":%.4f,\"deck\":%d,\"start_s\":%.4f,\"bass_s\":%.4f,\"end_s\":%.4f,\"info\":%s}%s\n", b0,
                     where[i].second, notes.m_tempo.secondsAt(b0), notes.m_tempo.secondsAt(b0 + 4.0 * tracks[i].m_bassBar),
                     notes.m_tempo.secondsAt(b0 + 4.0 * tracks[i].m_bars), infoJson(tracks[i]).c_str(), i + 1 < tracks.size() ? "," : "");
    }
    std::fprintf(f, "],\n\"parts\":[");
    for (int i = 0; i < NumParts; ++i) std::fprintf(f, "%s\"%s\"", i ? "," : "", PartNames[i]);
    std::fprintf(f, "],\n\"notes\":[\n");
    for (size_t i = 0; i < notes.m_notes.size(); ++i) {
        const NoteEvent& n = notes.m_notes[i];
        std::fprintf(f, "[%.5f,%d,%d,%.3f,%d]%s\n", n.m_beat, static_cast<int>(n.m_part), n.m_pitch, static_cast<double>(n.m_velocity),
                     i < noteDecks.size() ? noteDecks[i] : 0, i + 1 < notes.m_notes.size() ? "," : "");
    }
    std::fprintf(f, "],\n\"ops\":[\n");
    for (size_t i = 0; i < ops.size(); ++i)
        std::fprintf(f, "[%.4f,\"%s\",%d,%d]%s\n", ops[i].first.m_beat, OpNames[static_cast<int>(ops[i].first.m_kind)], ops[i].first.m_layer,
                     ops[i].second, i + 1 < ops.size() ? "," : "");
    std::fprintf(f, "],\n\"tempo\":[");
    const auto& pts = notes.m_tempo.points();
    for (size_t i = 0; i < pts.size(); ++i) std::fprintf(f, "%s[%.4f,%.3f]", i ? "," : "", pts[i].m_beat, pts[i].m_bpm);
    std::fprintf(f, "],\n\"length\":%.4f\n}\n", notes.m_lengthBeats);
    return std::fclose(f) == 0;
}

} // namespace

/** @brief Phase 18: a reading's balance -- every part heard, its loudest sample against the kick's and its correction. */
std::string balanceText(const LevelReading& r)
{
    std::string t;
    char buf[64];
    for (int p = 0; p < BalParts; ++p) {
        const float f = r.m_found[static_cast<size_t>(p)];
        if (std::isnan(f)) continue;
        std::snprintf(buf, sizeof buf, " %s %.0f (%+.0f)", BalPartNames[p], f, r.m_bal[static_cast<size_t>(p)]);
        t += buf;
    }
    const float room = r.m_bal[static_cast<size_t>(BalPartE::ROOM)];
    if (room != 0.0f) {
        std::snprintf(buf, sizeof buf, "; the room's return %+.0f", room);
        t += buf;
    }
    return t.empty() ? std::string(" none measured") : t;
}

/** @brief Renders what the command line asks for; the exit code is 0 on success. */
int main(int argc, char** argv)
{
    uint64_t seed = 1;
    double rate = 48000.0;
    int block = 512;
    std::string out, midi, stems, set, saveSetPath, loadSetPath;
    bool quest = false;
    std::string dump;
    bool bench = false, list = false, stats = false, patterns = false, study = false, seedGiven = false, planOnly = false;
    float minutes = -1.0f, bpm = -1.0f;
    int low = -1, form = -1;
    double setMinutes = 0.0, seconds = 0.0;
    std::string decksDir, loopsDir, scoreJson;
    Curation curation;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
        if (!std::strcmp(a, "--seed")) { seed = std::strtoull(next(), nullptr, 10); seedGiven = true; }
        else if (!std::strcmp(a, "--form")) {
            const std::string f = next();
            form = f == "arc" ? 0 : f == "peak" ? 1 : f == "endless" ? 2 : -2;
            if (form == -2) { usage(); return 2; }
        }
        else if (!std::strcmp(a, "--reroll")) curation.reroll(next());
        else if (!std::strcmp(a, "--save-set")) saveSetPath = next();
        else if (!std::strcmp(a, "--load-set")) loadSetPath = next();
        else if (!std::strcmp(a, "--study")) study = true;
        else if (!std::strcmp(a, "--set-minutes")) setMinutes = std::atof(next());
        else if (!std::strcmp(a, "--seconds")) seconds = std::atof(next());
        else if (!std::strcmp(a, "--decks")) decksDir = next();
        else if (!std::strcmp(a, "--loops")) loopsDir = next();
        else if (!std::strcmp(a, "--score-json")) scoreJson = next();
        else if (!std::strcmp(a, "--minutes")) minutes = static_cast<float>(std::atof(next()));
        else if (!std::strcmp(a, "--bpm")) bpm = static_cast<float>(std::atof(next()));
        else if (!std::strcmp(a, "--low")) low = std::strcmp(next(), "sub") == 0 ? 1 : 0;
        else if (!std::strcmp(a, "--set")) { set += next(); set += ";"; }
        else if (!std::strcmp(a, "--out")) out = next();
        else if (!std::strcmp(a, "--midi")) midi = next();
        else if (!std::strcmp(a, "--stems")) stems = next();
        else if (!std::strcmp(a, "--rate")) rate = std::atof(next());
        else if (!std::strcmp(a, "--block")) block = std::max(1, std::atoi(next()));
        else if (!std::strcmp(a, "--bench")) bench = true;
        else if (!std::strcmp(a, "--quality") && i + 1 < argc) quest = !std::strcmp(argv[++i], "quest");
        else if (!std::strcmp(a, "--plan")) planOnly = true;
        else if (!std::strcmp(a, "--list")) list = true;
        else if (!std::strcmp(a, "--stats")) stats = true;
        else if (!std::strcmp(a, "--dump-params")) dump = next();
        else if (!std::strcmp(a, "--version")) { std::printf("Totality %s\n", TOT_VERSION); return 0; }
        else if (!std::strcmp(a, "--patterns")) patterns = true;
        else { usage(); return 2; }
    }

    auto engine = std::make_unique<Engine>();
    ParamStore& p = engine->params();
    if (list) {
        for (int id = 0; id < p.count(); ++id) std::printf("%-28s %s\n", p.key(id).c_str(), p.format(id).c_str());
        return 0;
    }
    if (!dump.empty()) {
        // One object per parameter: the key, the descriptor, the default of this instance (Tools/manual, after Ephemeris).
        FILE* f = std::fopen(dump.c_str(), "wb");
        if (f == nullptr) { std::fprintf(stderr, "cannot write %s\n", dump.c_str()); return 1; }
        auto quoted = [](const char* s) {
            std::string o = "\"";
            for (const char* c = s != nullptr ? s : ""; *c != 0; ++c) { if (*c == '"' || *c == '\\') o += '\\'; o += *c; }
            return o + "\"";
        };
        static const char* const curves[] = { "linear", "log", "int", "choice", "toggle" };
        std::fprintf(f, "[\n");
        for (int id = 0; id < p.count(); ++id) {
            const ParamDesc& d = p.desc(id);
            std::string choices = "[]";
            if (d.m_curve == CurveE::CHOICE && d.m_choices != nullptr) {
                choices = "[";
                for (int c = 0; c <= static_cast<int>(d.m_maxValue); ++c) choices += (c ? ", " : "") + quoted(d.m_choices[c]);
                choices += "]";
            }
            std::fprintf(f, "  {\"key\": %s, \"name\": %s, \"unit\": %s, \"min\": %g, \"max\": %g, \"default\": %g, \"curve\": \"%s\", \"choices\": %s}%s\n",
                         quoted(p.key(id).c_str()).c_str(), quoted(d.m_name).c_str(), quoted(d.m_unit).c_str(),
                         static_cast<double>(d.m_minValue), static_cast<double>(d.m_maxValue), static_cast<double>(p.defaultValue(id)),
                         curves[static_cast<int>(d.m_curve)], choices.c_str(), id + 1 < p.count() ? "," : "");
        }
        std::fprintf(f, "]\n");
        std::fclose(f);
        return 0;
    }
    std::string error;
    if (!loadSetPath.empty()) {
        SetFile sf;
        if (!loadSet(loadSetPath.c_str(), sf, p, &error)) { std::fprintf(stderr, "--load-set: %s\n", error.c_str()); return 2; }
        if (!seedGiven) seed = sf.m_seed;
        for (const auto& r : sf.m_curation.m_rerolls) curation.m_rerolls[r.first] += r.second;
        if (sf.m_minutes > 0.0 && minutes <= 0.0f) minutes = static_cast<float>(sf.m_minutes);
        if (sf.m_set > 0.0 && setMinutes <= 0.0) setMinutes = sf.m_set;
    }
    if (minutes > 0.0f) p.set(p.id(ModuleE::COMPOSE, 0, compose::MINUTES), minutes);
    if (bpm > 0.0f) p.set(p.id(ModuleE::COMPOSE, 0, compose::BPM), bpm);
    if (low >= 0) p.set(p.id(ModuleE::COMPOSE, 0, compose::LOW_OWNER), static_cast<float>(low));
    if (!set.empty() && !p.parseText(set, &error)) { std::fprintf(stderr, "--set: %s\n", error.c_str()); return 2; }
    if (!saveSetPath.empty()) {
        SetFile sf;
        sf.m_seed = seed;
        sf.m_minutes = p.get(p.id(ModuleE::COMPOSE, 0, compose::MINUTES));
        sf.m_set = setMinutes;
        sf.m_curation = curation;
        if (!saveSet(saveSetPath.c_str(), sf, p)) { std::fprintf(stderr, "cannot write %s\n", saveSetPath.c_str()); return 1; }
        std::printf("set: %s\n", saveSetPath.c_str());
    }

    const auto t0 = std::chrono::steady_clock::now();
    auto t1 = t0;
    std::vector<CueAt> cues;
    std::string infoTitle;
    std::string tonality;          // Phase 16: the key for rekordbox ("Am"), a set's first track's
    TempoMap cueTempo;
    double cueLength = 0.0;
    if (setMinutes <= 0.0) {
        TrackRequest req;
        if (bpm > 0.0f) req.m_bpm = bpm;           // what the command line names, the composer does not draw
        if (low >= 0) req.m_lowOwner = low;
        if (form >= 0) req.m_form = static_cast<FormTypeE>(form);
        TrackInfo info;
        Score score = study ? composeStudy(p, seed) : composeTrack(p, seed, req, &curation, std::string(), &info);
        // The Leveler: the loudest part to the style's target (PLAN 8.5), before a sample is written.
        const std::vector<LevelReading> levels = bench || planOnly ? std::vector<LevelReading>{} : levelScore(score, p);
        t1 = std::chrono::steady_clock::now();
        if (quest) engine->setQuality(Engine::QualityE::QUEST);
        engine->prepare(rate, block);
        engine->load(score);

        std::printf("Totality %s  seed %llu  %.1f BPM  key %s  %s  %.0f bars  %.1f s\n", TOT_VERSION,
                    static_cast<unsigned long long>(seed), score.m_tempo.bpmAt(0.0),
                    KeyNames[score.m_keyRoot], (study ? p.getInt(p.id(ModuleE::COMPOSE, 0, compose::LOW_OWNER)) == 1 : info.m_subOwns) ? "sub owns the low end" : "rumble owns the low end",
                    score.m_lengthBeats / 4.0, engine->lengthSeconds());
        std::printf("kick tuned to %.1f Hz; %zu notes, %zu automation curves, %zu operations\n",
                    static_cast<double>(Kick::tuneToKey(score.m_keyRoot, p.getInt(p.id(ModuleE::KICK, 0, kick::TUNE)),
                                                        p.get(p.id(ModuleE::KICK, 0, kick::PITCH_END)))),
                    score.m_notes.size(), score.m_gestures.size(), score.m_ops.size());
        if (!study) {
            std::printf("%s, %s, %s %s (%s), %s; intro %d bars, body from bar %d, outro from bar %d, loudest from bar %d\n",
                        info.m_style.c_str(), FormNames[static_cast<int>(info.m_form)], KeyNames[info.m_key], ScaleNames[info.m_scale],
                        info.m_camelot.c_str(), info.m_subOwns ? "sub bass" : "rumble", info.m_introBars, info.m_bassBar + 1,
                        info.m_outroBar + 1, info.m_peakBar + 1);
            for (size_t r = 0; r < info.m_reductions.size(); ++r)
                std::printf("kick-out bars %d .. %d, return bar %d\n", info.m_reductions[r] + 1, info.m_returns[r], info.m_returns[r] + 1);
            std::string layers;
            for (int l : info.m_layers) { layers += layers.empty() ? "" : ", "; layers += LayerNames[l]; }
            std::printf("archetype: %s; layers in order: %s\n", ArchetypeNames[info.m_archetype], layers.c_str());
            if (info.m_form != FormTypeE::ENDLESS)
                std::printf("opening: %s%s\n", OpeningNames[info.m_opening],
                            info.m_kickBar > 0 ? (", the kick from bar " + std::to_string(info.m_kickBar + 1)).c_str() : ", the kick from bar 1");
            if (info.m_form != FormTypeE::ENDLESS)
                std::printf("ending: %s\n", info.m_kickLeaves > 0 ? ("the kick leaves " + std::to_string(info.m_kickLeaves) + " bars before the end").c_str()
                                                              : "the kick to the end");
            // Phase 8: the figure, the waves' landings and what moves between the operations.
            if (info.m_figure >= 0)
                std::printf("figure: the %s, a motif of %d bar%s, from bar %d\n", LayerNames[info.m_figure], info.m_figureBars,
                            info.m_figureBars == 1 ? "" : "s", info.m_figureBar + 1);
            if (!info.m_landings.empty()) {
                std::string lands;
                for (int l : info.m_landings) lands += (lands.empty() ? "" : ", ") + std::to_string(l + 1);
                std::printf("waves land on bars %s\n", lands.c_str());
            }
            for (const auto& [bar, what] : info.m_moments) std::printf("  bar %4d  %s\n", bar + 1, what.c_str());
            // The composer's presets (Presets.h): per synth, and per lane of the kit.
            std::string sounds;
            for (const SoundPick& k : score.m_sounds) {
                const ModuleE m = static_cast<ModuleE>(k.m_module);
                const std::vector<SoundPreset>& presets = factoryPresets(m);
                if (k.m_preset < 0 || k.m_preset >= static_cast<int>(presets.size())) continue;
                const std::string& key = p.key(p.id(m, k.m_instance, 0));
                sounds += (sounds.empty() ? "" : "; ") + key.substr(0, key.find('.')) + " " + presets[static_cast<size_t>(k.m_preset)].m_name
                        + " (" + presets[static_cast<size_t>(k.m_preset)].m_group + ")";
            }
            if (!sounds.empty()) std::printf("sounds: %s\n", sounds.c_str());
            std::printf("corridor over the body: bar similarity %.3f, micro-change %.2f dB, %.1f onsets a bar\n", info.m_similarity,
                        info.m_micro, info.m_density);
        }
        for (const LevelReading& r : levels) {
            std::printf("level: the loudest part (bar %.0f) measured %.1f LUFS, target %.1f, trim %+.1f dB (%.1f after the first)\n",
                        score.m_levels.empty() ? 0.0 : score.m_levels[0].m_peakBeat / 4.0 + 1.0, r.m_measured, r.m_target, r.m_trim, r.m_after);
            std::printf("balance against the kick (loudest sample, dB; correction):%s\n", balanceText(r).c_str());
        }
        for (const BlockOp& o : score.m_ops) {
            std::printf("  bar %4.0f  %-8s %s\n", o.m_beat / 4.0 + 1.0, OpNames[static_cast<int>(o.m_kind)],
                        o.m_layer >= 0 ? LayerNames[o.m_layer] : "");
        }
        if (stats) {
            // Symbolic repetition (PLAN 7.9): per part, the share of the body's bars (the middle three fifths) whose onsets
            // on the sixteenth grid equal the bar before's, and the share of bars it plays in.
            const int bars = static_cast<int>(score.m_lengthBeats / 4.0);
            std::vector<std::vector<uint32_t>> grid(NumParts, std::vector<uint32_t>(static_cast<size_t>(bars), 0u));
            for (const NoteEvent& n : score.m_notes) {
                const int step = static_cast<int>(std::floor(n.m_beat * 4.0 + 0.5));
                const int bar = step / 16;
                if (bar >= 0 && bar < bars) grid[static_cast<size_t>(n.m_part)][static_cast<size_t>(bar)] |= 1u << (step % 16);
            }
            std::printf("repetition per part over bars %d .. %d (same as the bar before / playing):\n", bars / 5, bars - bars / 5);
            for (int pi = 0; pi < NumParts; ++pi) {
                int same = 0, playing = 0, total = 0;
                for (int b = bars / 5 + 1; b < bars - bars / 5; ++b) {
                    const uint32_t g = grid[static_cast<size_t>(pi)][static_cast<size_t>(b)];
                    if (g == 0) continue;
                    ++playing;
                    same += g == grid[static_cast<size_t>(pi)][static_cast<size_t>(b - 1)] ? 1 : 0;
                }
                total = bars - 2 * (bars / 5) - 1;
                if (playing > 0) std::printf("  %-7s %4.0f %% same  %4.0f %% playing\n", PartNames[pi], 100.0 * same / playing, 100.0 * playing / total);
            }
        }
        if (patterns) {
            // The first bar of every block of every layer in mini-notation (PLAN 6.7).
            const RackPlan plan = makeRackPlan(p, mixSeed(seed, 1));
            static const char* const Sound[NumLayers] = { "bd", "bd:1", "hh", "hh:1", "oh", "ride", "cp", "cp", "cp:1", "shaker",
                                                            "lt", "rim", "bass", "ping", "chord", "drone", "acid", "tex" };
            for (int li = 0; li < NumLayers; ++li) {
                if (static_cast<LayerIdE>(li) == LayerIdE::CLAP_B) continue;
                std::printf("  %-11s \"%s\"\n", LayerNames[li], miniNotation(plan, static_cast<LayerIdE>(li), 32, Sound[li]).c_str());
            }
        }
        if (!midi.empty()) {
            if (!writeMidiFile(score, midi.c_str(), "Totality", &p)) { std::fprintf(stderr, "cannot write %s\n", midi.c_str()); return 1; }
            std::printf("MIDI: %s\n", midi.c_str());
        }
        if (!study) {
            trackCues(info, 0.0, score.m_tempo, std::string(), cues);
            tonality = std::string(KeyNames[info.m_key]) + "m";
            cueTempo = score.m_tempo;
            cueLength = score.m_lengthBeats;
            infoTitle = "Totality " + info.m_style + " " + FormNames[static_cast<int>(info.m_form)] + " " + KeyNames[info.m_key] + " "
                      + info.m_camelot + " seed " + std::to_string(seed);
            if (!scoreJson.empty()) {
                std::vector<std::pair<BlockOp, int>> ops;
                for (const BlockOp& o : score.m_ops) ops.push_back({ o, 0 });
                if (!writeScoreJson(scoreJson, score, { { 0.0, 0 } }, { info }, ops, {})) { std::fprintf(stderr, "cannot write %s\n", scoreJson.c_str()); return 1; }
                std::printf("score: %s\n", scoreJson.c_str());
            }
            if (!loopsDir.empty() && !planOnly) {
                if (!renderLoops(p, score, info, loopsDir, rate)) { std::fprintf(stderr, "cannot write the loops\n"); return 1; }
                std::printf("loops: %s (4 and 8 bars from bar %d, each with _kick, _hats, _perc)\n", loopsDir.c_str(), info.m_peakBar + 1);
            }
        }
    } else {
        // A set (PLAN 7.7): its tracks on the decks, levelled one by one, mixed.
        SetInfo si;
        SetScore setScore = composeSet(p, seed, setMinutes, &curation, &si);
        const std::vector<LevelReading> levels = bench || planOnly ? std::vector<LevelReading>{} : levelSet(setScore, p);
        t1 = std::chrono::steady_clock::now();
        if (quest) engine->setQuality(Engine::QualityE::QUEST);
        engine->prepare(rate, block);
        engine->loadSet(setScore);
        std::printf("Totality %s  set of seed %llu, %s, %zu tracks, %zu borrowed loops, %zu breaks, %.1f min\n", TOT_VERSION,
                    static_cast<unsigned long long>(seed), DramaturgyNames[static_cast<int>(si.m_dramaturgy)], si.m_tracks.size(),
                    si.m_loops.size(), si.m_breaks.size(), engine->lengthSeconds() / 60.0);
        const TempoMap& tm = setScore.m_decks[0].m_tempo;
        for (size_t i = 0; i < si.m_tracks.size(); ++i) {
            const SetTrack& t = si.m_tracks[i];
            std::printf("  T%-2zu deck %c  %6.2f min  %5.1f BPM  %-10s %-9s %-7s %-2s %-16s %-4s  %d bars, swap in %6.2f, energy %.2f; "
                        "opening: %s, the kick from bar %d, leaving %d bars before the end\n",
                        i + 1, 'A' + t.m_deck, tm.secondsAt(t.m_start) / 60.0, t.m_info.m_bpm, t.m_info.m_style.c_str(), ArchetypeNames[t.m_info.m_archetype],
                        FormNames[static_cast<int>(t.m_info.m_form)], KeyNames[t.m_info.m_key], ScaleNames[t.m_info.m_scale],
                        t.m_info.m_camelot.c_str(), t.m_info.m_bars, tm.secondsAt(t.m_swapIn) / 60.0, t.m_energy, OpeningNames[t.m_info.m_opening],
                        t.m_info.m_kickBar + 1, t.m_info.m_kickLeaves);
        }
        for (const SetLoop& l : si.m_loops)
            std::printf("  %s of T%d (%d bars) on deck C from %.2f to %.2f min\n", LoopKindNames[static_cast<int>(l.m_kind)], l.m_from + 1, l.m_bars,
                        tm.secondsAt(l.m_start) / 60.0, tm.secondsAt(l.m_end) / 60.0);
        // Phase 9: the DJ's hand, counted by kind.
        if (!si.m_moves.empty()) {
            int count[static_cast<int>(MoveKindE::COUNT)] = {};
            for (const SetMove& m : si.m_moves) ++count[static_cast<int>(m.m_kind)];
            std::string moves;
            for (int k = 0; k < static_cast<int>(MoveKindE::COUNT); ++k)
                if (count[k] > 0) moves += (moves.empty() ? "" : ", ") + std::to_string(count[k]) + " " + MoveNames[k];
            std::printf("  the DJ's hand: %zu moves (%s)\n", si.m_moves.size(), moves.c_str());
        }
        // Phase 9: how much of the set layers sources (Mix-Dok. 6: "zwei bis drei Tracks laufen ständig"): per bar, the
        // tracks heard (from their fader's opening to its closing) and the third deck's loops.
        {
            const double blend = p.getInt(p.id(ModuleE::SET, 0, set::BLEND_BARS)) == 0 ? 64.0 : 128.0;
            int two = 0, three = 0, bars = 0;
            for (double beat = 2.0; beat < setScore.m_lengthBeats; beat += 4.0, ++bars) {
                int heard = 0;
                for (size_t i = 0; i < si.m_tracks.size(); ++i) {
                    const SetTrack& t = si.m_tracks[i];
                    const double from = i == 0 ? t.m_start : t.m_swapIn - blend;
                    const double to = i + 1 < si.m_tracks.size() ? t.m_swapOut + 96.0 : t.m_end;
                    if (beat >= from && beat < to) ++heard;
                }
                for (const SetLoop& l : si.m_loops) if (beat >= l.m_start && beat < l.m_end) ++heard;
                two += heard >= 2 ? 1 : 0;
                three += heard >= 3 ? 1 : 0;
            }
            const double setMin = tm.secondsAt(setScore.m_lengthBeats) / 60.0;
            std::printf("  %.1f tracks an hour; two sources or more in %.0f %% of the bars, three in %.0f %%\n",
                        si.m_tracks.size() * 60.0 / std::max(1.0, setMin), 100.0 * two / std::max(1, bars), 100.0 * three / std::max(1, bars));
        }
        size_t k = 0;
        for (const LevelReading& r : levels)
            std::printf("  level %zu: measured %.1f LUFS, target %.1f, trim %+.1f dB; balance%s\n", ++k, r.m_measured, r.m_target, r.m_trim,
                        balanceText(r).c_str());
        if (!midi.empty()) {
            if (!writeMidiFile(flattenSet(setScore), midi.c_str(), "Totality", &p)) { std::fprintf(stderr, "cannot write %s\n", midi.c_str()); return 1; }
            std::printf("MIDI: %s\n", midi.c_str());
        }
        // The set's cues: every track, every swap, every loop.
        std::vector<std::pair<double, int>> where;
        std::vector<TrackInfo> infos;
        for (const SetTrack& t : si.m_tracks) { where.push_back({ t.m_start, t.m_deck }); infos.push_back(t.m_info); }
        cues = setCues(si, tm);
        tonality = si.m_tracks.empty() ? std::string() : std::string(KeyNames[si.m_tracks[0].m_info.m_key]) + "m";
        cueTempo = tm;
        cueLength = setScore.m_lengthBeats;
        infoTitle = std::string("Totality set ") + DramaturgyNames[static_cast<int>(si.m_dramaturgy)] + " seed " + std::to_string(seed);
        if (!scoreJson.empty()) {
            std::vector<std::pair<BlockOp, int>> ops;   // every deck's operations, with the deck
            for (int d = 0; d < Decks; ++d) for (const BlockOp& o : setScore.m_decks[d].m_ops) ops.push_back({ o, d });
            std::stable_sort(ops.begin(), ops.end(), [](const auto& a, const auto& b) { return a.first.m_beat < b.first.m_beat; });
            // Every deck's notes, with the deck (the loops on deck C are another track's).
            Score all = flattenSet(setScore);
            all.m_notes.clear();
            std::vector<std::pair<NoteEvent, int>> tagged;
            for (int d = 0; d < Decks; ++d) for (const NoteEvent& n : setScore.m_decks[d].m_notes) tagged.push_back({ n, d });
            std::stable_sort(tagged.begin(), tagged.end(), [](const auto& a, const auto& b) { return a.first.m_beat < b.first.m_beat; });
            std::vector<int> decksOf;
            for (const auto& t : tagged) { all.m_notes.push_back(t.first); decksOf.push_back(t.second); }
            if (!writeScoreJson(scoreJson, all, where, infos, ops, decksOf)) { std::fprintf(stderr, "cannot write %s\n", scoreJson.c_str()); return 1; }
            std::printf("score: %s\n", scoreJson.c_str());
        }
    }
    if (planOnly) return 0;   // --plan: what was composed, nothing rendered
    WavWriter wav;
    if (!out.empty() && !wav.open(out.c_str(), static_cast<int>(rate), 2, WavFormatE::PCM24)) {
        std::fprintf(stderr, "cannot write %s\n", out.c_str());
        return 1;
    }
    if (!out.empty()) {
        // The cues (PLAN 9): in the WAV (a cue chunk with labels) and as JSON beside it; the tags.
        for (const CueAt& c : cues) wav.addCue(static_cast<uint64_t>(std::llround(c.m_seconds * rate)), c.m_label);
        if (!infoTitle.empty()) wav.setInfo("INAM", infoTitle);
        wav.setInfo("ISFT", std::string("Totality ") + TOT_VERSION);
        wav.setInfo("IGNR", "Techno");
        if (!cues.empty() && !writeCuesJson(out + ".cues.json", cues, rate)) { std::fprintf(stderr, "cannot write the cues\n"); return 1; }
        // Phase 16: the WAV as a rekordbox collection, its grid and its cues.
        if (cueLength > 0.0 && !writeRekordboxXml(out + ".rekordbox.xml", out, infoTitle, tonality, cues, cueTempo, cueLength, rate)) {
            std::fprintf(stderr, "cannot write the rekordbox xml\n");
            return 1;
        }
    }
    std::vector<std::unique_ptr<WavWriter>> stemFiles;
    std::vector<std::vector<float>> stemBufL, stemBufR;
    std::vector<float*> stemL, stemR;
    if (!stems.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(stems, ec);
        for (int s = 0; s < Engine::STEMS; ++s) {
            stemFiles.push_back(std::make_unique<WavWriter>());
            const std::string path = stems + "/" + Engine::stemName(s) + ".wav";
            if (!stemFiles.back()->open(path.c_str(), static_cast<int>(rate), 2, WavFormatE::FLOAT32)) {
                std::fprintf(stderr, "cannot write %s\n", path.c_str());
                return 1;
            }
            stemBufL.emplace_back(static_cast<size_t>(block), 0.0f);
            stemBufR.emplace_back(static_cast<size_t>(block), 0.0f);
        }
        for (int s = 0; s < Engine::STEMS; ++s) { stemL.push_back(stemBufL[static_cast<size_t>(s)].data()); stemR.push_back(stemBufR[static_cast<size_t>(s)].data()); }
        engine->setStems(stemL.data(), stemR.data());
    }

    // Deck taps (a set's decks after their channels).
    std::vector<std::unique_ptr<WavWriter>> tapFiles;
    std::vector<std::vector<float>> tapBufL, tapBufR;
    std::vector<float*> tapL, tapR;
    if (!decksDir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(decksDir, ec);
        for (int k = 0; k < Decks; ++k) {
            tapFiles.push_back(std::make_unique<WavWriter>());
            const std::string path = decksDir + "/deck" + std::string(1, static_cast<char>('A' + k)) + ".wav";
            if (!tapFiles.back()->open(path.c_str(), static_cast<int>(rate), 2, WavFormatE::FLOAT32)) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); return 1; }
            tapBufL.emplace_back(static_cast<size_t>(block), 0.0f);
            tapBufR.emplace_back(static_cast<size_t>(block), 0.0f);
        }
        for (int k = 0; k < Decks; ++k) { tapL.push_back(tapBufL[static_cast<size_t>(k)].data()); tapR.push_back(tapBufR[static_cast<size_t>(k)].data()); }
        engine->setDeckTaps(tapL.data(), tapR.data());
    }

    LoudnessMeter meter;
    meter.prepare(rate);
    std::vector<float> L(static_cast<size_t>(block)), R(static_cast<size_t>(block));
    int64_t total = static_cast<int64_t>(engine->lengthSeconds() * rate) + static_cast<int64_t>(2.0 * rate);
    if (seconds > 0.0) total = std::min(total, static_cast<int64_t>(seconds * rate));   // --seconds: the beginning only
    int64_t done = 0;
    const auto t2 = std::chrono::steady_clock::now();
    while (done < total) {
        const int n = static_cast<int>(std::min<int64_t>(block, total - done));
        engine->process(L.data(), R.data(), n);
        if (!bench) meter.process(L.data(), R.data(), n);
        if (!out.empty()) wav.write(L.data(), R.data(), n);
        for (size_t s = 0; s < stemFiles.size(); ++s) stemFiles[s]->write(stemL[s], stemR[s], n);
        for (size_t d = 0; d < tapFiles.size(); ++d) tapFiles[d]->write(tapL[d], tapR[d], n);
        done += n;
    }
    const auto t3 = std::chrono::steady_clock::now();
    if (!out.empty()) wav.close();
    for (auto& f : stemFiles) f->close();
    for (auto& f : tapFiles) f->close();

    const double composeS = std::chrono::duration<double>(t1 - t0).count();
    const double renderS = std::chrono::duration<double>(t3 - t2).count();
    const double audioS = static_cast<double>(total) / rate;
    std::printf("composed and levelled in %.3f s; rendered %.1f s of audio in %.2f s: %.0f x real time, %.2f %% of a core\n", composeS,
                audioS, renderS, audioS / renderS, 100.0 * renderS / audioS);
#ifdef TOT_PROFILE
    // What each stage cost (Profile.h): its share of the render and of a core in real time.
    {
        double sum = 0.0;
        for (double v : prof::ns) sum += v;
        for (int k = 0; k < prof::COUNT; ++k)
            std::printf("  %-18s %5.1f %% of the render  %6.3f %% of a core\n", prof::Names[k], 100.0 * prof::ns[k] / std::max(1.0, sum),
                        100.0 * prof::ns[k] * 1.0e-9 / std::max(1e-9, audioS));
    }
#endif
    if (!bench) {
        const LoudnessReport r = meter.report();
        std::printf("loudness %.1f LUFS integrated, %.1f LUFS short-term max, true peak %.2f dBTP, LRA %.1f LU, crest %.1f dB,"
                    " correlation %.2f\n", r.m_integrated, r.m_shortTermMax, r.m_truePeak, r.m_range, r.m_crest, r.m_correlation);
    }
    if (!out.empty()) std::printf("WAV: %s\n", out.c_str());
    return 0;
}
