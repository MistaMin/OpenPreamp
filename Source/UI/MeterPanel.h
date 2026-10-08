#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Toolkit.h"
#include "Theme.h"
#include <GoodLookinUI.h>
#include <Meters.h>
#include <Retro.h>
#include <vector>
#include <algorithm>
#include <cmath>

// Level section: needle meter (or digital face), input / output LED ladders,
// a 7-segment peak readout and a latching clip lamp. Levels come from the processor's
// lock-free taps; all ballistics live here, in the UI.
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
        vu.position = 0.0; vu.velocity = 0.0;
        repaint();
    }
    bool isInputSource() const { return inputSource; }
    void setFace(Face f) { face = f; repaint(); }
    Face getFace() const { return face; }
    static const char* faceCode(Face f) { for (auto& i : faces) if (i.face == f) return i.code; return "cream"; }
    static Face faceFromCode(const juce::String& c) { for (auto& i : faces) if (c == i.code) return i.face; return Face::VuCream; }

    void setChannel(int channel) { channelIndex = std::clamp(channel,-1,1); }
    int getChannel() const { return channelIndex; }
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
        vuTarget = goodlookinui::juce_adapter::meters::vuPosition(amp / refAmp);
        vu.step(vuTarget, double(dt), 14.0);
        peakNow = -99.0f;
        clock += dt;
        namespace meters = goodlookinui::juce_adapter::meters;
        const int source = inputSource ? 0 : 1;
        levelL = meters::dbPosition(ladder[source][0]); levelR = meters::dbPosition(ladder[source][1]);
        meters_level = std::max(levelL, levelR);
        rmsDb = amp > 1.0e-5f ? 20.0f * std::log10(amp) : -99.0f;
        history.push_back(std::max(levelL, levelR)); if (history.size() > 90) history.erase(history.begin());
    }

    void paint(juce::Graphics& g) override {
        using namespace goodlookinui::juce_adapter;
        auto b = getLocalBounds().toFloat().reduced(1);
        drawPanel(g, b, Theme::panelTop, Theme::panelBottom, Theme::panelFinish);
        g.setColour(Theme::faceAccent.withAlpha(0.85f)); g.fillRect(10.0f, 10.0f, 3.0f, 15.0f);
        g.setColour(Theme::textDark); g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.drawText(caption, 18, 8, 100, 20, juce::Justification::centredLeft);
        g.setColour(Theme::textMid); g.setFont(juce::FontOptions(9.0f));
        g.drawText(inputSource ? "INPUT RMS / PEAK" : "OUTPUT RMS / PEAK", 18, 27, 140, 14, juce::Justification::centredLeft);

        const auto main = mainRect();
        const float w = float(getWidth());
        switch (face) {
            case Face::VuCream: meters::drawVuMeter(g, main, float(vu.position), meters::VuFace::Cream); break;
            case Face::VuBlack: meters::drawVuMeter(g, main, float(vu.position), meters::VuFace::Black); break;
            case Face::Arc: {
                g.setColour(Colour(0xff08090a)); g.fillRoundedRectangle(main, 4);
                meters::drawSegmentArc(g, main.reduced(3), float(vu.position), Colour(0xffffb02e));
                break; }
            case Face::Fan: meters::drawVectorFan(g, main, float(vu.position), Colour(0xff39ff7a)); break;
            case Face::Wedge: retro::drawBarWedge(g, main, meters_level, Colour(0xffd6ff2e), clock); break;
            case Face::Mountains: retro::drawMountains(g, main, levelL, levelR, Colour(0xff39ff7a), clock); break;
            case Face::Lcd: retro::drawLcdPanel(g, main, rmsDb < -90.0f ? "--" : juce::String(rmsDb, 1), "RMS dBFS", hold < -90.0f ? "--" : juce::String(hold, 1), "PEAK dBFS", Colour(0xffff9a1f), clock); break;
            case Face::ArcGauge: retro::drawArcGauge(g, main, meters_level, Colour(0xffffa31a), clock); break;
            case Face::Columns: retro::drawColourColumns(g, main, levelL, levelR, meters::dbPosition(hold), meters::dbPosition(hold), clock); break;
            case Face::NeonBars: retro::drawNeonBars(g, main, meters_level, meters::dbPosition(hold), Colour(0xff39f2ff), Colour(0xffff4fd8), clock); break;
            case Face::HexRing: retro::drawHexRing(g, main, meters_level, Colour(0xff39f2ff), Colour(0xffff4fd8), clock); break;
            case Face::Scope: retro::drawScope(g, main, history, clip, Colour(0xff39ff9a), clock); break;
            case Face::Ppm: meters::drawPpm(g, main, meters_level); break;
            case Face::StereoLadder: meters::drawStereoLadder(g, main, ladder[1][0], ladder[1][1], hold); break;
            case Face::FlatBars: retro::drawFlatBars(g, main, levelL, levelR, meters::dbPosition(hold), meters::dbPosition(hold), Colour(0xff3ccf5a)); break;
            case Face::Chassis: retro::drawChassis(g, main, meters_level, Colour(0xfff2c81e), Colour(0xff39f2ff), clock); break;
        }

        // IN / OUT ladders (right column, under the peak readout)
        g.setColour(Theme::textMid); g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
        const float rx = rightColumnX(), rw = w - 12.0f - rx;
        const float bx = rx + 22.0f, bw = rw - 22.0f;
        g.drawText("IN", int(rx), 86, 22, 12, juce::Justification::centredLeft);
        g.drawText("OUT", int(rx), 103, 24, 12, juce::Justification::centredLeft);
        for (int ch = 0; ch < (channelIndex >= 0 ? 1 : 2); ++ch) {
            meters::drawLedLadder(g, {bx, 87.0f + float(ch) * 6.5f, bw, 5.0f}, true, ladder[0][ch], ladder[0][ch], 18);
            meters::drawLedLadder(g, {bx, 104.0f + float(ch) * 6.5f, bw, 5.0f}, true, ladder[1][ch], hold, 18);
        }

        // peak readout and clip lamp
        juce::String text = hold <= -90.0f ? juce::String("--") : juce::String(hold, 1);
        meters::drawSevenSegment(g, {rx, 44.0f, rw - 30.0f, 26.0f}, text, Colour(0xff39ff7a));
        g.setColour(Theme::textMid); g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
        g.drawText("PEAK dBFS", int(rx), 71, int(rw - 30.0f), 11, juce::Justification::centred);
        meters::drawLamp(g, clipLamp(), 5.5f, Colour(0xffff3b30), clip);
        g.drawText("CLIP", int(w) - 38, 63, 36, 11, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (e.position.getDistanceFrom(clipLamp()) < 14.0f) resetClip();
    }

private:
    LevelTracker& inputLevel;
    LevelTracker& outputLevel;
    Face face = Face::VuCream;
    bool inputSource = false;
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
