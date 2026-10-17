#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Studio graphite theme: dark grey surfaces, violet accents, restrained glow.
// (Namespace name kept from the first version so the rest of the code stays the same.)
namespace Neon
{
    using juce::Colour;
    inline const Colour bg0      { 0xff101115 };
    inline const Colour bg1      { 0xff1a1b21 };
    inline const Colour panel    { 0xff1c1e24 };
    inline const Colour panelTop { 0xff22242b };
    inline const Colour border   { 0xff2d3039 };
    inline const Colour cyan     { 0xffc6b8ff };   // light lilac accent (output, highlights)
    inline const Colour magenta  { 0xff7457ff };   // deep violet accent
    inline const Colour violet   { 0xff9a82ff };   // main violet
    inline const Colour grey     { 0xff70737f };   // input pitch / inactive
    inline const Colour text     { 0xffe8e9ee };
    inline const Colour dim      { 0xff8c8f9b };
    inline const Colour knobTop  { 0xff3a3c45 };
    inline const Colour knobEdge { 0xff1a1b20 };

    /** Main stroke plus one faint halo — a soft studio glow, not neon. */
    inline void glowStroke (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width)
    {
        g.setColour (c.withMultipliedAlpha (0.14f));
        g.strokePath (p, juce::PathStrokeType (width * 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius = 10.0f)
    {
        juce::ColourGradient fill (panelTop, 0, r.getY(), panel, 0, r.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (r, radius);
        g.setColour (border);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.035f));   // top bevel highlight
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + radius, r.getRight() - radius);
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
        setColour (ComboBox::backgroundColourId, Colour (0xff23252c));
        setColour (ComboBox::outlineColourId, Neon::border);
        setColour (ComboBox::arrowColourId, Neon::violet);
        setColour (PopupMenu::backgroundColourId, Colour (0xf81d1f25));
        setColour (PopupMenu::textColourId, Neon::text);
        setColour (PopupMenu::highlightedBackgroundColourId, Neon::magenta.withAlpha (0.45f));
        setColour (PopupMenu::highlightedTextColourId, Colours::white);
        setColour (TextEditor::backgroundColourId, Colour (0xff23252c));
        setColour (TextEditor::textColourId, Neon::text);
        setColour (TextEditor::highlightColourId, Neon::violet.withAlpha (0.4f));
        setColour (CaretComponent::caretColourId, Neon::violet);
        setColour (TooltipWindow::backgroundColourId, Colour (0xff23252c));
        setColour (TooltipWindow::textColourId, Neon::text);
        setColour (TooltipWindow::outlineColourId, Neon::border);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace juce;
        const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float size = jmin (bounds.getWidth(), bounds.getHeight());
        const auto r = bounds.withSizeKeepingCentre (size, size);
        const auto c = r.getCentre();
        const float arcW = jlimit (3.0f, 6.0f, size * 0.05f);
        const float arcR = size * 0.5f - arcW;
        const float angle = startAngle + pos * (endAngle - startAngle);
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float zeroAngle = bipolar ? startAngle + (float) s.valueToProportionOfLength (0.0) * (endAngle - startAngle) : startAngle;
        const bool hover = s.isMouseOverOrDragging();

        // track
        Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (Colour (0xff2a2c34));
        g.strokePath (track, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));

        // value arc
        if (std::abs (angle - zeroAngle) > 0.001f)
        {
            Path value;
            value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, jmin (zeroAngle, angle), jmax (zeroAngle, angle), true);
            const Colour col = Neon::magenta.interpolatedWith (Neon::cyan, 0.25f + 0.5f * pos);
            Neon::glowStroke (g, value, hover ? col.brighter (0.15f) : col, arcW);
        }

        // matte body with a soft top light
        const float bodyR = arcR - arcW * 2.2f;
        g.setColour (Colours::black.withAlpha (0.35f));                        // drop shadow
        g.fillEllipse (c.x - bodyR, c.y - bodyR + 3.0f, bodyR * 2, bodyR * 2);
        ColourGradient body (Neon::knobTop.brighter (hover ? 0.08f : 0.0f), c.x, c.y - bodyR,
                             Neon::knobEdge, c.x, c.y + bodyR, false);
        g.setGradientFill (body);
        g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2);
        g.setColour (Colours::white.withAlpha (0.07f));
        g.drawEllipse (c.x - bodyR + 0.5f, c.y - bodyR + 0.5f, bodyR * 2 - 1, bodyR * 2 - 1, 1.0f);

        // indicator line
        const float r1 = bodyR * 0.45f, r2 = bodyR * 0.86f;
        const float sx = std::sin (angle), cy = -std::cos (angle);
        Path ind;
        ind.startNewSubPath (c.x + r1 * sx, c.y + r1 * cy);
        ind.lineTo (c.x + r2 * sx, c.y + r2 * cy);
        g.setColour (Neon::cyan);
        g.strokePath (ind, PathStrokeType (jmax (2.0f, size * 0.025f), PathStrokeType::curved, PathStrokeType::rounded));
    }

    /** Header value bars (Transpose, A4): a flat pill with a violet fill and the value written on it. */
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        using namespace juce;
        if (style != Slider::LinearBar) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, 0, 0, 0, style, s); return; }
        auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (0.5f);
        g.setColour (findColour (ComboBox::backgroundColourId).brighter (s.isMouseOverOrDragging() ? 0.06f : 0.0f));
        g.fillRoundedRectangle (r, 6.0f);

        const double prop = s.valueToProportionOfLength (s.getValue());
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const double zero = bipolar ? s.valueToProportionOfLength (0.0) : 0.0;
        auto fill = r.reduced (2.0f);
        const float x0 = fill.getX() + fill.getWidth() * (float) jmin (zero, prop);
        const float x1 = fill.getX() + fill.getWidth() * (float) jmax (zero, prop);
        g.setColour (Neon::magenta.withAlpha (0.55f));
        g.fillRoundedRectangle (Rectangle<float> (x0, fill.getY(), jmax (0.0f, x1 - x0), fill.getHeight()), 4.0f);

        g.setColour (s.isMouseOverOrDragging() ? Neon::violet : Neon::border);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        g.setColour (Neon::text);
        g.setFont (FontOptions (13.5f));
        g.drawText (s.getTextFromValue (s.getValue()), r, Justification::centred);
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
        g.setColour (findColour (ComboBox::backgroundColourId).brighter (box.isMouseOver() ? 0.06f : 0.0f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (box.isMouseOver() ? Neon::violet : Neon::border);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);

        Path arrow;
        const float ax = (float) w - 15.0f, ay = (float) h * 0.5f;
        arrow.startNewSubPath (ax - 4, ay - 2); arrow.lineTo (ax, ay + 2); arrow.lineTo (ax + 4, ay - 2);
        g.setColour (Neon::violet);
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
