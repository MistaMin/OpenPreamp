#include "PluginEditor.h"
#include <EmbeddedLooks.h>
#include <EmbeddedOpenPreampDesign.h>

OpenPreampEditor::OpenPreampEditor(OpenPreampProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      rightMeter(p), rightMeterSourceBtn(p.apvts, "meterSource", "SOURCE", true),
      meterModeBtn(p.apvts, "meterMode", "MODE", true), rightMeterModeBtn(p.apvts, "meterMode", "MODE", true),
      midSideToggle(p.apvts, "midSide", "MS MODE", Theme::preampCol),
      cutsToggle(p.apvts, "cutsEnabled", "CUTS", Theme::preampCol),
      linkToggle(p.apvts, "channelLink", "L/R LINK", Theme::preampCol),
      monoMakerToggle(p.apvts, "monoMakerEnabled", "ON", Theme::preampCol),
      gainCompensationToggle(p.apvts, "gainCompensation", "GAIN COMPENSATION", Theme::preampCol),
      preampTypeBtn(p.apvts, "preampType", "MODEL"),
      preampPadBtn(p.apvts, "preampPad", "PAD"),
      circuitEngineBtn(p.apvts,"circuitEngine","ENGINE",true),
      meterPanel(p),
      meterSourceBtn(p.apvts, "meterSource", "SOURCE", true),
      oversamplingBtn(p.apvts, "preampOversampling", "OS", true),
      preampBypassBtn(p.apvts, "preampBypass", Theme::preampCol),
      preampCircuitToggle(p.apvts, "preampCircuit", "MODELED", Theme::preampCol)
{
    addAndMakeVisible(canvas);
    proc.chooseInitialRate();
    preampTypeBtn.setMenuOrder({3,0,1,2,4});
    oversamplingBtn.setMenuOrder({1,2,3});
    oversamplingBtn.onChoiceSelected=[this](int){proc.markRateChosen();};
    circuitEngineBtn.onChoiceSelected=[this](int){proc.chooseInitialRate();};
    preampCircuitToggle.onClick=[this]{proc.chooseInitialRate();};
    circuitEngineBtn.setMenuOrder({1,0});
    circuitEngineBtn.setSegmented(true);oversamplingBtn.setSegmented(true);
    meterModeBtn.setSegmented(true,false);rightMeterModeBtn.setSegmented(true,false);
    for(auto* key : {&preampPadBtn,&preampTypeBtn,&circuitEngineBtn,&oversamplingBtn,&meterModeBtn,&rightMeterModeBtn,&meterSourceBtn,&rightMeterSourceBtn}) key->setHardwareStyle(true);
    for(auto* toggle : {&midSideToggle,&linkToggle,&gainCompensationToggle,&cutsToggle,&monoMakerToggle,&preampCircuitToggle}) toggle->setHardwareStyle(true);
    for(auto* panel : {&preampPanel,&rightPreampPanel,&cutsPanel,&controlsPanel,&monoMakerPanel}) panel->setHardwareStyle(true);
    meterPanel.setHardwareStyle(true);rightMeter.setHardwareStyle(true);
    for(auto* knob : {&preampGainDial,&outputGainDial,&rightGainDial,&rightOutputDial,&highPassDial,&lowPassDial,&monoMakerDial}) knob->setHardwareStyle(true);
    modelLooks.fromCsv(openPreampModelLooks);
    preampPanel.setAccent(Theme::preampCol,"INPUT GAIN / OUTPUT TRIM");
    rightPreampPanel.setAccent(Theme::preampCol,"INPUT GAIN / OUTPUT TRIM");
    cutsPanel.setAccent(Theme::preampCol,"SHARED / 12 dB PER OCTAVE");
    monoMakerPanel.setAccent(Theme::preampCol,"SIDE / 6 dB PER OCTAVE");
    controlsPanel.setAccent(Theme::preampCol,"MODEL / DRIVE / QUALITY");
    controlsPanel.setBypassButton(preampBypassBtn);
    preampPanel.addAndMakeVisible(preampGainDial);
    preampPanel.addAndMakeVisible(outputGainDial);
    rightPreampPanel.addAndMakeVisible(rightGainDial);
    rightPreampPanel.addAndMakeVisible(rightOutputDial);
    for (auto* c : std::initializer_list<juce::Component*>{&highPassDial,&lowPassDial,&cutsToggle}) cutsPanel.addAndMakeVisible(c);
    for (auto* c : std::initializer_list<juce::Component*>{&preampPadBtn,&preampTypeBtn,&preampCircuitToggle,&oversamplingBtn,&circuitEngineBtn,&rateLabel}) controlsPanel.addAndMakeVisible(c);
    for (auto* c : std::initializer_list<juce::Component*>{&preampPanel,&rightPreampPanel,&cutsPanel,&controlsPanel,&monoMakerPanel,&midSideToggle,&linkToggle,&gainCompensationToggle,&meterPanel,&rightMeter,&meterSourceBtn,&rightMeterSourceBtn,&meterModeBtn,&rightMeterModeBtn}) canvas.addAndMakeVisible(c);
    monoMakerPanel.addAndMakeVisible(monoMakerDial); monoMakerPanel.addAndMakeVisible(monoMakerToggle);
    meterPanel.setChannel(0); rightMeter.setChannel(1);
    meterPanel.setExternallyDriven(); rightMeter.setExternallyDriven();
    auto attach = [&](const char* id,RotaryKnob& knob) { return std::make_unique<SliderAttachment>(p.apvts,id,knob); };
    preampGainAtt=attach("preampGain",preampGainDial); outputGainAtt=attach("outputGain",outputGainDial);
    rightGainAtt=attach("preampGainR",rightGainDial); rightOutputAtt=attach("outputGainR",rightOutputDial);
    monoMakerAtt=attach("monoMakerFrequency",monoMakerDial);
    monoMakerDial.setDoubleClickReturnValue(true,20);
    highPassAtt=attach("highPass",highPassDial); lowPassAtt=attach("lowPass",lowPassDial);
    for(auto* knob : {&preampGainDial,&rightGainDial,&outputGainDial,&rightOutputDial}) knob->setDoubleClickReturnValue(true,0);
    highPassDial.setDoubleClickReturnValue(true,20); lowPassDial.setDoubleClickReturnValue(true,20000);
    for(auto* knob : {&preampGainDial,&rightGainDial,&outputGainDial,&rightOutputDial,&highPassDial,&lowPassDial,&monoMakerDial}) {
        knob->setScrollWheelEnabled(true);knob->setWantsKeyboardFocus(true);
        knob->onUserSelect=[this,knob]{selectedKnob=knob;};
    }
    auto pair=[this](RotaryKnob& source,RotaryKnob& target,const char* targetID) {
        auto gestureOpen=std::make_shared<bool>(false);
        auto* parameter=proc.apvts.getParameter(targetID);
        source.onDragStart=[this,gestureOpen,parameter]{
            if(proc.apvts.getRawParameterValue("gainCompensation")->load()>0.5f){parameter->beginChangeGesture();*gestureOpen=true;}
        };
        source.onDragEnd=[gestureOpen,parameter]{if(*gestureOpen){parameter->endChangeGesture();*gestureOpen=false;}};
        source.onUserValueChanged=[this,&source,&target](double before,double after) {
            if(proc.apvts.getRawParameterValue("gainCompensation")->load()<0.5f) return;
            const double oldTarget=target.getValue();
            const double next=juce::jlimit(target.getMinimum(),target.getMaximum(),oldTarget-(after-before));
            target.setValue(next,juce::sendNotificationSync);
            source.setValue(before+oldTarget-next,juce::sendNotificationSync);
        };
    };
    pair(preampGainDial,outputGainDial,"outputGain");pair(outputGainDial,preampGainDial,"preampGain");
    pair(rightGainDial,rightOutputDial,"outputGainR");pair(rightOutputDial,rightGainDial,"preampGainR");
    gainCompensationToggle.setTooltip("Move INPUT and OUTPUT inversely in dB. Applies to knob drag, wheel, keys and double-click; both gain values are saved and can be recorded as automation. This is not automatic loudness matching.");
    preampGainDial.setTooltip("Left / Mid input gain. Double-click for 0 dB.");
    rightGainDial.setTooltip("Right / Side input gain. Double-click for 0 dB.");
    outputGainDial.setTooltip("Left / Mid output trim. Double-click for 0 dB.");
    rightOutputDial.setTooltip("Right / Side output trim. Double-click for 0 dB.");
    preampBypassBtn.setTooltip("Bypass cuts and preamp; output trims and meters remain active.");
    preampPadBtn.setButtonTooltip("Shared input offset: -20 dB, unity, or +10 dB.");
    preampTypeBtn.setButtonTooltip("Shared preamp model for both channels.");
    preampCircuitToggle.setTooltip("Off: Standard preamp. On: the selected Circuit or HQ engine. Standard and Circuit use ADAA only at 1x.");
    circuitEngineBtn.setButtonTooltip("HQ: full component model. Circuit: reduced model, still under validation, with differences in driven harmonics and recovery. Turn MODELED off for Standard.");
    oversamplingBtn.setButtonTooltip("Select 1x, 2x or 4x for any engine. Starts at 2x for HQ. Standard/Circuit use 1x as their initial rate; older sessions retain their saved rates. ADAA is automatic at 1x for Standard/Circuit and off at 2x/4x. The displayed factor is the effective rate.");
    midSideToggle.setTooltip("Off: normal L/R. On: encode Mid/Side before the preamps and decode after downsampling and output trims. Link is disabled in M/S.");
    linkToggle.setTooltip("Link both input and output gains to the left controls. Right settings return when unlinked. Unavailable in M/S.");
    monoMakerToggle.setTooltip("Reduce bass width with a session-rate 6 dB/octave high-pass on Side only, before the preamp.");
    monoMakerDial.setTooltip("Side high-pass cutoff: 20–500 Hz. Mid is unchanged. Double-click for 20 Hz.");
    cutsToggle.setTooltip("Shared session-rate 12 dB/octave high-pass and low-pass.");
    for (auto* button : {&meterSourceBtn,&rightMeterSourceBtn}) button->setButtonTooltip("Select input or output for both independent meters.");
    for (auto* button : {&meterModeBtn,&rightMeterModeBtn}) button->setButtonTooltip("Switch both meter needles and numeric readouts between Peak and RMS. Clip lamps always detect peaks.");
    rateLabel.setFont(juce::FontOptions(10.0f));
    rateLabel.setJustificationType(juce::Justification::centredRight);
    rateLabel.setInterceptsMouseClicks(false,false);
    setResizable(true,true);
    setResizeLimits(400,296,1500,1110);
    getConstrainer()->setFixedAspectRatio(1000.0/740.0);
    setSize(1000,740);
    preampGainDial.setDesignParameter("preampGain");
    outputGainDial.setDesignParameter("outputGain");
    rightGainDial.setDesignParameter("preampGainR");
    rightOutputDial.setDesignParameter("outputGainR");
    highPassDial.setDesignParameter("highPass");
    lowPassDial.setDesignParameter("lowPass");
    monoMakerDial.setDesignParameter("monoMakerFrequency");
    loadKnobLayout(openPreampKnobDesign);
#if GOODLOOKINUI_ENABLE_EDITOR
    knobDefaults.clear();
    for (auto* dial : {&preampGainDial, &outputGainDial, &rightGainDial, &rightOutputDial, &highPassDial, &lowPassDial, &monoMakerDial}) {
        knobDefaults.emplace_back(dial, dial->getDesign().style.empty() ? "A" : dial->getDesign().style);
    }
#endif
    layoutReady=true;
    timerCallback();
#if GOODLOOKINUI_ENABLE_EDITOR
    setupDeveloperTools();
#endif
    proc.apvts.state.addListener(this);
    startTimerHz(30);
}

void OpenPreampEditor::applyModelLook()
{
    const int model = int(proc.apvts.getRawParameterValue("preampType")->load());
    if (model == lastModel) return;
    lastModel = model;
    auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter("preampType"));
    if (!parameter) return;
    const auto choice = parameter->getCurrentChoiceName();
    const auto* look = modelLooks.find("preampType", choice.toStdString());

#if GOODLOOKINUI_ENABLE_EDITOR
    const auto mode = getTriggerMode();
    juce::String fpCode = proc.apvts.state.getProperty("lookFaceplate", "").toString();
    if (mode == TriggerMode::Faceplate && look && !look->faceplate.empty() && fpCode.isEmpty())
        fpCode = look->faceplate;
    const auto* face = fpCode.isNotEmpty() ? goodlookinui::findFaceplate(fpCode.toStdString())
                     : (mode == TriggerMode::Control && look && !look->faceplate.empty() ? goodlookinui::findFaceplate(look->faceplate) : nullptr);
#else
    const auto* face = look && !look->faceplate.empty() ? goodlookinui::findFaceplate(look->faceplate) : nullptr;
#endif

    if (face) {
        editorLook = Theme::Look::from(*face);
    } else {
        editorLook = Theme::current();
        editorLook.bgTop = juce::Colour(0xff102333);
        editorLook.bgBottom = juce::Colour(0xff06121c);
        editorLook.titleBar = juce::Colour(0xff0b1b29);
        editorLook.text = PreampChrome::ink;
        editorLook.textMid = PreampChrome::muted;
        editorLook.border = PreampChrome::rim;
        editorLook.accent = PreampChrome::accent;
    }

#if GOODLOOKINUI_ENABLE_EDITOR
    const juce::String allPlate = proc.apvts.state.getProperty("lookPlate", "").toString();
    const auto* plate = allPlate.isNotEmpty() ? goodlookinui::findFaceplate(allPlate.toStdString())
                      : ((mode == TriggerMode::Control && look && !look->plate.empty()) ? goodlookinui::findFaceplate(look->plate)
                      : (mode == TriggerMode::Control && look && !look->faceplate.empty()) ? goodlookinui::findFaceplate(look->faceplate)
                      : (mode == TriggerMode::Faceplate && face ? face
                      : (look && !look->plate.empty() ? goodlookinui::findFaceplate(look->plate)
                      : (look && !look->faceplate.empty() ? goodlookinui::findFaceplate(look->faceplate) : nullptr))));
#else
    const auto* plate = (look && !look->plate.empty()) ? goodlookinui::findFaceplate(look->plate)
                      : (look && !look->faceplate.empty() ? goodlookinui::findFaceplate(look->faceplate) : nullptr);
#endif

    const auto colour = (look && !look->colour.empty())
                      ? juce::Colour::fromString("ff" + juce::String(look->colour.substr(1)))
                      : Theme::preampCol;

    for (auto* panel : {&preampPanel, &rightPreampPanel, &cutsPanel, &controlsPanel, &monoMakerPanel}) {
        panel->setAccentColour(colour);
        if (plate)
            panel->setPlate(juce::Colour(plate->panelTop), juce::Colour(plate->panelBottom),
                            juce::Colour(plate->text), juce::Colour(plate->textMid), plate->finish);
        else
            panel->clearPlate();
    }

    for (auto* meter : {&meterPanel, &rightMeter}) {
        if (plate)
            meter->setPlate(juce::Colour(plate->panelTop), juce::Colour(plate->panelBottom),
                            juce::Colour(plate->text), juce::Colour(plate->textMid), plate->finish);
        else
            meter->clearPlate();
    }

#if GOODLOOKINUI_ENABLE_EDITOR
    refreshKnobStyles();
    if (lookStudio)
        lookStudio->follow("preampType", choice);
#else
    for (auto* knob : {&preampGainDial, &outputGainDial, &rightGainDial, &rightOutputDial, &highPassDial, &lowPassDial, &monoMakerDial}) {
        auto design = knob->getDesign();
        if (look && !look->style.empty()) design.style = look->style;
        if (look && !look->colour.empty()) design.colour = look->colour;
        knob->applyDesign(design);
    }
#endif

    for (auto* knob : {&preampGainDial, &outputGainDial, &rightGainDial, &rightOutputDial, &highPassDial, &lowPassDial, &monoMakerDial}) {
        if (plate) {
            const juce::Colour text(plate->text), mid(plate->textMid);
            knob->setPlateText(&text, &mid);
        } else {
            knob->setPlateText(nullptr, nullptr);
        }
    }

    if (plate) {
        const juce::Colour text(plate->text), mid(plate->textMid);
        preampPadBtn.setPlateText(&text, &mid); preampTypeBtn.setPlateText(&text, &mid);
        circuitEngineBtn.setPlateText(&text, &mid); oversamplingBtn.setPlateText(&text, &mid);
        rateLabel.setColour(juce::Label::textColourId, mid);
    } else {
        preampPadBtn.setPlateText(nullptr, nullptr); preampTypeBtn.setPlateText(nullptr, nullptr);
        circuitEngineBtn.setPlateText(nullptr, nullptr); oversamplingBtn.setPlateText(nullptr, nullptr);
    }

    preampBypassBtn.setAccent(colour);
    preampCircuitToggle.setAccent(colour); oversamplingBtn.refreshTheme();
    linkToggle.setAccent(colour); monoMakerToggle.setAccent(colour);
    cutsToggle.setAccent(colour); midSideToggle.setAccent(colour); gainCompensationToggle.setAccent(colour);
    repaint();
}

void OpenPreampEditor::paint(juce::Graphics& g)
{
    g.addTransform(juce::AffineTransform::scale(float(getHeight())/740.0f));
    g.setGradientFill(juce::ColourGradient(editorLook.bgTop,0,0,editorLook.bgBottom,0,740,false));
    g.fillAll();
    g.setColour(editorLook.titleBar);g.fillRect(0,0,1000,52);
    g.setColour(editorLook.accent);g.fillRect(22,14,3,24);
    g.setColour(editorLook.text);g.setFont(juce::FontOptions("Georgia",28.0f,juce::Font::bold));
    g.drawText("OpenPreamp",36,10,240,32,juce::Justification::centredLeft);
    g.setColour(editorLook.textMid);g.setFont(juce::FontOptions(9.5f));
    g.drawText(juce::String(JucePlugin_VersionString)+"  /  OPENGRID",800,14,176,24,juce::Justification::centredRight);
    g.setColour(editorLook.border);g.drawHorizontalLine(52,16,984);
    g.setColour(editorLook.textMid);g.setFont(juce::FontOptions(9.0f));
    g.drawText("0 VU = -18 dBFS",24,284,220,14,juce::Justification::centredLeft);
    g.drawText("0 VU = -18 dBFS",516,284,220,14,juce::Justification::centredLeft);
    g.drawText("L/R OR M/S  /  SESSION-RATE CUTS & MONO MAKER",24,716,520,18,juce::Justification::centredLeft);
    g.drawText("Click CLIP to reset",800,716,176,18,juce::Justification::centredRight);
}

void OpenPreampEditor::resized()
{
    canvas.setBounds(0,0,1000,740);
    canvas.setTransform(juce::AffineTransform::scale(float(getHeight())/740.0f));
    meterPanel.setBounds(16,68,476,210);rightMeter.setBounds(508,68,476,210);
    meterSourceBtn.setBounds(382,75,92,27);rightMeterSourceBtn.setBounds(874,75,92,27);
    meterModeBtn.setBounds(246,75,128,27);rightMeterModeBtn.setBounds(738,75,128,27);
    preampPanel.setBounds(16,306,476,218);rightPreampPanel.setBounds(508,306,476,218);
    if(!layoutReady) {
        preampGainDial.setBounds(60,54,148,154);outputGainDial.setBounds(286,54,132,154);
        rightGainDial.setBounds(preampGainDial.getBounds());rightOutputDial.setBounds(outputGainDial.getBounds());
    }
    cutsPanel.setBounds(16,540,356,164);monoMakerPanel.setBounds(402,540,196,164);controlsPanel.setBounds(628,540,356,164);
    highPassDial.setBounds(24,54,108,102);lowPassDial.setBounds(150,54,108,102);
    cutsToggle.setBounds(270,80,70,30);
    monoMakerDial.setBounds(12,54,98,102);monoMakerToggle.setBounds(116,80,66,30);
    preampPadBtn.setBounds(14,56,100,60);preampTypeBtn.setBounds(120,56,112,60);
    preampCircuitToggle.setBounds(244,76,96,27);oversamplingBtn.setBounds(180,118,160,28);
    constexpr int keyGap=10, menuWidth=110+80+164+2*keyGap;
    auto headerMenu=juce::Rectangle<int>((1000-menuWidth)/2,14,menuWidth,24);
    midSideToggle.setBounds(headerMenu.removeFromLeft(110));headerMenu.removeFromLeft(keyGap);
    linkToggle.setBounds(headerMenu.removeFromLeft(80));headerMenu.removeFromLeft(keyGap);
    gainCompensationToggle.setBounds(headerMenu);
    circuitEngineBtn.setBounds(14,118,160,28);
    rateLabel.setBounds(14,146,326,16);
#if GOODLOOKINUI_ENABLE_EDITOR
    developerButton.setBounds(190,14,46,24);
    lookBtn.setBounds(242,14,56,24);
    knobStyleBtn.setBounds(700,14,96,24);
#endif
}

void OpenPreampEditor::mouseWheelMove(const juce::MouseEvent& event,const juce::MouseWheelDetails& wheel) {
    auto* focused=selectedKnob.getComponent();
    if(focused && event.eventComponent!=focused && !focused->isParentOf(event.eventComponent) && isParentOf(focused) && focused->isEnabled())
        focused->mouseWheelMove(event.getEventRelativeTo(focused),wheel);
}

void OpenPreampEditor::timerCallback()
{
    applyModelLook();
    const bool peak = proc.apvts.getRawParameterValue("meterMode")->load() < 0.5f;
    meterPanel.setPeakMode(peak); rightMeter.setPeakMode(peak);
    rightMeter.setInputSource(proc.apvts.getRawParameterValue("meterSource")->load() < 0.5f);
    meterPanel.setInputSource(proc.apvts.getRawParameterValue("meterSource")->load() < 0.5f);
    const bool active = proc.apvts.getRawParameterValue("preampBypass")->load() < 0.5f
                     && proc.apvts.getRawParameterValue("preampType")->load() != 3.0f;
    preampGainDial.setActive(proc.apvts.getRawParameterValue("preampBypass")->load() < 0.5f); preampPadBtn.setActive(proc.apvts.getRawParameterValue("preampBypass")->load() < 0.5f);
    preampTypeBtn.setActive(active); preampCircuitToggle.setActive(active);
    oversamplingBtn.setActive(active);
    const int chosen=int(proc.apvts.getRawParameterValue("preampOversampling")->load());
    oversamplingBtn.setDisplayTextOverride(juce::String(chosen>0 ? 1<<(chosen-1) : proc.getEffectiveOversampleFactor())+"x");
    circuitEngineBtn.setActive(active && preampCircuitToggle.getToggleState());
    rightGainDial.setActive(!proc.apvts.getRawParameterValue("preampBypass")->load());
    highPassDial.setActive(cutsToggle.getToggleState() && proc.apvts.getRawParameterValue("preampBypass")->load() < 0.5f); lowPassDial.setActive(highPassDial.getActive());
    const bool ms = proc.apvts.getRawParameterValue("midSide")->load() > 0.5f && proc.getTotalNumInputChannels() == 2;
    meterPanel.setCaption(ms ? "MID LEVEL" : "LEFT LEVEL");
    rightMeter.setCaption(ms ? "SIDE LEVEL" : "RIGHT LEVEL");
    preampPanel.setTitle(ms ? "MID" : "LEFT");
    rightPreampPanel.setTitle(ms ? "SIDE" : "RIGHT");
    const bool compensated=proc.apvts.getRawParameterValue("gainCompensation")->load()>0.5f;
    preampPanel.setCompensation(compensated);
    const bool stereo = proc.getTotalNumInputChannels() == 2;
    const bool linked = !ms && proc.apvts.getRawParameterValue("channelLink")->load() > 0.5f;
    linkToggle.setVisible(true); linkToggle.setEnabled(stereo && !ms); linkToggle.setActive(stereo && !ms);
    rightGainDial.setEnabled(stereo && !linked); rightOutputDial.setEnabled(stereo && !linked);
    rightGainDial.setActive(stereo && !linked && !proc.apvts.getRawParameterValue("preampBypass")->load());
    rightOutputDial.setActive(stereo && !linked);
    rightPreampPanel.setCompensation(compensated && stereo && !linked);
    monoMakerToggle.setEnabled(stereo); monoMakerToggle.setActive(stereo);
    monoMakerDial.setEnabled(stereo);
    monoMakerDial.setActive(stereo && monoMakerToggle.getToggleState() && !proc.apvts.getRawParameterValue("preampBypass")->load());
    midSideToggle.setEnabled(stereo); midSideToggle.setActive(stereo);
    if (isShowing()) {
        const auto input = proc.getInputLevel().read(), output = proc.getOutputLevel().read();
        meterPanel.step(input,output,1.0f/30); rightMeter.step(input,output,1.0f/30);
        meterPanel.repaint(); rightMeter.repaint();
    }
    const int factor = proc.getEffectiveOversampleFactor();
    const auto engineName = !active ? "Off" : !preampCircuitToggle.getToggleState() ? "Standard" : proc.apvts.getRawParameterValue("circuitEngine")->load() < 0.5f ? "HQ" : "Circuit";
    const auto text = juce::String(engineName) + " " + juce::String(factor) + "x" + (proc.isADAAActive()?" ADAA":"") + " / " + juce::String(proc.getProcessingSampleRate() / 1000.0, 1) + " kHz";
    if (rateText != text) { rateText = text; rateLabel.setText(text, juce::dontSendNotification); }
}

OpenPreampEditor::~OpenPreampEditor()
{
    stopTimer();
    proc.apvts.state.removeListener(this);
#if GOODLOOKINUI_ENABLE_EDITOR
    developerWindow.reset();
    designAutosave.flush();
#endif
}

void OpenPreampEditor::loadKnobLayout(const std::string& csv)
{
    std::istringstream in(csv);
    for (const auto& item : goodlookinui::readDesign(in)) {
        auto* knob = item.parameter == "preampGain" ? &preampGainDial : item.parameter == "outputGain" ? &outputGainDial :
                     item.parameter == "preampGainR" ? &rightGainDial : item.parameter == "outputGainR" ? &rightOutputDial :
                     item.parameter == "highPass" ? &highPassDial : item.parameter == "lowPass" ? &lowPassDial :
                     item.parameter == "monoMakerFrequency" ? &monoMakerDial : nullptr;
        if (!knob) continue;
        knob->setBounds(juce::roundToInt(item.x), juce::roundToInt(item.y),
                        juce::roundToInt(item.width), juce::roundToInt(item.height));
        knob->applyDesign(item);
        if (knob == &preampGainDial) rightGainDial.setBounds(knob->getBounds());
        if (knob == &outputGainDial) rightOutputDial.setBounds(knob->getBounds());
    }
}

#if GOODLOOKINUI_ENABLE_EDITOR
namespace {
class DesignWindow : public juce::DocumentWindow {
public:
    DesignWindow() : DocumentWindow("OpenPreamp developer tools", juce::Colour(0xff18222c), closeButton) {
        setUsingNativeTitleBar(true);
        setResizable(true, false);
    }
    void closeButtonPressed() override { setVisible(false); }
};
}

const std::vector<juce::String>& OpenPreampEditor::settingKeys()
{
    static const std::vector<juce::String> keys{"lookTrigger", "lookFaceplate", "lookPlate", "knobStyle", "meterFace", "faderStyle", "glow"};
    return keys;
}

TriggerMode OpenPreampEditor::getTriggerMode() const
{
    const auto m = proc.apvts.state.getProperty("lookTrigger", "control").toString();
    if (m == "control") return TriggerMode::Control;
    if (m == "faceplate") return TriggerMode::Faceplate;
    if (m == "off") return TriggerMode::Off;
    return TriggerMode::Control;
}

void OpenPreampEditor::loadDesignSettings()
{
    SettingsTable table;
    const auto saved = designAutosave.read("Settings.csv");
    if (!saved.empty()) table.fromCsv(saved);
    for (const auto& key : settingKeys()) {
        if (!table.has(key.toStdString())) continue;
        proc.apvts.state.setProperty(key, juce::String(table.get(key.toStdString())), nullptr);
    }
}

void OpenPreampEditor::saveSettingsFile()
{
    SettingsTable table;
    for (const auto& key : settingKeys())
        if (proc.apvts.state.hasProperty(key))
            table.set(key.toStdString(), proc.apvts.state.getProperty(key).toString().toStdString());
    designAutosave.request("Settings.csv", table.toCsv());
}

void OpenPreampEditor::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree != proc.apvts.state) return;
    const auto prop = property.toString();
    if (prop == "knobStyle") { refreshKnobStyles(); saveSettingsFile(); return; }
    if (prop == "meterFace" || prop == "glow" || prop == "faderStyle") { applyMeterStyles(); saveSettingsFile(); return; }
    if (prop == "lookTrigger" || prop == "lookFaceplate" || prop == "lookPlate") { lastModel = -1; applyModelLook(); saveSettingsFile(); return; }
}

void OpenPreampEditor::applyKnobStyle(const juce::String& code)
{
    proc.apvts.state.setProperty("knobStyle", code, nullptr);
    knobStyleBtn.setButtonText("KNOBS: " + (code.isEmpty() ? juce::String("DEFAULT") : code));
    refreshKnobStyles();
}

void OpenPreampEditor::refreshKnobStyles()
{
    const auto global = proc.apvts.state.getProperty("knobStyle", "").toString().toStdString();
    const auto mode = getTriggerMode();
    auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter("preampType"));
    const auto choice = parameter ? parameter->getCurrentChoiceName().toStdString() : std::string{};
    const auto* look = (mode == TriggerMode::Control) ? modelLooks.find("preampType", choice) : nullptr;

    for (auto& [knob, baked] : knobDefaults) {
        if (!knob) continue;
        std::string style = baked;
        if (look && !look->style.empty())
            style = look->style;
        if (!global.empty())
            style = global;

        auto item = knob->getDesign();
        if (look && !look->colour.empty())
            item.colour = look->colour;
        if (item.style != style || (look && !look->colour.empty())) {
            item.style = style;
            knob->applyDesign(item);
        }
    }
}

void OpenPreampEditor::onKnobDesignEdited(RotaryKnob& knob, const goodlookinui::Item& item)
{
    if (initializingTools) {
        for (auto& k : knobDefaults)
            if (k.first == &knob) k.second = item.style;
        knob.applyDesign(item);
        return;
    }

    bool isPreamp = (&knob == &preampGainDial || &knob == &outputGainDial ||
                     &knob == &rightGainDial || &knob == &rightOutputDial);
    if (getTriggerMode() == TriggerMode::Control && isPreamp) {
        auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter("preampType"));
        const auto choice = parameter ? parameter->getCurrentChoiceName().toStdString() : std::string{};
        goodlookinui::Look l;
        if (auto* e = modelLooks.find("preampType", choice)) l = *e;
        else { l.parameter = "preampType"; l.choice = choice; }
        l.style = item.style;
        l.colour = item.colour;
        modelLooks.set(l);
        designAutosave.request("OpenPreampLooks.csv", modelLooks.toCsv());
        auto keep = item;
        keep.style = knob.getDesign().style;
        keep.colour = knob.getDesign().colour;
        knob.applyDesign(keep);
        if (lookStudio) lookStudio->refresh();
    } else {
        for (auto& k : knobDefaults)
            if (k.first == &knob) k.second = item.style;
        knob.applyDesign(item);
    }
    lastModel = -1;
    applyModelLook();
}

void OpenPreampEditor::applyMeterStyles()
{
    goodlookinui::juce_adapter::retro::glowEnabled = proc.apvts.state.getProperty("glow", "on").toString() != "off";
    const auto faceCode = proc.apvts.state.getProperty("meterFace", "cream").toString();
    const auto face = MeterPanel::faceFromCode(faceCode);
    meterPanel.setFace(face);
    rightMeter.setFace(face);
    const bool overridden = proc.apvts.state.hasProperty("meterFace") && faceCode != "cream";
    meterPanel.setMeterFaceOverridden(overridden);
    rightMeter.setMeterFaceOverridden(overridden);
}

void OpenPreampEditor::showKnobStyleMenu()
{
    juce::PopupMenu menu;
    const auto current = proc.apvts.state.getProperty("knobStyle", "").toString();
    menu.addItem(1, "DEFAULT (each knob keeps its own style)", true, current.isEmpty());
    menu.addItem(2, "Chrome  -  OpenPreamp tactile vector chrome (1.1.0)", true, current == "Chrome");
    menu.addSeparator();
    int id = 3;
    for (const auto& s : goodlookinui::knobStyles)
        menu.addItem(id++, juce::String(s.code) + "  -  " + s.description, true, current == s.code);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&knobStyleBtn),
        [safe = juce::Component::SafePointer<OpenPreampEditor>(this)](int r) {
            if (!safe || r <= 0) return;
            if (r == 1) safe->applyKnobStyle(juce::String());
            else if (r == 2) safe->applyKnobStyle("Chrome");
            else safe->applyKnobStyle(juce::String(goodlookinui::knobStyles[r - 3].code));
            safe->saveSettingsFile();
        });
}

void OpenPreampEditor::showLookMenu()
{
    juce::PopupMenu menu;
    const auto mode = getTriggerMode();
    menu.addSectionHeader("Colour trigger");
    menu.addItem(1, "Off", true, mode == TriggerMode::Off);
    menu.addItem(2, "Per section (knob style, colour and plate follow each model)", true, mode == TriggerMode::Control);
    menu.addItem(3, "Whole UI (faceplate follows Preamp type)", true, mode == TriggerMode::Faceplate);
    menu.addSeparator();

    const auto code = proc.apvts.state.getProperty("lookFaceplate", "").toString();
    const auto plateCode = proc.apvts.state.getProperty("lookPlate", "").toString();

    juce::PopupMenu faces, plates;
    faces.addItem(499, "None (follow default)", true, code.isEmpty());
    faces.addSeparator();
    plates.addItem(500, "None (follow trigger / faceplate)", true, plateCode.isEmpty());
    plates.addSeparator();
    int n = 0;
    for (const auto& f : goodlookinui::faceplates) {
        faces.addItem(1000 + n, juce::String(f.code) + "  -  " + f.name, true, code == f.code);
        plates.addItem(2000 + n, juce::String(f.code) + "  -  " + f.name, true, plateCode == f.code);
        ++n;
    }

    juce::PopupMenu meters, pro, retro;
    const auto currentFace = juce::String(MeterPanel::faceCode(meterPanel.getFace()));
    for (int i = 0; i < int(std::size(MeterPanel::faces)); ++i)
        (MeterPanel::faces[i].retro ? retro : pro).addItem(3000 + i, MeterPanel::faces[i].name, true, currentFace == MeterPanel::faces[i].code);
    meters.addSubMenu("Meter face: professional hardware", pro);
    meters.addSubMenu("Meter face: retro / 80s / cyberpunk", retro);
    meters.addSeparator();
    meters.addItem(3200, "Glow, scanlines and flicker (neon look)", true, goodlookinui::juce_adapter::retro::glowEnabled);

    menu.addSubMenu("Meter face", meters);
    menu.addSubMenu("Faceplate (whole UI)", faces);
    menu.addSubMenu("Plate for all sections", plates);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&lookBtn),
        [safe = juce::Component::SafePointer<OpenPreampEditor>(this)](int r) {
            if (!safe || r <= 0) return;
            auto& st = safe->proc.apvts.state;
            if (r <= 3) {
                st.setProperty("lookTrigger", r == 1 ? "off" : r == 2 ? "control" : "faceplate", nullptr);
            } else if (r >= 3000 && r < 3000 + int(std::size(MeterPanel::faces))) {
                st.setProperty("meterFace", MeterPanel::faces[r - 3000].code, nullptr);
                safe->applyMeterStyles();
                safe->saveSettingsFile();
                return;
            } else if (r == 3200) {
                st.setProperty("glow", goodlookinui::juce_adapter::retro::glowEnabled ? "off" : "on", nullptr);
                safe->applyMeterStyles();
                safe->saveSettingsFile();
                return;
            } else if (r == 499) {
                st.setProperty("lookFaceplate", "", nullptr);
            } else if (r == 500) {
                st.setProperty("lookPlate", "", nullptr);
            } else if (r >= 2000) {
                st.setProperty("lookPlate", juce::String(goodlookinui::faceplates[r - 2000].code), nullptr);
            } else if (r >= 1000) {
                st.setProperty("lookFaceplate", juce::String(goodlookinui::faceplates[r - 1000].code), nullptr);
            }
            safe->saveSettingsFile();
            safe->lastModel = -1;
            safe->applyModelLook();
        });
}

void OpenPreampEditor::setupDeveloperTools()
{
    canvas.addAndMakeVisible(developerButton);
    canvas.addAndMakeVisible(lookBtn);
    canvas.addAndMakeVisible(knobStyleBtn);

    developerButton.setBounds(190, 14, 46, 24);
    developerButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff182b3a));
    developerButton.setColour(juce::TextButton::textColourOffId, PreampChrome::ink);
    developerButton.setTooltip("Open the GoodLookinUI developer window (Knobs, Model looks, Gallery).");

    lookBtn.setBounds(242, 14, 56, 24);
    lookBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff182b3a));
    lookBtn.setColour(juce::TextButton::textColourOffId, PreampChrome::ink);
    lookBtn.setTooltip("Choose trigger mode, faceplate, section plates, and meter face.");
    lookBtn.onClick = [this] { showLookMenu(); };

    knobStyleBtn.setBounds(700, 14, 96, 24);
    knobStyleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff182b3a));
    knobStyleBtn.setColour(juce::TextButton::textColourOffId, PreampChrome::ink);
    knobStyleBtn.setTooltip("Override knob style across the whole plugin to compare styles.");
    knobStyleBtn.onClick = [this] { showKnobStyleMenu(); };

    designAutosave.setFolder(juce::File(OPENPREAMP_DESIGNS_DIR));
    loadDesignSettings();

    const auto savedLooks = designAutosave.read("OpenPreampLooks.csv");
    if (!savedLooks.empty()) modelLooks.fromCsv(savedLooks);

    struct KnobEntry { RotaryKnob* knob; const char* id; const char* name; };
    const KnobEntry knobs[] = {
        {&preampGainDial,  "preampGain",          "Left / Mid input"},
        {&outputGainDial,  "outputGain",          "Left / Mid output"},
        {&rightGainDial,   "preampGainR",         "Right / Side input"},
        {&rightOutputDial, "outputGainR",         "Right / Side output"},
        {&highPassDial,    "highPass",            "Input high-pass"},
        {&lowPassDial,     "lowPass",             "Input low-pass"},
        {&monoMakerDial,   "monoMakerFrequency",  "Mono maker frequency"}
    };
    for (const auto& k : knobs) {
        auto item = k.knob->getDesign();
        item.id = item.parameter = k.id;
        const auto bounds = k.knob->getBounds();
        item.x = float(bounds.getX()); item.y = float(bounds.getY());
        item.width = float(bounds.getWidth()); item.height = float(bounds.getHeight());
        auto* knobPtr = k.knob;
        designStudio.add(item, *knobPtr, [this, knobPtr](const goodlookinui::Item& changed) {
            onKnobDesignEdited(*knobPtr, changed);
        }, k.name);
        k.knob->onSelect = [this, knobPtr] { designStudio.select(knobPtr); };
    }

    initializingTools = true;
    const auto savedKnobs = designAutosave.read("OpenPreampKnobs.csv");
    if (!savedKnobs.empty()) designStudio.fromCsv(savedKnobs);
    initializingTools = false;

    designStudio.saveTransform = [this](const goodlookinui::Item& it) {
        auto out = it;
        for (auto& k : knobDefaults)
            if (k.first && (k.first->getDesign().id == it.id || k.first->getDesign().parameter == it.parameter))
                out.style = k.second;
        return out;
    };

    designStudio.onChanged = [this] {
        designAutosave.request("OpenPreampKnobs.csv", designStudio.toCsv());
    };

    lookStudio = std::make_unique<LookStudio>(modelLooks, std::vector<LookStudio::Section>{{"preampType", "Preamp model"}},
        [this](const std::string& id) {
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(id))) return choice->choices;
            return juce::StringArray{};
        });
    lookStudio->onChanged = [this] {
        lastModel = -1;
        applyModelLook();
        designAutosave.request("OpenPreampLooks.csv", modelLooks.toCsv());
    };

    gallery = std::make_unique<goodlookinui::juce_adapter::Gallery>();

    auto* tabs = new juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop);
    tabs->addTab("Knobs / layout", juce::Colour(0xff18222c), &designStudio, false);
    tabs->addTab("Model looks", juce::Colour(0xff18222c), lookStudio.get(), false);
    tabs->addTab("Gallery", juce::Colour(0xff18222c), gallery.get(), false);
    tabs->setSize(1280, 600);

    developerWindow = std::make_unique<DesignWindow>();
    developerWindow->setContentOwned(tabs, true);
    developerWindow->setResizeLimits(1000, 400, 1920, 1080);
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        developerWindow->setBounds(display->userArea.withSizeKeepingCentre(1280, 600));
    else
        developerWindow->setBounds(40, 40, 1280, 600);

    developerButton.onClick = [this] {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter("preampType")))
            if (lookStudio) lookStudio->follow("preampType", choice->getCurrentChoiceName());
        developerWindow->setVisible(true);
        developerWindow->toFront(true);
    };

    designAutosave.onSaved = [this](const goodlookinui::juce_adapter::DesignAutosave::Event& e) {
        juce::String msg;
        if (!e.ok) msg = "NOT SAVED  " + e.name + "  -  " + e.error;
        else if (e.wrote) msg = "Saved  " + e.name + "  ->  " + e.file.getFullPathName() + "   (" + e.time.formatted("%H:%M:%S") + ")";
        else return;
        designStudio.setStatus(msg);
        if (lookStudio) lookStudio->setStatus(msg);
    };

    applyKnobStyle(proc.apvts.state.getProperty("knobStyle", "").toString());
    applyMeterStyles();
    lastModel = -1;
    applyModelLook();
}
#endif
