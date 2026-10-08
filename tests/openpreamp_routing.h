// Focused routing checks exercise complete host-rate and oversampled paths.
static void runRoutingChecks() {
    juce::MidiBuffer midi;
    auto prepare = [](OpenPreampProcessor& p, double rate=48000.0) {
        p.setPlayConfigDetails(2,2,rate,256); p.prepareToPlay(rate,256);
    };
    auto clean = [](OpenPreampProcessor& p) {
        set(p,"preampType",3); set(p,"preampCircuit",0); set(p,"cutsEnabled",0);
    };
    {
        OpenPreampProcessor p; clean(p); set(p,"preampGainR",6); set(p,"outputGainR",6); prepare(p);
        juce::AudioBuffer<float> b(2,256); b.clear(); b.setSample(0,0,.1f); b.setSample(1,0,.1f); p.processBlock(b,midi);
        const int delay=p.getLatencySamples();
        check(std::abs(b.getSample(0,delay)-.1f)<1e-6f && std::abs(b.getSample(1,delay)-.1f*juce::Decibels::decibelsToGain(12.0f))<1e-6f,
              "independent input/output trims affect only their own channel at session rate");
    }
    for (bool antiPhase : {false,true}) {
        OpenPreampProcessor p; clean(p); set(p,"midSide",1); prepare(p);
        juce::AudioBuffer<float> b(2,256); b.clear(); b.setSample(0,0,.25f); b.setSample(1,0,antiPhase ? -.25f : .25f);
        p.processBlock(b,midi); const int delay=p.getLatencySamples();
        const auto in=p.getInputLevel().read();
        check(std::abs(b.getSample(0,delay)-.25f)<1e-6f && std::abs(b.getSample(1,delay)-(antiPhase ? -.25f : .25f))<1e-6f,
              "M/S encode/decode round trip preserves correlated and anti-phase stereo");
        check(in.peak[antiPhase ? 0 : 1]<1e-7f && std::abs(in.peak[antiPhase ? 1 : 0]-.25f*std::sqrt(2.0f))<1e-6f,
              "M/S input meters observe separate Mid and Side streams");
    }
    {
        OpenPreampProcessor p; clean(p); set(p,"midSide",1); set(p,"outputGainR",-6); prepare(p);
        juce::AudioBuffer<float> b(2,256); b.clear(); b.setSample(0,0,.25f); b.setSample(1,0,.10f); p.processBlock(b,midi);
        const float side=.075f*juce::Decibels::decibelsToGain(-6.0f); const int delay=p.getLatencySamples();
        check(std::abs(b.getSample(0,delay)-(.175f+side))<1e-6f && std::abs(b.getSample(1,delay)-(.175f-side))<1e-6f,
              "Side output trim acts before decoder and changes stereo width");
    }
    {
        LevelTracker input,output; MeterPanel left(input,output),right(input,output);
        left.setExternallyDriven();right.setExternallyDriven();left.setChannel(0);right.setChannel(1);
        LevelTracker::Reading in,out;out.peak[0]=1.1f;out.peak[1]=.05f;out.meanSquare[0]=.03f;out.meanSquare[1]=.0001f;
        for(int n=0;n<30;++n){left.step(in,out,1.0f/30);right.step(in,out,1.0f/30);}
        check(left.clipped() && !right.clipped() && left.holdDb()>right.holdDb()+20 && left.needle()>right.needle(),
              "dual-mono VU RMS, peak hold and clip lamps are independent");
    }
    // Physical filter cutoffs must be unchanged as the circuit switches 1x/2x/4x.
    for(double rate : {44100.0,48000.0,96000.0}) {
        OpenPreampProcessor p; clean(p); set(p,"cutsEnabled",1); set(p,"highPass",200);set(p,"lowPass",5000);prepare(p,rate);
        juce::AudioBuffer<float> b(2,256);
        auto amplitude = [&](double frequency) {
            double energy=0;int phase=0;
            for(int block=0;block<80;++block) {
                for(int i=0;i<256;++i) {const float x=float(.1*std::sin(6.283185307179586*frequency*double(phase++)/rate)); b.setSample(0,i,x);b.setSample(1,i,x);}
                p.processBlock(b,midi);
                if(block>=40)for(int i=0;i<256;++i)energy+=double(b.getSample(0,i))*b.getSample(0,i);
            }
            return std::sqrt(energy/(40*256));
        };
        const double pass=amplitude(1000),bass=amplitude(20),treble=amplitude(15000);
        check(bass<pass*.025 && treble<pass*.12,"12 dB/octave session-rate cuts reject low/high tones at multiple host rates");
        for(int mode=0;mode<3;++mode) {
            set(p,"preampType",1);set(p,"preampCircuit",float(mode>0));set(p,"hqMode",float(mode==2)); b.clear();p.processBlock(b,midi);
            check(p.getSessionProcessorSampleRate()==rate && p.getProcessingSampleRate()==rate*(mode==0 ? 1 : mode==1 ? 2 : 4),
                  "cut/gain/M/S domain stays at host rate independently of preamp oversampling");
        }
    }
    for(int mode=0;mode<3;++mode) {
        OpenPreampProcessor encoded, reference;
        for(auto* p : {&encoded,&reference}) {
            set(*p,"preampGainR",6);set(*p,"outputGainR",-3);
            set(*p,"preampCircuit",float(mode>0));set(*p,"hqMode",float(mode==2));
            set(*p,"highPass",80);set(*p,"lowPass",12000);
        }
        set(encoded,"midSide",1);set(encoded,"monoMakerEnabled",1);set(encoded,"monoMakerFrequency",100);prepare(encoded);prepare(reference);
        SessionMonoMaker manualSide;manualSide.prepare(48000,100);
        juce::AudioBuffer<float> a(2,256),b(2,256);
        for(int model : {0,1,2,4}) {
            set(encoded,"preampType",float(model));set(reference,"preampType",float(model));
            float error=0;bool finite=true;
            for(int block=0;block<3;++block) {
                for(int i=0;i<256;++i) {
                    const float l=.1f*std::sin(float(i+block*256)*.07f),r=.08f*std::cos(float(i+block*256)*.11f);
                    a.setSample(0,i,l);a.setSample(1,i,r);
                    b.setSample(0,i,(l+r)*.7071067811865475f);b.setSample(1,i,manualSide.process((l-r)*.7071067811865475f));
                }
                encoded.processBlock(a,midi);reference.processBlock(b,midi);
                for(int i=0;i<256;++i) {
                    const float m=b.getSample(0,i),side=b.getSample(1,i);
                    const float l=(m+side)*.7071067811865475f,r=(m-side)*.7071067811865475f;
                    finite &= std::isfinite(a.getSample(0,i)) && std::isfinite(a.getSample(1,i));
                    error=std::max(error,std::abs(a.getSample(0,i)-l));error=std::max(error,std::abs(a.getSample(1,i)-r));
                }
            }
            check(finite && error<1e-5f,"all preamp models match manual M/S + native Side filter before processing and decoding after native/2x/4x");
        }
    }
    // Real nonlinear right-channel processing must not contaminate the silent left channel.
    for(int mode=0;mode<3;++mode) {
        OpenPreampProcessor p;set(p,"cutsEnabled",0);set(p,"preampCircuit",float(mode>0));set(p,"hqMode",float(mode==2));prepare(p);
        juce::AudioBuffer<float> b(2,256);float leftPeak=0,rightPeak=0;
        for(int block=0;block<5;++block) {
            b.clear();for(int i=0;i<256;++i)b.setSample(1,i,.1f*std::sin(float(i+block*256)*.04f));p.processBlock(b,midi);
            leftPeak=std::max(leftPeak,b.getMagnitude(0,0,256));rightPeak=std::max(rightPeak,b.getMagnitude(1,0,256));
        }
        check(leftPeak<1e-5f && rightPeak>1e-4f,"preamp state is independent between channels in native/2x/HQ modes");
    }
    {
        OpenPreampProcessor p;set(p,"preampGain",9);set(p,"outputGain",-3);
        auto old=p.apvts.copyState();
        for(const char* id : {"preampGainR","outputGainR","midSide","cutsEnabled","highPass","lowPass"})old.removeChild(old.getChildWithProperty("id",id),nullptr);
        juce::MemoryBlock data;auto xml=old.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,data);
        OpenPreampProcessor restored;restored.setStateInformation(data.getData(),int(data.getSize()));
        check(std::abs(restored.apvts.getRawParameterValue("preampGainR")->load()-9)<1e-4f && std::abs(restored.apvts.getRawParameterValue("outputGainR")->load()+3)<1e-4f && restored.apvts.getRawParameterValue("cutsEnabled")->load()==0,
              "old stereo state migrates right gains and leaves newly added cuts disabled");
    }
}
