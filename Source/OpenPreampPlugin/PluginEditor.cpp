#include "PluginEditor.h"
#include <EmbeddedLooks.h>
#include <EmbeddedOpenPreampDesign.h>

OpenPreampEditor::OpenPreampEditor(OpenPreampProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
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
    for (auto* panel : {&preampPanel, &outputPanel}) {
        panel->setAccentColour(colour);
        if (plate) panel->setPlate(juce::Colour(plate->panelTop), juce::Colour(plate->panelBottom),
                                  juce::Colour(plate->text), juce::Colour(plate->textMid), plate->finish);
    }
    for (auto* knob : {&preampGainDial, &outputGainDial}) {
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
    repaint();
}

void OpenPreampEditor::paint(juce::Graphics& g)
{
    g.addTransform(juce::AffineTransform::scale(float(getWidth()) / 600.0f));
    g.setGradientFill(juce::ColourGradient(editorLook.bgTop, 0.0f, 0.0f,
                                          editorLook.bgBottom, 0.0f, 840.0f, false));
    g.fillAll();
    g.setColour(editorLook.titleBar); g.fillRect(0, 0, 600, 60);
    g.setColour(editorLook.accent); g.fillRect(22, 18, 3, 24);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.setColour(editorLook.text); g.drawText("OpenPreamp", 36, 14, 240, 32, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(9.5f)); g.setColour(editorLook.textMid);
    g.drawText(juce::String(JucePlugin_VersionString) + "  /  OPENGRID", 400, 20, 176, 24, juce::Justification::centredRight);
    g.setColour(editorLook.border); g.drawHorizontalLine(60, 16.0f, float(600 - 16));
    g.setFont(juce::FontOptions(9.5f)); g.setColour(editorLook.textMid);
    g.drawText("0 VU = -18 dBFS", 24, 300, 220, 18, juce::Justification::centredLeft);
    g.setColour(editorLook.textMid);
    g.drawText("PAD  >  PREAMP  >  OUTPUT", 24, 816, 270, 18, juce::Justification::centredLeft);
    g.drawText("Click CLIP to reset", 400, 816, 176, 18, juce::Justification::centredRight);
}

void OpenPreampEditor::resized()
{
    canvas.setBounds(0, 0, 600, 840);
    canvas.setTransform(juce::AffineTransform::scale(float(getWidth()) / 600.0f));
    meterPanel.setBounds(16, 76, 568, 214);
    meterSourceBtn.setBounds(474, 83, 92, 27);
    preampPanel.setBounds(16, 330, 568, 264);
    preampPadBtn.setBounds(38, 108, 82, 60);
    if (!layoutReady) preampGainDial.setBounds(196, 56, 176, 198);
    preampTypeCircuitPair.setBounds(428, 90, 94, 100);
    outputPanel.setBounds(16, 602, 568, 198);
    if (!layoutReady) outputGainDial.setBounds(52, 52, 132, 144);
    hqToggle.setBounds(392, 68, 96, 32);
    rateLabel.setBounds(276, 114, 264, 26);
#if GOODLOOKINUI_ENABLE_EDITOR
    developerButton.setBounds(316, 20, 60, 24);
#endif
}

void OpenPreampEditor::timerCallback()
{
    applyModelLook();
    meterPanel.setInputSource(proc.apvts.getRawParameterValue("meterSource")->load() < 0.5f);
    const bool active = proc.apvts.getRawParameterValue("preampBypass")->load() < 0.5f
                     && proc.apvts.getRawParameterValue("preampType")->load() != 3.0f;
    preampGainDial.setActive(active); preampPadBtn.setActive(active);
    preampTypeBtn.setActive(active); preampCircuitToggle.setActive(active);
    hqToggle.setActive(active && preampCircuitToggle.getToggleState());
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
        auto* knob = item.parameter == "preampGain" ? &preampGainDial : item.parameter == "outputGain" ? &outputGainDial : nullptr;
        if (!knob) continue;
        knob->setBounds(juce::roundToInt(item.x), juce::roundToInt(item.y),
                        juce::roundToInt(item.width), juce::roundToInt(item.height));
        knob->applyDesign(item);
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
    for (auto* knob : {&preampGainDial, &outputGainDial}) {
        auto item = knob->getDesign();
        item.id = item.parameter = knob == &preampGainDial ? "preampGain" : "outputGain";
        const auto bounds = knob->getBounds();
        item.x = float(bounds.getX()); item.y = float(bounds.getY());
        item.width = float(bounds.getWidth()); item.height = float(bounds.getHeight());
        designStudio.add(item, *knob, [knob](const goodlookinui::Item& changed) { knob->applyDesign(changed); },
                         knob == &preampGainDial ? "Preamp gain" : "Output trim");
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
