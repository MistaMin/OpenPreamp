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
        juce::ParameterID{"preampCircuit", 1}, "Modeled Preamp", true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1}, "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"hqMode", 1}, "Legacy 4x Oversampling", false));
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
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"circuitEngine", 1}, "Circuit Engine", juce::StringArray{"HQ", "Circuit"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"preampOversampling", 2}, "Preamp Oversampling", juce::StringArray{"Legacy", "1x", "2x", "4x"}, 2));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"gainCompensation", 3}, "Input / Output Gain Compensation", false));
    return {params.begin(), params.end()};
}

OpenPreampProcessor::OpenPreampProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    static constexpr const char* ids[] = {"preampType","preampPad","preampGain","preampBypass","preampCircuit","outputGain","hqMode","meterSource","preampGainR","outputGainR","midSide","highPass","lowPass","cutsEnabled","channelLink","monoMakerEnabled","monoMakerFrequency","meterMode","circuitEngine","preampOversampling","gainCompensation"};
    static_assert(std::size(ids)==ParameterCount);
    for(size_t i=0;i<parameterValues.size();++i)parameterValues[i]=apvts.getRawParameterValue(ids[i]);
    apvts.addParameterListener("preampOversampling",this);
}

OpenPreampProcessor::~OpenPreampProcessor() { apvts.removeParameterListener("preampOversampling",this); }

void OpenPreampProcessor::parameterChanged(const juce::String&, float) {
    if(!settingInitialRate.load(std::memory_order_relaxed)) markRateChosen();
}

void OpenPreampProcessor::chooseInitialRate() {
    if(rateChosen.load(std::memory_order_relaxed)) return;
    const bool full=value(Circuit)>0.5f && value(Engine)<0.5f;
    const int choice=full ? 2 : 1;
    settingInitialRate.store(true,std::memory_order_relaxed);
    auto* rate=apvts.getParameter("preampOversampling");
    rate->setValueNotifyingHost(rate->convertTo0to1(float(choice)));
    settingInitialRate.store(false,std::memory_order_relaxed);
}

bool OpenPreampProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto output = layout.getMainOutputChannelSet();
    return (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo())
        && layout.getMainInputChannelSet() == output;
}

void OpenPreampProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    chooseInitialRate();
    sampleRateAtomic.store(sampleRate, std::memory_order_relaxed);
    multirate.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    multirate.setMode(dsp::OversampleMode::FourX);
    fixedLatency = int(std::ceil(multirate.getLatencySamples()));
    latencyPadding.setMaximumDelayInSamples(fixedLatency + 1);
    latencyPadding.prepare({sampleRate, juce::uint32(samplesPerBlock), juce::uint32(getTotalNumOutputChannels())});
    setLatencySamples(fixedLatency);
    reducedBank=std::make_unique<dsp::ReducedCircuitBank>();
    reducedBank->prepare(sampleRate);
    reducedType=-1;reducedFactor=0;reducedActive=false;
    cuts.prepare(sampleRate);
    monoMaker.prepare(sampleRate, apvts.getRawParameterValue("monoMakerFrequency")->load());
    monoMakerActive = false;
    for (auto& engine : preamps) {
        engine.prepare(sampleRate);
        engine.reserveProcessingSamples(std::max(1,samplesPerBlock)*4);
        engine.setADAAEnabled(true);
        engine.setLightTablesEnabled(true);
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
    const bool active=!(value(Bypass)>0.5f) && int(value(Type))!=3;
    const bool circuitActive=active && value(Circuit)>0.5f;
    const bool nextReduced=circuitActive && int(value(Engine))==1;
    const int selection=std::clamp(int(value(Oversampling)),0,3);
    const int defaultFactor=circuitActive ? (value(HQ)>0.5f ? 4 : nextReduced && !legacyRateDefaults.load(std::memory_order_relaxed) ? 1 : 2) : 1;
    const int factor=active ? (selection==0 ? defaultFactor : 1<<(selection-1)) : 1;
    const bool antialias=active && factor==1 && (!circuitActive || nextReduced);
    multirate.setMode(factor == 4 ? dsp::OversampleMode::FourX : factor == 2 ? dsp::OversampleMode::TwoX : dsp::OversampleMode::Native);
    effectiveFactorAtomic.store(factor, std::memory_order_relaxed);
    adaaActiveAtomic.store(antialias,std::memory_order_relaxed);
    latencyPadding.setDelay(float(fixedLatency) - multirate.getLatencySamples());
    const int type=int(value(Type));
    if(nextReduced && (!reducedActive || reducedType!=type || reducedFactor!=factor)) {
        for(auto& kernel : reducedKernels)kernel.configure(reducedBank->get(type,factor),antialias);
        reducedType=type;reducedFactor=factor;
    }
    reducedActive=nextReduced;
    for (auto& engine : preamps) {
        engine.setADAAEnabled(antialias);
        engine.setSampleRate(multirate.getEffectiveSampleRate());
        engine.setType(static_cast<dsp::PreampType>(int(value(Type))));
        engine.setBypassed((value(Bypass) > 0.5f));
        engine.setCircuitEnabled(circuitActive && !reducedActive);
    }
    inputBypassed = (value(Bypass) > 0.5f);
    msActive = (value(MS) > 0.5f) && getTotalNumInputChannels() == 2;
    const bool linked = (value(Link) > 0.5f) && !msActive;
    const bool newMonoMaker = (value(MonoEnabled) > 0.5f) && !inputBypassed && getTotalNumInputChannels() == 2;
    if (newMonoMaker != monoMakerActive) monoMaker.reset();
    monoMakerActive = newMonoMaker;
    monoMaker.setFrequency(value(MonoFrequency));
    const bool newCuts = (value(Cuts) > 0.5f) && !inputBypassed;
    if (newCuts != cutsActive) cuts.reset();
    cutsActive = newCuts;
    cuts.setFrequencies(value(HP), value(LP));
    static constexpr float padGains[] = {-20.0f, 0.0f, 10.0f};
    const float pad = padGains[std::clamp(int(value(Pad)),0,2)];
    inputGains[0].setTargetValue(juce::Decibels::decibelsToGain(value(Input)+pad));
    inputGains[1].setTargetValue(juce::Decibels::decibelsToGain(value(linked ? Input : InputR)+pad));
    outputGains[0].setTargetValue(juce::Decibels::decibelsToGain(value(Output)));
    outputGains[1].setTargetValue(juce::Decibels::decibelsToGain(value(linked ? Output : OutputR)));
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
        if(reducedActive) {
            auto& kernel=reducedKernels[size_t(ch)];
            kernel.processBlock(channel,block.getNumSamples());
        } else preamps[size_t(ch)].processBlock(&channel,1,int(block.getNumSamples()));
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
    state.setProperty("rateChosen",rateChosen.load(std::memory_order_relaxed),nullptr);
    state.setProperty("legacyRateDefaults",legacyRateDefaults.load(std::memory_order_relaxed),nullptr);
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
        for (const auto id : {"channelLink", "monoMakerEnabled", "monoMakerFrequency", "meterMode", "gainCompensation"})
            if (!state.getChildWithProperty("id",id).isValid()) {
                juce::ValueTree param("PARAM"); param.setProperty("id",id,nullptr);
                param.setProperty("value", juce::String(id) == "monoMakerFrequency" ? 20.0f : 0.0f,nullptr);
                state.appendChild(param,nullptr);
            }
        // 1.0.0 and earlier sessions retain their full circuit sound/CPU path.
        if(!state.getChildWithProperty("id","circuitEngine").isValid()) {
            juce::ValueTree engine("PARAM");engine.setProperty("id","circuitEngine",nullptr);
            engine.setProperty("value",0.0f,nullptr);state.appendChild(engine,nullptr);
        }
        // Preserve old Standard 1x / modeled 2x-or-4x sessions. Newly created
        // Circuit instances use 1x; explicit selections override either policy.
        const bool missingRate=!state.getChildWithProperty("id","preampOversampling").isValid();
        legacyRateDefaults.store(missingRate || bool(state.getProperty("legacyRateDefaults",false)),std::memory_order_relaxed);
        if(missingRate){juce::ValueTree rate("PARAM");rate.setProperty("id","preampOversampling",nullptr);rate.setProperty("value",0.f,nullptr);state.appendChild(rate,nullptr);}
        apvts.replaceState(state);
        // Older saved states retain their dynamic legacy/explicit rate policy.
        rateChosen.store(bool(state.getProperty("rateChosen",true)),std::memory_order_relaxed);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OpenPreampProcessor();
}
