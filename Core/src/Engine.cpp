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
    static const char* const kNames[kStems] = { "kick", "rumble", "sub", "hats", "perc", "ping", "room" };
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
    ping_.prepare(sampleRate);
    room_.prepare(sampleRate);
    glue_.prepare(sampleRate);
    limiter_.prepare(sampleRate);
    for (Oversampler4& o : clipOs_) o.reset();
    const size_t n = static_cast<size_t>(kRaster);
    kickBuf_.assign(n, 0.0f);
    bodyBuf_.assign(n, 0.0f);
    rumbleBuf_.assign(n, 0.0f);
    subBuf_.assign(n, 0.0f);
    pingL_.assign(n, 0.0f);
    pingR_.assign(n, 0.0f);
    sendL_.assign(n, 0.0f);
    sendR_.assign(n, 0.0f);
    retL_.assign(n, 0.0f);
    retR_.assign(n, 0.0f);
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
    ping_.reset();
    room_.reset();
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

    // The global motion (Dok. 8.4): three LFOs of 7, 11 and 13 beats and a drift of 0.065 Hz, from the absolute beat
    // and second, so the phases do not depend on where a render starts.
    // They step every 128 samples (an absolute raster, 2.7 ms), so the kit recomputes its lanes a quarter as often.
    float mv[16];
    readPlayed(Module::Motion, 0, mv);
    const int64_t q = (sample_ / 128) * 128;
    const double qs = static_cast<double>(q) / sampleRate_, qb = score_.tempo.beatAt(qs);
    const float amt = mv[motion::Amount];
    const float lfo7 = amt * static_cast<float>(std::sin(2.0 * 3.141592653589793 * qb / 7.0));
    const float lfo11 = amt * static_cast<float>(std::sin(2.0 * 3.141592653589793 * qb / 11.0 + 1.3));
    const float lfo13 = amt * static_cast<float>(std::sin(2.0 * 3.141592653589793 * qb / 13.0 + 2.1));
    const float drift = amt * static_cast<float>(std::sin(2.0 * 3.141592653589793 * 0.065 * qs + 0.7));
    const float drift2 = amt * static_cast<float>(std::sin(2.0 * 3.141592653589793 * 0.05 * qs + 2.9));

    float v[64];
    readPlayed(Module::Kick, 0, v);
    Kick::constrain(v, 0.0, keyRoot_);
    kick_.update(v, keyRoot_);

    readPlayed(Module::Rumble, 0, v);
    if (subOwns_) v[rumble::Sub] = -60.0f;   // the sub bass owns the band under the split
    v[rumble::Drive] = std::max(0.0f, v[rumble::Drive] + mv[motion::RumbleDrive] * drift2);
    rumble_.update(v, kick_.tunedEndHz());

    readPlayed(Module::Sub, 0, v);
    sub_.update(v);

    kit_.setTempo(bpm);
    for (int l = 0; l < kPercLanes; ++l) {
        readPlayed(Module::Perc, l, v);
        const PercRole r = static_cast<PercRole>(std::lround(v[perc::Role]));
        if (r == PercRole::ClosedHat || r == PercRole::RollingHat || r == PercRole::OpenHat) {
            const float k = 1.0f + mv[motion::HatDecay] * lfo11;
            v[perc::NoiseDecay] *= k;
            v[perc::Decay] *= k;
            // The hats' own filters wander, not the bus's low pass (which is open, and a ramp target of the form).
            v[perc::Cutoff] *= std::pow(2.0f, mv[motion::HatsCut] * drift);
        } else {
            v[perc::Cutoff] *= std::pow(2.0f, mv[motion::PercCut] * lfo13);
        }
        kit_.update(l, v, keyRoot_, scale_);
        const PercRole role = static_cast<PercRole>(std::lround(v[perc::Role]));
        laneIsHat_[l] = role == PercRole::ClosedHat || role == PercRole::RollingHat || role == PercRole::OpenHat
                     || role == PercRole::Ride || role == PercRole::Shaker;
    }

    readPlayed(Module::Ping, 0, v);
    ping_.update(v);

    const float fs = static_cast<float>(sampleRate_);
    readPlayed(Module::Mix, 0, v);
    hatsGain_ = dbToGain(v[mix::HatsLevel] + mv[motion::HatsLevel] * lfo7);
    percGain_ = dbToGain(v[mix::PercLevel]);
    const float hatsCut = v[mix::HatsCut];
    const float percCut = v[mix::PercCut];
    for (Svf& f : hatsLp_) f.setQ(std::min(hatsCut, 0.45f * fs), 0.7071f, fs);
    for (Svf& f : percLp_) f.setQ(std::min(percCut, 0.45f * fs), 0.7071f, fs);

    readPlayed(Module::Space, 0, v);
    room_.set(v[space::Size], v[space::Decay], v[space::Damping], v[space::PreDelay] * 0.001f * fs, v[space::LowCut],
              v[space::HighCut]);
    roomReturn_ = v[space::Level] <= -59.9f ? 0.0f : dbToGain(v[space::Level]);
    hatsSend_ = v[space::HatsSend];
    percSend_ = v[space::PercSend];
    pingSend_ = v[space::PingSend];

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
    if (part == Part::Ping) {
        ping_.noteOn(e.pitch, e.velocity, e.late, static_cast<uint32_t>(e.id));
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
    ping_.process(pingL_.data(), pingR_.data(), n, sample_);
    const float* ll = kit_.laneL();
    const float* lr = kit_.laneR();
    float busH[2][kRaster], busP[2][kRaster];
    for (int i = 0; i < n; ++i) {
        float hl = 0.0f, hr = 0.0f, pl = 0.0f, pr = 0.0f;
        for (int l = 0; l < kPercLanes; ++l) {
            const size_t k = static_cast<size_t>(i * PercKit::kStride + l);
            if (laneIsHat_[l]) { hl += ll[k]; hr += lr[k]; }
            else { pl += ll[k]; pr += lr[k]; }
        }
        busH[0][i] = hatsLp_[0].lp(hl) * hatsGain_;
        busH[1][i] = hatsLp_[1].lp(hr) * hatsGain_;
        busP[0][i] = percLp_[0].lp(pl) * percGain_;
        busP[1][i] = percLp_[1].lp(pr) * percGain_;
        const size_t k = static_cast<size_t>(i);
        sendL_[k] = busH[0][i] * hatsSend_ + busP[0][i] * percSend_ + pingL_[k] * pingSend_;
        sendR_[k] = busH[1][i] * hatsSend_ + busP[1][i] * percSend_ + pingR_[k] * pingSend_;
    }
    room_.process(sendL_.data(), sendR_.data(), retL_.data(), retR_.data(), n);
    for (int i = 0; i < n; ++i) {
        const float hl = busH[0][i], hr = busH[1][i], pl = busP[0][i], pr = busP[1][i];
        const size_t k = static_cast<size_t>(i);
        const float rl = retL_[k] * roomReturn_, rr = retR_[k] * roomReturn_;
        const float mono = kickBuf_[k] + rumbleBuf_[k] + subBuf_[k];
        if (stemL_ != nullptr) {
            const int o = stemOffset_ + i;
            stemL_[0][o] = stemR_[0][o] = kickBuf_[k];
            stemL_[1][o] = stemR_[1][o] = rumbleBuf_[k];
            stemL_[2][o] = stemR_[2][o] = subBuf_[k];
            stemL_[3][o] = hl; stemR_[3][o] = hr;
            stemL_[4][o] = pl; stemR_[4][o] = pr;
            stemL_[5][o] = pingL_[k]; stemR_[5][o] = pingR_[k];
            stemL_[6][o] = rl; stemR_[6][o] = rr;
        }
        float l = mono + hl + pl + pingL_[k] + rl, r = mono + hr + pr + pingR_[k] + rr;
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
