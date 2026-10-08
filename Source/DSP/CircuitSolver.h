#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

// Small real-time nonlinear circuit solver (nodal analysis, trapezoidal
// integration, Newton iteration per sample, double precision).
//
// Elements: resistor, capacitor, inductor (linear or flux-saturating,
// monotonic two-slope B-H law), ideal transformer, NPN BJT (Ebers-Moll with
// Early effect), and fixed-voltage nodes (rails / driven inputs).
// Node 0 is ground.
class CircuitSolver {
public:
    struct NpnModel {
        double is, bf, br, va, cje, cjc;
        double tf = 0.0;   // forward transit time: diffusion charge tf * If
        bool pnp = false;  // PNP device (all junction polarities and currents mirrored)
    };

    CircuitSolver() { nodeFixed.push_back(true); nodeVal.push_back(0.0); }

    int addNode()
    {
        nodeFixed.push_back(false);
        nodeVal.push_back(0.0);
        return static_cast<int>(nodeFixed.size()) - 1;
    }

    int addFixedNode(double v)
    {
        nodeFixed.push_back(true);
        nodeVal.push_back(v);
        return static_cast<int>(nodeFixed.size()) - 1;
    }

    void setFixed(int node, double v) noexcept { nodeVal[static_cast<size_t>(node)] = v; }

    // Returns the resistor's index so setR can change it while running.
    int addR(int a, int b, double ohm, bool variable = false)
    {
        res.push_back({a, b, 1.0 / ohm, variable});
        return static_cast<int>(res.size()) - 1;
    }

    void setR(int index, double ohm) noexcept { res[static_cast<size_t>(index)].g = 1.0 / ohm; }

    void addC(int a, int b, double farad) { caps.push_back({a, b, farad, 0.0, 0.0, 0.0}); }

    // Inductor a->b. If isat > 0 the flux is lSat*i + (l0-lSat)*isat*tanh(i/isat).
    void addL(int a, int b, double l0, double lSat = 0.0, double isat = 0.0)
    {
        inds.push_back({a, b, l0, lSat, isat, 0, 0.0, 0.0});
    }

    // Ideal transformer: v(sa)-v(sb) = n * (v(pa)-v(pb)).
    void addIdeal(int pa, int pb, int sa_, int sb_, double ratio) { idls.push_back({pa, pb, sa_, sb_, ratio, 0}); }

    // Ideal-exponential diode anode -> cathode (used e.g. as a breakdown clamp).
    void addDiode(int anode, int cathode, double is, double n = 1.0) { dios.push_back({anode, cathode, is, n}); }

    // Behavioural controlled elements (the building blocks of an op-amp):
    //   transconductor: current i = imax*tanh(gm*(v(p)-v(n))/imax) injected into `node`
    //   buffer:         out is driven through rout from vmax*tanh(gain*(v(p)-v(n))/vmax)
    // Smooth tanh limits are used instead of hard clamps so Newton converges.
    void addTransconductor(int p, int n_, int node, double gm, double imax) { gms.push_back({p, n_, node, gm, imax}); }
    // Steep one-sided-by-magnitude leak that pins `node` near +/-vcl: carries
    // imax at |v| = vcl and rises as (v/vcl)^16, so it is inert well inside.
    void addClampLeak(int node, double vcl, double imax) { leaks.push_back({node, vcl, imax / vcl}); }

    void addBuffer(int p, int n_, int out, double gain, double vmax, double rout) { bufs.push_back({p, n_, out, gain, vmax, 1.0 / rout}); }

    // Op-amp as in the netlists: input resistance, single dominant pole
    // (gbw, aol), slew limit sr (V/us), output clip +/-vsw and output resistance.
    void addOpamp(int inp, int inn, int out, double gbw, double sr, double aol, double vsw, double rout, double rin)
    {
        const double ci = 1e-12;
        const double gm = 6.2831853 * gbw * ci;
        const double imax = sr * 1e6 * ci;
        const int n1 = addNode();
        addR(inp, inn, rin);
        addTransconductor(inp, inn, n1, gm, imax);
        addC(n1, 0, ci);
        addR(n1, 0, aol / gm);
        addClampLeak(n1, 1.1 * vsw, imax);
        addBuffer(n1, 0, out, 1.0, vsw, rout);
    }

    void addNpn(int c, int b, int e, const NpnModel& m)
    {
        bjts.push_back({c, b, e, m, 0.0, 0.0});
        if (m.cje > 0.0) addC(b, e, m.cje);
        if (m.cjc > 0.0) addC(b, c, m.cjc);
    }

    // Finish construction: allocate, solve the DC operating point, and
    // initialise reactive-element state for transient simulation.
    bool prepare(double sampleRate)
    {
        h = 0.5 / sampleRate;
        const size_t nn = nodeFixed.size();
        idx.assign(nn, -1);
        int k = 0;
        for (size_t i = 0; i < nn; ++i)
            if (!nodeFixed[i]) idx[i] = k++;
        nodeUnknowns = k;
        for (auto& l : inds) l.ex = k++;
        for (auto& t : idls) t.ex = k++;
        n = k;
        x.assign(static_cast<size_t>(n), 0.0);
        f.assign(static_cast<size_t>(n), 0.0);
        dx.assign(static_cast<size_t>(n), 0.0);
        rhs.assign(static_cast<size_t>(n), 0.0);
        sol.assign(static_cast<size_t>(n), 0.0);
        rp.assign(static_cast<size_t>(n), 0);
        nzStart.assign(static_cast<size_t>(n), 0);
        nzEnd.assign(static_cast<size_t>(n), 0);
        nzIdx.assign(static_cast<size_t>(n) * static_cast<size_t>(n), 0);
        jac.assign(static_cast<size_t>(n) * static_cast<size_t>(n), 0.0);
        for (auto& c : caps) c.geq = 2.0 * c.c * sampleRate;
        orderColumns();
        std::fill(x.begin(), x.end(), 0.0);

        staticOk = false;
        fastOk = false;
        bool ok = true;
        dc = true;
        for (double gm : {1e-3, 1e-5, 1e-7, 1e-9, 1e-12}) {
            gmin = gm;
            ok = newton(200, 1e-9) && ok;
        }
        if (!ok) {
            // Plain DC Newton failed (high-gain loops, integrators): ramp the
            // supplies and integrate to the operating point instead.
            dc = false;
            if (settle(sampleRate)) {
                dc = true;
                gmin = 1e-12;
                ok = newton(200, 1e-8);
                if (!ok) ok = true;   // the settled point is already a valid operating point
            }
        }
        dc = false;
        gmin = 1e-12;
        xDc = x;
        nodePrev = nodeVal;
        sr = sampleRate;
        buildLinear();
        fastOk = true;
        initHistory();
        buildSymbolic();
        return ok;
    }

    double sampleRate() const noexcept { return sr; }
    int size() const noexcept { return n; }
    long long iterCount = 0, fallbacks = 0, failCount = 0, hardFails = 0;
    int consecutiveFails = 0;

    // Back to the stored DC operating point (no allocation; audio-thread safe).
    void restoreDc() noexcept
    {
        x = xDc;
        nodePrev = nodeVal;
        initHistory();
    }

    // Change the sample rate and return to the DC operating point.
    // Re-freeze the symbolic factorisation because capacitor g_eq (and therefore
    // the numeric pivot sequence) changes with sample rate.
    void restoreDc(double sampleRate)
    {
        sr = sampleRate;
        h = 0.5 / sampleRate;
        for (auto& c : caps) c.geq = 2.0 * c.c * sampleRate;
        buildLinear();
        restoreDc();
        buildSymbolic();
    }

    double voltage(int node) const noexcept { return v(node); }

    // Advance one sample (inputs already set with setFixed).
    void step() noexcept
    {
        xSave = x;
        if (!newton(8, 1e-7)) {
            ++failCount;
            // Recovery: source-step the fixed nodes from last sample's values with a tight step limit.
            x = xSave;
            const auto target = nodeVal;
            bool ok = true;
            constexpr int kSteps = 8;
            for (int k = 1; k <= kSteps && ok; ++k) {
                for (size_t i = 0; i < nodeVal.size(); ++i)
                    nodeVal[i] = nodePrev[i] + (target[i] - nodePrev[i]) * k / kSteps;
                ok = newton(40, 1e-7, 1.0);
            }
            nodeVal = target;
            if (!ok || !std::isfinite(x[0])) {
                ++hardFails;
                x = xSave;
                if (++consecutiveFails > 200) {
                    restoreDc();
                    consecutiveFails = 0;
                }
            } else {
                consecutiveFails = 0;
            }
        }
        nodePrev = nodeVal;
        for (auto& c : caps) {
            const double vn = v(c.a) - v(c.b);
            c.iPrev = c.geq * (vn - c.vPrev) - c.iPrev;
            c.vPrev = vn;
        }
        for (auto& l : inds) {
            l.phiPrev = flux(l, x[static_cast<size_t>(l.ex)]);
            l.vPrev = v(l.a) - v(l.b);
        }
        for (auto& q : bjts) {
            if (q.m.tf <= 0.0) continue;
            const double vbe = (q.m.pnp ? -1.0 : 1.0) * (v(q.b) - v(q.e));
            double e, de;
            expLim(vbe, e, de);
            const double qd = q.m.tf * q.m.is * (e - 1.0);
            q.iqPrev = 2.0 * sr * (qd - q.qPrev) - q.iqPrev;
            q.qPrev = qd;
        }
        updateConst();
    }

private:
    void initHistory() noexcept
    {
        for (auto& c : caps) {
            c.vPrev = v(c.a) - v(c.b);
            c.iPrev = 0.0;
        }
        for (auto& l : inds) {
            l.phiPrev = flux(l, x[static_cast<size_t>(l.ex)]);
            l.vPrev = 0.0;
        }
        for (auto& q : bjts) {
            q.qPrev = q.m.tf * q.m.is * (std::exp(std::min((q.m.pnp ? -1.0 : 1.0) * (v(q.b) - v(q.e)), vLim) / vt) - 1.0);
            q.iqPrev = 0.0;
        }
        if (!bconst.empty()) updateConst();
    }

    struct Res { int a, b; double g; bool var; };
    struct Cap { int a, b; double c, vPrev, iPrev, geq; };
    struct Ind { int a, b; double l0, lSat, isat; int ex; double phiPrev, vPrev; };
    struct Idl { int pa, pb, sa, sb; double n; int ex; };
    struct Dio { int a, c; double is, n; };
    struct Gm { int p, n, node; double gm, imax; };
    struct Leak { int node; double vcl, k; };
    struct Buf { int p, n, out; double gain, vmax, g; };
    struct Bjt { int c, b, e; NpnModel m; double qPrev, iqPrev; };

    static constexpr double vt = 0.02585;
    static constexpr double vLim = 0.95;

    double v(int node) const noexcept
    {
        const int k = idx[static_cast<size_t>(node)];
        return k >= 0 ? x[static_cast<size_t>(k)] : nodeVal[static_cast<size_t>(node)];
    }

    static double flux(const Ind& l, double i) noexcept
    {
        if (l.isat <= 0.0) return l.l0 * i;
        return l.lSat * i + (l.l0 - l.lSat) * l.isat * std::tanh(i / l.isat);
    }

    static double fluxD(const Ind& l, double i) noexcept
    {
        if (l.isat <= 0.0) return l.l0;
        const double t = std::tanh(i / l.isat);
        return l.lSat + (l.l0 - l.lSat) * (1.0 - t * t);
    }

    static void expLim(double u, double& e, double& de) noexcept
    {
        if (u < vLim) {
            e = std::exp(u / vt);
            de = e / vt;
        } else {
            const double e0 = std::exp(vLim / vt);
            e = e0 * (1.0 + (u - vLim) / vt);
            de = e0 / vt;
        }
    }

    void addF(int node, double val) noexcept
    {
        const int k = idx[static_cast<size_t>(node)];
        if (k >= 0) f[static_cast<size_t>(k)] += val;
    }

    void addJ(int row, int col, double val) noexcept
    {
        const int r = idx[static_cast<size_t>(row)];
        const int c = idx[static_cast<size_t>(col)];
        if (r >= 0 && c >= 0) jac[static_cast<size_t>(r) * static_cast<size_t>(n) + colIdx(c)] += val;
    }

    void addJrc(int r, int col_, double val) noexcept
    {
        const int c = idx[static_cast<size_t>(col_)];
        if (c >= 0) jac[static_cast<size_t>(r) * static_cast<size_t>(n) + colIdx(c)] += val;
    }

    void addJcr(int row, int c, double val) noexcept
    {
        const int r = idx[static_cast<size_t>(row)];
        if (r >= 0) jac[static_cast<size_t>(r) * static_cast<size_t>(n) + colIdx(c)] += val;
    }

    void addJrr(int r, int c, double val) noexcept
    {
        jac[static_cast<size_t>(r) * static_cast<size_t>(n) + colIdx(c)] += val;
    }

    size_t colIdx(int variable) const noexcept { return static_cast<size_t>(colPos[static_cast<size_t>(variable)]); }

    void branch(int a, int b, double i, double g) noexcept
    {
        addF(a, i);
        addF(b, -i);
        addJ(a, a, g);
        addJ(a, b, -g);
        addJ(b, b, g);
        addJ(b, a, -g);
    }

    // Linear elements (fixed resistors, capacitors, linear inductors, ideal
    // transformers, gmin). In the transient their Jacobian is constant.
    void stampLinear() noexcept
    {
        for (int k = 0; k < nodeUnknowns; ++k) {
            const size_t ks = static_cast<size_t>(k);
            f[ks] += gmin * x[ks];
            jac[ks * static_cast<size_t>(n) + colIdx(k)] += gmin;
        }
        for (const auto& r : res)
            if (!r.var) branch(r.a, r.b, r.g * (v(r.a) - v(r.b)), r.g);

        if (!dc)
            for (const auto& c : caps)
                branch(c.a, c.b, c.geq * (v(c.a) - v(c.b) - c.vPrev) - c.iPrev, c.geq);

        for (const auto& l : inds)
            if (l.isat <= 0.0) stampInd(l);

        for (const auto& t : idls) {
            const double j = x[static_cast<size_t>(t.ex)];
            addF(t.sa, j);
            addF(t.sb, -j);
            addF(t.pa, -t.n * j);
            addF(t.pb, t.n * j);
            addJcr(t.sa, t.ex, 1.0);
            addJcr(t.sb, t.ex, -1.0);
            addJcr(t.pa, t.ex, -t.n);
            addJcr(t.pb, t.ex, t.n);
            f[static_cast<size_t>(t.ex)] += (v(t.sa) - v(t.sb)) - t.n * (v(t.pa) - v(t.pb));
            addJrc(t.ex, t.sa, 1.0);
            addJrc(t.ex, t.sb, -1.0);
            addJrc(t.ex, t.pa, -t.n);
            addJrc(t.ex, t.pb, t.n);
        }
    }

    void stampInd(const Ind& l) noexcept
    {
        const double i = x[static_cast<size_t>(l.ex)];
        const double vab = v(l.a) - v(l.b);
        addF(l.a, i);
        addF(l.b, -i);
        addJcr(l.a, l.ex, 1.0);
        addJcr(l.b, l.ex, -1.0);
        if (dc) {
            f[static_cast<size_t>(l.ex)] += vab;
            addJrc(l.ex, l.a, 1.0);
            addJrc(l.ex, l.b, -1.0);
        } else {
            f[static_cast<size_t>(l.ex)] += flux(l, i) - l.phiPrev - h * (vab + l.vPrev);
            addJrr(l.ex, l.ex, fluxD(l, i));
            addJrc(l.ex, l.a, -h);
            addJrc(l.ex, l.b, h);
        }
    }

    void stampNonlinear() noexcept
    {
        for (const auto& r : res)
            if (r.var) branch(r.a, r.b, r.g * (v(r.a) - v(r.b)), r.g);

        for (const auto& l : inds)
            if (l.isat > 0.0) stampInd(l);

        for (const auto& d : dios) {
            double e, de;
            expLim((v(d.a) - v(d.c)) / d.n, e, de);
            branch(d.a, d.c, d.is * (e - 1.0), d.is * de / d.n);
        }

        for (const auto& t : gms) {
            const double vd = v(t.p) - v(t.n);
            const double th = std::tanh(t.gm * vd / t.imax);
            const double i = t.imax * th;
            const double g = t.gm * (1.0 - th * th);
            addF(t.node, -i);
            addJ(t.node, t.p, -g);
            addJ(t.node, t.n, g);
        }

        for (const auto& l : leaks) {
            const double vn = v(l.node);
            const double xx = vn / l.vcl, x2 = xx * xx, x4 = x2 * x2, x8 = x4 * x4, x16 = x8 * x8;
            addF(l.node, l.k * vn * x16);
            addJ(l.node, l.node, 17.0 * l.k * x16);
        }

        for (const auto& b : bufs) {
            // Sharp-knee limiter x / (1 + x^8)^(1/8): linear below ~0.6 of the
            // limit, hard above, with a continuous derivative (x = v / vmax).
            const double vd = v(b.p) - v(b.n);
            const double xx = b.gain * vd / b.vmax;
            const double x2 = xx * xx, x4 = x2 * x2;
            const double u = 1.0 + x4 * x4;
            const double ui = std::pow(u, -0.125);
            const double vt_ = b.vmax * xx * ui;
            const double dv = b.gain * ui / u;
            const double i = (vt_ - v(b.out)) * b.g;   // current into out
            addF(b.out, -i);
            addJ(b.out, b.out, b.g);
            addJ(b.out, b.p, -dv * b.g);
            addJ(b.out, b.n, dv * b.g);
        }

        for (const auto& q : bjts) {
            const double pol = q.m.pnp ? -1.0 : 1.0;
            const double vb = v(q.b), vc = v(q.c), ve = v(q.e);
            const double vbe = pol * (vb - ve), vbc = pol * (vb - vc);
            double ef, def, er, der;
            expLim(vbe, ef, def);
            expLim(vbc, er, der);
            const double If = q.m.is * (ef - 1.0), Ir = q.m.is * (er - 1.0);
            const double gf = q.m.is * def, gr = q.m.is * der;
            double E = 1.0 - vbc / q.m.va, dE = -1.0 / q.m.va;
            if (E < 0.2) { E = 0.2; dE = 0.0; }
            const double ict = (If - Ir) * E;
            const double dictBe = gf * E;
            const double dictBc = -gr * E + (If - Ir) * dE;
            const double ic = ict - Ir / q.m.br;
            const double ib = If / q.m.bf + Ir / q.m.br;
            const double dicBe = dictBe, dicBc = dictBc - gr / q.m.br;
            const double dibBe = gf / q.m.bf, dibBc = gr / q.m.br;
            addF(q.c, pol * ic);
            addF(q.b, pol * ib);
            addF(q.e, -pol * (ic + ib));
            const double dicB = dicBe + dicBc, dicC = -dicBc, dicE = -dicBe;
            const double dibB = dibBe + dibBc, dibC = -dibBc, dibE = -dibBe;
            addJ(q.c, q.b, dicB); addJ(q.c, q.c, dicC); addJ(q.c, q.e, dicE);
            addJ(q.b, q.b, dibB); addJ(q.b, q.c, dibC); addJ(q.b, q.e, dibE);
            addJ(q.e, q.b, -(dicB + dibB));
            addJ(q.e, q.c, -(dicC + dibC));
            addJ(q.e, q.e, -(dicE + dibE));
            if (q.m.tf > 0.0 && !dc) {
                // diffusion charge qd = tf * If(vbe), trapezoidal: i = 2/dt*(qd - qPrev) - iqPrev, base -> emitter
                const double k = 2.0 * sr;
                const double qd = q.m.tf * If;
                const double iq = k * (qd - q.qPrev) - q.iqPrev;
                const double g = k * q.m.tf * gf;
                addF(q.b, pol * iq);
                addF(q.e, -pol * iq);
                addJ(q.b, q.b, g); addJ(q.b, q.e, -g);
                addJ(q.e, q.b, -g); addJ(q.e, q.e, g);
            }
        }
    }

    // Full assembly: used for the DC solve and for structural analysis.
    void assemble() noexcept
    {
        std::fill(f.begin(), f.end(), 0.0);
        std::fill(jac.begin(), jac.end(), 0.0);
        stampLinear();
        stampNonlinear();
    }

    // Transient assembly: constant linear part copied in, only the
    // nonlinear devices are stamped each iteration.
    void assembleFast() noexcept
    {
        std::copy(jacLin.begin(), jacLin.end(), jac.begin());
        std::copy(bconst.begin(), bconst.end(), f.begin());
        for (const auto& e : linEntries)
            f[static_cast<size_t>(e.row)] += e.val * x[static_cast<size_t>(e.var)];
        for (const auto& e : fixEntries)
            f[static_cast<size_t>(e.row)] += e.val * nodeVal[static_cast<size_t>(e.var)];
        stampNonlinear();
    }

    // Constant Jacobian of the linear elements for the current sample rate.
    void buildLinear()
    {
        const size_t N = static_cast<size_t>(n);
        dc = false;
        gmin = 1e-12;
        std::fill(f.begin(), f.end(), 0.0);
        std::fill(jac.begin(), jac.end(), 0.0);
        stampLinear();
        jacLin = jac;
        invPos.assign(N, 0);
        for (size_t v_ = 0; v_ < N; ++v_) invPos[static_cast<size_t>(colPos[v_])] = static_cast<int>(v_);
        linEntries.clear();
        for (size_t r = 0; r < N; ++r)
            for (size_t c = 0; c < N; ++c)
                if (jacLin[r * N + c] != 0.0)
                    linEntries.push_back({static_cast<int>(r), invPos[c], jacLin[r * N + c]});
        bconst.assign(N, 0.0);

        // Couplings from the linear elements to fixed-voltage nodes (rail,
        // driven input): residual contribution per volt on each fixed node.
        fixEntries.clear();
        const auto savedX = x;
        const auto savedVal = nodeVal;
        const auto savedCaps = caps;
        const auto savedInds = inds;
        for (auto& c : caps) c.vPrev = c.iPrev = 0.0;
        for (auto& l : inds) l.phiPrev = l.vPrev = 0.0;
        std::fill(x.begin(), x.end(), 0.0);
        for (size_t k = 1; k < nodeFixed.size(); ++k) {
            if (!nodeFixed[k]) continue;
            std::fill(nodeVal.begin(), nodeVal.end(), 0.0);
            nodeVal[k] = 1.0;
            std::fill(f.begin(), f.end(), 0.0);
            stampLinear();
            for (size_t r = 0; r < N; ++r)
                if (f[r] != 0.0) fixEntries.push_back({static_cast<int>(r), static_cast<int>(k), f[r]});
        }
        x = savedX;
        nodeVal = savedVal;
        caps = savedCaps;
        inds = savedInds;
        updateConst();
    }

    // History-dependent constant part of the linear residual.
    void updateConst() noexcept
    {
        std::fill(bconst.begin(), bconst.end(), 0.0);
        for (const auto& c : caps) {
            const double cst = -c.geq * c.vPrev - c.iPrev;
            const int ra = idx[static_cast<size_t>(c.a)], rb = idx[static_cast<size_t>(c.b)];
            if (ra >= 0) bconst[static_cast<size_t>(ra)] += cst;
            if (rb >= 0) bconst[static_cast<size_t>(rb)] -= cst;
        }
        for (const auto& l : inds)
            if (l.isat <= 0.0) bconst[static_cast<size_t>(l.ex)] += -l.phiPrev - h * l.vPrev;
    }

    // Solve jac * dx = -f. Columns are in a fixed min-degree order chosen at
    // prepare time; rows are pivoted numerically. Zero entries are skipped, so
    // cost follows the circuit's sparsity rather than n^3.
    bool solveDynamic() noexcept
    {
        const size_t N = static_cast<size_t>(n);
        for (size_t i = 0; i < N; ++i) { rhs[i] = -f[i]; rp[i] = static_cast<int>(i); }
        size_t nzTop = 0;
        for (size_t c = 0; c < N; ++c) {
            size_t p = c;
            double best = std::abs(jac[static_cast<size_t>(rp[c]) * N + c]);
            for (size_t r = c + 1; r < N; ++r) {
                const double a = std::abs(jac[static_cast<size_t>(rp[r]) * N + c]);
                if (a > best) { best = a; p = r; }
            }
            if (best < 1e-300) return false;
            std::swap(rp[c], rp[p]);
            const double* pr = &jac[static_cast<size_t>(rp[c]) * N];
            const double inv = 1.0 / pr[c];
            nzStart[c] = nzTop;
            for (size_t k = c + 1; k < N; ++k)
                if (pr[k] != 0.0) nzIdx[nzTop++] = static_cast<int>(k);
            nzEnd[c] = nzTop;
            const double bp = rhs[static_cast<size_t>(rp[c])];
            for (size_t r = c + 1; r < N; ++r) {
                double* row = &jac[static_cast<size_t>(rp[r]) * N];
                if (row[c] == 0.0) continue;
                const double m = row[c] * inv;
                for (size_t q = nzStart[c]; q < nzEnd[c]; ++q) {
                    const size_t k = static_cast<size_t>(nzIdx[q]);
                    row[k] -= m * pr[k];
                }
                rhs[static_cast<size_t>(rp[r])] -= m * bp;
            }
        }
        for (size_t ii = N; ii-- > 0;) {
            const double* pr = &jac[static_cast<size_t>(rp[ii]) * N];
            double sum = rhs[static_cast<size_t>(rp[ii])];
            for (size_t q = nzStart[ii]; q < nzEnd[ii]; ++q) {
                const size_t k = static_cast<size_t>(nzIdx[q]);
                sum -= pr[k] * sol[k];
            }
            sol[ii] = sum / pr[ii];
        }
        for (size_t v_ = 0; v_ < N; ++v_) dx[v_] = sol[static_cast<size_t>(colPos[v_])];
        return true;
    }

    // Fast path: pivot sequence and fill pattern frozen from a reference
    // factorisation (buildSymbolic). Returns false if a pivot has become
    // unacceptably small, in which case the caller re-assembles and falls
    // back to solveDynamic.
    bool solveStatic() noexcept
    {
        const size_t N = static_cast<size_t>(n);
        for (size_t i = 0; i < N; ++i) rhs[i] = -f[i];
        for (size_t c = 0; c < N; ++c) {
            const size_t prow = static_cast<size_t>(rpS[c]);
            const double* pr = &jac[prow * N];
            double mx = std::abs(pr[c]);
            for (size_t q = sNzStart[c]; q < sNzEnd[c]; ++q)
                mx = std::max(mx, std::abs(pr[static_cast<size_t>(sNzIdx[q])]));
            if (!(std::abs(pr[c]) > 1e-9 * mx) || !(mx > 1e-290)) return false;
            const double inv = 1.0 / pr[c];
            const double bp = rhs[prow];
            for (size_t e = sElimStart[c]; e < sElimStart[c + 1]; ++e) {
                const size_t rrow = static_cast<size_t>(sElimRows[e]);
                double* row = &jac[rrow * N];
                const double m = row[c] * inv;
                for (size_t q = sNzStart[c]; q < sNzEnd[c]; ++q) {
                    const size_t k = static_cast<size_t>(sNzIdx[q]);
                    row[k] -= m * pr[k];
                }
                rhs[rrow] -= m * bp;
            }
        }
        for (size_t ii = N; ii-- > 0;) {
            const size_t prow = static_cast<size_t>(rpS[ii]);
            const double* pr = &jac[prow * N];
            double sum = rhs[prow];
            for (size_t q = sNzStart[ii]; q < sNzEnd[ii]; ++q) {
                const size_t k = static_cast<size_t>(sNzIdx[q]);
                sum -= pr[k] * sol[k];
            }
            sol[ii] = sum / pr[ii];
        }
        for (size_t v_ = 0; v_ < N; ++v_) dx[v_] = sol[static_cast<size_t>(colPos[v_])];
        return true;
    }


    bool solveLinear() noexcept
    {
        if (staticOk) {
            if (solveStatic()) return true;
            ++fallbacks;
            assembleCurrent();
        }
        return solveDynamic();
    }

    void assembleCurrent() noexcept { if (dc || !fastOk) assemble(); else assembleFast(); }

    // Freeze the pivot order from one numeric factorisation at the stored
    // operating point and derive the elimination lists (including fill-in).
    void buildSymbolic()
    {
        const size_t N = static_cast<size_t>(n);
        staticOk = false;
        dc = false;
        assembleFast();
        if (!solveDynamic()) return;
        rpS = rp;
        std::vector<char> pt = pattern;
        sNzStart.assign(N, 0);
        sNzEnd.assign(N, 0);
        sNzIdx.clear();
        sElimStart.assign(N + 1, 0);
        sElimRows.clear();
        for (size_t c = 0; c < N; ++c) {
            const size_t prow = static_cast<size_t>(rpS[c]);
            sNzStart[c] = sNzIdx.size();
            for (size_t k = c + 1; k < N; ++k)
                if (pt[prow * N + k]) sNzIdx.push_back(static_cast<int>(k));
            sNzEnd[c] = sNzIdx.size();
            sElimStart[c] = sElimRows.size();
            for (size_t r = c + 1; r < N; ++r) {
                const size_t rrow = static_cast<size_t>(rpS[r]);
                if (!pt[rrow * N + c]) continue;
                sElimRows.push_back(static_cast<int>(rrow));
                for (size_t q = sNzStart[c]; q < sNzEnd[c]; ++q)
                    pt[rrow * N + static_cast<size_t>(sNzIdx[q])] = 1;
            }
        }
        sElimStart[N] = sElimRows.size();
        staticOk = true;
    }

    // Pseudo-transient continuation to the DC operating point: supplies are
    // ramped from zero and the circuit is integrated with backward Euler and
    // growing time steps until it stops moving.
    bool settle(double sampleRate)
    {
        const auto target = nodeVal;
        std::fill(x.begin(), x.end(), 0.0);
        std::fill(nodeVal.begin(), nodeVal.end(), 0.0);
        for (auto& c : caps) c.vPrev = c.iPrev = 0.0;
        for (auto& l : inds) l.phiPrev = l.vPrev = 0.0;
        for (auto& q : bjts) q.qPrev = q.iqPrev = 0.0;
        dc = false;
        gmin = 1e-12;
        const double ramp = 0.05, tEnd = 6.0;
        double t = 0.0, dt = 1e-6;
        std::vector<double> xPrev;
        int guard = 0;
        bool good = true;
        while (t < tEnd && guard++ < 40000) {
            xPrev = x;
            const double tn = t + dt;
            const double sc = std::min(1.0, tn / ramp);
            for (size_t k = 0; k < nodeVal.size(); ++k) nodeVal[k] = target[k] * sc;
            sr = 0.5 / dt;
            h = dt;
            for (auto& c : caps) c.geq = c.c / dt;
            if (!newton(60, 1e-8, 1.0)) {
                x = xPrev;
                dt *= 0.5;
                if (dt < 1e-13) { good = false; break; }
                continue;
            }
            for (auto& c : caps) { c.vPrev = v(c.a) - v(c.b); c.iPrev = 0.0; }
            for (auto& l : inds) { l.phiPrev = flux(l, x[static_cast<size_t>(l.ex)]); l.vPrev = 0.0; }
            for (auto& q : bjts) {
                double e, de;
                expLim((q.m.pnp ? -1.0 : 1.0) * (v(q.b) - v(q.e)), e, de);
                q.qPrev = q.m.tf * q.m.is * (e - 1.0);
                q.iqPrev = 0.0;
            }
            t = tn;
            dt = std::min(dt * 1.4, 0.02);
        }
        nodeVal = target;
        sr = sampleRate;
        h = 0.5 / sampleRate;
        for (auto& c : caps) c.geq = 2.0 * c.c * sampleRate;
        return good;
    }

    // Choose the column elimination order (greedy minimum degree on the
    // structural pattern of both the DC and transient Jacobians).
    void orderColumns()
    {
        const size_t N = static_cast<size_t>(n);
        colPos.resize(N);
        for (size_t i = 0; i < N; ++i) colPos[i] = static_cast<int>(i);
        for (size_t i = 0; i < N; ++i) x[i] = 0.1 * std::sin(1.7 * static_cast<double>(i) + 0.3) + 0.4;
        std::vector<char>& pat = pattern;
        pat.assign(N * N, 0);
        for (int mode = 0; mode < 2; ++mode) {
            dc = (mode == 0);
            assemble();
            for (size_t k = 0; k < N * N; ++k) if (jac[k] != 0.0) pat[k] = 1;
        }
        dc = false;
        // symmetric adjacency
        std::vector<std::vector<int>> adj(N);
        for (size_t r = 0; r < N; ++r)
            for (size_t c = 0; c < N; ++c)
                if (r != c && (pat[r * N + c] || pat[c * N + r])) adj[r].push_back(static_cast<int>(c));
        std::vector<char> done(N, 0);
        std::vector<int> order;
        for (size_t step = 0; step < N; ++step) {
            int best = -1; size_t bd = N + 1;
            for (size_t v_ = 0; v_ < N; ++v_) {
                if (done[v_]) continue;
                size_t d = 0;
                for (int w : adj[v_]) if (!done[static_cast<size_t>(w)]) ++d;
                if (d < bd) { bd = d; best = static_cast<int>(v_); }
            }
            done[static_cast<size_t>(best)] = 1;
            order.push_back(best);
            std::vector<int> nb;
            for (int w : adj[static_cast<size_t>(best)]) if (!done[static_cast<size_t>(w)]) nb.push_back(w);
            for (int a : nb)
                for (int b : nb)
                    if (a != b && std::find(adj[static_cast<size_t>(a)].begin(), adj[static_cast<size_t>(a)].end(), b)
                                      == adj[static_cast<size_t>(a)].end())
                        adj[static_cast<size_t>(a)].push_back(b);
        }
        for (size_t pos = 0; pos < N; ++pos) colPos[static_cast<size_t>(order[pos])] = static_cast<int>(pos);
        // re-express the pattern in permuted column space
        std::vector<char> permuted(N * N, 0);
        for (size_t r = 0; r < N; ++r)
            for (size_t v_ = 0; v_ < N; ++v_)
                if (pat[r * N + v_]) permuted[r * N + static_cast<size_t>(colPos[v_])] = 1;
        pat = permuted;
    }

    bool newton(int maxIt, double tol, double stepLim = 5.0) noexcept
    {
        for (int it = 0; it < maxIt; ++it) {
            ++iterCount;
            assembleCurrent();
            if (!solveLinear()) return false;
            double mx = 0.0;
            for (int k = 0; k < nodeUnknowns; ++k) mx = std::max(mx, std::abs(dx[static_cast<size_t>(k)]));
            const double lim = dc ? 2.0 : stepLim;
            const double sc = mx > lim ? lim / mx : 1.0;
            double mi = 0.0;
            for (size_t k = 0; k < static_cast<size_t>(n); ++k) {
                x[k] += sc * dx[k];
                if (static_cast<int>(k) >= nodeUnknowns) mi = std::max(mi, std::abs(dx[k]));
            }
            if (sc == 1.0 && mx < tol && mi < tol * 1e-2) return true;
        }
        return false;
    }

    std::vector<bool> nodeFixed;
    std::vector<double> nodeVal;
    std::vector<int> idx;
    std::vector<Res> res;
    std::vector<Cap> caps;
    std::vector<Ind> inds;
    std::vector<Idl> idls;
    std::vector<Bjt> bjts;
    std::vector<Dio> dios;
    std::vector<Gm> gms;
    std::vector<Leak> leaks;
    std::vector<Buf> bufs;
    struct LinEntry { int row, var; double val; };
    std::vector<LinEntry> linEntries, fixEntries;
    std::vector<int> invPos;
    std::vector<double> xSave, nodePrev;
    std::vector<double> x, xDc, f, dx, jac, rhs, sol, jacLin, bconst;
    std::vector<int> colPos, rp, nzIdx, rpS, sNzIdx, sElimRows;
    std::vector<size_t> sNzStart, sNzEnd, sElimStart;
    std::vector<char> pattern;
    bool staticOk = false, fastOk = false;
    std::vector<size_t> nzStart, nzEnd;
    double sr = 48000.0;
    int n = 0, nodeUnknowns = 0;
    double h = 1e-5, gmin = 1e-12;
    bool dc = false;
};
