#pragma once

#include "PluginProcessor.h"
#include "Themes.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class KaleidoEditor;

// All seven lanes at a glance: shows how lanes of different lengths drift against each other.
class LaneOverview : public juce::Component
{
public:
    explicit LaneOverview (KaleidoEditor& e) : editor (e) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    KaleidoEditor& editor;
};

// Draw editor for the selected lane.
class LaneEditor : public juce::Component
{
public:
    explicit LaneEditor (KaleidoEditor& e) : editor (e) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    int stepAt (float x) const;
    int valueAt (float y) const;
    void apply (const juce::MouseEvent&, bool first);
    KaleidoEditor& editor;
    int paintValue = 1;
    int lastStep = -1;
};

class KaleidoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit KaleidoEditor (KaleidoProcessor&);
    ~KaleidoEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // shared with the lane components
    kal::Pattern pat;
    kal::Lane selected = kal::Lane::Rhythm;
    int playStep = -1;                 // step within the playing slot, -1 when idle
    int editSlot = 0;
    int playSlot = 0;
    int lanePos (kal::Lane l) const;   // playhead position in a lane of the edited slot, or -1
    const kal::Theme& theme() const { return lnf.theme(); }
    void beginEdit();                  // pushes undo state
    void commit();                     // sends the working pattern to the processor
    void selectLane (kal::Lane l);

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Choice
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
    };
    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
    };
    struct GenSlider
    {
        juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        juce::Label label;
    };

    Knob& knob (const char* paramId, const juce::String& label);
    Choice& choice (const char* paramId, const juce::String& label);
    Toggle& toggle (const char* paramId, const juce::String& label);
    GenSlider& genSlider (const juce::String& label, double lo, double hi, double step, double init);
    juce::TextButton& button (const juce::String& text, std::function<void()> fn);

    void timerCallback() override;
    void applyTheme (int index);
    void rebuildPresetMenu();
    void presetChosen();
    void stepPreset (int delta);
    void savePresetDialog();
    void undo();
    void tool (std::function<void (kal::Pattern&)> fn);
    void refreshChordLabel();
    void layoutKnobRow (juce::Rectangle<int> r, std::initializer_list<Knob*> knobs);
    void selectSlot (int slot);

    KaleidoProcessor& proc;
    kal::KaleidoLookAndFeel lnf;
    juce::Random random;

    std::map<juce::String, std::unique_ptr<Knob>> knobs;
    std::map<juce::String, std::unique_ptr<Choice>> choices;
    std::map<juce::String, std::unique_ptr<Toggle>> toggles;
    std::vector<std::unique_ptr<GenSlider>> genSliders;
    std::vector<std::unique_ptr<juce::TextButton>> buttons;

    juce::ComboBox presetBox, themeBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "Save" }, undoBtn { "Undo" };
    juce::TextButton enableBtn { "ARP ON" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAtt;
    juce::Label chordLabel, statusLabel;
    juce::OwnedArray<juce::TextButton> laneTabs;
    juce::Slider lengthSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label lengthLabel;
    juce::Slider divSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label divLabel;
    juce::OwnedArray<juce::TextButton> slotButtons;
    juce::TextButton copyBtn { "Copy" }, pasteBtn { "Paste" };
    kal::Pattern clipboard;
    bool haveClipboard = false;
    juce::Rectangle<int> slotRow;
    int lastVersion = -1;

    LaneOverview overview { *this };
    LaneEditor laneEditor { *this };

    // generator controls (pointers into genSliders)
    GenSlider *eHits = nullptr, *eSteps = nullptr, *eRot = nullptr, *density = nullptr,
              *mStay = nullptr, *mStep = nullptr, *mLeap = nullptr, *mutateAmt = nullptr;
    // tool buttons laid out in rows
    std::vector<juce::TextButton*> toolRow, genRow1, genRow2;
    juce::TextButton* learnBtn = nullptr;

    std::vector<kal::Pattern> history;
    std::vector<int> presetIds;        // combo ids in display order
    juce::Array<juce::File> userFiles;
    std::unique_ptr<juce::AlertWindow> saveWindow;
    int lastActivity = 0;
    float activityGlow = 0.0f;

    // section frames for paint()
    juce::Rectangle<int> playArea, pitchArea, outArea, laneArea, genArea, headerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KaleidoEditor)
};
