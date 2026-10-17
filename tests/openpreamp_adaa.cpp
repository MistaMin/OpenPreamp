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

static double measureGainDb(dsp::PreampType type, double rate, double freq, bool adaaEnabled) {
    constexpr int n = 4800, block = 128;
    dsp::PreampEngine preamp;
    preamp.prepare(rate); preamp.setType(type); preamp.setCircuitEnabled(false);
    preamp.setADAAEnabled(adaaEnabled); preamp.setDriveDB(0.0f);
    float data[block]; float* channels[] = {data};
    double sumIn = 0.0, sumOut = 0.0;
    const double w = 2.0 * juce::MathConstants<double>::pi * freq / rate;
    for (int offset = 0; offset < n; offset += block) {
        for (int i = 0; i < block; ++i) {
            const double s = 0.01 * std::sin(w * (offset + i));
            data[i] = float(s);
            if (offset >= 1024) sumIn += s * s;
        }
        preamp.processBlock(channels, 1, block);
        if (offset >= 1024) {
            for (int i = 0; i < block; ++i) sumOut += double(data[i]) * double(data[i]);
        }
    }
    return 10.0 * std::log10(sumOut / sumIn);
}

static double aliasRatio(dsp::PreampType type, double rate, bool enabled) {
    constexpr int n = 8192, k0 = 750, block = 128;
    dsp::PreampEngine preamp;
    preamp.prepare(rate); preamp.setType(type); preamp.setCircuitEnabled(false);
    preamp.setADAAEnabled(enabled); preamp.setDriveDB(0.0f);
    std::vector<float> fftData(size_t(2 * n), 0.0f);
    float data[block]; float* channels[] = {data};
    for (int offset = 0; offset < 3 * n; offset += block) {
        for (int i = 0; i < block; ++i)
            data[i] = float(0.7 * std::pow(10.0, 12.0 / 20.0) * std::sin(juce::MathConstants<double>::twoPi * k0 * ((offset + i) % n) / n));
        preamp.processBlock(channels, 1, block);
        if (offset >= 2 * n) std::copy(data, data + block, fftData.begin() + offset - 2 * n);
    }
    juce::dsp::FFT fft(13); fft.performFrequencyOnlyForwardTransform(fftData.data());
    double aliases = 0.0;
    for (int bin = 1; bin < n / 2; ++bin) {
        if (bin % k0 == 0) continue;
        aliases += double(fftData[size_t(bin)]) * fftData[size_t(bin)];
    }
    return aliases / (double(fftData[size_t(k0)]) * fftData[size_t(k0)]);
}

int main() {
    dsp::adaa::Residual stage;
    auto f = [](double x) { return std::tanh(x); };
    auto primitive = [](double x) { return dsp::adaa::logCosh(x); };

    // Test stability and bounds
    bool stable = true;
    for (double x : {0.8, 0.8000000000001, 0.0, -0.00000000001, -10.0, 10.0, 100.0, -100.0})
        stable &= std::isfinite(stage.step(x, f(x), primitive(x), 1.0, dsp::adaa::Mode::On));
    check(stable, "ADAA Residual is finite for repeated, near-equal, zero-crossing and large inputs");

    stage.reset();
    check(std::abs(stage.step(0.3, f(0.3), primitive(0.3), 1.0, dsp::adaa::Mode::Off) - f(0.3)) < 1e-12, "Mode::Off returns f without modification");

    // 1) Test high-end energy preservation for all preamp models
    std::printf("\n--- High-end energy test (ADAA ON vs OFF) ---\n");
    for (auto type : {dsp::PreampType::Brit, dsp::PreampType::N, dsp::PreampType::FSF, dsp::PreampType::AType}) {
        for (double freq : {5000.0, 10000.0, 15000.0, 20000.0}) {
            double gainOff = measureGainDb(type, 48000.0, freq, false);
            double gainOn = measureGainDb(type, 48000.0, freq, true);
            double diff = gainOn - gainOff;
            std::printf("Model %d at %5.0f Hz: ADAA On=%+.4f dB, Off=%+.4f dB, Diff=%+.4f dB\n",
                        int(type), freq, gainOn, gainOff, diff);
            check(std::abs(diff) < 0.05, "ADAA preserves high-end energy (difference < 0.05 dB)");
        }
    }

    // 2) Test alias reduction
    std::printf("\n--- Alias reduction test ---\n");
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
