#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ValueFormat.h"
#include "Theme.h"
#include "SectionPanel.h"
#include <cmath>
#include <functional>
#include <GoodLookinUI.h>

enum class KnobValueType { Frequency, Gain, Q };

// A large rotary control styled like the reference console: dark graphite
// knurled body with the band's accent colour as a thin indicator ring, and
// the current value rendered directly inside the knob body. When the band is
// bypassed the knob turns neutral grey via setActive(false).
class RotaryKnob : public juce::Slider, private juce::Timer {
public:
    RotaryKnob(const juce::String& labelText, KnobValueType type, juce::Colour accent)
        : valueType(type), accentColour(accent)
    {
        setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                             juce::MathConstants<float>::pi * 2.8f, true);
        setPopupDisplayEnabled(false, false, nullptr);

        title.setText(labelText, juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centred);
        title.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        title.setColour(juce::Label::textColourId, Theme::textDark);
        title.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(title);
        design.label = labelText.toStdString();
        design.colour = "#" + accent.toDisplayString(false).toStdString();
        design.style = "console";
        setMouseDragSensitivity(180);
        setScrollWheelEnabled(false);
        startTimerHz(60);
    }

    void setActive(bool active)
    {
        if (isActive == active)
            return;
        isActive = active;
        repaint();
    }

    bool getActive() const { return isActive; }

    // Re-tint at runtime (colour triggers); keeps the saved design otherwise untouched.
    void setAccent(juce::Colour c)
    {
        accentColour = c;
        design.colour = "#" + c.toDisplayString(false).toStdString();
        repaint();
    }
    juce::Colour getAccent() const { return juce::Colour::fromString("ff" + juce::String(design.colour.substr(1))); }
    // Text colours of the section plate this knob sits on (nullptr args = follow the theme).
    void setPlateText(const juce::Colour* text, const juce::Colour* mid)
    {
        hasPlateText = text != nullptr;
        if (hasPlateText) { plateText = *text; plateMid = *mid; }
        refreshTheme();
    }
    void refreshTheme()
    {
        title.setColour(juce::Label::textColourId, hasPlateText ? plateText : Theme::textDark);
        repaint();
    }

    void resized() override
    {
        auto b = getLocalBounds();
        title.setBounds(b.removeFromTop(14));
        valueArea = b.removeFromBottom(18);
        knobArea = b.reduced(1, 0);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = knobArea.toFloat();
        auto diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
        auto knobBounds = juce::Rectangle<float>(diameter, diameter).withCentre(bounds.getCentre());

        juce::Colour backdrop;   // plate behind this knob: lets the toolkit draw dark marks on light plates and light marks on dark ones
        if (auto* panel = dynamic_cast<SectionPanel*>(getParentComponent())) backdrop = panel->getBackdrop();
        goodlookinui::juce_adapter::drawKnob(g, knobBounds, isShowing() ? motion.position : valueToProportionOfLength(getValue()), design, isActive, backdrop);

        if(getWidth()>=110) {
            g.setColour(hasPlateText?plateMid:Theme::textMid);g.setFont(juce::FontOptions(8.0f));
            const int y=juce::roundToInt(knobBounds.getBottom()-13);
            g.drawText(formatValue(float(getMinimum())),6,y,35,12,juce::Justification::centred);
            g.drawText(formatValue(float(getMaximum())),getWidth()-41,y,35,12,juce::Justification::centred);
        }
        // Exact values sit in a recessed readout beneath the physical control.
        auto readout=valueArea.toFloat().withSizeKeepingCentre(68.0f,15.0f);
        g.setColour(juce::Colour(0xff20292c));g.fillRoundedRectangle(readout,2);
        g.setColour(juce::Colour(0xff465255));g.drawRoundedRectangle(readout,2,0.6f);
        // Value readout below the knob
        g.setColour(isActive ? Theme::knobText : Theme::knobTextInactive);
        g.setFont(juce::FontOptions(design.fontSize, juce::Font::bold));
        g.drawText(formattedValue(), valueArea, juce::Justification::centred);
    }

    std::function<void()> onSelect;   // developer mode: clicking a knob selects it in the inspector
    void mouseDown(const juce::MouseEvent& e) override { if (onSelect) onSelect(); juce::Slider::mouseDown(e); }
    void applyDesign(const goodlookinui::Item& item) {
        design = item;
        title.setText(item.label, juce::dontSendNotification);
        title.setFont(juce::FontOptions(item.fontSize, juce::Font::bold));
        repaint();
    }
    const goodlookinui::Item& getDesign() const { return design; }
private:
    void timerCallback() override {
        const auto target = valueToProportionOfLength(getValue());
        if (!isShowing()) { motion.position=target; motion.velocity=0; return; }
        if (std::abs(motion.position-target)<0.0001 && std::abs(motion.velocity)<0.0001) return;
        motion.step(target,1.0/60.0);
        repaint();
    }
    goodlookinui::Item design;
    goodlookinui::Motion motion;
    juce::String formattedValue() const
    {
        return formatValue(static_cast<float>(getValue()));
    }
    juce::String formatValue(float v) const
    {
        switch (valueType) {
            case KnobValueType::Frequency: return ValueFormat::frequency(v);
            case KnobValueType::Gain:      return ValueFormat::gainDB(v);
            case KnobValueType::Q:         return ValueFormat::qFactor(v);
        }
        return {};
    }

    KnobValueType valueType;
    juce::Colour accentColour;
    bool isActive = true;
    bool hasPlateText = false;
    juce::Colour plateText, plateMid;
    juce::Label title;
    juce::Rectangle<int> knobArea;
    juce::Rectangle<int> valueArea;
};
