#include "Presets.h"

#include "Generators.h"

#include <algorithm>
#include <functional>
#include <sstream>

namespace kal
{

// ---------------------------------------------------------------------------------------------
// Text format

namespace
{
    std::string joinInts (const int* v, int n)
    {
        std::string s;
        for (int i = 0; i < n; ++i) { if (i) s += ','; s += std::to_string (v[i]); }
        return s;
    }

    std::vector<int> splitInts (const std::string& s)
    {
        std::vector<int> v;
        std::stringstream ss (s);
        std::string item;
        while (std::getline (ss, item, ','))
            if (! item.empty())
            {
                try { v.push_back (std::stoi (item)); } catch (...) {}
            }
        return v;
    }

    template <typename Enum, typename NameFn>
    bool enumFromName (const std::string& name, int count, NameFn fn, Enum& out)
    {
        for (int i = 0; i < count; ++i)
            if (name == fn ((Enum) i)) { out = (Enum) i; return true; }
        return false;
    }

    std::string trim (std::string s)
    {
        while (! s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
        return s.substr (i);
    }
}

std::string toText (const Preset& p)
{
    const auto& s = p.settings;
    std::ostringstream o;
    o << "kaleido-preset=1\n";
    o << "name=" << p.name << "\n";
    o << "category=" << p.category << "\n";
    o << "enabled=" << s.enabled << "\n";
    o << "mode=" << modeName (s.mode) << "\n";
    o << "octaves=" << s.octaves << "\n";
    o << "octaveMode=" << octaveModeName (s.octaveMode) << "\n";
    o << "rate=" << rates()[(size_t) std::clamp (s.rateIndex, 0, (int) rates().size() - 1)].name << "\n";
    o << "swing=" << s.swing << "\n";
    o << "gateScale=" << s.gateScale << "\n";
    o << "velSource=" << velocitySourceName (s.velSource) << "\n";
    o << "latch=" << s.latch << "\n";
    o << "retrigger=" << s.retrigger << "\n";
    o << "advanceOnRest=" << s.advanceOnRest << "\n";
    o << "noteRepeat=" << s.noteRepeat << "\n";
    o << "transpose=" << s.transpose << "\n";
    o << "scaleRoot=" << s.scaleRoot << "\n";
    o << "scale=" << scales()[(size_t) std::clamp (s.scaleIndex, 0, (int) scales().size() - 1)].name << "\n";
    o << "degreeMode=" << s.degreeMode << "\n";
    o << "chordMemory=" << s.chordMemory << "\n";
    o << "strumMs=" << s.strumMs << "\n";
    o << "humaniseMs=" << s.humaniseMs << "\n";
    o << "humaniseVel=" << s.humaniseVel << "\n";
    o << "rangeLow=" << s.rangeLow << "\n";
    o << "rangeHigh=" << s.rangeHigh << "\n";
    o << "outChannel=" << s.outChannel << "\n";
    o << "syncToHost=" << s.syncToHost << "\n";
    o << "internalBpm=" << s.internalBpm << "\n";
    o << "seed=" << s.seed << "\n";
    o << "chord=" << joinInts (p.pattern.chordMemory.data(), (int) p.pattern.chordMemory.size()) << "\n";
    for (int i = 0; i < kNumLanes; ++i)
    {
        const auto& ld = p.pattern.lanes[(size_t) i];
        o << "lane." << laneSpec ((Lane) i).name << "=" << ld.length << ":" << joinInts (ld.values.data(), kMaxSteps) << "\n";
    }
    return o.str();
}

bool fromText (const std::string& text, Preset& out)
{
    Preset p;
    auto& s = p.settings;
    bool sawHeader = false;
    std::istringstream in (text);
    std::string line;
    while (std::getline (in, line))
    {
        line = trim (line);
        const auto eq = line.find ('=');
        if (line.empty() || line[0] == '#' || eq == std::string::npos) continue;
        const std::string k = line.substr (0, eq), v = line.substr (eq + 1);
        auto i = [&] { try { return std::stoi (v); } catch (...) { return 0; } };
        auto f = [&] { try { return std::stof (v); } catch (...) { return 0.0f; } };
        auto b = [&] { return v == "1" || v == "true"; };

        if (k == "kaleido-preset") sawHeader = true;
        else if (k == "name") p.name = v;
        else if (k == "category") p.category = v;
        else if (k == "enabled") s.enabled = b();
        else if (k == "mode") enumFromName (v, (int) ArpMode::Count, modeName, s.mode);
        else if (k == "octaves") s.octaves = std::clamp (i(), 1, 4);
        else if (k == "octaveMode") enumFromName (v, (int) OctaveMode::Count, octaveModeName, s.octaveMode);
        else if (k == "rate")
        {
            for (size_t r = 0; r < rates().size(); ++r)
                if (v == rates()[r].name) s.rateIndex = (int) r;
        }
        else if (k == "swing") s.swing = std::clamp (f(), 0.0f, 1.0f);
        else if (k == "gateScale") s.gateScale = std::clamp (f(), 0.1f, 2.0f);
        else if (k == "velSource") enumFromName (v, (int) VelocitySource::Count, velocitySourceName, s.velSource);
        else if (k == "latch") s.latch = b();
        else if (k == "retrigger") s.retrigger = b();
        else if (k == "advanceOnRest") s.advanceOnRest = b();
        else if (k == "noteRepeat") s.noteRepeat = std::clamp (i(), 1, 8);
        else if (k == "transpose") s.transpose = std::clamp (i(), -24, 24);
        else if (k == "scaleRoot") s.scaleRoot = std::clamp (i(), 0, 11);
        else if (k == "scale")
        {
            for (size_t r = 0; r < scales().size(); ++r)
                if (v == scales()[r].name) s.scaleIndex = (int) r;
        }
        else if (k == "degreeMode") s.degreeMode = b();
        else if (k == "chordMemory") s.chordMemory = b();
        else if (k == "strumMs") s.strumMs = std::clamp (f(), 0.0f, 200.0f);
        else if (k == "humaniseMs") s.humaniseMs = std::clamp (f(), 0.0f, 40.0f);
        else if (k == "humaniseVel") s.humaniseVel = std::clamp (i(), 0, 40);
        else if (k == "rangeLow") s.rangeLow = std::clamp (i(), 0, 127);
        else if (k == "rangeHigh") s.rangeHigh = std::clamp (i(), 0, 127);
        else if (k == "outChannel") s.outChannel = std::clamp (i(), 0, 16);
        else if (k == "syncToHost") s.syncToHost = b();
        else if (k == "internalBpm") s.internalBpm = std::clamp (f(), 20.0f, 400.0f);
        else if (k == "seed") { try { s.seed = (uint32_t) std::stoul (v); } catch (...) {} }
        else if (k == "chord")
        {
            p.pattern.chordMemory = splitInts (v);
            if (p.pattern.chordMemory.size() > 12) p.pattern.chordMemory.resize (12);
        }
        else if (k.rfind ("lane.", 0) == 0)
        {
            const std::string laneName = k.substr (5);
            for (int li = 0; li < kNumLanes; ++li)
            {
                if (laneName != laneSpec ((Lane) li).name) continue;
                const auto colon = v.find (':');
                if (colon == std::string::npos) break;
                auto& ld = p.pattern.lanes[(size_t) li];
                try { ld.length = std::clamp (std::stoi (v.substr (0, colon)), 1, kMaxSteps); } catch (...) {}
                const auto vals = splitInts (v.substr (colon + 1));
                const auto& spec = laneSpec ((Lane) li);
                for (size_t n = 0; n < vals.size() && n < (size_t) kMaxSteps; ++n)
                    ld.values[n] = std::clamp (vals[n], spec.minValue, spec.maxValue);
            }
        }
    }
    if (! sawHeader) return false;
    out = p;
    return true;
}

void setRhythm (Pattern& p, const std::string& s)
{
    auto& r = p.lane (Lane::Rhythm);
    const int n = std::clamp ((int) s.size(), 1, kMaxSteps);
    for (int i = 0; i < n; ++i)
        r.values[(size_t) i] = s[(size_t) i] == 'x' ? kOn : s[(size_t) i] == '-' ? kTie : kRest;
    r.length = n;
}

void setLane (Pattern& p, Lane lane, const std::vector<int>& values)
{
    auto& ld = p.lane (lane);
    const auto& spec = laneSpec (lane);
    const int n = std::clamp ((int) values.size(), 1, kMaxSteps);
    for (int i = 0; i < n; ++i) ld.values[(size_t) i] = std::clamp (values[(size_t) i], spec.minValue, spec.maxValue);
    ld.length = n;
}

// ---------------------------------------------------------------------------------------------
// Factory library

namespace
{
    int rateIdx (const char* name)
    {
        for (size_t i = 0; i < rates().size(); ++i)
            if (std::string (name) == rates()[i].name) return (int) i;
        return defaultRateIndex();
    }

    int scaleIdx (const char* name)
    {
        for (size_t i = 0; i < scales().size(); ++i)
            if (std::string (name) == scales()[i].name) return (int) i;
        return 0;
    }

    using Fn = std::function<void (Settings&, Pattern&)>;

    std::vector<Preset> build()
    {
        std::vector<Preset> list;
        auto add = [&] (const char* cat, const char* name, Fn fn)
        {
            Preset p;
            p.name = name;
            p.category = cat;
            fn (p.settings, p.pattern);
            list.push_back (p);
        };
        auto vel = [] (Pattern& p, std::vector<int> v) { setLane (p, Lane::Velocity, v); };
        auto gate = [] (Pattern& p, std::vector<int> v) { setLane (p, Lane::Gate, v); };
        auto oct = [] (Pattern& p, std::vector<int> v) { setLane (p, Lane::Octave, v); };
        auto semi = [] (Pattern& p, std::vector<int> v) { setLane (p, Lane::Semitone, v); };
        auto prob = [] (Pattern& p, std::vector<int> v) { setLane (p, Lane::Probability, v); };
        auto rat = [] (Pattern& p, std::vector<int> v) { setLane (p, Lane::Ratchet, v); };

        // --- Init --------------------------------------------------------------------------
        add ("Init", "Init", [] (Settings&, Pattern&) {});

        // --- Classic -----------------------------------------------------------------------
        add ("Classic", "Straight Up 16ths", [] (Settings& s, Pattern&) { s.mode = ArpMode::Up; });
        add ("Classic", "Straight Down 16ths", [] (Settings& s, Pattern&) { s.mode = ArpMode::Down; });
        add ("Classic", "Up/Down Two Octaves", [] (Settings& s, Pattern&) { s.mode = ArpMode::UpDown; s.octaves = 2; });
        add ("Classic", "Eighth Note Climb", [] (Settings& s, Pattern&) { s.mode = ArpMode::Up; s.octaves = 3; s.rateIndex = rateIdx ("1/8"); });
        add ("Classic", "Triplet Cascade", [] (Settings& s, Pattern&) { s.mode = ArpMode::Down; s.octaves = 2; s.rateIndex = rateIdx ("1/8T"); });
        add ("Classic", "As Played Legato", [&] (Settings& s, Pattern& p) { s.mode = ArpMode::AsPlayed; gate (p, { 110 }); });
        add ("Classic", "Thumb Pedal", [] (Settings& s, Pattern&) { s.mode = ArpMode::ThumbUp; s.octaves = 2; });
        add ("Classic", "Pinky Pedal", [] (Settings& s, Pattern&) { s.mode = ArpMode::PinkyUp; });
        add ("Classic", "Converge", [] (Settings& s, Pattern&) { s.mode = ArpMode::Converge; s.octaves = 2; });
        add ("Classic", "Con/Diverge", [] (Settings& s, Pattern&) { s.mode = ArpMode::ConDiverge; s.octaves = 2; });
        add ("Classic", "Interleaved Octaves", [] (Settings& s, Pattern&) { s.mode = ArpMode::Up; s.octaves = 2; s.octaveMode = OctaveMode::Interleave; });
        add ("Classic", "Staccato Sixteens", [&] (Settings& s, Pattern& p) { s.mode = ArpMode::UpDownIncl; gate (p, { 22 }); });
        add ("Classic", "Shuffle Swing", [] (Settings& s, Pattern&) { s.mode = ArpMode::Up; s.swing = 0.55f; s.octaves = 2; });

        // --- Trance & EDM ------------------------------------------------------------------
        add ("Trance & EDM", "Gated Trance", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.octaves = 2; s.velSource = VelocitySource::Lane;
            setRhythm (p, "xxxxxxxxxxxxxxxx");
            gate (p, { 45, 30, 45, 30 });
            vel (p, { 127, 80, 100, 80 });
        });
        add ("Trance & EDM", "Offbeat Pluck", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.rateIndex = rateIdx ("1/16");
            setRhythm (p, ".x.x.x.x.x.x.x.x"); gate (p, { 25 });
        });
        add ("Trance & EDM", "Supersaw Run", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::UpDown; s.octaves = 3; s.velSource = VelocitySource::Lane;
            gen::accents (p, 4, 0, 85, 125); gate (p, { 50 });
        });
        add ("Trance & EDM", "Ratchet Riser", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.octaves = 2; s.rateIndex = rateIdx ("1/8");
            rat (p, { 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 2, 2, 3, 3, 4, 4 });
        });
        add ("Trance & EDM", "Big Room Stabs", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Chord; s.rateIndex = rateIdx ("1/16");
            setRhythm (p, "x..x..x...x..x.."); gate (p, { 35 });
        });
        add ("Trance & EDM", "Psy Rolling", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.velSource = VelocitySource::Lane;
            setRhythm (p, ".xxx.xxx.xxx.xxx"); gate (p, { 40 }); vel (p, { 127, 90, 100, 90 });
            oct (p, { 0, 0, 1, 0 });
        });
        add ("Trance & EDM", "Pumping Octaves", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.rateIndex = rateIdx ("1/8"); s.velSource = VelocitySource::Lane;
            oct (p, { 0, 1 }); vel (p, { 90, 120 }); gate (p, { 70, 40 });
        });

        // --- Synthwave ---------------------------------------------------------------------
        add ("Synthwave", "Night Drive", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.octaves = 2; s.velSource = VelocitySource::Lane;
            vel (p, { 120, 80, 95, 80 }); gate (p, { 55 });
        });
        add ("Synthwave", "Neon Octave Bass", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.rateIndex = rateIdx ("1/8"); s.rangeHigh = 60;
            oct (p, { 0, 1 }); gate (p, { 60, 35 });
        });
        add ("Synthwave", "Chrome Gallop", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; setRhythm (p, "x.xxx.xxx.xxx.xx"); gate (p, { 45 });
        });
        add ("Synthwave", "Sunset Dotted", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::UpDown; s.octaves = 2; s.rateIndex = rateIdx ("1/8D"); gate (p, { 85 });
        });
        add ("Synthwave", "Arcade Runner", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.octaves = 3; s.rateIndex = rateIdx ("1/32"); gate (p, { 50 });
        });

        // --- Ambient & Pads ----------------------------------------------------------------
        add ("Ambient & Pads", "Slow Bloom", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::UpDownIncl; s.octaves = 2; s.rateIndex = rateIdx ("1/4"); s.humaniseMs = 12; s.humaniseVel = 15;
            gate (p, { 180 });
        });
        add ("Ambient & Pads", "Glass Rain", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Random; s.octaves = 3; s.rateIndex = rateIdx ("1/8"); s.velSource = VelocitySource::Lane;
            prob (p, { 80, 50, 70, 40, 90, 50, 60, 35 }); vel (p, { 70, 50, 90, 60, 45, 80 }); gate (p, { 140 });
            s.humaniseMs = 15;
        });
        add ("Ambient & Pads", "Tide Pools", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::RandomWalk; s.octaves = 2; s.rateIndex = rateIdx ("1/8T"); gate (p, { 160 }); s.humaniseVel = 20;
        });
        add ("Ambient & Pads", "Harp Sweep", [] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::StrumUp; s.strumMs = 60; s.octaves = 2; s.octaveMode = OctaveMode::UpDown;
            s.rateIndex = rateIdx ("1/2"); setRhythm (p, "x");
        });
        add ("Ambient & Pads", "Drifting Fifths", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.chordMemory = true; p.chordMemory = { 0, 7, 12 }; s.rateIndex = rateIdx ("1/8");
            gate (p, { 150 }); s.humaniseMs = 10;
        });
        add ("Ambient & Pads", "Music Box", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::UpDown; s.octaves = 2; s.transpose = 12; s.rateIndex = rateIdx ("1/8");
            s.velSource = VelocitySource::Lane; vel (p, { 100, 70, 85, 70, 60, 70 }); gate (p, { 60 });
        });

        // --- Bass --------------------------------------------------------------------------
        add ("Bass", "Rolling Bass", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.rangeHigh = 55; s.rangeLow = 24;
            setRhythm (p, ".xxx.xxx.xxx.xxx"); gate (p, { 50 });
        });
        add ("Bass", "Octave Bounce", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::ThumbUp; s.rateIndex = rateIdx ("1/16"); oct (p, { -1, 0, -1, 0 }); gate (p, { 45 });
        });
        add ("Bass", "Acid Line", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.velSource = VelocitySource::Lane;
            setRhythm (p, "xx-x.xx.x-xx.x.x");
            semi (p, { 0, 0, 12, 0, 0, 3, 0, 7, 0, 0, 10, 0, 0, 12, 0, 5 });
            vel (p, { 127, 90, 90, 110, 80, 127, 90, 100 });
            gate (p, { 55 });
            s.scaleIndex = scaleIdx ("Minor");
        });
        add ("Bass", "Funk Sixteenths", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::AsPlayed; s.swing = 0.3f; setRhythm (p, "x.xx.x.xx..x.x.x"); oct (p, { 0, 0, 1, 0, 0, 0, -1, 0 }); gate (p, { 40 });
        });
        add ("Bass", "Sub Pulse", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.rateIndex = rateIdx ("1/8"); s.rangeHigh = 48; s.rangeLow = 24; gate (p, { 80 });
        });

        // --- Polymeter ---------------------------------------------------------------------
        add ("Polymeter", "3 Against 4", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.octaves = 2; s.velSource = VelocitySource::Lane;
            vel (p, { 127, 80, 80 }); oct (p, { 0, 0, 0, 1 });
        });
        add ("Polymeter", "Five Over Sixteen", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::UpDown; s.velSource = VelocitySource::Lane;
            setRhythm (p, "x.xx."); vel (p, { 120, 90, 100, 90 }); gate (p, { 40, 70, 40 });
        });
        add ("Polymeter", "Seven Phase", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.octaves = 2;
            setRhythm (p, "xx.x.xx"); oct (p, { 0, 1, 0, 0, 1 }); gate (p, { 30, 60, 90 });
        });
        add ("Polymeter", "Quintuplet Drift", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.rateIndex = rateIdx ("1/16 x5"); s.velSource = VelocitySource::Lane;
            vel (p, { 120, 70, 85, 70, 95 }); oct (p, { 0, 0, 1 });
        });
        add ("Polymeter", "Septuplet Spiral", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::ConDiverge; s.octaves = 2; s.rateIndex = rateIdx ("1/16 x7"); s.velSource = VelocitySource::Lane;
            vel (p, { 125, 75, 90, 75, 100, 75, 85 }); gate (p, { 45 });
        });
        add ("Polymeter", "Shifting Accents", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.velSource = VelocitySource::Lane;
            vel (p, { 127, 70, 70, 127, 70, 70, 127, 70, 70, 70, 127 }); gate (p, { 30, 60, 45, 80, 30 });
            oct (p, { 0, 0, 0, 1, 0, 0, -1 });
        });
        add ("Polymeter", "Clockwork", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.octaves = 2;
            setRhythm (p, "xxx.xx.x.xxx."); rat (p, { 1, 1, 2, 1, 1, 1, 1, 1, 1, 3, 1 }); gate (p, { 40 });
        });

        // --- Euclidean ---------------------------------------------------------------------
        const struct { const char* name; int hits, steps, rot; ArpMode mode; } euclids[] = {
            { "Tresillo (3/8)", 3, 8, 0, ArpMode::Up },
            { "Cinquillo (5/8)", 5, 8, 0, ArpMode::UpDown },
            { "Bossa (5/16)", 5, 16, 0, ArpMode::Up },
            { "West African Bell (7/12)", 7, 12, 0, ArpMode::Converge },
            { "Aksak (4/9)", 4, 9, 0, ArpMode::Up },
            { "Ruchenitza (4/7)", 4, 7, 0, ArpMode::Down },
            { "Bembe (7/16)", 7, 16, 2, ArpMode::AsPlayed },
            { "Samba (9/16)", 9, 16, 0, ArpMode::UpDown },
        };
        for (auto& e : euclids)
        {
            const auto ec = e;
            add ("Euclidean", e.name, [ec, &vel] (Settings& s, Pattern& p)
            {
                s.mode = ec.mode; s.octaves = 2; s.velSource = VelocitySource::Mix;
                gen::euclid (p, ec.hits, ec.steps, ec.rot);
                vel (p, { 127, 100, 110, 100 });
            });
        }

        // --- Chords & Strums ---------------------------------------------------------------
        add ("Chords & Strums", "Piano Comp", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Chord; s.rateIndex = rateIdx ("1/8"); s.swing = 0.35f; s.humaniseMs = 8; s.humaniseVel = 10;
            setRhythm (p, "x.-x..x.x.-x..x."); gate (p, { 70 });
        });
        add ("Chords & Strums", "Guitar Strum D-DU-UDU", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::StrumDown; s.strumMs = 18; s.rateIndex = rateIdx ("1/8"); s.velSource = VelocitySource::Mix;
            setRhythm (p, "x.xx.xxx"); vel (p, { 127, 90, 110, 80, 100, 85, 110, 80 }); gate (p, { 90 });
        });
        add ("Chords & Strums", "Harp Glissando", [] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::StrumUp; s.strumMs = 35; s.octaves = 3; s.octaveMode = OctaveMode::Up;
            s.rateIndex = rateIdx ("1/4"); setRhythm (p, "x");
        });
        add ("Chords & Strums", "Reggae Skank", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Chord; s.rateIndex = rateIdx ("1/8"); setRhythm (p, ".x.x.x.x"); gate (p, { 30 });
        });
        add ("Chords & Strums", "One Finger Minor 7", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.chordMemory = true; p.chordMemory = { 0, 3, 7, 10 }; s.octaves = 2;
        });
        add ("Chords & Strums", "One Finger Major 9", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::UpDown; s.chordMemory = true; p.chordMemory = { 0, 4, 7, 11, 14 }; s.rateIndex = rateIdx ("1/8T");
        });
        add ("Chords & Strums", "Stab House Chords", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Chord; s.chordMemory = true; p.chordMemory = { 0, 3, 7, 10, 14 };
            setRhythm (p, "..x..x....x..x.."); gate (p, { 30 });
        });
        add ("Chords & Strums", "Sus4 Shimmer", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.chordMemory = true; p.chordMemory = { 0, 5, 7, 12 }; s.octaves = 2; s.rateIndex = rateIdx ("1/16T");
            gate (p, { 120 });
        });

        // --- Scale-locked melodic ----------------------------------------------------------
        add ("Melodic", "Dorian Wanderer", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.scaleIndex = scaleIdx ("Dorian"); s.scaleRoot = 2; s.degreeMode = true;
            semi (p, { 0, 2, 1, 3, 2, 4, 3, 1, 0, -1, 0, 2, 4, 5, 4, 2 });
        });
        add ("Melodic", "Pentatonic Sparkle", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Random; s.octaves = 2; s.scaleIndex = scaleIdx ("Major Pentatonic"); s.degreeMode = true;
            semi (p, { 0, 1, 0, 2, 0, 3, 1, 0 }); gate (p, { 50 });
        });
        add ("Melodic", "Phrygian Tension", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::UpDown; s.scaleIndex = scaleIdx ("Phrygian"); s.scaleRoot = 4; s.degreeMode = true;
            semi (p, { 0, 1, 0, -1 }); rat (p, { 1, 1, 1, 2 });
        });
        add ("Melodic", "Japanese Garden", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::RandomWalk; s.octaves = 2; s.scaleIndex = scaleIdx ("In (Japanese)"); s.degreeMode = true;
            s.rateIndex = rateIdx ("1/8"); semi (p, { 0, 1, 2, 1 }); gate (p, { 110 }); s.humaniseMs = 10;
        });
        add ("Melodic", "Blues Turnaround", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.scaleIndex = scaleIdx ("Blues"); s.degreeMode = true; s.swing = 0.6f; s.rateIndex = rateIdx ("1/8");
            semi (p, { 0, 1, 2, 3, 4, 3, 2, 1 });
        });
        add ("Melodic", "Harmonic Minor Run", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.octaves = 2; s.scaleIndex = scaleIdx ("Harmonic Minor"); s.scaleRoot = 9; s.degreeMode = true;
            s.rateIndex = rateIdx ("1/16T"); semi (p, { 0, 1, 2, 1, 0, -1 });
        });
        add ("Melodic", "Whole Tone Dream", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Converge; s.octaves = 2; s.scaleIndex = scaleIdx ("Whole Tone"); s.degreeMode = true;
            semi (p, { 0, 1, 2, 3 }); gate (p, { 130 });
        });

        // --- Rhythmic ----------------------------------------------------------------------
        add ("Rhythmic", "Ratchet Rolls", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.rateIndex = rateIdx ("1/8"); rat (p, { 1, 2, 1, 3, 1, 1, 4, 2 }); gate (p, { 60 });
        });
        add ("Rhythmic", "Ghost Notes", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.velSource = VelocitySource::Lane;
            vel (p, { 127, 35, 40, 90, 35, 110, 40, 35 }); prob (p, { 100, 60, 50, 100, 60, 100, 50, 70 }); gate (p, { 35 });
        });
        add ("Rhythmic", "Stutter Gate", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::AsPlayed; setRhythm (p, "x-x-xxx.x-x-xx.x"); rat (p, { 1, 1, 1, 1, 2, 3, 1, 1 }); gate (p, { 25 });
        });
        add ("Rhythmic", "Dotted Delay Feel", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; setRhythm (p, "x..x..x..x..x..x"); gate (p, { 80 }); s.octaves = 2;
        });
        add ("Rhythmic", "Swung Hats Style", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.swing = 0.65f; s.velSource = VelocitySource::Lane;
            vel (p, { 110, 60, 85, 60 }); gate (p, { 20 }); rat (p, { 1, 1, 1, 1, 1, 1, 1, 2 });
        });
        add ("Rhythmic", "Tied Syncopation", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::UpDown; setRhythm (p, "x-.x-.x-x-.x-.x."); gate (p, { 90 });
        });
        add ("Rhythmic", "Note Repeat Doubles", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Up; s.noteRepeat = 2; s.octaves = 2; gate (p, { 40 });
        });
        add ("Rhythmic", "Note Repeat Triples", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::UpDown; s.noteRepeat = 3; s.rateIndex = rateIdx ("1/16T"); s.velSource = VelocitySource::Lane;
            vel (p, { 120, 80, 80 }); gate (p, { 40 });
        });

        // --- Generative (seeded so they recall the same) -----------------------------------
        const char* genNames[] = { "Seeded Garden", "Fractal Steps", "Weather Station", "Signal Noise",
                                   "Daydream Machine", "Moss", "Constellation", "Night Market" };
        for (int i = 0; i < 8; ++i)
        {
            const uint32_t seed = 1000u + (uint32_t) i * 7919u;
            add ("Generative", genNames[i], [seed, i] (Settings& s, Pattern& p)
            {
                gen::surprise (p, seed);
                s.mode = (ArpMode) ((int) ArpMode::Up + (i % 6));
                s.octaves = 1 + i % 3;
                s.velSource = VelocitySource::Mix;
                s.seed = seed;
                if (i % 2 == 1) { s.scaleIndex = 1 + i % 5; s.degreeMode = true; }
            });
        }
        add ("Generative", "Markov Melody Minor", [] (Settings& s, Pattern& p)
        {
            gen::markovMelody (p, 1.0f, 2.5f, 0.5f, 4242);
            gen::markovRhythm (p, 0.75f, 0.6f, 99);
            s.scaleIndex = scaleIdx ("Minor"); s.degreeMode = true; s.mode = ArpMode::Up;
        });
        add ("Generative", "Markov Sparse Bells", [&] (Settings& s, Pattern& p)
        {
            gen::markovMelody (p, 0.5f, 1.5f, 1.5f, 777);
            gen::markovRhythm (p, 0.4f, 0.35f, 31);
            s.scaleIndex = scaleIdx ("Major Pentatonic"); s.degreeMode = true; s.mode = ArpMode::Random; s.octaves = 2;
            s.transpose = 12; gate (p, { 150 });
        });
        add ("Generative", "Probability Cloud", [&] (Settings& s, Pattern& p)
        {
            s.mode = ArpMode::Shuffle; s.octaves = 3; s.rateIndex = rateIdx ("1/16");
            prob (p, { 90, 30, 60, 20, 80, 40, 50, 25, 95, 30, 55, 20, 75, 45, 35, 20 });
            s.humaniseMs = 6; s.humaniseVel = 18;
        });

        return list;
    }
}

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets = build();
    return presets;
}

} // namespace kal
