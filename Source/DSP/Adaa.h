#pragma once

#include <algorithm>
#include <array>
#include <cmath>

// First-order antiderivative anti-aliasing (ADAA) for the static preamp models.
//
// A memoryless nonlinearity y = f(x) creates harmonics that fold back below Nyquist as aliasing. First-order ADAA
// replaces f(x_n) with the average of f over the straight line from x_{n-1} to x_n, which is
// (F(x_n) - F(x_{n-1})) / (x_n - x_{n-1}) with F the antiderivative of f. That band-limits the nonlinearity's
// output without extra sample rate. See Residual below for how it is kept transparent for clean signals.
namespace dsp::adaa {

// ln(cosh(x)), the antiderivative of tanh, without overflow.
inline double logCosh(double x)
{
    const double a = std::abs(x);
    return a + std::log1p(std::exp(-2.0 * a)) - 0.6931471805599453;
}

// Antiderivative of the "core" saturator f(v) = v / sqrt(1 + (v/sat)^2):  sat^2 * sqrt(1 + (v/sat)^2).
inline double coreSaturateF(double v, double sat)
{
    const double r = v / sat;
    return sat * sat * std::sqrt(1.0 + r * r);
}

// The rail limiter f(v) = v / (1 + |v/rail|^8)^(1/8) has no closed-form antiderivative. G(t) = integral of
// t / (1 + t^8)^(1/8) from 0 to t is tabulated once and read back with cubic Hermite interpolation (the slope
// is known exactly), accurate to ~1e-11, which is far below the differences ADAA subtracts.
class RailTable {
public:
    RailTable()
    {
        for (int i = 0; i <= kSize; ++i) {
            const double t = i * kStep;
            fn[static_cast<size_t>(i)] = t / std::pow(1.0 + std::pow(t, 8.0), 0.125);
        }
        G[0] = 0.0;
        for (int i = 0; i < kSize; ++i) {                 // Simpson on each interval, 16 sub-steps
            double acc = 0.0;
            const double t0 = i * kStep, h = kStep / 16.0;
            for (int k = 0; k < 16; ++k) {
                const double a = t0 + k * h, m = a + 0.5 * h, b = a + h;
                acc += h / 6.0 * (value(a) + 4.0 * value(m) + value(b));
            }
            G[static_cast<size_t>(i) + 1] = G[static_cast<size_t>(i)] + acc;
        }
    }

    static double value(double t) { return t / std::pow(1.0 + std::pow(t, 8.0), 0.125); }

    // integral of value() from 0 to t, t >= 0
    double integral(double t) const
    {
        if (t >= kMax) return G[kSize] + (t - kMax);       // the limiter is flat (slope 1) beyond the table
        const double u = t / kStep;
        const int i = std::min(static_cast<int>(u), kSize - 1);
        const double s = u - i, s2 = s * s, s3 = s2 * s;
        const double g0 = G[static_cast<size_t>(i)], g1 = G[static_cast<size_t>(i) + 1];
        const double f0 = fn[static_cast<size_t>(i)], f1 = fn[static_cast<size_t>(i) + 1];
        return (2 * s3 - 3 * s2 + 1) * g0 + (s3 - 2 * s2 + s) * kStep * f0 + (-2 * s3 + 3 * s2) * g1 + (s3 - s2) * kStep * f1;
    }

private:
    static constexpr int kSize = 4096;
    static constexpr double kMax = 8.0, kStep = kMax / kSize;
    std::array<double, kSize + 1> G{}, fn{};
};

inline const RailTable& railTable()
{
    static const RailTable table;          // built on first use; PreampEngine::prepare() touches it
    return table;
}

// Antiderivative of railLimit(v, rail) = v / (1 + |v/rail|^8)^(1/8).
inline double railLimitF(double v, double rail)
{
    return rail * rail * railTable().integral(std::abs(v) / rail);
}

// Antiderivative of asymLimit(v, posRail, negRail) (the rail depends on the sign of v; F is continuous at 0).
inline double asymLimitF(double v, double posRail, double negRail)
{
    return railLimitF(v, v >= 0.0 ? posRail : negRail);
}

// Transistor leg: input v satisfies v = w + kappa (e^w - 1) with I = e^w (the relative collector current, what
// the preamp's solveLeg returns). Since dv = (1/I + kappa) dI, the integral of I dv is I + kappa I^2 / 2.
inline double legF(double I, double kappa)
{
    return I + 0.5 * kappa * I * I;
}

// Whether a stage runs ADAA: Off returns f(x) untouched; Hold keeps the stage's timing consistent without the
// antiderivative (used while a gain control is moving, when the antiderivative would be for a different gain);
// On is the full method.
enum class Mode { Off, Hold, On };

// One memoryless stage y = f(x) with antiderivative F and small-signal slope `slope` (f(x) ~ slope * x near 0).
//
// Plain first-order ADAA replaces f(x_n) with the mean of f over the straight line from x_{n-1} to x_n,
// (F(x_n) - F(x_{n-1})) / (x_n - x_{n-1}). That is a half-sample delay and a loss of top end (about -2 dB at 10 kHz
// at 48 kHz) even for a linear f, and a clean preamp must not lose it. So the stage is split:
//   y = slope * x  +  r(x),   r(x) = f(x) - slope * x     (r is zero while the stage is linear)
// ADAA is applied to the residual r only (its mean over the step, which is also delayed by half a sample), and the
// linear part goes through a first-order all-pass with the same half-sample delay at low frequencies: unity
// magnitude, so quiet signals keep their level at every frequency, and no antiderivative is needed for them.
// As the stage compresses, the weight w (a smooth function of the compression at this sample) fades the linear part
// from the all-pass to the same two-sample average the residual uses, which makes the result the plain ADAA mean:
// in deep saturation the two parts then cancel exactly and nothing is left over from the all-pass's phase error.
struct Residual {
    static constexpr double kKnee = 0.3;           // compression (1 - f / (slope x)) at which the weight is one half

    double x1 = 0.0, F1 = 0.0, r1 = 0.0, apx = 0.0, apy = 0.0;
    bool primed = false;

    void reset() noexcept { primed = false; }

    double step(double x, double f, double F, double slope, Mode mode) noexcept
    {
        if (mode == Mode::Off) {
            primed = false;
            return f;
        }
        const double lin_in = slope * x;
        if (!primed) apx = apy = lin_in;
        const double lin = (1.0 / 3.0) * (lin_in - apy) + apx;       // all-pass (a + z^-1) / (1 + a z^-1), a = 1/3
        apx = lin_in;
        apy = lin;

        const double r = f - lin_in;
        const double Fr = F - 0.5 * slope * x * x;
        double result = lin + r;
        if (mode == Mode::On && primed) {
            const double dx = x - x1;
            if (std::abs(dx) > 1.0e-5) {                          // below that, r(x) is the same to ~1e-5
                double meanR = (Fr - F1) / dx;
                // The mean of r over [x1, x] stays near its end values; where the antiderivatives are huge and
                // subtract to rounding noise (extreme overdrive) fall back to the plain value.
                const double lo = std::min(r, r1), hi = std::max(r, r1), tol = (hi - lo) + 1.0e-6 * (1.0 + std::abs(r));
                if (!(meanR >= lo - tol && meanR <= hi + tol)) meanR = r;
                const double c = std::abs(x) > 1.0e-9 ? 1.0 - f / lin_in : 0.0;
                const double c2 = c * c, c4 = c2 * c2, c8 = c4 * c4, k2 = kKnee * kKnee, k4 = k2 * k2;
                const double w = c8 / (c8 + k4 * k4);
                result = lin + meanR + w * (0.5 * slope * (x + x1) - lin);
            }
        }
        x1 = x; F1 = Fr; r1 = r; primed = true;
        return result;
    }
};

} // namespace dsp::adaa
