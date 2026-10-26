#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Toolkit.h"
#include "Theme.h"
#include "../DSP/Metering.h"
#include "PreampChrome.h"
#include <GoodLookinUI.h>
#include <Meters.h>
#include <Retro.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>

// Level section: needle meter (or digital face), input / output LED ladders,
// a 7-segment peak readout and a latching clip lamp. Levels come from the processor's
// lock-free taps. OpenPreamp detector ballistics run at audio rate; legacy
// HybridEQ meters retain their UI ballistics.
class MeterPanel : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    enum class Face { VuCream, VuBlack, Arc, Fan, Wedge, Mountains, Lcd, ArcGauge, Columns, NeonBars, HexRing, Scope, Chassis, FlatBars, Ppm, StereoLadder };
    struct FaceInfo { Face face; const char* code; const char* name; bool retro; };
    static constexpr FaceInfo faces[] = {
        // Professional hardware
        {Face::VuCream, "cream", "VU needle, cream face", false}, {Face::VuBlack, "black", "VU needle, black amber face", false},
        {Face::Ppm, "ppm", "PPM needle (1-7 scale)", false}, {Face::StereoLadder, "ladder", "Stereo LED ladders", false},
        {Face::FlatBars, "flat", "Flat bars (clean, no glow)", false},
        {Face::Arc, "arc", "Segmented arc (digital)", false}, {Face::Fan, "fan", "Vector fan (phosphor)", false},
        // Retro / 80s / cyberpunk
        {Face::Wedge, "wedge", "80s dash: bar-graph wedge", true}, {Face::Mountains, "mountains", "80s dash: green vector mountains (L/R)", true},
        {Face::Lcd, "lcd", "80s dash: orange LCD panel", true}, {Face::ArcGauge, "gauge", "80s dash: arc gauge with scale", true},
        {Face::Columns, "columns", "80s dash: colour bar columns (L/R)", true}, {Face::NeonBars, "neonbars", "Cyberpunk: neon bars", true},
        {Face::HexRing, "hexring", "Cyberpunk: hex ring", true}, {Face::Scope, "scope", "Cyberpunk: glitch scope", true},
        {Face::Chassis, "chassis", "Cyberpunk: yellow chassis", true}};
    template <typename Processor>
    explicit MeterPanel(Processor& p) : MeterPanel(p.getInputLevel(), p.getOutputLevel()) {}
    MeterPanel(LevelTracker& input, LevelTracker& output) : inputLevel(input), outputLevel(output) {
        setTooltip("Output level. Click the CLIP lamp to reset it.");
        startTimerHz(30);
    }
    void setInputSource(bool input) {
        if (inputSource == input) return;
        inputSource = input;
        hold = -99.0f; holdTimer = 0.0f; clip = false;
        rmsReadout = std::numeric_limits<float>::quiet_NaN();
        vu.position = 0.0; vu.velocity = 0.0;
        repaint();
    }
    bool isInputSource() const { return inputSource; }
    void setPeakMode(bool peak) {
        if (selectableMode && peakMode == peak) return;
        selectableMode = true; peakMode = peak; vu.position = 0.0; vu.velocity = 0.0; repaint();
    }
    bool isPeakMode() const { return peakMode; }
    void setMeasurement(int mode, float db, bool aes, int weighting) {
        const bool changed = mode != measurementMode || aes != measurementAES || weighting != measurementWeight;
        if (openpreamp::isRMS(mode)) {
            // A 0.02 dB margin around the rounding boundary keeps the last digit
            // steady without changing detector ballistics or masking real level changes.
            if (changed || db <= -90.f || !std::isfinite(rmsReadout) || std::abs(db-rmsReadout) > .07001f)
                rmsReadout = std::round(db*10.f)/10.f;
        } else rmsReadout = std::numeric_limits<float>::quiet_NaN();
        measurementMode=mode; measurementDb=db; measurementAES=aes; measurementWeight=weighting;
    }
    float numericReadoutDb() const { return openpreamp::isRMS(measurementMode) && std::isfinite(rmsReadout) ? rmsReadout : displayedDb(); }
    float displayedDb() const { return measurementMode >= 0 ? measurementDb : selectableMode && !peakMode ? rmsDb : hold; }
    void setFace(Face f) { face = f; repaint(); }
    Face getFace() const { return face; }
    static const char* faceCode(Face f) { for (auto& i : faces) if (i.face == f) return i.code; return "cream"; }
    static Face faceFromCode(const juce::String& c) { for (auto& i : faces) if (c == i.code) return i.face; return Face::VuCream; }

    void setPlate(juce::Colour top, juce::Colour bottom, juce::Colour text, juce::Colour mid, int finish = 0)
    { plate[0]=top; plate[1]=bottom; plate[2]=text; plate[3]=mid; plateFinish=finish; hasPlate=true; repaint(); }
    void clearPlate() { if (hasPlate) { hasPlate=false; repaint(); } }
    bool getPlate(juce::Colour out[4], int* finish = nullptr) const
    {
        if (!hasPlate) return false;
        if (finish) *finish = plateFinish;
        for (int i=0; i<4; ++i) out[i] = plate[i];
        return true;
    }
    void setMeterFaceOverridden(bool overridden) { if (meterFaceOverridden != overridden) { meterFaceOverridden = overridden; repaint(); } }

    void setChannel(int channel) { channelIndex = std::clamp(channel,-1,1); }
    int getChannel() const { return channelIndex; }
    void setHardwareStyle(bool enabled) { hardware=enabled; repaint(); }
    void setCaption(const juce::String& text) { if (caption != text) { caption = text; repaint(); } }
    void setExternallyDriven() { stopTimer(); }

    // --- values the painter shows (exposed so the tests can read them) ---
    float needle() const { return float(vu.position); }
    float ladderDb(int bus, int ch) const { return ladder[bus][ch]; }
    float holdDb() const { return hold; }
    float faceLevel() const { return meters_level; }
    float faceLevel(int ch) const { return ch ? levelR : levelL; }
    float time() const { return clock; }
    bool clipped() const { return clip; }
    void resetClip() { clip = false; repaint(); }
    // Advance the ballistics by one UI tick using a reading (also used by the timer).
    void step(const LevelTracker::Reading& input, const LevelTracker::Reading& output, float dt) {
        auto in = input, out = output;
        if (channelIndex >= 0) for (int ch=0;ch<2;++ch) {
            in.peak[ch] = input.peak[channelIndex]; in.meanSquare[ch] = input.meanSquare[channelIndex];
            out.peak[ch] = output.peak[channelIndex]; out.meanSquare[ch] = output.meanSquare[channelIndex];
        }
        const auto& selected = inputSource ? in : out;
        for (int ch = 0; ch < 2; ++ch) {
            fall(ladder[0][ch], toDb(in.peak[ch]), dt);
            fall(ladder[1][ch], toDb(out.peak[ch]), dt);
            peakNow = std::max(peakNow, toDb(selected.peak[ch]));
            if (selected.peak[ch] >= 0.999f) clip = true;
        }
        const float outDb = std::max(toDb(selected.peak[0]), toDb(selected.peak[1]));
        if (outDb >= hold) { hold = outDb; holdTimer = 1.5f; }
        else if ((holdTimer -= dt) <= 0.0f) hold = std::max(-99.0f, hold - 12.0f * dt);
        const float power = 0.5f * (selected.meanSquare[0] + selected.meanSquare[1]);
        const float amp = std::sqrt(std::max(power, 0.0f));
        vuTarget = goodlookinui::juce_adapter::meters::vuPosition((peakMode ? std::max(selected.peak[0],selected.peak[1]) : amp) / refAmp);
        if (measurementMode >= 0) {
            // Detector ballistics already run at sample rate. The UI maps only the scale.
            const float relative = measurementDb - float(openpreamp::reference(measurementMode));
            vuTarget = goodlookinui::juce_adapter::meters::vuPositionForDb(relative);
            vu.position = vuTarget;
        } else vu.step(vuTarget, double(dt), 14.0);
        peakNow = -99.0f;
        clock += dt;
        namespace meters = goodlookinui::juce_adapter::meters;
        const int source = inputSource ? 0 : 1;
        levelL = meters::dbPosition(ladder[source][0]); levelR = meters::dbPosition(ladder[source][1]);
        meters_level = std::max(levelL, levelR);
        if(measurementMode>=0) {
            // Digital faces with dBFS legends retain their absolute scale.
            levelL=levelR=meters_level=meters::dbPosition(measurementDb);
        }
        rmsDb = amp > 1.0e-5f ? 20.0f * std::log10(amp) : -99.0f;
        // The scope renderer interpolates across size-1 intervals. Seed a flat
        // first segment so its first frame never divides by zero.
        if (history.empty()) history.push_back(std::max(levelL, levelR));
        history.push_back(std::max(levelL, levelR)); if (history.size() > 90) history.erase(history.begin());
    }

    void paint(juce::Graphics& g) override {
        using namespace goodlookinui::juce_adapter;
        auto b = getLocalBounds().toFloat().reduced(1);
        if(hasPlate) goodlookinui::juce_adapter::drawPanel(g,b,plate[0],plate[1],plateFinish);
        else if(hardware) PreampChrome::panel(g,b);
        else drawPanel(g, b, Theme::panelTop, Theme::panelBottom, Theme::panelFinish);
        g.setColour((hardware?PreampChrome::accent:Theme::faceAccent).withAlpha(0.85f)); g.fillRect(10.0f, 10.0f, 3.0f, 15.0f);
        g.setColour(hasPlate?plate[2]:(hardware?PreampChrome::ink:Theme::textDark)); g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.drawText(caption, 18, 8, 100, 20, juce::Justification::centredLeft);
        g.setColour(hasPlate?plate[3]:(hardware?PreampChrome::muted:Theme::textMid)); g.setFont(juce::FontOptions(9.0f));
        g.drawText(juce::String(inputSource ? "INPUT " : "OUTPUT ") + (measurementMode >= 0 ? measurementName()+" | "+measurementReferenceText() : (selectableMode ? (peakMode ? "PEAK" : "RMS") : "RMS / PEAK")), 18, 27, 190, 14, juce::Justification::centredLeft);

        const auto main = mainRect();
        const float w = float(getWidth());
        if (measurementMode >= 0 && (face==Face::VuCream || face==Face::VuBlack || face==Face::Ppm)) drawMeasurement(g, main);
        else switch (face) {
            case Face::VuCream: if(hardware && !meterFaceOverridden) PreampChrome::vu(g,main,float(vu.position)); else meters::drawVuMeter(g, main, float(vu.position), meters::VuFace::Cream); break;
            case Face::VuBlack: meters::drawVuMeter(g, main, float(vu.position), meters::VuFace::Black); break;
            case Face::Arc: {
                g.setColour(Colour(0xff08090a)); g.fillRoundedRectangle(main, 4);
                meters::drawSegmentArc(g, main.reduced(3), float(vu.position), Colour(0xffffb02e));
                break; }
            case Face::Fan: meters::drawVectorFan(g, main, float(vu.position), Colour(0xff39ff7a)); break;
            case Face::Wedge: retro::drawBarWedge(g, main, meters_level, Colour(0xffd6ff2e), clock); break;
            case Face::Mountains: retro::drawMountains(g, main, levelL, levelR, Colour(0xff39ff7a), clock); break;
            case Face::Lcd: retro::drawLcdPanel(g, main, (measurementMode >= 0 ? numericReadoutDb() : rmsDb) < -90.0f ? "--" : juce::String(measurementMode >= 0 ? numericReadoutDb() : rmsDb, 1), measurementMode >= 0 ? measurementName() : juce::String("RMS dBFS"), hold < -90.0f ? "--" : juce::String(hold, 1), "PEAK dBFS", Colour(0xffff9a1f), clock); break;
            case Face::ArcGauge: retro::drawArcGauge(g, main, meters_level, Colour(0xffffa31a), clock); break;
            case Face::Columns: retro::drawColourColumns(g, main, levelL, levelR, meters::dbPosition(hold), meters::dbPosition(hold), clock); break;
            case Face::NeonBars: retro::drawNeonBars(g, main, meters_level, meters::dbPosition(hold), Colour(0xff39f2ff), Colour(0xffff4fd8), clock); break;
            case Face::HexRing: retro::drawHexRing(g, main, meters_level, Colour(0xff39f2ff), Colour(0xffff4fd8), clock); break;
            case Face::Scope: retro::drawScope(g, main, history, clip, Colour(0xff39ff9a), clock); break;
            case Face::Ppm: meters::drawPpm(g, main, meters_level); break;
            case Face::StereoLadder: meters::drawStereoLadder(g, main, ladder[measurementMode >= 0 ? sourceIndex() : 1][0], ladder[measurementMode >= 0 ? sourceIndex() : 1][1], hold); break;
            case Face::FlatBars: retro::drawFlatBars(g, main, levelL, levelR, meters::dbPosition(hold), meters::dbPosition(hold), Colour(0xff3ccf5a)); break;
            case Face::Chassis: retro::drawChassis(g, main, meters_level, Colour(0xfff2c81e), Colour(0xff39f2ff), clock); break;
        }

        // IN / OUT ladders (right column, under the peak readout)
        g.setColour(hasPlate?plate[3]:(hardware?PreampChrome::muted:Theme::textMid)); g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
        const float rx = rightColumnX(), rw = w - 12.0f - rx;
        const float bx = rx + 22.0f, bw = rw - 22.0f;
        g.drawText("IN", int(rx), 86, 22, 12, juce::Justification::centredLeft);
        g.drawText("OUT", int(rx), 103, 24, 12, juce::Justification::centredLeft);
        for (int ch = 0; ch < (channelIndex >= 0 ? 1 : 2); ++ch) {
            meters::drawLedLadder(g, {bx, 87.0f + float(ch) * 6.5f, bw, 5.0f}, true, ladder[0][ch], ladder[0][ch], 18);
            meters::drawLedLadder(g, {bx, 104.0f + float(ch) * 6.5f, bw, 5.0f}, true, ladder[1][ch], hold, 18);
        }

        // Selected measurement readout; clip detection remains sample-peak based.
        const float reading = numericReadoutDb();
        juce::String text = reading <= -90.0f ? juce::String("--") : juce::String(reading, 1);
        meters::drawSevenSegment(g, {rx, 44.0f, rw - 30.0f, 26.0f}, text, Colour(0xff39ff7a));
        g.setColour(hasPlate?plate[3]:(hardware?PreampChrome::muted:Theme::textMid)); g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
        g.drawText(measurementMode >= 0 ? "dBFS" : selectableMode && !peakMode ? "RMS dBFS" : "PEAK dBFS", int(rx), 71, int(rw - 30.0f), 11, juce::Justification::centred);
        meters::drawLamp(g, clipLamp(), 5.5f, Colour(0xffff3b30), clip);
        g.drawText("CLIP", int(w) - 38, 63, 36, 11, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (e.position.getDistanceFrom(clipLamp()) < 14.0f) resetClip();
    }

private:
    int measurementMode=-1, measurementWeight=0;
    float measurementDb=-99;
    float rmsReadout=std::numeric_limits<float>::quiet_NaN();
    bool measurementAES=false;
    juce::String measurementName() const {
        return measurementMode==openpreamp::RMS && measurementAES ? "RMS +3" : juce::String(openpreamp::modeNames[measurementMode]);
    }
    juce::String measurementReferenceText() const {
        const juce::String mark=measurementMode==openpreamp::BBC?"4":measurementMode==openpreamp::Nordic?"+6":measurementMode==openpreamp::VU?"0 VU":"0";
        return mark+" = "+juce::String(openpreamp::reference(measurementMode),0)+" dBFS";
    }
    void drawMeasurement(juce::Graphics& g, juce::Rectangle<float> b) {
        // Keep each original needle skin's dimensions, bold lettering and stroke
        // weights. Measurement selection changes the scale, never the face style.
        const bool cream=face==Face::VuCream, ppm=face==Face::Ppm;
        const bool chrome=cream && hardware && !meterFaceOverridden;
        const auto ink=chrome?juce::Colour(0xff26251e):cream?juce::Colour(0xff1c1812):ppm?juce::Colour(0xff1b1d1c):juce::Colour(0xffffb347);
        const auto red=chrome?juce::Colour(0xffb8322a):cream||ppm?juce::Colour(0xffc0281c):juce::Colour(0xffff6a3a);
        g.setColour(juce::Colour(chrome?0xff02080b:0xff08090a));g.fillRoundedRectangle(b,4);
        if(!chrome){g.setColour(juce::Colour(0x35ffffff));g.drawHorizontalLine(int(b.getBottom()-1),b.getX()+2,b.getRight()-2);}
        auto f=b.reduced(chrome?3.f:3.5f);
        const auto top=chrome?juce::Colour(0xfffff2cb):cream?juce::Colour(0xfff8eece):ppm?juce::Colour(0xfff4f4f0):juce::Colour(0xff2d261a);
        const auto bottom=chrome?juce::Colour(0xffd9c58a):cream?juce::Colour(0xffd4c07e):ppm?juce::Colour(0xffcbcdc6):juce::Colour(0xff0a0907);
        g.setGradientFill(juce::ColourGradient(top,f.getX(),f.getY(),bottom,f.getX(),f.getBottom(),false));g.fillRoundedRectangle(f,chrome?2.f:2.5f);
        g.saveState();juce::Path clip;clip.addRoundedRectangle(f,chrome?2.f:2.5f);g.reduceClipRegion(clip);
        if(!chrome){
            g.setGradientFill(juce::ColourGradient(juce::Colour(0x55000000),0,f.getY(),juce::Colour(0x00000000),0,f.getY()+9,false));g.fillRect(f.withHeight(9));
        }
        const bool k=measurementMode>=openpreamp::K12 && measurementMode<=openpreamp::K20;
        const bool linear=measurementMode>=openpreamp::K12;
        const float ref=float(openpreamp::reference(measurementMode));
        const float low=k?-40.f:measurementMode==openpreamp::BBC||measurementMode==openpreamp::EBU?-12.f:measurementMode==openpreamp::DIN?-50.f:measurementMode==openpreamp::Nordic?-36.f:-20.f;
        const float high=k?-ref:measurementMode==openpreamp::BBC||measurementMode==openpreamp::EBU?12.f:measurementMode==openpreamp::DIN?5.f:measurementMode==openpreamp::Nordic?6.f:3.f;
        auto pos=[&](float v){return linear?std::clamp((v-low)/(high-low),0.f,1.f):goodlookinui::juce_adapter::meters::vuPositionForDb(v);};
        const float h=f.getHeight(),radius=h*1.55f,cx=f.getCentreX(),cy=f.getY()+radius+h*(chrome?.34f:.30f);
        const float span=std::min(.62f,f.getWidth()*.43f/radius);
        auto point=[&](float value,float rad){const float angle=-span+2*span*pos(value);return juce::Point<float>(cx+std::sin(angle)*rad,cy-std::cos(angle)*rad);};
        auto arc=[&](float start,float end,float width,juce::Colour colour){
            juce::Path path;path.addCentredArc(cx,cy,radius,radius,0,-span+2*span*start,-span+2*span*end,true);
            g.setColour(colour);g.strokePath(path,juce::PathStrokeType(width,juce::PathStrokeType::curved,juce::PathStrokeType::butt));
        };
        const float normalWidth=h*(chrome?.015f:.025f),redWidth=h*(chrome?.045f:.07f);
        if(ppm)arc(0,1,normalWidth,ink);
        else {arc(0,pos(0),normalWidth,ink);arc(pos(0),1,redWidth,red);}
        g.setFont(juce::FontOptions(h*(chrome?.095f:ppm?.19f:.15f),juce::Font::bold));
        auto tickAt=[&](float tick,bool major=true){
            g.setColour(tick>0?red:ink);
            const float length=h*(chrome?.09f:ppm?.12f:major?.11f:.07f);
            g.drawLine({point(tick,radius),point(tick,radius-length)},chrome?1.f:ppm?1.5f:major?1.4f:1.f);
            juce::String label(tick,0);
            if(measurementMode==openpreamp::BBC)label=juce::String(4+tick/4,0);
            else if(measurementMode==openpreamp::Nordic)label=juce::String(tick+6,0);
            else if(!linear && !chrome)label=juce::String(std::abs(tick),0);
            g.drawText(label,juce::Rectangle<float>(h*.4f,h*(chrome?.16f:ppm?.22f:.2f)).withCentre(point(tick,radius+h*(ppm?.14f:.12f))),juce::Justification::centred);
        };
        if(!linear){for(float tick:{-20.f,-10.f,-7.f,-5.f,-3.f,-2.f,-1.f,0.f,1.f,2.f,3.f})tickAt(tick,tick!=-2 && tick!=-1 && tick!=1 && tick!=2);}
        else {
            const float increment=k?10.f:measurementMode==openpreamp::BBC?4.f:measurementMode==openpreamp::DIN?10.f:6.f;
            for(float tick=low;tick<high;tick+=increment)if(!k || high-tick>=7.f)tickAt(tick);
            if(std::fmod(-low,increment)!=0)tickAt(0);
            tickAt(high);
        }
        const float value=measurementDb-ref;
        const auto start=point(value,radius*.45f),end=point(value,radius*1.01f);
        g.setColour(juce::Colour(chrome?0x50000000:0x40000000));g.drawLine({start.translated(chrome?1.f:1.8f,chrome?1.f:2.f),end.translated(chrome?1.f:1.8f,chrome?1.f:2.f)},chrome?2.f:1.8f);
        g.setColour(chrome?juce::Colour(0xff151c1e):cream?juce::Colour(0xff0e0c09):ppm?juce::Colour(0xff0d0d0b):juce::Colour(0xffff8e2b));g.drawLine({start,end},chrome?1.5f:1.6f);
        if(!chrome){
            juce::Path glass;glass.startNewSubPath(f.getX(),f.getY());glass.lineTo(f.getRight(),f.getY());glass.lineTo(f.getRight(),f.getY()+h*.36f);glass.quadraticTo(cx,f.getY()+h*.50f,f.getX(),f.getY()+h*.36f);glass.closeSubPath();
            g.setGradientFill(juce::ColourGradient(juce::Colour(0x28ffffff),cx,f.getY(),juce::Colour(0x00ffffff),cx,f.getY()+h*.5f,false));g.fillPath(glass);
        }
        // Compact measurement and reference labels replace the decorative VU/PPM
        // initials on every calibrated needle face, including VU and plain RMS.
        g.setColour(ink);g.setFont(juce::FontOptions(h*.095f,juce::Font::bold));
        const char* weights[]={"","A","B","C","D","K"};
        juce::String detail=measurementName();
        if(openpreamp::isRMS(measurementMode) && measurementWeight>0)detail+=" / "+juce::String(weights[measurementWeight]);
        g.drawText(detail,f.withTrimmedTop(h*.72f).withHeight(h*.12f),juce::Justification::centred);
        g.setFont(juce::FontOptions(h*.075f,juce::Font::bold));
        g.drawText(measurementReferenceText(),f.withTrimmedTop(h*.85f).withHeight(h*.1f),juce::Justification::centred);
        g.restoreState();g.setColour(chrome?PreampChrome::rim:juce::Colour(0xff040506));g.drawRoundedRectangle(b,4,chrome?.8f:1.5f);
    }
    LevelTracker& inputLevel;
    LevelTracker& outputLevel;
    Face face = Face::VuCream;
    bool hardware=false;
    juce::Colour plate[4];
    bool hasPlate=false;
    int plateFinish=0;
    bool meterFaceOverridden=false;
    bool inputSource = false, peakMode = false, selectableMode = false;
    int channelIndex = -1;
    juce::String caption{"LEVEL"};
    goodlookinui::Motion vu;
    float vuTarget = 0.0f, ladder[2][2] = {{-99, -99}, {-99, -99}}, hold = -99.0f, holdTimer = 0.0f, peakNow = -99.0f;
    bool clip = false;
    float lastSig = -1.0f;
    static constexpr float refAmp = 0.12589f;   // 0 VU = -18 dBFS
    using Colour = juce::Colour;
    float meters_level = 0.0f, levelL = 0.0f, levelR = 0.0f, rmsDb = -99.0f, clock = 0.0f;
    std::vector<float> history;

    // Compact layout for the 290 x 132 bottom-right slot: meter face on the left, readouts on the right.
    int sourceIndex() const { return inputSource ? 0 : 1; }
    float rightColumnX() const { return 12.0f + std::max(120.0f, float(getWidth()) - 24.0f - 116.0f) + 10.0f; }
    juce::Rectangle<float> mainRect() const { return {12.0f, 44.0f, std::max(120.0f, float(getWidth()) - 24.0f - 116.0f), std::max(80.0f, float(getHeight()) - 52.0f)}; }
    juce::Point<float> clipLamp() const { return {float(getWidth()) - 20.0f, 52.0f}; }
    static float toDb(float amp) { return amp <= 1.0e-5f ? -99.0f : 20.0f * std::log10(amp); }
    static void fall(float& bar, float db, float dt) { bar = db >= bar ? db : std::max(db, bar - 24.0f * dt); }

    void timerCallback() override {
        if (!isShowing()) return;
        step(inputLevel.read(), outputLevel.read(), 1.0f / 30.0f);
        const float sig = float(vu.position) * 100.0f + ladder[0][0] + ladder[0][1] + ladder[1][0] + ladder[1][1] + hold + (clip ? 1000.0f : 0.0f);
        if (std::abs(sig - lastSig) > 0.05f) { lastSig = sig; repaint(); }
    }
};
