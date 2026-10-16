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
#include "UI/LookTriggers.h"
#include <GoodLookinUI.h>
#include <Gallery.h>
#include <goodlookinui/Settings.h>
#include <memory>

class OpenPreampEditor : public juce::AudioProcessorEditor,
                         private juce::Timer,
                         private juce::ValueTree::Listener {
public:
    explicit OpenPreampEditor(OpenPreampProcessor&);
    ~OpenPreampEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

#if GOODLOOKINUI_ENABLE_EDITOR
    juce::TextButton developerButton{"DEV"};
    juce::TextButton lookBtn{"LOOK"};
    juce::TextButton knobStyleBtn{"KNOBS: DEFAULT"};
    goodlookinui::juce_adapter::Studio designStudio;
    std::unique_ptr<LookStudio> lookStudio;
    std::unique_ptr<goodlookinui::juce_adapter::Gallery> gallery;
    DesignAutosave designAutosave;
    std::unique_ptr<juce::DocumentWindow> developerWindow;
    void setupDeveloperTools();
    void showLookMenu();
    void showKnobStyleMenu();
    void applyKnobStyle(const juce::String& code);
    void refreshKnobStyles();
    void applyMeterStyles();
    void loadDesignSettings();
    void saveSettingsFile();
    TriggerMode getTriggerMode() const;
    static const std::vector<juce::String>& settingKeys();
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;
    void onKnobDesignEdited(RotaryKnob& knob, const goodlookinui::Item& item);
    std::vector<std::pair<RotaryKnob*, std::string>> knobDefaults;
    bool initializingTools = false;
#endif
    juce::Component::SafePointer<RotaryKnob> selectedKnob;
    bool layoutReady = false;
    void loadKnobLayout(const std::string&);
    juce::Component canvas;
    OpenPreampProcessor& proc;

    SectionPanel preampPanel{"PREAMP"};
    SectionPanel rightPreampPanel{"RIGHT"};
    SectionPanel cutsPanel{"INPUT CUTS"}, controlsPanel{"PREAMP"}, monoMakerPanel{"MONO MAKER"};
    RotaryKnob rightGainDial{"GAIN", KnobValueType::Gain, Theme::preampCol};
    RotaryKnob rightOutputDial{"OUTPUT", KnobValueType::Gain, Theme::outputCol};
    RotaryKnob highPassDial{"HIGH PASS", KnobValueType::Frequency, Theme::preampCol};
    RotaryKnob lowPassDial{"LOW PASS", KnobValueType::Frequency, Theme::preampCol};
    MeterPanel rightMeter;
    CycleButton rightMeterSourceBtn;
    CycleButton meterModeBtn, rightMeterModeBtn;
    SmallToggle midSideToggle, cutsToggle, linkToggle, monoMakerToggle, gainCompensationToggle;
    RotaryKnob monoMakerDial{"FREQUENCY", KnobValueType::Frequency, Theme::preampCol};
    std::unique_ptr<SliderAttachment> monoMakerAtt;
    std::unique_ptr<SliderAttachment> rightGainAtt, rightOutputAtt, highPassAtt, lowPassAtt;

    CycleButton preampTypeBtn;
    CycleButton preampPadBtn;
    CycleButton circuitEngineBtn;
    MeterPanel meterPanel;
    CycleButton meterSourceBtn;
    CycleButton oversamplingBtn;
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

    RotaryKnob outputGainDial{"OUTPUT", KnobValueType::Gain, Theme::outputCol};

    std::unique_ptr<SliderAttachment> preampGainAtt;
    std::unique_ptr<SliderAttachment> outputGainAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenPreampEditor)
};
