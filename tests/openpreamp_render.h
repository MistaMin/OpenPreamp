// Matched processor exports for analysis/listening. Linear trims/cuts and
// latency compensation are identical for both circuit engines.
static int renderComparisons(const juce::File& directory) {
    if(!directory.createDirectory())return 2;
    constexpr double rate=48000;constexpr int block=64,frames=96000;
    OpenPreampProcessor p;p.setPlayConfigDetails(2,2,rate,block);p.prepareToPlay(rate,block);
    set(p,"cutsEnabled",0);set(p,"preampCircuit",1);set(p,"preampGain",0);set(p,"preampGainR",0);
    juce::MidiBuffer midi;juce::AudioBuffer<float> audio(2,block);
    std::vector<float> input(size_t(frames),0);juce::Random rng(17);
    const auto pi=juce::MathConstants<double>::pi;
    for(int i=0;i<frames;++i){double t=i/rate,x=0;
        if(t<.5)x=.5*(std::sin(2*pi*60*t)+std::sin(2*pi*7000*t));
        else if(t<.7)x=8*std::sin(2*pi*80*t);
        else if(t<1)x=.1*std::sin(2*pi*997*t);
        else if(t<1.5)x=.9*std::sin(2*pi*7000*t);
        else {double beat=std::fmod(t-1.5,.125);x=.55*std::exp(-beat*30)*std::sin(2*pi*73*t)+.25*std::exp(-beat*60)*(2*rng.nextDouble()-1)+.12*std::sin(2*pi*883*t);}
        input[size_t(i)]=float(x);
    }
    auto write=[&](const juce::String& name,const std::vector<float>& samples){
        auto stream=directory.getChildFile(name+".wav").createOutputStream();if(!stream)return false;
        stream->setPosition(0);stream->truncate();juce::WavAudioFormat format;
        std::unique_ptr<juce::OutputStream> output=std::move(stream);
        auto writer=format.createWriterFor(output,juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(1).withBitsPerSample(32).withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        const float* data=samples.data();return writer && writer->writeFromFloatArrays(&data,1,int(samples.size()));
    };
    if(!write("input",input))return 2;
    for(int model:{0,1,2,4})for(int hq:{0,1})for(int backend:{0,1}) {
        set(p,"preampType",float(model));set(p,"hqMode",float(hq));set(p,"circuitEngine",float(backend));
        set(p,"preampOversampling",hq?3.f:2.f); // Matched legacy 2x/4x comparison.
        // Enter each model/rate/backend through another type so both engines
        // start from their own reset histories, then settle on silence.
        set(p,"preampType",3);audio.clear();p.processBlock(audio,midi);set(p,"preampType",float(model));
        for(int i=0;i<375;++i){audio.clear();p.processBlock(audio,midi);}
        const int latency=p.getLatencySamples();const int total=((frames+latency+block-1)/block)*block;
        std::vector<float> rendered(size_t(total),0);
        for(int offset=0;offset<total;offset+=block){for(int ch=0;ch<2;++ch)for(int i=0;i<block;++i)audio.setSample(ch,i,offset+i<frames?input[size_t(offset+i)]:0);
            p.processBlock(audio,midi);for(int i=0;i<block;++i)rendered[size_t(offset+i)]=audio.getSample(0,i);}
        std::vector<float> aligned(rendered.begin()+latency,rendered.begin()+latency+frames);
        const char* names[]={"brit","ntype","fsf","off","atype"};
        auto name=juce::String(names[model])+"-"+(hq?"4x":"2x")+"-"+(backend?"reduced":"full");
        if(!write(name,aligned))return 2;std::printf("Rendered %s\n",name.toRawUTF8());std::fflush(stdout);
    }
    return 0;
}
