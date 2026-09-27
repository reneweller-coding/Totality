/**
 * @file Engine.cpp
 * @brief The engine: events on the sample grid, the parameter raster, the low end as one system, buses, sends, master.
 * @note The raster, the event splitting and the automation cursors follow Ephemeris `Core/src/Engine.cpp` at d047d79
 *       (27.09.2026); the signal flow is Umbra's.
 */
#include "umb/Engine.h"
#include <algorithm>
#include <cmath>

namespace umb {

namespace {
constexpr double kPiD = 3.141592653589793;
constexpr float kClipCeiling = 0.97f;   ///< where the soft clipper's curve flattens (-0.26 dBFS)
constexpr float kBassMinCut = 100.0f;   ///< the bass synth's and the 303's lowest low cut: under it only kick, rumble, sub
constexpr float kGlueMix = 0.35f;       ///< the master glue in parallel (PLAN 8.4)
/** The room's makeup: the FDN returns quietly (Ephemeris: +4 dB at sends of 0.3); +12 dB made it audible in Phase 2, the
 *  fit against the references' width another +10 dB (Params.cpp, kMixParams), so space.level's 0 dB is the fitted room. */
constexpr float kRoomMakeupDb = 22.0f;
}

const char* Engine::stemName(int s)
{
    static const char* const kNames[kStems] = { "kick", "rumble", "sub", "hats", "perc", "ping", "room", "bass", "acid",
                                                "chord", "drone", "texture", "dub", "cloud" };
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
    bass_.prepare(sampleRate);
    acid_.prepare(sampleRate);
    chord_.prepare(sampleRate);
    drone_.prepare(sampleRate);
    texture_.prepare(sampleRate);
    room_.prepare(sampleRate);
    dub_.prepare(sampleRate, kRaster);
    cloud_.prepare(sampleRate, 0x434C4F5544ull);   // "CLOUD"
    padsDuck_.prepare(sampleRate);
    fxDuck_.prepare(sampleRate);
    glue_.prepare(sampleRate);
    limiter_.prepare(sampleRate);
    for (Oversampler4& o : clipOs_) o.reset();
    for (BandLimit& b : cutLp_) b.prepare(sampleRate, 16000.0f);
    const float fs = static_cast<float>(sampleRate);
    for (int c = 0; c < 2; ++c) { cutHp1_[c].setK(6000.0f, 1.41421356f, fs); cutHp2_[c].setK(6000.0f, 1.41421356f, fs); }
    cutAtt_ = 1.0f - std::exp(-1.0f / (0.001f * fs));
    cutRel_ = 1.0f - std::exp(-1.0f / (0.05f * fs));
    tiltCoef_ = 1.0f - std::exp(-2.0f * kPi * 1000.0f / fs);
    trimCoef_ = 1.0f - std::exp(-1.0f / (1.0f * fs));   // a correction glides in over about a second
    const size_t n = static_cast<size_t>(kRaster);
    for (std::vector<float>* b : { &kickBuf_, &bodyBuf_, &rumbleBuf_, &subBuf_, &pingL_, &pingR_, &bassL_, &bassR_, &acidL_,
                                   &acidR_, &chordL_, &chordR_, &droneL_, &droneR_, &texL_, &texR_, &roomInL_, &roomInR_,
                                   &roomL_, &roomR_, &echoInL_, &echoInR_, &plateInL_, &plateInR_, &dubL_, &dubR_, &drumL_,
                                   &drumR_, &cloudInL_, &cloudInR_, &cloudL_, &cloudR_ })
        b->assign(n, 0.0f);
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
        Ev on{ static_cast<int64_t>(s), 1, static_cast<uint8_t>(note.part), note.accent, note.slide, note.pitch, note.velocity,
               note.shift, s - x, id };
        events_.push_back(on);
        if (!isOneShot(note.part)) {
            const double xo = score_.tempo.secondsAt(note.beat + note.length) * sampleRate_;
            const double so = std::ceil(xo);
            events_.push_back(Ev{ static_cast<int64_t>(so), 0, on.part, false, false, note.pitch, 0.0f, 0, so - xo, id });
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
    lateTrims_.clear();
    endSample_ = static_cast<int64_t>(std::ceil(lengthSeconds() * sampleRate_));
    seek(0.0);
}

void Engine::setLevelTrims(const std::vector<float>& trims)
{
    lateTrims_ = trims;
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
    bass_.reset();
    acid_.reset();
    chord_.reset();
    drone_.reset();
    texture_.reset();
    room_.reset();
    dub_.reset();
    cloud_.reset();
    padsDuck_.reset();
    fxDuck_.reset();
    glue_.reset();
    limiter_.reset();
    for (Oversampler4& o : clipOs_) o.reset();
    for (Svf& f : hatsLp_) f.reset();
    for (Svf& f : percLp_) f.reset();
    for (BandLimit& b : cutLp_) b.reset();
    for (int c = 0; c < 2; ++c) { cutHp1_[c].reset(); cutHp2_[c].reset(); tiltState_[c] = 0.0f; }
    cutEnv_ = 0.0f;
    sideHp1_.reset();
    sideHp2_.reset();
    haveKick_ = false;
    subNote_ = bassNote_ = acidNote_ = droneNote_ = -1;
    // The loudness correction at the new place, at once.
    const double b = score_.tempo.beatAt(static_cast<double>(sample_) / sampleRate_);
    trimGain_ = trimTarget_ = dbToGain(score_.trimAt(b));
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
    beatsPerSample_ = bpm / 60.0 / sampleRate_;

    // The global motion (Dok. 8.4): three LFOs of 7, 11 and 13 beats and a drift of 0.065 Hz, from the absolute beat
    // and second, so the phases do not depend on where a render starts. They step every 128 samples (an absolute
    // raster, 2.7 ms), so the kit recomputes its lanes a quarter as often.
    float mv[16];
    readPlayed(Module::Motion, 0, mv);
    const int64_t q = (sample_ / 128) * 128;
    const double qs = static_cast<double>(q) / sampleRate_, qb = score_.tempo.beatAt(qs);
    const float amt = mv[motion::Amount];
    const float lfo7 = amt * static_cast<float>(std::sin(2.0 * kPiD * qb / 7.0));
    const float lfo11 = amt * static_cast<float>(std::sin(2.0 * kPiD * qb / 11.0 + 1.3));
    const float lfo13 = amt * static_cast<float>(std::sin(2.0 * kPiD * qb / 13.0 + 2.1));
    const float drift = amt * static_cast<float>(std::sin(2.0 * kPiD * 0.065 * qs + 0.7));
    const float drift2 = amt * static_cast<float>(std::sin(2.0 * kPiD * 0.05 * qs + 2.9));

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
        laneIsHat_[l] = r == PercRole::ClosedHat || r == PercRole::RollingHat || r == PercRole::OpenHat
                     || r == PercRole::Ride || r == PercRole::Shaker;
    }

    readPlayed(Module::Ping, 0, v);
    ping_.update(v);

    readPlayed(Module::Bass, 0, v);
    bass_.update(v, kBassMinCut);
    bassSends_ = Sends{ v[synth::RoomSend], v[synth::DubSend], 0.0f };
    readPlayed(Module::Acid, 0, v);
    acid_.update(v, kBassMinCut);
    acidSends_ = Sends{ v[synth::RoomSend], v[synth::DubSend], 0.0f };
    readPlayed(Module::Chord, 0, v);
    chord_.update(v);
    chordLevel_ = v[chord::Level] <= -59.9f ? 0.0f : dbToGain(v[chord::Level]);
    chordSends_ = Sends{ 0.0f, v[chord::DubSend], v[chord::PlateSend] };
    readPlayed(Module::Drone, 0, v);
    drone_.update(v);
    drone_.at(beat);
    droneSends_ = Sends{ v[drone::RoomSend], 0.0f, v[drone::PlateSend] };
    readPlayed(Module::Texture, 0, v);
    texture_.update(v);
    readPlayed(Module::Dub, 0, v);
    dub_.update(v, bpm);
    pingEcho_ = v[dub::PingSend];
    hatsEcho_ = v[dub::HatsSend];
    percEcho_ = v[dub::PercSend];
    readPlayed(Module::Cloud, 0, v);
    cloud_.update(v);
    cloudPing_ = v[cloud::PingSend];
    cloudChord_ = v[cloud::ChordSend];
    cloudPlate_ = v[cloud::PlateSend];

    const float fs = static_cast<float>(sampleRate_);
    readPlayed(Module::Mix, 0, v);
    hatsGain_ = dbToGain(v[mix::HatsLevel] + mv[motion::HatsLevel] * lfo7);
    percGain_ = dbToGain(v[mix::PercLevel]);
    for (Svf& f : hatsLp_) f.setQ(std::min(v[mix::HatsCut], 0.45f * fs), 0.7071f, fs);
    for (Svf& f : percLp_) f.setQ(std::min(v[mix::PercCut], 0.45f * fs), 0.7071f, fs);
    drumSat_ = v[mix::DrumSat];
    padsDuck_.set(v[mix::DuckLow], v[mix::DuckMid], 60.0f, 250.0f);
    fxDuck_.set(v[mix::DuckLow], v[mix::DuckMid], 60.0f, 250.0f);

    readPlayed(Module::Space, 0, v);
    room_.set(v[space::Size], v[space::Decay], v[space::Damping], v[space::PreDelay] * 0.001f * fs, v[space::LowCut],
              v[space::HighCut]);
    roomReturn_ = v[space::Level] <= -59.9f ? 0.0f : dbToGain(v[space::Level] + kRoomMakeupDb);
    hatsSend_ = v[space::HatsSend];
    percSend_ = v[space::PercSend];
    pingSend_ = v[space::PingSend];

    readPlayed(Module::Master, 0, v);
    masterGain_ = dbToGain(v[master::Level]);
    glue_.set(v[master::Threshold], v[master::Ratio], 6.0f, 20.0f, 200.0f);
    clipDrive_ = dbToGain(v[master::Clip]);
    limiter_.set(v[master::Ceiling], 60.0f);
    cut_ = v[master::Cut] >= 0.5f;
    const float mono = cut_ ? std::max(v[master::MonoBelow], 150.0f) : v[master::MonoBelow];
    sideHp1_.setK(mono, 1.8477590f, fs);   // fourth-order Butterworth
    sideHp2_.setK(mono, 0.7653669f, fs);
    // The tilt: the highs against the lows by Tilt dB, the lows kept (a pivot at 1 kHz took the low end down by half the
    // tilt, and with it the level the Leveler had to win back).
    tiltHigh_ = dbToGain(v[master::Tilt]);
    tiltLow_ = 1.0f;

    // The Leveler's correction of the track playing: the score's, or the one found while it plays.
    float trim = score_.trimAt(beat);
    if (!lateTrims_.empty()) {
        size_t k = 0;
        for (size_t i = 0; i < score_.levels.size(); ++i) if (score_.levels[i].beat <= beat) k = i;
        if (k < lateTrims_.size()) trim = lateTrims_[k];
    }
    trimTarget_ = dbToGain(trim);
}

void Engine::dispatch(const Ev& e)
{
    const Part part = static_cast<Part>(e.part);
    switch (part) {
    case Part::Kick: {
        kick_.trigger(e.velocity, e.late);
        const double c = kick_.asymptoticPhase(), f0 = kick_.tunedEndHz();
        rumble_.kick(e.late, c, f0);
        sub_.kick(e.late);
        bass_.kick(e.late);
        acid_.kick(e.late);
        padsDuck_.trigger(e.late);
        fxDuck_.trigger(e.late);
        haveKick_ = true;
        kickTime_ = static_cast<double>(e.sample) - e.late;
        kickC_ = c;
        kickF0_ = f0;
        return;
    }
    case Part::Sub:
    case Part::Bass: {
        if (e.on == 0) {
            if (part == Part::Bass && e.id == bassNote_) { bass_.noteOff(); bassNote_ = -1; }
            if (e.id == subNote_) { sub_.noteOff(); subNote_ = -1; }
            return;
        }
        if (part == Part::Bass) {
            bass_.noteOn(e.pitch, e.velocity, e.late, e.accent, e.slide);
            bassNote_ = e.id;
            if (!subOwns_) return;   // the sine plays the line only where the sub owns the low end
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
    case Part::Acid:
        if (e.on == 0) { if (e.id == acidNote_) { acid_.noteOff(); acidNote_ = -1; } return; }
        acid_.noteOn(e.pitch, e.velocity, e.late, e.accent, e.slide);
        acidNote_ = e.id;
        return;
    case Part::Chord:
        if (e.on == 0) chord_.noteOff(e.pitch);
        else chord_.noteOn(e.pitch, e.velocity, e.late);
        return;
    case Part::Drone:
        if (e.on == 0) { if (e.id == droneNote_) { drone_.noteOff(); droneNote_ = -1; } return; }
        drone_.noteOn(e.pitch, e.velocity);
        droneNote_ = e.id;
        return;
    case Part::Texture:
        texture_.gate(e.on != 0);
        return;
    case Part::Ping:
        ping_.noteOn(e.pitch, e.velocity, e.late, static_cast<uint32_t>(e.id));
        return;
    default: {
        const int lane = laneOf(part);
        if (lane >= 0) kit_.trigger(lane, e.velocity, e.shift, e.late);
        return;
    }
    }
}

void Engine::renderSpan(float* L, float* R, int n)
{
    // The sources.
    kick_.process(kickBuf_.data(), bodyBuf_.data(), n);
    rumble_.process(bodyBuf_.data(), rumbleBuf_.data(), n);
    sub_.process(subBuf_.data(), n);
    kit_.processLanes(n);
    ping_.process(pingL_.data(), pingR_.data(), n, sample_);
    bass_.process(bassL_.data(), bassR_.data(), n);
    acid_.process(acidL_.data(), acidR_.data(), n);
    chord_.process(chordL_.data(), chordR_.data(), n, sample_);
    drone_.process(droneL_.data(), droneR_.data(), n);
    texture_.process(texL_.data(), texR_.data(), n);

    // The percussion buses, the drum bus, the pads, the sends.
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
        chordL_[k] *= chordLevel_;
        chordR_[k] *= chordLevel_;
        roomInL_[k] = busH[0][i] * hatsSend_ + busP[0][i] * percSend_ + pingL_[k] * pingSend_ + bassL_[k] * bassSends_.room
                    + acidL_[k] * acidSends_.room + droneL_[k] * droneSends_.room;
        roomInR_[k] = busH[1][i] * hatsSend_ + busP[1][i] * percSend_ + pingR_[k] * pingSend_ + bassR_[k] * bassSends_.room
                    + acidR_[k] * acidSends_.room + droneR_[k] * droneSends_.room;
        echoInL_[k] = bassL_[k] * bassSends_.dub + acidL_[k] * acidSends_.dub + chordL_[k] * chordSends_.dub
                    + pingL_[k] * pingEcho_ + busH[0][i] * hatsEcho_ + busP[0][i] * percEcho_;
        echoInR_[k] = bassR_[k] * bassSends_.dub + acidR_[k] * acidSends_.dub + chordR_[k] * chordSends_.dub
                    + pingR_[k] * pingEcho_ + busH[1][i] * hatsEcho_ + busP[1][i] * percEcho_;
        plateInL_[k] = chordL_[k] * chordSends_.plate + droneL_[k] * droneSends_.plate;
        plateInR_[k] = chordR_[k] * chordSends_.plate + droneR_[k] * droneSends_.plate;
        // The drum bus: three stages of saturation mixed in (Dok. 8.7: "3 or 4 drum buss units at 10 or 20 % wet").
        const float mono = kickBuf_[k] + rumbleBuf_[k];
        float dl = mono + busH[0][i] + busP[0][i], dr = mono + busH[1][i] + busP[1][i];
        if (drumSat_ > 0.0f) {
            const float m = 0.43f * drumSat_;   // 15 % a stage at the default 0.35
            static const float kDrive[3] = { 1.5f, 2.0f, 2.5f };
            for (float d : kDrive) {
                dl += m * (std::tanh(d * dl) / d - dl);
                dr += m * (std::tanh(d * dr) / d - dr);
            }
        }
        drumL_[k] = dl;
        drumR_[k] = dr;
    }
    // The cloud hears the ping and the chord; half of it goes into the plate.
    for (int i = 0; i < n; ++i) {
        const size_t k = static_cast<size_t>(i);
        cloudInL_[k] = pingL_[k] * cloudPing_ + chordL_[k] * cloudChord_;
        cloudInR_[k] = pingR_[k] * cloudPing_ + chordR_[k] * cloudChord_;
    }
    cloud_.process(cloudInL_.data(), cloudInR_.data(), cloudL_.data(), cloudR_.data(), n);
    for (int i = 0; i < n; ++i) {
        const size_t k = static_cast<size_t>(i);
        plateInL_[k] += cloudL_[k] * cloudPlate_;
        plateInR_[k] += cloudR_[k] * cloudPlate_;
    }
    room_.process(roomInL_.data(), roomInR_.data(), roomL_.data(), roomR_.data(), n);
    dub_.process(echoInL_.data(), echoInR_.data(), plateInL_.data(), plateInR_.data(), dubL_.data(), dubR_.data(), n);
    // The pads (chord and drone) and the returns step aside for the kick, band by band.
    for (int i = 0; i < n; ++i) {
        const size_t k = static_cast<size_t>(i);
        chordL_[k] += droneL_[k];
        chordR_[k] += droneR_[k];
        roomL_[k] = roomL_[k] * roomReturn_ + dubL_[k] + cloudL_[k];
        roomR_[k] = roomR_[k] * roomReturn_ + dubR_[k] + cloudR_[k];
    }
    padsDuck_.process(chordL_.data(), chordR_.data(), n);
    fxDuck_.process(roomL_.data(), roomR_.data(), n);

    for (int i = 0; i < n; ++i) {
        const size_t k = static_cast<size_t>(i);
        if (stemL_ != nullptr) {
            const int o = stemOffset_ + i;
            auto put = [&](int s, float l, float r) { stemL_[s][o] = l; stemR_[s][o] = r; };
            put(kStemKick, kickBuf_[k], kickBuf_[k]);
            put(kStemRumble, rumbleBuf_[k], rumbleBuf_[k]);
            put(kStemSub, subBuf_[k], subBuf_[k]);
            put(kStemHats, busH[0][i], busH[1][i]);
            put(kStemPerc, busP[0][i], busP[1][i]);
            put(kStemPing, pingL_[k], pingR_[k]);
            put(kStemRoom, roomL_[k] - dubL_[k] - cloudL_[k], roomR_[k] - dubR_[k] - cloudR_[k]);
            put(kStemBass, bassL_[k], bassR_[k]);
            put(kStemAcid, acidL_[k], acidR_[k]);
            put(kStemChord, chordL_[k] - droneL_[k], chordR_[k] - droneR_[k]);
            put(kStemDrone, droneL_[k], droneR_[k]);
            put(kStemTexture, texL_[k], texR_[k]);
            put(kStemDub, dubL_[k], dubR_[k]);
            put(kStemCloud, cloudL_[k], cloudR_[k]);
        }
        float l = drumL_[k] + subBuf_[k] + pingL_[k] + bassL_[k] + acidL_[k] + chordL_[k] + texL_[k] + roomL_[k];
        float r = drumR_[k] + subBuf_[k] + pingR_[k] + bassR_[k] + acidR_[k] + chordR_[k] + texR_[k] + roomR_[k];
        // The side under Mono Below goes (PLAN 8.2).
        const float mid = 0.5f * (l + r);
        float side = 0.5f * (l - r), lp, bp, hp;
        sideHp1_.tick(side, lp, bp, hp);
        sideHp2_.tick(hp, lp, bp, hp);
        side = hp;
        l = (mid + side) * masterGain_;
        r = (mid - side) * masterGain_;
        // The tilt: a complementary one-pole split at 1 kHz, the highs apart.
        tiltState_[0] += tiltCoef_ * (l - tiltState_[0]);
        tiltState_[1] += tiltCoef_ * (r - tiltState_[1]);
        L[i] = tiltLow_ * tiltState_[0] + tiltHigh_ * (l - tiltState_[0]);
        R[i] = tiltLow_ * tiltState_[1] + tiltHigh_ * (r - tiltState_[1]);
        drumL_[k] = L[i];   // the glue's dry signal (the drum bus is spent)
        drumR_[k] = R[i];
    }
    glue_.process(L, R, n);
    // The glue in parallel, then the Leveler's trim: after the glue, which would otherwise halve every correction (a
    // ratio of 2 far over its threshold); the clipper and the limiter take it.
    for (int i = 0; i < n; ++i) {
        const size_t k = static_cast<size_t>(i);
        trimGain_ += (trimTarget_ - trimGain_) * trimCoef_;
        L[i] = (drumL_[k] + kGlueMix * (L[i] - drumL_[k])) * trimGain_;
        R[i] = (drumR_[k] + kGlueMix * (R[i] - drumR_[k])) * trimGain_;
    }
    if (cut_) {
        // The vinyl cut (Erg. 3): the band over 6 kHz limited dynamically (a cutter head's protection), then 16 kHz.
        for (int i = 0; i < n; ++i) {
            float h[2];
            float* ch[2] = { &L[i], &R[i] };
            float peak = 0.0f;
            for (int c = 0; c < 2; ++c) {
                float lp, bp, hp;
                cutHp1_[c].tick(*ch[c], lp, bp, hp);
                cutHp2_[c].tick(hp, lp, bp, h[c]);
                peak = std::max(peak, std::fabs(h[c]));
            }
            cutEnv_ += (peak - cutEnv_) * (peak > cutEnv_ ? cutAtt_ : cutRel_);
            const float thr = 0.1f;   // -20 dBFS in the band
            const float gr = cutEnv_ > thr ? std::pow(thr / cutEnv_, 0.75f) : 1.0f;   // ratio 4
            for (int c = 0; c < 2; ++c) *ch[c] = cutLp_[c].process(*ch[c] - (1.0f - gr) * h[c]);
        }
    }
    const float gd = clipDrive_, t = kClipCeiling;
    auto curve = [gd, t](float x) { return t * std::tanh(gd * x / t); };
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
