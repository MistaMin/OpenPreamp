#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include <vector>
#include <GoodLookinUI.h>

// Console faceplate with fixed knob slots and aligned choice keys.
class SectionPanel : public juce::Component {
public:
    explicit SectionPanel(const juce::String& titleText) : title(titleText) {}

    void addItem(juce::Component& c, int preferredWidth)
    {
        items.push_back({&c, preferredWidth});
        addAndMakeVisible(c);
    }

    // Narrow vertical channel-strip layout: items are stacked top-to-bottom
    // (each item's "width" value is reused as its height in this mode),
    // matching a hardware EQ module rather than a wide horizontal row.
    void setVerticalLayout(bool v, int width = 132)
    {
        vLayout = v;
        vWidth = width;
    }

    // Reserve a tiny square slot (top-right) for a per-band bypass button.
    void setBypassButton(juce::Component& bypass)
    {
        bypassButton = &bypass;
        addAndMakeVisible(bypass);
    }

    int getPreferredHeight() const
    {
        if (!vLayout)
            return 116;
        int h = 40;
        for (auto& item : items)
            h += item.width + 5;
        return h;
    }

    void setTitle(const juce::String& text) { if (title != text) { title = text; repaint(); } }
    void setAccent(juce::Colour colour, const juce::String& range) { accent=colour; subtitle=range; }
    void setAccentColour(juce::Colour colour) { accent=colour; repaint(); }
    juce::Colour getAccentColour() const { return accent; }
    // Per-section plate: top, bottom, text, secondary text. Overrides the global theme.
    void setPlate(juce::Colour top, juce::Colour bottom, juce::Colour text, juce::Colour mid, int finish=0)
    { plate[0]=top; plate[1]=bottom; plate[2]=text; plate[3]=mid; plateFinish=finish; hasPlate=true; repaint(); }
    void clearPlate() { if(hasPlate) { hasPlate=false; repaint(); } }
    // Colour behind the controls (mean of the plate gradient), used by knobs to pick light or dark printed marks.
    juce::Colour getBackdrop() const
    {
        return hasPlate ? plate[0].interpolatedWith(plate[1], 0.5f) : Theme::panelTop.interpolatedWith(Theme::panelBottom, 0.5f);
    }
    bool getPlate(juce::Colour out[4], int* finish=nullptr) const
    {
        if(!hasPlate) return false;
        if(finish) *finish=plateFinish;
        for(int i=0;i<4;++i) out[i]=plate[i];
        return true;
    }
    void paint(juce::Graphics& g) override
    {
        auto b=getLocalBounds().toFloat().reduced(1);
        goodlookinui::juce_adapter::drawPanel(g,b,hasPlate?plate[0]:Theme::panelTop,hasPlate?plate[1]:Theme::panelBottom,hasPlate?plateFinish:Theme::panelFinish);
        g.setColour(accent.withAlpha(0.85f));g.fillRect(10.0f,10.0f,3.0f,15.0f);
        g.setColour(hasPlate?plate[2]:Theme::textDark);g.setFont(juce::FontOptions(11.5f,juce::Font::bold));
        g.drawText(title,18,8,getWidth()-58,20,juce::Justification::centredLeft);
        g.setColour(hasPlate?plate[3]:Theme::textMid);g.setFont(juce::FontOptions(9.0f));
        g.drawText(subtitle,18,27,getWidth()-36,14,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff20292c));g.drawHorizontalLine(46,10.0f,float(getWidth()-10));
        g.setColour(juce::Colour(0x16ffffff));g.drawHorizontalLine(47,10.0f,float(getWidth()-10));
        if(vLayout && items.size()==4) {
            g.setColour((hasPlate?plate[3]:Theme::textMid).withAlpha(0.6f));g.setFont(juce::FontOptions(9.0f));
            g.drawText(title.contains("CUT")?"FILTER / RESONANCE":"GAIN / FREQUENCY",12,285,getWidth()-24,18,juce::Justification::centred);
            g.setColour(accent.withAlpha(0.35f));g.drawHorizontalLine(314,48.0f,float(getWidth()-48));
        }
    }

    void resized() override
    {
        if(bypassButton) bypassButton->setBounds(getWidth()-36,10,24,20);
        if(vLayout) {
            int knob=0,button=0;
            for(auto& item:items) {
                if(item.width>=70) item.component->setBounds(12,52+knob++*100,getWidth()-24,98);
                else item.component->setBounds(22,354+button++*36,getWidth()-44,32);
            }
        } else {
            int content=0;for(auto& item:items)content+=item.width+12;content-=12;
            int x=(getWidth()-content)/2;
            for(auto& item:items){item.component->setBounds(x,50,item.width,getHeight()-56);x+=item.width+12;}
        }
    }

    int getPreferredWidth() const
    {
        if (vLayout)
            return vWidth + 16;
        int w = 16;
        for (auto& item : items)
            w += item.width + 6;
        return w;
    }

private:
    // Stack every item top-to-bottom, full width, using each item's stored
    // "width" value as its height (narrow hardware channel-strip look).
    void layoutVertical(juce::Rectangle<int> b)
    {
        int y = b.getY();
        for (auto& item : items) {
            item.component->setBounds(b.getX(), y, b.getWidth(), item.width);
            y += item.width + 5;
        }
    }

    struct Item { juce::Component* component; int width; };

    juce::String title, subtitle;
    juce::Colour accent=Theme::preampCol;
    juce::Colour plate[4];
    bool hasPlate=false;
    int plateFinish=0;
    juce::Component* bypassButton = nullptr;
    bool vLayout = false;
    int vWidth = 372;
    std::vector<Item> items;
};
