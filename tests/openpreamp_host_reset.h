static int runHostResetChecks() {
    juce::MidiBuffer midi;
    for(double rate:{44100.,48000.,96000.})for(int block:{64,257}) {
        OpenPreampProcessor p;p.setPlayConfigDetails(2,2,rate,block);p.prepareToPlay(rate,block);
        for(int model:{0,1,2,4,3})for(int mode:{0,1,2})for(int factor:{1,2,3}) {
            set(p,"preampType",float(model));set(p,"preampCircuit",mode==0?0.f:1.f);set(p,"circuitEngine",mode==1?1.f:0.f);set(p,"preampOversampling",float(factor));
            set(p,"preampGain",6);set(p,"outputGain",-3);set(p,"monoMakerEnabled",1);set(p,"monoMakerFrequency",130);set(p,"highPass",75);set(p,"lowPass",14000);
            juce::AudioBuffer<float> audio(2,block);
            auto render=[&]{std::vector<float> samples;for(int b=0;b<12;++b){for(int ch=0;ch<2;++ch)for(int i=0;i<block;++i)audio.setSample(ch,i,float(.2*std::sin(.117*(b*block+i)+ch*.3)));p.processBlock(audio,midi);for(int ch=0;ch<2;++ch)for(int i=0;i<block;++i)samples.push_back(audio.getSample(ch,i));}return samples;};
            // Prime configuration and dirty every active history before reset.
            render();p.reset();const auto first=render();render();p.reset();const auto second=render();
            check(first==second,"host reset produces sample-exact repeated renders across models/rates/blocks/engines/oversampling");
        }
    }
    std::printf("%d checks, %d failures\n",checks,failures);return failures?1:0;
}
