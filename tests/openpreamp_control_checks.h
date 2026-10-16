static int runControlChecks() {
    OpenPreampProcessor p;
    check(p.apvts.getRawParameterValue("preampType")->load()==1,"new instance selects N-Type, not Off");
    check(p.apvts.getRawParameterValue("preampOversampling")->load()==2,"new HQ instance starts at explicit 2x");
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    CycleButton* model=nullptr;CycleButton* rate=nullptr;CycleButton* engine=nullptr;
    std::map<juce::String,RotaryKnob*> knobs;
    std::vector<RotaryKnob*> frequencyKnobs;
    std::function<void(juce::Component&)> visit=[&](juce::Component& c){
        if(auto* button=dynamic_cast<CycleButton*>(&c)) {
            if(button->getMenuOrder()==std::vector<int>{3,0,1,2,4})model=button;
            if(button->getMenuOrder()==std::vector<int>{1,2,3})rate=button;
            // Unique two-choice engine key starts with HQ.
            for(auto* child:button->getChildren())if(auto* key=dynamic_cast<juce::TextButton*>(child))if(key->getButtonText()=="HQ")engine=button;
        }
        if(auto* knob=dynamic_cast<RotaryKnob*>(&c)){
            knobs[knob->getDesign().id]=knob;
            if(knob->getMinimum()==20 && knob->getMaximum()>=500)frequencyKnobs.push_back(knob);
        }
        for(auto* child:c.getChildren())visit(*child);
    };visit(*editor);
    check(model && rate && engine,"model order starts with Off; oversampling menu contains only 1x/2x/4x");
    if(!model || !rate || !engine)return 1;
    check(model->selectChoice(3) && model->selectChoice(1) && p.apvts.getRawParameterValue("preampType")->load()==1,"Off can be selected and switched back without remapping model IDs");
    check(!rate->selectChoice(0),"retired legacy rate cannot be chosen from the UI");
    engine->selectChoice(1);
    check(p.apvts.getRawParameterValue("preampOversampling")->load()==1,"Circuit initially selects explicit 1x");
    check(dynamic_cast<juce::AudioParameterChoice*>(p.apvts.getParameter("circuitEngine"))->getCurrentChoiceName()=="Circuit","engine key is named Circuit");
    rate->selectChoice(3);engine->selectChoice(0);engine->selectChoice(1);
    check(p.apvts.getRawParameterValue("preampOversampling")->load()==3,"user-selected rate survives later engine changes");
    OpenPreampProcessor automatic;
    set(automatic,"circuitEngine",1);automatic.chooseInitialRate();
    check(automatic.apvts.getRawParameterValue("preampOversampling")->load()==1,"initial Circuit rate is 1x before audio preparation");
    set(automatic,"circuitEngine",0);automatic.chooseInitialRate();
    check(automatic.apvts.getRawParameterValue("preampOversampling")->load()==2,"initial HQ rate is 2x before a user rate choice");
    set(automatic,"preampCircuit",0);automatic.chooseInitialRate();
    check(automatic.apvts.getRawParameterValue("preampOversampling")->load()==1,"initial Standard rate is 1x");
    check(knobs.count("preampGain") && knobs.count("outputGain") && knobs.count("preampGainR") && knobs.count("outputGainR"),"gain knobs retain their parameter identities");
    if(!knobs.count("preampGain") || !knobs.count("outputGain"))return 1;
    auto* input=knobs["preampGain"];auto* output=knobs["outputGain"];
    auto wheel=[&](RotaryKnob& knob,float delta){
        static int serial=0;
        const auto now=juce::Time(juce::Time::currentTimeMillis() + ++serial);
        juce::MouseEvent e(juce::Desktop::getInstance().getMainMouseSource(),{50,50},{},0,0,0,0,0,&knob,&knob,now,{50,50},now,0,false);
        juce::MouseWheelDetails details{};details.deltaY=delta;details.isSmooth=false;details.isReversed=false;details.isInertial=false;
        knob.mouseWheelMove(e,details);
    };
    input->setValue(0,juce::sendNotificationSync);output->setValue(0,juce::sendNotificationSync);
    wheel(*input,.1f);
    check(input->getValue()>0 && std::abs(output->getValue())<1e-5,"wheel raises input without compensation when Gain Compensation is off");
    set(p,"gainCompensation",1);input->setValue(0,juce::sendNotificationSync);output->setValue(0,juce::sendNotificationSync);
    wheel(*input,-.1f);
    check(input->getValue()<0 && std::abs(input->getValue()+output->getValue())<1e-5,"Gain Compensation inversely adjusts output on an input wheel edit");
    wheel(*output,-.1f);
    check(std::abs(input->getValue()+output->getValue())<1e-5,"Gain Compensation also compensates an output wheel edit");
    input->setValue(0,juce::sendNotificationSync);output->setValue(-24,juce::sendNotificationSync);wheel(*input,.1f);
    check(std::abs(input->getValue())<1e-5 && output->getValue()==-24,"linked gains stop at the peer limit without losing their dB sum");
    check(std::abs(p.apvts.getRawParameterValue("outputGain")->load()-output->getValue())<1e-4,"linked knob change reaches the saved host parameter");
    // Host/restored values must not be recursively linked a second time.
    set(p,"preampGain",6);check(output->getValue()==-24,"programmatic/host changes do not create feedback edits");
    juce::MemoryBlock state;p.getStateInformation(state);OpenPreampProcessor restored;restored.setStateInformation(state.getData(),int(state.getSize()));
    check(restored.apvts.getRawParameterValue("gainCompensation")->load()==1 && restored.apvts.getRawParameterValue("preampOversampling")->load()==3,"Gain Compensation and explicit rate survive state restore");
    check(frequencyKnobs.size()==3,"all three filter/mono-maker frequency knobs are available");
    for(auto* frequency : frequencyKnobs){
        frequency->setActive(true);frequency->setValue(100,juce::sendNotificationSync);wheel(*frequency,.1f);
        check(frequency->getValue()>100,"wheel raises the hovered filter frequency");
        frequency->setValue(100,juce::sendNotificationSync);wheel(*frequency,-.1f);
        check(frequency->getValue()<100,"wheel lowers the hovered filter frequency");
    }
    // A click selects the control even when the host does not grant keyboard focus.
    auto now=juce::Time::getCurrentTime();
    juce::MouseEvent click(juce::Desktop::getInstance().getMainMouseSource(),{50,50},{},0,0,0,0,0,input,input,now,{50,50},now,0,false);
    input->mouseDown(click);input->mouseUp(click);
    input->setValue(0,juce::sendNotificationSync);output->setValue(0,juce::sendNotificationSync);
    juce::MouseEvent background(juce::Desktop::getInstance().getMainMouseSource(),{10,10},{},0,0,0,0,0,editor.get(),editor.get(),juce::Time(juce::Time::currentTimeMillis()+1000),{10,10},now,0,false);
    juce::MouseWheelDetails backgroundWheel{};backgroundWheel.deltaY=.1f;
    editor->mouseWheelMove(background,backgroundWheel);
    check(input->getValue()>0 && std::abs(input->getValue()+output->getValue())<1e-5,"background wheel edits the clicked gain with inverse compensation");
    if(knobs.count("preampGainR") && knobs.count("outputGainR")){
        auto* right=knobs["preampGainR"];auto* rightOutput=knobs["outputGainR"];
        const double left=input->getValue();right->setValue(0,juce::sendNotificationSync);rightOutput->setValue(0,juce::sendNotificationSync);wheel(*right,.1f);
        check(right->getValue()>0 && std::abs(right->getValue()+rightOutput->getValue())<1e-5 && input->getValue()==left,"right-channel compensation remains independent of the left");
    }
    auto old=p.apvts.copyState();old.removeChild(old.getChildWithProperty("id","gainCompensation"),nullptr);
    auto oldXML=old.createXml();juce::MemoryBlock oldState;juce::AudioProcessor::copyXmlToBinary(*oldXML,oldState);
    restored.setStateInformation(oldState.getData(),int(oldState.getSize()));
    check(restored.apvts.getRawParameterValue("gainCompensation")->load()==0,"older sessions restore Gain Compensation off");
    check(contained(*editor),"updated header controls fit the editor");
    editor->setSize(400,296);check(contained(*editor),"updated controls remain contained at minimum size");
    editor->setSize(1000,740);
    OpenPreampProcessor display;set(display,"circuitEngine",1);set(display,"gainCompensation",1);
    set(display,"preampGain",6);set(display,"outputGain",-6);display.prepareToPlay(48000,128);
    juce::AudioBuffer<float> tone(2,128);juce::MidiBuffer midi;
    for(int block=0;block<16;++block){for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)tone.setSample(ch,i,float(.125*std::sin(juce::MathConstants<double>::twoPi*1000*(block*128+i)/48000)));display.processBlock(tone,midi);}
    std::unique_ptr<juce::AudioProcessorEditor> preview(display.createEditor());
    const auto snapshot=preview->createComponentSnapshot(preview->getLocalBounds());
    juce::FileOutputStream file(juce::File("/private/tmp/openpreamp-controls.png"));file.setPosition(0);file.truncate();juce::PNGImageFormat png;png.writeImageToStream(snapshot,file);
    printf("%d checks, %d failures\n",checks,failures);return failures?1:0;
}
