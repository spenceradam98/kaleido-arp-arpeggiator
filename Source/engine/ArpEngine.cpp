#include "ArpEngine.h"

#include <algorithm>
#include <cmath>

namespace kal
{

namespace
{
    constexpr double kEps = 1e-9;

    std::vector<int> modeOrder (int n, ArpMode mode)
    {
        std::vector<int> o;
        if (n <= 0) return o;
        auto up = [&] { for (int i = 0; i < n; ++i) o.push_back (i); };
        auto down = [&] { for (int i = n - 1; i >= 0; --i) o.push_back (i); };
        auto converge = [&] (std::vector<int>& v)
        {
            int lo = 0, hi = n - 1;
            while (lo <= hi)
            {
                v.push_back (lo);
                if (hi != lo) v.push_back (hi);
                ++lo; --hi;
            }
        };

        switch (mode)
        {
            case ArpMode::Down: down(); break;
            case ArpMode::UpDown:
                up();
                for (int i = n - 2; i >= 1; --i) o.push_back (i);
                break;
            case ArpMode::DownUp:
                down();
                for (int i = 1; i <= n - 2; ++i) o.push_back (i);
                break;
            case ArpMode::UpDownIncl: up(); down(); break;
            case ArpMode::DownUpIncl: down(); up(); break;
            case ArpMode::Converge: converge (o); break;
            case ArpMode::Diverge:
                converge (o);
                std::reverse (o.begin(), o.end());
                break;
            case ArpMode::ConDiverge:
            {
                converge (o);
                std::vector<int> d (o.rbegin(), o.rend());
                for (size_t i = 1; i + 1 < d.size(); ++i) o.push_back (d[i]);
                break;
            }
            case ArpMode::ThumbUp:
                if (n == 1) o.push_back (0);
                for (int i = 1; i < n; ++i) { o.push_back (0); o.push_back (i); }
                break;
            case ArpMode::PinkyUp:
                if (n == 1) o.push_back (0);
                for (int i = 0; i < n - 1; ++i) { o.push_back (i); o.push_back (n - 1); }
                break;
            default: up(); break;
        }
        return o;
    }

    bool isChordMode (ArpMode m) { return m == ArpMode::Chord || m == ArpMode::StrumUp || m == ArpMode::StrumDown; }
}

ArpEngine::ArpEngine()
{
    held.reserve (128);
    physicallyDown.reserve (128);
    seq.reserve (1024);
    pending.reserve (4096);
    lastStepNotes.reserve (128);
    shuffleOrder.reserve (1024);
    rng.seed (settings.seed);
}

void ArpEngine::prepare (double sr)
{
    sampleRate = sr > 0 ? sr : 44100.0;
    reset();
}

void ArpEngine::reset()
{
    held.clear();
    physicallyDown.clear();
    pending.clear();
    lastStepNotes.clear();
    for (auto& ch : soundingSerial)
        for (auto& s : ch) s = 0;
    running = false;
    seqDirty = true;
    seqPos = 0; repeatCount = 0;
    nextK = 0; stepCounter = 0; slotStart = 0; curSlot = 0;
    lastBlockEnd = -1.0;
    hostMode = false;
    lastStepLen = 0.0;
    uiStepCounter = -1;
    uiHeldCount = 0;
}

int ArpEngine::outChannelFor (int inChannel) const
{
    return settings.outChannel >= 1 && settings.outChannel <= 16 ? settings.outChannel - 1 : (inChannel & 15);
}

double ArpEngine::stepBeat (long long k, double stepLen) const
{
    double t = gridOrigin + (double) k * stepLen;
    if ((k & 1) != 0) t += (double) settings.swing * 0.5 * stepLen;
    return t;
}

std::vector<int> ArpEngine::heldChordShape() const
{
    std::vector<int> pitches;
    for (auto& h : held) pitches.push_back (h.pitch);
    std::sort (pitches.begin(), pitches.end());
    pitches.erase (std::unique (pitches.begin(), pitches.end()), pitches.end());
    std::vector<int> shape;
    for (int p : pitches) shape.push_back (p - pitches.front());
    return shape;
}

void ArpEngine::rebuildSequence()
{
    seqDirty = false;
    seq.clear();
    if (held.empty()) return;

    // 1. pool of (pitch, velocity), ordered by pitch or by arrival
    struct PV { int pitch, vel, order; };
    std::vector<PV> pool;
    pool.reserve (256);
    for (auto& h : held)
    {
        if (settings.chordMemory && ! cur().chordMemory.empty())
        {
            for (int iv : cur().chordMemory)
                pool.push_back ({ h.pitch + iv, h.velocity, h.order * 64 + (iv & 63) });
        }
        else
            pool.push_back ({ h.pitch, h.velocity, h.order * 64 });
    }

    const bool asPlayed = settings.mode == ArpMode::AsPlayed;
    std::stable_sort (pool.begin(), pool.end(), [&] (const PV& a, const PV& b)
                      { return asPlayed ? a.order < b.order : a.pitch < b.pitch; });
    {   // de-duplicate pitches (keep the first occurrence)
        std::vector<PV> unique;
        for (auto& p : pool)
            if (std::none_of (unique.begin(), unique.end(), [&] (const PV& u) { return u.pitch == p.pitch; }))
                unique.push_back (p);
        pool.swap (unique);
    }

    const int octs = std::clamp (settings.octaves, 1, 4);
    const auto om = settings.octaveMode;

    // Octave walk for block-based modes.
    std::vector<int> walk;
    if (om == OctaveMode::Down)
        for (int o = 0; o < octs; ++o) walk.push_back (-o);
    else
        for (int o = 0; o < octs; ++o) walk.push_back (o);
    if (om == OctaveMode::UpDown)
        for (int o = octs - 2; o >= 1; --o) walk.push_back (o);
    if (om == OctaveMode::Random)
        walk = { 0 };

    auto single = [] (int pitch, int vel) { Group g; g.notes[0] = pitch; g.vel[0] = vel; g.count = 1; return g; };

    if (isChordMode (settings.mode))
    {
        for (int o : walk)
        {
            Group g;
            for (auto& p : pool)
                if (g.count < 16) { g.notes[g.count] = p.pitch + 12 * o; g.vel[g.count] = p.vel; ++g.count; }
            if (settings.mode == ArpMode::StrumDown)
            {
                std::reverse (g.notes, g.notes + g.count);
                std::reverse (g.vel, g.vel + g.count);
            }
            seq.push_back (g);
        }
    }
    else if (om == OctaveMode::Up || om == OctaveMode::Down)
    {
        // Spread the pool across octaves first, then order the whole range with the mode.
        std::vector<PV> flat;
        for (int o : walk)
            for (auto& p : pool) flat.push_back ({ p.pitch + 12 * o, p.vel, p.order + o * 100000 });
        if (! asPlayed)
            std::stable_sort (flat.begin(), flat.end(), [] (const PV& a, const PV& b) { return a.pitch < b.pitch; });
        for (int idx : modeOrder ((int) flat.size(), settings.mode))
            seq.push_back (single (flat[(size_t) idx].pitch, flat[(size_t) idx].vel));
    }
    else if (om == OctaveMode::Interleave)
    {
        for (int idx : modeOrder ((int) pool.size(), settings.mode))
            for (int o = 0; o < octs; ++o)
                seq.push_back (single (pool[(size_t) idx].pitch + 12 * o, pool[(size_t) idx].vel));
    }
    else // UpDown octave walk, or Random (one block; octave chosen per step)
    {
        auto order = modeOrder ((int) pool.size(), settings.mode);
        for (int o : walk)
            for (int idx : order)
                seq.push_back (single (pool[(size_t) idx].pitch + 12 * o, pool[(size_t) idx].vel));
    }

    const int n = (int) seq.size();
    if (n > 0) seqPos %= n;
    shuffleOrder.clear();
}

bool ArpEngine::nextGroup (Group& g)
{
    if (seqDirty) rebuildSequence();
    const int n = (int) seq.size();
    if (n == 0) return false;

    if (repeatCount == 0)
    {
        switch (settings.mode)
        {
            case ArpMode::Random:
                seqPos = (int) (rng() % (uint32_t) n);
                break;
            case ArpMode::RandomWalk:
            {
                int next = seqPos + ((rng() & 1u) ? 1 : -1);
                if (next < 0) next = n > 1 ? 1 : 0;
                if (next >= n) next = n > 1 ? n - 2 : 0;
                seqPos = next;
                break;
            }
            case ArpMode::Shuffle:
            {
                if ((int) shuffleOrder.size() != n + 1 || shuffleOrder[(size_t) n] >= n)
                {
                    shuffleOrder.resize ((size_t) n + 1);
                    for (int i = 0; i < n; ++i) shuffleOrder[(size_t) i] = i;
                    std::shuffle (shuffleOrder.begin(), shuffleOrder.begin() + n, rng);
                    shuffleOrder[(size_t) n] = 0;   // cursor
                }
                seqPos = shuffleOrder[(size_t) shuffleOrder[(size_t) n]++];
                break;
            }
            default: break;
        }
    }

    g = seq[(size_t) (seqPos % n)];

    if (settings.octaveMode == OctaveMode::Random && settings.octaves > 1)
    {
        const int o = (int) (rng() % (uint32_t) std::clamp (settings.octaves, 1, 4));
        for (int i = 0; i < g.count; ++i) g.notes[i] += 12 * o;
    }

    const int reps = std::clamp (settings.noteRepeat, 1, 8);
    repeatCount = (repeatCount + 1) % reps;
    if (repeatCount == 0)
    {
        const bool sequential = settings.mode != ArpMode::Random && settings.mode != ArpMode::RandomWalk
                                && settings.mode != ArpMode::Shuffle;
        if (sequential) seqPos = (seqPos + 1) % n;
    }
    return true;
}

static int quantizeToScale (int p, int root, uint16_t mask)
{
    auto inScale = [&] (int x) { return (mask >> (((x - root) % 12 + 12) % 12)) & 1u; };
    for (int d = 0; d < 12; ++d)
    {
        if (inScale (p - d)) return p - d;
        if (inScale (p + d)) return p + d;
    }
    return p;
}

int ArpEngine::shapePitch (int pitch, int octaveOffset, int semitoneOffset) const
{
    int p = pitch + settings.transpose;
    const auto& sc = scales();
    const int si = std::clamp (settings.scaleIndex, 0, (int) sc.size() - 1);
    if (si > 0)
    {
        const uint16_t mask = sc[(size_t) si].mask;
        const int root = settings.scaleRoot;
        if (settings.degreeMode)
        {
            p = quantizeToScale (p, root, mask);
            const int dir = semitoneOffset > 0 ? 1 : -1;
            for (int n = std::abs (semitoneOffset); n > 0; --n)
            {
                p += dir;
                for (int guard = 0; guard < 12 && ! ((mask >> (((p - root) % 12 + 12) % 12)) & 1u); ++guard)
                    p += dir;
            }
        }
        else
            p = quantizeToScale (p + semitoneOffset, root, mask);
    }
    else
        p += semitoneOffset;

    p += 12 * octaveOffset;

    const int lo = std::clamp (settings.rangeLow, 0, 127);
    const int hi = std::clamp (settings.rangeHigh, lo, 127);
    if (hi - lo >= 11)
    {
        while (p > hi) p -= 12;
        while (p < lo) p += 12;
    }
    else
        p = std::clamp (p, lo, hi);

    return (p < 0 || p > 127) ? -1 : p;
}

void ArpEngine::scheduleNote (int pitch, int velocity, int channel, double onBeat, double offBeat)
{
    const uint32_t serial = serialCounter++;
    if (serialCounter == 0) serialCounter = 1;
    pending.push_back ({ onBeat, (uint8_t) (0x90 | channel), (uint8_t) pitch, (uint8_t) velocity, serial });
    pending.push_back ({ offBeat, (uint8_t) (0x80 | channel), (uint8_t) pitch, 0, serial });
    lastStepNotes.push_back ({ channel, pitch, serial });
}

void ArpEngine::fireStep (double beat, double stepLen)
{
    // Pattern slots: either the selected slot, or a chain A..N that moves on after each slot
    // has played its rhythm lane `chainRepeats` times.
    const int chain = std::clamp (settings.chainLength, 1, kNumSlots);
    if (chain <= 1)
    {
        const int want = std::clamp (settings.activeSlot, 0, kNumSlots - 1);
        if (want != curSlot) { curSlot = want; seqDirty = true; }
    }
    else
    {
        if (curSlot >= chain) { curSlot = 0; slotStart = stepCounter; seqDirty = true; }
        const auto& rl = cur().lane (Lane::Rhythm);
        const long long cycle = (long long) std::max (1, rl.length) * std::max (1, rl.divide) * std::clamp (settings.chainRepeats, 1, 8);
        if (stepCounter - slotStart >= cycle)
        {
            curSlot = (curSlot + 1) % chain;
            slotStart = stepCounter;
            seqDirty = true;
        }
    }
    uiSlot = curSlot;

    const int s = (int) ((stepCounter - slotStart) % 1000000);
    uiStepCounter = s;
    ++stepCounter;

    const int rhythm = cur().value (Lane::Rhythm, s);
    const double gateFrac = std::clamp (cur().value (Lane::Gate, s) / 100.0 * settings.gateScale, 0.02, 4.0);

    if (rhythm == kTie)
    {
        // Hold the previous step's notes through this step.
        const double newOff = beat + stepLen * gateFrac;
        for (auto& p : pending)
            if ((p.status & 0xF0) == 0x80)
                for (auto& sn : lastStepNotes)
                    if (sn.serial == p.serial) p.beat = newOff;
        return;
    }

    if (rhythm == kRest || (int) (rng() % 100u) >= cur().value (Lane::Probability, s))
    {
        if (settings.advanceOnRest) { Group skip; nextGroup (skip); }
        return;
    }

    Group g;
    if (! nextGroup (g)) return;

    const int oct = cur().value (Lane::Octave, s);
    const int semi = cur().value (Lane::Semitone, s);
    const int ratchets = std::clamp (cur().value (Lane::Ratchet, s), 1, 8);
    const int laneVel = cur().value (Lane::Velocity, s);
    const double sub = stepLen / ratchets;
    const double len = ratchets > 1 ? sub * std::min (gateFrac, 0.9) : stepLen * gateFrac;
    // If the next step is a tie, keep the last hit sounding until that step takes it over.
    const bool nextIsTie = cur().value (Lane::Rhythm, s + 1) == kTie;
    const double beatsPerSecond = currentBpm / 60.0;
    const bool strum = settings.mode == ArpMode::StrumUp || settings.mode == ArpMode::StrumDown;
    const int channel = outChannelFor (lastInputChannel);

    lastStepNotes.clear();
    std::uniform_real_distribution<double> uni (0.0, 1.0);

    for (int r = 0; r < ratchets; ++r)
    {
        for (int i = 0; i < g.count; ++i)
        {
            const int pitch = shapePitch (g.notes[i], oct, semi);
            if (pitch < 0) continue;

            int vel = g.vel[i];
            if (settings.velSource == VelocitySource::Lane) vel = laneVel;
            else if (settings.velSource == VelocitySource::Mix) vel = g.vel[i] * laneVel / 127;
            if (settings.humaniseVel > 0)
                vel += (int) std::lround ((uni (rng) * 2.0 - 1.0) * settings.humaniseVel);
            vel = std::clamp (vel, 1, 127);

            double on = beat + r * sub;
            if (strum) on += i * (settings.strumMs / 1000.0) * beatsPerSecond;
            if (settings.humaniseMs > 0.0f) on += uni (rng) * (settings.humaniseMs / 1000.0) * beatsPerSecond;

            const double noteLen = (nextIsTie && r == ratchets - 1) ? std::max (len, stepLen * 1.6) : len;
            scheduleNote (pitch, vel, channel, on, on + std::max (noteLen, 0.001));
            uiLastNote = pitch;
        }
    }
}

void ArpEngine::allNotesOff (std::vector<MidiEvent>& out, int sampleOffset)
{
    for (int ch = 0; ch < 16; ++ch)
        for (int p = 0; p < 128; ++p)
            if (soundingSerial[ch][p] != 0)
            {
                out.push_back ({ sampleOffset, (uint8_t) (0x80 | ch), (uint8_t) p, 0 });
                soundingSerial[ch][p] = 0;
            }
    pending.clear();
    lastStepNotes.clear();
}

void ArpEngine::handleInput (const MidiEvent& e, double beat, std::vector<MidiEvent>& out, int sampleOffset)
{
    const int type = e.status & 0xF0;
    const int ch = e.status & 0x0F;
    const bool noteOn = type == 0x90 && e.data2 > 0;
    const bool noteOff = type == 0x80 || (type == 0x90 && e.data2 == 0);

    if (! noteOn && ! noteOff)
    {
        if (type == 0xB0 && (e.data1 == 123 || e.data1 == 120))
        {
            held.clear(); physicallyDown.clear(); seqDirty = true; running = false;
            allNotesOff (out, sampleOffset);
        }
        out.push_back ({ sampleOffset, e.status, e.data1, e.data2 });   // CCs, bend, aftertouch pass through
        return;
    }

    if (noteOn)
    {
        if (settings.latch && physicallyDown.empty())
            held.clear();                                   // a new gesture replaces the latched chord
        if (std::find (physicallyDown.begin(), physicallyDown.end(), (int) e.data1) == physicallyDown.end())
            physicallyDown.push_back (e.data1);
        auto it = std::find_if (held.begin(), held.end(), [&] (const Held& h) { return h.pitch == e.data1; });
        if (it == held.end()) held.push_back ({ e.data1, e.data2, ch, orderCounter++ });
        else it->velocity = e.data2;
        lastInputChannel = ch;
        seqDirty = true;

        if (! running)
        {
            running = true;
            const double L = rates()[(size_t) std::clamp (settings.rateIndex, 0, (int) rates().size() - 1)].beats;
            lastStepLen = L;
            if (settings.retrigger)
            {
                seqPos = 0; repeatCount = 0; stepCounter = 0; slotStart = 0;
                if (settings.chainLength > 1) curSlot = 0;
                shuffleOrder.clear();
            }
            if (hostMode)
            {
                gridOrigin = 0.0;
                const double pos = beat / L;
                const double fl = std::floor (pos + kEps);
                // Pressed just after a grid line: play now rather than waiting a whole step.
                nextK = (pos - fl) * L < 0.06 ? (long long) fl : (long long) fl + 1;
                if (! settings.retrigger) stepCounter = nextK;
                if (nextK == (long long) fl) fireStep (beat, L), ++nextK;
            }
            else
            {
                gridOrigin = beat;
                nextK = 0;
            }
        }
    }
    else
    {
        physicallyDown.erase (std::remove (physicallyDown.begin(), physicallyDown.end(), (int) e.data1), physicallyDown.end());
        if (! settings.latch)
        {
            held.erase (std::remove_if (held.begin(), held.end(), [&] (const Held& h) { return h.pitch == e.data1; }), held.end());
            seqDirty = true;
        }
        if (held.empty()) running = false;
    }
    uiHeldCount = (int) held.size();
}

void ArpEngine::processSegment (double startBeat, double endBeat, std::vector<MidiEvent>& out, int numSamples)
{
    const double L = rates()[(size_t) std::clamp (settings.rateIndex, 0, (int) rates().size() - 1)].beats;

    if (running)
    {
        if (std::abs (L - lastStepLen) > kEps && lastStepLen > 0.0)
        {
            // Rate changed: keep playing from the next grid line of the new rate.
            if (hostMode) { gridOrigin = 0.0; nextK = (long long) std::ceil (startBeat / L - kEps); }
            else { gridOrigin = stepBeat (nextK, lastStepLen); nextK = 0; }
        }
        lastStepLen = L;

        for (int guard = 0; guard < 4096; ++guard)
        {
            const double t = stepBeat (nextK, L);
            if (t >= endBeat) break;
            fireStep (std::max (t, startBeat), L);
            ++nextK;
        }
    }
    else
        lastStepLen = L;

    // Emit everything due before endBeat; note-offs before note-ons at the same time.
    std::stable_sort (pending.begin(), pending.end(), [] (const Pending& a, const Pending& b)
    {
        if (a.beat != b.beat) return a.beat < b.beat;
        return (a.status & 0xF0) == 0x80 && (b.status & 0xF0) == 0x90;
    });

    size_t i = 0;
    for (; i < pending.size() && pending[i].beat < endBeat; ++i)
    {
        const auto& p = pending[i];
        int off = (int) std::floor ((p.beat - blockStartBeat) / beatsPerSample);
        off = std::clamp (off, 0, std::max (0, numSamples - 1));
        const int ch = p.status & 0x0F;
        if ((p.status & 0xF0) == 0x90)
        {
            if (soundingSerial[ch][p.data1] != 0)
                out.push_back ({ off, (uint8_t) (0x80 | ch), p.data1, 0 });   // retrigger same pitch cleanly
            soundingSerial[ch][p.data1] = p.serial;
            out.push_back ({ off, p.status, p.data1, p.data2 });
        }
        else if (soundingSerial[ch][p.data1] == p.serial)
        {
            soundingSerial[ch][p.data1] = 0;
            out.push_back ({ off, p.status, p.data1, 0 });
        }
    }
    pending.erase (pending.begin(), pending.begin() + (long) i);
}

void ArpEngine::process (const std::vector<MidiEvent>& in, std::vector<MidiEvent>& out,
                         int numSamples, const TransportInfo& transport)
{
    const size_t outStart = out.size();

    if (settings.seed != lastSeed) { rng.seed (settings.seed); lastSeed = settings.seed; }

    if (! settings.enabled)
    {
        if (lastEnabled)
        {
            allNotesOff (out, 0);
            held.clear(); physicallyDown.clear(); running = false; seqDirty = true;
        }
        lastEnabled = false;
        for (auto& e : in) out.push_back (e);
        return;
    }
    lastEnabled = true;

    const int seqKey = (int) settings.mode * 1000 + settings.octaves * 100 + (int) settings.octaveMode * 10 + (settings.chordMemory ? 1 : 0);
    if (seqKey != lastSeqKey) { seqDirty = true; lastSeqKey = seqKey; }
    if (! (cur().chordMemory == lastChordMemory)) { seqDirty = true; lastChordMemory = cur().chordMemory; }

    // Latch switched off with no keys down: release the latched chord.
    if (! settings.latch && physicallyDown.empty() && ! held.empty())
    {
        held.clear(); running = false; seqDirty = true;
    }

    const bool host = settings.syncToHost && transport.hostPlaying;
    currentBpm = (settings.syncToHost && transport.bpm > 0.0) ? transport.bpm : std::clamp ((double) settings.internalBpm, 20.0, 400.0);
    beatsPerSample = currentBpm / 60.0 / sampleRate;

    if (host)
    {
        blockStartBeat = transport.ppq;
        const bool jumped = ! hostMode || std::abs (blockStartBeat - lastBlockEnd) > 1e-3;
        hostMode = true;
        if (jumped)
        {
            allNotesOff (out, 0);
            const double L = rates()[(size_t) std::clamp (settings.rateIndex, 0, (int) rates().size() - 1)].beats;
            gridOrigin = 0.0;
            lastStepLen = L;
            nextK = (long long) std::ceil (blockStartBeat / L - kEps);
            if (! settings.retrigger) stepCounter = nextK;
        }
    }
    else
    {
        if (hostMode)
        {
            // Host stopped: silence, then keep running on the free clock if keys are held.
            allNotesOff (out, 0);
            hostMode = false;
            gridOrigin = freeBeat;
            nextK = 0;
        }
        blockStartBeat = freeBeat;
    }

    const double blockEndBeat = blockStartBeat + numSamples * beatsPerSample;
    auto beatAt = [&] (int sample) { return blockStartBeat + sample * beatsPerSample; };

    int cursor = 0;
    for (auto& e : in)
    {
        const int pos = std::clamp (e.sampleOffset, 0, std::max (0, numSamples - 1));
        if (pos > cursor)
        {
            processSegment (beatAt (cursor), beatAt (pos), out, numSamples);
            cursor = pos;
        }
        handleInput (e, beatAt (pos), out, pos);
    }
    processSegment (beatAt (cursor), blockEndBeat, out, numSamples);

    lastBlockEnd = blockEndBeat;
    freeBeat = host ? freeBeat + numSamples * beatsPerSample : blockEndBeat;

    std::stable_sort (out.begin() + (long) outStart, out.end(),
                      [] (const MidiEvent& a, const MidiEvent& b) { return a.sampleOffset < b.sampleOffset; });
}

} // namespace kal
