#include "PluginEditor.h"
#include <EmbeddedLooks.h>

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
    modelLooks.fromCsv(hybridEQLooks);
    preampPanel.setAccent(Theme::preampCol, "INPUT / ANALOG CHARACTER");
    preampPanel.setBypassButton(preampBypassBtn);
    for (auto* control : std::initializer_list<juce::Component*>{&preampPadBtn, &preampGainDial, &preampTypeCircuitPair})
        preampPanel.addAndMakeVisible(control);
    preampTypeCircuitPair.setChildren(preampTypeBtn, preampCircuitToggle);
    addAndMakeVisible(preampPanel);
    outputPanel.setAccent(Theme::outputCol, "LEVEL TRIM / QUALITY");
    outputPanel.addAndMakeVisible(outputGainDial);
    outputPanel.addAndMakeVisible(hqToggle);
    outputPanel.addAndMakeVisible(rateLabel);
    rateLabel.setFont(juce::FontOptions(10.0f));
    rateLabel.setJustificationType(juce::Justification::centredRight);
    rateLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(outputPanel);
    addAndMakeVisible(meterPanel);
    addAndMakeVisible(meterSourceBtn);
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
    setSize(500, 800);
    timerCallback();
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
    g.setGradientFill(juce::ColourGradient(editorLook.bgTop, 0.0f, 0.0f,
                                          editorLook.bgBottom, 0.0f, float(getHeight()), false));
    g.fillAll();
    g.setColour(editorLook.titleBar); g.fillRect(0, 0, getWidth(), 60);
    g.setColour(editorLook.accent); g.fillRect(22, 18, 3, 24);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.setColour(editorLook.text); g.drawText("OpenPreamp", 36, 14, 240, 32, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(9.5f)); g.setColour(editorLook.textMid);
    g.drawText(juce::String(JucePlugin_VersionString) + "  /  OPENGRID", 300, 20, 176, 24, juce::Justification::centredRight);
    g.setColour(editorLook.border); g.drawHorizontalLine(60, 16.0f, float(getWidth() - 16));
    g.setFont(juce::FontOptions(9.5f)); g.setColour(editorLook.textMid);
    g.drawText("0 VU = -18 dBFS", 24, 248, 220, 18, juce::Justification::centredLeft);
    g.setColour(editorLook.textMid);
    g.drawText("PAD  >  PREAMP  >  OUTPUT", 24, 774, 270, 18, juce::Justification::centredLeft);
    g.drawText("Click CLIP to reset", 300, 774, 176, 18, juce::Justification::centredRight);
}

void OpenPreampEditor::resized()
{
    meterPanel.setBounds(16, 76, 468, 168);
    meterSourceBtn.setBounds(350, 83, 116, 27);
    preampPanel.setBounds(16, 280, 468, 286);
    preampPadBtn.setBounds(18, 122, 94, 60);
    preampGainDial.setBounds(140, 66, 178, 202);
    preampTypeCircuitPair.setBounds(338, 100, 112, 100);
    outputPanel.setBounds(16, 582, 468, 178);
    outputGainDial.setBounds(30, 48, 116, 120);
    hqToggle.setBounds(286, 68, 152, 32);
    rateLabel.setBounds(198, 106, 252, 26);
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
