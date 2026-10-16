#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "PreampChrome.h"
#include <memory>
#include <algorithm>
#include <functional>
#include <vector>
#include <GoodLookinUI.h>

// Compact look for cycle buttons: small bold text, subtle chrome.
class CycleButtonLNF final : public juce::LookAndFeel_V4 {
public:
    bool hardware=false;
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour& base,bool over,bool down) override {
        if(hardware) PreampChrome::key(g,b.getLocalBounds().toFloat().reduced(2),b.getComponentID().isEmpty() || b.getToggleState(),b.isEnabled(),over,down);
        else goodlookinui::juce_adapter::drawKey(g,b.getLocalBounds().toFloat().reduced(2),over?base.brighter(0.06f):base,down);
        if(hardware && b.hasKeyboardFocus(true)){g.setColour(PreampChrome::ink);g.drawRoundedRectangle(b.getLocalBounds().toFloat().reduced(1),3,1);}
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

        for (int i=0; param && i<param->choices.size(); ++i) menuOrder.push_back(i);
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
            if (param && menuOrder.size() == 2) {
                selectChoice(param->getIndex()==menuOrder[0] ? menuOrder[1] : menuOrder[0]);
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
        if(segmented) {
            if(showSegmentTitle) title.setBounds(b.removeFromLeft(title.getText()=="OS" ? 22 : 42));
            const int count=int(choiceKeys.size());
            for(int i=0;i<count;++i) choiceKeys[size_t(i)]->setBounds(b.getX()+i*b.getWidth()/count,b.getY(),(i+1)*b.getWidth()/count-i*b.getWidth()/count,b.getHeight());
            return;
        }
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

    // Presentation order is independent of stable host parameter indices.
    void setMenuOrder(std::vector<int> order) {
        for (auto index : order) {
            if (!param || index<0 || index>=param->choices.size()) { jassertfalse; return; }
        }
        menuOrder=std::move(order);
        if(segmented)setSegmented(true,showSegmentTitle);
    }
    // Direct choices still use the original parameter and host gestures.
    void setSegmented(bool enabled, bool showTitle=true) {
        segmented=enabled;showSegmentTitle=showTitle;button.setVisible(!enabled);
        title.setVisible(!enabled || showTitle);choiceKeys.clear();
        if(enabled) for(auto index : menuOrder) {
            auto key=std::make_unique<juce::TextButton>(param->choices[index]);
            key->setComponentID(paramID+"-"+juce::String(index));
            key->setLookAndFeel(lnf.get());key->setTooltip(button.getTooltip());
            key->onClick=[this,index]{selectChoice(index);};addAndMakeVisible(*key);choiceKeys.push_back(std::move(key));
        }
        updateText();refreshTheme();resized();
    }
    void setHardwareStyle(bool enabled) { lnf->hardware=enabled; refreshTheme(); }
    const std::vector<int>& getMenuOrder() const { return menuOrder; }
    bool selectChoice(int index) {
        if (!param || !isEnabled() || std::find(menuOrder.begin(),menuOrder.end(),index)==menuOrder.end()) return false;
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(float(index)));
        param->endChangeGesture();
        if(onChoiceSelected) onChoiceSelected(index);
        updateText();return true;
    }
    std::function<void(int)> onChoiceSelected;

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
        for(auto& key:choiceKeys) {
            key->setEnabled(isActive && isEnabled());
            key->setColour(juce::TextButton::buttonColourId,Theme::buttonTop);
            key->setColour(juce::TextButton::textColourOffId,PreampChrome::keyText(false,isActive));
            key->setColour(juce::TextButton::textColourOnId,PreampChrome::keyText(true,isActive));
            key->repaint();
        }
        button.repaint();
    }

    bool getActive() const { return isActive; }

    void setButtonTooltip(const juce::String& tip) { button.setTooltip(tip);for(auto& key:choiceKeys)key->setTooltip(tip); }

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
        for(auto index : menuOrder)
            menu.addItem(index+1,param->choices[index],true,index==param->getIndex());
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&button),
            [safe=juce::Component::SafePointer<CycleButton>(this)](int result){
                if(!safe || result<=0)return;
                safe->selectChoice(result-1);
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
        for(size_t i=0;i<choiceKeys.size();++i) {
            const auto index=menuOrder[i];
            const bool selected=param->getIndex()==index || (param->getIndex()==0 && paramID=="preampOversampling" && textOverride==param->choices[index]);
            choiceKeys[i]->setToggleState(selected,juce::dontSendNotification);
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String paramID;
    juce::AudioParameterChoice* param = nullptr;
    juce::String textOverride;
    std::vector<int> menuOrder;
    bool isActive = true;
    bool hasPlateText = false;
    juce::Colour plateText, plateMid;
    bool isHorizontal = false;
    bool segmented=false,showSegmentTitle=true;
    std::unique_ptr<CycleButtonLNF> lnf;
    juce::Label title;
    juce::TextButton button;
    std::vector<std::unique_ptr<juce::TextButton>> choiceKeys;
};
