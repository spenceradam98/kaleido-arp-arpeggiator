#include "ArpTypes.h"

namespace kal
{

const LaneSpec& laneSpec (Lane lane)
{
    static const LaneSpec specs[kNumLanes] = {
        { "Rhythm",      0,   2,   1 },
        { "Velocity",    1, 127, 100 },
        { "Gate",        5, 200,  60 },
        { "Octave",     -3,   3,   0 },
        { "Semitone",  -12,  12,   0 },
        { "Probability", 0, 100, 100 },
        { "Ratchet",     1,   8,   1 },
    };
    return specs[(int) lane];
}

const std::vector<Rate>& rates()
{
    static const std::vector<Rate> r = {
        { "1/1", 4.0 },          { "1/2D", 3.0 },        { "1/2", 2.0 },        { "1/2T", 4.0 / 3.0 },
        { "1/4D", 1.5 },         { "1/4", 1.0 },         { "1/4T", 2.0 / 3.0 }, { "1/8D", 0.75 },
        { "1/8", 0.5 },          { "1/8T", 1.0 / 3.0 },  { "1/16D", 0.375 },    { "1/16", 0.25 },
        { "1/16 x5", 0.2 },      { "1/16T", 1.0 / 6.0 }, { "1/16 x7", 1.0 / 7.0 },
        { "1/32", 0.125 },       { "1/32T", 1.0 / 12.0 }, { "1/64", 0.0625 },
    };
    return r;
}

int defaultRateIndex() { return 11; }

static constexpr uint16_t m (std::initializer_list<int> degrees)
{
    uint16_t v = 0;
    for (int d : degrees) v = (uint16_t) (v | (1u << d));
    return v;
}

const std::vector<Scale>& scales()
{
    static const std::vector<Scale> s = {
        { "Chromatic",       0x0FFF },
        { "Major",           m ({ 0, 2, 4, 5, 7, 9, 11 }) },
        { "Minor",           m ({ 0, 2, 3, 5, 7, 8, 10 }) },
        { "Harmonic Minor",  m ({ 0, 2, 3, 5, 7, 8, 11 }) },
        { "Melodic Minor",   m ({ 0, 2, 3, 5, 7, 9, 11 }) },
        { "Dorian",          m ({ 0, 2, 3, 5, 7, 9, 10 }) },
        { "Phrygian",        m ({ 0, 1, 3, 5, 7, 8, 10 }) },
        { "Lydian",          m ({ 0, 2, 4, 6, 7, 9, 11 }) },
        { "Mixolydian",      m ({ 0, 2, 4, 5, 7, 9, 10 }) },
        { "Locrian",         m ({ 0, 1, 3, 5, 6, 8, 10 }) },
        { "Major Pentatonic",m ({ 0, 2, 4, 7, 9 }) },
        { "Minor Pentatonic",m ({ 0, 3, 5, 7, 10 }) },
        { "Blues",           m ({ 0, 3, 5, 6, 7, 10 }) },
        { "Whole Tone",      m ({ 0, 2, 4, 6, 8, 10 }) },
        { "Diminished",      m ({ 0, 2, 3, 5, 6, 8, 9, 11 }) },
        { "Hungarian Minor", m ({ 0, 2, 3, 6, 7, 8, 11 }) },
        { "Double Harmonic", m ({ 0, 1, 4, 5, 7, 8, 11 }) },
        { "In (Japanese)",   m ({ 0, 1, 5, 7, 8 }) },
        { "Hirajoshi",       m ({ 0, 2, 3, 7, 8 }) },
        { "Prometheus",      m ({ 0, 2, 4, 6, 9, 10 }) },
    };
    return s;
}

const char* modeName (ArpMode mode)
{
    static const char* n[] = { "Up", "Down", "Up/Down", "Down/Up", "Up/Down Incl", "Down/Up Incl",
                               "Converge", "Diverge", "Con/Diverge", "Thumb", "Pinky", "As Played",
                               "Random", "Random Walk", "Shuffle", "Chord", "Strum Up", "Strum Down" };
    return n[(int) mode];
}

const char* octaveModeName (OctaveMode o)
{
    static const char* n[] = { "Up", "Down", "Up/Down", "Interleave", "Random" };
    return n[(int) o];
}

const char* velocitySourceName (VelocitySource v)
{
    static const char* n[] = { "Played", "Lane", "Mix" };
    return n[(int) v];
}

const char* noteName (int pc)
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n[((pc % 12) + 12) % 12];
}

Pattern::Pattern()
{
    for (int i = 0; i < kNumLanes; ++i)
        resetLane ((Lane) i);
}

void Pattern::resetLane (Lane l)
{
    auto& ld = lane (l);
    ld.length = 16;
    ld.values.fill (laneSpec (l).defaultValue);
}

int Pattern::value (Lane l, int step) const
{
    const auto& ld = lane (l);
    const int len = ld.length < 1 ? 1 : ld.length;
    return ld.values[(size_t) (((step % len) + len) % len)];
}

bool Pattern::operator== (const Pattern& o) const
{
    for (int i = 0; i < kNumLanes; ++i)
        if (lanes[(size_t) i].length != o.lanes[(size_t) i].length || lanes[(size_t) i].values != o.lanes[(size_t) i].values)
            return false;
    return chordMemory == o.chordMemory;
}

} // namespace kal
