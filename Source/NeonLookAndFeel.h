#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Neon
{
    using juce::Colour;
    inline const Colour bg0      { 0xff08080f };
    inline const Colour bg1      { 0xff141027 };
    inline const Colour panel    { 0x0dffffff };
    inline const Colour border   { 0x338c78ff };
    inline const Colour cyan     { 0xff22e4ff };
    inline const Colour magenta  { 0xffff3dcb };
    inline const Colour violet   { 0xff8a5cff };
    inline const Colour text     { 0xffeae8ff };
    inline const Colour dim      { 0xff8c88b4 };
    inline const Colour knobTop  { 0xff2b2652 };
    inline const Colour knobEdge { 0xff100e1e };

    /** Strokes a path several times with growing width / falling alpha = soft neon glow. */
    inline void glowStroke (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width)
    {
        const float widths[] = { width * 4.0f, width * 2.6f, width * 1.7f };
        const float alphas[] = { 0.06f, 0.12f, 0.22f };
        for (int i = 0; i < 3; ++i)
        {
            g.setColour (c.withMultipliedAlpha (alphas[i]));
            g.strokePath (p, juce::PathStrokeType (widths[i], juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius = 14.0f)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (r, radius);
        juce::ColourGradient edge (violet.withAlpha (0.45f), r.getX(), r.getY(),
                                   cyan.withAlpha (0.12f), r.getRight(), r.getBottom(), false);
        g.setGradientFill (edge);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    }
}

class NeonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NeonLookAndFeel()
    {
        using namespace juce;
        setColour (Slider::textBoxTextColourId, Neon::text);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        setColour (Slider::textBoxHighlightColourId, Neon::violet.withAlpha (0.4f));
        setColour (Label::textColourId, Neon::text);
        setColour (ComboBox::textColourId, Neon::text);
        setColour (ComboBox::backgroundColourId, Colour (0xff15122b));
        setColour (ComboBox::outlineColourId, Neon::border);
        setColour (ComboBox::arrowColourId, Neon::cyan);
        setColour (PopupMenu::backgroundColourId, Colour (0xf0110f22));
        setColour (PopupMenu::textColourId, Neon::text);
        setColour (PopupMenu::highlightedBackgroundColourId, Neon::violet.withAlpha (0.35f));
        setColour (PopupMenu::highlightedTextColourId, Colours::white);
        setColour (TextEditor::backgroundColourId, Colour (0xff15122b));
        setColour (TextEditor::textColourId, Neon::text);
        setColour (TextEditor::highlightColourId, Neon::violet.withAlpha (0.4f));
        setColour (CaretComponent::caretColourId, Neon::cyan);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace juce;
        const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
        const float size = jmin (bounds.getWidth(), bounds.getHeight());
        const auto r = bounds.withSizeKeepingCentre (size, size);
        const auto c = r.getCentre();
        const float arcR = size * 0.5f - 4.0f;
        const float angle = startAngle + pos * (endAngle - startAngle);
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float zeroAngle = bipolar ? startAngle + (float) s.valueToProportionOfLength (0.0) * (endAngle - startAngle) : startAngle;
        const float hover = s.isMouseOverOrDragging() ? 1.0f : 0.0f;

        // track
        Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (Colour (0xff221e3d));
        g.strokePath (track, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));

        // value arc with neon glow
        if (std::abs (angle - zeroAngle) > 0.001f)
        {
            Path value;
            value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, jmin (zeroAngle, angle), jmax (zeroAngle, angle), true);
            const Colour a = Neon::cyan, b = Neon::magenta;
            const Colour col = a.interpolatedWith (b, pos);
            Neon::glowStroke (g, value, col.withMultipliedBrightness (1.0f + 0.15f * hover), 4.0f);
        }

        // body
        const float bodyR = arcR - 9.0f;
        ColourGradient body (Neon::knobTop.brighter (0.15f * hover), c.x - bodyR * 0.4f, c.y - bodyR * 0.6f,
                             Neon::knobEdge, c.x + bodyR * 0.5f, c.y + bodyR, true);
        g.setGradientFill (body);
        g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2);
        g.setColour (Colours::white.withAlpha (0.08f));
        g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2, 1.0f);

        // pointer dot
        const float pr = bodyR - 7.0f;
        const Point<float> p (c.x + pr * std::sin (angle), c.y - pr * std::cos (angle));
        const Colour pc = Neon::cyan.interpolatedWith (Neon::magenta, pos);
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (pc.withAlpha (0.12f * (4 - i)));
            g.fillEllipse (p.x - i * 2.5f, p.y - i * 2.5f, i * 5.0f, i * 5.0f);
        }
        g.setColour (Colours::white);
        g.fillEllipse (p.x - 2.2f, p.y - 2.2f, 4.4f, 4.4f);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (juce::FontOptions (13.0f));
        l->setJustificationType (juce::Justification::centred);
        return l;
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace juce;
        auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (findColour (ComboBox::backgroundColourId).brighter (box.isMouseOver() ? 0.08f : 0.0f));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (box.isMouseOver() ? Neon::cyan.withAlpha (0.6f) : Neon::border);
        g.drawRoundedRectangle (r, 8.0f, 1.0f);

        Path arrow;
        const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
        arrow.startNewSubPath (ax - 4, ay - 2); arrow.lineTo (ax, ay + 2); arrow.lineTo (ax + 4, ay - 2);
        g.setColour (Neon::cyan);
        g.strokePath (arrow, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
        label.setFont (juce::FontOptions (14.0f));
    }

    juce::Font getPopupMenuFont() override { return juce::FontOptions (14.0f); }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
        g.setColour (Neon::border);
        g.drawRect (0, 0, w, h);
    }
};
