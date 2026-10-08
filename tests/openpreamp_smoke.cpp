#include "OpenPreampPlugin/PluginEditor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <cstdio>
#include <cmath>
#include <EmbeddedOpenPreampDesign.h>

static int failures = 0, checks = 0;
static void check(bool ok, const char* name) {
    ++checks;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++failures;
}
static void set(OpenPreampProcessor& p, const char* id, float value) {
    auto* parameter = p.apvts.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
static bool contained(juce::Component& c) {
    for (auto* child : c.getChildren()) {
        if (child->isVisible() && (!c.getLocalBounds().contains(child->getBoundsInParent()) || !contained(*child))) return false;
    }
    return true;
}
#include "openpreamp_routing.h"
int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenPreampProcessor p;
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> audio(2, 128);
    check(p.apvts.getParameter("oversampleMode") == nullptr, "oversampling parameter removed");
    check(p.getParameters().size() == 14, "preamp-only parameter set; no EQ or harmonics controls");
    set(p, "preampCircuit", 0);
    p.setPlayConfigDetails(2, 2, 48000, 128);
    p.prepareToPlay(48000, 128);
    const int latency = p.getLatencySamples();
    check(latency > 0, "reports fixed latency for circuit oversampling");
    for (int pad = 0; pad < 3; ++pad) {
        set(p, "preampPad", float(pad));
        audio.clear(); p.processBlock(audio, midi);
        check(p.getProcessingSampleRate() == 48000.0, "PAD does not alter session rate");
    }
    set(p, "preampPad", 1);
    for (double rate : {44100.0, 48000.0, 96000.0, 192000.0}) {
        p.setPlayConfigDetails(2, 2, rate, 128);
        p.prepareToPlay(rate, 128);
        for (int type : {0, 1, 2, 4}) {
            set(p, "preampType", float(type));
            for (int hq = 0; hq < 2; ++hq) {
              set(p, "hqMode", float(hq));
              for (int circuit = 0; circuit < 2; ++circuit) {
                set(p, "preampCircuit", float(circuit));
                bool finite = true;
                for (int block = 0; block < 3; ++block) {
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 128; ++i)
                            audio.setSample(ch, i, 0.1f * std::sin(float(i + block * 128) * 0.1f));
                    p.processBlock(audio, midi);
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 128; ++i)
                            finite &= std::isfinite(audio.getSample(ch, i));
                }
                check(p.getProcessingSampleRate() == rate * (circuit ? (hq ? 4 : 2) : 1) && finite, "model/rate/circuit/HQ combination uses correct rate and produces finite audio");
                check(p.getLatencySamples() == latency, "latency stays fixed during automation");
              }
            }
        }
    }
    set(p, "preampBypass", 1); set(p, "outputGain", 6); set(p, "hqMode", 0);
    p.setPlayConfigDetails(2, 2, 96000, 128); p.prepareToPlay(96000, 128);
    audio.clear(); audio.setSample(0, 0, 0.25f); audio.setSample(1, 0, 0.25f);
    p.processBlock(audio, midi);
    check(std::abs(audio.getSample(0, latency) - 0.25f * juce::Decibels::decibelsToGain(6.0f)) < 1e-5f,
          "bypass impulse arrives at reported latency with output trim");
    check(std::abs(p.getInputLevel().read().peak[0] - 0.25f) < 1e-5f, "input meter receives dry audio");
    check(std::abs(p.getOutputLevel().read().peak[0] - 0.25f * juce::Decibels::decibelsToGain(6.0f)) < 1e-5f,
          "output meter receives trimmed audio");
    juce::MemoryBlock state; p.getStateInformation(state);
    OpenPreampProcessor restored; restored.setStateInformation(state.getData(), int(state.getSize()));
    bool stateMatches = true;
    for (auto* parameter : p.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            stateMatches &= std::abs(restored.apvts.getParameter(ranged->paramID)->getValue() - ranged->getValue()) < 1e-6f;
    check(stateMatches, "state round-trip preserves all parameters");
    set(p, "preampBypass", 0); set(p, "preampType", 1); set(p, "preampCircuit", 1); set(p, "outputGain", 0);
    audio.clear(); p.processBlock(audio, midi);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    check(editor->getWidth() * 74 == editor->getHeight() * 100 && editor->isResizable(), "editor is resizable with exact 100:74 aspect ratio");
    check(contained(*editor), "all GUI controls fit within their panels");
    for (auto* surface : editor->getChildren()) for (auto* child : surface->getChildren()) if (auto* meter = dynamic_cast<MeterPanel*>(child)) {
        LevelTracker::Reading in, out; in.peak[0] = 0.3f; in.peak[1] = 0.2f;
        out.peak[0] = 1.1f; out.peak[1] = 1.1f; out.meanSquare[0] = out.meanSquare[1] = 0.015f;
        for (int i = 0; i < 30; ++i) meter->step(in, out, 1.0f / 30);
        check(meter->clipped() && meter->holdDb() > 0 && meter->needle() > 0, "VU, peak hold, and clip indication respond");
        meter->resetClip(); check(!meter->clipped(), "clip lamp resets");
        meter->setInputSource(true);
        in.meanSquare[0] = in.meanSquare[1] = 0.0001f;
        for (int i = 0; i < 30; ++i) meter->step(in, out, 1.0f / 30);
        check(meter->isInputSource() && !meter->clipped() && meter->holdDb() < 0, "VU source changes to input including peak and clip indication");
        meter->setInputSource(false);
        out.peak[0] = 0.42f; out.peak[1] = 0.38f;
        for (int i = 0; i < 30; ++i) meter->step(in, out, 1.0f / 30);
    }
    for (const auto size : {juce::Point<int>(750,555), juce::Point<int>(1500,1110), juce::Point<int>(1000,740)}) {
        editor->setSize(size.x,size.y);
        check(contained(*editor), "scaled controls fit at small, large and default editor sizes");
    }
#if GOODLOOKINUI_ENABLE_EDITOR
    bool devOpened = false;
    for (auto* surface : editor->getChildren()) for (auto* child : surface->getChildren())
        if (auto* button = dynamic_cast<juce::TextButton*>(child); button && button->getButtonText() == "DEV") {
            button->onClick();
            auto& desktop = juce::Desktop::getInstance();
            for (int i=0;i<desktop.getNumComponents();++i)
                if (auto* window = dynamic_cast<juce::DocumentWindow*>(desktop.getComponent(i)); window && window->getName() == "OpenPreamp developer tools") {
                    devOpened = window->isVisible();
                    const auto devImage = window->getContentComponent()->createComponentSnapshot(window->getContentComponent()->getLocalBounds());
                    juce::FileOutputStream devFile(juce::File("/private/tmp/openpreamp-031-developer.png"));
                    devFile.setPosition(0); devFile.truncate(); juce::PNGImageFormat devPng; devPng.writeImageToStream(devImage, devFile);
                    window->closeButtonPressed();
                }
        }
    check(devOpened, "DEV opens the separate knob/layout and model-look window");
#endif
    const auto themeBefore = Theme::editorTop;
    LookTable expectedLooks; expectedLooks.fromCsv(openPreampModelLooks);
    for (int model = 0; model < 5; ++model) {
        set(p, "preampType", float(model));
        set(p, "meterSource", float(model % 2));
        audio.clear(); p.processBlock(audio, midi);
        std::unique_ptr<juce::AudioProcessorEditor> variant(p.createEditor());
        const auto name = dynamic_cast<juce::AudioParameterChoice*>(p.apvts.getParameter("preampType"))->getCurrentChoiceName();
        const auto* expected = expectedLooks.find("preampType", name.toStdString());
        bool matches = expected != nullptr; int knobs = 0;
        std::function<void(juce::Component&)> visit = [&](juce::Component& component) {
            if (auto* knob = dynamic_cast<RotaryKnob*>(&component)) {
                ++knobs; matches &= expected && knob->getDesign().style == expected->style && knob->getDesign().colour == expected->colour;
            }
            if (auto* meter = dynamic_cast<MeterPanel*>(&component))
                matches &= meter->isInputSource() == (model % 2 == 0);
            if (auto* panel = dynamic_cast<SectionPanel*>(&component)) {
                const auto* plate = expected ? goodlookinui::findFaceplate(expected->plate.empty() ? expected->faceplate : expected->plate) : nullptr;
                juce::Colour colours[4];
                matches &= plate && panel->getPlate(colours) && colours[0] == juce::Colour(plate->panelTop);
            }
            for (auto* child : component.getChildren()) visit(*child);
        };
        visit(*variant);
        check(matches && knobs == 6 && contained(*variant), "both knob and plate styles follow the saved model mapping; VU source restores");
        const auto image = variant->createComponentSnapshot(variant->getLocalBounds());
        juce::FileOutputStream file(juce::File("/private/tmp/openpreamp-031-model-" + juce::String(model) + ".png"));
        file.setPosition(0); file.truncate(); juce::PNGImageFormat png; png.writeImageToStream(image, file);
    }
    check(Theme::editorTop == themeBefore, "per-instance styles do not mutate the global theme");
    OpenPreampProcessor mono;
    auto layout = mono.getBusesLayout();
    layout.inputBuses.set(0, juce::AudioChannelSet::mono());
    layout.outputBuses.set(0, juce::AudioChannelSet::mono());
    check(mono.setBusesLayout(layout), "accepts matched mono buses");
    mono.prepareToPlay(48000, 128);
    juce::AudioBuffer<float> monoAudio(1, 128); monoAudio.clear();
    mono.processBlock(monoAudio, midi);
    check(std::isfinite(monoAudio.getSample(0, 127)), "mono audio processes safely");
    layout.outputBuses.set(0, juce::AudioChannelSet::stereo());
    check(!mono.isBusesLayoutSupported(layout), "rejects mismatched input/output buses");
    editor->setVisible(false); // Render the parameter position without waiting for UI animation.
    const auto image = editor->createComponentSnapshot(editor->getLocalBounds());
    juce::FileOutputStream output(juce::File("/private/tmp/openpreamp-031-preview.png"));
    output.setPosition(0); output.truncate();
    juce::PNGImageFormat png; check(png.writeImageToStream(image, output), "editor preview renders");
    int visibleMeters=0;
    for(auto* surface : editor->getChildren()) for(auto* child : surface->getChildren())
        if(auto* meter=dynamic_cast<MeterPanel*>(child); meter && meter->isVisible()) ++visibleMeters;
    check(visibleMeters==2, "both independent meters are visible by default");
    auto oldState=p.apvts.copyState();oldState.setProperty("expanded",false,nullptr);
    juce::MemoryBlock oldData;auto oldXml=oldState.createXml();juce::AudioProcessor::copyXmlToBinary(*oldXml,oldData);
    OpenPreampProcessor restoredDual;restoredDual.setStateInformation(oldData.getData(),int(oldData.getSize()));
    std::unique_ptr<juce::AudioProcessorEditor> dualEditor(restoredDual.createEditor());
    check(dualEditor->getWidth()==1000 && dualEditor->getHeight()==740 && contained(*dualEditor), "old folded state restores to the standard dual-channel layout");
    runRoutingChecks();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
