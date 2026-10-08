#include "PluginEditor.h"

OpenPreampEditor::OpenPreampEditor(OpenPreampProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      preampTypeBtn(p.apvts, "preampType", "TYPE"),
      preampPadBtn(p.apvts, "preampPad", "PAD"),
      preampBypassBtn(p.apvts, "preampBypass", Theme::preampCol),
      preampCircuitToggle(p.apvts, "preampCircuit", "CIRCUIT", Theme::preampCol)
{
    setSize(420, 280);

    preampTypeCircuitPair.setChildren(preampTypeBtn, preampCircuitToggle);

    preampPanel.addItem(preampPadBtn, 84);
    preampPanel.addItem(preampGainDial, 78);
    preampPanel.addItem(preampTypeCircuitPair, 84);
    preampPanel.setBypassButton(preampBypassBtn);
    preampPanel.setAccent(Theme::preampCol, "PREAMP CHARACTER");
    addAndMakeVisible(preampPanel);

    outputPanel.addItem(outputGainDial, 78);
    outputPanel.setAccent(Theme::outputCol, "OUTPUT TRIM");
    addAndMakeVisible(outputPanel);

    preampGainAtt = std::make_unique<SliderAttachment>(p.apvts, "preampGain", preampGainDial);
    outputGainAtt = std::make_unique<SliderAttachment>(p.apvts, "outputGain", outputGainDial);
}

void OpenPreampEditor::paint(juce::Graphics& g)
{
    g.fillAll(Theme::editorTop);
}

void OpenPreampEditor::resized()
{
    auto b = getLocalBounds();
    preampPanel.setBounds(b.removeFromTop(150).reduced(8));
    outputPanel.setBounds(b.reduced(8));
}
