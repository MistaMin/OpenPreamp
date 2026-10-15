#pragma once
#include <array>
#include <algorithm>
#include <cmath>

// Two-pole Butterworth cuts. All coefficients/state and smoothing use the host rate.
class SessionCuts {
public:
    void prepare(double rate) {
        sampleRate = rate; hp = hpTarget; lp = lpTarget; phase = 0; moving=false;
        slew = 1.0 - std::exp(-1.0 / (0.01 * rate)); reset(); updateCoefficients();
    }
    void reset() { for (auto& b : high) b.reset(); for (auto& b : low) b.reset(); }
    void setFrequencies(double highPass, double lowPass) {
        moving=moving || std::abs(highPass-hpTarget)>0 || std::abs(lowPass-lpTarget)>0;
        hpTarget=highPass;lpTarget=lowPass;
    }
    void tick() {
        if(!moving)return;
        hp += slew * (hpTarget - hp); lp += slew * (lpTarget - lp);
        const bool settled=std::abs(hp-hpTarget)<1e-7*hpTarget && std::abs(lp-lpTarget)<1e-7*lpTarget;
        if(settled){hp=hpTarget;lp=lpTarget;moving=false;}
        if (++phase == 16 || settled) { phase = 0; updateCoefficients(); }
    }
    float process(int channel, float x) {
        return low[size_t(channel)].process(high[size_t(channel)].process(x, hpCoeffs), lpCoeffs);
    }
    double getSampleRate() const { return sampleRate; }
private:
    struct Biquad {
        double z1 = 0, z2 = 0;
        void reset() { z1 = z2 = 0; }
        float process(double x, const std::array<double,5>& c) {
            const double y = c[0]*x+z1;
            z1 = c[1]*x-c[3]*y+z2; z2 = c[2]*x-c[4]*y;
            return float(y);
        }
    };
    std::array<Biquad,2> high, low;
    std::array<double,5> hpCoeffs{}, lpCoeffs{};
    double sampleRate = 48000, hp = 20, lp = 20000, hpTarget = 20, lpTarget = 20000, slew = 0;
    int phase = 0;
    bool moving=false;
    std::array<double,5> coefficients(double frequency, bool highPass) const {
        const double w = 6.283185307179586 * std::clamp(frequency, 5.0, sampleRate * 0.475) / sampleRate;
        const double c = std::cos(w), alpha = std::sin(w) / std::sqrt(2.0), norm = 1.0 / (1.0+alpha);
        const double b = highPass ? (1+c)*0.5 : (1-c)*0.5;
        return {b*norm, (highPass ? -2*b : 2*b)*norm, b*norm, -2*c*norm, (1-alpha)*norm};
    }
    void updateCoefficients() { hpCoeffs = coefficients(hp,true); lpCoeffs = coefficients(lp,false); }
};
