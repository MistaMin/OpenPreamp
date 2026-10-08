#pragma once

#include "CircuitSolver.h"

// Brit: balanced 500-series console mic preamp, simulated from a transcribed
// netlist (kept privately, not distributed). Signal path: mic input with RF
// filters -> matched NPN pair with op-amp current feedback (stage 1) ->
// instrumentation pair (stage 2) -> difference amp (stage 3) -> DC servos ->
// balanced line driver. Gain is the dual-gang pot RV1 (stages 1 and 2 share
// it).
//
// Provenance
//   [NETLIST]   topology and values from the netlist (BOM values where the
//               schematic and BOM differ)
//   [DATASHEET] op-amp, input-pair and line-driver figures in the netlist
//   [MODEL]     op-amps are single-pole, slew-limited, output-clipped
//               behavioural models (as in the netlist): they capture gain,
//               bandwidth, slew rate and clipping, not noise. Smooth tanh
//               limits replace hard clamps so the solver converges.
//               Transistor base/emitter series resistance is not modelled.
//   [ESTIMATE]  full-scale calibration; the gain pot taper
class BritCircuit {
public:
    static constexpr double kRail = 15.0;        // [NETLIST]
    static constexpr double kRvMax = 2200.0;     // [NETLIST] gain pot, 0 = max gain
    static constexpr double kRvRef = 2200.0;     // minimum-gain setting is the 0 dB drive reference

    static constexpr double kMaxGain = 1000.0;  // about 60 dB above the reference

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

    // Line EMF (differential, source 150 ohm) at 0 dBFS.
    void setFullScaleVolts(double v) noexcept { vFull = v; }

    // Gain relative to the reference (minimum-gain) setting, 1 = reference.
    // Chooses the pot value that gives that ratio from the stage-gain formulas
    // G1 = 1 + 2000/(27+R), G2 = 1.02 + 2000/(39+R).
    void setGain(double g) noexcept
    {
        if (std::abs(g - lastGain) <= 1e-4 * lastGain) return;
        lastGain = g;
        const double target = gainAt(kRvRef) * std::max(g, 1.0);
        double lo = 0.0, hi = kRvMax;
        for (int i = 0; i < 40; ++i) {
            const double mid = 0.5 * (lo + hi);
            if (gainAt(mid) > target) lo = mid; else hi = mid;
        }
        const double r = 0.5 * (lo + hi) + 1e-3;
        solver.setR(rvA, r);
        solver.setR(rvB, r);
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
    int pBA = 0, pEA = 0, pO29 = 0, pO22 = 0, pO30 = 0, pO23 = 0, pO24 = 0, pP = 0, pN = 0, pIn = 0;   // probe nodes
    double outScale = 1.0;
    double vFull = 1.0;

private:
    static double gainAt(double r) noexcept { return (1.0 + 2000.0 / (27.0 + r)) * (1.02 + 2000.0 / (39.0 + r)); }

    using Npn = CircuitSolver::NpnModel;

    void build()
    {
        solver = CircuitSolver();
        auto& s = solver;
        const Npn mat02{1.2e-13, 605.0, 5.0, 200.0, 40e-12, 23e-12, 0.8e-9};

        const int VP = s.addFixedNode(kRail), VN = s.addFixedNode(-kRail);
        nSp = s.addFixedNode(0.0);
        nSn = s.addFixedNode(0.0);

        auto ne3 = [&](int p, int n, int o) { s.addOpamp(p, n, o, 48e6, 13.0, 1e5, 13.0, 10.0, 1e6); };
        auto ne10 = [&](int p, int n, int o) { s.addOpamp(p, n, o, 34e6, 13.0, 1e5, 13.0, 10.0, 1e6); };
        auto tl = [&](int p, int n, int o) { s.addOpamp(p, n, o, 5.25e6, 20.0, 2e5, 13.5, 125.0, 1e6); };

        // test-bench source: 150 ohm balanced mic (75 + 75)
        const int INP = s.addNode(), INN = s.addNode();
        s.addR(nSp, INP, 75.0);
        s.addR(nSn, INN, 75.0);

        // phantom feed (off) and PAD off (K3 shorts R17/R22: HL = INN, CL = INP)
        const int NPH = s.addNode();
        s.addR(INN, NPH, 6.8e3);
        s.addR(INP, NPH, 6.8e3);
        s.addC(NPH, 0, 100e-6);
        s.addR(NPH, 0, 1e3);

        // MIC input: coupling caps and RF filters
        const int nL2 = s.addNode(), nL1 = s.addNode(), nBA = s.addNode(), nBB = s.addNode();
        s.addC(INP, nL2, 2.2e-6);
        s.addL(nL2, nBA, 18e-6);
        s.addC(INN, nL1, 2.2e-6);
        s.addL(nL1, nBB, 18e-6);
        const int nZ1 = s.addNode(), nZ2 = s.addNode();
        s.addC(nBA, 0, 100e-12);
        s.addC(nBA, nZ1, 330e-12);
        s.addR(nZ1, 0, 300.0);
        s.addR(nBA, 0, 100e3);
        s.addC(nBB, 0, 100e-12);
        s.addC(nBB, nZ2, 330e-12);
        s.addR(nZ2, 0, 300.0);
        s.addR(nBB, 0, 100e3);

        // stage 1: input pair + current feedback
        const int VCCF = s.addNode(), nCA = s.addNode(), nCB = s.addNode(), nREF = s.addNode();
        const int nEA = s.addNode(), nEB = s.addNode(), nRVA = s.addNode();
        s.addR(VP, VCCF, 100.0);
        s.addC(VCCF, 0, 220e-6);
        s.addR(VCCF, nCA, 3.9e3);
        s.addR(VCCF, nCB, 3.9e3);
        s.addR(VCCF, nREF, 4.22e3);
        s.addR(nREF, 0, 10e3);
        s.addC(nREF, 0, 10e-6);
        s.addNpn(nCA, nBA, nEA, mat02);
        s.addNpn(nCB, nBB, nEB, mat02);
        s.addR(nEA, nRVA, 27.0);
        rvA = s.addR(nRVA, nEB, kRvMax, true);
        const int nZ3 = s.addNode(), nZ4 = s.addNode();
        s.addC(nCA, nZ3, 330e-12);
        s.addR(nZ3, 0, 200.0);
        s.addC(nCB, nZ4, 330e-12);
        s.addR(nZ4, 0, 200.0);
        const int nO29 = s.addNode(), nO22 = s.addNode(), nX1 = s.addNode(), nX2 = s.addNode();
        ne3(nREF, nCA, nO29);
        s.addR(nO29, nEA, 1e3);
        s.addR(nEA, nX1, 100.0);
        s.addC(nX1, nO29, 470e-12);
        ne3(nREF, nCB, nO22);
        s.addR(nO22, nEB, 1e3);
        s.addR(nEB, nX2, 100.0);
        s.addC(nX2, nO22, 470e-12);

        // stage 2: instrumentation pair
        const int TP3 = s.addNode(), TP4 = s.addNode(), nN30 = s.addNode(), nO30 = s.addNode();
        const int nRVB = s.addNode(), nN23 = s.addNode(), nO23 = s.addNode(), nSV1 = s.addNode();
        s.addC(nO29, TP3, 2.2e-6);
        s.addR(TP3, 0, 100e3);
        s.addC(nO22, TP4, 2.2e-6);
        s.addR(TP4, 0, 100e3);
        ne3(TP3, nN30, nO30);
        s.addR(nN30, nO30, 1e3);
        s.addC(nN30, nO30, 100e-12);
        s.addR(nN30, 0, 49.9e3);
        s.addR(nN30, nRVB, 39.0);
        rvB = s.addR(nRVB, nN23, kRvMax, true);
        ne3(TP4, nN23, nO23);
        s.addR(nN23, nO23, 1e3);
        s.addC(nN23, nO23, 100e-12);
        s.addR(nN23, nSV1, 49.9e3);

        pBA = nBA; pEA = nEA; pO29 = nO29; pO22 = nO22; pO30 = nO30; pO23 = nO23;

        // servo 1: nulls V(nO23) - V(nO30)
        const int nSP = s.addNode(), nSM = s.addNode();
        tl(nSP, nSM, nSV1);
        s.addR(nO23, nSP, 470e3);
        s.addC(nSP, 0, 940e-9);
        s.addR(nO30, nSM, 470e3);
        s.addC(nSM, nSV1, 940e-9);

        // stage 3: difference amp
        const int nP24 = s.addNode(), nN24 = s.addNode(), nO24 = s.addNode(), nSV2 = s.addNode();
        s.addR(nO30, nP24, 1.0 / (1.0 / 6.8e3 + 1.0 / 100e3));
        s.addR(nP24, 0, 6.8e3);
        s.addR(nP24, nSV2, 100e3);
        s.addR(nO23, nN24, 6.8e3);
        s.addR(nN24, nO24, 6.8e3);
        ne10(nP24, nN24, nO24);

        pO24 = nO24;

        // servo 2: nulls U24 output
        const int nSM2 = s.addNode();
        tl(0, nSM2, nSV2);
        s.addR(nO24, nSM2, 470e3);
        s.addC(nSM2, nSV2, 940e-9);

        // balanced line driver (+/-0.995 x, 10k in, 50 ohm out per leg, clip +/-7.4 V)
        const int nSPo = s.addNode(), nSN = s.addNode();
        s.addR(nO24, 0, 10e3);
        s.addBuffer(nO24, 0, nSPo, 0.995, 7.4, 50.0);
        s.addBuffer(nO24, 0, nSN, -0.995, 7.4, 50.0);
        nOutP = s.addNode();
        nOutN = s.addNode();
        s.addC(nOutP, nSPo, 100e-6);
        s.addC(nOutN, nSN, 100e-6);
        s.addR(nOutP, 0, 10e3);
        s.addR(nOutN, 0, 10e3);
        s.addR(nOutP, nOutN, 600.0);   // load
        pP = nOutP; pN = nOutN; pIn = INP;
    }

    // Unity at the reference gain setting at 1 kHz.
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
    int nSp = 0, nSn = 0, nOutP = 0, nOutN = 0, rvA = 0, rvB = 0;
    double lastGain = 0.0;
};
