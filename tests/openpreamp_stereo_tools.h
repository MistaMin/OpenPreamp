// End-to-end checks for linked trims and the side-only first-order filter.
static void runStereoToolsChecks() {
    juce::MidiBuffer midi;
    auto prepare = [](OpenPreampProcessor& p, double rate=48000.0) {
        p.setPlayConfigDetails(2,2,rate,256); p.prepareToPlay(rate,256);
    };
    auto clean = [](OpenPreampProcessor& p) {
        set(p,"preampType",3); set(p,"cutsEnabled",0); set(p,"preampCircuit",0);
    };
    {
        OpenPreampProcessor p; clean(p);
        set(p,"preampGain",3);set(p,"outputGain",3);set(p,"preampGainR",-3);set(p,"outputGainR",-3);set(p,"channelLink",1);
        for(bool linked : {true,false}) {
            set(p,"channelLink",linked ? 1 : 0);prepare(p);
            juce::AudioBuffer<float> b(2,256);b.clear();b.setSample(0,0,.1f);b.setSample(1,0,.1f);p.processBlock(b,midi);
            check(std::abs(b.getSample(1,p.getLatencySamples())-.1f*juce::Decibels::decibelsToGain(linked ? 6.0f : -6.0f))<1e-6f,
                linked ? "Link applies left input AND output trims to right audio" : "Unlink restores the independent right trim values");
        }
        set(p,"channelLink",1);set(p,"midSide",1);prepare(p);
        juce::AudioBuffer<float> b(2,256);b.clear();b.setSample(0,0,.1f);b.setSample(1,0,-.1f);p.processBlock(b,midi);
        check(std::abs(b.getSample(0,p.getLatencySamples())-.1f*juce::Decibels::decibelsToGain(-6.0f))<1e-6f,
            "M/S ignores Link and uses independent Side input and output trims");
    }
    for(bool ms : {false,true}) {
        OpenPreampProcessor p;set(p,"channelLink",1);set(p,"midSide",ms ? 1 : 0);prepare(p);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        bool linkVisible=false,linkEnabled=false,linkActive=false,rightEnabled=true,rightDimmed=false;int gainControls=0;
        for(auto* surface : editor->getChildren())for(auto* child : surface->getChildren()) {
            if(auto* button=dynamic_cast<SmallToggle*>(child);button && button->getButtonText()=="LINK"){linkVisible=button->isVisible();linkEnabled=button->isEnabled();linkActive=button->getActive();}
            if(auto* panel=dynamic_cast<SectionPanel*>(child);panel && panel->getX()==508)
                for(auto* control : panel->getChildren())if(auto* knob=dynamic_cast<RotaryKnob*>(control)) {
                    ++gainControls;rightEnabled &= knob->isEnabled();rightDimmed |= !knob->getActive();
                }
        }
        check(gainControls==2 && linkVisible && linkEnabled==!ms && linkActive==!ms && rightEnabled==ms && rightDimmed==!ms,
            ms ? "M/S keeps Link visible but disabled and dimmed, with both Side controls enabled" : "Link greys and disables both right controls in L/R");
        const auto image=editor->createComponentSnapshot(editor->getLocalBounds());
        juce::FileOutputStream file(juce::File(ms ? "/private/tmp/openpreamp-100-ms.png" : "/private/tmp/openpreamp-100-linked.png"));
        file.setPosition(0);file.truncate();juce::PNGImageFormat png;png.writeImageToStream(image,file);
    }
    {
        LevelTracker input,output;MeterPanel meter(input,output);meter.setExternallyDriven();meter.setChannel(0);
        LevelTracker::Reading in,out;out.peak[0]=.5f;out.meanSquare[0]=.01f;
        meter.setPeakMode(false);for(int n=0;n<60;++n)meter.step(in,out,1.0f/30);
        const float rmsNeedle=meter.needle(),rmsDb=meter.displayedDb();
        meter.setPeakMode(true);for(int n=0;n<60;++n)meter.step(in,out,1.0f/30);
        check(meter.needle()>rmsNeedle && std::abs(rmsDb+20)<1e-4f && std::abs(meter.displayedDb()+6.0206f)<1e-4f,
            "Peak/RMS switches both needle measurement and numeric dBFS readout");
        meter.setPeakMode(false);out.peak[0]=1.1f;meter.step(in,out,1.0f/30);
        check(meter.clipped(),"RMS mode retains peak-based clip indication");
        OpenPreampProcessor p;
        for(int mode : {0,1}) {
            set(p,"meterMode",float(mode));std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());int matching=0;
            for(auto* surface : editor->getChildren())for(auto* child : surface->getChildren())
                if(auto* vu=dynamic_cast<MeterPanel*>(child);vu && vu->isPeakMode()==(mode==0))++matching;
            check(matching==2,"Both independent VUs restore the shared Peak/RMS choice");
            if(mode==1) {
                const auto image=editor->createComponentSnapshot(editor->getLocalBounds());
                juce::FileOutputStream file(juce::File("/private/tmp/openpreamp-100-rms.png"));
                file.setPosition(0);file.truncate();juce::PNGImageFormat png;png.writeImageToStream(image,file);
            }
        }
    }
    {
        OpenPreampProcessor p;std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());int buttons=0;
        for(auto* surface : editor->getChildren())for(auto* child : surface->getChildren())if(auto* choice=dynamic_cast<CycleButton*>(child)) {
            juce::String label;juce::TextButton* key=nullptr;
            for(auto* control : choice->getChildren()) {
                if(auto* title=dynamic_cast<juce::Label*>(control))label=title->getText();
                if(auto* button=dynamic_cast<juce::TextButton*>(control))key=button;
            }
            if(key && (label=="MODE" || label=="VU")) {
                const char* id=label=="MODE" ? "meterMode" : "meterSource";
                const float original=p.apvts.getRawParameterValue(id)->load();
                key->onClick();const float toggled=p.apvts.getRawParameterValue(id)->load();
                key->onClick();const float restored=p.apvts.getRawParameterValue(id)->load();
                check(std::abs(toggled-(1-original))<1e-6f && std::abs(restored-original)<1e-6f,"Two-choice meter key changes host parameter directly on each click");
                ++buttons;
            }
        }
        check(buttons==4,"Both VUs expose direct-click MODE and source keys");
        editor->setSize(400,296);
        const auto image=editor->createComponentSnapshot(editor->getLocalBounds());
        juce::FileOutputStream file(juce::File("/private/tmp/openpreamp-100-small.png"));
        file.setPosition(0);file.truncate();juce::PNGImageFormat png;png.writeImageToStream(image,file);
    }
    for(double rate : {44100.0,48000.0,96000.0})for(bool ms : {false,true}) {
        auto amplitude = [&](double frequency,bool side,bool enabled) {
            OpenPreampProcessor p;clean(p);set(p,"midSide",ms ? 1 : 0);set(p,"monoMakerEnabled",enabled ? 1 : 0);set(p,"monoMakerFrequency",200);prepare(p,rate);
            juce::AudioBuffer<float> b(2,256);double energy=0;int phase=0,count=0;
            for(int block=0;block<int(std::ceil(rate/256.0));++block) {
                for(int i=0;i<256;++i) {
                    const float x=float(.1*std::sin(6.283185307179586*frequency*double(phase++)/rate));
                    b.setSample(0,i,x);b.setSample(1,i,side ? -x : x);
                }
                p.processBlock(b,midi);
                for(int i=0;i<256;++i)if(block*256+i>=int(rate/2) && block*256+i<int(rate)) { energy+=double(b.getSample(0,i))*b.getSample(0,i); ++count; }
            }
            return std::sqrt(energy/count);
        };
        const double dry=amplitude(20,true,false),low=amplitude(20,true,true),octave=amplitude(40,true,true);
        check(std::abs(low/dry-.0995)<.003 && octave/low>1.9 && octave/low<2.05,
            "Mono maker attenuates only Side bass at 6 dB/octave across host rates and L/R or M/S");
        check(std::abs(amplitude(20,false,true)/dry-1)<.005,"Mono maker preserves Mid bass and correlated mono audio");
    }
    {
        OpenPreampProcessor p;clean(p);set(p,"monoMakerEnabled",1);set(p,"preampBypass",1);prepare(p);
        juce::AudioBuffer<float> b(2,256);b.clear();b.setSample(0,0,.1f);b.setSample(1,0,-.1f);p.processBlock(b,midi);
        check(std::abs(b.getSample(0,p.getLatencySamples())-.1f)<1e-6f,"Preamp bypass also bypasses mono maker");
        auto old=p.apvts.copyState();
        for(const char* id : {"channelLink","monoMakerEnabled","monoMakerFrequency"})old.removeChild(old.getChildWithProperty("id",id),nullptr);
        juce::MemoryBlock data;auto xml=old.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,data);
        set(p,"channelLink",1);set(p,"monoMakerFrequency",300);p.setStateInformation(data.getData(),int(data.getSize()));
        check(p.apvts.getRawParameterValue("channelLink")->load()<.5f && p.apvts.getRawParameterValue("monoMakerEnabled")->load()<.5f && std::abs(p.apvts.getRawParameterValue("monoMakerFrequency")->load()-20)<1e-4f,
            "Old sessions retain independent gains and restore new mono maker off at 20 Hz");
    }
}
