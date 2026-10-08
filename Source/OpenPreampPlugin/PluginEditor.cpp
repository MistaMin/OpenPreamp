#include "PluginEditor.h"

OpenPreampEditor::OpenPreampEditor(OpenPreampProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      preampTypeBtn(p.apvts, "preampType", "MODEL"),
      preampPadBtn(p.apvts, "preampPad", "PAD"),
      meterPanel(p),
      preampBypassBtn(p.apvts, "preampBypass", Theme::preampCol),
      preampCircuitToggle(p.apvts, "preampCircuit", "CIRCUIT", Theme::preampCol)
{
    preampPanel.setAccent(Theme::preampCol, "INPUT / ANALOG CHARACTER");
    preampPanel.setBypassButton(preampBypassBtn);
    preampPanel.addAndMakeVisible(preampPadBtn);
    preampPanel.addAndMakeVisible(preampGainDial);
    preampPanel.addAndMakeVisible(preampTypeCircuitPair);
    preampTypeCircuitPair.setChildren(preampTypeBtn, preampCircuitToggle);
    addAndMakeVisible(preampPanel);

    outputPanel.setAccent(Theme::outputCol, "LEVEL TRIM");
    outputPanel.addAndMakeVisible(outputGainDial);
    addAndMakeVisible(outputPanel);
    addAndMakeVisible(meterPanel);

    preampGainAtt = std::make_unique<SliderAttachment>(p.apvts, "preampGain", preampGainDial);
    outputGainAtt = std::make_unique<SliderAttachment>(p.apvts, "outputGain", outputGainDial);
    preampGainDial.setDoubleClickReturnValue(true, 0.0);
    outputGainDial.setDoubleClickReturnValue(true, 0.0);
    preampGainDial.setTooltip("Drive the preamp. Double-click to return to 0 dB.");
    outputGainDial.setTooltip("Output trim after the preamp. Double-click to return to 0 dB.");
    preampPadBtn.setButtonTooltip("Input gain offset: -20 dB, unity, or +10 dB.");
    preampTypeBtn.setButtonTooltip("Choose the preamp circuit character. Off passes audio without preamp coloration.");
    preampCircuitToggle.setTooltip("Enable the component-level circuit simulation. Runs at the session sample rate.");
    setSize(860, 330);
    timerCallback();
    startTimerHz(30);
}

void OpenPreampEditor::paint(juce::Graphics& g)
{
    g.setGradientFill(juce::ColourGradient(Theme::editorTop, 0.0f, 0.0f,
                                          Theme::editorBottom, 0.0f, float(getHeight()), false));
    g.fillAll();
    g.setColour(Theme::titleBar);
    g.fillRect(0, 0, getWidth(), 62);
    g.setColour(Theme::preampCol);
    g.fillRect(24, 20, 3, 24);
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.setColour(Theme::textDark);
    g.drawText("OpenPreamp", 38, 15, 240, 32, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(10.0f));
    g.setColour(Theme::textMid);
    g.drawText("ANALOG PREAMP", 300, 22, 180, 22, juce::Justification::centredLeft);
    g.drawText("OPENGRID", getWidth() - 160, 22, 130, 22, juce::Justification::centredRight);
    g.setColour(Theme::border);
    g.drawHorizontalLine(62, 16.0f, float(getWidth() - 16));
    g.setColour(Theme::textMid);
    g.setFont(juce::FontOptions(9.5f));
    g.drawText("PAD  >  PREAMP  >  OUTPUT", 24, 298, 360, 18, juce::Justification::centredLeft);
    g.drawText("0 VU = -18 dBFS", 548, 218, 296, 16, juce::Justification::centred);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("SESSION RATE", 560, 244, 116, 25, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText(rateText, 688, 244, 150, 25, juce::Justification::centredRight);
    g.drawText("Click CLIP to reset", 548, 298, 296, 18, juce::Justification::centredRight);
}

void OpenPreampEditor::resized()
{
    preampPanel.setBounds(16, 78, 370, 210);
    outputPanel.setBounds(398, 78, 138, 210);
    preampPadBtn.setBounds(16, 82, 82, 60);
    preampGainDial.setBounds(110, 58, 140, 140);
    preampTypeCircuitPair.setBounds(264, 72, 94, 100);
    outputGainDial.setBounds(10, 58, 118, 140);
    meterPanel.setBounds(548, 78, 296, 132);
}

void OpenPreampEditor::timerCallback()
{
    const bool active = proc.apvts.getRawParameterValue("preampBypass")->load() < 0.5f
                     && proc.apvts.getRawParameterValue("preampType")->load() != 3.0f;
    preampGainDial.setActive(active);
    preampPadBtn.setActive(active);
    preampTypeBtn.setActive(active);
    preampCircuitToggle.setActive(active);
    const auto text = juce::String(proc.getProcessingSampleRate() / 1000.0, 1) + " kHz";
    if (rateText != text) { rateText = text; repaint(); }
}
