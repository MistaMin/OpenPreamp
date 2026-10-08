#pragma once
#include <algorithm>
#include <cmath>

namespace dsp {
// First-order antiderivative antialiasing: the interval-average of f between
// consecutive input samples. One history per channel and nonlinear stage.
// This is for memoryless mappings, not arbitrary stateful circuit equations.
class AdaaStage {
public:
    void reset() noexcept { initialized = false; previous = 0.0; }

    template <class Function, class Primitive>
    double process(double input, Function function, Primitive primitive) noexcept
    {
        if (!initialized) { previous = input; initialized = true; return function(input); }
        const double old = previous;
        previous = input;
        const double delta = input - old;
        const double threshold = 1e-5 * std::max({1.0, std::abs(input), std::abs(old)});
        // Avoid cancellation in the divided difference; the midpoint is its limit.
        if (std::abs(delta) < threshold) return function(0.5 * (input + old));
        return (primitive(input) - primitive(old)) / delta;
    }

    // Numerically evaluate [F(x)-F(old)] / [x-old] directly as an interval
    // integral when a closed-form primitive is unavailable. Eight-point
    // Gauss-Legendre quadrature avoids subtracting large nearly equal F values.
    template <class Function>
    double processNumerical(double input, Function function) noexcept
    {
        if (!initialized) { previous = input; initialized = true; return function(input); }
        const double old = previous;
        previous = input;
        const double mid = 0.5 * (input + old), half = 0.5 * (input - old);
        if (std::abs(half) < 1e-7 * std::max({1.0, std::abs(input), std::abs(old)}))
            return function(mid);
        constexpr double nodes[] = {0.1834346424956498, 0.5255324099163290,
                                    0.7966664774136267, 0.9602898564975363};
        constexpr double weights[] = {0.3626837833783620, 0.3137066458778873,
                                      0.2223810344533745, 0.1012285362903763};
        double sum = 0.0;
        for (int i = 0; i < 4; ++i)
            sum += weights[i] * (function(mid + half * nodes[i]) + function(mid - half * nodes[i]));
        return 0.5 * sum;
    }
private:
    double previous = 0.0;
    bool initialized = false;
};

inline double logCosh(double x) noexcept
{
    const double a = std::abs(x);
    return a + std::log1p(std::exp(-2.0 * a)) - 0.69314718055994530942;
}
} // namespace dsp
