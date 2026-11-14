#include "PluginEditor.h"

#include "Generators.h"

using namespace kal;

namespace
{
    constexpr int kWidth = 1120, kHeight = 720;

    float norm (Lane l, int v)
    {
        const auto& s = laneSpec (l);
        return (float) (v - s.minValue) / (float) juce::jmax (1, s.maxValue - s.minValue);
    }
    bool bipolar (Lane l) { return l == Lane::Octave || l == Lane::Semitone; }

    juce::String valueText (Lane l, int v)
    {
        switch (l)
        {
            case Lane::Rhythm: return v == kOn ? "on" : v == kTie ? "tie" : "rest";
            case Lane::Gate: case Lane::Probability: return juce::String (v) + "%";
            case Lane::Octave: case Lane::Semitone: return (v > 0 ? "+" : "") + juce::String (v);
            case Lane::Ratchet: return "x" + juce::String (v);
            default: return juce::String (v);
        }
    }

    void sectionFrame (juce::Graphics& g, juce::Rectangle<int> r, const Theme& t, const juce::String& title)
    {
        g.setColour (t.panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (t.edge);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (t.accent2);
        g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
        g.drawText (title.toUpperCase(), r.reduced (12, 6).removeFromTop (14), juce::Justification::topLeft);
    }
}

// =============================================================================================
// LaneOverview

void LaneOverview::paint (juce::Graphics& g)
{
    const auto& t = editor.theme();
    const auto b = getLocalBounds().toFloat();
    const float rowH = b.getHeight() / (float) kNumLanes;
    const float labelW = 78.0f;
    const float cellW = (b.getWidth() - labelW) / (float) kMaxSteps;

    for (int li = 0; li < kNumLanes; ++li)
    {
        const auto lane = (Lane) li;
        const auto& ld = editor.pat.lane (lane);
        const float y = b.getY() + li * rowH;
        const bool sel = lane == editor.selected;

        g.setColour (sel ? t.accent : t.dim);
        g.setFont (juce::Font (juce::FontOptions (11.0f, sel ? juce::Font::bold : juce::Font::plain)));
        g.drawText (juce::String (laneSpec (lane).name) + " " + juce::String (ld.length), juce::Rectangle<float> (0, y, labelW - 4, rowH),
                    juce::Justification::centredLeft);

        const int pos = editor.playStep >= 0 ? editor.playStep % juce::jmax (1, ld.length) : -1;
        for (int s = 0; s < kMaxSteps; ++s)
        {
            auto cell = juce::Rectangle<float> (labelW + s * cellW, y, cellW, rowH).reduced (1.0f, 1.5f);
            if (s >= ld.length)
            {
                g.setColour (t.cellOff.withAlpha (0.35f));
                g.fillRect (cell);
                continue;
            }
            const int v = ld.values[(size_t) s];
            float a = lane == Lane::Rhythm ? (v == kOn ? 1.0f : v == kTie ? 0.55f : 0.0f)
                    : bipolar (lane) ? std::abs (v) / (float) laneSpec (lane).maxValue
                    : norm (lane, v);
            if (lane == Lane::Ratchet) a = v > 1 ? 0.35f + 0.65f * norm (lane, v) : 0.0f;
            if (lane == Lane::Probability) a = 1.0f - norm (lane, v) > 0.0f ? 1.0f - norm (lane, v) : 0.0f;
            g.setColour (t.cellOff);
            g.fillRect (cell);
            g.setColour ((li % 2 == 0 ? t.accent : t.accent2).withAlpha (0.15f + 0.85f * a));
            if (a > 0.0f) g.fillRect (cell);
            if (s == pos)
            {
                g.setColour (t.playhead);
                g.drawRect (cell.expanded (0.5f), 1.5f);
            }
        }
    }
}

void LaneOverview::mouseDown (const juce::MouseEvent& e)
{
    const int row = juce::jlimit (0, kNumLanes - 1, (int) (e.position.y / (getHeight() / (float) kNumLanes)));
    editor.selectLane ((Lane) row);
}

// =============================================================================================
// LaneEditor

int LaneEditor::stepAt (float x) const
{
    return juce::jlimit (0, kMaxSteps - 1, (int) (x / (getWidth() / (float) kMaxSteps)));
}

int LaneEditor::valueAt (float y) const
{
    const auto& s = laneSpec (editor.selected);
    const float n = 1.0f - juce::jlimit (0.0f, 1.0f, y / (float) getHeight());
    return juce::jlimit (s.minValue, s.maxValue, juce::roundToInt (s.minValue + n * (s.maxValue - s.minValue)));
}

void LaneEditor::paint (juce::Graphics& g)
{
    const auto& t = editor.theme();
    const auto lane = editor.selected;
    const auto& ld = editor.pat.lane (lane);
    const auto b = getLocalBounds().toFloat();
    const float cw = b.getWidth() / (float) kMaxSteps;

    g.setColour (t.bg.withAlpha (0.6f));
    g.fillRoundedRectangle (b, 6.0f);

    const int pos = editor.playStep >= 0 ? editor.playStep % juce::jmax (1, ld.length) : -1;
    const float zeroY = bipolar (lane) ? b.getCentreY() : b.getBottom();

    for (int s = 0; s < kMaxSteps; ++s)
    {
        auto col = juce::Rectangle<float> (s * cw, 0, cw, b.getHeight()).reduced (2.0f, 4.0f);
        const bool active = s < ld.length;
        g.setColour (s % 4 == 0 ? t.cellOff.brighter (0.06f) : t.cellOff);
        g.fillRoundedRectangle (col, 3.0f);
        if (! active)
        {
            g.setColour (t.bg.withAlpha (0.55f));
            g.fillRoundedRectangle (col, 3.0f);
        }

        const int v = ld.values[(size_t) s];
        auto fill = (active ? t.accent : t.dim).withAlpha (active ? 0.9f : 0.3f);
        if (lane == Lane::Rhythm)
        {
            if (v == kOn) { g.setColour (fill); g.fillRoundedRectangle (col.reduced (3.0f, col.getHeight() * 0.3f), 4.0f); }
            else if (v == kTie)
            {
                g.setColour (fill.withAlpha (0.55f));
                g.fillRect (col.withX (col.getX() - 4.0f).withWidth (col.getWidth() + 4.0f)
                               .reduced (0.0f, col.getHeight() * 0.42f));
            }
        }
        else if (bipolar (lane))
        {
            const float n = (float) v / (float) laneSpec (lane).maxValue;
            const float y = zeroY - n * (col.getHeight() * 0.5f);
            auto bar = juce::Rectangle<float>::leftTopRightBottom (col.getX() + 2, juce::jmin (y, zeroY), col.getRight() - 2, juce::jmax (y, zeroY));
            g.setColour (fill);
            g.fillRect (bar.getHeight() < 2.0f ? bar.withHeight (2.0f).withCentre ({ bar.getCentreX(), zeroY }) : bar);
        }
        else
        {
            const float n = norm (lane, v);
            auto bar = col.reduced (2.0f, 0.0f).withTrimmedTop (col.getHeight() * (1.0f - n));
            g.setGradientFill (juce::ColourGradient (t.accent2.withAlpha (active ? 0.9f : 0.3f), 0, col.getBottom(),
                                                     fill, 0, col.getY(), false));
            g.fillRect (bar);
        }

        if (active && (lane != Lane::Rhythm || v != kRest) && cw > 18.0f)
        {
            g.setColour (t.text.withAlpha (0.8f));
            g.setFont (juce::Font (juce::FontOptions (9.5f)));
            g.drawText (valueText (lane, v), col.removeFromBottom (14.0f), juce::Justification::centred);
        }
        if (s == pos)
        {
            g.setColour (t.playhead);
            g.drawRoundedRectangle (juce::Rectangle<float> (s * cw, 0, cw, b.getHeight()).reduced (1.0f, 2.0f), 3.0f, 2.0f);
        }
    }
    if (bipolar (lane))
    {
        g.setColour (t.dim.withAlpha (0.5f));
        g.drawHorizontalLine ((int) zeroY, 0.0f, b.getRight());
    }
}

void LaneEditor::apply (const juce::MouseEvent& e, bool first)
{
    const auto lane = editor.selected;
    auto& ld = editor.pat.lane (lane);
    const int s = stepAt (e.position.x);

    if (first && e.mods.isShiftDown()) { ld.length = s + 1; editor.commit(); repaint(); return; }

    if (lane == Lane::Rhythm)
    {
        if (first)
        {
            const int v = ld.values[(size_t) s];
            paintValue = e.mods.isPopupMenu() ? kRest : (v == kRest ? kOn : v == kOn ? kTie : kRest);
        }
        if (first || s != lastStep) ld.values[(size_t) s] = paintValue;
    }
    else
    {
        const int v = e.mods.isPopupMenu() ? laneSpec (lane).defaultValue : valueAt (e.position.y);
        // Fill skipped columns when the mouse moves fast.
        const int from = (first || lastStep < 0) ? s : lastStep;
        for (int i = juce::jmin (from, s); i <= juce::jmax (from, s); ++i) ld.values[(size_t) i] = v;
    }
    lastStep = s;
    editor.commit();
    repaint();
}

void LaneEditor::mouseDown (const juce::MouseEvent& e) { editor.beginEdit(); lastStep = -1; apply (e, true); }
void LaneEditor::mouseDrag (const juce::MouseEvent& e) { if (! e.mods.isShiftDown()) apply (e, false); }
void LaneEditor::mouseUp (const juce::MouseEvent&) { lastStep = -1; }

// =============================================================================================
// KaleidoEditor

KaleidoEditor::Knob& KaleidoEditor::knob (const char* id, const juce::String& text)
{
    auto k = std::make_unique<Knob>();
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 15);
    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::Font (juce::FontOptions (11.5f)));
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.params, id, k->slider);
    auto& ref = *k;
    knobs[id] = std::move (k);
    return ref;
}

KaleidoEditor::Choice& KaleidoEditor::choice (const char* id, const juce::String& text)
{
    auto c = std::make_unique<Choice>();
    if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (proc.params.getParameter (id)))
        c->box.addItemList (prm->choices, 1);
    c->label.setText (text, juce::dontSendNotification);
    c->label.setFont (juce::Font (juce::FontOptions (11.5f)));
    addAndMakeVisible (c->box);
    addAndMakeVisible (c->label);
    c->att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.params, id, c->box);
    auto& ref = *c;
    choices[id] = std::move (c);
    return ref;
}

KaleidoEditor::Toggle& KaleidoEditor::toggle (const char* id, const juce::String& text)
{
    auto t = std::make_unique<Toggle>();
    t->button.setButtonText (text);
    addAndMakeVisible (t->button);
    t->att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.params, id, t->button);
    auto& ref = *t;
    toggles[id] = std::move (t);
    return ref;
}

KaleidoEditor::GenSlider& KaleidoEditor::genSlider (const juce::String& text, double lo, double hi, double step, double init)
{
    auto s = std::make_unique<GenSlider>();
    s->slider.setRange (lo, hi, step);
    s->slider.setValue (init, juce::dontSendNotification);
    s->slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 34, 18);
    s->label.setText (text, juce::dontSendNotification);
    s->label.setFont (juce::Font (juce::FontOptions (11.5f)));
    addAndMakeVisible (s->slider);
    addAndMakeVisible (s->label);
    genSliders.push_back (std::move (s));
    return *genSliders.back();
}

juce::TextButton& KaleidoEditor::button (const juce::String& text, std::function<void()> fn)
{
    auto b = std::make_unique<juce::TextButton> (text);
    b->onClick = std::move (fn);
    addAndMakeVisible (*b);
    buttons.push_back (std::move (b));
    return *buttons.back();
}

KaleidoEditor::KaleidoEditor (KaleidoProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    pat = proc.getPattern();

    // ---- header ----
    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this] { presetChosen(); };
    for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &undoBtn }) addAndMakeVisible (*b);
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    saveBtn.onClick = [this] { savePresetDialog(); };
    undoBtn.onClick = [this] { undo(); };
    for (int i = 0; i < themes().size(); ++i) themeBox.addItem (themes()[i].name, i + 1);
    themeBox.onChange = [this] { applyTheme (themeBox.getSelectedId() - 1); };
    addAndMakeVisible (themeBox);
    enableBtn.setClickingTogglesState (true);
    addAndMakeVisible (enableBtn);
    enableAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.params, "enabled", enableBtn);

    // ---- play ----
    choice ("mode", "Mode");
    choice ("octaveMode", "Octave Mode");
    choice ("rate", "Rate");
    choice ("velSource", "Velocity");
    knob ("octaves", "Octaves");
    knob ("swing", "Swing");
    knob ("gate", "Gate");
    knob ("noteRepeat", "Repeat");
    knob ("strum", "Strum");
    toggle ("latch", "Latch");
    toggle ("retrigger", "Retrigger");
    toggle ("advanceOnRest", "Move on rests");

    // ---- pitch & feel ----
    choice ("scaleRoot", "Root");
    choice ("scale", "Scale");
    knob ("transpose", "Transpose");
    knob ("rangeLow", "Low");
    knob ("rangeHigh", "High");
    knob ("humTime", "Hum. Time");
    knob ("humVel", "Hum. Vel");
    toggle ("degreeMode", "Lane in scale steps");
    toggle ("chordMemory", "Chord memory");
    learnBtn = &button ("Learn chord", [this] { proc.requestChordLearn(); statusLabel.setText ("Hold a chord: it is captured on the next audio block.", juce::dontSendNotification); });
    addAndMakeVisible (chordLabel);
    chordLabel.setFont (juce::Font (juce::FontOptions (12.0f)));

    // ---- output ----
    choice ("outChannel", "MIDI Out");
    toggle ("sync", "Host sync");
    toggle ("preview", "Preview synth");
    knob ("bpm", "Free BPM");
    knob ("previewLevel", "Level");

    // ---- lanes ----
    for (int i = 0; i < kNumLanes; ++i)
    {
        auto* b = laneTabs.add (new juce::TextButton (laneSpec ((Lane) i).name));
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { selectLane ((Lane) i); };
        addAndMakeVisible (b);
    }
    addAndMakeVisible (overview);
    addAndMakeVisible (laneEditor);
    lengthSlider.setRange (1, kMaxSteps, 1);
    lengthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 34, 18);
    lengthSlider.onDragStart = [this] { beginEdit(); };
    lengthSlider.onValueChange = [this]
    {
        pat.lane (selected).length = (int) lengthSlider.getValue();
        commit();
    };
    addAndMakeVisible (lengthSlider);
    lengthLabel.setText ("Length", juce::dontSendNotification);
    lengthLabel.setFont (juce::Font (juce::FontOptions (11.5f)));
    addAndMakeVisible (lengthLabel);

    toolRow = {
        &button ("Rotate <", [this] { tool ([this] (Pattern& q) { gen::rotate (q, selected, -1); }); }),
        &button ("Rotate >", [this] { tool ([this] (Pattern& q) { gen::rotate (q, selected, 1); }); }),
        &button ("Reverse", [this] { tool ([this] (Pattern& q) { gen::reverse (q, selected); }); }),
        &button ("Invert", [this] { tool ([this] (Pattern& q) { gen::invert (q, selected); }); }),
        &button ("Ramp up", [this] { tool ([this] (Pattern& q) { gen::ramp (q, selected, true); }); }),
        &button ("Ramp down", [this] { tool ([this] (Pattern& q) { gen::ramp (q, selected, false); }); }),
        &button ("Humanise", [this] { tool ([this] (Pattern& q) { gen::humanise (q, selected, 1.0f, (uint32_t) random.nextInt()); }); }),
        &button ("x2", [this] { tool ([this] (Pattern& q) { gen::doubleLength (q, selected); }); }),
        &button ("All = len", [this] { tool ([this] (Pattern& q) { gen::setAllLengths (q, q.lane (selected).length); }); }),
        &button ("Clear", [this] { tool ([this] (Pattern& q) { gen::clear (q, selected); }); }),
    };

    // ---- generators ----
    eHits = &genSlider ("Hits", 0, 32, 1, 5);
    eSteps = &genSlider ("Steps", 1, 32, 1, 16);
    eRot = &genSlider ("Rotate", 0, 31, 1, 0);
    density = &genSlider ("Density", 0, 1, 0.01, 0.6);
    mStay = &genSlider ("Stay", 0, 4, 0.1, 1.0);
    mStep = &genSlider ("Step", 0, 4, 0.1, 2.0);
    mLeap = &genSlider ("Leap", 0, 4, 0.1, 0.6);
    mutateAmt = &genSlider ("Amount", 0, 1, 0.01, 0.4);

    genRow1 = {
        &button ("Euclid", [this] { tool ([this] (Pattern& q) { gen::euclid (q, (int) eHits->slider.getValue(), (int) eSteps->slider.getValue(), (int) eRot->slider.getValue()); }); selectLane (Lane::Rhythm); }),
        &button ("Random lane", [this] { tool ([this] (Pattern& q) { gen::randomLane (q, selected, (float) density->slider.getValue(), (uint32_t) random.nextInt()); }); }),
        &button ("Random rhythm", [this] { tool ([this] (Pattern& q) { gen::randomRhythm (q, (float) density->slider.getValue(), 0.12f, (uint32_t) random.nextInt()); }); selectLane (Lane::Rhythm); }),
    };
    genRow2 = {
        &button ("Markov melody", [this] { tool ([this] (Pattern& q) { gen::markovMelody (q, (float) mStay->slider.getValue(), (float) mStep->slider.getValue(), (float) mLeap->slider.getValue(), (uint32_t) random.nextInt()); }); selectLane (Lane::Semitone); }),
        &button ("Markov rhythm", [this] { tool ([this] (Pattern& q) { const float d = (float) density->slider.getValue(); gen::markovRhythm (q, juce::jlimit (0.05f, 0.95f, d + 0.1f), juce::jlimit (0.05f, 0.95f, d), (uint32_t) random.nextInt()); }); selectLane (Lane::Rhythm); }),
        &button ("Mutate", [this] { tool ([this] (Pattern& q) { gen::mutate (q, (float) mutateAmt->slider.getValue(), (uint32_t) random.nextInt()); }); }),
        &button ("Surprise me", [this] { tool ([this] (Pattern& q) { gen::surprise (q, (uint32_t) random.nextInt()); }); }),
        &button ("Reset all", [this] { tool ([] (Pattern& q) { const auto chord = q.chordMemory; q = Pattern(); q.chordMemory = chord; }); }),
    };

    addAndMakeVisible (statusLabel);
    statusLabel.setFont (juce::Font (juce::FontOptions (11.5f)));
    statusLabel.setText ("Click steps to edit. Shift-click sets the lane length. Right-click resets a step.", juce::dontSendNotification);

    rebuildPresetMenu();
    themeBox.setSelectedId (juce::jlimit (0, themes().size() - 1, proc.themeIndex) + 1, juce::sendNotificationSync);
    selectLane (Lane::Rhythm);
    refreshChordLabel();

    setSize (kWidth, kHeight);
    setResizable (true, true);
    setResizeLimits (kWidth * 3 / 4, kHeight * 3 / 4, kWidth * 2, kHeight * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kWidth / kHeight);
    lastActivity = proc.outputActivity.load();
    startTimerHz (30);
}

KaleidoEditor::~KaleidoEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void KaleidoEditor::applyTheme (int index)
{
    if (! juce::isPositiveAndBelow (index, themes().size())) return;
    proc.themeIndex = index;
    lnf.setTheme (themes()[index]);
    for (auto& [id, k] : knobs) k->label.setColour (juce::Label::textColourId, theme().dim);
    for (auto& [id, c] : choices) c->label.setColour (juce::Label::textColourId, theme().dim);
    for (auto& s : genSliders) s->label.setColour (juce::Label::textColourId, theme().dim);
    lengthLabel.setColour (juce::Label::textColourId, theme().dim);
    statusLabel.setColour (juce::Label::textColourId, theme().dim);
    chordLabel.setColour (juce::Label::textColourId, theme().text);
    sendLookAndFeelChange();
    repaint();
}

void KaleidoEditor::selectLane (Lane l)
{
    selected = l;
    for (int i = 0; i < laneTabs.size(); ++i) laneTabs[i]->setToggleState (i == (int) l, juce::dontSendNotification);
    lengthSlider.setValue (pat.lane (l).length, juce::dontSendNotification);
    overview.repaint();
    laneEditor.repaint();
}

void KaleidoEditor::beginEdit()
{
    history.push_back (pat);
    if (history.size() > 64) history.erase (history.begin());
}

void KaleidoEditor::commit()
{
    proc.setPattern (pat);
    proc.presetName = proc.presetName.endsWith ("*") ? proc.presetName : proc.presetName + " *";
    overview.repaint();
    laneEditor.repaint();
}

void KaleidoEditor::undo()
{
    if (history.empty()) return;
    pat = history.back();
    history.pop_back();
    commit();
    selectLane (selected);
}

void KaleidoEditor::tool (std::function<void (Pattern&)> fn)
{
    beginEdit();
    fn (pat);
    commit();
    selectLane (selected);
}

void KaleidoEditor::refreshChordLabel()
{
    juce::String s = "Chord: ";
    if (pat.chordMemory.empty()) s << "(none, hold notes + Learn)";
    for (size_t i = 0; i < pat.chordMemory.size(); ++i)
        s << (i ? " " : "") << noteName (pat.chordMemory[i]) << (pat.chordMemory[i] >= 12 ? "'" : "");
    chordLabel.setText (s, juce::dontSendNotification);
}

void KaleidoEditor::rebuildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    presetIds.clear();
    std::string lastCat;
    const auto& fp = factoryPresets();
    for (size_t i = 0; i < fp.size(); ++i)
    {
        if (fp[i].category != lastCat) { presetBox.addSectionHeading (fp[i].category); lastCat = fp[i].category; }
        presetBox.addItem (fp[i].name, (int) i + 1);
        presetIds.push_back ((int) i + 1);
    }
    userFiles = proc.userPresetFiles();
    if (! userFiles.isEmpty())
    {
        presetBox.addSectionHeading ("User");
        for (int i = 0; i < userFiles.size(); ++i)
        {
            presetBox.addItem (userFiles[i].getFileNameWithoutExtension(), 10000 + i);
            presetIds.push_back (10000 + i);
        }
    }
    presetBox.setText (proc.presetName, juce::dontSendNotification);
}

void KaleidoEditor::presetChosen()
{
    const int id = presetBox.getSelectedId();
    if (id <= 0) return;
    beginEdit();
    if (id >= 10000)
    {
        if (! proc.loadPresetFile (userFiles[id - 10000]))
            statusLabel.setText ("Could not read that preset file.", juce::dontSendNotification);
    }
    else
        proc.setCurrentProgram (id - 1);
    pat = proc.getPattern();
    selectLane (selected);
    refreshChordLabel();
    statusLabel.setText ("Loaded " + proc.presetName, juce::dontSendNotification);
}

void KaleidoEditor::stepPreset (int delta)
{
    if (presetIds.empty()) return;
    const auto it = std::find (presetIds.begin(), presetIds.end(), presetBox.getSelectedId());
    int idx = it == presetIds.end() ? (delta > 0 ? -1 : 0) : (int) (it - presetIds.begin());
    idx = (idx + delta + (int) presetIds.size()) % (int) presetIds.size();
    presetBox.setSelectedId (presetIds[(size_t) idx], juce::sendNotificationSync);
}

void KaleidoEditor::savePresetDialog()
{
    saveWindow = std::make_unique<juce::AlertWindow> ("Save preset", "Name for this preset:", juce::MessageBoxIconType::NoIcon, this);
    saveWindow->addTextEditor ("name", proc.presetName.trimCharactersAtEnd (" *"));
    saveWindow->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveWindow->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        if (result == 1 && saveWindow != nullptr)
        {
            const auto name = saveWindow->getTextEditorContents ("name");
            const bool ok = proc.saveUserPreset (name);
            statusLabel.setText (ok ? "Saved to " + KaleidoProcessor::userPresetFolder().getFullPathName()
                                    : juce::String ("Could not save the preset."), juce::dontSendNotification);
            rebuildPresetMenu();
        }
        saveWindow.reset();
    }));
}

void KaleidoEditor::timerCallback()
{
    static int lastVersion = -1;
    const int v = proc.patternVersion.load();
    if (v != lastVersion)
    {
        lastVersion = v;
        const auto fresh = proc.getPattern();
        if (! (fresh == pat)) { pat = fresh; selectLane (selected); }
        refreshChordLabel();
    }
    if (proc.chordLearned.exchange (false))
        statusLabel.setText ("Chord learned. Turn on Chord memory to play it from one key.", juce::dontSendNotification);

    const int held = proc.engine().uiHeldCount.load();
    const int step = held > 0 ? proc.engine().uiStepCounter.load() : -1;
    if (step != playStep) { playStep = step; overview.repaint(); laneEditor.repaint(); }

    const int act = proc.outputActivity.load();
    if (act != lastActivity) { lastActivity = act; activityGlow = 1.0f; }
    else activityGlow *= 0.8f;
    if (activityGlow > 0.01f || activityGlow < 0.0f) repaint (headerArea);

    if (! presetBox.isPopupActive() && presetBox.getText() != proc.presetName)
        presetBox.setText (proc.presetName, juce::dontSendNotification);
}

// ---- layout -------------------------------------------------------------------------------

void KaleidoEditor::layoutKnobRow (juce::Rectangle<int> r, std::initializer_list<Knob*> ks)
{
    const int w = r.getWidth() / (int) ks.size();
    for (auto* k : ks)
    {
        auto cell = r.removeFromLeft (w);
        k->label.setBounds (cell.removeFromTop (16));
        k->slider.setBounds (cell);
    }
}

void KaleidoEditor::resized()
{
    // Lay out in the design size, then scale everything uniformly.
    const float scale = getWidth() / (float) kWidth;
    auto full = juce::Rectangle<int> (0, 0, kWidth, kHeight);

    headerArea = full.removeFromTop (52).reduced (12, 10);
    {
        auto h = headerArea;
        h.removeFromLeft (190);   // title
        enableBtn.setBounds (h.removeFromLeft (80));
        h.removeFromLeft (14);
        prevBtn.setBounds (h.removeFromLeft (28));
        h.removeFromLeft (4);
        presetBox.setBounds (h.removeFromLeft (300));
        h.removeFromLeft (4);
        nextBtn.setBounds (h.removeFromLeft (28));
        h.removeFromLeft (8);
        saveBtn.setBounds (h.removeFromLeft (60));
        h.removeFromLeft (6);
        undoBtn.setBounds (h.removeFromLeft (60));
        themeBox.setBounds (h.removeFromRight (140));
        h.removeFromRight (8);
    }

    full.reduce (12, 0);
    auto left = full.removeFromLeft (360);
    full.removeFromLeft (12);
    auto right = full;

    // PLAY
    playArea = left.removeFromTop (262);
    {
        auto r = playArea.reduced (12, 8);
        r.removeFromTop (18);
        auto comboRow = [&] (Choice& a, Choice& b)
        {
            auto row = r.removeFromTop (44);
            auto ca = row.removeFromLeft (row.getWidth() / 2).reduced (0, 0).withTrimmedRight (6);
            a.label.setBounds (ca.removeFromTop (16)); a.box.setBounds (ca.withHeight (24));
            auto cb = row.withTrimmedLeft (6);
            b.label.setBounds (cb.removeFromTop (16)); b.box.setBounds (cb.withHeight (24));
        };
        comboRow (*choices["mode"], *choices["rate"]);
        comboRow (*choices["octaveMode"], *choices["velSource"]);
        r.removeFromTop (4);
        layoutKnobRow (r.removeFromTop (92), { knobs["octaves"].get(), knobs["swing"].get(), knobs["gate"].get(), knobs["noteRepeat"].get(), knobs["strum"].get() });
        auto tr = r.removeFromTop (26);
        toggles["latch"]->button.setBounds (tr.removeFromLeft (80));
        toggles["retrigger"]->button.setBounds (tr.removeFromLeft (100));
        toggles["advanceOnRest"]->button.setBounds (tr);
    }
    left.removeFromTop (10);

    // PITCH & FEEL
    pitchArea = left.removeFromTop (262);
    {
        auto r = pitchArea.reduced (12, 8);
        r.removeFromTop (18);
        auto row = r.removeFromTop (44);
        auto ca = row.removeFromLeft (110).withTrimmedRight (6);
        choices["scaleRoot"]->label.setBounds (ca.removeFromTop (16)); choices["scaleRoot"]->box.setBounds (ca.withHeight (24));
        auto cb = row.withTrimmedLeft (6);
        choices["scale"]->label.setBounds (cb.removeFromTop (16)); choices["scale"]->box.setBounds (cb.withHeight (24));
        r.removeFromTop (4);
        layoutKnobRow (r.removeFromTop (92), { knobs["transpose"].get(), knobs["rangeLow"].get(), knobs["rangeHigh"].get(), knobs["humTime"].get(), knobs["humVel"].get() });
        auto tr = r.removeFromTop (26);
        toggles["degreeMode"]->button.setBounds (tr.removeFromLeft (170));
        toggles["chordMemory"]->button.setBounds (tr);
        r.removeFromTop (6);
        auto lr = r.removeFromTop (26);
        learnBtn->setBounds (lr.removeFromLeft (100));
        lr.removeFromLeft (8);
        chordLabel.setBounds (lr);
    }
    left.removeFromTop (10);

    // OUTPUT
    outArea = left;
    {
        auto r = outArea.reduced (12, 8);
        r.removeFromTop (16);
        auto col = r.removeFromLeft (150);
        auto& oc = *choices["outChannel"];
        oc.label.setBounds (col.removeFromTop (16));
        oc.box.setBounds (col.removeFromTop (24));
        col.removeFromTop (4);
        toggles["sync"]->button.setBounds (col.removeFromTop (24));
        toggles["preview"]->button.setBounds (col.removeFromTop (24));
        r.removeFromLeft (10);
        layoutKnobRow (r, { knobs["bpm"].get(), knobs["previewLevel"].get() });
    }

    // LANES
    laneArea = right.removeFromTop (452);
    {
        auto r = laneArea.reduced (12, 8);
        r.removeFromTop (18);
        auto tabs = r.removeFromTop (26);
        const int tw = tabs.getWidth() / kNumLanes;
        for (auto* t : laneTabs) t->setBounds (tabs.removeFromLeft (tw).reduced (2, 0));
        r.removeFromTop (6);
        overview.setBounds (r.removeFromTop (112));
        r.removeFromTop (6);
        auto bottom = r.removeFromBottom (28);
        r.removeFromBottom (4);
        auto lenRow = r.removeFromBottom (24);
        laneEditor.setBounds (r);
        lengthLabel.setBounds (lenRow.removeFromLeft (50));
        lengthSlider.setBounds (lenRow.removeFromLeft (260));
        const int bw = bottom.getWidth() / (int) toolRow.size();
        for (auto* b : toolRow) b->setBounds (bottom.removeFromLeft (bw).reduced (2, 0));
    }
    right.removeFromTop (10);

    // GENERATE
    genArea = right;
    {
        auto r = genArea.reduced (12, 8);
        r.removeFromTop (18);
        statusLabel.setBounds (r.removeFromBottom (20));
        auto placeSliders = [] (juce::Rectangle<int> row, std::initializer_list<GenSlider*> ss)
        {
            const int w = row.getWidth() / (int) ss.size();
            for (auto* s : ss)
            {
                auto c = row.removeFromLeft (w).reduced (4, 0);
                s->label.setBounds (c.removeFromLeft (54));
                s->slider.setBounds (c);
            }
        };
        auto placeButtons = [] (juce::Rectangle<int> row, const std::vector<juce::TextButton*>& bs)
        {
            const int w = row.getWidth() / (int) bs.size();
            for (auto* b : bs) b->setBounds (row.removeFromLeft (w).reduced (3, 0));
        };
        placeSliders (r.removeFromTop (26), { eHits, eSteps, eRot, density });
        r.removeFromTop (6);
        placeButtons (r.removeFromTop (28), genRow1);
        r.removeFromTop (12);
        placeSliders (r.removeFromTop (26), { mStay, mStep, mLeap, mutateAmt });
        r.removeFromTop (6);
        placeButtons (r.removeFromTop (28), genRow2);
    }

    // Uniform scaling: every child was laid out at design size.
    for (auto* c : getChildren())
        c->setTransform (juce::AffineTransform::scale (scale));
    headerArea = headerArea.transformedBy (juce::AffineTransform::scale (scale));
}

void KaleidoEditor::paint (juce::Graphics& g)
{
    const auto& t = theme();
    g.fillAll (t.bg);
    const float scale = getWidth() / (float) kWidth;

    {
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::scale (scale));

        // Kaleidoscope mark + title
        const juce::Point<float> c (32.0f, 26.0f);
        for (int i = 0; i < 6; ++i)
        {
            juce::Path petal;
            petal.addEllipse (-4.0f, -14.0f, 8.0f, 14.0f);
            g.setColour ((i % 2 ? t.accent2 : t.accent).withAlpha (0.85f));
            g.fillPath (petal, juce::AffineTransform::rotation (i * juce::MathConstants<float>::pi / 3.0f).translated (c));
        }
        g.setColour (t.playhead.withAlpha (0.4f + 0.6f * activityGlow));
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (c));
        g.setColour (t.text);
        g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
        g.drawText ("KALEIDO", 54, 12, 140, 28, juce::Justification::centredLeft);

        sectionFrame (g, playArea, t, "Play");
        sectionFrame (g, pitchArea, t, "Pitch & Feel");
        sectionFrame (g, outArea, t, "Output");
        sectionFrame (g, laneArea, t, "Step Lanes");
        sectionFrame (g, genArea, t, "Generate");
    }
}
