#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "NeonLookAndFeel.h"

// ----------------------------------------------------------------------------
/** Rotary knob + caption + value text, bound to a parameter. */
class NeonKnob : public juce::Component
{
public:
    NeonKnob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& caption,
              const juce::String& tooltip = {});
    void resized() override;

    juce::Slider slider;
private:
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

// ----------------------------------------------------------------------------
/** Shared, UI-side history of pitch values (filled by the editor timer). */
struct PitchHistory
{
    static constexpr int size = 300;          // 10 s at 30 fps
    float in[size] {}, out[size] {};
    int   target[size] {};
    int   write = 0;
    void push (float i, float o, int t) { in[write] = i; out[write] = o; target[write] = t; write = (write + 1) % size; }
    int index (int age) const { return ((write - 1 - age) % size + size) % size; }   // age 0 = newest
};

class PitchGraph : public juce::Component
{
public:
    PitchGraph (const PitchHistory& h, AuraTuneProcessor& p) : history (h), proc (p) {}
    void paint (juce::Graphics&) override;
    void tick();
private:
    const PitchHistory& history;
    AuraTuneProcessor& proc;
    float centre = 60.0f;
};

class NoteDisplay : public juce::Component
{
public:
    NoteDisplay (AuraTuneProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void tick();
private:
    AuraTuneProcessor& proc;
    float shownIn = 0.0f, shownOut = 0.0f, activity = 0.0f;
    int note = -1;
};

class ScaleKeyboard : public juce::Component
{
public:
    ScaleKeyboard (AuraTuneProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
private:
    juce::Rectangle<float> keyRect (int pc) const;
    int hitTest (juce::Point<float>) const;
    AuraTuneProcessor& proc;
    int hover = -1;
};

// ----------------------------------------------------------------------------
class AuraTuneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AuraTuneEditor (AuraTuneProcessor&);
    ~AuraTuneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void renderBackground();

    AuraTuneProcessor& proc;
    NeonLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    PitchHistory history;
    NoteDisplay display { proc };
    PitchGraph graph { history, proc };
    ScaleKeyboard keyboard { proc };

    juce::ComboBox keyBox, scaleBox, inputBox, presetBox;
    juce::Label keyLbl, scaleLbl, inputLbl, presetLbl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt, inputAtt;

    NeonKnob retune, humanize, flex, tracking;
    NeonKnob transition, softness;
    NeonKnob transpose, mix, output, refHz;

    juce::Rectangle<int> correctionArea, softArea, outArea, headerArea;
    juce::Image background;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraTuneEditor)
};
