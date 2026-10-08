#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include <memory>
#include <GoodLookinUI.h>

// Small chrome on/off toggle (e.g. the preamp's CIRCUIT active-circuit
// model). Lights up in the accent colour when enabled; call setActive(false)
// to grey it out while its owning section is bypassed.
class SmallToggle : public juce::ToggleButton {
public:
    SmallToggle(juce::AudioProcessorValueTreeState& state, const juce::String& parameterID,
                const juce::String& labelText, juce::Colour accent)
        : label(labelText), accentColour(accent)
    {
        setButtonText(labelText);
        setClickingTogglesState(true);
        setTooltip("Active-circuit phase/magnitude model (coupling + transformer poles)");
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            state, parameterID, *this);
    }

    void setActive(bool active)
    {
        if (isActive == active)
            return;
        isActive = active;
        repaint();
    }

    void setAccent(juce::Colour colour) { accentColour = colour; repaint(); }

    bool getActive() const { return isActive; }

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto b = getLocalBounds().toFloat();
        float h = juce::jmin(b.getHeight(), 30.0f);
        auto r = juce::Rectangle<float>(b.getWidth() - 4.0f, h).withCentre(b.getCentre());

        juce::Colour top, bottom;
        if (getToggleState()) {
            juce::Colour base = isActive ? accentColour : Theme::buttonInactiveTop;
            top = base.brighter(0.18f);
            bottom = base.darker(0.22f);
        } else {
            top = isActive ? Theme::buttonTop : Theme::buttonInactiveTop;
            bottom = isActive ? Theme::buttonBottom : Theme::buttonInactiveBottom;
        }
        if (shouldDrawButtonAsDown) {
            top = top.darker(0.06f);
            bottom = bottom.darker(0.06f);
        }

        // Pick black or white text based on the actual fill brightness so it
        // stays legible against light accent colours too (e.g. cream/tan).
        juce::Colour midCol = top.interpolatedWith(bottom, 0.5f);
        juce::Colour textCol = midCol.getPerceivedBrightness() > 0.55f ? juce::Colours::black
                                                                        : juce::Colours::white;

        juce::ColourGradient fill(top, 0.0f, r.getY(), bottom, 0.0f, r.getBottom(), false);
        goodlookinui::juce_adapter::drawKey(g,r,top,shouldDrawButtonAsDown);

        g.setColour(Theme::buttonOutline.withAlpha(0.6f));
        g.drawRoundedRectangle(r, 4.0f, 1.0f);

        if (shouldDrawButtonAsHighlighted && !getToggleState()) {
            g.setColour(juce::Colour(0x12FFFFFF));
            g.fillRoundedRectangle(r, 4.0f);
        }

        g.setColour(textCol);
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText(label, r, juce::Justification::centred);
    }

private:
    juce::String label;
    juce::Colour accentColour;
    bool isActive = true;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};
