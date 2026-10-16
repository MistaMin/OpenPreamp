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
    loadKnobLayout(openPreampKnobDesign);
    layoutReady=true;
    timerCallback();
#if GOODLOOKINUI_ENABLE_EDITOR
    setupDeveloperTools();
#endif
    startTimerHz(30);
}

void OpenPreampEditor::applyModelLook()
{
    const int model = int(proc.apvts.getRawParameterValue("preampType")->load());
    if (model == lastModel) return;
    lastModel = model;
    auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter("preampType"));
    if (!parameter) return;
    const auto* look = modelLooks.find("preampType", parameter->getCurrentChoiceName().toStdString());
    if (!look) return;
    const auto* face = goodlookinui::findFaceplate(look->faceplate);
    if (face) editorLook = Theme::Look::from(*face);
    editorLook.bgTop=juce::Colour(0xff102333);editorLook.bgBottom=juce::Colour(0xff06121c);
    editorLook.titleBar=juce::Colour(0xff0b1b29);editorLook.text=PreampChrome::ink;
    editorLook.textMid=PreampChrome::muted;editorLook.border=PreampChrome::rim;
    editorLook.accent=PreampChrome::accent;
    const auto* plate = goodlookinui::findFaceplate(look->plate);
    if (!plate) plate = face;
    const auto colour = juce::Colour::fromString("ff" + juce::String(look->colour.substr(1)));
    for (auto* panel : {&preampPanel, &rightPreampPanel, &cutsPanel, &controlsPanel, &monoMakerPanel}) {
        panel->setAccentColour(colour);
        if (plate) panel->setPlate(juce::Colour(plate->panelTop), juce::Colour(plate->panelBottom),
                                  PreampChrome::ink, PreampChrome::muted, plate->finish);
    }
    for (auto* knob : {&preampGainDial, &outputGainDial, &rightGainDial, &rightOutputDial, &highPassDial, &lowPassDial, &monoMakerDial}) {
        auto design = knob->getDesign(); design.style = look->style; design.colour = look->colour;
        knob->applyDesign(design);
        if (plate) { const juce::Colour text(PreampChrome::ink), mid(PreampChrome::muted); knob->setPlateText(&text, &mid); }
    }
    if (plate) {
        const juce::Colour text(PreampChrome::ink), mid(PreampChrome::muted);
        preampPadBtn.setPlateText(&text, &mid); preampTypeBtn.setPlateText(&text, &mid); circuitEngineBtn.setPlateText(&text,&mid);oversamplingBtn.setPlateText(&text,&mid);
        rateLabel.setColour(juce::Label::textColourId, mid);
    }
    preampBypassBtn.setAccent(colour);
    preampCircuitToggle.setAccent(colour);oversamplingBtn.refreshTheme();
    linkToggle.setAccent(colour); monoMakerToggle.setAccent(colour);
    cutsToggle.setAccent(colour); midSideToggle.setAccent(colour);gainCompensationToggle.setAccent(colour);
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
    developerButton.setBounds(246,14,56,24);
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
                     item.parameter == "preampGainR" ? &rightGainDial : item.parameter == "outputGainR" ? &rightOutputDial : nullptr;
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

void OpenPreampEditor::setupDeveloperTools()
{
    canvas.addAndMakeVisible(developerButton);
    developerButton.setBounds(316, 14, 60, 24);
    developerButton.setTooltip("Open the knob and model-look editors. Edits autosave to Designs/.");
    designAutosave.setFolder(juce::File(OPENPREAMP_DESIGNS_DIR));
    const auto savedLooks = designAutosave.read("OpenPreampLooks.csv");
    if (!savedLooks.empty()) modelLooks.fromCsv(savedLooks);
    lastModel = -1; applyModelLook();
    for (auto* knob : {&preampGainDial, &outputGainDial, &rightGainDial, &rightOutputDial}) {
        auto item = knob->getDesign();
        item.id = item.parameter = knob == &preampGainDial ? "preampGain" : knob == &outputGainDial ? "outputGain" : knob == &rightGainDial ? "preampGainR" : "outputGainR";
        const auto bounds = knob->getBounds();
        item.x = float(bounds.getX()); item.y = float(bounds.getY());
        item.width = float(bounds.getWidth()); item.height = float(bounds.getHeight());
        designStudio.add(item, *knob, [knob](const goodlookinui::Item& changed) { knob->applyDesign(changed); },
                         knob == &preampGainDial ? "Left / Mid input" : knob == &outputGainDial ? "Left / Mid output" : knob == &rightGainDial ? "Right / Side input" : "Right / Side output");
        knob->onSelect = [this, knob] { designStudio.select(knob); };
    }
    const auto savedKnobs = designAutosave.read("OpenPreampKnobs.csv");
    if (!savedKnobs.empty()) designStudio.fromCsv(savedKnobs);
    lastModel = -1; applyModelLook();
    designStudio.onChanged = [this] { designAutosave.request("OpenPreampKnobs.csv", designStudio.toCsv()); };
    lookStudio = std::make_unique<LookStudio>(modelLooks, std::vector<LookStudio::Section>{{"preampType", "Preamp model"}},
        [this](const std::string& id) {
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(id))) return choice->choices;
            return juce::StringArray{};
        });
    lookStudio->onChanged = [this] {
        lastModel = -1; applyModelLook();
        designAutosave.request("OpenPreampLooks.csv", modelLooks.toCsv());
    };
    auto* tabs = new juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop);
    tabs->addTab("Knobs / layout", juce::Colour(0xff18222c), &designStudio, false);
    tabs->addTab("Model looks", juce::Colour(0xff18222c), lookStudio.get(), false);
    tabs->setSize(1120, 210);
    developerWindow = std::make_unique<DesignWindow>();
    developerWindow->setContentOwned(tabs, true);
    developerWindow->setResizeLimits(1120, 250, 1800, 600);
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        developerWindow->setBounds(display->userArea.withSizeKeepingCentre(1120, 250));
    else
        developerWindow->setBounds(80, 80, 1120, 250);
    developerButton.onClick = [this] {
        developerWindow->setVisible(true);
        developerWindow->toFront(true);
    };
}
#endif
