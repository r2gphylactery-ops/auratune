#include "PluginEditor.h"

using namespace juce;

namespace
{
    const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    std::array<bool, 12> inScale (AuraTuneProcessor& p)
    {
        auto& s = p.apvts;
        return aura::scaleMask ((int) s.getRawParameterValue (ParamIDs::key)->load(),
                                (int) s.getRawParameterValue (ParamIDs::scale)->load());
    }
    bool isRemoved (AuraTuneProcessor& p, int pc)
    {
        return p.apvts.getRawParameterValue (ParamIDs::noteOff (pc))->load() > 0.5f;
    }
    int pcOf (int midi) { return ((midi % 12) + 12) % 12; }
}

// ============================================================================
NeonKnob::NeonKnob (AudioProcessorValueTreeState& s, const String& id, const String& caption, const String& tooltip)
    : attachment (s, id, slider)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxTextColourId, Neon::text);
    slider.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 80, 18);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setTooltip (tooltip);
    slider.setDoubleClickReturnValue (true, (double) s.getParameterRange (id).convertFrom0to1 (s.getParameter (id)->getDefaultValue()));
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (220);
    addAndMakeVisible (slider);

    label.setText (caption.toUpperCase(), dontSendNotification);
    label.setJustificationType (Justification::centred);
    label.setFont (FontOptions (11.5f, Font::bold));
    label.setColour (Label::textColourId, Neon::dim);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
}

void NeonKnob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

// ============================================================================
ValueBar::ValueBar (AudioProcessorValueTreeState& s, const String& id, const String& caption, const String& tooltip)
    : attachment (s, id, slider)
{
    slider.setSliderStyle (Slider::LinearBar);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setTooltip (tooltip);
    slider.setDoubleClickReturnValue (true, (double) s.getParameterRange (id).convertFrom0to1 (s.getParameter (id)->getDefaultValue()));
    slider.setMouseDragSensitivity (300);
    addAndMakeVisible (slider);
    label.setText (caption.toUpperCase(), dontSendNotification);
    label.setFont (FontOptions (11.0f, Font::bold));
    label.setColour (Label::textColourId, Neon::dim);
    label.setJustificationType (Justification::bottomLeft);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
}

void ValueBar::resized()
{
    auto r = getLocalBounds();
    const int lh = jmax (12, r.getHeight() * 14 / 44);
    label.setBounds (r.removeFromTop (lh));
    slider.setBounds (r.withTrimmedTop (2));
}

// ============================================================================
void NoteDisplay::tick()
{
    const float in  = proc.engine.inputMidi.load();
    const float out = proc.engine.outputMidi.load();
    const int   t   = proc.engine.targetNote.load();
    const bool voiced = in > 0.0f && t >= 0;
    const float twoPi = MathConstants<float>::twoPi;

    activity += ((voiced ? 1.0f : 0.0f) - activity) * 0.18f;
    phase += 0.06f;
    pop *= 0.88f;
    sweep = std::fmod (sweep + 0.012f + 0.03f * activity, twoPi);   // faster when singing

    if (voiced)
    {
        if (t != note)
        {
            pop = 1.0f;
            // a little burst of sparks on every new note
            for (int k = 0; k < 10; ++k)
                for (auto& p : particles)
                    if (p.life <= 0.0f)
                    {
                        const float ang = rng.nextFloat() * twoPi, sp = 0.006f + 0.012f * rng.nextFloat();
                        p = { 0.0f, 0.0f, sp * std::sin (ang), -sp * std::cos (ang), 1.0f, 0.6f + rng.nextFloat() };
                        break;
                    }
        }
        note = t;
        shownIn  += ((in  - (float) t) - shownIn)  * 0.3f;
        shownOut += (((out > 0 ? out : in) - (float) t) - shownOut) * 0.3f;

        const float targetAngle = std::fmod (in, 12.0f) / 12.0f * twoPi;
        float d = targetAngle - angle;
        while (d >  MathConstants<float>::pi) d -= twoPi;
        while (d < -MathConstants<float>::pi) d += twoPi;
        angle += d * 0.35f;

        // sparks shed by the comet head
        if (rng.nextFloat() < 0.7f)
            for (auto& p : particles)
                if (p.life <= 0.0f)
                {
                    const float r = 0.93f, out2 = 0.002f + 0.004f * rng.nextFloat();
                    const float jitter = (rng.nextFloat() - 0.5f) * 0.004f;
                    p = { r * std::sin (angle), -r * std::cos (angle),
                          out2 * std::sin (angle) + jitter, -out2 * std::cos (angle) + jitter,
                          0.8f + 0.2f * rng.nextFloat(), 0.4f + 0.8f * rng.nextFloat() };
                    break;
                }
    }
    for (auto& p : particles)
        if (p.life > 0.0f) { p.x += p.vx; p.y += p.vy; p.vx *= 0.97f; p.vy *= 0.97f; p.life -= 0.025f; }

    for (int i = 13; i > 0; --i) tail[i] = tail[i - 1];
    tail[0] = angle;
    repaint();
}

void NoteDisplay::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    Neon::drawPanel (g, bounds);

    g.setColour (Neon::dim);
    g.setFont (FontOptions (11.5f, Font::bold));
    g.drawText ("TARGET NOTE", bounds.reduced (16, 12).removeFromTop (14), Justification::centredLeft);

    auto area = bounds.reduced (14, 12).withTrimmedTop (10).withTrimmedBottom (20);
    const float R = jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const auto c = area.getCentre();
    const float a = 0.3f + 0.7f * activity;
    const float breathe = 0.5f + 0.5f * std::sin (phase);
    const float pi = MathConstants<float>::pi, twoPi = MathConstants<float>::twoPi;
    auto polar = [&] (float r, float ang) { return Point<float> (c.x + r * std::sin (ang), c.y - r * std::cos (ang)); };
    auto glowDot = [&] (Point<float> p, float rad, Colour col, float alpha)
    {
        ColourGradient gr (col.withAlpha (alpha), p.x, p.y, col.withAlpha (0.0f), p.x + rad, p.y, true);
        g.setGradientFill (gr);
        g.fillEllipse (p.x - rad, p.y - rad, rad * 2, rad * 2);
    };

    // ---- aura: two breathing halos ------------------------------------------------
    glowDot (c, R * 1.15f, Neon::magenta, 0.06f + 0.20f * activity * (0.55f + 0.45f * breathe));
    glowDot (c, R * 0.98f, Neon::cyan,    0.03f + 0.10f * activity * breathe);

    // ---- metallic bezel + rotating light sweep -------------------------------------
    {
        ColourGradient bez (Colour (0xff5a5d6a), c.x, c.y - R, Colour (0xff1b1c22), c.x, c.y + R, false);
        g.setGradientFill (bez);
        g.drawEllipse (c.x - R + 0.5f, c.y - R + 0.5f, R * 2 - 1, R * 2 - 1, 1.5f);

        const int slices = 36;
        const float len = 1.6f;   // radians of trail
        for (int i = 0; i < slices; ++i)
        {
            const float a0 = sweep - len * (i + 1) / slices, a1 = sweep - len * i / slices;
            Path arc;
            arc.addCentredArc (c.x, c.y, R - 0.5f, R - 0.5f, 0.0f, a0, a1, true);
            const float k = 1.0f - (float) i / slices;
            g.setColour (Neon::cyan.withAlpha ((0.15f + 0.55f * activity) * k * k));
            g.strokePath (arc, PathStrokeType (2.0f + 2.0f * k));
        }
        glowDot (polar (R - 0.5f, sweep), 10.0f + 6.0f * activity, Colours::white, 0.35f + 0.4f * activity);
    }

    // ---- glass disc ---------------------------------------------------------------
    const float discR = R * 0.86f;
    {
        ColourGradient disc (Colour (0xff272a33), c.x, c.y - discR * 0.7f, Colour (0xff111216), c.x, c.y + discR, false);
        g.setGradientFill (disc);
        g.fillEllipse (c.x - discR, c.y - discR, discR * 2, discR * 2);

        // inner violet light from below, like a lit glass lens
        ColourGradient under (Neon::magenta.withAlpha (0.10f + 0.12f * activity), c.x, c.y + discR * 0.9f,
                              Neon::magenta.withAlpha (0.0f), c.x, c.y - discR * 0.1f, false);
        g.setGradientFill (under);
        g.fillEllipse (c.x - discR, c.y - discR, discR * 2, discR * 2);

        // glossy reflection on top
        Graphics::ScopedSaveState ss (g);
        Path clip; clip.addEllipse (c.x - discR, c.y - discR, discR * 2, discR * 2);
        g.reduceClipRegion (clip);
        ColourGradient gloss (Colours::white.withAlpha (0.09f), c.x, c.y - discR, Colours::white.withAlpha (0.0f), c.x, c.y - discR * 0.15f, false);
        g.setGradientFill (gloss);
        g.fillEllipse (c.x - discR * 0.9f, c.y - discR * 1.25f, discR * 1.8f, discR * 1.2f);

        g.setColour (Colours::white.withAlpha (0.08f));
        g.drawEllipse (c.x - discR, c.y - discR, discR * 2, discR * 2, 1.0f);
    }

    // ---- fine dial ticks that light up around the live pitch ------------------------
    for (int i = 0; i < 72; ++i)
    {
        const float ang = i * twoPi / 72.0f;
        float d = std::abs (ang - angle);
        d = jmin (d, twoPi - d);
        const float lit = activity * std::exp (-d * d / 0.05f);
        const bool major = i % 6 == 0;
        g.setColour (Neon::cyan.withAlpha (0.10f + (major ? 0.10f : 0.0f) + 0.8f * lit));
        g.drawLine ({ polar (discR - (major ? 7.0f : 4.0f) - 4.0f * lit, ang), polar (discR - 1.5f, ang) }, major ? 1.3f : 1.0f);
    }

    // ---- 12 note segments -------------------------------------------------------------
    const auto scale = inScale (proc);
    const int targetPc = note >= 0 ? pcOf (note) : -1;
    const float segR = R * 0.93f, segW = jmax (4.0f, R * 0.055f);
    const float step = twoPi / 12.0f, gap = 0.035f;
    for (int pc = 0; pc < 12; ++pc)
    {
        const float a0 = (pc - 0.5f) * step + gap, a1 = (pc + 0.5f) * step - gap;
        Path seg;
        seg.addCentredArc (c.x, c.y, segR, segR, 0.0f, a0, a1, true);
        const bool on = scale[(size_t) pc] && ! isRemoved (proc, pc);
        if (pc == targetPc)
        {
            const float gl = a * (0.7f + 0.3f * breathe);
            g.setColour (Neon::magenta.withAlpha (0.10f * gl)); g.strokePath (seg, PathStrokeType (segW * 5.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (Neon::cyan.withAlpha (0.22f * gl));    g.strokePath (seg, PathStrokeType (segW * 2.6f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (Neon::cyan.withAlpha (a));             g.strokePath (seg, PathStrokeType (segW, PathStrokeType::curved, PathStrokeType::butt));
            g.setColour (Colours::white.withAlpha (0.8f * a));  g.strokePath (seg, PathStrokeType (segW * 0.35f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        else
        {
            g.setColour (on ? Neon::violet.withAlpha (0.42f) : Colours::white.withAlpha (0.06f));
            g.strokePath (seg, PathStrokeType (segW, PathStrokeType::curved, PathStrokeType::butt));
        }
        const auto lp = polar (discR - R * 0.17f, pc * step);
        g.setFont (FontOptions (jmax (8.5f, R * 0.075f), pc == targetPc ? Font::bold : Font::plain));
        g.setColour (pc == targetPc ? Neon::text.withAlpha (a) : on ? Neon::dim : Neon::dim.withAlpha (0.3f));
        g.drawText (noteNames[pc], Rectangle<float> (lp.x - 14, lp.y - 7, 28, 14), Justification::centred);
    }

    // ---- comet + sparks ------------------------------------------------------------------
    if (activity > 0.02f)
    {
        for (int i = 13; i >= 1; --i)
        {
            const auto p0 = polar (segR, tail[i]), p1 = polar (segR, tail[i - 1]);
            const float k = 1.0f - i / 14.0f;
            g.setColour (Neon::cyan.withAlpha (activity * 0.6f * k));
            g.drawLine ({ p0, p1 }, segW * (0.4f + 0.8f * k));
        }
        const auto head = polar (segR, angle);
        glowDot (head, segW * 5.0f, Neon::magenta, 0.45f * activity);
        glowDot (head, segW * 2.6f, Neon::cyan, 0.75f * activity);
        g.setColour (Colours::white.withAlpha (activity));
        g.fillEllipse (head.x - segW * 0.55f, head.y - segW * 0.55f, segW * 1.1f, segW * 1.1f);
    }
    for (const auto& p : particles)
        if (p.life > 0.0f)
        {
            const Point<float> pp (c.x + p.x * R, c.y + p.y * R);
            const float rad = p.size * (1.0f + 1.5f * p.life);
            glowDot (pp, rad * 3.0f, Neon::cyan, 0.35f * p.life);
            g.setColour (Colours::white.withAlpha (0.8f * p.life));
            g.fillEllipse (pp.x - rad * 0.5f, pp.y - rad * 0.5f, rad, rad);
        }

    // ---- cents arc --------------------------------------------------------------------
    {
        const float ar = R * 0.6f, span = pi / 3.0f;
        Path track;
        track.addCentredArc (c.x, c.y, ar, ar, 0.0f, -span, span, true);
        g.setColour (Colours::black.withAlpha (0.35f));
        g.strokePath (track, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colour (0xff30323b));
        g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        for (int k = -2; k <= 2; ++k)
        {
            const float ang = k * span / 2.0f;
            g.setColour (Colours::white.withAlpha (k == 0 ? 0.5f : 0.18f));
            g.drawLine ({ polar (ar - (k == 0 ? 8.0f : 4.0f), ang), polar (ar + (k == 0 ? 8.0f : 4.0f), ang) }, 1.0f);
        }
        auto angFor = [&] (float semis) { return jlimit (-0.5f, 0.5f, semis) * 2.0f * span; };
        const float ao = angFor (shownOut), ai = angFor (shownIn);
        const bool inTune = std::abs (shownOut) < 0.05f && activity > 0.5f;
        if (std::abs (ao) > 0.005f)
        {
            Path dev;
            dev.addCentredArc (c.x, c.y, ar, ar, 0.0f, jmin (0.0f, ao), jmax (0.0f, ao), true);
            Neon::glowStroke (g, dev, Neon::violet.withAlpha (a), 3.0f);
        }
        const auto pin = polar (ar, ai), po = polar (ar, ao);
        g.setColour (Neon::grey.withAlpha (a));
        g.drawEllipse (pin.x - 5, pin.y - 5, 10, 10, 1.6f);
        glowDot (po, inTune ? 16.0f : 10.0f, Neon::cyan, (inTune ? 0.7f : 0.4f) * a);
        g.setColour (Colours::white.withAlpha (a));
        g.fillEllipse (po.x - 3.5f, po.y - 3.5f, 7, 7);
    }

    // ---- glowing note name ------------------------------------------------------------------
    {
        const String name = note >= 0 ? String (noteNames[pcOf (note)]) : String ("--");
        const String oct  = note >= 0 ? String (note / 12 - 1) : String();
        const float scaleF = 1.0f + 0.14f * pop;
        const float fs = R * 0.46f;
        const float cy = c.y + R * 0.08f;

        GlyphArrangement ga;
        ga.addLineOfText (FontOptions (fs, Font::bold), name, 0.0f, 0.0f);
        Path textPath;
        ga.createPath (textPath);
        const auto tb = textPath.getBounds();
        textPath.applyTransform (AffineTransform::translation (c.x - tb.getCentreX(), cy - tb.getCentreY())
                                    .scaled (scaleF, scaleF, c.x, cy));

        // soft bloom behind the letters (blurred copy)
        DropShadow (Neon::magenta.withAlpha (0.55f * a + 0.3f * pop), roundToInt (R * 0.16f), {}).drawForPath (g, textPath);
        DropShadow (Neon::cyan.withAlpha (0.35f * a + 0.4f * pop), roundToInt (R * 0.06f), {}).drawForPath (g, textPath);

        const auto b2 = textPath.getBounds();
        ColourGradient fill (Colours::white.withAlpha (0.4f + 0.6f * a), 0, b2.getY(),
                             Neon::cyan.withAlpha (0.4f + 0.6f * a), 0, b2.getBottom(), false);
        g.setGradientFill (fill);
        g.fillPath (textPath);

        g.setFont (FontOptions (fs * 0.36f, Font::bold));
        g.setColour (Neon::violet.withAlpha (a));
        g.drawText (oct, Rectangle<float> (b2.getRight() + 3, b2.getBottom() - fs * 0.42f, 40, fs * 0.45f), Justification::bottomLeft);

        const int co = roundToInt (shownOut * 100.0f);
        g.setFont (FontOptions (jmax (10.0f, R * 0.085f)));
        g.setColour (Neon::dim.withAlpha (a));
        g.drawText ((co > 0 ? "+" : "") + String (co) + " c", Rectangle<float> (c.x - 40, b2.getBottom() + 6, 80, 16), Justification::centred);
    }

    // ---- numbers at the bottom -----------------------------------------------------------------
    auto nums = bounds.reduced (16, 8).removeFromBottom (16);
    g.setFont (FontOptions (12.5f));
    const int ci = roundToInt (shownIn * 100.0f), co = roundToInt (shownOut * 100.0f);
    g.setColour (Neon::grey.brighter (0.3f));
    g.drawText ("IN  " + String (ci > 0 ? "+" : "") + String (ci) + " c", nums, Justification::centredLeft);
    g.setColour (Neon::cyan);
    g.drawText ("OUT " + String (co > 0 ? "+" : "") + String (co) + " c", nums, Justification::centredRight);
}

// ============================================================================
void PitchGraph::tick()
{
    // smoothly follow the singer so the curve stays centred
    for (int age = 0; age < 8; ++age)
    {
        const float v = history.out[history.index (age)];
        if (v > 0.0f) { centre += (v - centre) * 0.06f; break; }
    }
    phase += 0.12f;
    repaint();
}

void PitchGraph::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    Neon::drawPanel (g, r);

    const float gutter = 46.0f, headR = 34.0f;           // note labels left, "now" zone right
    const auto plot = r.reduced (10, 30).withTrimmedLeft (gutter).withTrimmedRight (headR).withTrimmedBottom (-18);
    const float range = 7.0f;
    const float semiH = plot.getHeight() / (2.0f * range);
    auto yFor = [&] (float midi) { return plot.getCentreY() - (midi - centre) * semiH; };

    const int N = PitchHistory::size;
    auto xFor = [&] (int age) { return plot.getRight() - plot.getWidth() * (float) age / (float) (N - 1); };

    const auto scale = inScale (proc);
    const int lo = (int) std::floor (centre - range) - 1, hi = (int) std::ceil (centre + range) + 1;
    const int curTarget = history.target[history.index (0)];
    const float latestOut = history.out[history.index (0)];
    const bool live = latestOut > 0.0f && curTarget >= 0;
    const float pulse = 0.5f + 0.5f * std::sin (phase);

    Graphics::ScopedSaveState clipState (g);
    g.reduceClipRegion (r.reduced (1).toNearestInt());

    // ---- piano-roll lanes -------------------------------------------------------------
    for (int m = lo; m <= hi; ++m)
    {
        const float y = yFor ((float) m);
        const int pc = pcOf (m);
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        const bool on = scale[(size_t) pc] && ! isRemoved (proc, pc);
        auto lane = Rectangle<float> (r.getX() + 1, y - semiH * 0.5f, r.getWidth() - 2, semiH);

        if (black) { g.setColour (Colours::black.withAlpha (0.16f)); g.fillRect (lane); }
        if (on)    { g.setColour (Neon::magenta.withAlpha (0.045f)); g.fillRect (lane); }
        if (m == curTarget && live)
        {
            ColourGradient band (Neon::magenta.withAlpha (0.0f), plot.getX(), 0, Neon::magenta.withAlpha (0.20f), plot.getRight() + headR, 0, false);
            g.setGradientFill (band);
            g.fillRect (lane);
        }
        g.setColour (on ? Neon::violet.withAlpha (0.16f) : Colours::white.withAlpha (0.03f));
        g.fillRect (plot.getX(), y - 0.5f, plot.getWidth() + headR, 1.0f);

        // note badge in the gutter
        if (on || m == curTarget)
        {
            const bool active = m == curTarget && live;
            auto badge = Rectangle<float> (r.getX() + 8, y - 8, gutter - 10, 16);
            g.setColour (active ? Neon::magenta.withAlpha (0.55f + 0.25f * pulse) : Colours::white.withAlpha (0.04f));
            g.fillRoundedRectangle (badge, 8.0f);
            if (active)
            {
                g.setColour (Neon::cyan.withAlpha (0.25f));
                g.drawRoundedRectangle (badge.expanded (1.5f), 9.0f, 1.0f);
            }
            g.setColour (active ? Colours::white : Neon::dim);
            g.setFont (FontOptions (10.5f, active ? Font::bold : Font::plain));
            g.drawText (String (noteNames[pc]) + String (m / 12 - 1), badge, Justification::centred);
        }
    }

    // ---- scrolling time grid (one line per second) ------------------------------------------
    for (int age = 0; age < N; ++age)
        if ((history.total - age) % 30 == 0)
        {
            g.setColour (Colours::white.withAlpha (0.035f));
            g.fillRect (xFor (age) - 0.5f, plot.getY() - 10, 1.0f, plot.getHeight() + 20);
        }

    // ---- target notes as glowing pills ------------------------------------------------------
    {
        int age = N - 1;
        while (age >= 0)
        {
            const int t = history.target[history.index (age)];
            if (t < 0) { --age; continue; }
            int end = age;
            while (end - 1 >= 0 && history.target[history.index (end - 1)] == t) --end;
            const float x0 = xFor (age), x1 = xFor (end);
            auto pill = Rectangle<float> (x0, yFor ((float) t) - semiH * 0.36f, jmax (6.0f, x1 - x0), semiH * 0.72f);
            const bool current = end == 0 && live;
            ColourGradient fill (Neon::magenta.withAlpha (current ? 0.40f : 0.24f), pill.getX(), 0,
                                 Neon::violet.withAlpha (current ? 0.55f : 0.32f), pill.getRight(), 0, false);
            g.setGradientFill (fill);
            g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
            g.setColour (Neon::cyan.withAlpha (current ? 0.55f : 0.25f));
            g.drawRoundedRectangle (pill, pill.getHeight() * 0.5f, 1.0f);
            age = end - 1;
        }
    }

    // ---- smooth curves -------------------------------------------------------------------------
    auto buildPath = [&] (const float* data)
    {
        Path p; bool pen = false; Point<float> prev;
        for (int age = N - 1; age >= 0; --age)
        {
            const float v = data[history.index (age)];
            if (v <= 0.0f) { pen = false; continue; }
            const Point<float> pt (xFor (age), yFor (v));
            if (! pen) { p.startNewSubPath (pt); pen = true; }
            else       p.quadraticTo (prev, (prev + pt) * 0.5f);   // midpoint smoothing
            prev = pt;
        }
        return p;
    };

    // input: thin, neutral, fading into the past
    {
        ColourGradient fade (Neon::grey.withAlpha (0.0f), plot.getX(), 0, Neon::grey.withAlpha (0.9f), plot.getRight(), 0, false);
        g.setGradientFill (fade);
        g.strokePath (buildPath (history.in), PathStrokeType (1.4f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    // output: glowing lilac ribbon, brightest at "now"
    {
        const Path outPath = buildPath (history.out);
        ColourGradient halo (Neon::magenta.withAlpha (0.0f), plot.getX(), 0, Neon::magenta.withAlpha (0.35f), plot.getRight(), 0, false);
        g.setGradientFill (halo);
        g.strokePath (outPath, PathStrokeType (7.0f, PathStrokeType::curved, PathStrokeType::rounded));
        ColourGradient core (Neon::violet.withAlpha (0.15f), plot.getX(), 0, Neon::cyan, plot.getRight(), 0, false);
        g.setGradientFill (core);
        g.strokePath (outPath, PathStrokeType (2.4f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // ---- "now" playhead -----------------------------------------------------------------------------
    {
        const float x = plot.getRight();
        ColourGradient line (Neon::cyan.withAlpha (0.0f), x, r.getY() + 8, Neon::cyan.withAlpha (0.0f), x, r.getBottom() - 8, false);
        line.addColour (0.5, Neon::cyan.withAlpha (0.45f));
        g.setGradientFill (line);
        g.fillRect (x - 0.75f, r.getY() + 8, 1.5f, r.getHeight() - 16);

        if (live)
        {
            const Point<float> head (x, yFor (latestOut));
            const float rad = 14.0f + 5.0f * pulse;
            ColourGradient bloom (Neon::magenta.withAlpha (0.55f), head.x, head.y, Neon::magenta.withAlpha (0.0f), head.x + rad * 1.6f, head.y, true);
            g.setGradientFill (bloom);
            g.fillEllipse (head.x - rad * 1.6f, head.y - rad * 1.6f, rad * 3.2f, rad * 3.2f);
            ColourGradient core (Neon::cyan.withAlpha (0.9f), head.x, head.y, Neon::cyan.withAlpha (0.0f), head.x + 8, head.y, true);
            g.setGradientFill (core);
            g.fillEllipse (head.x - 8, head.y - 8, 16, 16);
            g.setColour (Colours::white);
            g.fillEllipse (head.x - 3, head.y - 3, 6, 6);
        }
    }

    // ---- legend pills ----------------------------------------------------------------------------------
    {
        auto lg = r.reduced (14, 9).removeFromTop (16);
        auto pill = [&] (const String& text, Colour dot, float w)
        {
            auto b = lg.removeFromRight (w);
            lg.removeFromRight (6);
            g.setColour (Colours::black.withAlpha (0.25f));
            g.fillRoundedRectangle (b, 8.0f);
            g.setColour (dot);
            g.fillEllipse (b.getX() + 7, b.getCentreY() - 3, 6, 6);
            g.setColour (Neon::dim);
            g.setFont (FontOptions (10.0f, Font::bold));
            g.drawText (text, b.withTrimmedLeft (17), Justification::centredLeft);
        };
        pill ("OUTPUT", Neon::cyan, 66);
        pill ("INPUT", Neon::grey, 56);
        pill ("TARGET", Neon::magenta, 64);
        g.setColour (Neon::dim);
        g.setFont (FontOptions (11.5f, Font::bold));
        g.drawText ("PITCH", r.reduced (16, 9).removeFromTop (16), Justification::centredLeft);
    }
}

// ============================================================================
Rectangle<float> ScaleKeyboard::keyRect (int pc) const
{
    auto r = getLocalBounds().toFloat().reduced (14, 10);
    static const int whiteIndex[] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    const float ww = r.getWidth() / 7.0f;
    if (whiteIndex[pc] >= 0)
        return { r.getX() + ww * whiteIndex[pc] + 2, r.getY(), ww - 4, r.getHeight() };
    const int leftWhite = whiteIndex[pc - 1];
    const float bw = ww * 0.56f;
    return { r.getX() + ww * (leftWhite + 1) - bw * 0.5f, r.getY(), bw, r.getHeight() * 0.58f };
}

int ScaleKeyboard::hitTest (Point<float> p) const
{
    for (int pc : { 1, 3, 6, 8, 10 }) if (keyRect (pc).contains (p)) return pc;
    for (int pc : { 0, 2, 4, 5, 7, 9, 11 }) if (keyRect (pc).contains (p)) return pc;
    return -1;
}

void ScaleKeyboard::mouseDown (const MouseEvent& e)
{
    const int pc = hitTest (e.position);
    if (pc < 0) return;
    if (auto* prm = proc.apvts.getParameter (ParamIDs::noteOff (pc)))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
        prm->endChangeGesture();
    }
    repaint();
}

void ScaleKeyboard::mouseMove (const MouseEvent& e)
{
    const int h = hitTest (e.position);
    if (h != hover) { hover = h; repaint(); }
}

void ScaleKeyboard::paint (Graphics& g)
{
    Neon::drawPanel (g, getLocalBounds().toFloat());
    const auto scale = inScale (proc);
    const int t = proc.engine.targetNote.load();
    const int activePc = t >= 0 ? pcOf (t) : -1;

    auto drawKey = [&] (int pc, bool black)
    {
        auto k = keyRect (pc);
        const bool in = scale[(size_t) pc], removed = isRemoved (proc, pc), on = in && ! removed;
        const bool active = pc == activePc;

        Colour fill = black ? Colour (0xff0d0b19) : Colour (0xff1b1834);
        if (on) fill = fill.interpolatedWith (Neon::violet, black ? 0.35f : 0.25f);
        if (active) fill = Neon::cyan.withAlpha (0.85f);
        if (pc == hover) fill = fill.brighter (0.15f);
        g.setColour (fill);
        g.fillRoundedRectangle (k, 6.0f);

        if (active)
        {
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (Neon::cyan.withAlpha (0.08f * (4 - i)));
                g.drawRoundedRectangle (k.expanded ((float) i * 1.5f), 7.0f, 2.0f);
            }
        }
        g.setColour (on ? Neon::cyan.withAlpha (0.55f) : Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (k, 6.0f, 1.0f);

        g.setFont (FontOptions (black ? 10.5f : 12.5f, Font::bold));
        g.setColour (active ? Neon::bg0 : on ? Neon::text : Neon::dim.withAlpha (0.5f));
        auto lbl = k.removeFromBottom (black ? 18.0f : 22.0f);
        g.drawText (noteNames[pc], lbl, Justification::centred);
        if (in && removed)
        {
            g.setColour (Neon::magenta.withAlpha (0.8f));
            const auto c = k.getCentre().translated (0, -4);
            g.drawLine (c.x - 5, c.y - 5, c.x + 5, c.y + 5, 1.6f);
            g.drawLine (c.x - 5, c.y + 5, c.x + 5, c.y - 5, 1.6f);
        }
    };
    for (int pc : { 0, 2, 4, 5, 7, 9, 11 }) drawKey (pc, false);
    for (int pc : { 1, 3, 6, 8, 10 })       drawKey (pc, true);
}

// ============================================================================
AuraTuneEditor::AuraTuneEditor (AuraTuneProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      retune     (p.apvts, ParamIDs::retune,     "Speed",           "Retune speed: how fast pitch is pulled to the note. 0 = hard robotic tune."),
      amount     (p.apvts, ParamIDs::amount,     "Amount",          "How much of the correction is applied. 100 % = fully in tune."),
      humanize   (p.apvts, ParamIDs::humanize,   "Humanize",        "Slows correction on long sustained notes so they stay natural."),
      flex       (p.apvts, ParamIDs::flex,       "Flex-Tune",       "Only corrects when the singer is already close to the note."),
      tracking   (p.apvts, ParamIDs::tracking,   "Tracking",        "Pitch detector: low = choosy, high = relaxed (breathy voices)."),
      transition (p.apvts, ParamIDs::transition, "Transition",      "Glide time between notes — the smooth, soft feel."),
      softness   (p.apvts, ParamIDs::softness,   "Softness",        "Rounds every correction curve. Higher = silkier, less digital."),
      dblMix     (p.apvts, ParamIDs::dblMix,     "Double",          "Level of the doubled voices (0 = off)."),
      dblWidth   (p.apvts, ParamIDs::dblWidth,   "Width",           "How far left / right the doubles are spread."),
      dblDetune  (p.apvts, ParamIDs::dblDetune,  "Detune",          "How loosely the doubles follow the lead, in cents."),
      mix        (p.apvts, ParamIDs::mix,        "Mix",             "Dry / wet balance."),
      output     (p.apvts, ParamIDs::output,     "Output",          "Output level."),
      transpose  (p.apvts, ParamIDs::transpose,  "Transpose",       "Shift the whole vocal in semitones."),
      refHz      (p.apvts, ParamIDs::refHz,      "A4 Ref",          "Concert pitch reference.")
{
    setLookAndFeel (&lnf);

    for (auto* c : std::initializer_list<Component*> { &display, &graph, &keyboard, &retune, &amount, &humanize, &flex, &tracking,
                                                       &transition, &softness, &dblMix, &dblWidth, &dblDetune,
                                                       &mix, &output, &transpose, &refHz })
        addAndMakeVisible (c);

    auto setupBox = [this] (ComboBox& box, Label& lbl, const String& text)
    {
        addAndMakeVisible (box);
        lbl.setText (text, dontSendNotification);
        lbl.setFont (FontOptions (11.0f, Font::bold));
        lbl.setColour (Label::textColourId, Neon::dim);
        lbl.setJustificationType (Justification::bottomLeft);
        addAndMakeVisible (lbl);
    };
    setupBox (keyBox, keyLbl, "KEY");
    setupBox (scaleBox, scaleLbl, "SCALE");
    setupBox (inputBox, inputLbl, "INPUT TYPE");
    setupBox (presetBox, presetLbl, "PRESET");

    keyBox.addItemList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    for (int i = 0; i < (int) aura::ScaleType::NumScales; ++i) scaleBox.addItem (aura::scaleName (i), i + 1);
    inputBox.addItemList ({ "Soprano", "Alto/Tenor", "Low Male", "Instrument" }, 1);
    keyAtt   = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ParamIDs::key, keyBox);
    scaleAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ParamIDs::scale, scaleBox);
    inputAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ParamIDs::inputType, inputBox);

    int id = 1;
    for (const auto& pr : AuraTuneProcessor::presets()) presetBox.addItem (pr.name, id++);
    presetBox.setTextWhenNothingSelected ("Choose...");
    presetBox.onChange = [this] { proc.applyPreset (presetBox.getSelectedId() - 1); };

    setResizable (true, true);
    setResizeLimits (950, 600, 1710, 1080);
    getConstrainer()->setFixedAspectRatio (1140.0 / 720.0);
    setSize (1140, 720);
    startTimerHz (60);
}

AuraTuneEditor::~AuraTuneEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AuraTuneEditor::timerCallback()
{
    // display animates at 60 fps, graph and keyboard update at 30 fps
    display.tick();
    if ((++tickCount & 1) == 0)
    {
        history.push (proc.engine.inputMidi.load(), proc.engine.outputMidi.load(), proc.engine.targetNote.load());
        graph.tick();
        keyboard.repaint();
    }
}

void AuraTuneEditor::renderBackground()
{
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;
    background = Image (Image::ARGB, w, h, true);
    Graphics g (background);

    ColourGradient base (Neon::bg1, 0, 0, Neon::bg0, 0, (float) h, false);
    g.setGradientFill (base);
    g.fillAll();

    // one very faint violet light from the top-left
    ColourGradient rg (Neon::magenta.withAlpha (0.07f), w * 0.15f, 0.0f, Neon::magenta.withAlpha (0.0f), w * 0.75f, 0.0f, true);
    g.setGradientFill (rg);
    g.fillRect (0, 0, w, h);

    // header bar
    g.setColour (Colours::black.withAlpha (0.18f));
    g.fillRect (headerArea);
    g.setColour (Neon::border);
    g.fillRect (0, headerArea.getBottom(), w, 1);
}

void AuraTuneEditor::paint (Graphics& g)
{
    if (background.isValid()) g.drawImageAt (background, 0, 0);
    else g.fillAll (Neon::bg0);

    // logo
    auto logo = headerArea.reduced (22, 0).toFloat().removeFromLeft (260);
    const float s = (float) getWidth() / 1140.0f;
    g.setFont (FontOptions (30.0f * s, Font::bold));
    const String aura ("AURA");
    const float aw = GlyphArrangement::getStringWidth (g.getCurrentFont(), aura);
    ColourGradient lg (Neon::magenta, logo.getX(), 0, Neon::cyan, logo.getX() + aw, 0, false);
    g.setGradientFill (lg);
    g.drawText (aura, logo, Justification::centredLeft);
    g.setFont (FontOptions (30.0f * s));
    g.setColour (Neon::text);
    g.drawText ("TUNE", logo.withTrimmedLeft (aw + 8 * s), Justification::centredLeft);
    g.setFont (FontOptions (10.0f * s));
    g.setColour (Neon::dim);
    g.drawText ("SOFT PITCH CORRECTION", logo.withTrimmedTop (logo.getHeight() * 0.72f), Justification::topLeft);

    // section panels + titles
    auto section = [&] (Rectangle<int> a, const String& title, const String& sub, Colour accent)
    {
        Neon::drawPanel (g, a.toFloat());
        auto t = a.reduced (16, 10).removeFromTop (16).toFloat();
        g.setFont (FontOptions (12.0f * s, Font::bold));
        g.setColour (accent);
        g.drawText (title, t, Justification::centredLeft);
        g.setFont (FontOptions (10.5f * s));
        g.setColour (Neon::dim);
        g.drawText (sub, t, Justification::centredRight);
    };
    section (correctionArea, "CORRECTION", "speed - amount - humanize - flex", Neon::cyan);
    section (softArea, "SOFTNESS", "smooth & warm", Neon::cyan);
    section (dblArea, "DOUBLER", "stereo width", Neon::cyan);
    section (outArea, "OUTPUT", "", Neon::cyan);
}

void AuraTuneEditor::resized()
{
    const float s = (float) getWidth() / 1140.0f;
    auto S = [s] (float v) { return roundToInt (v * s); };
    auto r = getLocalBounds();

    headerArea = r.removeFromTop (S (70));
    {
        auto h = headerArea.reduced (S (22), S (13));
        h.removeFromLeft (S (250));
        auto place = [&] (ComboBox& box, Label& lbl, int width)
        {
            auto col = h.removeFromLeft (S ((float) width));
            h.removeFromLeft (S (12));
            lbl.setBounds (col.removeFromTop (S (14)));
            box.setBounds (col.withTrimmedTop (S (2)));
        };
        place (presetBox, presetLbl, 170);
        h.removeFromLeft (S (16));
        place (keyBox, keyLbl, 72);
        place (scaleBox, scaleLbl, 165);
        place (inputBox, inputLbl, 125);
        refHz.setBounds (h.removeFromRight (S (100)));
        h.removeFromRight (S (12));
        transpose.setBounds (h.removeFromRight (S (100)));
    }

    r.reduce (S (20), S (14));
    auto top = r.removeFromTop (S (290));
    display.setBounds (top.removeFromLeft (S (300)));
    top.removeFromLeft (S (14));
    graph.setBounds (top);

    r.removeFromTop (S (12));
    keyboard.setBounds (r.removeFromTop (S (92)));
    r.removeFromTop (S (12));

    auto knobs = r;
    const int gap = S (12);
    correctionArea = knobs.removeFromLeft (S (490)); knobs.removeFromLeft (gap);
    softArea       = knobs.removeFromLeft (S (178)); knobs.removeFromLeft (gap);
    dblArea        = knobs.removeFromLeft (S (250)); knobs.removeFromLeft (gap);
    outArea        = knobs;

    // weights let the two main knobs (Speed, Amount) be bigger than the rest
    auto layoutKnobs = [&] (Rectangle<int> area, std::initializer_list<std::pair<NeonKnob*, float>> list)
    {
        auto a = area.reduced (S (8), S (8));
        a.removeFromTop (S (22));
        float total = 0; for (auto& k : list) total += k.second;
        const int W = a.getWidth();
        for (auto& k : list)
        {
            auto cell = a.removeFromLeft (roundToInt (W * k.second / total));
            if (k.second < 1.2f) cell = cell.withTrimmedTop (S (14));   // small knobs sit a bit lower
            k.first->setBounds (cell);
        }
    };
    layoutKnobs (correctionArea, { { &retune, 1.45f }, { &amount, 1.45f }, { &humanize, 1.0f }, { &flex, 1.0f }, { &tracking, 1.0f } });
    layoutKnobs (softArea, { { &transition, 1.0f }, { &softness, 1.0f } });
    layoutKnobs (dblArea, { { &dblMix, 1.0f }, { &dblWidth, 1.0f }, { &dblDetune, 1.0f } });
    layoutKnobs (outArea, { { &mix, 1.0f }, { &output, 1.0f } });

    renderBackground();
}
