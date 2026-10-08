#pragma once

#include <array>
#include "CircuitSolver.h"

// FSF: transformer-coupled console mic preamp channel, simulated from a
// transcribed netlist (kept privately, not distributed). Signal path: mic
// transformer (primaries in parallel, 1:5) -> 33k attenuator ladder ->
// 12-position gain switch -> op-amp gain stage with a tapped feedback ladder
// -> op-amp driving a class-AB discrete emitter-follower pair into the output
// transformer, whose tertiary winding closes the audio feedback loop -> 600
// ohm load.
//
// Provenance
//   [NETLIST]   topology and values from the netlist (phantom off, phase normal)
//   [DATASHEET] mic transformer ratio, DC resistance, inductance (12 H, derived
//               from the published response), pair-transistor figures
//   [PLACEHOLDER] output transformer: turns, inductances and resistances are
//               unknown (custom part); the netlist's guesses are used
//   [ESTIMATE]  saturation knees of both transformers (mic transformer fitted
//               to 0.2 % THD at 0 dBu / 50 Hz), full-scale calibration
//   [MODEL]     op-amps are single-pole, slew-limited, clipped behavioural
//               models; base/emitter series resistances are not modelled
//
// The gain switch is stepped in the hardware, so setGain() picks the nearest
// switch position and covers the remainder with a small input trim (+/- 3 dB).
class FsfCircuit {
public:
    static constexpr double kRail = 15.0;       // [NETLIST]
    static constexpr int kRefPos = 6;           // switch position that is the 0 dB drive reference
    static constexpr int kPositions = 12;

    // Mic transformer (LL1538-class), primaries parallel, 1:5.
    static constexpr double kMicL = 12.0;       // [DATASHEET, derived]
    static constexpr double kMicLSat = 0.6;     // [ESTIMATE]
    static constexpr double kMicISat = 0.0004;  // [ESTIMATE]
    // Output transformer: all [PLACEHOLDER].
    static constexpr double kOutL = 2.0;
    static constexpr double kOutLSat = 0.2;
    static constexpr double kOutISat = 0.15;

    static constexpr double kMaxGain = 75.0;    // about 37.5 dB above the reference position

    bool prepare(double sampleRate)
    {
        build();
        const bool ok = solver.prepare(sampleRate);
        calibrate();
        return ok;
    }

    // Second channel: share the gain table instead of re-measuring it.
    bool prepareLike(const FsfCircuit& other, double sampleRate)
    {
        build();
        const bool ok = solver.prepare(sampleRate);
        gainDb = other.gainDb;
        outScale = other.outScale;
        vFull = other.vFull;
        setPos(kRefPos);
        return ok;
    }

    void restoreDc(double sampleRate)
    {
        solver.restoreDc(sampleRate);
        resetPos();
    }

    void restoreDc() noexcept
    {
        solver.restoreDc();
        resetPos();
    }

    // Balanced source EMF (differential) at 0 dBFS.
    void setFullScaleVolts(double v) noexcept { vFull = v; }

    // Linear gain relative to the reference position (>= 1).
    void setGain(double g) noexcept
    {
        const double target = gainDb[kRefPos - 1] + 20.0 * std::log10(std::max(g, 1e-3));
        int best = pos;
        // hysteresis: only leave the current position when another is clearly nearer
        double bestErr = std::abs(target - gainDb[static_cast<size_t>(pos - 1)]) - 0.5;
        for (int p = 1; p <= kPositions; ++p) {
            const double e = std::abs(target - gainDb[static_cast<size_t>(p - 1)]);
            if (e < bestErr) { bestErr = e; best = p; }
        }
        if (best != pos) setPos(best);
        trim = std::pow(10.0, (target - gainDb[static_cast<size_t>(pos - 1)]) / 20.0);
    }

    double process(double x) noexcept
    {
        const double e = 0.5 * x * trim * vFull;
        solver.setFixed(nSp, e);
        solver.setFixed(nSn, -e);
        solver.step();
        return (solver.voltage(nOutP) - solver.voltage(nOutN)) * outScale / vFull;
    }

    CircuitSolver& core() noexcept { return solver; }
    std::array<double, kPositions> gainDb{};
    double outScale = 1.0;
    double vFull = 1.0;

private:
    using Npn = CircuitSolver::NpnModel;

    void resetPos() noexcept
    {
        const int p = pos;
        pos = 0;
        setPos(p);
    }

    // RS1: deck M picks the input tap, deck Z the feedback tap. Closed = 1 mOhm, open = 1 GOhm.
    void setPos(int p) noexcept
    {
        pos = p;
        auto sw = [&](int idx, bool closed) { solver.setR(idx, closed ? 1e-3 : 1e9); };
        for (int i = 0; i < 5; ++i) sw(rM[static_cast<size_t>(i)], p == i + 1);   // taps A..E on positions 1..5
        sw(rM[5], p >= 6);                                                          // full secondary from 6 up
        sw(rZ[0], p <= 6);                                                          // top feedback tap
        for (int i = 1; i < 7; ++i) sw(rZ[static_cast<size_t>(i)], p == 6 + i);     // taps 20..25 on positions 7..12
    }

    void build()
    {
        solver = CircuitSolver();
        auto& s = solver;
        const Npn bc441{1e-13, 100.0, 5.0, 100.0, 60e-12, 30e-12, 3.2e-9, false};
        const Npn bc461{1e-13, 100.0, 5.0, 100.0, 60e-12, 30e-12, 3.2e-9, true};
        const double d4148Is = 2.52e-9, d4148N = 1.752;

        const int VP = s.addFixedNode(kRail), VN = s.addFixedNode(-kRail);
        nSp = s.addFixedNode(0.0);
        nSn = s.addFixedNode(0.0);

        auto ne = [&](int p, int n, int o) { s.addOpamp(p, n, o, 10e6, 6.0, 1e5, 13.0, 10.0, 100e3); };

        // test-bench source: 150 ohm balanced mic (75 + 75)
        const int INP = s.addNode(), INN = s.addNode();
        s.addR(nSp, INP, 75.0);
        s.addR(nSn, INN, 75.0);

        // phantom feed (off): 3k6 each to a node discharged by 2k4
        const int NPH = s.addNode();
        s.addR(INP, NPH, 3.6e3);
        s.addR(INN, NPH, 3.6e3);
        s.addR(NPH, 0, 2.4e3);

        // mic transformer: primary DCR 22 (parallel), floating primary, 1:5, secondary DCR 1760
        const int nTP = s.addNode(), nTS = s.addNode(), S5 = s.addNode();
        s.addR(INP, nTP, 22.0);
        s.addL(nTP, INN, kMicL, kMicLSat, kMicISat);
        s.addIdeal(nTP, INN, nTS, 0, 5.0);
        s.addR(nTS, S5, 1760.0);

        // secondary Zobel and attenuator ladder
        const int nZb = s.addNode();
        s.addC(S5, nZb, 220e-12);
        s.addR(nZb, 0, 5.6e3);
        const int tE = s.addNode(), tD = s.addNode(), tC = s.addNode(), tB = s.addNode(), tA = s.addNode();
        s.addR(S5, tE, 15e3);
        s.addR(tE, tD, 9.1e3);
        s.addR(tD, tC, 4.7e3);
        s.addR(tC, tB, 2.2e3);
        s.addR(tB, tA, 1e3);
        s.addR(tA, 0, 1e3);

        // RS1 deck M: input tap
        const int WM = s.addNode();
        const int taps[6] = {tA, tB, tC, tD, tE, S5};
        for (int i = 0; i < 6; ++i) rM[static_cast<size_t>(i)] = s.addR(taps[i], WM, 1e9, true);

        // IC101 gain stage
        const int P1 = s.addNode(), N1 = s.addNode(), O1 = s.addNode(), nDC = s.addNode();
        s.addC(WM, P1, 100e-6 + 100e-9);
        s.addR(P1, 0, 220e3);
        ne(P1, N1, O1);
        s.addC(N1, O1, 220e-12);
        s.addR(O1, nDC, 6.2e3);
        s.addR(nDC, N1, 6.2e3);
        s.addC(nDC, 0, 100e-6);
        const int nY = s.addNode(), tX = s.addNode(), t20 = s.addNode(), t21 = s.addNode(), t22 = s.addNode();
        const int t23 = s.addNode(), t24 = s.addNode(), t25 = s.addNode(), nG = s.addNode();
        s.addC(O1, nY, 100e-6);
        s.addR(nY, tX, 1.8e3);
        s.addR(tX, t20, 2.4e3);
        s.addR(t20, t21, 1.2e3);
        s.addR(t21, t22, 560.0);
        s.addR(t22, t23, 270.0);
        s.addR(t23, t24, 120.0);
        s.addR(t24, t25, 56.0);
        s.addR(t25, nG, 56.0);
        s.addC(nG, 0, 100e-6);
        // RS1 deck Z: feedback tap
        const int ztaps[7] = {tX, t20, t21, t22, t23, t24, t25};
        for (int i = 0; i < 7; ++i) rZ[static_cast<size_t>(i)] = s.addR(ztaps[i], N1, 1e9, true);

        // interstage and meter load
        const int P3 = s.addNode(), MTR = s.addNode();
        s.addC(O1, P3, 100e-6);
        s.addR(P3, 0, 4.7e3);
        s.addC(P3, MTR, 100e-6);
        s.addR(MTR, 0, 10e3);

        // IC103 and class-AB output stage
        const int N3 = s.addNode(), O3 = s.addNode(), Q = s.addNode(), Q2 = s.addNode(), OUT = s.addNode();
        const int BL = s.addNode(), BN = s.addNode(), BP = s.addNode(), EN = s.addNode(), EP = s.addNode();
        ne(P3, N3, O3);
        s.addR(N3, Q, 1.2e3);
        s.addR(Q, Q2, 240.0);
        s.addR(Q2, OUT, 1.2e3);
        s.addC(Q, OUT, 22e-9);
        s.addC(BL, Q2, 100e-6);
        s.addR(VP, BN, 1.8e3);
        s.addDiode(BN, O3, d4148Is, d4148N);
        s.addDiode(O3, BP, d4148Is, d4148N);
        s.addR(BP, VN, 1.8e3);
        s.addC(BN, BP, 47e-9);
        s.addNpn(VP, BN, EN, bc441);
        s.addR(EN, OUT, 22.0);
        s.addNpn(VN, BP, EP, bc461);
        s.addR(EP, OUT, 22.0);

        // output transformer: primary 1, secondary 1, tertiary 0.5 (all placeholders)
        const int nPO = s.addNode(), nSO = s.addNode(), Y = s.addNode(), O = s.addNode(), nTO = s.addNode();
        s.addR(OUT, nPO, 10.0);
        s.addL(nPO, 0, kOutL, kOutLSat, kOutISat);
        s.addIdeal(nPO, 0, nSO, O, 1.0);
        s.addR(Y, nSO, 20.0);
        s.addIdeal(nPO, 0, nTO, 0, 0.5);
        s.addR(BL, nTO, 10.0);

        // output (phase normal): load across Y-O, high-value references for the floating winding
        nOutP = Y;
        nOutN = O;
        s.addR(Y, 0, 1e6);
        s.addR(O, 0, 1e6);
        s.addR(Y, O, 600.0);
    }

    // Small-signal 1 kHz gain at every switch position; output normalised to
    // unity at the reference position.
    void calibrate()
    {
        const double sr = solver.sampleRate();
        outScale = 1.0;
        vFull = 1.0;
        trim = 1.0;
        const double w = 2.0 * 3.14159265358979 * 1000.0 / sr;
        for (int p = 1; p <= kPositions; ++p) {
            solver.restoreDc();
            setPos(p);
            const double amp = 1e-3 * std::pow(10.0, -((p - 1) * 6.0) / 20.0);   // ~1 mV out at every position
            const int n = static_cast<int>(sr * 0.15);
            double pk = 0.0;
            for (int i = 0; i < n; ++i) {
                const double e = 0.5 * amp * std::sin(w * i);
                solver.setFixed(nSp, e);
                solver.setFixed(nSn, -e);
                solver.step();
                if (i > n / 2)
                    pk = std::max(pk, std::abs(solver.voltage(nOutP) - solver.voltage(nOutN)));
            }
            gainDb[static_cast<size_t>(p - 1)] = 20.0 * std::log10(pk / amp);
        }
        outScale = std::pow(10.0, -gainDb[kRefPos - 1] / 20.0);
        solver.restoreDc();
        pos = 0;
        setPos(kRefPos);
    }

    CircuitSolver solver;
    int nSp = 0, nSn = 0, nOutP = 0, nOutN = 0;
    std::array<int, 6> rM{};
    std::array<int, 7> rZ{};
    int pos = kRefPos;
    double trim = 1.0;
};
