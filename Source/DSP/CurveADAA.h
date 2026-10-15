#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace dsp {
// A warped knot grid with interpolation linear in actual input volts. Its
// primitive integrates that exact interpolant, not a separately fitted curve.
// Tables are immutable after prepare; each audio channel owns only its history.
class IntegratedCurve {
public:
    static constexpr int size=2049,center=(size-1)/2;
    struct Evaluation{double value,integral;int segment;};
    template<class Function>
    void prepare(double extent,Function function) {
        limit=extent;warpLimit=extent/(1+extent);scale=(size-1)/(2*warpLimit);
        for(int i=0;i<size;++i){const double u=(i-center)/scale;knots[size_t(i)]=u/(1-std::abs(u));values[size_t(i)]=function(knots[size_t(i)]);}
        knots[0]=-extent;knots[size-1]=extent;
        for(int i=0;i<size-1;++i)slopes[size_t(i)]=(values[size_t(i+1)]-values[size_t(i)])/(knots[size_t(i+1)]-knots[size_t(i)]);
        integrals[center]=0;
        for(int i=center+1;i<size;++i)integrals[size_t(i)]=integrals[size_t(i-1)]+.5*(values[size_t(i-1)]+values[size_t(i)])*(knots[size_t(i)]-knots[size_t(i-1)]);
        for(int i=center-1;i>=0;--i)integrals[size_t(i)]=integrals[size_t(i+1)]-.5*(values[size_t(i)]+values[size_t(i+1)])*(knots[size_t(i+1)]-knots[size_t(i)]);
    }
    double value(double x) const noexcept {
        if(x<=-limit)return values.front();if(x>=limit)return values.back();
        const size_t i=interval(x);return values[i]+(x-knots[i])*slopes[i];
    }
    Evaluation evaluate(double x) const noexcept {
        if(x<=-limit)return {values.front(),integrals.front()+(x+limit)*values.front(),-1};
        if(x>=limit)return {values.back(),integrals.back()+(x-limit)*values.back(),size-1};
        const size_t i=interval(x);const double dx=x-knots[i];
        return {values[i]+dx*slopes[i],integrals[i]+dx*(values[i]+.5*dx*slopes[i]),int(i)};
    }
    double average(double x,double old) const noexcept {
        const double delta=x-old;
        if(std::abs(delta)<1e-5*std::max({1.,std::abs(x),std::abs(old)}))return value(.5*x+.5*old);
        // Within a single linear segment, its midpoint is the exact average
        // and avoids subtracting nearly equal primitive values.
        if(std::abs(x)<limit && std::abs(old)<limit && interval(x)==interval(old))return value(.5*x+.5*old);
        return (primitive(x)-primitive(old))/delta;
    }
    double primitive(double x) const noexcept {
        if(x<=-limit)return integrals.front()+(x+limit)*values.front();
        if(x>=limit)return integrals.back()+(x-limit)*values.back();
        const size_t i=interval(x);const double dx=x-knots[i];
        return integrals[i]+dx*(values[i]+.5*dx*slopes[i]);
    }
    double lowerSafetyBound() const noexcept {return 4*values.front();}
    double upperSafetyBound() const noexcept {return 4*values.back();}
private:
    size_t interval(double x) const noexcept {
        const double u=x/(1+std::abs(x));
        return size_t(std::clamp(int((u+warpLimit)*scale),0,size-2));
    }
    alignas(64) std::array<double,size> knots{},values{},integrals{};
    std::array<double,size-1> slopes{};
    double limit=1,warpLimit=.5,scale=1;
};

class CurveADAA {
public:
    void reset() noexcept {initialized=false;previous=0;}
    double process(double input,const IntegratedCurve& curve) noexcept {
        const auto current=curve.evaluate(input);
        if(!initialized){previous=input;cached=current;initialized=true;return current.value;}
        const double old=previous;const auto prior=cached;previous=input;cached=current;
        // ADAA of r(x)=f(x)-x, with the identity path left unaveraged:
        // x + average(r) = average(f) + (x-old)/2. This preserves quiet HF
        // response instead of imposing the averaging filter on the linear path.
        const double delta=input-old;
        double average=0;
        if(current.segment==prior.segment)average=.5*current.value+.5*prior.value;
        else if(std::abs(delta)<1e-5*std::max({1.,std::abs(input),std::abs(old)}))average=curve.value(.5*input+.5*old);
        else average=(current.integral-prior.integral)/delta;
        // Residual ADAA can overshoot on extreme discontinuities. A safety range
        // beyond normal saturation bounds these events; it is not a claim of
        // circuit-equivalent overload. Stateful transformer ports keep their
        // passive implicit update, not a memoryless ADAA substitution.
        return std::clamp(average+.5*delta,curve.lowerSafetyBound(),curve.upperSafetyBound());
    }
private:
    bool initialized=false;
    double previous=0;
    IntegratedCurve::Evaluation cached{};
};
}
