/**
 * @file Engine.cpp
 * @brief The engine: events on the sample grid, the parameter raster, the low end as one system, buses, master.
 * @note The raster, the event splitting and the automation cursors follow Ephemeris `Core/src/Engine.cpp` at d047d79
 *       (27.09.2026); the signal flow is Umbra's.
 */
#include "umb/Engine.h"
#include <algorithm>
#include <cmath>

namespace umb {

namespace {
constexpr float kClipCeiling = 0.97f;   ///< where the soft clipper's curve flattens (-0.26 dBFS)
}

const char* Engine::stemName(int s)
{
    static const char* const kNames[kStems] = { "kick", "rumble", "sub", "hats", "perc" };
    return s >= 0 && s < kStems ? kNames[s] : "";
}

void Engine::prepare(double sampleRate, int maxBlock)
{
    sampleRate_ = sampleRate;
    maxBlock_ = std::max(1, maxBlock);
    kick_.prepare(sampleRate);
    rumble_.prepare(sampleRate, kRaster);
    sub_.prepare(sampleRate);
    kit_.prepare(sampleRate);
    glue_.prepare(sampleRate);
    limiter_.prepare(sampleRate);
    for (Oversampler4& o : clipOs_) o.reset();
    const size_t n = static_cast<size_t>(kRaster);
    kickBuf_.assign(n, 0.0f);
    bodyBuf_.assign(n, 0.0f);
    rumbleBuf_.assign(n, 0.0f);
    subBuf_.assign(n, 0.0f);
    cellDirty_ = true;
}

void Engine::load(const Score& score)
{
    score_ = score;
    score_.sort();
    // Events on the sample grid: the first sample at or after the ideal time, and how late that is.
    events_.clear();
    int id = 0;
    for (const NoteEvent& note : score_.notes) {
        const double x = score_.tempo.secondsAt(note.beat) * sampleRate_;
        const double s = std::ceil(x);
        Ev on{ static_cast<int64_t>(s), 1, static_cast<uint8_t>(note.part), note.pitch, note.velocity, note.shift, s - x, id };
        events_.push_back(on);
        if (note.part == Part::Sub) {
            const double xo = score_.tempo.secondsAt(note.beat + note.length) * sampleRate_;
            const double so = std::ceil(xo);
            events_.push_back(Ev{ static_cast<int64_t>(so), 0, on.part, note.pitch, 0.0f, 0, so - xo, id });
        }
        ++id;
    }
    std::stable_sort(events_.begin(), events_.end(), [](const Ev& a, const Ev& b) {
        return a.sample != b.sample ? a.sample < b.sample : a.on < b.on;
    });
    // The automation, per parameter.
    tracks_.clear();
    trackOf_.assign(static_cast<size_t>(params_.count()), -1);
    for (const Gesture& g : score_.gestures) {
        if (g.param < 0 || g.param >= params_.count()) continue;
        int& t = trackOf_[static_cast<size_t>(g.param)];
        if (t < 0) { t = static_cast<int>(tracks_.size()); tracks_.push_back(Track{ g.param, {}, 0, 0.0f }); }
        tracks_[static_cast<size_t>(t)].gestures.push_back(g);
    }
    endSample_ = static_cast<int64_t>(std::ceil(lengthSeconds() * sampleRate_));
    seek(0.0);
}

void Engine::seek(double beat)
{
    const double x = std::max(0.0, score_.tempo.secondsAt(beat)) * sampleRate_;
    sample_ = static_cast<int64_t>(std::llround(x));
    evCursor_ = 0;
    while (evCursor_ < events_.size() && events_[evCursor_].sample < sample_) ++evCursor_;
    // A jump back to the start plays what is due at it; elsewhere the voices fall silent.
    if (sample_ == 0) evCursor_ = 0;
    for (Track& t : tracks_) { t.cursor = t.gestures.size(); t.offset = 0.0f; }
    kick_.reset();
    rumble_.reset();
    sub_.reset();
    kit_.reset();
    glue_.reset();
    limiter_.reset();
    for (Oversampler4& o : clipOs_) o.reset();
    for (Svf& f : hatsLp_) f.reset();
    for (Svf& f : percLp_) f.reset();
    sideHp1_.reset();
    sideHp2_.reset();
    haveKick_ = false;
    subNote_ = -1;
    cellDirty_ = true;
}

float Engine::played(int id) const
{
    const float knob = params_.get(id);
    if (id < 0 || id >= static_cast<int>(trackOf_.size()) || trackOf_[static_cast<size_t>(id)] < 0) return knob;
    const float off = tracks_[static_cast<size_t>(trackOf_[static_cast<size_t>(id)])].offset;
    if (off == 0.0f) return knob;
    return params_.fromNormalised(id, params_.toNormalised(id, knob) + off);
}

void Engine::readPlayed(Module m, int instance, float* out) const
{
    const int b = params_.base(m, instance);
    const int n = ParamStore::moduleCount(m);
    for (int i = 0; i < n; ++i) out[i] = played(b + i);
}

void Engine::updateCell()
{
    cellDirty_ = false;
    const double beat = score_.tempo.beatAt(seconds());
    for (Track& t : tracks_) {
        // The latest curve that has started (curves are sorted by beat; cursor == size() means none yet).
        const size_t none = t.gestures.size();
        size_t c = t.cursor, next = c == none ? 0 : c + 1;
        while (next < t.gestures.size() && t.gestures[next].beat <= beat) { c = next; ++next; }
        t.cursor = c;
        t.offset = c == none ? 0.0f : gestureValue(t.gestures[c], beat);
    }

    float c[64];
    readPlayed(Module::Compose, 0, c);
    keyRoot_ = static_cast<int>(std::lround(c[compose::Key]));
    scale_ = static_cast<int>(std::lround(c[compose::Scale]));
    subOwns_ = std::lround(c[compose::LowOwner]) == static_cast<long>(LowOwner::Sub);
    const double bpm = score_.tempo.bpmAt(beat);

    float v[64];
    readPlayed(Module::Kick, 0, v);
    Kick::constrain(v, 0.0, keyRoot_);
    kick_.update(v, keyRoot_);

    readPlayed(Module::Rumble, 0, v);
    if (subOwns_) v[rumble::Sub] = -60.0f;   // the sub bass owns the band under the split
    rumble_.update(v, kick_.tunedEndHz());

    readPlayed(Module::Sub, 0, v);
    sub_.update(v);

    kit_.setTempo(bpm);
    for (int l = 0; l < kPercLanes; ++l) {
        readPlayed(Module::Perc, l, v);
        kit_.update(l, v, keyRoot_, scale_);
        const PercRole role = static_cast<PercRole>(std::lround(v[perc::Role]));
        laneIsHat_[l] = role == PercRole::ClosedHat || role == PercRole::RollingHat || role == PercRole::OpenHat
                     || role == PercRole::Ride || role == PercRole::Shaker;
    }

    const float fs = static_cast<float>(sampleRate_);
    readPlayed(Module::Mix, 0, v);
    hatsGain_ = dbToGain(v[mix::HatsLevel]);
    percGain_ = dbToGain(v[mix::PercLevel]);
    for (Svf& f : hatsLp_) f.setQ(std::min(v[mix::HatsCut], 0.45f * fs), 0.7071f, fs);
    for (Svf& f : percLp_) f.setQ(std::min(v[mix::PercCut], 0.45f * fs), 0.7071f, fs);

    readPlayed(Module::Master, 0, v);
    masterGain_ = dbToGain(v[master::Level]);
    glue_.set(v[master::Threshold], v[master::Ratio], 6.0f, 20.0f, 200.0f);
    clipDrive_ = dbToGain(v[master::Clip]);
    limiter_.set(v[master::Ceiling], 60.0f);
    sideHp1_.setK(v[master::MonoBelow], 1.8477590f, fs);   // fourth-order Butterworth
    sideHp2_.setK(v[master::MonoBelow], 0.7653669f, fs);
}

void Engine::dispatch(const Ev& e)
{
    const Part part = static_cast<Part>(e.part);
    if (part == Part::Kick) {
        kick_.trigger(e.velocity, e.late);
        const double c = kick_.asymptoticPhase(), f0 = kick_.tunedEndHz();
        rumble_.kick(e.late, c, f0);
        sub_.kick(e.late);
        haveKick_ = true;
        kickTime_ = static_cast<double>(e.sample) - e.late;
        kickC_ = c;
        kickF0_ = f0;
        return;
    }
    if (part == Part::Sub) {
        if (e.on == 0) {
            if (e.id == subNote_) { sub_.noteOff(); subNote_ = -1; }
            return;
        }
        double phase = -1.0;
        if (sub_.locked() && haveKick_) {
            // The kick's phase at the note's ideal start, f0 dt + c, carried to the note's pitch.
            const double dt = (static_cast<double>(e.sample) - e.late - kickTime_) / sampleRate_;
            const double hz = sub_.noteHz(e.pitch);
            const double kickPhase = kickF0_ * dt + kickC_;
            const double p = hz / kickF0_ * kickPhase - sub_.chainPhase(hz);
            phase = p - std::floor(p);
        }
        sub_.noteOn(e.pitch, e.velocity, e.late, phase);
        subNote_ = e.id;
        return;
    }
    const int lane = laneOf(part);
    if (lane >= 0) kit_.trigger(lane, e.velocity, e.shift, e.late);
}

void Engine::renderSpan(float* L, float* R, int n)
{
    kick_.process(kickBuf_.data(), bodyBuf_.data(), n);
    rumble_.process(bodyBuf_.data(), rumbleBuf_.data(), n);
    sub_.process(subBuf_.data(), n);
    kit_.processLanes(n);
    const float* ll = kit_.laneL();
    const float* lr = kit_.laneR();
    for (int i = 0; i < n; ++i) {
        float hl = 0.0f, hr = 0.0f, pl = 0.0f, pr = 0.0f;
        for (int l = 0; l < kPercLanes; ++l) {
            const size_t k = static_cast<size_t>(i * PercKit::kStride + l);
            if (laneIsHat_[l]) { hl += ll[k]; hr += lr[k]; }
            else { pl += ll[k]; pr += lr[k]; }
        }
        hl = hatsLp_[0].lp(hl) * hatsGain_;
        hr = hatsLp_[1].lp(hr) * hatsGain_;
        pl = percLp_[0].lp(pl) * percGain_;
        pr = percLp_[1].lp(pr) * percGain_;
        const size_t k = static_cast<size_t>(i);
        const float mono = kickBuf_[k] + rumbleBuf_[k] + subBuf_[k];
        if (stemL_ != nullptr) {
            const int o = stemOffset_ + i;
            stemL_[0][o] = stemR_[0][o] = kickBuf_[k];
            stemL_[1][o] = stemR_[1][o] = rumbleBuf_[k];
            stemL_[2][o] = stemR_[2][o] = subBuf_[k];
            stemL_[3][o] = hl; stemR_[3][o] = hr;
            stemL_[4][o] = pl; stemR_[4][o] = pr;
        }
        float l = mono + hl + pl, r = mono + hr + pr;
        // The side under Mono Below goes (PLAN 8.2).
        const float mid = 0.5f * (l + r);
        float side = 0.5f * (l - r), lp, bp, hp;
        sideHp1_.tick(side, lp, bp, hp);
        sideHp2_.tick(hp, lp, bp, hp);
        side = hp;
        L[i] = (mid + side) * masterGain_;
        R[i] = (mid - side) * masterGain_;
    }
    glue_.process(L, R, n);
    const float g = clipDrive_, t = kClipCeiling;
    auto curve = [g, t](float x) { return t * std::tanh(g * x / t); };
    for (int i = 0; i < n; ++i) {
        L[i] = clipOs_[0].process(L[i], curve);
        R[i] = clipOs_[1].process(R[i], curve);
    }
    limiter_.process(L, R, n);
}

bool Engine::process(float* L, float* R, int n)
{
    DenormalGuard guard;
    int done = 0;
    while (done < n) {
        if (cellDirty_ || sample_ % kRaster == 0) updateCell();
        while (evCursor_ < events_.size() && events_[evCursor_].sample <= sample_) dispatch(events_[evCursor_++]);
        int64_t end = std::min<int64_t>(sample_ + (n - done), (sample_ / kRaster + 1) * kRaster);
        if (evCursor_ < events_.size() && events_[evCursor_].sample < end) end = events_[evCursor_].sample;
        const int len = static_cast<int>(end - sample_);
        stemOffset_ = done;
        renderSpan(L + done, R + done, len);
        done += len;
        sample_ += len;
    }
    return sample_ < endSample_;
}

} // namespace umb
