#pragma once
#include <algorithm>
#include <cmath>

// One-pole, bilinear-transform high-pass on Side only, at the host sample rate.
class SessionMonoMaker {
public:
    void prepare(double rate, double cutoff) {
        sampleRate = rate; frequency = target = cutoff;moving=false;
        slew = 1.0 - std::exp(-1.0 / (0.01 * rate)); reset();updateCoefficient();
    }
    void reset() { state = 0.0; }
    void setFrequency(double cutoff) {moving=moving || std::abs(cutoff-target)>0;target=cutoff;}
    float process(float side) {
        if(moving) {
            frequency += slew * (target - frequency);
            if(std::abs(frequency-target)<1e-7*target){frequency=target;moving=false;}
            updateCoefficient();
        }
        const double v = (double(side) - state) * coefficient;
        const double low = v + state;
        state = low + v;
        return float(double(side) - low);
    }
private:
    void updateCoefficient() {
        const double g=std::tan(3.141592653589793 * std::clamp(frequency,20.0,std::min(500.0,sampleRate*.475))/sampleRate);
        coefficient=g/(1+g);
    }
    double sampleRate = 48000, frequency = 20, target = 20, slew = 0, state = 0, coefficient = 0;
    bool moving=false;
};
