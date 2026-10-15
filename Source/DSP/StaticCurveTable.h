#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace dsp {
// Fixed transfer curves built during preparation, shared by every instance.
// Out-of-domain values use the original curve; never silently clamp input.
// Numerical ADAA still integrates the interpolated curve with the same nodes
// and weights as the reference path, preserving its interval approximation.
template <std::size_t Size>
class StaticCurveTable {
    static_assert(Size > 1);
public:
    template <class Curve>
    void build(double extent, Curve curve) {
        limit = extent;
        scale = double(Size - 1) / (2 * extent);
        for (std::size_t i = 0; i < Size; ++i)
            values[i] = curve(-extent + double(i) / scale);
    }
    template <class Curve>
    double evaluate(double x, Curve fallback) const noexcept {
        if (!(x > -limit && x < limit)) return fallback(x);
        const double index = (x + limit) * scale;
        const auto i = std::min(std::size_t(index), Size - 2);
        const double fraction = index - double(i);
        return values[i] + fraction * (values[i + 1] - values[i]);
    }
private:
    std::array<double, Size> values{};
    double limit = 1, scale = 1;
};
}
