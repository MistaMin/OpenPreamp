#include "PluginEditor.h"
#include <EmbeddedLooks.h>
#include <EmbeddedOpenPreampDesign.h>

OpenPreampEditor::OpenPreampEditor(OpenPreampProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      rightMeter(p), rightMeterSourceBtn(p.apvts, "meterSource", "VU", true),
      midSideToggle(p.apvts, "midSide", "M / S", Theme::preampCol),
      cutsToggle(p.apvts, "cutsEnabled", "CUTS", Theme::preampCol),
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
    rightPreampPanel.setAccent(Theme::preampCol, "INDEPENDENT INPUT GAIN");
    rightOutputPanel.setAccent(Theme::outputCol, "INDEPENDENT OUTPUT TRIM");
    rightPreampPanel.addAndMakeVisible(rightGainDial);
    rightPreampPanel.addAndMakeVisible(midSideToggle);
    rightOutputPanel.addAndMakeVisible(rightOutputDial);
    rightOutputPanel.addAndMakeVisible(rightRateLabel);
    canvas.addChildComponent(rightMeter);
    canvas.addChildComponent(rightMeterSourceBtn);
    canvas.addChildComponent(rightPreampPanel);
    canvas.addChildComponent(rightOutputPanel);
    for (auto* c : std::initializer_list<juce::Component*>{&highPassDial, &lowPassDial, &cutsToggle, &expandButton})
        preampPanel.addAndMakeVisible(c);
    meterPanel.setChannel(0); rightMeter.setChannel(1);
    meterPanel.setExternallyDriven(); rightMeter.setExternallyDriven();
    expandButton.onClick = [this] { setExpanded(!expanded); };
    expandButton.setTooltip("Show or hide the second channel strip.");
    midSideToggle.setTooltip("Encode L/R to Mid/Side before the cuts and preamp; decode after downsampling and output trims.");
    cutsToggle.setTooltip("Shared 12 dB/octave high-pass and low-pass before the preamp, at the session rate.");
    rightGainAtt = std::make_unique<SliderAttachment>(p.apvts,"preampGainR",rightGainDial);
    rightOutputAtt = std::make_unique<SliderAttachment>(p.apvts,"outputGainR",rightOutputDial);
    highPassAtt = std::make_unique<SliderAttachment>(p.apvts,"highPass",highPassDial);
    lowPassAtt = std::make_unique<SliderAttachment>(p.apvts,"lowPass",lowPassDial);
    rightGainDial.setDoubleClickReturnValue(true,0); rightOutputDial.setDoubleClickReturnValue(true,0);
    highPassDial.setDoubleClickReturnValue(true,20); lowPassDial.setDoubleClickReturnValue(true,20000);
    rightRateLabel.setFont(juce::FontOptions(10.0f));
    rightRateLabel.setJustificationType(juce::Justification::centredRight);
    preampPanel.setAccent(Theme::preampCol, "INPUT / ANALOG CHARACTER");
    preampPanel.setBypassButton(preampBypassBtn);
    for (auto* control : std::initializer_list<juce::Component*>{&preampPadBtn, &preampGainDial, &preampTypeCircuitPair})
        preampPanel.addAndMakeVisible(control);
    preampTypeCircuitPair.setChildren(preampTypeBtn, preampCircuitToggle);
    canvas.addAndMakeVisible(preampPanel);
    outputPanel.setAccent(Theme::outputCol, "LEVEL TRIM / QUALITY");
    outputPanel.addAndMakeVisible(outputGainDial);
    outputPanel.addAndMakeVisible(hqToggle);
    outputPanel.addAndMakeVisible(rateLabel);
    rateLabel.setFont(juce::FontOptions(10.0f));
    rateLabel.setJustificationType(juce::Justification::centredRight);
    rateLabel.setInterceptsMouseClicks(false, false);
    canvas.addAndMakeVisible(outputPanel);
    canvas.addAndMakeVisible(meterPanel);
    canvas.addAndMakeVisible(meterSourceBtn);
    preampGainAtt = std::make_unique<SliderAttachment>(p.apvts, "preampGain", preampGainDial);
    outputGainAtt = std::make_unique<SliderAttachment>(p.apvts, "outputGain", outputGainDial);
    preampGainDial.setDoubleClickReturnValue(true, 0.0);
    outputGainDial.setDoubleClickReturnValue(true, 0.0);
    preampGainDial.setTooltip("Preamp drive. Double-click to return to 0 dB.");
    outputGainDial.setTooltip("Output trim after the preamp. Double-click to return to 0 dB.");
    preampBypassBtn.setTooltip("Bypass the preamp; output trim and meters remain active.");
    preampPadBtn.setButtonTooltip("Input drive offset: -20 dB, unity, or +10 dB.");
    preampTypeBtn.setButtonTooltip("Select preamp character. Off keeps clean drive without coloration.");
    preampCircuitToggle.setTooltip("Circuit on: component simulation at 2x. Circuit off: preamp coloration with ADAA at the session rate.");
    hqToggle.setTooltip("Use 4x oversampling with the circuit on. The circuit-off path stays at the session rate with ADAA.");
    meterSourceBtn.setButtonTooltip("Select input or output for the VU needle, peak hold and clip lamp. Both stereo ladders stay visible.");
    setResizable(true, true);
    setResizeLimits(450, 630, 1000, 1400);
    getConstrainer()->setFixedAspectRatio(5.0 / 7.0);
    setSize(600, 840);
    loadKnobLayout(openPreampKnobDesign);
    layoutReady = true;
    timerCallback();
#if GOODLOOKINUI_ENABLE_EDITOR
    setupDeveloperTools();
#endif
    if (bool(p.apvts.state.getProperty("expanded",false))) setExpanded(true);
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
    for (auto* panel : {&preampPanel, &outputPanel, &rightPreampPanel, &rightOutputPanel}) {
        panel->setAccentColour(colour);
        if (plate) panel->setPlate(juce::Colour(plate->panelTop), juce::Colour(plate->panelBottom),
                                  juce::Colour(plate->text), juce::Colour(plate->textMid), plate->finish);
    }
    for (auto* knob : {&preampGainDial, &outputGainDial, &rightGainDial, &rightOutputDial, &highPassDial, &lowPassDial}) {
        auto design = knob->getDesign(); design.style = look->style; design.colour = look->colour;
        knob->applyDesign(design);
        if (plate) { const juce::Colour text(plate->text), mid(plate->textMid); knob->setPlateText(&text, &mid); }
    }
    if (plate) {
        const juce::Colour text(plate->text), mid(plate->textMid);
        preampPadBtn.setPlateText(&text, &mid); preampTypeBtn.setPlateText(&text, &mid);
        rateLabel.setColour(juce::Label::textColourId, mid);
        rightRateLabel.setColour(juce::Label::textColourId, mid);
    }
    preampBypassBtn.setAccent(colour);
    preampCircuitToggle.setAccent(colour); hqToggle.setAccent(colour);
    cutsToggle.setAccent(colour); midSideToggle.setAccent(colour);
    repaint();
}

void OpenPreampEditor::paint(juce::Graphics& g)
{
    g.addTransform(juce::AffineTransform::scale(float(getHeight()) / 840.0f));
    g.setGradientFill(juce::ColourGradient(editorLook.bgTop, 0.0f, 0.0f,
                                          editorLook.bgBottom, 0.0f, 840.0f, false));
    g.fillAll();
    g.setColour(editorLook.titleBar); g.fillRect(0, 0, expanded ? 1200 : 600, 60);
    g.setColour(editorLook.accent); g.fillRect(22, 18, 3, 24);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.setColour(editorLook.text); g.drawText("OpenPreamp", 36, 14, 240, 32, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(9.5f)); g.setColour(editorLook.textMid);
    g.drawText(juce::String(JucePlugin_VersionString) + "  /  OPENGRID", 400, 20, 176, 24, juce::Justification::centredRight);
    g.setColour(editorLook.border); g.drawHorizontalLine(60, 16.0f, float((expanded ? 1200 : 600) - 16));
    g.setFont(juce::FontOptions(9.5f)); g.setColour(editorLook.textMid);
    g.drawText("0 VU = -18 dBFS", 24, 291, 220, 12, juce::Justification::centredLeft);
    g.setColour(editorLook.textMid);
    g.drawText("CUTS  >  GAIN  >  PREAMP  >  OUTPUT", 24, 816, 270, 18, juce::Justification::centredLeft);
    g.drawText("Click CLIP to reset", 400, 816, 176, 18, juce::Justification::centredRight);
    if (expanded) {
        g.drawText("INDEPENDENT CHANNEL", 636, 20, 300, 24, juce::Justification::centredLeft);
        g.drawText("M / S encodes before the preamp and decodes to L / R at output", 624, 816, 552, 18, juce::Justification::centredLeft);
    }
}

void OpenPreampEditor::resized()
{
    canvas.setBounds(0, 0, expanded ? 1200 : 600, 840);
    canvas.setTransform(juce::AffineTransform::scale(float(getHeight()) / 840.0f));
    meterPanel.setBounds(16, 76, 568, 214);
    meterSourceBtn.setBounds(474, 83, 92, 27);
    preampPanel.setBounds(16, 304, 568, 286);
    preampPadBtn.setBounds(38, 86, 82, 60);
    if (!layoutReady) preampGainDial.setBounds(196, 56, 176, 198);
    preampTypeCircuitPair.setBounds(428, 70, 94, 98);
    outputPanel.setBounds(16, 602, 568, 198);
    if (!layoutReady) outputGainDial.setBounds(52, 52, 132, 144);
    hqToggle.setBounds(392, 68, 96, 32);
    rateLabel.setBounds(276, 114, 264, 26);
    highPassDial.setBounds(54,174,110,108);
    lowPassDial.setBounds(408,174,110,108);
    cutsToggle.setBounds(240,258,88,24);
    expandButton.setBounds(536,250,24,28);
    rightMeter.setBounds(616,76,568,214);
    rightMeterSourceBtn.setBounds(1074,83,92,27);
    rightPreampPanel.setBounds(616,304,568,286);
    rightOutputPanel.setBounds(616,602,568,198);
    if (!layoutReady) {
        rightGainDial.setBounds(preampGainDial.getBounds());
        rightOutputDial.setBounds(outputGainDial.getBounds());
    }
    midSideToggle.setBounds(428,100,94,30);
    rightRateLabel.setBounds(276,114,264,26);
#if GOODLOOKINUI_ENABLE_EDITOR
    developerButton.setBounds(316, 20, 60, 24);
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
    preampPanel.setTitle(ms ? "MID PREAMP" : "LEFT PREAMP");
    rightPreampPanel.setTitle(ms ? "SIDE PREAMP" : "RIGHT PREAMP");
    outputPanel.setTitle(ms ? "MID OUTPUT" : "LEFT OUTPUT");
    rightOutputPanel.setTitle(ms ? "SIDE OUTPUT" : "RIGHT OUTPUT");
    midSideToggle.setActive(proc.getTotalNumInputChannels() == 2);
    if (isShowing()) {
        const auto input = proc.getInputLevel().read(), output = proc.getOutputLevel().read();
        meterPanel.step(input,output,1.0f/30); rightMeter.step(input,output,1.0f/30);
        meterPanel.repaint(); rightMeter.repaint();
    }
    const int factor = proc.getEffectiveOversampleFactor();
    const auto text = juce::String(factor) + "x  /  " + juce::String(proc.getProcessingSampleRate() / 1000.0, 1) + " kHz";
    if (rateText != text) { rateText = text; rateLabel.setText(text, juce::dontSendNotification); rightRateLabel.setText(text,juce::dontSendNotification); }
}

OpenPreampEditor::~OpenPreampEditor()
{
    stopTimer();
    proc.apvts.state.setProperty("expanded",expanded,nullptr);
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
    developerButton.setBounds(316, 20, 60, 24);
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

void OpenPreampEditor::setExpanded(bool show)
{
    if (expanded == show) return;
    expanded = show;
    const int h = getHeight();
    getConstrainer()->setFixedAspectRatio(show ? 10.0/7.0 : 5.0/7.0);
    setResizeLimits(show ? 900 : 450,630,show ? 2000 : 1000,1400);
    expandButton.setButtonText(show ? "<" : ">");
    for (auto* c : std::initializer_list<juce::Component*>{&rightMeter,&rightMeterSourceBtn,&rightPreampPanel,&rightOutputPanel}) c->setVisible(show);
    setSize(h*(show ? 10 : 5)/7,h);
    proc.apvts.state.setProperty("expanded",show,nullptr);
    resized(); repaint();
}
