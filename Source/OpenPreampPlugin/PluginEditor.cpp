#include "PluginEditor.h"
#include <EmbeddedLooks.h>
#include <EmbeddedOpenPreampDesign.h>

OpenPreampEditor::OpenPreampEditor(OpenPreampProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      rightMeter(p), rightMeterSourceBtn(p.apvts, "meterSource", "VU", true),
      midSideToggle(p.apvts, "midSide", "LR / M/S", Theme::preampCol),
      cutsToggle(p.apvts, "cutsEnabled", "CUTS", Theme::preampCol),
      linkToggle(p.apvts, "channelLink", "LINK", Theme::preampCol),
      monoMakerToggle(p.apvts, "monoMakerEnabled", "ON", Theme::preampCol),
      preampTypeBtn(p.apvts, "preampType", "MODEL"),
      preampPadBtn(p.apvts, "preampPad", "PAD"),
      meterPanel(p),
      meterSourceBtn(p.apvts, "meterSource", "VU", true),
      hqToggle(p.apvts, "hqMode", "HQ MODE", Theme::preampCol),
      preampBypassBtn(p.apvts, "preampBypass", Theme::preampCol),
      preampCircuitToggle(p.apvts, "preampCircuit", "CIRCUIT", Theme::preampCol)
{
    addAndMakeVisible(canvas);
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
    for (auto* c : std::initializer_list<juce::Component*>{&preampPadBtn,&preampTypeBtn,&preampCircuitToggle,&hqToggle,&rateLabel}) controlsPanel.addAndMakeVisible(c);
    for (auto* c : std::initializer_list<juce::Component*>{&preampPanel,&rightPreampPanel,&cutsPanel,&controlsPanel,&monoMakerPanel,&midSideToggle,&linkToggle,&meterPanel,&rightMeter,&meterSourceBtn,&rightMeterSourceBtn}) canvas.addAndMakeVisible(c);
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
    preampGainDial.setTooltip("Left / Mid input gain. Double-click for 0 dB.");
    rightGainDial.setTooltip("Right / Side input gain. Double-click for 0 dB.");
    outputGainDial.setTooltip("Left / Mid output trim. Double-click for 0 dB.");
    rightOutputDial.setTooltip("Right / Side output trim. Double-click for 0 dB.");
    preampBypassBtn.setTooltip("Bypass cuts and preamp; output trims and meters remain active.");
    preampPadBtn.setButtonTooltip("Shared input offset: -20 dB, unity, or +10 dB.");
    preampTypeBtn.setButtonTooltip("Shared preamp model for both channels.");
    preampCircuitToggle.setTooltip("Circuit at 2x, or lighter native-rate preamp with ADAA when off.");
    hqToggle.setTooltip("Use 4x oversampling when CIRCUIT is on.");
    midSideToggle.setTooltip("Encode Mid/Side before the preamps; decode to stereo after downsampling and output trims.");
    linkToggle.setTooltip("Link both input and output gains to the left controls. Right settings return when unlinked. Unavailable in M/S.");
    monoMakerToggle.setTooltip("Reduce bass width with a session-rate 6 dB/octave high-pass on Side only, before the preamp.");
    monoMakerDial.setTooltip("Side high-pass cutoff: 20–500 Hz. Mid is unchanged. Double-click for 20 Hz.");
    cutsToggle.setTooltip("Shared session-rate 12 dB/octave high-pass and low-pass.");
    for (auto* button : {&meterSourceBtn,&rightMeterSourceBtn}) button->setButtonTooltip("Select input or output for both independent meters.");
    rateLabel.setFont(juce::FontOptions(10.0f));
    rateLabel.setJustificationType(juce::Justification::centredRight);
    rateLabel.setInterceptsMouseClicks(false,false);
    setResizable(true,true);
    setResizeLimits(750,555,1500,1110);
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
    const auto* plate = goodlookinui::findFaceplate(look->plate);
    if (!plate) plate = face;
    const auto colour = juce::Colour::fromString("ff" + juce::String(look->colour.substr(1)));
    for (auto* panel : {&preampPanel, &rightPreampPanel, &cutsPanel, &controlsPanel, &monoMakerPanel}) {
        panel->setAccentColour(colour);
        if (plate) panel->setPlate(juce::Colour(plate->panelTop), juce::Colour(plate->panelBottom),
                                  juce::Colour(plate->text), juce::Colour(plate->textMid), plate->finish);
    }
    for (auto* knob : {&preampGainDial, &outputGainDial, &rightGainDial, &rightOutputDial, &highPassDial, &lowPassDial, &monoMakerDial}) {
        auto design = knob->getDesign(); design.style = look->style; design.colour = look->colour;
        knob->applyDesign(design);
        if (plate) { const juce::Colour text(plate->text), mid(plate->textMid); knob->setPlateText(&text, &mid); }
    }
    if (plate) {
        const juce::Colour text(plate->text), mid(plate->textMid);
        preampPadBtn.setPlateText(&text, &mid); preampTypeBtn.setPlateText(&text, &mid);
        rateLabel.setColour(juce::Label::textColourId, mid);
    }
    preampBypassBtn.setAccent(colour);
    preampCircuitToggle.setAccent(colour); hqToggle.setAccent(colour);
    linkToggle.setAccent(colour); monoMakerToggle.setAccent(colour);
    cutsToggle.setAccent(colour); midSideToggle.setAccent(colour);
    repaint();
}

void OpenPreampEditor::paint(juce::Graphics& g)
{
    g.addTransform(juce::AffineTransform::scale(float(getHeight())/740.0f));
    g.setGradientFill(juce::ColourGradient(editorLook.bgTop,0,0,editorLook.bgBottom,0,740,false));
    g.fillAll();
    g.setColour(editorLook.titleBar);g.fillRect(0,0,1000,52);
    g.setColour(editorLook.accent);g.fillRect(22,14,3,24);
    g.setColour(editorLook.text);g.setFont(juce::FontOptions(22.0f,juce::Font::bold));
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
    preampPanel.setBounds(16,306,476,218);rightPreampPanel.setBounds(508,306,476,218);
    if(!layoutReady) {
        preampGainDial.setBounds(60,54,148,154);outputGainDial.setBounds(286,54,132,154);
        rightGainDial.setBounds(preampGainDial.getBounds());rightOutputDial.setBounds(outputGainDial.getBounds());
    }
    cutsPanel.setBounds(16,540,356,164);monoMakerPanel.setBounds(388,540,196,164);controlsPanel.setBounds(600,540,384,164);
    highPassDial.setBounds(24,54,108,102);lowPassDial.setBounds(150,54,108,102);
    cutsToggle.setBounds(270,80,70,30);
    monoMakerDial.setBounds(12,54,98,102);monoMakerToggle.setBounds(116,80,66,30);
    preampPadBtn.setBounds(18,56,82,60);preampTypeBtn.setBounds(118,56,94,60);
    preampCircuitToggle.setBounds(236,54,126,30);hqToggle.setBounds(236,96,126,30);
    midSideToggle.setBounds(405,14,110,24);linkToggle.setBounds(525,14,80,24);
    rateLabel.setBounds(184,132,178,20);
#if GOODLOOKINUI_ENABLE_EDITOR
    developerButton.setBounds(316,14,60,24);
#endif
}

void OpenPreampEditor::timerCallback()
{
    applyModelLook();
    rightMeter.setInputSource(proc.apvts.getRawParameterValue("meterSource")->load() < 0.5f);
    meterPanel.setInputSource(proc.apvts.getRawParameterValue("meterSource")->load() < 0.5f);
    const bool active = proc.apvts.getRawParameterValue("preampBypass")->load() < 0.5f
                     && proc.apvts.getRawParameterValue("preampType")->load() != 3.0f;
    preampGainDial.setActive(proc.apvts.getRawParameterValue("preampBypass")->load() < 0.5f); preampPadBtn.setActive(proc.apvts.getRawParameterValue("preampBypass")->load() < 0.5f);
    preampTypeBtn.setActive(active); preampCircuitToggle.setActive(active);
    hqToggle.setActive(active && preampCircuitToggle.getToggleState());
    rightGainDial.setActive(!proc.apvts.getRawParameterValue("preampBypass")->load());
    highPassDial.setActive(cutsToggle.getToggleState() && proc.apvts.getRawParameterValue("preampBypass")->load() < 0.5f); lowPassDial.setActive(highPassDial.getActive());
    const bool ms = proc.apvts.getRawParameterValue("midSide")->load() > 0.5f && proc.getTotalNumInputChannels() == 2;
    meterPanel.setCaption(ms ? "MID LEVEL" : "LEFT LEVEL");
    rightMeter.setCaption(ms ? "SIDE LEVEL" : "RIGHT LEVEL");
    preampPanel.setTitle(ms ? "MID" : "LEFT");
    rightPreampPanel.setTitle(ms ? "SIDE" : "RIGHT");
    const bool stereo = proc.getTotalNumInputChannels() == 2;
    const bool linked = !ms && proc.apvts.getRawParameterValue("channelLink")->load() > 0.5f;
    linkToggle.setVisible(!ms); linkToggle.setEnabled(stereo); linkToggle.setActive(stereo);
    rightGainDial.setEnabled(stereo && !linked); rightOutputDial.setEnabled(stereo && !linked);
    rightGainDial.setActive(stereo && !linked && !proc.apvts.getRawParameterValue("preampBypass")->load());
    rightOutputDial.setActive(stereo && !linked);
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
    const auto text = juce::String(factor) + "x  /  " + juce::String(proc.getProcessingSampleRate() / 1000.0, 1) + " kHz";
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
