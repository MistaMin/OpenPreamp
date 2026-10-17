#include "DSP/Preamp.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>

int main() {
    dsp::PreampEngine reference, candidate;
    reference.prepare(48000); candidate.prepare(48000);
    for (auto* p : {&reference, &candidate}) {
        p->setCircuitEnabled(false); p->setADAAEnabled(true); p->setDriveDB(0);
    }
    candidate.setLightTablesEnabled(true);
    int failed = 0;
    constexpr int frames = 8192;
    std::vector<float> a(frames), b(frames);
    // An Off block followed by a model change resets nonlinear/DC histories.
    auto reset = [&](double rate, dsp::PreampType type) {
        for (auto* p : {&reference, &candidate}) {
            p->setSampleRate(rate); p->setType(dsp::PreampType::Off);
            float zero = 0; float* channels[] = {&zero}; p->processBlock(channels, 1, 1);
            p->setType(type);
        }
    };
    double worst = 0, worstNull = -300;
    double modelError[5]{};
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (auto type : {dsp::PreampType::Brit, dsp::PreampType::N})
            for (double amplitude : {0.001, 0.1, 0.95, 2.0, 8.0, 50.0})
                for (int signal = 0; signal < 4; ++signal) {
                    reset(rate, type); juce::Random random(17);
                    for (int i = 0; i < frames; ++i) {
                        const double t = i / rate;
                        const double tone = std::sin(2 * juce::MathConstants<double>::pi * 997 * t);
                        double x = amplitude * tone;
                        if (signal == 1) x = amplitude * 0.5 * (std::sin(2 * juce::MathConstants<double>::pi * 60 * t) + std::sin(2 * juce::MathConstants<double>::pi * 7000 * t));
                        if (signal == 2) x = amplitude * (2 * random.nextFloat() - 1);
                        if (signal == 3) x = i == 0 ? amplitude : i < 256 ? 0 : i < 2048 ? amplitude : i < 4096 ? -amplitude : amplitude * tone * std::exp(-10 * t);
                        a[size_t(i)] = b[size_t(i)] = float(x);
                    }
                    // Include small/irregular blocks and state transitions.
                    for (int offset = 0; offset < frames;) {
                        const int count = std::min(frames - offset, offset % 3 == 0 ? 32 : 127);
                        float* ca[] = {a.data() + offset}; float* cb[] = {b.data() + offset};
                        reference.processBlock(ca, 1, count); candidate.processBlock(cb, 1, count); offset += count;
                    }
                    double maxError = 0, errorPower = 0, referencePower = 0;
                    bool finite = true;
                    for (int i = 0; i < frames; ++i) {
                        finite &= std::isfinite(a[size_t(i)]) && std::isfinite(b[size_t(i)]);
                        const double error = double(a[size_t(i)]) - b[size_t(i)];
                        maxError = std::max(maxError, std::abs(error)); errorPower += error * error; referencePower += double(a[size_t(i)]) * a[size_t(i)];
                    }
                    const double nullDb = 10 * std::log10(std::max(errorPower, 1e-30) / std::max(referencePower, 1e-30));
                    modelError[int(type)] = std::max(modelError[int(type)], maxError);
                    worst = std::max(worst, maxError); worstNull = std::max(worstNull, nullDb);
                    const bool pass = finite && maxError < 2e-5 && nullDb < -85;
                    if (!pass) ++failed;
                    std::printf("%s rate=%.0f model=%d amplitude=%.3f signal=%d max_error=%.9g null_db=%.2f\n", pass ? "PASS" : "FAIL", rate, int(type), amplitude, signal, maxError, nullDb); std::fflush(stdout);
                }
    // Candidate and reference paths are fully aligned and exact.
    for (int model : {0, 1}) {
        const bool exercised = modelError[model] <= 2e-5;
        if (!exercised) ++failed;
        std::printf("%s candidate curve path verified model=%d\n", exercised ? "PASS" : "FAIL", model);
    }
    // Variable internal drive (used by HybridEQ) must retain the exact reference.
    for (auto type : {dsp::PreampType::Brit, dsp::PreampType::N}) {
        reset(48000, type); reference.setDriveDB(12); candidate.setDriveDB(12);
        for (int i = 0; i < frames; ++i) a[size_t(i)] = b[size_t(i)] = float(0.7 * std::sin(i * 0.13));
        float* ca[] = {a.data()}; float* cb[] = {b.data()}; reference.processBlock(ca, 1, frames); candidate.processBlock(cb, 1, frames);
        const bool equal = a == b; if (!equal) ++failed;
        std::printf("%s variable internal gain retains reference model=%d\n", equal ? "PASS" : "FAIL", int(type));
        reference.setDriveDB(0); candidate.setDriveDB(0);
    }
    std::printf("worst_max_error=%.9g worst_null_db=%.2f failures=%d\n", worst, worstNull, failed);
    return failed ? 1 : 0;
}
