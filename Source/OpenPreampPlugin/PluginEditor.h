#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "OpenPreampPlugin/PluginProcessor.h"
#include "UI/RotaryKnob.h"
#include "UI/CycleButton.h"
#include "UI/BypassButton.h"
#include "UI/SmallToggle.h"
#include "UI/SectionPanel.h"
#include "UI/VerticalPair.h"
#include "UI/Theme.h"
#include "Toolkit.h"
#include "UI/ValueFormat.h"
#include "UI/MeterPanel.h"
#include <memory>

class OpenPreampEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit OpenPreampEditor(OpenPreampProcessor&);
    ~OpenPreampEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

#if GOODLOOKINUI_ENABLE_EDITOR
    juce::TextButton developerButton{"DEV"};
    goodlookinui::juce_adapter::Studio designStudio;
    std::unique_ptr<LookStudio> lookStudio;
    DesignAutosave designAutosave;
    std::unique_ptr<juce::DocumentWindow> developerWindow;
    void setupDeveloperTools();
#endif
    bool layoutReady = false;
    void loadKnobLayout(const std::string&);
    juce::Component canvas;
    OpenPreampProcessor& proc;

    SectionPanel preampPanel{"PREAMP"};
    SectionPanel outputPanel{"OUTPUT"};

    CycleButton preampTypeBtn;
    CycleButton preampPadBtn;
    MeterPanel meterPanel;
    CycleButton meterSourceBtn;
    SmallToggle hqToggle;
    LookTable modelLooks;
    Theme::Look editorLook = Theme::current();
    int lastModel = -1;
    void applyModelLook();
    juce::TooltipWindow tooltips{this, 700};
    juce::String rateText;
    juce::Label rateLabel;
    void timerCallback() override;
    RotaryKnob preampGainDial{"GAIN", KnobValueType::Gain, Theme::preampCol};
    BypassButton preampBypassBtn;
    SmallToggle preampCircuitToggle;
    VerticalPair preampTypeCircuitPair;

    RotaryKnob outputGainDial{"OUTPUT", KnobValueType::Gain, Theme::outputCol};

    std::unique_ptr<SliderAttachment> preampGainAtt;
    std::unique_ptr<SliderAttachment> outputGainAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenPreampEditor)
};
