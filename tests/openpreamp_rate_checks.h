static void runRateChecks() {
    juce::MidiBuffer midi;juce::AudioBuffer<float> audio(2,128);
    OpenPreampProcessor p;
    for(double rate:{48000.,96000.}) {
        p.setPlayConfigDetails(2,2,rate,128);p.prepareToPlay(rate,128);const int latency=p.getLatencySamples();
        for(int model:{0,1,2,4})for(int mode:{0,1,2})for(int os:{0,1,2,3}) {
            // mode: Standard, Circuit, HQ. HQ legacy switch stays off here.
            set(p,"preampType",float(model));set(p,"preampCircuit",mode==0?0.f:1.f);set(p,"circuitEngine",mode==1?1.f:0.f);set(p,"hqMode",0);set(p,"preampOversampling",float(os));
            for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)audio.setSample(ch,i,float(.3*std::sin(.2*i)));
            p.processBlock(audio,midi);
            const int factor=os==0?(mode==2?2:1):1<<(os-1);
            bool finite=true;for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)finite&=std::isfinite(audio.getSample(ch,i));
            check(p.getEffectiveOversampleFactor()==factor && p.getProcessingSampleRate()==rate*factor && finite,"all engines use the selected 1x/2x/4x rate with finite output");
            check(p.isADAAActive()==(factor==1 && mode!=2) && p.getLatencySamples()==latency,"ADAA is exclusive to native Standard/Circuit; latency remains fixed");
        }
    }
    set(p,"preampCircuit",1);set(p,"circuitEngine",1);set(p,"hqMode",0);
    auto old=p.apvts.copyState();old.removeChild(old.getChildWithProperty("id","preampOversampling"),nullptr);
    auto xml=old.createXml();juce::MemoryBlock legacy;juce::AudioProcessor::copyXmlToBinary(*xml,legacy);
    p.setStateInformation(legacy.getData(),int(legacy.getSize()));audio.clear();p.processBlock(audio,midi);
    check(p.getEffectiveOversampleFactor()==2 && !p.isADAAActive(),"old Circuit projects retain 2x and no ADAA");
    set(p,"hqMode",1);audio.clear();p.processBlock(audio,midi);
    check(p.getEffectiveOversampleFactor()==4 && !p.isADAAActive(),"old 4x automation still controls Default oversampling");
    juce::MemoryBlock saved;p.getStateInformation(saved);OpenPreampProcessor restored;restored.setStateInformation(saved.getData(),int(saved.getSize()));
    restored.prepareToPlay(48000,128);audio.clear();restored.processBlock(audio,midi);
    check(restored.getEffectiveOversampleFactor()==4 && !restored.isADAAActive(),"legacy rate policy survives another state round trip");
    set(p,"preampOversampling",1);audio.clear();p.processBlock(audio,midi);
    check(p.getEffectiveOversampleFactor()==1 && p.isADAAActive(),"explicit 1x overrides legacy 4x and enables Circuit ADAA");
    set(p,"preampBypass",1);set(p,"preampOversampling",3);audio.clear();p.processBlock(audio,midi);
    check(p.getEffectiveOversampleFactor()==1 && !p.isADAAActive(),"bypass disables oversampling and ADAA");
    // Verify actual native Circuit samples, not only the reported flag.
    OpenPreampProcessor native;set(native,"circuitEngine",1);set(native,"cutsEnabled",0);set(native,"preampType",0);
    native.prepareToPlay(48000,128);auto bank=std::make_unique<dsp::ReducedCircuitBank>();bank->prepare(48000);
    dsp::ReducedCircuitKernel expected;expected.configure(bank->get(0,1),true);
    std::vector<float> reference(512);float error=0;
    for(int block=0;block<4;++block){
        for(int i=0;i<128;++i){const float x=float(2*std::sin(.37*(block*128+i)));for(int ch=0;ch<2;++ch)audio.setSample(ch,i,x);reference[size_t(block*128+i)]=float(expected.process(x));}
        native.processBlock(audio,midi);
        for(int i=0;i<128;++i){int inputIndex=block*128+i-native.getLatencySamples();if(inputIndex>=0)error=std::max(error,std::abs(audio.getSample(0,i)-reference[size_t(inputIndex)]));}
    }
    check(error<2e-6f,"native Circuit processor applies the prepared ADAA kernel to actual audio");
}
