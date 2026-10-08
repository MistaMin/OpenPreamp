#include "OpenPreampPlugin/PluginEditor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <cstdio>
#include <cmath>

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
        if (!c.getLocalBounds().contains(child->getBounds()) || !contained(*child)) return false;
    }
    return true;
}
int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenPreampProcessor p;
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> audio(2, 128);
    check(p.apvts.getParameter("oversampleMode") == nullptr, "oversampling parameter removed");
    check(p.getParameters().size() == 6, "preamp-only parameter set; no EQ or harmonics controls");
    set(p, "preampCircuit", 0);
    p.setPlayConfigDetails(2, 2, 48000, 128);
    p.prepareToPlay(48000, 128);
    const int latency = p.getLatencySamples();
    check(latency == 0, "native processing has zero latency");
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
                check(p.getProcessingSampleRate() == rate && finite, "model/rate/circuit combination produces finite audio at session rate");
                check(p.getLatencySamples() == latency, "latency stays fixed during automation");
            }
        }
    }
    set(p, "preampBypass", 1); set(p, "outputGain", 6);
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
    check(contained(*editor), "all GUI controls fit within their panels");
    for (auto* child : editor->getChildren()) if (auto* meter = dynamic_cast<MeterPanel*>(child)) {
        LevelTracker::Reading in, out; in.peak[0] = 0.3f; in.peak[1] = 0.2f;
        out.peak[0] = 1.1f; out.peak[1] = 0.8f; out.meanSquare[0] = out.meanSquare[1] = 0.015f;
        for (int i = 0; i < 30; ++i) meter->step(in, out, 1.0f / 30);
        check(meter->clipped() && meter->holdDb() > 0 && meter->needle() > 0, "VU, peak hold, and clip indication respond");
        meter->resetClip(); check(!meter->clipped(), "clip lamp resets");
    }
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
    juce::FileOutputStream output(juce::File("/private/tmp/openpreamp-preview.png"));
    output.setPosition(0); output.truncate();
    juce::PNGImageFormat png; check(png.writeImageToStream(image, output), "editor preview renders");
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
