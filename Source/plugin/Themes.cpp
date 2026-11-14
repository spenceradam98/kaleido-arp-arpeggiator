#include "Themes.h"

namespace kal
{

const juce::Array<Theme>& themes()
{
    using C = juce::Colour;
    static const juce::Array<Theme> t = [] {
        juce::Array<Theme> a;
        const std::initializer_list<Theme> list {
        //       name           bg                panel             edge              text              dim               accent            accent2           cellOff           playhead
        { "Kaleido",    C (0xff1a1027), C (0xff26173a), C (0xff3d2a5c), C (0xfff3e9ff), C (0xff9c88bd), C (0xffff4fb8), C (0xff3ce0d0), C (0xff311f49), C (0xffffd34f) },
        { "Midnight",   C (0xff0d1320), C (0xff151e30), C (0xff26324d), C (0xffe6eefc), C (0xff8796b3), C (0xff4fa3ff), C (0xff8b6cff), C (0xff1b2639), C (0xfff5f5f5) },
        { "Daylight",   C (0xfff3eee4), C (0xfffbf8f2), C (0xffd6cdbb), C (0xff2b2620), C (0xff877d6e), C (0xffe8642c), C (0xff2c8c7a), C (0xffe9e2d4), C (0xff2b2620) },
        { "Phosphor",   C (0xff050806), C (0xff0b120d), C (0xff1d3322), C (0xff9dff9d), C (0xff4f8a55), C (0xff39ff6a), C (0xffd0ff3a), C (0xff0f1c12), C (0xffffffff) },
        { "Tape Deck",  C (0xff221a14), C (0xff2e241b), C (0xff4a3a2b), C (0xfff2e3c8), C (0xffb39f80), C (0xffff9a3c), C (0xffd9c26a), C (0xff3a2d21), C (0xfffff0d0) },
        { "Paisley",    C (0xff201016), C (0xff2f1820), C (0xff5a2a3a), C (0xfffde6d8), C (0xffc69a9a), C (0xffffb347), C (0xffb06cff), C (0xff3d1f2a), C (0xff7cf2e0) },
        { "Mono",       C (0xff141414), C (0xff1e1e1e), C (0xff353535), C (0xffeeeeee), C (0xff8a8a8a), C (0xffffffff), C (0xffb0b0b0), C (0xff262626), C (0xffff5050) },
        };
        for (auto& th : list) a.add (th);
        return a;
    }();
    return t;
}

KaleidoLookAndFeel::KaleidoLookAndFeel() { setTheme (themes()[0]); }

void KaleidoLookAndFeel::setTheme (const Theme& t)
{
    current = t;
    setColour (juce::ResizableWindow::backgroundColourId, t.bg);
    setColour (juce::Label::textColourId, t.text);
    setColour (juce::Slider::textBoxTextColourId, t.text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, t.accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, t.edge);
    setColour (juce::Slider::trackColourId, t.accent);
    setColour (juce::Slider::backgroundColourId, t.cellOff);
    setColour (juce::Slider::thumbColourId, t.text);
    setColour (juce::ComboBox::backgroundColourId, t.cellOff);
    setColour (juce::ComboBox::textColourId, t.text);
    setColour (juce::ComboBox::outlineColourId, t.edge);
    setColour (juce::ComboBox::arrowColourId, t.dim);
    setColour (juce::PopupMenu::backgroundColourId, t.panel);
    setColour (juce::PopupMenu::textColourId, t.text);
    setColour (juce::PopupMenu::headerTextColourId, t.accent2);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, t.accent.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, t.text);
    setColour (juce::TextButton::buttonColourId, t.cellOff);
    setColour (juce::TextButton::buttonOnColourId, t.accent);
    setColour (juce::TextButton::textColourOffId, t.text);
    setColour (juce::TextButton::textColourOnId, t.bg);
    setColour (juce::ToggleButton::textColourId, t.text);
    setColour (juce::ToggleButton::tickColourId, t.accent);
    setColour (juce::TextEditor::backgroundColourId, t.cellOff);
    setColour (juce::TextEditor::textColourId, t.text);
    setColour (juce::TextEditor::outlineColourId, t.edge);
    setColour (juce::AlertWindow::backgroundColourId, t.panel);
    setColour (juce::AlertWindow::textColourId, t.text);
    setColour (juce::AlertWindow::outlineColourId, t.edge);
}

juce::Label* KaleidoLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::Font (juce::FontOptions (11.5f)));
    l->setColour (juce::Label::textColourId, current.dim);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

void KaleidoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float start, float end, juce::Slider&)
{
    const auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = start + pos * (end - start);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, start, end, true);
    g.setColour (current.edge);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path val;
    val.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, start, angle, true);
    g.setGradientFill (juce::ColourGradient (current.accent, c.x - r, c.y, current.accent2, c.x + r, c.y, false));
    g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (current.panel.brighter (0.08f));
    g.fillEllipse (juce::Rectangle<float> (r * 1.2f, r * 1.2f).withCentre (c));
    juce::Path tick;
    tick.addRoundedRectangle (-1.25f, -r * 0.6f, 2.5f, r * 0.38f, 1.0f);
    g.setColour (current.text);
    g.fillPath (tick, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void KaleidoLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto fill = b.getToggleState() ? current.accent : current.cellOff;
    if (over) fill = fill.brighter (0.12f);
    if (down) fill = fill.darker (0.15f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (b.getToggleState() ? current.accent : current.edge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void KaleidoLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft (r.getHeight()).reduced (4.0f);
    g.setColour (b.getToggleState() ? current.accent : current.cellOff);
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (over ? current.text : current.edge);
    g.drawRoundedRectangle (box, 3.0f, 1.0f);
    g.setColour (current.text);
    g.setFont (juce::Font (juce::FontOptions (12.5f)));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (3.0f), juce::Justification::centredLeft, true);
}

void KaleidoLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (current.cellOff);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? current.accent : current.edge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (current.dim);
    g.fillPath (arrow);
}

} // namespace kal
