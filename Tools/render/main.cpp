/**
 * @file main.cpp
 * @brief umb_render: composes a track from a seed and renders it offline -- the determinism oracle, the MIDI export,
 *        the stems, the loudness report.
 *
 * @code
 *   umb_render [--seed N] [--minutes M] [--bpm B] [--low rumble|sub] [--set "key=value; ..."]
 *              [--out track.wav] [--midi track.mid] [--stems dir] [--rate 48000] [--block 512] [--bench] [--list]
 * @endcode
 * Without --out nothing is written and the render only measures (the loudness report, the time it took).
 */
#include "umb/Engine.h"
#include "umb/Loudness.h"
#include "umb/Midi.h"
#include "umb/WavWriter.h"
#include "umb/compose/Study.h"
#include "umb/pattern/Rack.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace umb;

namespace {

void usage()
{
    std::printf("umb_render [--seed N] [--minutes M] [--bpm B] [--low rumble|sub] [--set \"key=value; ...\"]\n"
                "           [--out track.wav] [--midi track.mid] [--stems dir] [--rate 48000] [--block 512]\n"
                "           [--bench] [--list]\n");
}

} // namespace

int main(int argc, char** argv)
{
    uint64_t seed = 1;
    double rate = 48000.0;
    int block = 512;
    std::string out, midi, stems, set;
    bool bench = false, list = false;
    float minutes = -1.0f, bpm = -1.0f;
    int low = -1;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
        if (!std::strcmp(a, "--seed")) seed = std::strtoull(next(), nullptr, 10);
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
        else if (!std::strcmp(a, "--list")) list = true;
        else { usage(); return 2; }
    }

    auto engine = std::make_unique<Engine>();
    ParamStore& p = engine->params();
    if (list) {
        for (int id = 0; id < p.count(); ++id) std::printf("%-28s %s\n", p.key(id).c_str(), p.format(id).c_str());
        return 0;
    }
    if (minutes > 0.0f) p.set(p.id(Module::Compose, 0, compose::Minutes), minutes);
    if (bpm > 0.0f) p.set(p.id(Module::Compose, 0, compose::Bpm), bpm);
    if (low >= 0) p.set(p.id(Module::Compose, 0, compose::LowOwner), static_cast<float>(low));
    std::string error;
    if (!set.empty() && !p.parseText(set, &error)) { std::fprintf(stderr, "--set: %s\n", error.c_str()); return 2; }

    const auto t0 = std::chrono::steady_clock::now();
    const Score score = composeStudy(p, seed);
    const auto t1 = std::chrono::steady_clock::now();
    engine->prepare(rate, block);
    engine->load(score);

    std::printf("Umbra %s  seed %llu  %.1f BPM  key %s  %s  %.0f bars  %.1f s\n", UMB_VERSION,
                static_cast<unsigned long long>(seed), p.get(p.id(Module::Compose, 0, compose::Bpm)),
                kKeyNames[score.keyRoot], p.getInt(p.id(Module::Compose, 0, compose::LowOwner)) == 1 ? "sub owns the low end" : "rumble owns the low end",
                score.lengthBeats / 4.0, engine->lengthSeconds());
    std::printf("kick tuned to %.1f Hz; %zu notes, %zu automation curves, %zu operations\n",
                static_cast<double>(Kick::tuneToKey(score.keyRoot, p.getInt(p.id(Module::Kick, 0, kick::Tune)),
                                                    p.get(p.id(Module::Kick, 0, kick::PitchEnd)))),
                score.notes.size(), score.gestures.size(), score.ops.size());
    for (const BlockOp& o : score.ops) {
        std::printf("  bar %4.0f  %-8s %s\n", o.beat / 4.0 + 1.0, kOpNames[static_cast<int>(o.kind)],
                    o.layer >= 0 ? kLayerNames[o.layer] : "");
    }
    if (!midi.empty()) {
        if (!writeMidiFile(score, midi.c_str(), "Umbra", &p)) { std::fprintf(stderr, "cannot write %s\n", midi.c_str()); return 1; }
        std::printf("MIDI: %s\n", midi.c_str());
    }

    WavWriter wav;
    if (!out.empty() && !wav.open(out.c_str(), static_cast<int>(rate), 2, WavFormat::Pcm24)) {
        std::fprintf(stderr, "cannot write %s\n", out.c_str());
        return 1;
    }
    std::vector<std::unique_ptr<WavWriter>> stemFiles;
    std::vector<std::vector<float>> stemBufL, stemBufR;
    std::vector<float*> stemL, stemR;
    if (!stems.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(stems, ec);
        for (int s = 0; s < Engine::kStems; ++s) {
            stemFiles.push_back(std::make_unique<WavWriter>());
            const std::string path = stems + "/" + Engine::stemName(s) + ".wav";
            if (!stemFiles.back()->open(path.c_str(), static_cast<int>(rate), 2, WavFormat::Float32)) {
                std::fprintf(stderr, "cannot write %s\n", path.c_str());
                return 1;
            }
            stemBufL.emplace_back(static_cast<size_t>(block), 0.0f);
            stemBufR.emplace_back(static_cast<size_t>(block), 0.0f);
        }
        for (int s = 0; s < Engine::kStems; ++s) { stemL.push_back(stemBufL[static_cast<size_t>(s)].data()); stemR.push_back(stemBufR[static_cast<size_t>(s)].data()); }
        engine->setStems(stemL.data(), stemR.data());
    }

    LoudnessMeter meter;
    meter.prepare(rate);
    std::vector<float> L(static_cast<size_t>(block)), R(static_cast<size_t>(block));
    const int64_t total = static_cast<int64_t>(engine->lengthSeconds() * rate) + static_cast<int64_t>(2.0 * rate);
    int64_t done = 0;
    const auto t2 = std::chrono::steady_clock::now();
    while (done < total) {
        const int n = static_cast<int>(std::min<int64_t>(block, total - done));
        engine->process(L.data(), R.data(), n);
        if (!bench) meter.process(L.data(), R.data(), n);
        if (!out.empty()) wav.write(L.data(), R.data(), n);
        for (size_t s = 0; s < stemFiles.size(); ++s) stemFiles[s]->write(stemL[s], stemR[s], n);
        done += n;
    }
    const auto t3 = std::chrono::steady_clock::now();
    if (!out.empty()) wav.close();
    for (auto& f : stemFiles) f->close();

    const double composeS = std::chrono::duration<double>(t1 - t0).count();
    const double renderS = std::chrono::duration<double>(t3 - t2).count();
    const double audioS = static_cast<double>(total) / rate;
    std::printf("composed in %.3f s; rendered %.1f s of audio in %.2f s: %.0f x real time, %.2f %% of a core\n", composeS,
                audioS, renderS, audioS / renderS, 100.0 * renderS / audioS);
    if (!bench) {
        const LoudnessReport r = meter.report();
        std::printf("loudness %.1f LUFS integrated, %.1f LUFS short-term max, true peak %.2f dBTP, LRA %.1f LU, crest %.1f dB,"
                    " correlation %.2f\n", r.integrated, r.shortTermMax, r.truePeak, r.range, r.crest, r.correlation);
    }
    if (!out.empty()) std::printf("WAV: %s\n", out.c_str());
    return 0;
}
