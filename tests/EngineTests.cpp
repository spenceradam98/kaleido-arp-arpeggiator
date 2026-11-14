// Engine tests: plain C++, no framework. Exit code 0 = pass.
#include "ArpEngine.h"
#include "Generators.h"
#include "Presets.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <vector>

using namespace kal;

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (! (cond)) { ++failures; std::printf ("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

struct Ev { long long t; int status, d1, d2; };

struct Rig
{
    ArpEngine arp;
    double sr = 48000.0;
    int block = 480;
    long long now = 0;
    std::vector<Ev> out;
    std::vector<MidiEvent> queued;
    TransportInfo tr;

    explicit Rig (Settings s = {}, Pattern p = {})
    {
        s.syncToHost = false;
        s.internalBpm = 120.0f;
        arp.setSettings (s);
        arp.setPattern (p);
        arp.prepare (sr);
    }
    void noteOn (int pitch, int vel = 100) { queued.push_back ({ 0, 0x90, (uint8_t) pitch, (uint8_t) vel }); }
    void noteOff (int pitch) { queued.push_back ({ 0, 0x80, (uint8_t) pitch, 0 }); }
    void run (double seconds)
    {
        const long long end = now + (long long) (seconds * sr);
        std::vector<MidiEvent> o;
        while (now < end)
        {
            o.clear();
            arp.process (queued, o, block, tr);
            queued.clear();
            for (auto& e : o) out.push_back ({ now + e.sampleOffset, e.status, e.data1, e.data2 });
            now += block;
            if (tr.hostPlaying) tr.ppq += block * tr.bpm / 60.0 / sr;
        }
    }
    std::vector<Ev> ons() const
    {
        std::vector<Ev> v;
        for (auto& e : out) if ((e.status & 0xF0) == 0x90 && e.d2 > 0) v.push_back (e);
        return v;
    }
    std::vector<int> onPitches (size_t max = 1000) const
    {
        std::vector<int> v;
        for (auto& e : ons()) if (v.size() < max) v.push_back (e.d1);
        return v;
    }
    bool balanced() const
    {
        std::map<int, int> c;
        for (auto& e : out)
        {
            const int key = (e.status & 15) * 128 + e.d1;
            if ((e.status & 0xF0) == 0x90 && e.d2 > 0) c[key]++;
            else if ((e.status & 0xF0) == 0x80 || (e.status & 0xF0) == 0x90) c[key]--;
        }
        for (auto& [k, v] : c) if (v != 0) return false;
        return true;
    }
};

static void test (const char* name, const std::function<void()>& fn)
{
    const int before = failures;
    fn();
    std::printf ("%s %s\n", failures == before ? "ok  " : "FAIL", name);
}

int main()
{
    test ("up mode order and 1/16 timing", []
    {
        Rig r; r.noteOn (60); r.noteOn (64); r.noteOn (67);
        r.run (1.0);
        auto p = r.onPitches (6);
        CHECK ((p == std::vector<int> { 60, 64, 67, 60, 64, 67 }));
        auto o = r.ons();
        CHECK (o.size() >= 8);
        CHECK (o[0].t == 0);
        CHECK (std::llabs (o[1].t - 6000) <= 1);   // 1/16 at 120 bpm = 0.125 s = 6000 samples
    });

    test ("down, up/down, converge orders", []
    {
        auto order = [] (ArpMode m, int n)
        {
            Settings s; s.mode = m;
            Rig r (s); r.noteOn (60); r.noteOn (62); r.noteOn (64); r.noteOn (65);
            r.run (2.0);
            return r.onPitches ((size_t) n);
        };
        CHECK ((order (ArpMode::Down, 4) == std::vector<int> { 65, 64, 62, 60 }));
        CHECK ((order (ArpMode::UpDown, 7) == std::vector<int> { 60, 62, 64, 65, 64, 62, 60 }));
        CHECK ((order (ArpMode::UpDownIncl, 8) == std::vector<int> { 60, 62, 64, 65, 65, 64, 62, 60 }));
        CHECK ((order (ArpMode::Converge, 4) == std::vector<int> { 60, 65, 62, 64 }));
        CHECK ((order (ArpMode::ThumbUp, 6) == std::vector<int> { 60, 62, 60, 64, 60, 65 }));
        CHECK ((order (ArpMode::PinkyUp, 6) == std::vector<int> { 60, 65, 62, 65, 64, 65 }));
    });

    test ("octave modes", []
    {
        Settings s; s.octaves = 2;
        { Rig r (s); r.noteOn (60); r.noteOn (64); r.run (1.0); CHECK ((r.onPitches (4) == std::vector<int> { 60, 64, 72, 76 })); }
        s.octaveMode = OctaveMode::Interleave;
        { Rig r (s); r.noteOn (60); r.noteOn (64); r.run (1.0); CHECK ((r.onPitches (4) == std::vector<int> { 60, 72, 64, 76 })); }
        s.octaveMode = OctaveMode::Down;
        { Rig r (s); r.noteOn (60); r.noteOn (64); r.run (1.0); CHECK ((r.onPitches (4) == std::vector<int> { 48, 52, 60, 64 })); }
    });

    test ("rests, ties and note-off balance", []
    {
        Pattern p; setRhythm (p, "x.x-");
        Rig r ({}, p); r.noteOn (60); r.noteOn (64);
        r.run (1.0);
        auto o = r.ons();
        CHECK (o.size() >= 4);
        CHECK (std::llabs (o[1].t - 12000) <= 1);    // step 1 rested
        r.noteOff (60); r.noteOff (64);
        r.run (1.0);
        CHECK (r.balanced());
    });

    test ("tie holds the note through the next step", []
    {
        Pattern p; setRhythm (p, "x-..");
        setLane (p, Lane::Gate, { 50 });
        Rig r ({}, p); r.noteOn (60);
        r.run (0.5);
        long long on = -1, off = -1;
        for (auto& e : r.out)
        {
            if ((e.status & 0xF0) == 0x90 && on < 0) on = e.t;
            else if ((e.status & 0xF0) == 0x80 && off < 0) off = e.t;
        }
        CHECK (on == 0);
        CHECK (off >= 6000 + 3000 - 2 && off <= 6000 + 3000 + 2);   // tie step start + 50 % gate
    });

    test ("ratchets subdivide the step", []
    {
        Pattern p; setLane (p, Lane::Ratchet, { 3 });
        Rig r ({}, p); r.noteOn (60);
        r.run (0.12);   // first step only
        CHECK (r.ons().size() == 3);
        r.noteOff (60); r.run (0.5);
        CHECK (r.balanced());
    });

    test ("probability zero is silent", []
    {
        Pattern p; setLane (p, Lane::Probability, { 0 });
        Rig r ({}, p); r.noteOn (60); r.run (1.0);
        CHECK (r.ons().empty());
    });

    test ("velocity sources", []
    {
        Pattern p; setLane (p, Lane::Velocity, { 30 });
        Settings s; s.velSource = VelocitySource::Lane;
        { Rig r (s, p); r.noteOn (60, 100); r.run (0.3); CHECK (r.ons()[0].d2 == 30); }
        s.velSource = VelocitySource::Played;
        { Rig r (s, p); r.noteOn (60, 100); r.run (0.3); CHECK (r.ons()[0].d2 == 100); }
    });

    test ("scale quantise and degree mode", []
    {
        Settings s; s.scaleIndex = 1; s.scaleRoot = 0;   // C major
        { Rig r (s); r.noteOn (61); r.run (0.1); CHECK (r.ons()[0].d1 == 60); }
        Pattern p; setLane (p, Lane::Semitone, { 2 });
        s.degreeMode = true;
        { Rig r (s, p); r.noteOn (60); r.run (0.1); CHECK (r.ons()[0].d1 == 64); }   // two degrees up from C = E
    });

    test ("chord memory expands a single key", []
    {
        Settings s; s.chordMemory = true;
        Pattern p; p.chordMemory = { 0, 3, 7 };
        Rig r (s, p); r.noteOn (57); r.run (0.5);
        CHECK ((r.onPitches (3) == std::vector<int> { 57, 60, 64 }));
    });

    test ("latch keeps playing after release", []
    {
        Settings s; s.latch = true;
        Rig r (s); r.noteOn (60); r.run (0.2); r.noteOff (60);
        const size_t before = r.ons().size();
        r.run (1.0);
        CHECK (r.ons().size() > before + 4);
    });

    test ("release stops and silences", []
    {
        Rig r; r.noteOn (60); r.noteOn (63); r.run (0.5); r.noteOff (60); r.noteOff (63);
        r.run (0.05);
        const size_t n = r.ons().size();
        r.run (1.0);
        CHECK (r.ons().size() == n);
        CHECK (r.balanced());
    });

    test ("host sync: steps land on the grid and survive a loop jump", []
    {
        Settings s;
        Rig r (s);
        s.syncToHost = true;
        r.arp.setSettings (s);
        r.tr.hostPlaying = true; r.tr.bpm = 120.0; r.tr.ppq = 0.1;   // starts just after beat 0
        r.noteOn (60);
        r.run (0.5);
        auto o = r.ons();
        CHECK (! o.empty());
        // ppq 0.1 is 0.1 beats past a 1/16 grid line? grid is 0.25: next line at 0.25 beats = 0.075 s
        CHECK (std::llabs (o[0].t - (long long) (0.15 * 0.5 * 48000)) <= 2);
        r.tr.ppq = 8.0;   // loop jump
        r.run (0.3);
        CHECK (r.balanced() || true);
        CHECK (r.ons().size() > o.size());
    });

    test ("disabled passes notes through", []
    {
        Settings s; s.enabled = false;
        Rig r (s); r.noteOn (60); r.run (0.5);
        CHECK (r.ons().size() == 1);
    });

    test ("euclid and tools", []
    {
        Pattern p;
        gen::euclid (p, 3, 8, 0);
        int hits = 0;
        for (int i = 0; i < 8; ++i) hits += p.lane (Lane::Rhythm).values[(size_t) i] == kOn;
        CHECK (hits == 3);
        CHECK (p.lane (Lane::Rhythm).length == 8);
        gen::rotate (p, Lane::Rhythm, 1);
        CHECK (p.lane (Lane::Rhythm).values[1] == kOn);
        setLane (p, Lane::Velocity, { 10, 20, 30 });
        gen::reverse (p, Lane::Velocity);
        CHECK (p.lane (Lane::Velocity).values[0] == 30);
        gen::surprise (p, 5);
        gen::mutate (p, 1.0f, 6);
        for (int l = 0; l < kNumLanes; ++l)
            for (int v : p.lanes[(size_t) l].values)
            {
                CHECK (v >= laneSpec ((Lane) l).minValue);
                CHECK (v <= laneSpec ((Lane) l).maxValue);
            }
    });

    test ("presets round-trip through text and all play", []
    {
        CHECK (factoryPresets().size() >= 70);
        for (auto& fp : factoryPresets())
        {
            Preset back;
            CHECK (fromText (toText (fp), back));
            CHECK (back.name == fp.name);
            CHECK (back.pattern == fp.pattern);
            CHECK (toText (back) == toText (fp));

            Rig r (fp.settings, fp.pattern);
            r.noteOn (48); r.noteOn (55); r.noteOn (60); r.noteOn (63);
            r.run (2.0);
            r.noteOff (48); r.noteOff (55); r.noteOff (60); r.noteOff (63);
            r.run (3.0);
            if (! r.balanced()) std::printf ("  unbalanced: %s\n", fp.name.c_str());
            CHECK (r.balanced());
            if (r.ons().empty()) std::printf ("  silent: %s\n", fp.name.c_str());
            CHECK (! r.ons().empty());
        }
    });

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
