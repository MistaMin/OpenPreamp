#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/Preamp.h"
#include "DSP/Resampler.h"
#include "Toolkit.h"

class OpenPreampProcessor;

class OpenPreampProcessor : public juce::AudioProcessor {
public:
    OpenPreampProcessor();
    ~OpenPreampProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    int getEffectiveOversampleFactor() const { return effectiveFactorAtomic.load(std::memory_order_relaxed); }
    double getProcessingSampleRate() const { return sampleRateAtomic.load(std::memory_order_relaxed) * getEffectiveOversampleFactor(); }
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    LevelTracker& getInputLevel() { return inLevel; }
    LevelTracker& getOutputLevel() { return outLevel; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void updateParameters();

    dsp::PreampEngine preamp;
    dsp::MultirateEngine multirate;
    juce::dsp::DelayLine<float> latencyPadding;
    int fixedLatency = 0;
    std::atomic<int> effectiveFactorAtomic{1};
    LevelTracker inLevel, outLevel;
    std::atomic<double> sampleRateAtomic{44100.0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenPreampProcessor)
};
