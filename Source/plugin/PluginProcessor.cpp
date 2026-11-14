#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace kal;

namespace
{
    juce::StringArray namesOf (int count, const char* (*fn) (int))
    {
        juce::StringArray a;
        for (int i = 0; i < count; ++i) a.add (fn (i));
        return a;
    }
    const char* modeN (int i) { return modeName ((ArpMode) i); }
    const char* octN (int i) { return octaveModeName ((OctaveMode) i); }
    const char* velN (int i) { return velocitySourceName ((VelocitySource) i); }
    const char* rateN (int i) { return rates()[(size_t) i].name; }
    const char* scaleN (int i) { return scales()[(size_t) i].name; }
    const char* noteN (int i) { return noteName (i); }

    juce::String pct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
    juce::String ms (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; }
    juce::String midiNote (int v, int) { return juce::String (noteName (v)) + juce::String (v / 12 - 1); }
}

juce::AudioProcessorValueTreeState::ParameterLayout KaleidoProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto id = [] (const char* s) { return ParameterID { s, 1 }; };
    auto fAttr = [] (auto fn) { return AudioParameterFloatAttributes().withStringFromValueFunction (fn); };

    l.add (std::make_unique<AudioParameterBool> (id ("enabled"), "Arp On", true));
    l.add (std::make_unique<AudioParameterChoice> (id ("mode"), "Mode", namesOf ((int) ArpMode::Count, modeN), 0));
    l.add (std::make_unique<AudioParameterInt> (id ("octaves"), "Octaves", 1, 4, 1));
    l.add (std::make_unique<AudioParameterChoice> (id ("octaveMode"), "Octave Mode", namesOf ((int) OctaveMode::Count, octN), 0));
    l.add (std::make_unique<AudioParameterChoice> (id ("rate"), "Rate", namesOf ((int) rates().size(), rateN), defaultRateIndex()));
    l.add (std::make_unique<AudioParameterFloat> (id ("swing"), "Swing", NormalisableRange<float> (0.0f, 1.0f), 0.0f, fAttr (pct)));
    l.add (std::make_unique<AudioParameterFloat> (id ("gate"), "Gate", NormalisableRange<float> (0.1f, 2.0f), 1.0f, fAttr (pct)));
    l.add (std::make_unique<AudioParameterChoice> (id ("velSource"), "Velocity Source", namesOf ((int) VelocitySource::Count, velN), 0));
    l.add (std::make_unique<AudioParameterBool> (id ("latch"), "Latch", false));
    l.add (std::make_unique<AudioParameterBool> (id ("retrigger"), "Retrigger", true));
    l.add (std::make_unique<AudioParameterBool> (id ("advanceOnRest"), "Advance On Rest", false));
    l.add (std::make_unique<AudioParameterInt> (id ("noteRepeat"), "Note Repeat", 1, 8, 1));
    l.add (std::make_unique<AudioParameterInt> (id ("transpose"), "Transpose", -24, 24, 0));
    l.add (std::make_unique<AudioParameterChoice> (id ("scaleRoot"), "Scale Root", namesOf (12, noteN), 0));
    l.add (std::make_unique<AudioParameterChoice> (id ("scale"), "Scale", namesOf ((int) scales().size(), scaleN), 0));
    l.add (std::make_unique<AudioParameterBool> (id ("degreeMode"), "Scale Degrees", false));
    l.add (std::make_unique<AudioParameterBool> (id ("chordMemory"), "Chord Memory", false));
    l.add (std::make_unique<AudioParameterFloat> (id ("strum"), "Strum", NormalisableRange<float> (0.0f, 200.0f), 30.0f, fAttr (ms)));
    l.add (std::make_unique<AudioParameterFloat> (id ("humTime"), "Humanise Time", NormalisableRange<float> (0.0f, 40.0f), 0.0f, fAttr (ms)));
    l.add (std::make_unique<AudioParameterInt> (id ("humVel"), "Humanise Velocity", 0, 40, 0));
    l.add (std::make_unique<AudioParameterInt> (id ("rangeLow"), "Range Low", 0, 127, 0, AudioParameterIntAttributes().withStringFromValueFunction (midiNote)));
    l.add (std::make_unique<AudioParameterInt> (id ("rangeHigh"), "Range High", 0, 127, 127, AudioParameterIntAttributes().withStringFromValueFunction (midiNote)));
    StringArray chans { "As input" };
    for (int c = 1; c <= 16; ++c) chans.add ("Ch " + String (c));
    l.add (std::make_unique<AudioParameterChoice> (id ("outChannel"), "Output Channel", chans, 0));
    l.add (std::make_unique<AudioParameterBool> (id ("sync"), "Host Sync", true));
    l.add (std::make_unique<AudioParameterFloat> (id ("bpm"), "Internal BPM", NormalisableRange<float> (20.0f, 300.0f, 0.1f), 120.0f));
    l.add (std::make_unique<AudioParameterBool> (id ("preview"), "Preview Synth", false));
    l.add (std::make_unique<AudioParameterFloat> (id ("previewLevel"), "Preview Level", NormalisableRange<float> (0.0f, 1.0f), 0.7f, fAttr (pct)));
    return l;
}

juce::AudioProcessor::BusesProperties KaleidoProcessor::makeBuses()
{
#if JucePlugin_IsMidiEffect
    return BusesProperties();
#else
    return BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true);
#endif
}

KaleidoProcessor::KaleidoProcessor()
    : AudioProcessor (makeBuses()),
      params (*this, nullptr, "KaleidoParams", createLayout())
{
    midiIn.reserve (2048);
    midiOut.reserve (4096);
    presetName = "Init";
    if (wrapperType == wrapperType_Standalone)
        setParam ("preview", 1.0f);   // standalone: audible out of the box
}

bool KaleidoProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

bool KaleidoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
#endif
}

void KaleidoProcessor::prepareToPlay (double sampleRate, int)
{
    arp.prepare (sampleRate);
    preview.prepare (sampleRate);
    patternDirty = true;
}

void KaleidoProcessor::setParam (const char* id, float plainValue)
{
    if (auto* prm = params.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
}

Settings KaleidoProcessor::currentSettings() const
{
    Settings s;
    s.enabled = p ("enabled") > 0.5f;
    s.mode = (ArpMode) (int) p ("mode");
    s.octaves = (int) p ("octaves");
    s.octaveMode = (OctaveMode) (int) p ("octaveMode");
    s.rateIndex = (int) p ("rate");
    s.swing = p ("swing");
    s.gateScale = p ("gate");
    s.velSource = (VelocitySource) (int) p ("velSource");
    s.latch = p ("latch") > 0.5f;
    s.retrigger = p ("retrigger") > 0.5f;
    s.advanceOnRest = p ("advanceOnRest") > 0.5f;
    s.noteRepeat = (int) p ("noteRepeat");
    s.transpose = (int) p ("transpose");
    s.scaleRoot = (int) p ("scaleRoot");
    s.scaleIndex = (int) p ("scale");
    s.degreeMode = p ("degreeMode") > 0.5f;
    s.chordMemory = p ("chordMemory") > 0.5f;
    s.strumMs = p ("strum");
    s.humaniseMs = p ("humTime");
    s.humaniseVel = (int) p ("humVel");
    s.rangeLow = (int) p ("rangeLow");
    s.rangeHigh = (int) p ("rangeHigh");
    s.outChannel = (int) p ("outChannel");
    s.syncToHost = p ("sync") > 0.5f;
    s.internalBpm = p ("bpm");
    s.seed = seed.load();
    return s;
}

void KaleidoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    if (patternDirty.load())
    {
        const juce::SpinLock::ScopedTryLockType lock (patternLock);
        if (lock.isLocked())
        {
            arp.setPattern (sharedPattern);
            patternDirty = false;
        }
    }
    arp.setSettings (currentSettings());

    if (learnRequested.exchange (false))
    {
        auto shape = arp.heldChordShape();
        if (! shape.empty())
        {
            const juce::SpinLock::ScopedLockType lock (patternLock);
            sharedPattern.chordMemory = shape;
            arp.setPattern (sharedPattern);
            chordLearned = true;
            ++patternVersion;
        }
    }

    kal::TransportInfo tr;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            tr.hostPlaying = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition()) tr.ppq = *ppq;
            else tr.hostPlaying = false;
            if (auto bpm = pos->getBpm()) tr.bpm = *bpm;
        }
    }
    if (wrapperType == wrapperType_Standalone) { tr.hostPlaying = false; tr.bpm = 0.0; }

    midiIn.clear();
    juce::MidiBuffer passThroughLong;   // sysex and other long messages pass straight through
    for (const auto m : midi)
    {
        if (m.numBytes >= 1 && m.numBytes <= 3)
            midiIn.push_back ({ m.samplePosition, m.data[0],
                                (uint8_t) (m.numBytes > 1 ? m.data[1] : 0),
                                (uint8_t) (m.numBytes > 2 ? m.data[2] : 0) });
        else
            passThroughLong.addEvent (m.data, m.numBytes, m.samplePosition);
    }

    midiOut.clear();
    arp.process (midiIn, midiOut, n, tr);

    midi.clear();
    midi.addEvents (passThroughLong, 0, n, 0);

    const bool usePreview = p ("preview") > 0.5f && buffer.getNumChannels() > 0;
    const float level = p ("previewLevel");
    int rendered = 0;
    float* left = buffer.getNumChannels() > 0 ? buffer.getWritePointer (0) : nullptr;
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : left;

    for (auto& e : midiOut)
    {
        const int type = e.status & 0xF0;
        const int len = juce::jlimit (1, 3, juce::MidiMessage::getMessageLengthFromFirstByte (e.status));
        const uint8_t bytes[3] = { e.status, e.data1, e.data2 };
        const int pos = juce::jlimit (0, juce::jmax (0, n - 1), e.sampleOffset);
        midi.addEvent (bytes, len, pos);

        if (type == 0x90 && e.data2 > 0) ++outputActivity;
        if (usePreview)
        {
            if (pos > rendered) { preview.render (left, right, rendered, pos - rendered, level); rendered = pos; }
            if (type == 0x90 && e.data2 > 0) preview.noteOn (e.status & 15, e.data1, e.data2);
            else if (type == 0x80 || type == 0x90) preview.noteOff (e.status & 15, e.data1);
        }
    }
    if (usePreview)
    {
        if (n > rendered) preview.render (left, right, rendered, n - rendered, level);
    }
    else
        preview.allOff();

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
}

// ---- pattern / presets -----------------------------------------------------------------------

kal::Pattern KaleidoProcessor::getPattern() const
{
    const juce::SpinLock::ScopedLockType lock (patternLock);
    return sharedPattern;
}

void KaleidoProcessor::setPattern (const kal::Pattern& np)
{
    {
        const juce::SpinLock::ScopedLockType lock (patternLock);
        sharedPattern = np;
    }
    patternDirty = true;
    ++patternVersion;
}

void KaleidoProcessor::loadPreset (const kal::Preset& pr)
{
    const auto& s = pr.settings;
    setParam ("mode", (float) s.mode);
    setParam ("octaves", (float) s.octaves);
    setParam ("octaveMode", (float) s.octaveMode);
    setParam ("rate", (float) s.rateIndex);
    setParam ("swing", s.swing);
    setParam ("gate", s.gateScale);
    setParam ("velSource", (float) s.velSource);
    setParam ("latch", s.latch ? 1.0f : 0.0f);
    setParam ("retrigger", s.retrigger ? 1.0f : 0.0f);
    setParam ("advanceOnRest", s.advanceOnRest ? 1.0f : 0.0f);
    setParam ("noteRepeat", (float) s.noteRepeat);
    setParam ("transpose", (float) s.transpose);
    setParam ("scaleRoot", (float) s.scaleRoot);
    setParam ("scale", (float) s.scaleIndex);
    setParam ("degreeMode", s.degreeMode ? 1.0f : 0.0f);
    setParam ("chordMemory", s.chordMemory ? 1.0f : 0.0f);
    setParam ("strum", s.strumMs);
    setParam ("humTime", s.humaniseMs);
    setParam ("humVel", (float) s.humaniseVel);
    setParam ("rangeLow", (float) s.rangeLow);
    setParam ("rangeHigh", (float) s.rangeHigh);
    // Output channel, host sync, tempo, on/off and preview are setup choices, not part of a sound.
    seed = s.seed;
    setPattern (pr.pattern);
    presetName = pr.name;
}

kal::Preset KaleidoProcessor::makePreset (const juce::String& name, const juce::String& category) const
{
    kal::Preset pr;
    pr.name = name.toStdString();
    pr.category = category.toStdString();
    pr.settings = currentSettings();
    pr.pattern = getPattern();
    return pr;
}

int KaleidoProcessor::getNumPrograms() { return (int) factoryPresets().size(); }

void KaleidoProcessor::setCurrentProgram (int index)
{
    if (juce::isPositiveAndBelow (index, (int) factoryPresets().size()))
    {
        currentProgram = index;
        loadPreset (factoryPresets()[(size_t) index]);
    }
}

const juce::String KaleidoProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) factoryPresets().size())
               ? juce::String (factoryPresets()[(size_t) index].name) : juce::String();
}

juce::File KaleidoProcessor::userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Kaleido Audio").getChildFile ("Kaleido").getChildFile ("Presets");
}

juce::Array<juce::File> KaleidoProcessor::userPresetFiles() const
{
    auto files = userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.kalp");
    files.sort();
    return files;
}

bool KaleidoProcessor::saveUserPreset (const juce::String& name)
{
    auto folder = userPresetFolder();
    if (! folder.createDirectory()) return false;
    const auto safe = juce::File::createLegalFileName (name.trim().isEmpty() ? "Untitled" : name.trim());
    const auto text = kal::toText (makePreset (safe, "User"));
    presetName = safe;
    return folder.getChildFile (safe + ".kalp").replaceWithText (text);
}

bool KaleidoProcessor::loadPresetFile (const juce::File& f)
{
    kal::Preset pr;
    if (! kal::fromText (f.loadFileAsString().toStdString(), pr)) return false;
    if (pr.name.empty()) pr.name = f.getFileNameWithoutExtension().toStdString();
    loadPreset (pr);
    return true;
}

// ---- state ----------------------------------------------------------------------------------

void KaleidoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = params.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    auto* extra = xml->createNewChildElement ("Kaleido");
    extra->setAttribute ("preset", juce::String (kal::toText (makePreset (presetName, "State"))));
    extra->setAttribute ("presetName", presetName);
    extra->setAttribute ("theme", themeIndex);
    copyXmlToBinary (*xml, destData);
}

void KaleidoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr) return;
    kal::Preset pr;
    bool havePattern = false;
    if (auto* extra = xml->getChildByName ("Kaleido"))
    {
        havePattern = kal::fromText (extra->getStringAttribute ("preset").toStdString(), pr);
        presetName = extra->getStringAttribute ("presetName", "Init");
        themeIndex = extra->getIntAttribute ("theme", 0);
        xml->removeChildElement (extra, true);
    }
    if (xml->hasTagName (params.state.getType()))
        params.replaceState (juce::ValueTree::fromXml (*xml));
    if (havePattern)
    {
        seed = pr.settings.seed;
        setPattern (pr.pattern);
    }
}

juce::AudioProcessorEditor* KaleidoProcessor::createEditor() { return new KaleidoEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KaleidoProcessor(); }
