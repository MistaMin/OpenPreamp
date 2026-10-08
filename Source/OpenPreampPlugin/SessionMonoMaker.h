#pragma once
#include <algorithm>
#include <cmath>

// One-pole, bilinear-transform high-pass on Side only, at the host sample rate.
class SessionMonoMaker {
public:
    void prepare(double rate, double cutoff) {
        sampleRate = rate; frequency = target = cutoff;
        slew = 1.0 - std::exp(-1.0 / (0.01 * rate)); reset();
    }
    void reset() { state = 0.0; }
    void setFrequency(double cutoff) { target = cutoff; }
    float process(float side) {
        frequency += slew * (target - frequency);
        const double g = std::tan(3.141592653589793 * std::clamp(frequency, 20.0, std::min(500.0, sampleRate * 0.475)) / sampleRate);
        const double v = (double(side) - state) * g / (1.0 + g);
        const double low = v + state;
        state = low + v;
        return float(double(side) - low);
    }
private:
    double sampleRate = 48000, frequency = 20, target = 20, slew = 0, state = 0;
};
