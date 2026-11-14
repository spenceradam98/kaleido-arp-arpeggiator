// Kaleido arpeggiator engine: sample-accurate MIDI in -> MIDI out. No allocation in process()
// after prepare() for typical use (vectors are reserved up front).
#pragma once

#include "ArpTypes.h"

#include <atomic>
#include <random>

namespace kal
{

struct MidiEvent
{
    int sampleOffset = 0;
    uint8_t status = 0, data1 = 0, data2 = 0;
};

struct TransportInfo
{
    bool hostPlaying = false;
    double ppq = 0.0;        // host position in quarter notes at block start (valid when playing)
    double bpm = 120.0;      // host tempo, or <= 0 when unknown
};

class ArpEngine
{
public:
    ArpEngine();

    void prepare (double sampleRate);
    void reset();                                       // silences everything and forgets held notes

    void setSettings (const Settings& s) { settings = s; }
    void setPattern (const Pattern& p) { pattern = p; }
    const Settings& getSettings() const { return settings; }
    const Pattern& getPattern() const { return pattern; }

    // Input events must be sorted by sampleOffset. Output is appended to `out`, sorted.
    void process (const std::vector<MidiEvent>& in, std::vector<MidiEvent>& out,
                  int numSamples, const TransportInfo& transport);

    // Captures the currently held notes as the chord-memory shape (intervals from lowest note).
    std::vector<int> heldChordShape() const;

    // UI feedback (safe to read from another thread).
    std::atomic<int> uiStepCounter { -1 };              // step counter of the last fired step
    std::atomic<int> uiLastNote { -1 };
    std::atomic<int> uiHeldCount { 0 };

private:
    struct Held { int pitch, velocity, channel, order; };
    struct Pending { double beat; uint8_t status, data1, data2; uint32_t serial; };
    struct Group { int notes[16]; int vel[16]; int count = 0; };

    void processSegment (double startBeat, double endBeat, std::vector<MidiEvent>& out, int numSamples);
    void handleInput (const MidiEvent& e, double beat, std::vector<MidiEvent>& out, int sampleOffset);
    void fireStep (double beat, double stepLen);
    void rebuildSequence();
    bool nextGroup (Group& g);
    int  shapePitch (int pitch, int octaveOffset, int semitoneOffset) const;
    void scheduleNote (int pitch, int velocity, int channel, double onBeat, double offBeat);
    void allNotesOff (std::vector<MidiEvent>& out, int sampleOffset);
    double stepBeat (long long k, double stepLen) const;
    int outChannelFor (int inChannel) const;

    Settings settings;
    Pattern pattern;
    double sampleRate = 44100.0;

    std::vector<Held> held;            // physically held (or latched) notes
    std::vector<int> physicallyDown;   // keys actually down (for latch)
    int orderCounter = 0;
    bool seqDirty = true;

    // Note sequence derived from held notes; each entry is one pitch or (for chord modes) a group.
    std::vector<Group> seq;
    int seqPos = 0, seqDir = 1, repeatCount = 0;
    std::vector<int> shuffleOrder;

    // Clock
    double freeBeat = 0.0;
    double lastBlockEnd = -1.0;
    double gridOrigin = 0.0;
    long long nextK = 0;
    long long phaseStartK = 0;
    double lastStepLen = 0.0;
    bool running = false;              // notes are held and the arp is stepping
    long long stepCounter = 0;

    // Output scheduling
    std::vector<Pending> pending;
    uint32_t serialCounter = 1;
    uint32_t soundingSerial[16][128] {};
    struct Sounding { int channel, pitch; uint32_t serial; };
    std::vector<Sounding> lastStepNotes;  // for ties

    int lastInputChannel = 0;
    double currentBpm = 120.0, beatsPerSample = 0.0, blockStartBeat = 0.0;
    bool hostMode = false, lastEnabled = true;
    int lastSeqKey = -1;
    std::vector<int> lastChordMemory;

    std::mt19937 rng;
    uint32_t lastSeed = 0;
};

} // namespace kal
