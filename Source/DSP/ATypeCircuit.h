#pragma once

#include "CircuitSolver.h"

// A-Type: transformer-coupled 500-series mic preamp, simulated from a
// transcribed netlist (kept privately, not distributed). Signal path: RF
// filter and phantom feed -> 1:8 mic transformer -> discrete op-amp gain stage
// with a T feedback network (gain pot P1, ground leg switched MID/HIGH) ->
// coupling network -> 1:2 output transformer -> output pad -> 600 ohm load.
//
// Provenance
//   [NETLIST]     topology and values (MIC input, phantom off, phase normal,
//                 output pad at maximum). The input pad (LOW position) is
//                 not modelled: the engine's own pad control does that job.
//   [PLACEHOLDER] the discrete op-amp (a behavioural single-pole, slew-
//                 limited, clipped model until its schematic is available)
//                 and both transformers' inductance/resistance (the netlist
//                 states they are placeholders)
//   [ESTIMATE]    transformer saturation knees, full-scale calibration
//
// Gain: the reference (0 dB drive) is the MID position with the gain pot at
// its low end. Raising gain first turns the pot up, then moves to the HIGH
// position (ground leg 90.9 ohm) and continues with the pot.
class ATypeCircuit {
public:
    static constexpr double kRail = 16.0;        // [NETLIST]
    static constexpr double kMaxGain = 100.0;    // about 40 dB above the reference
    // Transformer saturation (all estimates; core knee in volt-seconds / L).
    static constexpr double kMicLSat = 0.1;      // of 2 H
    static constexpr double kMicISat = 0.0024;   // ~4.8 mV*s of flux
    static constexpr double kOutLSat = 0.1;      // of 1 H
    static constexpr double kOutISat = 0.08;     // ~80 mV*s of flux

    bool prepare(double sampleRate)
    {
        build();
        const bool ok = solver.prepare(sampleRate);
        calibrate();
        return ok;
    }

    void restoreDc(double sampleRate)
    {
        solver.restoreDc(sampleRate);
        lastGain = 0.0;
    }

    void restoreDc() noexcept
    {
        solver.restoreDc();
        lastGain = 0.0;
    }

    // Balanced source EMF (differential) at 0 dBFS.
    void setFullScaleVolts(double v) noexcept { vFull = v; }

    // Linear gain relative to the reference (>= 1), from the stage-gain
    // formula G = Vw + RA*((Vw - 1)/R13 + Vw/(RB + R15)), Vw = 1 + R13/Rg.
    void setGain(double g) noexcept
    {
        if (std::abs(g - lastGain) <= 1e-4 * lastGain) return;
        lastGain = g;
        const double target = 2.0 * std::max(g, 1.0);   // reference stage gain is 2 (6 dB)
        const bool high = target > stageGain(false, kPot) * 0.999;
        double lo = 0.0, hi = kPot;
        for (int i = 0; i < 40; ++i) {
            const double mid = 0.5 * (lo + hi);
            if (stageGain(high, mid) < target) lo = mid; else hi = mid;
        }
        const double ra = 0.5 * (lo + hi);
        solver.setR(rPa, ra + 1e-3);
        solver.setR(rPb, kPot - ra + 1e-3);
        solver.setR(rHigh, high ? 1e-2 : 1e9);   // R11 (100 ohm) is already in series
    }

    double process(double x) noexcept
    {
        const double e = 0.5 * x * vFull;
        solver.setFixed(nSp, e);
        solver.setFixed(nSn, -e);
        solver.step();
        return (solver.voltage(nOutP) - solver.voltage(nOutN)) * outScale / vFull;
    }

    CircuitSolver& core() noexcept { return solver; }
    double outScale = 1.0;
    double vFull = 1.0;

private:
    static constexpr double kPot = 10e3;

    static double stageGain(bool high, double ra) noexcept
    {
        const double rg = high ? 1.0 / (1.0 / 1e3 + 1.0 / 100.0) : 1e3;
        const double vw = 1.0 + 1e3 / rg;
        return vw + ra * ((vw - 1.0) / 1e3 + vw / ((kPot - ra) + 1e3));
    }

    void build()
    {
        solver = CircuitSolver();
        auto& s = solver;
        nSp = s.addFixedNode(0.0);
        nSn = s.addFixedNode(0.0);

        // test-bench source: 150 ohm balanced mic (75 + 75)
        const int INP = s.addNode(), INN = s.addNode();
        s.addR(nSp, INP, 75.0);
        s.addR(nSn, INN, 75.0);

        // RF filter and phantom feed (off)
        const int nC3 = s.addNode(), NPH = s.addNode();
        s.addC(INP, nC3, 470e-12);
        s.addC(INN, nC3, 470e-12);
        s.addC(nC3, 0, 100e-12);
        s.addR(INP, NPH, 6.8e3);
        s.addR(INN, NPH, 6.8e3);
        s.addC(NPH, 0, 100e-6);
        s.addR(NPH, 0, 150.0);

        // T1 1:8, primaries in series (floating), secondary to ground
        const int nT1P = s.addNode(), nT1S = s.addNode(), TP1 = s.addNode();
        s.addR(INP, nT1P, 30.0);
        s.addL(nT1P, INN, 2.0, 2.0 * kMicLSat, kMicISat);
        s.addIdeal(nT1P, INN, nT1S, 0, 8.0);
        s.addR(TP1, nT1S, 1000.0);
        const int nZ = s.addNode();
        s.addC(TP1, nZ, 22e-12);
        s.addR(nZ, 0, 10e3);
        s.addR(TP1, 0, 100e3);

        // A1 gain stage
        const int N = s.addNode(), A = s.addNode(), W = s.addNode(), nP1 = s.addNode(), G = s.addNode(), nR11 = s.addNode();
        s.addOpamp(TP1, N, A, 20e6, 15.0, 3e5, 14.0, 5.0, 1e6);
        s.addC(A, N, 100e-12);
        rPa = s.addR(A, W, kPot / 2, true);
        rPb = s.addR(W, nP1, kPot / 2, true);
        s.addR(N, W, 1e3);
        s.addR(nP1, G, 1e3);
        s.addR(N, G, 1e3);
        s.addR(N, nR11, 100.0);
        rHigh = s.addR(nR11, G, 1e9, true);
        s.addC(G, 0, 1000e-6);
        s.addC(G, 0, 470e-9);

        // output coupling and T2 (1:2, primaries parallel, secondaries series)
        const int nL = s.addNode(), B = s.addNode(), nT2P = s.addNode(), nT2S = s.addNode();
        nOutP = s.addNode();   // GRN
        nOutN = s.addNode();   // GRY
        s.addC(A, nL, 1000e-6);
        s.addR(A, nL, 39.0);
        s.addL(nL, B, 6e-6);
        s.addR(B, 0, 10e3);
        s.addR(B, nT2P, 5.0);
        s.addL(nT2P, 0, 1.0, 1.0 * kOutLSat, kOutISat);
        s.addIdeal(nT2P, 0, nT2S, nOutN, 2.0);
        s.addR(nOutP, nT2S, 20.0);
        s.addR(nOutP, nOutN, 2.2e3);
        s.addR(nOutP, 0, 1e6);
        s.addR(nOutN, 0, 1e6);

        // output pad at maximum output (wipers at the transformer end): shunt branch only
        const int nPa = s.addNode(), nPb = s.addNode();
        s.addR(nOutP, nPa, 1e3);
        s.addR(nPa, nPb, 220.0);
        s.addR(nPb, nOutN, 1e3);

        // load
        s.addR(nOutP, nOutN, 600.0);
    }

    // Unity at the reference gain at 1 kHz.
    void calibrate()
    {
        const double sr = solver.sampleRate();
        outScale = 1.0;
        vFull = 1.0;
        lastGain = 0.0;
        setGain(1.0);
        const double amp = 1e-3, w = 2.0 * 3.14159265358979 * 1000.0 / sr;
        const int n = static_cast<int>(sr * 0.25);
        double pk = 0.0;
        for (int i = 0; i < n; ++i) {
            const double e = 0.5 * amp * std::sin(w * i);
            solver.setFixed(nSp, e);
            solver.setFixed(nSn, -e);
            solver.step();
            if (i > n / 2)
                pk = std::max(pk, std::abs(solver.voltage(nOutP) - solver.voltage(nOutN)));
        }
        outScale = amp / pk;
        restoreDc();
    }

    CircuitSolver solver;
    int nSp = 0, nSn = 0, nOutP = 0, nOutN = 0, rPa = 0, rPb = 0, rHigh = 0;
    double lastGain = 0.0;
};
