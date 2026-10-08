// Per-configuration CPU table for the standalone OpenPreamp plugin.
// Measures the OpenPreampProcessor across sample rate x block size, with the
// circuit decimation disabled so the netlist runs at the session rate.
// PAD stays at unity for every measurement.
#include "OpenPreampPlugin/PluginProcessor.h"
#include <chrono>
#include <cstdio>
#include <vector>

namespace {

double measure(double sr, int bs, bool circuit, double seconds = 4.0)
{
    OpenPreampProcessor p;
    p.setPlayConfigDetails(2, 2, sr, bs);
    p.prepareToPlay(sr, bs);
    if (auto* prm = p.apvts.getParameter("preampCircuit"))
        prm->setValueNotifyingHost(circuit ? 1.0f : 0.0f);
    if (auto* prm = p.apvts.getParameter("preampType"))
        prm->setValueNotifyingHost(prm->convertTo0to1(1.0f));   // N-Type
    p.prepareToPlay(sr, bs);   // prepare at the session sample rate

    juce::AudioBuffer<float> buf(2, bs);
    juce::MidiBuffer midi;
    juce::Random rng(7);
    const int blocks = static_cast<int>(seconds * sr / bs);
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < blocks; ++i) {
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < bs; ++s)
                buf.setSample(c, s, 0.5f * (rng.nextFloat() * 2.0f - 1.0f));
        p.processBlock(buf, midi);
    }
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return 100.0 * wall / seconds;
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    const double rates[] = {44100.0, 48000.0, 96000.0, 192000.0};
    const int blocks[] = {64, 128, 512, 1024};

    std::printf("OpenPreamp CPU table (%% of one core, stereo, N-Type, no decimation)\n");
    std::printf("%-9s %-6s %-9s %-9s %s\n", "rate", "block", "circuit", "cpu%", "xRT");
    std::printf("%s\n", std::string(60, '-').c_str());

    for (double sr : rates)
        for (int bs : blocks)
            for (bool circuit : {false, true}) {
                const double cpu = measure(sr, bs, circuit);
                std::printf("%-9.0f %-6d %-9s %8.1f%% %6.1fx\n",
                            sr, bs, circuit ? "ON" : "off", cpu, 100.0 / cpu);
                std::fflush(stdout);
            }
    return 0;
}
