// Offline measurement host. Keeps the processor's latency in the capture;
// analysis removes the explicitly logged transport delay, not the DUT phase.
static int renderMeasurementBatch(const juce::File& batchFile) {
    const auto batch=juce::JSON::parse(batchFile);
    const auto* jobs=batch.getArray();if(!jobs)return 2;
    for(const auto& job:*jobs) {
        const juce::File input(job["input"].toString()),output(job["output"].toString());
        const int block=int(job["block_size"]),bits=int(job["bit_depth"]);
        if(block<1 || block>8192 || (bits!=16 && bits!=24 && bits!=32) || output.existsAsFile())return 2;
        juce::WavAudioFormat format;
        auto reader=std::unique_ptr<juce::AudioFormatReader>(format.createReaderFor(input.createInputStream().release(),true));
        if(!reader || reader->numChannels!=1 || reader->bitsPerSample!=unsigned(bits) || (reader->sampleRate!=48000 && reader->sampleRate!=96000))return 2;
        OpenPreampProcessor p;
        const auto* parameters=job["parameters"].getDynamicObject();if(!parameters)return 2;
        for(const auto& item:parameters->getProperties()) {
            auto* param=p.apvts.getParameter(item.name.toString());if(!param)return 2;
            const auto value=float(item.value);
            if(!std::isfinite(value) || value<param->getNormalisableRange().start || value>param->getNormalisableRange().end)return 2;
            param->setValueNotifyingHost(param->convertTo0to1(value));
        }
        p.setPlayConfigDetails(2,2,reader->sampleRate,block);p.setNonRealtime(true);p.prepareToPlay(reader->sampleRate,block);
        if(!output.getParentDirectory().createDirectory())return 2;
        std::unique_ptr<juce::OutputStream> stream=output.createOutputStream();
        auto writer=format.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(reader->sampleRate).withNumChannels(1).withBitsPerSample(bits));
        if(!writer)return 2;
        juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;
        const auto total=reader->lengthInSamples+p.getLatencySamples()+block;
        for(juce::int64 offset=0;offset<total;offset+=block){
            audio.clear();const int n=int(std::min<juce::int64>(block,reader->lengthInSamples-offset));
            if(n>0 && !reader->read(&audio,0,n,offset,true,true))return 2;
            audio.copyFrom(1,0,audio,0,0,block);p.processBlock(audio,midi);
            const float* data=audio.getReadPointer(0);if(!writer->writeFromFloatArrays(&data,1,block))return 2;
        }
        writer.reset();
        auto meta=std::make_unique<juce::DynamicObject>();
        meta->setProperty("plugin_version",JucePlugin_VersionString);meta->setProperty("sample_rate",reader->sampleRate);
        meta->setProperty("bit_depth",bits);meta->setProperty("block_size",block);meta->setProperty("offline",true);
        meta->setProperty("reported_latency_samples",p.getLatencySamples());
        meta->setProperty("effective_factor",p.getEffectiveOversampleFactor());meta->setProperty("adaa",p.isADAAActive());
        auto logged=std::make_unique<juce::DynamicObject>();
        for(auto* parameter:p.getParameters())if(auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(parameter))
            logged->setProperty(ranged->paramID,ranged->convertFrom0to1(ranged->getValue()));
        meta->setProperty("parameters",juce::var(logged.release()));
        if(!output.getSiblingFile(output.getFileName()+".json").replaceWithText(juce::JSON::toString(juce::var(meta.release()))))return 2;
        std::printf("Captured %s\n",output.getFileName().toRawUTF8());std::fflush(stdout);
    }
    return 0;
}
