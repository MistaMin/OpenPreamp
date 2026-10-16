#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/Preamp.h"
#include "DSP/Resampler.h"
#include "DSP/ReducedCircuitBank.h"
#include "Toolkit.h"
#include "SessionCuts.h"
#include "SessionMonoMaker.h"

class OpenPreampProcessor;

class OpenPreampProcessor : public juce::AudioProcessor, private juce::AudioProcessorParameter::Listener {
public:
    OpenPreampProcessor();
    ~OpenPreampProcessor() override;
    void chooseInitialRate();
    void markRateChosen() { rateChosen.store(true, std::memory_order_relaxed); }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    int getEffectiveOversampleFactor() const { return effectiveFactorAtomic.load(std::memory_order_relaxed); }
    bool isADAAActive() const { return adaaActiveAtomic.load(std::memory_order_relaxed); }
    double getProcessingSampleRate() const { return sampleRateAtomic.load(std::memory_order_relaxed) * getEffectiveOversampleFactor(); }
    double getSessionProcessorSampleRate() const { return cuts.getSampleRate(); }
    // Offline benchmark/reference selection; never call concurrently with audio.
    void setLightTablesEnabled(bool enabled) { for (auto& engine : preamps) engine.setLightTablesEnabled(enabled); }
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
    enum ParameterIndex { Type, Pad, Input, Bypass, Circuit, Output, HQ, MeterSource,
        InputR, OutputR, MS, HP, LP, Cuts, Link, MonoEnabled, MonoFrequency, MeterMode, Engine, Oversampling, GainCompensation, ParameterCount };
    std::array<std::atomic<float>*,ParameterCount> parameterValues{};
    float value(ParameterIndex index) const noexcept { return parameterValues[size_t(index)]->load(std::memory_order_relaxed); }
    void updateParameters();
    void parameterValueChanged(int, float) override;
    void parameterGestureChanged(int, bool) override {}
    std::atomic<bool> rateChosen{false}, settingInitialRate{false};
    std::unique_ptr<dsp::ReducedCircuitBank> reducedBank;
    std::array<dsp::ReducedCircuitKernel,2> reducedKernels;
    bool reducedActive=false;
    int reducedType=-1, reducedFactor=0;

    std::array<dsp::PreampEngine,2> preamps;
    SessionCuts cuts;
    SessionMonoMaker monoMaker;
    bool monoMakerActive = false;
    std::array<juce::SmoothedValue<float>,2> inputGains, outputGains;
    bool msActive = false, cutsActive = true, inputBypassed = false;
    dsp::MultirateEngine multirate;
    juce::dsp::DelayLine<float> latencyPadding;
    int fixedLatency = 0;
    std::atomic<int> effectiveFactorAtomic{1};
    std::atomic<bool> adaaActiveAtomic{false},legacyRateDefaults{false};
    LevelTracker inLevel, outLevel;
    std::atomic<double> sampleRateAtomic{44100.0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenPreampProcessor)
};
