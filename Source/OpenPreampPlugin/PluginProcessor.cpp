#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>

juce::AudioProcessorValueTreeState::ParameterLayout OpenPreampProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"preampType", 1}, "Preamp Type",
        juce::StringArray{"Brit", "N-Type", "FSF", "Off", "A-Type"}, 1));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"preampPad", 1}, "Preamp Pad",
        juce::StringArray{"-20 dB", "Unity", "+10 dB"}, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"preampGain", 1}, "Preamp Gain",
        juce::NormalisableRange<float>(-12.0f, 24.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"preampBypass", 1}, "Preamp Bypass", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"preampCircuit", 1}, "Preamp Circuit", true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1}, "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"hqMode", 1}, "HQ Mode", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"meterSource", 1}, "VU Source",
        juce::StringArray{"Input", "Output"}, 1));

    return {params.begin(), params.end()};
}

OpenPreampProcessor::OpenPreampProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

bool OpenPreampProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto output = layout.getMainOutputChannelSet();
    return (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo())
        && layout.getMainInputChannelSet() == output;
}

void OpenPreampProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sampleRateAtomic.store(sampleRate, std::memory_order_relaxed);
    multirate.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    multirate.setMode(dsp::OversampleMode::FourX);
    fixedLatency = int(std::ceil(multirate.getLatencySamples()));
    latencyPadding.setMaximumDelayInSamples(fixedLatency + 1);
    latencyPadding.prepare({sampleRate, juce::uint32(samplesPerBlock), juce::uint32(getTotalNumOutputChannels())});
    setLatencySamples(fixedLatency);
    preamp.prepare(sampleRate);
    preamp.setADAAEnabled(true);
    // The circuit follows the selected 2x/4x rate directly; never decimate it back to 96 kHz.
    preamp.setDecimationEnabled(false);
    preamp.setSampleRate(sampleRate);
    updateParameters();
}

void OpenPreampProcessor::updateParameters()
{
    auto getFloat = [&](const juce::String& id) {
        return apvts.getRawParameterValue(id)->load();
    };
    auto getChoice = [&](const juce::String& id) {
        return static_cast<int>(apvts.getRawParameterValue(id)->load());
    };
    auto getBool = [&](const juce::String& id) {
        return apvts.getRawParameterValue(id)->load() > 0.5f;
    };

    const bool circuitActive = getBool("preampCircuit") && !getBool("preampBypass") && getChoice("preampType") != 3;
    const int factor = circuitActive ? (getBool("hqMode") ? 4 : 2) : 1;
    multirate.setMode(factor == 4 ? dsp::OversampleMode::FourX : factor == 2 ? dsp::OversampleMode::TwoX : dsp::OversampleMode::Native);
    effectiveFactorAtomic.store(factor, std::memory_order_relaxed);
    latencyPadding.setDelay(float(fixedLatency) - multirate.getLatencySamples());
    preamp.setSampleRate(multirate.getEffectiveSampleRate());

    static constexpr float padGains[] = {-20.0f, 0.0f, 10.0f};
    preamp.setType(static_cast<dsp::PreampType>(getChoice("preampType")));
    preamp.setDriveDB(getFloat("preampGain") + padGains[std::clamp(getChoice("preampPad"), 0, 2)]);
    preamp.setBypassed(getBool("preampBypass"));
    preamp.setCircuitEnabled(getBool("preampCircuit"));
}

void OpenPreampProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    updateParameters();

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    inLevel.push(buffer.getArrayOfReadPointers(), buffer.getNumChannels(), buffer.getNumSamples());

    auto block = multirate.upsample(buffer);
    const int numChannels = std::min(int(block.getNumChannels()), 2);
    float* channels[2] = {block.getChannelPointer(0), nullptr};
    if (numChannels > 1) channels[1] = block.getChannelPointer(1);
    preamp.processBlock(channels, numChannels, int(block.getNumSamples()));
    multirate.downsample(buffer);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            latencyPadding.pushSample(ch, buffer.getSample(ch, i));
            buffer.setSample(ch, i, latencyPadding.popSample(ch));
        }

    float outGainDB = apvts.getRawParameterValue("outputGain")->load();
    float outGain = juce::Decibels::decibelsToGain(outGainDB);
    buffer.applyGain(outGain);
    outLevel.push(buffer.getArrayOfReadPointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

juce::AudioProcessorEditor* OpenPreampProcessor::createEditor()
{
    return new OpenPreampEditor(*this);
}

void OpenPreampProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void OpenPreampProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OpenPreampProcessor();
}
