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

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"preampGainR", 1}, "Right / Side Input Gain",
        juce::NormalisableRange<float>(-12.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGainR", 1}, "Right / Side Output Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"midSide", 1}, "Mid / Side", false));
    auto cutRange = juce::NormalisableRange<float>(20.0f, 20000.0f);
    cutRange.setSkewForCentre(1000.0f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"highPass", 1}, "High Pass", cutRange, 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lowPass", 1}, "Low Pass", cutRange, 20000.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"cutsEnabled", 1}, "Input Cuts", true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"channelLink", 1}, "Link L/R Gains", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"monoMakerEnabled", 1}, "Mono Maker", false));
    auto monoRange = juce::NormalisableRange<float>(20.0f, 500.0f);
    monoRange.setSkewForCentre(100.0f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"monoMakerFrequency", 1}, "Mono Maker Frequency", monoRange, 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"meterMode", 1}, "Meter Mode", juce::StringArray{"Peak", "RMS"}, 0));
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
    cuts.prepare(sampleRate);
    monoMaker.prepare(sampleRate, apvts.getRawParameterValue("monoMakerFrequency")->load());
    monoMakerActive = false;
    for (auto& engine : preamps) {
        engine.prepare(sampleRate);
        engine.setADAAEnabled(true);
        engine.setDecimationEnabled(false);
        // Drive and PAD are now session-rate input trims outside oversampling.
        engine.setDriveDB(0.0f);
    }
    for (auto& gain : inputGains) gain.reset(sampleRate, 0.01);
    for (auto& gain : outputGains) gain.reset(sampleRate, 0.01);
    updateParameters();
    for (auto& gain : inputGains) gain.setCurrentAndTargetValue(gain.getTargetValue());
    for (auto& gain : outputGains) gain.setCurrentAndTargetValue(gain.getTargetValue());
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
    for (auto& engine : preamps) {
        engine.setSampleRate(multirate.getEffectiveSampleRate());
        engine.setType(static_cast<dsp::PreampType>(getChoice("preampType")));
        engine.setBypassed(getBool("preampBypass"));
        engine.setCircuitEnabled(circuitActive);
    }
    inputBypassed = getBool("preampBypass");
    msActive = getBool("midSide") && getTotalNumInputChannels() == 2;
    const bool linked = getBool("channelLink") && !msActive;
    const bool newMonoMaker = getBool("monoMakerEnabled") && !inputBypassed && getTotalNumInputChannels() == 2;
    if (newMonoMaker != monoMakerActive) monoMaker.reset();
    monoMakerActive = newMonoMaker;
    monoMaker.setFrequency(getFloat("monoMakerFrequency"));
    const bool newCuts = getBool("cutsEnabled") && !inputBypassed;
    if (newCuts != cutsActive) cuts.reset();
    cutsActive = newCuts;
    cuts.setFrequencies(getFloat("highPass"), getFloat("lowPass"));
    static constexpr float padGains[] = {-20.0f, 0.0f, 10.0f};
    const float pad = padGains[std::clamp(getChoice("preampPad"),0,2)];
    inputGains[0].setTargetValue(juce::Decibels::decibelsToGain(getFloat("preampGain")+pad));
    inputGains[1].setTargetValue(juce::Decibels::decibelsToGain(getFloat(linked ? "preampGain" : "preampGainR")+pad));
    outputGains[0].setTargetValue(juce::Decibels::decibelsToGain(getFloat("outputGain")));
    outputGains[1].setTargetValue(juce::Decibels::decibelsToGain(getFloat(linked ? "outputGain" : "outputGainR")));
}

void OpenPreampProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    updateParameters();

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    const int numChannels = std::min(buffer.getNumChannels(),2);
    constexpr float invRootTwo = 0.7071067811865475f;
    // Orthonormal M/S: energy preserving, with an exact inverse after downsampling.
    if (msActive) for (int i=0;i<buffer.getNumSamples();++i) {
        const float l = buffer.getSample(0,i), r = buffer.getSample(1,i);
        buffer.setSample(0,i,(l+r)*invRootTwo);
        buffer.setSample(1,i,(l-r)*invRootTwo);
    }
    inLevel.push(buffer.getArrayOfReadPointers(),numChannels,buffer.getNumSamples());
    for (int i=0;i<buffer.getNumSamples();++i) {
        // In L/R mode temporarily encode/decode just this linear side filter.
        // In M/S mode the buffer is already encoded. All of this precedes oversampling.
        if (monoMakerActive) {
            if (msActive) buffer.setSample(1,i,monoMaker.process(buffer.getSample(1,i)));
            else {
                const float l = buffer.getSample(0,i), r = buffer.getSample(1,i);
                const float mid = (l+r)*invRootTwo;
                const float side = monoMaker.process((l-r)*invRootTwo);
                buffer.setSample(0,i,(mid+side)*invRootTwo);
                buffer.setSample(1,i,(mid-side)*invRootTwo);
            }
        }
        cuts.tick();
        for (int ch=0;ch<numChannels;++ch) {
            float x = buffer.getSample(ch,i);
            const float gain = inputGains[size_t(ch)].getNextValue();
            if (!inputBypassed) {
                if (cutsActive) x = cuts.process(ch,x);
                x *= gain;
            }
            buffer.setSample(ch,i,x);
        }
    }
    auto block = multirate.upsample(buffer);
    for (int ch=0;ch<numChannels;++ch) {
        float* channel = block.getChannelPointer(size_t(ch));
        preamps[size_t(ch)].processBlock(&channel,1,int(block.getNumSamples()));
    }
    multirate.downsample(buffer);
    for (int ch=0;ch<numChannels;++ch)
        for (int i=0;i<buffer.getNumSamples();++i) {
            latencyPadding.pushSample(ch,buffer.getSample(ch,i));
            buffer.setSample(ch,i,latencyPadding.popSample(ch)*outputGains[size_t(ch)].getNextValue());
        }
    // M/S meters observe their own encoded streams; the host always receives L/R.
    outLevel.push(buffer.getArrayOfReadPointers(),numChannels,buffer.getNumSamples());
    if (msActive) for (int i=0;i<buffer.getNumSamples();++i) {
        const float m = buffer.getSample(0,i), side = buffer.getSample(1,i);
        buffer.setSample(0,i,(m+side)*invRootTwo);
        buffer.setSample(1,i,(m-side)*invRootTwo);
    }
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
    if (xml && xml->hasTagName(apvts.state.getType())) {
        auto state = juce::ValueTree::fromXml(*xml);
        // Old stereo gain settings affected both channels. Preserve those values on restore.
        for (const auto pair : {std::pair<const char*,const char*>{"preampGain","preampGainR"}, {"outputGain","outputGainR"}})
            if (!state.getChildWithProperty("id",pair.second).isValid()) {
                auto old = state.getChildWithProperty("id",pair.first);
                if (old.isValid()) { auto copy = old.createCopy(); copy.setProperty("id",pair.second,nullptr); state.appendChild(copy,nullptr); }
            }
        if (!state.getChildWithProperty("id","cutsEnabled").isValid()) {
            juce::ValueTree cut("PARAM"); cut.setProperty("id","cutsEnabled",nullptr); cut.setProperty("value",0.0f,nullptr); state.appendChild(cut,nullptr);
        }
        for (const auto id : {"channelLink", "monoMakerEnabled", "monoMakerFrequency", "meterMode"})
            if (!state.getChildWithProperty("id",id).isValid()) {
                juce::ValueTree param("PARAM"); param.setProperty("id",id,nullptr);
                param.setProperty("value", juce::String(id) == "monoMakerFrequency" ? 20.0f : 0.0f,nullptr);
                state.appendChild(param,nullptr);
            }
        apvts.replaceState(state);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OpenPreampProcessor();
}
