#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TuneEngine.h"

namespace ParamIDs
{
    inline constexpr const char* key        = "key";
    inline constexpr const char* scale      = "scale";
    inline constexpr const char* inputType  = "inputType";
    inline constexpr const char* retune     = "retune";
    inline constexpr const char* humanize   = "humanize";
    inline constexpr const char* flex       = "flex";
    inline constexpr const char* tracking   = "tracking";
    inline constexpr const char* transition = "transition";
    inline constexpr const char* softness   = "softness";
    inline constexpr const char* transpose  = "transpose";
    inline constexpr const char* refHz      = "refHz";
    inline constexpr const char* mix        = "mix";
    inline constexpr const char* output     = "output";
    inline juce::String noteOff (int i) { return "noteOff" + juce::String (i); }
}

class AuraTuneProcessor : public juce::AudioProcessor
{
public:
    AuraTuneProcessor();
    ~AuraTuneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct Preset { const char* name; float retune, humanize, flex, transition, softness; };
    static const std::vector<Preset>& presets();
    void applyPreset (int index);

    juce::AudioProcessorValueTreeState apvts;
    aura::TuneEngine engine;

private:
    aura::TuneParams readParams() const;
    juce::SmoothedValue<float> mixSmoothed, gainSmoothed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraTuneProcessor)
};
