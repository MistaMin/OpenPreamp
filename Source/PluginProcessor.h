#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/Resampler.h"
#include "DSP/HarmonicSaturation.h"
#include "DSP/EQFilters.h"
#include "DSP/Preamp.h"
#include "Toolkit.h"

class HybridEQProcessor : public juce::AudioProcessor {
public:
    HybridEQProcessor();
    ~HybridEQProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    dsp::AnalogEQ& getEQ() { return eq; }
    // Level taps for the meters (safe to read from the UI thread).
    LevelTracker& getInputLevel() { return inLevel; }
    LevelTracker& getOutputLevel() { return outLevel; }
    // Raw sample capture for the spectrum analyser (the FFT itself runs on the UI thread).
    const spectrum::SampleRing& getInputRing() const { return inRing; }
    const spectrum::SampleRing& getOutputRing() const { return outRing; }
    double getCurrentSampleRate() const { return sampleRateAtomic.load(std::memory_order_relaxed); }

    // Effective oversampling factor (1/2/4/8) the engine is currently running at. This is the user's
    // "Oversample" choice raised by the headroom floor (High Cut / High shelf / Preamp Circuit), so it
    // can be higher than the choice itself. Written from prepareToPlay, read from the editor.
    int getEffectiveOversampleFactor() const { return effectiveFactorAtomic.load(std::memory_order_relaxed); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    // Applies the current parameter values to the DSP. Never changes the reported latency and never
    // writes a host-visible parameter: both are settled in prepareToPlay, and the audio thread only
    // ever reads them. The effective oversampling factor is published in effectiveFactorAtomic so the
    // editor can show what the engine is actually running at.
    void updateParameters();

    dsp::MultirateEngine multirateEngine;
    dsp::PreampEngine preamp;
    dsp::HarmonicSaturationEngine saturation;
    dsp::AnalogEQ eq;
    double currentSampleRate = 44100.0;
    LevelTracker inLevel, outLevel;
    spectrum::SampleRing inRing, outRing;
    std::atomic<double> sampleRateAtomic{44100.0};
    std::atomic<int> effectiveFactorAtomic{1};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HybridEQProcessor)
};
