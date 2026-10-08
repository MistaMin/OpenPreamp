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

void OpenPreampProcessor::prepareToPlay(double sampleRate, int)
{
    sampleRateAtomic.store(sampleRate, std::memory_order_relaxed);
    setLatencySamples(0);
    preamp.prepare(sampleRate);
    // The circuit runs directly at the session rate, without decimation or resampling.
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

    float* channels[2] = {buffer.getWritePointer(0), nullptr};
    const int numChannels = std::min(buffer.getNumChannels(), 2);
    if (numChannels > 1) channels[1] = buffer.getWritePointer(1);
    preamp.processBlock(channels, numChannels, buffer.getNumSamples());

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
