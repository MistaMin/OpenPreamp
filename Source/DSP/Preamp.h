#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <array>
#include <vector>
#include <cmath>
#include "EQFilters.h"
#include "NTypeCircuit.h"
#include "BritCircuit.h"
#include "FsfCircuit.h"
#include "ATypeCircuit.h"

namespace dsp {

// AType is appended after Off so saved sessions keep their stored choice indices.
enum class PreampType { Brit = 0, N, FSF, Off, AType };

// Input-stage preamp emulation. Each flavour has two modes:
//   - "Preamp Circuit" ON: a transistor/op-amp-level simulation of the
//     transcribed netlist (see the *Circuit.h headers). Every
//     resistor, capacitor, transistor, op-amp and saturating transformer core
//     is solved together, per sample, by CircuitSolver (nodal analysis with
//     Newton iteration). Distortion, bandwidth and clipping come out of the
//     circuit itself. The simulation runs at about 96 kHz internally
//     (decimated from the oversampled rate with zero-latency IIR filters)
//     because it is CPU heavy. Values the manufacturers do not publish
//     (transformer inductance, saturation, leakage) are labelled estimates in
//     those headers.
//   - "Preamp Circuit" OFF: a light static waveshaper per flavour.
//
//   - Brit: balanced 500-series console mic preamp (BritCircuit.h): matched
//     transistor pair with op-amp current feedback, instrumentation pair,
//     difference amp, DC servos, balanced line driver. Gain is the dual-gang
//     pot. Even harmonics cancel, so the colour is odd-order.
//   - N:    transformer-coupled line path (NTypeCircuit.h): input transformer,
//     three-transistor feedback preamp, class-A line-driver stage, gapped
//     output transformer. Gain is the feedback network.
//   - A-Type: transformer-coupled 500-series mic preamp (ATypeCircuit.h): 1:8
//     mic transformer, discrete op-amp gain stage with a T feedback network,
//     1:2 output transformer. Its op-amp is a behavioural placeholder, so it
//     stays very clean until it clips.
//   - FSF:  transformer-coupled console mic channel (FsfCircuit.h): mic
//     transformer, 12-position gain network, two op-amp stages, class-AB
//     output stage inside a transformer feedback loop. Gain is stepped, with
//     a small trim between steps.
class PreampEngine {
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        slewCoef = 1.0f - static_cast<float>(std::exp(-1.0 / (sampleRate * 0.04)));
        updateEnvCoeffs(sampleRate);
        driveState = 1.0f;
        configuredType = static_cast<PreampType>(-1); // force (re)configure
        configuredOn = false;
        driveRamp.clear();
        // Build and solve the netlists once, off the audio thread's critical
        // path (allocates). Later sample-rate changes only re-init state.
        nCircOk = nCirc[0].prepare(kCircuitRate) && nCirc[1].prepare(kCircuitRate);
        bCircOk = bCirc[0].prepare(kCircuitRate) && bCirc[1].prepare(kCircuitRate);
        fCircOk = fCirc[0].prepare(kCircuitRate) && fCirc[1].prepareLike(fCirc[0], kCircuitRate);
        aCircOk = aCirc[0].prepare(kCircuitRate) && aCirc[1].prepare(kCircuitRate);
    }

    void setType(PreampType t) noexcept { type.store(t, std::memory_order_relaxed); }
    void setDriveDB(float db) noexcept { driveTargetDB.store(db, std::memory_order_relaxed); }
    void setBypassed(bool b) noexcept { bypassed.store(b, std::memory_order_relaxed); }
    void setCircuitEnabled(bool enabled) noexcept { circuitOn.store(enabled, std::memory_order_relaxed); }

    // By default the circuit runs at ~96 kHz decimated from the oversampled
    // host rate. For the standalone preamp plugin the circuit follows the
    // oversampled host rate directly, so decimation is disabled.
    void setDecimationEnabled(bool enabled) noexcept
    {
        if (decimationEnabled == enabled)
            return;
        decimationEnabled = enabled;
        configuredType = static_cast<PreampType>(-1); // force reconfigure
    }

    // Called every block with the current effective sample rate. The circuit
    // poles are computed from sr, so only force a reconfigure (which also
    // resets the circuit filters' state) when the rate has actually changed
    // - e.g. when the oversampling factor changes. Forcing it unconditionally
    // on every block would reset the filters' history constantly, which
    // shows up as a small gain/response discontinuity at every block
    // boundary and was a source of the level differing between oversampling
    // settings (more blocks per second at higher oversampling meant more
    // resets).
    void setSampleRate(double sampleRate) noexcept
    {
        if (std::abs(sampleRate - sr) < 1e-6)
            return;
        sr = sampleRate;
        slewCoef = 1.0f - static_cast<float>(std::exp(-1.0 / (sampleRate * 0.04)));
        updateEnvCoeffs(sampleRate);
        configuredType = static_cast<PreampType>(-1);
    }

    void process(juce::AudioBuffer<float>& buffer) noexcept
    {
        if (bypassed.load(std::memory_order_relaxed))
            return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        prepareDriveRamp(numSamples);
        const auto t = type.load(std::memory_order_relaxed);
        const bool on = circuitOn.load(std::memory_order_relaxed);
        ensureCircuitConfig(t, on);

        for (int ch = 0; ch < numChannels; ++ch) {
            auto* data = buffer.getWritePointer(ch);
            if (ch >= kMaxChannels) {
                for (int i = 0; i < numSamples; ++i)
                    data[i] = 0.0f;
                continue;
            }
            processChannel(data, numSamples, ch, t, on);
        }
    }

    // Channel-pointer variant for processing inside an oversampled
    // AudioBlock (the plugin's oversampled input stage).
    void processBlock(float* const* channelData, int numChannels, int numSamples) noexcept
    {
        if (bypassed.load(std::memory_order_relaxed))
            return;

        prepareDriveRamp(numSamples);
        const auto t = type.load(std::memory_order_relaxed);
        const bool on = circuitOn.load(std::memory_order_relaxed);
        ensureCircuitConfig(t, on);

        int numCh = std::min(numChannels, kMaxChannels);
        for (int ch = 0; ch < numCh; ++ch)
            processChannel(channelData[ch], numSamples, ch, t, on);
    }

private:
    static constexpr int kMaxChannels = 2;

    void prepareDriveRamp(int numSamples)
    {
        if (static_cast<int>(driveRamp.size()) < numSamples)
            driveRamp.resize(static_cast<size_t>(numSamples));

        const float target = static_cast<float>(
            std::pow(10.0, driveTargetDB.load(std::memory_order_relaxed) * 0.05));
        for (int i = 0; i < numSamples; ++i) {
            driveState += (target - driveState) * slewCoef;
            driveRamp[static_cast<size_t>(i)] = driveState;
        }
    }

    // Applies per-channel waveshape; the circuit network follows when on.
    void processChannel(float* data, int numSamples, int channel, PreampType t, bool circuit) noexcept
    {
        const float* ramp = driveRamp.data();

        if (circuit) {
            if (t == PreampType::N && nCircOk) { processCircuit(nCirc, data, numSamples, channel, kNFullScaleVolts); return; }
            if (t == PreampType::Brit && bCircOk) { processCircuit(bCirc, data, numSamples, channel, kBritFullScaleVolts); return; }
            if (t == PreampType::FSF && fCircOk) { processCircuit(fCirc, data, numSamples, channel, kFsfFullScaleVolts); return; }
            if (t == PreampType::AType && aCircOk) { processCircuit(aCirc, data, numSamples, channel, kAFullScaleVolts); return; }
        }

        if (t == PreampType::Off) {
            // Clean drive/trim only - no waveshaping, no added harmonics.
            // The circuit network (if enabled) still models the transformer
            // frequency response, just without any nonlinearity.
            if (circuit) {
                for (int i = 0; i < numSamples; ++i) {
                    const float s = data[i] * ramp[i];
                    data[i] = lpFilters[channel].process(hpFilters[channel].process(s));
                }
            } else {
                for (int i = 0; i < numSamples; ++i)
                    data[i] = data[i] * ramp[i];
            }
        } else if (t == PreampType::FSF) {
            // Light static model of a transformer-coupled input + output
            // stage: a soft, slightly asymmetric input curve (odd + a touch
            // of even) followed by a gentler output-transformer curve, so
            // harmonics are generated at both ends of the gain stage.
            constexpr float inW1 = 0.62f, inK1 = 1.15f;
            constexpr float inW2 = 0.38f, inK2 = 2.2f;
            constexpr float inNorm = inW1 * inK1 + inW2 * inK2;
            constexpr float inAsym = 0.06f;
            constexpr float outDrive = 1.35f;
            constexpr float outAsym = 0.08f;
            if (circuit) {
                for (int i = 0; i < numSamples; ++i) {
                    const float s = data[i] * ramp[i];
                    float in = (inW1 * std::tanh(inK1 * s) + inW2 * std::tanh(inK2 * s)) / inNorm
                               + inAsym * s * s;
                    float y = std::tanh(outDrive * in) / outDrive + outAsym * in * std::abs(in);
                    data[i] = lpFilters[channel].process(hpFilters[channel].process(y));
                }
            } else {
                for (int i = 0; i < numSamples; ++i) {
                    const float s = data[i] * ramp[i];
                    float in = (inW1 * std::tanh(inK1 * s) + inW2 * std::tanh(inK2 * s)) / inNorm
                               + inAsym * s * s;
                    data[i] = std::tanh(outDrive * in) / outDrive + outAsym * in * std::abs(in);
                }
            }
        } else if (t == PreampType::AType) {
            // Light static model: gentle transformer-style soft clip with a touch of even order.
            for (int i = 0; i < numSamples; ++i) {
                const double x = static_cast<double>(data[i]) * static_cast<double>(ramp[i]);
                data[i] = static_cast<float>(std::tanh(1.4 * x) / 1.4 + 0.03 * x * std::abs(x));
            }
        } else if (t == PreampType::N) {
            processN(data, numSamples, channel, circuit);
        } else {
            processBrit(data, numSamples, channel, circuit);
        }
    }

    // Brit: balanced degenerated-transistor input stage + op-amp stages.
    // See the class comment for the structure and what the constants mean.
    void processBrit(float* data, int numSamples, int channel, bool circuit) noexcept
    {
        constexpr double openGain = 20.0;  // input-stage gain with no degeneration
        constexpr double swing = 1.0;      // input-stage output swing, full-scale units
        constexpr double vScale = openGain / swing; // input voltage in units of kT/q
        constexpr double gainSplit = 0.7;  // share of the gain (in dB) taken by the input stage
        constexpr double legMismatch = 0.0015;
        constexpr double railClip = 1.2;   // op-amp output limit, full-scale units
        const size_t ch = static_cast<size_t>(channel);
        const float* ramp = driveRamp.data();

        for (int i = 0; i < numSamples; ++i) {
            const double g = static_cast<double>(ramp[i]);
            const double pad = g < 1.0 ? g : 1.0;      // attenuation ahead of the input stage
            const double gain = g < 1.0 ? 1.0 : g;     // amplification
            const double g1 = std::min(std::pow(gain, gainSplit), openGain);
            const double g2 = gain / g1;
            const double kappa = openGain / g1 - 1.0;  // degeneration (Re * gm)

            const double v = static_cast<double>(data[i]) * pad * vScale;
            const double legA = solveLeg(v, kappa);
            const double legB = solveLeg(-v, kappa);
            double o1 = railLimit(0.5 * swing * ((1.0 + legMismatch) * legA - legB - legMismatch), railClip);

            if (circuit) {
                brit1[ch] += britA1 * (o1 - brit1[ch]);
                o1 -= brit1[ch];
            }

            double o2 = railLimit(g2 * o1, railClip);

            if (circuit) {
                brit2[ch] += britA2 * (o2 - brit2[ch]);
                o2 -= brit2[ch];
                brit3[ch] += britA3 * (o2 - brit3[ch]);
                o2 -= brit3[ch];
            }

            data[i] = static_cast<float>(o2);
        }
    }

    // N, light static model (the netlist simulation is processCircuit).
    void processN(float* data, int numSamples, int channel, bool) noexcept
    {
        constexpr double vS = 20.0;          // input-stage junction voltage per full-scale unit (kT/q)
        constexpr double a0Gain = 300.0, kGain = 0.5;   // gain stage: open-loop gain, degeneration
        constexpr double a0Drv = 60.0, kDrv = 2.0;      // line driver: open-loop gain, degeneration
        constexpr double aeffGain = a0Gain / (1.0 + kGain);
        constexpr double aeffDrv = a0Drv / (1.0 + kDrv);
        constexpr double betaDrv = 1.0 - 1.0 / aeffDrv; // unity closed-loop gain
        const size_t ch = static_cast<size_t>(channel);
        const float* ramp = driveRamp.data();

        for (int i = 0; i < numSamples; ++i) {
            const double g = static_cast<double>(ramp[i]);
            const double pad = g < 1.0 ? g : 1.0;
            const double gain = std::min(g < 1.0 ? 1.0 : g, 0.9 * aeffGain);
            const double betaGain = 1.0 / gain - 1.0 / aeffGain;

            const double x = static_cast<double>(data[i]) * pad;

            double v = coreSaturate(x, 2.0);

            double y1 = solveLoop(v, betaGain, a0Gain, kGain, vS, nY1[ch]);
            y1 = asymLimit(y1, 1.5, 1.1);
            nDc1[ch] += dcA * (y1 - nDc1[ch]);
            y1 -= nDc1[ch];

            double y2 = solveLoop(y1, betaDrv, a0Drv, kDrv, vS, nY2[ch]);
            y2 = asymLimit(y2, 1.6, 1.3);
            nDc2[ch] += dcA * (y2 - nDc2[ch]);
            y2 -= nDc2[ch];

            data[i] = static_cast<float>(coreSaturate(y2, 2.5));
        }
    }

    // Full netlist simulation. Input -> (anti-alias, decimate) -> circuit ->
    // (zero-stuff, anti-image) -> output. Drive above 0 dB raises the
    // circuit's gain; any part the circuit cannot supply is applied as an
    // input trim.
    template <class C>
    void processCircuit(std::array<C, kMaxChannels>& circuits, float* data, int numSamples, int channel, double) noexcept
    {
        const size_t ch = static_cast<size_t>(channel);
        C& c = circuits[ch];
        const float* ramp = driveRamp.data();
        const int k = nDecim;
        const double kd = static_cast<double>(k);

        for (int i = 0; i < numSamples; ++i) {
            const double g = static_cast<double>(ramp[i]);
            const double x = static_cast<double>(data[i]) * (g < 1.0 ? g : 1.0);

            if (k == 1) {
                data[i] = static_cast<float>(stepCircuit(c, ch, x, g));
                continue;
            }

            const double xf = nAaDown[ch].process(x);
            double up = 0.0;
            if (nPhase[ch] == 0) {
                nHeld[ch] = stepCircuit(c, ch, xf, g);
                up = nHeld[ch] * kd;
            }
            if (++nPhase[ch] >= k) nPhase[ch] = 0;
            data[i] = static_cast<float>(nAaUp[ch].process(up));
        }
    }

    template <class C>
    double stepCircuit(C& c, size_t ch, double x, double g) noexcept
    {
        const double gain = g < 1.0 ? 1.0 : g;
        const double cg = std::min(gain, C::kMaxGain);
        if (std::abs(cg - nLastGain[ch]) > 1e-4 * nLastGain[ch]) {
            c.setGain(cg);
            nLastGain[ch] = cg;
        }
        double y = c.process(x * (gain / cg));
        if (!std::isfinite(y)) {
            c.restoreDc();
            nLastGain[ch] = 0.0;
            y = 0.0;
        }
        return y;
    }

    // Per-type setup when the circuit is (re)configured: internal rate, filters, state.
    template <class C>
    void configCircuits(std::array<C, kMaxChannels>& circuits, bool ok, double fullScale)
    {
        if (decimationEnabled) {
            nDecim = std::max(1, static_cast<int>(std::lround(sr / kCircuitRate)));
        } else {
            nDecim = 1;
        }
        const double internalRate = sr / static_cast<double>(nDecim);
        for (size_t ch = 0; ch < static_cast<size_t>(kMaxChannels); ++ch) {
            if (ok) {
                circuits[ch].restoreDc(internalRate);
                circuits[ch].setFullScaleVolts(fullScale);
            }
            nLastGain[ch] = 0.0;
            nPhase[ch] = 0;
            nHeld[ch] = 0.0;
            nAaDown[ch].design(sr, 0.4 * internalRate);
            nAaUp[ch].design(sr, 0.4 * internalRate);
        }
    }

    // Solves y = h(x - beta*y) for one transistor gain stage inside its
    // feedback loop, where h(v) = a0 * (I(v*vS) - 1) / vS and I is the
    // relative collector current from solveLeg. The loop function is
    // monotonic, so a few warm-started, step-limited Newton iterations
    // converge.
    static double solveLoop(double x, double beta, double a0, double kappa, double vS, double& y) noexcept
    {
        for (int n = 0; n < 5; ++n) {
            const double v = x - beta * y;
            const double cur = solveLeg(v * vS, kappa);
            const double h = a0 * (cur - 1.0) / vS;
            const double dh = a0 * cur / (1.0 + kappa * cur);
            double step = (y - h) / (1.0 + beta * dh);
            step = std::clamp(step, -2.0, 2.0);
            y -= step;
        }
        y = std::clamp(y, -8.0, 8.0);
        return y;
    }

    // Transformer core: unity small-signal slope, saturates smoothly.
    static double coreSaturate(double v, double sat) noexcept
    {
        const double r = v / sat;
        return v / std::sqrt(1.0 + r * r);
    }

    static double asymLimit(double v, double posRail, double negRail) noexcept
    {
        return railLimit(v, v >= 0.0 ? posRail : negRail);
    }

    // One transistor leg: solves w + kappa * (e^w - 1) = v for the log of the
    // relative collector current and returns e^w. v is the input in kT/q
    // units; kappa is the emitter degeneration (Re * gm). The start point is
    // chosen on the side of the root where the function is convex, so Newton
    // converges monotonically without overshooting into overflow.
    static double solveLeg(double v, double kappa) noexcept
    {
        double w = v >= 0.0 ? (kappa > 1e-9 ? std::min(v, std::log1p(v / kappa)) : v) : 0.0;
        for (int n = 0; n < 6; ++n) {
            if (w > 20.0)
                w = 20.0;
            const double e = std::exp(w);
            w -= (w + kappa * (e - 1.0) - v) / (1.0 + kappa * e);
        }
        return std::exp(std::min(w, 20.0));
    }

    // Op-amp output limit: unity slope below the rails, hard knee at them.
    static double railLimit(double v, double rail) noexcept
    {
        const double t = std::abs(v) / rail;
        const double t2 = t * t;
        const double t4 = t2 * t2;
        return v / std::pow(1.0 + t4 * t4, 0.125);
    }

    void ensureCircuitConfig(PreampType t, bool on)
    {
        if (on == configuredOn && t == configuredType)
            return;

        configuredOn = on;
        configuredType = t;

        // N carries its own state (static model and, with Circuit on, the netlist simulation).
        if (t == PreampType::N) {
            for (size_t ch = 0; ch < static_cast<size_t>(kMaxChannels); ++ch) {
                nY1[ch] = nY2[ch] = 0.0;
                nDc1[ch] = nDc2[ch] = 0.0;
            }
            configCircuits(nCirc, nCircOk, kNFullScaleVolts);
            return;
        }

        if (t == PreampType::AType) {
            configCircuits(aCirc, aCircOk, kAFullScaleVolts);
            return;
        }

        if (t == PreampType::Brit) {
            // Coupling/servo high-pass poles (1/(2*pi*R*C)): 2.2 uF into
            // 100 kOhm, 470 kOhm into 2 x 470 nF, and 100 uF into 10 kOhm.
            const auto coef = [this](double f) {
                return 1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * f / sr);
            };
            britA1 = coef(0.72);
            britA2 = coef(0.36);
            britA3 = coef(0.16);
            brit1.fill(0.0);
            brit2.fill(0.0);
            brit3.fill(0.0);
            configCircuits(bCirc, bCircOk, kBritFullScaleVolts);
            return;
        }

        double hpF = 22.0;
        double lpF = 22000.0;
        if (t == PreampType::FSF) { hpF = 15.0; lpF = 24000.0; }
        const auto hpCoeffs = FilterDesign::makeHighPass(sr, hpF, 0.7071);
        const auto lpCoeffs = FilterDesign::makeLowPass(sr, FilterDesign::safeFilterFreq(sr, lpF), 0.7071);

        for (int ch = 0; ch < kMaxChannels; ++ch) {
            hpFilters[static_cast<size_t>(ch)].setCoeffs(hpCoeffs);
            hpFilters[static_cast<size_t>(ch)].reset();
            lpFilters[static_cast<size_t>(ch)].setCoeffs(lpCoeffs);
            lpFilters[static_cast<size_t>(ch)].reset();
        }
        if (t == PreampType::FSF)
            configCircuits(fCirc, fCircOk, kFsfFullScaleVolts);
    }

    void updateEnvCoeffs(double sampleRate) noexcept
    {
        dcA = 1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * 1.5 / sampleRate);
    }

    double sr = 44100.0;
    float slewCoef = 0.001f;
    float driveState = 1.0f;
    std::vector<float> driveRamp;

    std::atomic<PreampType> type{PreampType::Brit};
    std::atomic<float> driveTargetDB{0.0f};
    std::atomic<bool> bypassed{false};
    std::atomic<bool> circuitOn{false};

    PreampType configuredType = PreampType::Brit;
    // Anti-alias / anti-image low-pass for the decimated circuit (8th-order
    // Butterworth as four biquads, minimum phase, so no latency to report).
    struct Lp8 {
        double b0[4]{}, b1[4]{}, b2[4]{}, a1[4]{}, a2[4]{}, z1[4]{}, z2[4]{};
        void design(double fs, double fc) noexcept
        {
            static constexpr double q[4] = {0.50979558, 0.60134489, 0.89997622, 2.56291545};
            const double w0 = 2.0 * juce::MathConstants<double>::pi * std::min(fc, 0.49 * fs) / fs;
            const double cw = std::cos(w0), sw = std::sin(w0);
            for (int i = 0; i < 4; ++i) {
                const double al = sw / (2.0 * q[i]);
                const double a0 = 1.0 + al;
                b0[i] = (1.0 - cw) * 0.5 / a0;
                b1[i] = (1.0 - cw) / a0;
                b2[i] = b0[i];
                a1[i] = -2.0 * cw / a0;
                a2[i] = (1.0 - al) / a0;
                z1[i] = z2[i] = 0.0;
            }
        }
        double process(double x) noexcept
        {
            for (int i = 0; i < 4; ++i) {
                const double y = b0[i] * x + z1[i];
                z1[i] = b1[i] * x - a1[i] * y + z2[i];
                z2[i] = b2[i] * x - a2[i] * y;
                x = y;
            }
            return x;
        }
    };

    // Internal rate of the netlist simulation and the line EMF at 0 dBFS
    // (0 dBFS at 0 dB drive just reaches the stage's clip point).
    static constexpr double kCircuitRate = 96000.0;
    static constexpr double kNFullScaleVolts = 2.0;      // N-Type line EMF at 0 dBFS
    static constexpr double kBritFullScaleVolts = 1.7;   // Brit balanced EMF at 0 dBFS
    static constexpr double kFsfFullScaleVolts = 0.8;    // FSF balanced EMF at 0 dBFS
    static constexpr double kAFullScaleVolts = 0.6;      // A-Type balanced EMF at 0 dBFS

    bool configuredOn = false;

    std::array<BiquadProcessor, kMaxChannels> hpFilters;
    std::array<BiquadProcessor, kMaxChannels> lpFilters;

    // N-type level-dependent circuit state: peak-follower + one-pole filter
    // state per channel for the input-transformer HP and output-transformer
    // LP poles (see processChannel's PreampType::N branch).
    std::array<double, kMaxChannels> nY1{}, nY2{}, nDc1{}, nDc2{};
    double dcA = 0.0;
    std::array<NTypeCircuit, kMaxChannels> nCirc;
    std::array<BritCircuit, kMaxChannels> bCirc;
    std::array<FsfCircuit, kMaxChannels> fCirc;
    std::array<ATypeCircuit, kMaxChannels> aCirc;
    std::array<Lp8, kMaxChannels> nAaDown, nAaUp;
    std::array<double, kMaxChannels> nHeld{}, nLastGain{};
    std::array<int, kMaxChannels> nPhase{};
    int nDecim = 1;
    bool decimationEnabled = true;
    bool nCircOk = false, bCircOk = false, fCircOk = false, aCircOk = false;

    // Brit coupling-network state (double: the poles are below 1 Hz).
    std::array<double, kMaxChannels> brit1{};
    std::array<double, kMaxChannels> brit2{};
    std::array<double, kMaxChannels> brit3{};
    double britA1 = 0.0;
    double britA2 = 0.0;
    double britA3 = 0.0;
};

} // namespace dsp
