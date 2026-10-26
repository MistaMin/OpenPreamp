#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <memory>

namespace dsp {

// TwoX exists purely as an internal safety step: it is never a user-selectable
// choice (the "Oversample" parameter only offers 1x/4x/8x), but the engine can
// still be driven to it silently to keep a cut/shelf frequency clear of
// Nyquist without jumping straight to 4x when 2x headroom is already enough.
enum class OversampleMode { Native, TwoX, FourX, EightX };

class MultirateEngine {
public:
    void prepare(double sampleRate, int samplesPerBlock, int numChannels)
    {
        baseSampleRate = sampleRate;
        baseBlockSize = samplesPerBlock;
        channels = numChannels;

        oversampler2x = std::make_unique<juce::dsp::Oversampling<float>>(
            numChannels, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversampler4x = std::make_unique<juce::dsp::Oversampling<float>>(
            numChannels, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true);
        oversampler8x = std::make_unique<juce::dsp::Oversampling<float>>(
            numChannels, 3, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true);

        oversampler2x->initProcessing(static_cast<size_t>(samplesPerBlock));
        oversampler4x->initProcessing(static_cast<size_t>(samplesPerBlock));
        oversampler8x->initProcessing(static_cast<size_t>(samplesPerBlock));
    }

    void reset() noexcept {
        if (oversampler2x) oversampler2x->reset();
        if (oversampler4x) oversampler4x->reset();
        if (oversampler8x) oversampler8x->reset();
    }

    void setMode(OversampleMode m)
    {
        if (m == mode)
            return;
        mode = m;
        // Clear internal filter history so a mode switch mid-stream cannot
        // splice stale samples from the old rate into the new path (a source
        // of clicks/crackle when switching 1x/2x/4x/8x).
        if (oversampler2x) oversampler2x->reset();
        if (oversampler4x) oversampler4x->reset();
        if (oversampler8x) oversampler8x->reset();
    }
    OversampleMode getMode() const { return mode; }

    double getEffectiveSampleRate() const
    {
        switch (mode) {
            case OversampleMode::Native: return baseSampleRate;
            case OversampleMode::TwoX:   return baseSampleRate * 2.0;
            case OversampleMode::FourX:  return baseSampleRate * 4.0;
            case OversampleMode::EightX: return baseSampleRate * 8.0;
        }
        return baseSampleRate;
    }

    juce::dsp::AudioBlock<float> upsample(juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        switch (mode) {
            case OversampleMode::TwoX:
                return oversampler2x->processSamplesUp(block);
            case OversampleMode::FourX:
                return oversampler4x->processSamplesUp(block);
            case OversampleMode::EightX:
                return oversampler8x->processSamplesUp(block);
            case OversampleMode::Native:
            default:
                return block;
        }
    }

    void downsample(juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        switch (mode) {
            case OversampleMode::TwoX:
                oversampler2x->processSamplesDown(block);
                break;
            case OversampleMode::FourX:
                oversampler4x->processSamplesDown(block);
                break;
            case OversampleMode::EightX:
                oversampler8x->processSamplesDown(block);
                break;
            case OversampleMode::Native:
            default:
                break;
        }
    }

    float getLatencySamples() const
    {
        switch (mode) {
            case OversampleMode::TwoX:   return oversampler2x->getLatencyInSamples();
            case OversampleMode::FourX:  return oversampler4x->getLatencyInSamples();
            case OversampleMode::EightX: return oversampler8x->getLatencyInSamples();
            default: return 0.0f;
        }
    }

private:
    double baseSampleRate = 44100.0;
    int baseBlockSize = 512;
    int channels = 2;
    OversampleMode mode = OversampleMode::Native;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler2x;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler4x;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler8x;
};

} // namespace dsp
