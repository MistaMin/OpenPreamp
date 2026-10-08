#pragma once

#include "CircuitSolver.h"

// N-Type: transformer-coupled line path, simulated from a transcribed
// amplifier-card netlist (kept privately, not distributed). Signal path:
// VTB9046 input transformer -> 3-transistor feedback preamp -> class-A
// TR1/TR2/2N3055 output stage -> gapped VTB9049 output transformer -> 600 ohm
// load with Zobel. Every component below is a real node in a nonlinear nodal
// solve; nothing is a behavioural shortcut.
//
// Provenance
//   [NETLIST]   values/topology from the SPICE file (medium/low-confidence
//               connections are used exactly as transcribed there)
//   [DATASHEET] transformer design guide: turns, DCR, source/load pairings
//   [ESTIMATE]  anything the guide does not publish: transformer inductance,
//               saturation, leakage, winding capacitance, bias-pot position,
//               transistor models (generic), full-scale calibration
class NTypeCircuit {
public:
    // Input transformer VTB9046, primaries series / secondaries parallel (4:1, -13 dB, 10k : 600).
    static constexpr double kInSource = 10000.0;  // [DATASHEET]
    static constexpr double kInDcrPri = 350.0;    // [DATASHEET] 175 + 175
    static constexpr double kInDcrSec = 28.0;     // [DATASHEET] 56 || 56
    static constexpr double kInTurns = 0.25;      // [DATASHEET] secondary/primary
    // The guide publishes no inductance. 16 H (the 1 H/half SPICE placeholder) puts the
    // -3 dB corner near 100 Hz from a 10k source, which no real line input does, so a
    // value giving a ~15 Hz corner is used instead. THIS IS THE KEY NUMBER TO FIT.
    static constexpr double kInL0 = 100.0;        // [ESTIMATE]
    static constexpr double kInLSat = 5.0;        // [ESTIMATE]
    static constexpr double kInISat = 0.0005;     // [ESTIMATE] core knee (0.05 V*s of flux)
    static constexpr double kInLeak = 0.02;       // [ESTIMATE] primary-referred leakage
    static constexpr double kInCap = 200e-12;     // [ESTIMATE] secondary winding capacitance

    // Output transformer VTB9049, series/series (1:1.7, 200 : 600, +4 dB), gapped.
    static constexpr double kOutDcrPri = 12.0;    // [DATASHEET]
    static constexpr double kOutDcrSec = 40.0;    // [DATASHEET]
    static constexpr double kOutTurns = 1.7;      // [DATASHEET]
    static constexpr double kOutL0 = 4.0;         // [ESTIMATE] (4 x 1 H placeholder)
    static constexpr double kOutLSat = 0.3;       // [ESTIMATE] gapped core
    static constexpr double kOutISat = 0.06;      // [ESTIMATE] knee vs ~42 mA standing current
    static constexpr double kOutLeak = 0.0005;    // [ESTIMATE] primary-referred leakage
    static constexpr double kVceo = 59.4;         // [DATASHEET-ish] 2N3055 Vceo(sus) 60 V less one diode drop
    static constexpr double kRail = 24.0;         // [NETLIST] "+24 V typical"
    static constexpr double kLoad = 600.0;        // [DATASHEET]
    static constexpr double kBiasPot = 0.5;       // [ESTIMATE] RV1 wiper position (trimmed for symmetrical clipping in the hardware)

    // Gain-control resistor (R11, T to 0V) bounds; the stock value is 1k8.
    static constexpr double kR11Nominal = 1800.0;

    static constexpr double kMaxGain = 20.0;    // closed-loop gain limit (26 dB); the engine trims the rest

    void restoreDc(double sampleRate) { solver.restoreDc(sampleRate); }
    void restoreDc() noexcept { solver.restoreDc(); }

    bool prepare(double sampleRate)
    {
        build();
        const bool ok = solver.prepare(sampleRate);
        calibrate();
        return ok;
    }

    // Input is in digital full-scale units; vFull is the line EMF at 0 dBFS.
    void setFullScaleVolts(double v) noexcept { vFull = v; }

    // Closed-loop gain relative to the stock 1k8 network (1 = stock), set by
    // R11 (T to 0V) the way the gain switch does. The feedback fraction beta
    // is the T-node divider; the stage's finite loop gain adds a small
    // offset delta (fitted to the simulation) so the gain tracks the request.
    // Usable range is about 1..20 (0..26 dB).
    void setGain(double g) noexcept
    {
        constexpr double kDelta = 0.00627;
        g = std::clamp(g, 1.0, 20.0);
        const double gOut = 1.0 / 2200.0 + 1.0 / 51000.0;
        const double gGnd0 = 1.0 / 390.0 + 1.0 / kR11Nominal;
        const double beta0 = gOut / (gOut + gGnd0);
        const double beta = std::max((beta0 + kDelta) / g - kDelta, 0.002);
        const double gGnd = gOut * (1.0 / beta - 1.0);
        const double g11 = gGnd - 1.0 / 390.0;
        solver.setR(rGain, g11 > 1.0 / 20000.0 ? 1.0 / g11 : 20000.0);
    }

    double process(double x) noexcept
    {
        solver.setFixed(nIn, x * vFull);
        solver.step();
        return solver.voltage(nOutP) * outScale / vFull;
    }

    double outputVolts() const noexcept { return solver.voltage(nOutP); }
    double voltage(int n) const noexcept { return solver.voltage(n); }
    CircuitSolver& core() noexcept { return solver; }
    int outNode() const noexcept { return nOutP; }
    int inNode() const noexcept { return nIn; }

    // Output normalisation measured at calibration time (unity at 1 kHz small-signal).
    double outScale = 1.0;
    double vFull = 1.0;

private:
    using Npn = CircuitSolver::NpnModel;

    void build()
    {
        solver = CircuitSolver();
        // Device models from ntype_preamp.cir (datasheet-fitted). RB and RC are not modelled.
        const Npn bc184c{1.8e-14, 450.0, 5.0, 80.0, 9e-12, 5e-12, 1.06e-9};
        const Npn bc184cHi{1.8e-14, 650.0, 5.0, 80.0, 9e-12, 5e-12, 1.06e-9};
        const Npn n2n3055{1e-12, 37.0, 5.0, 100.0, 500e-12, 200e-12, 64e-9};
        auto& s = solver;

        const int N = s.addFixedNode(kRail);
        nIn = s.addFixedNode(0.0);

        // ---- input transformer: source, DCR, leakage, magnetising L, ideal ratio, DCR, winding C
        const int inA = s.addNode(), inB = s.addNode(), inP = s.addNode(), inS = s.addNode();
        const int U = s.addNode();
        s.addR(nIn, inA, kInSource);
        s.addR(inA, inB, kInDcrPri);
        s.addL(inB, inP, kInLeak);
        s.addL(inP, 0, kInL0, kInLSat, kInISat);
        s.addIdeal(inP, 0, inS, 0, kInTurns);
        s.addR(inS, U, kInDcrSec);
        s.addC(U, 0, kInCap);

        // ---- PREAMP (netlist order)
        const int b1 = s.addNode(), fb = s.addNode(), c1 = s.addNode(), e1 = s.addNode();
        const int dec = s.addNode(), e2 = s.addNode(), c2 = s.addNode(), out = s.addNode();
        const int t = s.addNode(), T = s.addNode(), S = s.addNode(), P = s.addNode();
        s.addR(U, 0, 120e3);
        s.addC(b1, U, 10e-6);
        s.addR(b1, fb, 68e3);
        s.addC(c1, b1, 100e-12);
        s.addC(b1, e1, 1500e-12);
        s.addNpn(c1, b1, e1, bc184cHi);
        s.addR(N, dec, 33e3);
        s.addR(dec, c1, 47e3);
        s.addC(dec, e2, 22e-6);
        s.addC(c1, fb, 680e-12);
        s.addNpn(c2, c1, e2, bc184c);
        s.addR(N, c2, 5.1e3);
        s.addR(e2, fb, 470.0);
        s.addR(fb, 0, 1.5e3);
        s.addC(fb, 0, 125e-6);
        s.addC(e2, S, 22e-6);
        s.addNpn(N, c2, out, bc184c);
        s.addC(out, P, 22e-6);
        s.addR(out, t, 2.2e3);
        s.addR(t, 0, 390.0);
        s.addR(e1, t, 10e3);
        s.addC(S, 0, 1000e-12);
        s.addC(t, T, 400e-6);
        rGain = s.addR(T, 0, kR11Nominal, true);
        s.addR(P, T, 51e3);

        // ---- OUTSTAGE (L = P)
        const int n1 = s.addNode(), ob1 = s.addNode(), oc1 = s.addNode(), K = s.addNode();
        const int wip = s.addNode(), B = s.addNode(), ofb = s.addNode(), D = s.addNode();
        const int C = s.addNode(), F = s.addNode();
        s.addC(n1, P, 10e-6);
        s.addR(n1, ob1, 2.2e3);
        s.addNpn(oc1, ob1, K, bc184cHi);
        s.addC(oc1, ob1, 220e-12);
        s.addC(ob1, K, 4.7e-9);
        s.addR(ob1, wip, 56e3);
        s.addR(N, oc1, 68e3);
        s.addR(K, 0, 1.2e3);
        s.addC(B, ofb, 80e-6);
        s.addR(ofb, K, 3.3e3);
        s.addC(ofb, K, 330e-12);
        s.addNpn(B, oc1, D, bc184c);
        s.addR(D, 0, 18e3);
        s.addNpn(B, D, C, n2n3055);
        s.addR(C, wip, 4.7e3 * (1.0 - kBiasPot));
        s.addR(wip, 0, 4.7e3 * kBiasPot);
        s.addR(C, 0, 47.0);               // R7 to A = ground
        s.addC(B, F, 80e-6);
        s.addR(F, 0, 33e3);

        // ---- VTB9049: rail -> DCR -> leakage -> (magnetising L || ideal 1.7) -> collector B
        const int pa = s.addNode(), pb = s.addNode(), sa = s.addNode();
        nOutP = s.addNode();
        const int zn = s.addNode();
        s.addR(N, pa, kOutDcrPri);
        s.addL(pa, pb, kOutLeak);
        s.addL(pb, B, kOutL0, kOutLSat, kOutISat);
        s.addIdeal(pb, B, sa, 0, kOutTurns);
        // 2N3055 collector breakdown (Vceo ~60 V) stops transformer flyback running away.
        s.addDiode(B, s.addFixedNode(kVceo), 1e-14);
        s.addR(sa, nOutP, kOutDcrSec);
        s.addR(nOutP, 0, kLoad);
        s.addR(nOutP, zn, 1.5e3);        // Zobel
        s.addC(zn, 0, 10e-9);
    }

    // Measure small-signal 1 kHz gain at the stock gain setting so that the
    // normalised output is unity there.
    void calibrate()
    {
        outScale = 1.0;
        const double sr = solver.sampleRate();
        setGain(1.0);
        const double amp = 1e-3, w = 2.0 * 3.14159265358979 * 1000.0 / sr;
        const int n = static_cast<int>(sr * 0.1);
        double pk = 0.0;
        for (int i = 0; i < n; ++i) {
            solver.setFixed(nIn, amp * std::sin(w * i));
            solver.step();
            if (i > n / 2) pk = std::max(pk, std::abs(solver.voltage(nOutP)));
        }
        // outScale = 1 / (small-signal gain from line EMF to load voltage).
        outScale = amp / pk;
        solver.restoreDc();
    }

    CircuitSolver solver;
    int nIn = 0, nOutP = 0, rGain = 0;
};
