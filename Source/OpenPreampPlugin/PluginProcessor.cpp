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

void OpenPreampProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sampleRateAtomic.store(sampleRate, std::memory_order_relaxed);
    currentSampleRate = sampleRate;
    multirateEngine.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    preamp.prepare(sampleRate);
    // Standalone preamp: the netlist follows the oversampled host rate directly.
    preamp.setDecimationEnabled(false);
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

    // OpenPreamp rate policy (HANDOFF §5.1):
    //   - circuit OFF: native rate
    //   - circuit ON:  session rate if >= 88.2/96 kHz, otherwise 4x oversampled
    //   - user can push higher with 4x or 8x oversampling, but the circuit-on
    //     floor is always 4x below 88.2/96 kHz.
    static constexpr dsp::OversampleMode allModes[] = {
        dsp::OversampleMode::Native,
        dsp::OversampleMode::FourX,
        dsp::OversampleMode::EightX
    };
    static constexpr int allFactors[] = {1, 4, 8};

    int userFactor = allFactors[std::clamp(getChoice("preampPad"), 0, 2)];
    if (getBool("preampCircuit")) {
        const double nativeCircuitFloor = (currentSampleRate >= 88200.0) ? 1 : 4;
        userFactor = std::max(userFactor, static_cast<int>(nativeCircuitFloor));
    }

    dsp::OversampleMode effectiveMode = dsp::OversampleMode::Native;
    for (int i = 0; i < 3; ++i)
        if (allFactors[i] == userFactor)
            effectiveMode = allModes[i];

    multirateEngine.setMode(effectiveMode);
    effectiveFactorAtomic.store(userFactor, std::memory_order_relaxed);

    static constexpr float padGains[] = {-20.0f, 0.0f, 10.0f};
    preamp.setType(static_cast<dsp::PreampType>(getChoice("preampType")));
    preamp.setDriveDB(getFloat("preampGain") + padGains[getChoice("preampPad")]);
    preamp.setBypassed(getBool("preampBypass"));
    preamp.setCircuitEnabled(getBool("preampCircuit"));

    double effectiveSR = multirateEngine.getEffectiveSampleRate();
    preamp.setSampleRate(effectiveSR);
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

    auto osBlock = multirateEngine.upsample(buffer);
    int numCh = std::min(static_cast<int>(osBlock.getNumChannels()), 2);
    int numSamp = static_cast<int>(osBlock.getNumSamples());
    float* chPtrs[2] = {};
    for (int ch = 0; ch < numCh; ++ch)
        chPtrs[ch] = osBlock.getChannelPointer(static_cast<size_t>(ch));

    preamp.processBlock(chPtrs, numCh, numSamp);

    multirateEngine.downsample(buffer);

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
