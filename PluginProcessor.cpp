#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

AuraTuneProcessor::AuraTuneProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AuraTune", createLayout())
{
}

AudioProcessorValueTreeState::ParameterLayout AuraTuneProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    using P = ParameterID;

    StringArray keys { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    StringArray scales;
    for (int i = 0; i < (int) aura::ScaleType::NumScales; ++i) scales.add (aura::scaleName (i));

    l.add (std::make_unique<AudioParameterChoice> (P { ParamIDs::key, 1 }, "Key", keys, 0));
    l.add (std::make_unique<AudioParameterChoice> (P { ParamIDs::scale, 1 }, "Scale", scales, 0));
    l.add (std::make_unique<AudioParameterChoice> (P { ParamIDs::inputType, 1 }, "Input Type",
                                                   StringArray { "Soprano", "Alto/Tenor", "Low Male", "Instrument" }, 1));

    auto ms  = [] (float v, int) { return String (roundToInt (v)) + " ms"; };
    auto pct = [] (float v, int) { return String (roundToInt (v)) + " %"; };

    NormalisableRange<float> retuneRange (0.0f, 400.0f, 1.0f); retuneRange.setSkewForCentre (50.0f);
    NormalisableRange<float> transRange  (0.0f, 500.0f, 1.0f); transRange.setSkewForCentre (100.0f);

    l.add (std::make_unique<AudioParameterFloat> (P { ParamIDs::retune, 1 }, "Retune Speed", retuneRange, 20.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    l.add (std::make_unique<AudioParameterFloat> (P { ParamIDs::amount, 1 }, "Amount", NormalisableRange<float> (0, 100, 1), 100.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterFloat> (P { ParamIDs::dblMix, 1 }, "Doubler", NormalisableRange<float> (0, 100, 1), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterFloat> (P { ParamIDs::dblWidth, 1 }, "Width", NormalisableRange<float> (0, 100, 1), 70.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterFloat> (P { ParamIDs::dblDetune, 1 }, "Detune", NormalisableRange<float> (0, 30, 0.5f), 10.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return String (v, 1) + " c"; })));
    l.add (std::make_unique<AudioParameterFloat> (P { ParamIDs::humanize, 1 }, "Humanize", NormalisableRange<float> (0, 100, 1), 20.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterFloat> (P { ParamIDs::flex, 1 }, "Flex-Tune", NormalisableRange<float> (0, 100, 1), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterFloat> (P { ParamIDs::tracking, 1 }, "Tracking", NormalisableRange<float> (0, 100, 1), 50.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterFloat> (P { ParamIDs::transition, 1 }, "Note Transition", transRange, 60.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    l.add (std::make_unique<AudioParameterFloat> (P { ParamIDs::softness, 1 }, "Softness", NormalisableRange<float> (0, 100, 1), 50.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterFloat> (P { ParamIDs::transpose, 1 }, "Transpose", NormalisableRange<float> (-12, 12, 1), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return (v > 0 ? "+" : "") + String (roundToInt (v)) + " st"; })));
    l.add (std::make_unique<AudioParameterFloat> (P { ParamIDs::refHz, 1 }, "A4 Reference", NormalisableRange<float> (430, 450, 0.1f), 440.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return String (v, 1) + " Hz"; })));
    l.add (std::make_unique<AudioParameterFloat> (P { ParamIDs::mix, 1 }, "Mix", NormalisableRange<float> (0, 100, 1), 100.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterFloat> (P { ParamIDs::output, 1 }, "Output", NormalisableRange<float> (-24, 12, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return String (v, 1) + " dB"; })));
    for (int i = 0; i < 12; ++i)
        l.add (std::make_unique<AudioParameterBool> (P { ParamIDs::noteOff (i), 1 }, "Remove " + keys[i], false));
    return l;
}

const std::vector<AuraTuneProcessor::Preset>& AuraTuneProcessor::presets()
{
    //                         retune amount humanize flex transition softness doubler
    static const std::vector<Preset> p {
        { "Natural Pop",          35,   100,    40,     20,   80,     60,      0 },
        { "Smooth R&B",           15,   100,    25,     10,   120,    80,     20 },
        { "Gentle Ballad",        60,    90,    60,     35,   150,    90,      0 },
        { "Melodic Trap",          5,   100,    10,      0,   35,     30,     25 },
        { "Hard Tune",             0,   100,     0,      0,    0,      0,      0 },
        { "Transparent Fix",     120,    80,    70,     50,   100,    85,      0 },
        { "Wide Pop Double",      25,   100,    30,     10,   70,     60,     45 },
    };
    return p;
}

void AuraTuneProcessor::applyPreset (int index)
{
    const auto& all = presets();
    if (index < 0 || index >= (int) all.size()) return;
    const auto& p = all[(size_t) index];
    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };
    set (ParamIDs::retune, p.retune);
    set (ParamIDs::humanize, p.humanize);
    set (ParamIDs::flex, p.flex);
    set (ParamIDs::transition, p.transition);
    set (ParamIDs::softness, p.softness);
    set (ParamIDs::amount, p.amount);
    set (ParamIDs::dblMix, p.doubler);
}

bool AuraTuneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void AuraTuneProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (engine.getLatencySamples());
    doubler.prepare (sampleRate);
    dblSmoothed.reset (sampleRate, 0.05);
    widthSmoothed.reset (sampleRate, 0.05);
    dblSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue (ParamIDs::dblMix)->load() * 0.01f);
    widthSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue (ParamIDs::dblWidth)->load() * 0.01f);
    mixSmoothed.reset (sampleRate, 0.05);
    gainSmoothed.reset (sampleRate, 0.05);
    mixSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue (ParamIDs::mix)->load() * 0.01f);
    gainSmoothed.setCurrentAndTargetValue (Decibels::decibelsToGain (apvts.getRawParameterValue (ParamIDs::output)->load()));
}

aura::TuneParams AuraTuneProcessor::readParams() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    aura::TuneParams p;
    p.key          = (int) v (ParamIDs::key);
    p.scale        = (int) v (ParamIDs::scale);
    p.inputType    = (int) v (ParamIDs::inputType);
    p.retuneMs     = v (ParamIDs::retune);
    p.humanize     = v (ParamIDs::humanize) * 0.01f;
    p.flexTune     = v (ParamIDs::flex) * 0.01f;
    p.amount       = v (ParamIDs::amount) * 0.01f;
    p.tracking     = v (ParamIDs::tracking) * 0.01f;
    p.transitionMs = v (ParamIDs::transition);
    p.softness     = v (ParamIDs::softness) * 0.01f;
    p.transpose    = v (ParamIDs::transpose);
    p.refHz        = v (ParamIDs::refHz);
    for (int i = 0; i < 12; ++i)
        p.removed[(size_t) i] = apvts.getRawParameterValue (ParamIDs::noteOff (i))->load() > 0.5f;
    return p;
}

void AuraTuneProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int numCh = getTotalNumInputChannels();
    const int numSamples = buffer.getNumSamples();
    for (int ch = numCh; ch < getTotalNumOutputChannels(); ++ch) buffer.clear (ch, 0, numSamples);
    if (numCh == 0) return;

    engine.setParams (readParams());
    mixSmoothed.setTargetValue (apvts.getRawParameterValue (ParamIDs::mix)->load() * 0.01f);
    gainSmoothed.setTargetValue (Decibels::decibelsToGain (apvts.getRawParameterValue (ParamIDs::output)->load()));
    dblSmoothed.setTargetValue (apvts.getRawParameterValue (ParamIDs::dblMix)->load() * 0.01f);
    widthSmoothed.setTargetValue (apvts.getRawParameterValue (ParamIDs::dblWidth)->load() * 0.01f);
    const float detune = apvts.getRawParameterValue (ParamIDs::dblDetune)->load();

    // Vocals are mono: tune the channel sum, write the result to every channel.
    float* l = buffer.getWritePointer (0);
    float* r = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    const float inScale = r != nullptr ? 0.5f : 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float x = (l[i] + (r != nullptr ? r[i] : 0.0f)) * inScale;
        float wet, dry;
        engine.processSample (x, wet, dry);
        const float m = mixSmoothed.getNextValue();
        const float y = wet * m + dry * (1.0f - m);

        // doubler: stereo copies around the centre voice, level-compensated
        const float dm = dblSmoothed.getNextValue();
        const float wd = widthSmoothed.getNextValue();
        const float comp = 1.0f / std::sqrt (1.0f + 0.5f * dm * dm);
        float yl = y, yr = y;
        doubler.process (y, yl, yr, dm, r != nullptr ? wd : 0.0f, detune);

        const float g = gainSmoothed.getNextValue() * comp;
        if (r != nullptr) { l[i] = yl * g; r[i] = yr * g; }
        else              { l[i] = 0.5f * (yl + yr) * g; }
    }
}

AudioProcessorEditor* AuraTuneProcessor::createEditor() { return new AuraTuneEditor (*this); }

void AuraTuneProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AuraTuneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AuraTuneProcessor(); }
