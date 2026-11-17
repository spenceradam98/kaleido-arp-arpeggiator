#pragma once

#include "ArpEngine.h"
#include "Presets.h"
#include "PreviewSynth.h"

#include <juce_audio_processors/juce_audio_processors.h>

class KaleidoProcessor : public juce::AudioProcessor
{
public:
    KaleidoProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Kaleido"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- used by the editor (message thread) ----
    juce::AudioProcessorValueTreeState params;

    kal::Pattern getPattern (int slot) const;
    void setPattern (int slot, const kal::Pattern& p);
    kal::Bank getBank() const;
    void setBank (const kal::Bank& b);
    std::atomic<int> editSlot { 0 };            // slot shown in the editor (chord Learn writes here)
    kal::Settings currentSettings() const;
    void loadPreset (const kal::Preset& p);
    kal::Preset makePreset (const juce::String& name, const juce::String& category) const;

    static juce::File userPresetFolder();
    juce::Array<juce::File> userPresetFiles() const;
    bool saveUserPreset (const juce::String& name);
    bool loadPresetFile (const juce::File& f);

    void requestChordLearn() { learnRequested = true; }
    std::atomic<bool> chordLearned { false };   // set by the audio thread after a learn
    std::atomic<int> patternVersion { 0 };      // bumps whenever the shared pattern changes
    juce::String presetName;
    int themeIndex = 0;
    std::atomic<uint32_t> seed { 1 };

    kal::ArpEngine& engine() { return arp; }
    std::atomic<int> outputActivity { 0 };   // bumps on each note-on sent

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static BusesProperties makeBuses();
    float p (const char* id) const { return params.getRawParameterValue (id)->load(); }
    void setParam (const char* id, float plainValue);

    kal::ArpEngine arp;
    kal::PreviewSynth preview;

    mutable juce::SpinLock patternLock;
    kal::Bank sharedBank;
    std::atomic<bool> patternDirty { true };
    std::atomic<bool> learnRequested { false };

    std::vector<kal::MidiEvent> midiIn, midiOut;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KaleidoProcessor)
};
