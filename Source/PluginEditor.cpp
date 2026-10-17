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
void NoteDisplay::tick()
{
    const float in  = proc.engine.inputMidi.load();
    const float out = proc.engine.outputMidi.load();
    const int   t   = proc.engine.targetNote.load();
    const bool voiced = in > 0.0f && t >= 0;
    activity += ((voiced ? 1.0f : 0.0f) - activity) * 0.25f;
    if (voiced)
    {
        note = t;
        shownIn  += ((in  - (float) t) - shownIn)  * 0.35f;
        shownOut += (((out > 0 ? out : in) - (float) t) - shownOut) * 0.35f;
    }
    repaint();
}

void NoteDisplay::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    Neon::drawPanel (g, r);
    r.reduce (18, 14);

    g.setColour (Neon::dim);
    g.setFont (FontOptions (11.5f, Font::bold));
    g.drawText ("TARGET NOTE", r.removeFromTop (16), Justification::centredLeft);

    // big note name
    auto noteArea = r.removeFromTop (r.getHeight() * 0.55f);
    const String name = note >= 0 ? String (noteNames[pcOf (note)]) : String ("--");
    const String oct  = note >= 0 ? String (note / 12 - 1) : String();
    const float a = 0.35f + 0.65f * activity;

    g.setFont (FontOptions (86.0f, Font::bold));
    const float nameW = GlyphArrangement::getStringWidth (g.getCurrentFont(), name);
    auto nameRect = noteArea.withSizeKeepingCentre (nameW + 50, noteArea.getHeight());
    for (int i = 3; i >= 1; --i)  // glow
    {
        g.setColour (Neon::cyan.withAlpha (0.07f * a));
        g.drawText (name, nameRect.translated (-(float) i, 0).withTrimmedRight (40), Justification::centredRight);
        g.drawText (name, nameRect.translated ((float) i, 0).withTrimmedRight (40), Justification::centredRight);
    }
    g.setColour (Neon::text.withAlpha (a));
    g.drawText (name, nameRect.withTrimmedRight (40), Justification::centredRight);
    g.setFont (FontOptions (26.0f));
    g.setColour (Neon::magenta.withAlpha (a));
    g.drawText (oct, nameRect.withTrimmedLeft (nameW + 6).withTrimmedTop (nameRect.getHeight() * 0.45f), Justification::topLeft);

    // cents meter
    r.removeFromTop (6);
    auto meter = r.removeFromTop (34);
    auto bar = meter.withSizeKeepingCentre (meter.getWidth(), 6);
    ColourGradient grad (Neon::magenta.withAlpha (0.5f), bar.getX(), 0, Neon::magenta.withAlpha (0.5f), bar.getRight(), 0, false);
    grad.addColour (0.5, Neon::cyan.withAlpha (0.8f));
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bar, 3.0f);
    g.setColour (Colours::white.withAlpha (0.5f));
    for (int c = -50; c <= 50; c += 25)
    {
        const float x = bar.getX() + bar.getWidth() * (c + 50) / 100.0f;
        g.fillRect (x - 0.5f, meter.getY() + (c == 0 ? 2.0f : 8.0f), 1.0f, c == 0 ? meter.getHeight() - 4 : meter.getHeight() - 16);
    }
    auto xFor = [&] (float semis) { return bar.getX() + bar.getWidth() * (jlimit (-0.5f, 0.5f, semis) + 0.5f); };
    // input deviation (hollow ring) and corrected pitch (solid glow)
    const float xi = xFor (shownIn), xo = xFor (shownOut), cy = bar.getCentreY();
    g.setColour (Neon::violet.withAlpha (a));
    g.drawEllipse (xi - 7, cy - 7, 14, 14, 2.0f);
    for (int i = 3; i >= 1; --i) { g.setColour (Neon::cyan.withAlpha (0.15f * a * (4 - i))); g.fillEllipse (xo - 3.0f * i, cy - 3.0f * i, 6.0f * i, 6.0f * i); }
    g.setColour (Colours::white.withAlpha (a)); g.fillEllipse (xo - 3, cy - 3, 6, 6);

    // numbers
    r.removeFromTop (6);
    auto nums = r.removeFromTop (20);
    g.setFont (FontOptions (13.0f));
    const int ci = roundToInt (shownIn * 100.0f), co = roundToInt (shownOut * 100.0f);
    g.setColour (Neon::violet.brighter (0.3f));
    g.drawText ("IN  " + String (ci > 0 ? "+" : "") + String (ci) + " c", nums.removeFromLeft (nums.getWidth() / 2), Justification::centredLeft);
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
    repaint();
}

void PitchGraph::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    Neon::drawPanel (g, r);
    auto plot = r.reduced (14, 12).withTrimmedLeft (34);

    const float range = 7.0f;
    auto yFor = [&] (float midi) { return plot.getCentreY() - (midi - centre) / range * plot.getHeight() * 0.5f; };

    const auto scale = inScale (proc);
    const int lo = (int) std::floor (centre - range), hi = (int) std::ceil (centre + range);

    g.saveState();
    g.reduceClipRegion (r.reduced (1).toNearestInt());
    for (int m = lo; m <= hi; ++m)
    {
        const float y = yFor ((float) m);
        if (y < plot.getY() - 2 || y > plot.getBottom() + 2) continue;
        const int pc = pcOf (m);
        const bool on = scale[(size_t) pc] && ! isRemoved (proc, pc);
        g.setColour (on ? Neon::violet.withAlpha (0.22f) : Colours::white.withAlpha (0.035f));
        g.fillRect (plot.getX(), y - 0.5f, plot.getWidth(), 1.0f);
        if (on)
        {
            g.setColour (Neon::dim);
            g.setFont (FontOptions (10.5f));
            g.drawText (String (noteNames[pc]) + String (m / 12 - 1), Rectangle<float> (r.getX() + 6, y - 7, 38, 14), Justification::centredLeft);
        }
    }

    const int N = PitchHistory::size;
    auto xFor = [&] (int age) { return plot.getRight() - plot.getWidth() * (float) age / (float) (N - 1); };

    auto buildPath = [&] (const float* data)
    {
        Path p; bool pen = false;
        for (int age = N - 1; age >= 0; --age)
        {
            const float v = data[history.index (age)];
            if (v <= 0.0f) { pen = false; continue; }
            const Point<float> pt (xFor (age), yFor (v));
            if (! pen) { p.startNewSubPath (pt); pen = true; } else p.lineTo (pt);
        }
        return p;
    };

    // target note steps
    {
        Path t; bool pen = false;
        for (int age = N - 1; age >= 0; --age)
        {
            const int v = history.target[history.index (age)];
            if (v < 0) { pen = false; continue; }
            const Point<float> pt (xFor (age), yFor ((float) v));
            if (! pen) { t.startNewSubPath (pt); pen = true; } else t.lineTo (pt);
        }
        g.setColour (Neon::magenta.withAlpha (0.35f));
        g.strokePath (t, PathStrokeType (1.2f));
    }

    g.setColour (Neon::violet.withAlpha (0.75f));
    g.strokePath (buildPath (history.in), PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
    Neon::glowStroke (g, buildPath (history.out), Neon::cyan, 2.2f);
    g.restoreState();

    // legend
    auto lg = r.reduced (16, 10).removeFromTop (14);
    g.setFont (FontOptions (11.0f, Font::bold));
    g.setColour (Neon::cyan);       g.drawText ("OUTPUT", lg.removeFromRight (60), Justification::centredRight);
    g.setColour (Neon::violet);     g.drawText ("INPUT",  lg.removeFromRight (56), Justification::centredRight);
    g.setColour (Neon::magenta.withAlpha (0.7f)); g.drawText ("TARGET", lg.removeFromRight (60), Justification::centredRight);
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
      retune     (p.apvts, ParamIDs::retune,     "Retune Speed",    "How fast pitch is pulled to the note. 0 = hard robotic tune."),
      humanize   (p.apvts, ParamIDs::humanize,   "Humanize",        "Slows correction on long sustained notes so they stay natural."),
      flex       (p.apvts, ParamIDs::flex,       "Flex-Tune",       "Only corrects when the singer is already close to the note."),
      tracking   (p.apvts, ParamIDs::tracking,   "Tracking",        "Pitch detector: low = choosy, high = relaxed (breathy voices)."),
      transition (p.apvts, ParamIDs::transition, "Transition",      "Glide time between notes — the smooth, soft feel."),
      softness   (p.apvts, ParamIDs::softness,   "Softness",        "Rounds every correction curve. Higher = silkier, less digital."),
      transpose  (p.apvts, ParamIDs::transpose,  "Transpose",       "Shift the whole vocal in semitones."),
      mix        (p.apvts, ParamIDs::mix,        "Mix",             "Dry / wet balance."),
      output     (p.apvts, ParamIDs::output,     "Output",          "Output level."),
      refHz      (p.apvts, ParamIDs::refHz,      "A4 Ref",          "Concert pitch reference.")
{
    setLookAndFeel (&lnf);

    for (auto* c : std::initializer_list<Component*> { &display, &graph, &keyboard, &retune, &humanize, &flex, &tracking,
                                                       &transition, &softness, &transpose, &mix, &output, &refHz })
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
    setResizeLimits (900, 600, 1600, 1060);
    getConstrainer()->setFixedAspectRatio (1020.0 / 680.0);
    setSize (1020, 680);
    startTimerHz (30);
}

AuraTuneEditor::~AuraTuneEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AuraTuneEditor::timerCallback()
{
    history.push (proc.engine.inputMidi.load(), proc.engine.outputMidi.load(), proc.engine.targetNote.load());
    display.tick();
    graph.tick();
    keyboard.repaint();
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

    auto blob = [&] (float x, float y, float rad, Colour c)
    {
        ColourGradient rg (c, x, y, c.withAlpha (0.0f), x + rad, y, true);
        g.setGradientFill (rg);
        g.fillEllipse (x - rad, y - rad, rad * 2, rad * 2);
    };
    blob (w * 0.12f, h * 0.05f, w * 0.45f, Neon::violet.withAlpha (0.18f));
    blob (w * 0.92f, h * 0.35f, w * 0.40f, Neon::cyan.withAlpha (0.08f));
    blob (w * 0.55f, h * 1.05f, w * 0.50f, Neon::magenta.withAlpha (0.10f));

    // subtle dot grid
    g.setColour (Colours::white.withAlpha (0.025f));
    for (int y = 0; y < h; y += 18)
        for (int x = (y / 18) % 2 * 9; x < w; x += 18)
            g.fillRect (x, y, 1, 1);

    // header line
    ColourGradient line (Neon::cyan.withAlpha (0.0f), 0, 0, Neon::cyan.withAlpha (0.0f), (float) w, 0, false);
    line.addColour (0.3, Neon::violet.withAlpha (0.6f));
    line.addColour (0.7, Neon::cyan.withAlpha (0.5f));
    g.setGradientFill (line);
    g.fillRect (0, headerArea.getBottom(), w, 1);
}

void AuraTuneEditor::paint (Graphics& g)
{
    if (background.isValid()) g.drawImageAt (background, 0, 0);
    else g.fillAll (Neon::bg0);

    // logo
    auto logo = headerArea.reduced (22, 0).toFloat().removeFromLeft (260);
    const float s = (float) getWidth() / 1020.0f;
    g.setFont (FontOptions (30.0f * s, Font::bold));
    const String aura ("AURA");
    const float aw = GlyphArrangement::getStringWidth (g.getCurrentFont(), aura);
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (Neon::magenta.withAlpha (0.06f));
        g.drawText (aura, logo.translated (0, (float) i * 0.5f).expanded ((float) i), Justification::centredLeft);
    }
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
    section (correctionArea, "CORRECTION", "speed - humanize - flex", Neon::cyan);
    section (softArea, "SOFTNESS", "smooth & warm", Neon::magenta);
    section (outArea, "OUTPUT", "", Neon::violet.brighter (0.3f));
}

void AuraTuneEditor::resized()
{
    const float s = (float) getWidth() / 1020.0f;
    auto S = [s] (float v) { return roundToInt (v * s); };
    auto r = getLocalBounds();

    headerArea = r.removeFromTop (S (70));
    {
        auto h = headerArea.reduced (S (22), S (14));
        h.removeFromLeft (S (260));
        auto place = [&] (ComboBox& box, Label& lbl, int width)
        {
            auto col = h.removeFromLeft (S ((float) width));
            h.removeFromLeft (S (12));
            lbl.setBounds (col.removeFromTop (S (14)));
            box.setBounds (col.withTrimmedTop (S (2)));
        };
        place (presetBox, presetLbl, 170);
        h.removeFromLeft (S (40));
        place (keyBox, keyLbl, 80);
        place (scaleBox, scaleLbl, 170);
        place (inputBox, inputLbl, 130);
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
    correctionArea = knobs.removeFromLeft (S (404));
    knobs.removeFromLeft (S (14));
    softArea = knobs.removeFromLeft (S (216));
    knobs.removeFromLeft (S (14));
    outArea = knobs;

    auto layoutKnobs = [&] (Rectangle<int> area, std::initializer_list<NeonKnob*> list)
    {
        auto a = area.reduced (S (10), S (8));
        a.removeFromTop (S (22));
        const int w = a.getWidth() / (int) list.size();
        for (auto* k : list) k->setBounds (a.removeFromLeft (w));
    };
    layoutKnobs (correctionArea, { &retune, &humanize, &flex, &tracking });
    layoutKnobs (softArea, { &transition, &softness });
    layoutKnobs (outArea, { &transpose, &mix, &output, &refHz });

    renderBackground();
}
