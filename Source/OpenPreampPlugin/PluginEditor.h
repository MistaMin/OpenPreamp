#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "../UI/RotaryKnob.h"
#include "../UI/CycleButton.h"
#include "../UI/BypassButton.h"
#include "../UI/SmallToggle.h"
#include "../UI/SectionPanel.h"
#include "../UI/VerticalPair.h"
#include "../UI/Theme.h"
#include "../Toolkit.h"
#include "../UI/ValueFormat.h"
#include "../UI/MeterPanel.h"
#include <memory>

class OpenPreampEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit OpenPreampEditor(OpenPreampProcessor&);
    ~OpenPreampEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    OpenPreampProcessor& proc;

    SectionPanel preampPanel{"PREAMP"};
    SectionPanel outputPanel{"OUTPUT"};

    CycleButton preampTypeBtn;
    CycleButton preampPadBtn;
    MeterPanel meterPanel;
    juce::TooltipWindow tooltips{this, 700};
    juce::String rateText;
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
