/**
 * @file Deck.cpp
 * @brief A deck: events on the sample grid, the parameter raster, the low end as one system, buses, sends, track bus.
 * @note The raster, the event splitting and the automation cursors follow Ephemeris `Core/src/Engine.cpp` at d047d79
 *       (27.09.2026); the signal flow is Totality's. Until Phase 4 this was the engine itself.
 */
#include "tot/Deck.h"
#include "tot/Profile.h"
#include <algorithm>
#include <cmath>

namespace tot {

namespace {
constexpr double kPiD = 3.141592653589793;   ///< pi
constexpr float kBassMinCut = 100.0f;   ///< the bass synth's and the 303's lowest low cut: under it only kick, rumble, sub
constexpr float kGlueMix = 0.35f;       ///< the glue in parallel (PLAN 8.4)
/** The room's makeup: the FDN returns quietly (Ephemeris: +4 dB at sends of 0.3); +12 dB made it audible in Phase 2, the
 *  fit against the references' width another +10 dB (Params.cpp, kMixParams), so space.level's 0 dB is the fitted room. */
constexpr float kRoomMakeupDb = 22.0f;
constexpr double kLeadSeconds = 0.5;    ///< a deck starts playing this long before its first note
constexpr double kTailSeconds = 20.0;   ///< and plays on this long after its last (rooms, echo, plate)
}

void Deck::prepare(const ParamStore* params, double sampleRate, int index)
{
    params_ = params;
    sampleRate_ = sampleRate;
    index_ = index;
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
    cloud_.prepare(sampleRate, 0x434C4F5544ull + static_cast<uint64_t>(index));   // "CLOUD"
    padsDuck_.prepare(sampleRate);
    fxDuck_.prepare(sampleRate);
    glue_.prepare(sampleRate);
    const float fs = static_cast<float>(sampleRate);
    tiltCoef_ = 1.0f - std::exp(-2.0f * kPi * 1000.0f / fs);
    trimCoef_ = 1.0f - std::exp(-1.0f / (1.0f * fs));   // a correction glides in over about a second
    for (int p = 0; p < kBalParts; ++p) balGain_[p] = balTarget_[p] = 1.0f;
    const size_t n = static_cast<size_t>(kRaster);
    for (std::vector<float>* b : { &kickBuf_, &bodyBuf_, &rumbleBuf_, &subBuf_, &pingL_, &pingR_, &bassL_, &bassR_, &acidL_,
                                   &acidR_, &chordL_, &chordR_, &droneL_, &droneR_, &texL_, &texR_, &roomInL_, &roomInR_,
                                   &roomL_, &roomR_, &echoInL_, &echoInR_, &plateInL_, &plateInR_, &dubL_, &dubR_, &drumL_,
                                   &drumR_, &cloudInL_, &cloudInR_, &cloudL_, &cloudR_ })
        b->assign(n, 0.0f);
    clear();
}

void Deck::clear()
{
    loaded_ = false;
    score_ = Score{};
    events_.clear();
    tracks_.clear();
    trackOf_.assign(static_cast<size_t>(params_ != nullptr ? params_->count() : 0), -1);
    mixerSteps_.clear();
    plays_.clear();
    lateTrims_.clear();
    lateBal_.clear();
    base_.assign(static_cast<size_t>(params_ != nullptr ? params_->count() : 0), std::numeric_limits<float>::quiet_NaN());
    knobCursor_ = 0;
    knobGroup_ = -1.0;
    newGroup_ = false;
    evCursor_ = 0;
}

void Deck::load(const Score& score)
{
    clear();
    loaded_ = true;
    score_ = score;
    score_.sort();
    lateTrims_.reserve(score_.levels.size());
    lateBal_.reserve(score_.levels.size());
    // Events on the sample grid: the first sample at or after the ideal time, and how late that is.
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
    // The automation, per parameter; the steps on this deck's mixer channel, for the engine.
    const int mixFirst = params_->base(Module::Deck, index_), mixEnd = mixFirst + deck::Count;
    for (const Gesture& g : score_.gestures) {
        if (g.param < 0 || g.param >= params_->count()) continue;
        int& t = trackOf_[static_cast<size_t>(g.param)];
        if (t < 0) { t = static_cast<int>(tracks_.size()); tracks_.push_back(Track{ g.param, {}, 0, 0.0f }); }
        tracks_[static_cast<size_t>(t)].gestures.push_back(g);
        if (g.param >= mixFirst && g.param < mixEnd)
            mixerSteps_.push_back(static_cast<int64_t>(std::ceil(score_.tempo.secondsAt(g.beat) * sampleRate_)));
    }
    std::sort(mixerSteps_.begin(), mixerSteps_.end());
    mixerSteps_.erase(std::unique(mixerSteps_.begin(), mixerSteps_.end()), mixerSteps_.end());
    // Where it plays: around its notes, merged.
    const int64_t lead = static_cast<int64_t>(kLeadSeconds * sampleRate_), tail = static_cast<int64_t>(kTailSeconds * sampleRate_);
    for (const Ev& e : events_) {
        const int64_t from = std::max<int64_t>(0, e.sample - lead), to = e.sample + tail;
        if (!plays_.empty() && from <= plays_.back().second) plays_.back().second = std::max(plays_.back().second, to);
        else plays_.push_back({ from, to });
    }
    seek(0);
}

bool Deck::playsAt(int64_t sample) const
{
    for (const auto& r : plays_) {
        if (sample < r.first) return false;
        if (sample < r.second) return true;
    }
    return false;
}

int64_t Deck::nextRestChange(int64_t sample) const
{
    for (const auto& r : plays_) {
        if (r.first > sample) return r.first;
        if (r.second > sample) return r.second;
    }
    return kNever;
}

void Deck::seek(int64_t sample)
{
    evCursor_ = 0;
    while (evCursor_ < events_.size() && events_[evCursor_].sample < sample) ++evCursor_;
    // A jump back to the start plays what is due at it; elsewhere the voices fall silent.
    if (sample == 0) evCursor_ = 0;
    for (Track& t : tracks_) { t.cursor = t.gestures.size(); t.offset = 0.0f; }
    // The knob settings again from the start: the next cell takes those due where the deck now is.
    std::fill(base_.begin(), base_.end(), std::numeric_limits<float>::quiet_NaN());
    knobCursor_ = 0;
    knobGroup_ = -1.0;
    newGroup_ = false;
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
    resetStems();
    for (Svf& f : hatsLp_) f.reset();
    for (Svf& f : percLp_) f.reset();
    for (auto& ch : groupHp_) for (Svf& f : ch) f.reset();
    tiltState_[0] = tiltState_[1] = 0.0f;
    haveKick_ = false;
    subNote_ = bassNote_ = acidNote_ = droneNote_ = -1;
    // The loudness correction at the new place, at once.
    const double b = score_.tempo.beatAt(static_cast<double>(sample) / sampleRate_);
    trimGain_ = trimTarget_ = dbToGain(score_.trimAt(b));
    const BalanceDb bal = score_.balanceAt(b);
    for (int p = 0; p < kBalParts; ++p) balGain_[p] = balTarget_[p] = dbToGain(bal[static_cast<size_t>(p)]);
}

float Deck::played(int id) const
{
    const float knob = params_->get(id);
    float value = knob;
    if (id >= 0 && static_cast<size_t>(id) < base_.size()) {
        const float b = base_[static_cast<size_t>(id)];
        if (b == b) {
            // The track's own value; where a hand has turned the knob away from what the engine wrote on it, by as much.
            value = b;
            const float s = shown_ != nullptr ? shown_[id] : std::numeric_limits<float>::quiet_NaN();
            if (s == s && knob != s)
                value = params_->fromNormalised(id, params_->toNormalised(id, b) + params_->toNormalised(id, knob) - params_->toNormalised(id, s));
        }
    }
    if (id < 0 || id >= static_cast<int>(trackOf_.size()) || trackOf_[static_cast<size_t>(id)] < 0) return value;
    const float off = tracks_[static_cast<size_t>(trackOf_[static_cast<size_t>(id)])].offset;
    if (off == 0.0f) return value;
    return params_->fromNormalised(id, params_->toNormalised(id, value) + off);
}

void Deck::applyKnobs(double beat)
{
    // One beat early: a track's sounds are in place before its first note (its deck rests until then).
    while (knobCursor_ < score_.knobs.size() && score_.knobs[knobCursor_].beat <= beat + 1.0) {
        const KnobSet& k = score_.knobs[knobCursor_++];
        if (k.beat != knobGroup_) {
            std::fill(base_.begin(), base_.end(), std::numeric_limits<float>::quiet_NaN());
            knobGroup_ = k.beat;
            newGroup_ = true;
        }
        if (k.param >= 0 && static_cast<size_t>(k.param) < base_.size()) base_[static_cast<size_t>(k.param)] = k.value;
    }
}

void Deck::readPlayed(Module m, int instance, float* out) const
{
    const int b = params_->base(m, instance);
    const int n = ParamStore::moduleCount(m);
    for (int i = 0; i < n; ++i) out[i] = played(b + i);
}

void Deck::updateCell(int64_t sample)
{
    const double seconds = static_cast<double>(sample) / sampleRate_;
    const double beat = score_.tempo.beatAt(seconds);
    applyKnobs(beat);
    for (Track& t : tracks_) {
        // The latest curve that has started (curves are sorted by beat; cursor == size() means none yet).
        const size_t none = t.gestures.size();
        size_t c = t.cursor, next = c == none ? 0 : c + 1;
        while (next < t.gestures.size() && t.gestures[next].beat <= beat) { c = next; ++next; }
        t.cursor = c;
        t.offset = c == none ? 0.0f : gestureValue(t.gestures[c], beat);
    }
    // The performer's mutes (live only): a muted group's notes are not played, its tails ring out.
    mutes_ = 0;
    if (live_)
        for (int k = 0; k < perform::kMutes; ++k)
            if (params_->getBool(params_->id(Module::Perform, 0, perform::MuteKick + k))) mutes_ |= 1u << k;
    // The keyboard (live only, 01.10.2026): what it plays, whether that replaces the composer's notes, whether the
    // composer plays at all.
    keyTarget_ = live_ ? params_->getInt(params_->id(Module::Perform, 0, perform::KeyboardPart)) : perform::keys::Off;
    keyReplace_ = !live_ || params_->getInt(params_->id(Module::Perform, 0, perform::KeyboardMode)) == 0;
    composerOff_ = live_ && !params_->getBool(params_->id(Module::Perform, 0, perform::Composer));

    float c[64];
    readPlayed(Module::Compose, 0, c);
    keyRoot_ = static_cast<int>(std::lround(c[compose::Key]));
    scale_ = static_cast<int>(std::lround(c[compose::Scale]));
    subOwns_ = std::lround(c[compose::LowOwner]) == static_cast<long>(LowOwner::Sub);
    const double bpm = score_.tempo.bpmAt(beat);

    // The global motion (Dok. 8.4): three LFOs of 7, 11 and 13 beats and a drift of 0.065 Hz, from the absolute beat
    // and second, so the phases do not depend on where a render starts. They step every 128 samples (an absolute
    // raster, 2.7 ms), so the kit recomputes its lanes a quarter as often.
    float mv[16];
    readPlayed(Module::Motion, 0, mv);
    const int64_t q = (sample / 128) * 128;
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
    // The group high pass: off at its floor (the sweep of the form raises it over 8 to 16 bars and lets it fall back).
    groupHpOn_ = v[mix::LowCut] > 20.5f;
    if (!groupHpOn_) {   // back at the floor: no stale state later
        for (auto& ch : groupHp_) for (Svf& f : ch) f.reset();
        for (StemBus& s : stemBus_) for (auto& ch : s.hp) for (Svf& f : ch) f.reset();
    }
    for (auto& ch : groupHp_) {
        ch[0].setK(v[mix::LowCut], 1.8477590f, fs);
        ch[1].setK(v[mix::LowCut], 0.7653669f, fs);
    }
    fxDuck_.set(v[mix::DuckLow], v[mix::DuckMid], 60.0f, 250.0f);

    readPlayed(Module::Space, 0, v);
    room_.set(v[space::Size], v[space::Decay], v[space::Damping], v[space::PreDelay] * 0.001f * fs, v[space::LowCut],
              v[space::HighCut]);
    roomReturn_ = v[space::Level] <= -59.9f ? 0.0f : dbToGain(v[space::Level] + kRoomMakeupDb);
    hatsSend_ = v[space::HatsSend];
    percSend_ = v[space::PercSend];
    pingSend_ = v[space::PingSend];

    // The track bus: the tilt and the glue are the track's (a style's recipe sets them), the master the engine's.
    readPlayed(Module::Master, 0, v);
    glue_.set(v[master::Threshold], v[master::Ratio], 6.0f, 20.0f, 200.0f);
    // The tilt: the highs against the lows by Tilt dB, the lows kept (a pivot at 1 kHz took the low end down by half the
    // tilt, and with it the level the Leveler had to win back).
    tiltHigh_ = dbToGain(v[master::Tilt]);

    // The Leveler's correction of the track playing: the score's, or the one found while it plays.
    float trim = score_.trimAt(beat);
    if (!lateTrims_.empty()) {
        size_t k = 0;
        for (size_t i = 0; i < score_.levels.size(); ++i) if (score_.levels[i].beat <= beat) k = i;
        if (k < lateTrims_.size()) trim = lateTrims_[k];
    }
    trimTarget_ = dbToGain(trim);
    // Phase 18: the parts' corrections, the same way.
    BalanceDb bal = score_.balanceAt(beat);
    if (!lateBal_.empty()) {
        size_t k = 0;
        for (size_t i = 0; i < score_.levels.size(); ++i) if (score_.levels[i].beat <= beat) k = i;
        if (k < lateBal_.size()) bal = lateBal_[k];
    }
    for (int p = 0; p < kBalParts; ++p) balTarget_[p] = dbToGain(bal[static_cast<size_t>(p)]);
}

void Deck::watchPeaks(bool on)
{
    watch_ = on;
    if (!on) return;
    kickPeak_ = 0.0f;
    for (float& p : partPeak_) p = 0.0f;
}

void Deck::dispatchUntil(int64_t sample)
{
    while (evCursor_ < events_.size() && events_[evCursor_].sample <= sample) dispatch(events_[evCursor_++]);
}

void Deck::dispatch(const Ev& e)
{
    const Part part = static_cast<Part>(e.part);
    if (e.on != 0 && !liveEvent_ && silenced(part)) return;   // the keyboard's or nobody's (01.10.2026)
    switch (part) {
    case Part::Kick: {
        if (muted(perform::MuteKick)) return;
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
            if (!muted(perform::MuteBass)) {
                bass_.noteOn(e.pitch, e.velocity, e.late, e.accent, e.slide);
                bassNote_ = e.id;
            }
            if (!subOwns_) return;   // the sine plays the line only where the sub owns the low end
        }
        if (muted(perform::MuteSub)) return;
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
        if (muted(perform::MuteBass)) return;
        acid_.noteOn(e.pitch, e.velocity, e.late, e.accent, e.slide);
        acidNote_ = e.id;
        return;
    case Part::Chord:
        if (e.on == 0) chord_.noteOff(e.pitch);
        else if (!muted(perform::MutePads)) chord_.noteOn(e.pitch, e.velocity, e.late);
        return;
    case Part::Drone:
        if (e.on == 0) { if (e.id == droneNote_) { drone_.noteOff(); droneNote_ = -1; } return; }
        if (muted(perform::MutePads)) return;
        drone_.noteOn(e.pitch, e.velocity);
        droneNote_ = e.id;
        return;
    case Part::Texture:
        texture_.gate(e.on != 0 && !muted(perform::MutePads));
        return;
    case Part::Ping:
        if (muted(perform::MutePing)) return;
        ping_.noteOn(e.pitch, e.velocity, e.late, static_cast<uint32_t>(e.id));
        return;
    default: {
        const int lane = laneOf(part);
        if (lane >= 0 && !muted(laneIsHat_[lane] ? perform::MuteHats : perform::MutePerc)) kit_.trigger(lane, e.velocity, e.shift, e.late);
        return;
    }
    }
}

void Deck::resetStems()
{
    for (StemBus& s : stemBus_) {
        for (auto& ch : s.hp) for (Svf& f : ch) f.reset();
        s.tilt[0] = s.tilt[1] = 0.0f;
    }
    for (auto& r : stemPads_) r = padsDuck_.replica();
    for (auto& r : stemFx_) r = fxDuck_.replica();
}

void Deck::render(int64_t sample, float* L, float* R, int n, float* const* stemL, float* const* stemR)
{
    const bool stems = stemL != nullptr;
    // The sources.
    { TOT_PROF(Kick); kick_.process(kickBuf_.data(), bodyBuf_.data(), n); }
    { TOT_PROF(Rumble); rumble_.process(bodyBuf_.data(), rumbleBuf_.data(), n); }
    { TOT_PROF(Sub); sub_.process(subBuf_.data(), n); }
    { TOT_PROF(Kit); kit_.processLanes(n); }
    { TOT_PROF(Ping); ping_.process(pingL_.data(), pingR_.data(), n, sample); }
    { TOT_PROF(Bass); bass_.process(bassL_.data(), bassR_.data(), n); }
    { TOT_PROF(Acid); acid_.process(acidL_.data(), acidR_.data(), n); }
    { TOT_PROF(Chord); chord_.process(chordL_.data(), chordR_.data(), n, sample); }
    { TOT_PROF(Drone); drone_.process(droneL_.data(), droneR_.data(), n); }
    { TOT_PROF(Texture); texture_.process(texL_.data(), texR_.data(), n); }
    TOT_PROF_BEGIN(Buses);

    // The percussion buses, the drum bus, the pads, the sends.
    const float* ll = kit_.laneL();
    const float* lr = kit_.laneR();
    float busH[2][kRaster], busP[2][kRaster];
    for (int i = 0; i < n; ++i) {
        // Phase 18: the parts' gains against the kick, at the sources -- every lane of the kit, every tonal voice (the sends
        // follow them, as after a fader).
        for (int p = 0; p < kBalParts; ++p) balGain_[p] += (balTarget_[p] - balGain_[p]) * trimCoef_;
        {
            const size_t k = static_cast<size_t>(i);
            const float gp = balGain_[static_cast<int>(BalPart::Ping)], gb = balGain_[static_cast<int>(BalPart::Bass)];
            const float ga = balGain_[static_cast<int>(BalPart::Acid)], gd = balGain_[static_cast<int>(BalPart::Drone)];
            const float gt = balGain_[static_cast<int>(BalPart::Texture)];
            pingL_[k] *= gp; pingR_[k] *= gp;
            bassL_[k] *= gb; bassR_[k] *= gb;
            acidL_[k] *= ga; acidR_[k] *= ga;
            droneL_[k] *= gd; droneR_[k] *= gd;
            texL_[k] *= gt; texR_[k] *= gt;
            if (watch_) {
                const auto most = [](float& m, float a, float b) { m = std::max(m, std::max(std::fabs(a), std::fabs(b))); };
                most(kickPeak_, kickBuf_[k], kickBuf_[k]);
                most(partPeak_[static_cast<int>(BalPart::Ping)], pingL_[k], pingR_[k]);
                most(partPeak_[static_cast<int>(BalPart::Bass)], bassL_[k], bassR_[k]);
                most(partPeak_[static_cast<int>(BalPart::Acid)], acidL_[k], acidR_[k]);
                most(partPeak_[static_cast<int>(BalPart::Chord)], chordL_[k] * chordLevel_ * balGain_[static_cast<int>(BalPart::Chord)],
                     chordR_[k] * chordLevel_ * balGain_[static_cast<int>(BalPart::Chord)]);
                most(partPeak_[static_cast<int>(BalPart::Drone)], droneL_[k], droneR_[k]);
                most(partPeak_[static_cast<int>(BalPart::Texture)], texL_[k], texR_[k]);
            }
        }
        float hl = 0.0f, hr = 0.0f, pl = 0.0f, pr = 0.0f;
        for (int l = 0; l < kPercLanes; ++l) {
            const size_t k = static_cast<size_t>(i * PercKit::kStride + l);
            const float a = ll[k] * balGain_[l], b = lr[k] * balGain_[l];
            if (laneIsHat_[l]) { hl += a; hr += b; }
            else { pl += a; pr += b; }
            if (watch_) partPeak_[l] = std::max(partPeak_[l], std::max(std::fabs(a), std::fabs(b)) * (laneIsHat_[l] ? hatsGain_ : percGain_));
        }
        busH[0][i] = hatsLp_[0].lp(hl) * hatsGain_;
        busH[1][i] = hatsLp_[1].lp(hr) * hatsGain_;
        busP[0][i] = percLp_[0].lp(pl) * percGain_;
        busP[1][i] = percLp_[1].lp(pr) * percGain_;
        const size_t k = static_cast<size_t>(i);
        const float gChord = chordLevel_ * balGain_[static_cast<int>(BalPart::Chord)];
        chordL_[k] *= gChord;
        chordR_[k] *= gChord;
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
        const float inL = dl, inR = dr;
        if (drumSat_ > 0.0f) {
            const float m = 0.43f * drumSat_;   // 15 % a stage at the default 0.35
            static const float kDrive[3] = { 1.5f, 2.0f, 2.5f };
            for (float d : kDrive) {
                dl += m * (std::tanh(d * dl) / d - dl);
                dr += m * (std::tanh(d * dr) / d - dr);
            }
        }
        if (stems) {   // the saturation's gain on the sum, for its parts
            satGain_[0][i] = inL != 0.0f ? dl / inL : 1.0f;
            satGain_[1][i] = inR != 0.0f ? dr / inR : 1.0f;
        }
        drumL_[k] = dl;
        drumR_[k] = dr;
    }
    TOT_PROF_END(Buses);
    // The mixer page's meters, while it looks: the sources after their levels and corrections, the drums by bus.
    float meterPk[MeterSink::kStrips] = {};
    double meterSs[MeterSink::kStrips] = {};
    const auto measure = [&](int strip, float l, float r) {
        meterPk[strip] = std::max(meterPk[strip], std::max(std::fabs(l), std::fabs(r)));
        meterSs[strip] += 0.5 * (static_cast<double>(l) * l + static_cast<double>(r) * r);
    };
    if (meter_ != nullptr) {
        for (int i = 0; i < n; ++i) {
            const size_t k = static_cast<size_t>(i);
            measure(MeterSink::Kick, kickBuf_[k], kickBuf_[k]);
            measure(MeterSink::Rumble, rumbleBuf_[k], rumbleBuf_[k]);
            measure(MeterSink::Sub, subBuf_[k], subBuf_[k]);
            measure(MeterSink::Hats, busH[0][i], busH[1][i]);
            measure(MeterSink::Perc, busP[0][i], busP[1][i]);
            measure(MeterSink::Ping, pingL_[k], pingR_[k]);
            measure(MeterSink::Bass, bassL_[k], bassR_[k]);
            measure(MeterSink::Acid, acidL_[k], acidR_[k]);
            measure(MeterSink::Chord, chordL_[k], chordR_[k]);
            measure(MeterSink::Drone, droneL_[k], droneR_[k]);
            measure(MeterSink::Texture, texL_[k], texR_[k]);
        }
    }
    TOT_PROF_BEGIN(Cloud);
    // The cloud hears the ping and the chord; half of it goes into the plate.
    for (int i = 0; i < n; ++i) {
        const size_t k = static_cast<size_t>(i);
        cloudInL_[k] = pingL_[k] * cloudPing_ + chordL_[k] * cloudChord_;
        cloudInR_[k] = pingR_[k] * cloudPing_ + chordR_[k] * cloudChord_;
    }
    if (quest_) {   // the Quest's quality (PLAN 11): no grains
        std::fill(cloudL_.begin(), cloudL_.begin() + n, 0.0f);
        std::fill(cloudR_.begin(), cloudR_.begin() + n, 0.0f);
    } else {
        cloud_.process(cloudInL_.data(), cloudInR_.data(), cloudL_.data(), cloudR_.data(), n);
    }
    for (int i = 0; i < n; ++i) {
        const size_t k = static_cast<size_t>(i);
        plateInL_[k] += cloudL_[k] * cloudPlate_;
        plateInR_[k] += cloudR_[k] * cloudPlate_;
    }
    TOT_PROF_END(Cloud);
    { TOT_PROF(Room); room_.process(roomInL_.data(), roomInR_.data(), roomL_.data(), roomR_.data(), n); }
    { TOT_PROF(Dub); dub_.process(echoInL_.data(), echoInR_.data(), plateInL_.data(), plateInR_.data(), dubL_.data(), dubR_.data(), n); }
    if (meter_ != nullptr) {   // the returns
        const float gRoom = roomReturn_ * balGain_[static_cast<int>(BalPart::Room)];
        for (int i = 0; i < n; ++i) {
            const size_t k = static_cast<size_t>(i);
            measure(MeterSink::Room, roomL_[k] * gRoom, roomR_[k] * gRoom);
            measure(MeterSink::Dub, dubL_[k], dubR_[k]);
            measure(MeterSink::Cloud, cloudL_[k], cloudR_[k]);
        }
        meter_->add(meterPk, meterSs, 0, MeterSink::kStrips);
    }
    if (stems) {
        // The parts of the pads and of the returns, before their ducks.
        for (int i = 0; i < n; ++i) {
            const size_t k = static_cast<size_t>(i);
            stemL[kStemChord][i] = chordL_[k]; stemR[kStemChord][i] = chordR_[k];
            stemL[kStemDrone][i] = droneL_[k]; stemR[kStemDrone][i] = droneR_[k];
            const float gRoom = roomReturn_ * balGain_[static_cast<int>(BalPart::Room)];
            stemL[kStemRoom][i] = roomL_[k] * gRoom; stemR[kStemRoom][i] = roomR_[k] * gRoom;
            stemL[kStemDub][i] = dubL_[k]; stemR[kStemDub][i] = dubR_[k];
            stemL[kStemCloud][i] = cloudL_[k]; stemR[kStemCloud][i] = cloudR_[k];
        }
    }
    // The pads (chord and drone) and the returns step aside for the kick, band by band.
    for (int i = 0; i < n; ++i) {
        const size_t k = static_cast<size_t>(i);
        chordL_[k] += droneL_[k];
        chordR_[k] += droneR_[k];
        const float gRoom = roomReturn_ * balGain_[static_cast<int>(BalPart::Room)];   // (Phase 18: the guard's)
        roomL_[k] = roomL_[k] * gRoom + dubL_[k] + cloudL_[k];
        roomR_[k] = roomR_[k] * gRoom + dubR_[k] + cloudR_[k];
    }
    TOT_PROF_BEGIN(Ducks);
    padsDuck_.process(chordL_.data(), chordR_.data(), n, stems ? padsGains_ : nullptr);
    fxDuck_.process(roomL_.data(), roomR_.data(), n, stems ? fxGains_ : nullptr);
    TOT_PROF_END(Ducks);
    if (stems) {
        MultibandDucker::apply(stemPads_[0], padsGains_, stemL[kStemChord], stemR[kStemChord], n);
        MultibandDucker::apply(stemPads_[1], padsGains_, stemL[kStemDrone], stemR[kStemDrone], n);
        MultibandDucker::apply(stemFx_[0], fxGains_, stemL[kStemRoom], stemR[kStemRoom], n);
        MultibandDucker::apply(stemFx_[1], fxGains_, stemL[kStemDub], stemR[kStemDub], n);
        MultibandDucker::apply(stemFx_[2], fxGains_, stemL[kStemCloud], stemR[kStemCloud], n);
        for (int i = 0; i < n; ++i) {
            const size_t k = static_cast<size_t>(i);
            auto put = [&](int s, float l, float r) { stemL[s][i] = l; stemR[s][i] = r; };
            put(kStemKick, kickBuf_[k] * satGain_[0][i], kickBuf_[k] * satGain_[1][i]);
            put(kStemRumble, rumbleBuf_[k] * satGain_[0][i], rumbleBuf_[k] * satGain_[1][i]);
            put(kStemSub, subBuf_[k], subBuf_[k]);
            put(kStemHats, busH[0][i] * satGain_[0][i], busH[1][i] * satGain_[1][i]);
            put(kStemPerc, busP[0][i] * satGain_[0][i], busP[1][i] * satGain_[1][i]);
            put(kStemPing, pingL_[k], pingR_[k]);
            put(kStemBass, bassL_[k], bassR_[k]);
            put(kStemAcid, acidL_[k], acidR_[k]);
            put(kStemTexture, texL_[k], texR_[k]);
        }
    }

    TOT_PROF_BEGIN(TrackBus);
    for (int i = 0; i < n; ++i) {
        const size_t k = static_cast<size_t>(i);
        float l = drumL_[k] + subBuf_[k] + pingL_[k] + bassL_[k] + acidL_[k] + chordL_[k] + texL_[k] + roomL_[k];
        float r = drumR_[k] + subBuf_[k] + pingR_[k] + bassR_[k] + acidR_[k] + chordR_[k] + texR_[k] + roomR_[k];
        if (groupHpOn_) {
            float lp, bp, hp;
            groupHp_[0][0].tick(l, lp, bp, hp); groupHp_[0][1].tick(hp, lp, bp, l);
            groupHp_[1][0].tick(r, lp, bp, hp); groupHp_[1][1].tick(hp, lp, bp, r);
        }
        // The tilt: a complementary one-pole split at 1 kHz, the highs apart.
        tiltState_[0] += tiltCoef_ * (l - tiltState_[0]);
        tiltState_[1] += tiltCoef_ * (r - tiltState_[1]);
        L[i] = tiltState_[0] + tiltHigh_ * (l - tiltState_[0]);
        R[i] = tiltState_[1] + tiltHigh_ * (r - tiltState_[1]);
        drumL_[k] = L[i];   // the glue's dry signal (the drum bus is spent)
        drumR_[k] = R[i];
    }
    TOT_PROF_END(TrackBus);
    TOT_PROF_BEGIN(Glue);
    glue_.process(L, R, n, stems ? glueGains_ : nullptr);
    // The glue in parallel, then the Leveler's trim: after the glue, which would otherwise halve every correction (a
    // ratio of 2 far over its threshold).
    for (int i = 0; i < n; ++i) {
        const size_t k = static_cast<size_t>(i);
        trimGain_ += (trimTarget_ - trimGain_) * trimCoef_;
        trimGains_[i] = trimGain_;
        L[i] = (drumL_[k] + kGlueMix * (L[i] - drumL_[k])) * trimGain_;
        R[i] = (drumR_[k] + kGlueMix * (R[i] - drumR_[k])) * trimGain_;
    }
    TOT_PROF_END(Glue);
    if (!stems) return;
    // The stems through their own group high pass and tilt, then the glue's and the trim's gains.
    for (int s = 0; s < kStems; ++s) {
        StemBus& b = stemBus_[s];
        for (int c = 0; c < 2; ++c) for (int q = 0; q < 2; ++q) b.hp[c][q].copyCoefficients(groupHp_[c][q]);
        float* ch[2] = { stemL[s], stemR[s] };
        for (int c = 0; c < 2; ++c) {
            for (int i = 0; i < n; ++i) {
                float x = ch[c][i];
                if (groupHpOn_) {
                    float lp, bp, hp;
                    b.hp[c][0].tick(x, lp, bp, hp); b.hp[c][1].tick(hp, lp, bp, x);
                }
                b.tilt[c] += tiltCoef_ * (x - b.tilt[c]);
                x = b.tilt[c] + tiltHigh_ * (x - b.tilt[c]);
                ch[c][i] = x * (1.0f + kGlueMix * (glueGains_[i] - 1.0f)) * trimGains_[i];
            }
        }
    }
}

int Deck::targetOf(Part part)
{
    switch (part) {
    case Part::Kick: return perform::keys::Kit;
    case Part::Sub:
    case Part::Bass: return perform::keys::Bass;
    case Part::Acid: return perform::keys::Acid;
    case Part::Ping: return perform::keys::Ping;
    case Part::Chord: return perform::keys::Chord;
    case Part::Drone: return perform::keys::Drone;
    default: return laneOf(part) >= 0 ? perform::keys::Kit : perform::keys::Off;
    }
}

bool Deck::silenced(Part part) const
{
    if (composerOff_) return true;
    if (keyTarget_ == perform::keys::Off || !keyReplace_) return false;
    const int t = targetOf(part);
    if (t == perform::keys::Off) return false;
    // By channel, a part is the player's from its first played key on (until liveAllOff).
    if (keyTarget_ == perform::keys::ByChannel) return ((keyPlayed_ >> t) & 1u) != 0;
    return keyTarget_ == t;
}

void Deck::liveNote(int64_t sample, int target, int pitch, float velocity, bool on)
{
    if (!loaded_ || target <= perform::keys::Off || target >= perform::keys::ByChannel || pitch < 0 || pitch > 127) return;
    Ev e{};
    e.sample = sample;
    e.on = on ? 1 : 0;
    e.pitch = pitch;
    e.velocity = std::clamp(velocity, 0.0f, 1.0f);
    e.accent = velocity > 0.86f;   // a hard key is the 303's accent
    e.id = 0x40000000 + pitch;     // a key's own id: its release finds its note
    switch (target) {
    case perform::keys::Kit:
        if (!on) return;   // one-shots: a release does nothing
        if (pitch == 36) e.part = static_cast<uint8_t>(Part::Kick);
        else if (pitch > 36 && pitch <= 36 + kPercLanes) e.part = static_cast<uint8_t>(percPart(pitch - 37));
        else return;
        break;
    case perform::keys::Bass: e.part = static_cast<uint8_t>(Part::Bass); break;
    case perform::keys::Acid: e.part = static_cast<uint8_t>(Part::Acid); break;
    case perform::keys::Ping:
        if (!on) return;
        e.part = static_cast<uint8_t>(Part::Ping);
        break;
    case perform::keys::Chord:
        e.part = static_cast<uint8_t>(Part::Chord);
        liveChord_[pitch] = on ? 1 : 0;
        break;
    case perform::keys::Drone: e.part = static_cast<uint8_t>(Part::Drone); break;
    default: return;
    }
    if (target == perform::keys::Bass || target == perform::keys::Acid || target == perform::keys::Drone) {
        // A mono voice: one id for all its keys; a key pressed while another is held slides there, the release of a
        // key that no longer sounds does nothing.
        int& held = liveHeld_[target];
        e.id = 0x40000000;
        if (on) { e.slide = held != 0; held = pitch + 1; }
        else if (held != pitch + 1) return;
        else held = 0;
    }
    liveEvent_ = true;
    dispatch(e);
    liveEvent_ = false;
    if (on) keyPlayed_ |= 1u << target;
}

void Deck::liveAllOff()
{
    for (int t : { perform::keys::Bass, perform::keys::Acid, perform::keys::Drone })
        if (liveHeld_[t] != 0) liveNote(0, t, liveHeld_[t] - 1, 0.0f, false);
    for (int k = 0; k < 128; ++k)
        if (liveChord_[k] != 0) liveNote(0, perform::keys::Chord, k, 0.0f, false);
    keyPlayed_ = 0;
}

} // namespace tot
