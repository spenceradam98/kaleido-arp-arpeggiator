#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace kal
{

struct Theme
{
    juce::String name;
    juce::Colour bg, panel, edge, text, dim, accent, accent2, cellOff, playhead;
};

const juce::Array<Theme>& themes();

class KaleidoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KaleidoLookAndFeel();
    void setTheme (const Theme& t);
    const Theme& theme() const { return current; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f)); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (12.5f)); }
    juce::Label* createSliderTextBox (juce::Slider&) override;

private:
    Theme current;
};

} // namespace kal
