#include "DSP/Preamp.h"
#include <juce_dsp/juce_dsp.h>
#include <cstdio>
#include <vector>
#include <cmath>

static int failures = 0;
static void check(bool ok, const char* name) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++failures;
}
static double aliasRatio(dsp::PreampType type, double rate, bool enabled) {
    constexpr int n = 8192, tone = 1501, block = 128;
    dsp::PreampEngine preamp;
    preamp.prepare(rate); preamp.setType(type); preamp.setCircuitEnabled(false);
    preamp.setADAAEnabled(enabled); preamp.setDriveDB(12.0f);
    std::vector<float> fftData(size_t(2 * n), 0.0f);
    float data[block]; float* channels[] = {data};
    for (int offset = 0; offset < 3 * n; offset += block) {
        for (int i = 0; i < block; ++i)
            data[i] = float(0.7 * std::sin(juce::MathConstants<double>::twoPi * tone * ((offset + i) % n) / n));
        preamp.processBlock(channels, 1, block);
        if (offset >= 2 * n) std::copy(data, data + block, fftData.begin() + offset - 2 * n);
    }
    juce::dsp::FFT fft(13); fft.performFrequencyOnlyForwardTransform(fftData.data());
    double aliases = 0.0;
    for (int bin = 1; bin < n / 2; ++bin) {
        if (std::abs(bin - tone) <= 2 || std::abs(bin - 2 * tone) <= 2) continue;
        aliases += double(fftData[size_t(bin)]) * fftData[size_t(bin)];
    }
    return aliases / (double(fftData[tone]) * fftData[tone]);
}
int main() {
    dsp::AdaaStage analytic, numerical;
    auto f = [](double x) { return std::tanh(x); };
    auto primitive = [](double x) { return dsp::logCosh(x); };
    analytic.process(0.25, f, primitive);
    numerical.processNumerical(0.25, f);
    check(std::abs(analytic.process(0.8, f, primitive) - numerical.processNumerical(0.8, f)) < 1e-9,
          "numerical ADAA agrees with analytic tanh primitive");
    bool stable = true;
    for (double x : {0.8, 0.8000000000001, 0.0, -0.00000000001, -10.0, 10.0, 100.0, -100.0})
        stable &= std::isfinite(analytic.process(x, f, primitive));
    check(stable, "ADAA is finite for repeated, near-equal, zero-crossing and large inputs");
    analytic.reset(); check(std::abs(analytic.process(0.3, f, primitive) - f(0.3)) < 1e-12, "reset removes stale sample history");
    for (double rate : {44100.0, 48000.0})
        for (auto type : {dsp::PreampType::Brit, dsp::PreampType::N, dsp::PreampType::FSF, dsp::PreampType::AType}) {
            const double plain = aliasRatio(type, rate, false), aa = aliasRatio(type, rate, true);
            const double reduction = 10.0 * std::log10(plain / aa);
            std::printf("rate=%.0f model=%d alias reduction=%.2f dB (relative to fundamental)\n", rate, int(type), reduction);
            check(std::isfinite(reduction) && reduction > 1.0, "ADAA reduces actual preamp alias energy by more than 1 dB");
        }
    std::printf("%d failures\n", failures);
    return failures ? 1 : 0;
}
