// Kaleido arpeggiator: core data types (plain C++17, no JUCE).
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace kal
{

constexpr int kMaxSteps = 32;

enum class ArpMode : int
{
    Up, Down, UpDown, DownUp, UpDownIncl, DownUpIncl,
    Converge, Diverge, ConDiverge,
    ThumbUp, PinkyUp, AsPlayed,
    Random, RandomWalk, Shuffle,
    Chord, StrumUp, StrumDown,
    Count
};

enum class OctaveMode : int { Up, Down, UpDown, Interleave, Random, Count };

enum class VelocitySource : int { Played, Lane, Mix, Count };

// Each lane has its own length: different lengths give polymeters.
enum class Lane : int { Rhythm, Velocity, Gate, Octave, Semitone, Probability, Ratchet, Count };
constexpr int kNumLanes = (int) Lane::Count;

// Rhythm lane values.
enum RhythmValue : int { kRest = 0, kOn = 1, kTie = 2 };

struct LaneSpec { const char* name; int minValue; int maxValue; int defaultValue; };
const LaneSpec& laneSpec (Lane lane);

struct Rate { const char* name; double beats; };   // beats = quarter notes per step
const std::vector<Rate>& rates();
int defaultRateIndex();                             // 1/16

struct Scale { const char* name; uint16_t mask; };   // bit n = semitone n above root
const std::vector<Scale>& scales();

const char* modeName (ArpMode m);
const char* octaveModeName (OctaveMode m);
const char* velocitySourceName (VelocitySource v);
const char* noteName (int pitchClass);

struct LaneData
{
    int length = 16;
    std::array<int, kMaxSteps> values {};
};

// The step pattern: everything that is drawn rather than dialled.
struct Pattern
{
    std::array<LaneData, kNumLanes> lanes;
    std::vector<int> chordMemory;          // intervals in semitones, root (0) included

    Pattern();
    LaneData& lane (Lane l) { return lanes[(size_t) l]; }
    const LaneData& lane (Lane l) const { return lanes[(size_t) l]; }
    int value (Lane l, int step) const;    // wraps by that lane's length
    void resetLane (Lane l);
    bool operator== (const Pattern& o) const;
};

// The dialled settings (mirrored by host-automatable parameters in the plugin).
struct Settings
{
    bool enabled = true;
    ArpMode mode = ArpMode::Up;
    int octaves = 1;                       // 1..4
    OctaveMode octaveMode = OctaveMode::Up;
    int rateIndex = defaultRateIndex();
    float swing = 0.0f;                    // 0..1 (1 = odd steps delayed half a step)
    float gateScale = 1.0f;                // 0.1..2 multiplies the gate lane
    VelocitySource velSource = VelocitySource::Played;
    bool latch = false;
    bool retrigger = true;                 // restart the pattern on a new phrase
    bool advanceOnRest = false;            // note order moves on during rests too
    int noteRepeat = 1;                    // 1..8 plays of each note before moving on
    int transpose = 0;                     // -24..24
    int scaleRoot = 0;                     // 0..11
    int scaleIndex = 0;                    // 0 = chromatic (off)
    bool degreeMode = false;               // semitone lane counts scale degrees
    bool chordMemory = false;
    float strumMs = 30.0f;                 // 0..200, strum modes only
    float humaniseMs = 0.0f;               // 0..40
    int humaniseVel = 0;                   // 0..40
    int rangeLow = 0, rangeHigh = 127;     // out-of-range notes fold by octaves
    int outChannel = 0;                    // 0 = same as input, else 1..16
    bool syncToHost = true;
    float internalBpm = 120.0f;
    uint32_t seed = 1;
};

struct Preset
{
    std::string name;
    std::string category;
    Settings settings;
    Pattern pattern;
};

} // namespace kal
