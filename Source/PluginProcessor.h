#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Presets.h"
#include <array>

class AnjiAmpProcessor : public juce::AudioProcessor
{
public:
    AnjiAmpProcessor();
    ~AnjiAmpProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Anji's Amp"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return (int) anji::factoryPresets().size(); }
    int getCurrentProgram() override { return currentPreset.load(); }
    void setCurrentProgram (int index) override { loadPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- used by the editor
    void loadPreset (int index);
    bool loadIR (const juce::File& file);
    juce::String getIRName() const { return irFile.existsAsFile() ? irFile.getFileNameWithoutExtension() : juce::String(); }
    bool saveUserPreset (const juce::File& file);
    bool loadUserPreset (const juce::File& file);

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<int> currentPreset { 0 };
    std::atomic<float> inMeter { 0 }, outMeter { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    anji::Engine engine;
    std::array<std::atomic<float>*, anji::kNumParams> raw {};
    std::atomic<float>* irOn = nullptr;

    juce::dsp::Convolution convolution;
    juce::File irFile;
    std::atomic<bool> irLoaded { false };
    float irAccIn = 1.0e-6f, irAccOut = 1.0e-6f, irGain = 1.0f;

    juce::AudioBuffer<float> mono;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnjiAmpProcessor)
};
