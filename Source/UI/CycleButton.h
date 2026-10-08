#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include <memory>
#include <GoodLookinUI.h>

// Compact look for cycle buttons: small bold text, subtle chrome.
class CycleButtonLNF final : public juce::LookAndFeel_V4 {
public:
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour& base,bool over,bool down) override {
        goodlookinui::juce_adapter::drawKey(g,b.getLocalBounds().toFloat().reduced(2),over?base.brighter(0.06f):base,down);
    }
    juce::Font getTextButtonFont(juce::TextButton&, int) override
    {
        return juce::FontOptions(10.5f, juce::Font::bold);
    }
};

// Two-choice keys toggle directly; larger choice sets open a dropdown.
class CycleButton : public juce::Component, private juce::AudioProcessorValueTreeState::Listener {
public:
    CycleButton(juce::AudioProcessorValueTreeState& state, const juce::String& paramIDToUse,
                const juce::String& labelText, bool horizontalLayout = false)
        : apvts(state), paramID(paramIDToUse), isHorizontal(horizontalLayout)
    {
        param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(paramID));
        jassert(param != nullptr);

        title.setText(labelText, juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centredLeft);
        title.setFont(juce::FontOptions(isHorizontal ? 9.5f : 10.5f, juce::Font::bold));
        title.setColour(juce::Label::textColourId,
                        isHorizontal ? Theme::textMid : Theme::textDark);
        addAndMakeVisible(title);

        lnf = std::make_unique<CycleButtonLNF>();
        button.setLookAndFeel(lnf.get());
        button.setColour(juce::TextButton::buttonColourId, Theme::buttonTop);
        button.setColour(juce::TextButton::buttonOnColourId, Theme::buttonBottom);
        button.setColour(juce::TextButton::textColourOffId, Theme::buttonText);
        button.setColour(juce::TextButton::textColourOnId, Theme::buttonText);
        button.onClick = [this] {
            if (param && param->choices.size() == 2) {
                param->beginChangeGesture();
                param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1(float(1-param->getIndex())));
                param->endChangeGesture(); updateText();
            } else showChoices();
        };
        addAndMakeVisible(button);

        updateText();
        apvts.addParameterListener(paramID, this);
    }

    ~CycleButton() override
    {
        apvts.removeParameterListener(paramID, this);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        if (isHorizontal) {
            title.setBounds(b.removeFromLeft(42));
            button.setBounds(b.reduced(0, 1));
        } else if(getHeight()>44) {
            title.setJustificationType(juce::Justification::centred);
            title.setBounds(b.removeFromTop(14));
            button.setBounds(b.withHeight(28).withY(b.getY()+6).reduced(2,1));
        } else {
            title.setJustificationType(juce::Justification::centredLeft);
            title.setBounds(b.removeFromLeft(40));
            button.setBounds(b.reduced(1,3));
        }
    }

    // Grey-out styling used when the owning band is bypassed.
    void setActive(bool active)
    {
        if (isActive == active)
            return;
        isActive = active;
        refreshTheme();
    }

    void setPlateText(const juce::Colour* text, const juce::Colour* mid)
    {
        hasPlateText = text != nullptr;
        if (hasPlateText) { plateText = *text; plateMid = *mid; }
        refreshTheme();
    }

    void refreshTheme()
    {
        if (isActive) {
            button.setColour(juce::TextButton::buttonColourId, Theme::buttonTop);
            button.setColour(juce::TextButton::buttonOnColourId, Theme::buttonBottom);
            button.setColour(juce::TextButton::textColourOffId, Theme::buttonText);
            button.setColour(juce::TextButton::textColourOnId, Theme::buttonText);
        } else {
            button.setColour(juce::TextButton::buttonColourId, Theme::buttonInactiveTop);
            button.setColour(juce::TextButton::buttonOnColourId, Theme::buttonInactiveBottom);
            button.setColour(juce::TextButton::textColourOffId, Theme::buttonTextInactive);
            button.setColour(juce::TextButton::textColourOnId, Theme::buttonTextInactive);
        }
        title.setColour(juce::Label::textColourId, isActive
                             ? (hasPlateText ? (isHorizontal ? plateMid : plateText)
                                             : (isHorizontal ? Theme::textMid : Theme::textDark))
                             : Theme::buttonTextInactive);
        button.repaint();
    }

    bool getActive() const { return isActive; }

    void setButtonTooltip(const juce::String& tip) { button.setTooltip(tip); }

    // Overrides the text shown on the key without touching the parameter. Used for the Oversample key,
    // whose displayed factor is the engine's effective one (the user's choice raised by the headroom
    // floor), not the raw choice value. Pass an empty string to go back to the choice name.
    void setDisplayTextOverride(const juce::String& text)
    {
        if (textOverride == text)
            return;
        textOverride = text;
        updateText();
    }

private:
    void showChoices()
    {
        if(!param)return;
        juce::PopupMenu menu;
        for(int n=0;n<param->choices.size();++n)
            menu.addItem(n+1,param->choices[n],true,n==param->getIndex());
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&button),
            [safe=juce::Component::SafePointer<CycleButton>(this)](int result){
                if(!safe || result<=0)return;
                auto* parameter=safe->param;
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->getNormalisableRange().convertTo0to1(float(result-1)));
                parameter->endChangeGesture();safe->updateText();
            });
    }

    void parameterChanged(const juce::String&, float) override
    {
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<CycleButton>(this)] {
            if (safeThis != nullptr)
                safeThis->updateText();
        });
    }

    void updateText()
    {
        if (param == nullptr)
            return;
        button.setButtonText(textOverride.isNotEmpty() ? textOverride : param->getCurrentChoiceName());
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String paramID;
    juce::AudioParameterChoice* param = nullptr;
    juce::String textOverride;
    bool isActive = true;
    bool hasPlateText = false;
    juce::Colour plateText, plateMid;
    bool isHorizontal = false;
    std::unique_ptr<juce::LookAndFeel> lnf;
    juce::Label title;
    juce::TextButton button;
};
