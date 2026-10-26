static int runMeterChecks() {
    {
        LevelTracker input, output; MeterPanel panel(input,output);panel.setExternallyDriven();
        for(int mode : {openpreamp::RMS,openpreamp::K12,openpreamp::K14,openpreamp::K20}) {
            panel.setMeasurement(mode,-18.04f,false,0);
            for(int n=0;n<120;++n)panel.setMeasurement(mode,n%2?-18.06f:-18.04f,false,0);
            check(std::abs(panel.numericReadoutDb()+18.f)<.001f,"RMS/K readout does not flicker around a decimal boundary");
            panel.setMeasurement(mode,-18.2f,false,0);
            check(std::abs(panel.numericReadoutDb()+18.2f)<.001f,"RMS/K readout tracks a real change immediately");
            panel.setInputSource(!panel.isInputSource());panel.setMeasurement(mode,-18.06f,false,0);
            check(std::abs(panel.numericReadoutDb()+18.1f)<.001f,"source change discards previous rounded RMS value");
            panel.setMeasurement(mode,-99.f,false,0);check(panel.numericReadoutDb()<=-90,"RMS silence clears numeric readout");
        }
        panel.setMeasurement(openpreamp::Peak,-18.06f,false,0);
        check(std::abs(panel.numericReadoutDb()+18.06f)<.0001f,"RMS display stabilization does not change peak mode");
    }
    OpenPreampProcessor p;
    check(p.getMeterMode()==openpreamp::VU,"new instances default to dedicated VU detector");
    set(p,"preampBypass",1);set(p,"preampCircuit",0);set(p,"meterSource",0);
    p.setPlayConfigDetails(2,2,48000,480);p.prepareToPlay(48000,480);
    juce::MidiBuffer midi;juce::AudioBuffer<float> audio(2,480);
    auto feed=[&]{for(int n=0;n<400;++n){for(int i=0;i<480;++i){const float x=.12589254f*std::sin(float(2*juce::MathConstants<double>::pi*1000*(n*480+i)/48000));audio.setSample(0,i,x);audio.setSample(1,i,.5f*x);}p.processBlock(audio,midi);}};
    juce::Image sheet(juce::Image::RGB,1000,11*220,true);juce::Graphics grid(sheet);
    for(int type=0;type<=10;++type){
        set(p,"meterType",float(type));set(p,"meterAES17",1);set(p,"meterWeighting",0);feed();
        const int expected=type==0?0:type==1?2:type==2?1:type==10?0:type;
        check(p.getMeterMode()==expected,"host meter type selects the expected detector");
        check(std::abs(p.getMeterDb(true,0)+18)<.3 && std::abs(p.getMeterDb(true,1)+24.0206)<.3,"processor taps preserve independently calibrated channels");
        juce::MemoryBlock saved;p.getStateInformation(saved);OpenPreampProcessor restored;restored.setStateInformation(saved.getData(),int(saved.getSize()));
        check(restored.getMeterMode()==expected && restored.apvts.getRawParameterValue("meterAES17")->load()==1,"type and RMS settings survive state restoration");
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        int types=0,legacy=0,weights=0,aes=0;
        for(auto* surface:editor->getChildren())for(auto* c:surface->getChildren()) {
            if(auto* selector=dynamic_cast<CycleButton*>(c)) {
                if(selector->getMenuOrder().size()==11){++types;check(selector->selectChoice(type),"both type selectors drive shared host parameter");}
                if(selector->getMenuOrder().size()==6){++weights;check(selector->isEnabled()==openpreamp::isRMS(expected),"weighting control follows RMS availability");}
                for(auto* key:selector->getChildren())if(key->getComponentID()=="meterMode-0")++legacy;
            }
            if(auto* toggle=dynamic_cast<SmallToggle*>(c);toggle && toggle->getTooltip().contains("Normalize RMS")){++aes;check(toggle->isEnabled()==(expected==1),"AES-17 control is available only for plain RMS");}
            if(auto* meter=dynamic_cast<MeterPanel*>(c)) {
                LevelTracker::Reading in,out;in.peak[0]=.12589f;in.peak[1]=.062945f;
                meter->setMeasurement(expected,p.getMeterDb(true,meter->getChannel()),true,0);meter->step(in,out,1.f/30);
                check(std::abs(meter->displayedDb()-p.getMeterDb(true,meter->getChannel()))<1.e-5,"numeric readout uses audio-clocked detector");
            }
        }
        check(types==2 && legacy==1 && weights==1 && aes==1,"all new and legacy meter settings have visible controls");
        check(contained(*editor),"meter settings fit within the editor");
        auto image=editor->createComponentSnapshot(editor->getLocalBounds());grid.drawImageAt(image.getClippedImage({0,60,1000,240}),0,type*220);
    }
    {juce::FileOutputStream file(juce::File("/tmp/openpreamp-meter-qa/modes.png"));file.setPosition(0);file.truncate();juce::PNGImageFormat().writeImageToStream(sheet,file);}
    set(p,"meterType",4);feed();
    juce::Image faces(juce::Image::RGB,4*476,4*210,true);juce::Graphics faceGrid(faces);
    for(int f=0;f<16;++f){
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        for(auto* surface:editor->getChildren())for(auto* c:surface->getChildren())if(auto* meter=dynamic_cast<MeterPanel*>(c)) {
            meter->setFace(MeterPanel::faces[f].face);meter->setMeasurement(openpreamp::K14,-18,true,0);
            LevelTracker::Reading in,out;in.peak[0]=.12589f;in.peak[1]=.062945f;meter->step(in,out,1.f/30);
            if(meter->getChannel()==0)faceGrid.drawImageAt(meter->createComponentSnapshot(meter->getLocalBounds()),f%4*476,f/4*210);
        }
        for(const auto size:{juce::Point<int>(400,296),juce::Point<int>(1500,1110)}){
            editor->setSize(size.x,size.y);check(contained(*editor),"all meter faces and settings fit minimum and maximum sizes");
            if(f==0){juce::FileOutputStream file(juce::File("/tmp/openpreamp-meter-qa/editor-"+juce::String(size.x)+".png"));file.setPosition(0);file.truncate();juce::PNGImageFormat().writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),file);}
        }
    }
    {juce::FileOutputStream file(juce::File("/tmp/openpreamp-meter-qa/faces.png"));file.setPosition(0);file.truncate();juce::PNGImageFormat().writeImageToStream(faces,file);}
    set(p,"meterType",2);set(p,"meterWeighting",5);set(p,"meterAES17",1);
    {juce::MemoryBlock saved;p.getStateInformation(saved);OpenPreampProcessor restored;restored.setStateInformation(saved.getData(),int(saved.getSize()));check(restored.getMeterMode()==openpreamp::RMS && restored.apvts.getRawParameterValue("meterWeighting")->load()==5 && restored.apvts.getRawParameterValue("meterAES17")->load()==1,"non-default weighting and normalization survive state round trip");}
    // Old sessions have no meterType: restore must select Legacy, retaining 0/1
    // normalization of the existing meterMode automation parameter.
    auto state=p.apvts.copyState();for(const char* id:{"meterType","meterAES17","meterWeighting"})state.removeChild(state.getChildWithProperty("id",id),nullptr);
    state.getChildWithProperty("id","meterMode").setProperty("value",1.f,nullptr);
    juce::MemoryBlock data;auto xml=state.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,data);p.setStateInformation(data.getData(),int(data.getSize()));
    check(p.getMeterMode()==openpreamp::RMS && p.apvts.getParameter("meterMode")->convertTo0to1(1)==1,"old RMS sessions and normalized automation remain compatible");
    check(p.apvts.getRawParameterValue("meterAES17")->load()==0 && p.apvts.getRawParameterValue("meterWeighting")->load()==0,"old sessions retain unweighted raw RMS calibration");
    // Verify metering changes cannot change samples at the output.
    std::vector<float> baseline;
    for(int type=0;type<=10;++type){set(p,"meterType",float(type));set(p,"meterWeighting",float(type%6));set(p,"meterAES17",float(type%2));feed();std::vector<float> values;for(int c=0;c<2;++c)for(int i=0;i<480;++i)values.push_back(audio.getSample(c,i));if(type==0)baseline=values;else check(values==baseline,"meter type/calibration/weighting do not alter audio samples");}
    // Verify the actual post-trim OUTPUT tap against independently accumulated
    // output-buffer energy, including bass and irregular host block sizes.
    for(double rate : {44100.,48000.,96000.,192000.})for(int block : {64,257,1024})for(double hz : {31.,997.}) {
        OpenPreampProcessor outputTest;set(outputTest,"preampBypass",1);set(outputTest,"preampCircuit",0);
        set(outputTest,"meterType",2);set(outputTest,"meterWeighting",0);set(outputTest,"meterSource",1);
        set(outputTest,"outputGain",6);set(outputTest,"outputGainR",-3);
        outputTest.setPlayConfigDetails(2,2,rate,block);outputTest.prepareToPlay(rate,block);
        juce::AudioBuffer<float> samples(2,block);juce::MidiBuffer messages;
        double energy[2]{};int measured=0;
        const int total=int(rate*4),warm=int(rate*3);
        for(int pos=0;pos<total;pos+=block) {
            const int count=std::min(block,total-pos);samples.setSize(2,count,false,false,true);
            for(int i=0;i<count;++i){float x=float(.1*std::sin(2*juce::MathConstants<double>::pi*hz*(pos+i)/rate));samples.setSample(0,i,x);samples.setSample(1,i,x*.5f);}
            outputTest.processBlock(samples,messages);
            for(int i=0;i<count;++i)if(pos+i>=warm){++measured;for(int ch=0;ch<2;++ch){const double x=samples.getSample(ch,i);energy[ch]+=x*x;}}
        }
        for(int ch=0;ch<2;++ch){const double reference=10*std::log10(energy[ch]/measured);check(std::abs(outputTest.getMeterDb(false,ch)-reference)<(hz<100?.05:.005),"RMS OUTPUT dBFS matches actual post-trim buffer energy at every rate/block size");}
    }

    juce::String params;for(auto* param:p.getParameters())if(auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(param))params+=ranged->paramID+"\n";
    juce::File("/tmp/openpreamp-meter-qa/parameters.txt").replaceWithText(params);
    printf("%d checks, %d failures\n",checks,failures);return failures?1:0;
}
