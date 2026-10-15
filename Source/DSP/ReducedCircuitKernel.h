#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include "CurveADAA.h"
#if defined(__aarch64__)
#include <arm_neon.h>
#endif

namespace dsp {
// A fixed topology: nonlinear transformer -> bounded stage -> coupling ->
// bounded stage -> nonlinear transformer -> fitted linear response.
// The transformer constitutive curve follows CircuitSolver's saturating flux
// law, reduced to one source-loaded port. Its implicit map is built offline.
struct ReducedCircuitSpec {
    double inputHz=6, inputFlux=.008, inputRatio=.05;
    double couplingHz=2, couplingChargeHz=0, couplingThreshold=1, couplingGate=1.5, couplingRelease=.02, rail1=1.6, order1=4, bias1=0;
    double rail2=1.4, order2=4, bias2=0;
    double outputHz=1, outputFlux=.08, outputRatio=.1, outputBias=0;
    bool inputCore=true, outputCore=true;
    std::array<double,10> weights{};
};
inline constexpr std::array<double,10> reducedResponsePoles{.3,2,15,60,250,1000,4000,12000,30000,80000};

class ReducedCurve {
public:
    static constexpr int size=2049;
    template<class Function>
    void build(double extent,Function function) {
        limit=extent;warpedLimit=extent/(1+extent);
        scale=(size-1)/(2*warpedLimit);
        for(int i=0;i<size;++i) {
            const double u=-warpedLimit+i/scale;
            values[size_t(i)]=function(u/(1-std::abs(u)));
        }
    }
    double lookup(double x) const noexcept {
        const double u=x/(1+std::abs(x));
        const double position=std::clamp((u+warpedLimit)*scale,0.0,double(size-1));
        const int i=std::min(int(position),size-2);const double t=position-i;
        if(i==0 || i>=size-2)return values[size_t(i)]+t*(values[size_t(i+1)]-values[size_t(i)]);
        const double a=values[size_t(i-1)],b=values[size_t(i)],c=values[size_t(i+1)],d=values[size_t(i+2)];
        return b+0.5*t*(c-a+t*(2*a-5*b+4*c-d+t*(3*(b-c)+d-a)));
    }
    double limit=1;
private:
    alignas(64) std::array<double,size> values{};
    double warpedLimit=.5,scale=1;
};

struct ReducedClip {
    ReducedCurve curve;
    std::unique_ptr<IntegratedCurve> integrated;
    double rail=1,order=4,bias=0,offset=0,slope=1;
    double exact(double x) const noexcept {
        const double v=x+rail*bias, t=std::abs(v/rail);
        const double clip=t>1e6?std::copysign(rail,v):v/std::pow(1+std::pow(t,2*order),1/(2*order));
        return (clip-offset)/slope;
    }
    void prepare(double r,double m,double b,bool antialias=false) {
        rail=r;order=m;bias=b;
        offset=rail*bias/std::pow(1+std::pow(std::abs(bias),2*order),1/(2*order));
        slope=std::pow(1+std::pow(std::abs(bias),2*order),-1-1/(2*order));
        curve.build(64,[this](double x){return exact(x);});
        if(antialias){if(!integrated)integrated=std::make_unique<IntegratedCurve>();integrated->prepare(64,[this](double x){return exact(x);});}
        else integrated.reset();
    }
    double process(double x) const noexcept {return std::abs(x)<curve.limit?curve.lookup(x):exact(x);}
};

struct ReducedTransformer {
    ReducedCurve inverse;
    double a=0,b=0,r=.05,bias=0,origin=0,slope=1,tailSlope=.05,positive=1,negative=-1;
    double phi(double q) const noexcept {return (r*q+(1-r)*(std::tanh(q+bias)-origin))/slope;}
    void prepare(double rate,double hz,double flux,double ratio,double dcBias) {
        a=3.14159265358979323846*hz/rate;b=.5/(rate*flux);r=ratio;bias=dcBias;
        origin=std::tanh(bias);slope=r+(1-r)*(1-origin*origin);
        tailSlope=r/slope+a;positive=(1-r)*(1-origin)/slope;negative=(1-r)*(-1-origin)/slope;
        inverse.build(8,[this](double w){
            if(w==0)return 0.0;
            double lo=(w-std::max(positive,negative))/tailSlope-1;
            double hi=(w-std::min(positive,negative))/tailSlope+1;
            for(int i=0;i<55;++i){const double mid=.5*(lo+hi);if(phi(mid)+a*mid<w)lo=mid;else hi=mid;}
            return .5*(lo+hi);
        });
    }
    double process(double input,double& state) const noexcept {
        const double w=state+b*input;
        const double q=std::abs(w)<inverse.limit?inverse.lookup(w):(w-(w>0?positive:negative))/tailSlope;
        const double output=input-(a/b)*q;
        state+=2*b*output;
        return output;
    }
};

struct ReducedCircuitCoefficients {
    ReducedCircuitSpec spec;
    ReducedClip first,second;
    ReducedTransformer input,output;
    double inputG=0,couplingG=0,outputG=0,couplingA=0,couplingD=0,envelopeRelease=0;
    std::array<double,10> responseG{};
    void prepare(double rate,const ReducedCircuitSpec& model,bool antialias=false) {
        spec=model;
        first.prepare(model.rail1,model.order1,model.bias1,antialias);second.prepare(model.rail2,model.order2,model.bias2,antialias);
        if(model.inputCore)input.prepare(rate,model.inputHz,model.inputFlux,model.inputRatio,0);
        if(model.outputCore)output.prepare(rate,model.outputHz,model.outputFlux,model.outputRatio,model.outputBias);
        auto g=[rate](double hz){const double t=3.14159265358979323846*hz/rate;return t/(1+t);};
        inputG=g(model.inputHz);couplingG=g(model.couplingHz);outputG=g(model.outputHz);
        couplingA=3.14159265358979323846*model.couplingHz/rate;
        couplingD=3.14159265358979323846*model.couplingChargeHz/rate;
        envelopeRelease=std::exp(-1/(rate*model.couplingRelease));
        for(size_t i=0;i<responseG.size();++i)responseG[i]=g(reducedResponsePoles[i]);
    }
};

class ReducedCircuitKernel {
public:
    void configure(const ReducedCircuitCoefficients& prepared,bool antialias=false) noexcept {
        coefficients=&prepared;useADAA=antialias && prepared.first.integrated && prepared.second.integrated;reset();
    }
    void reset() noexcept {inputState=couplingState=outputState=envelope=0;responseStates.fill(0);firstADAA.reset();secondADAA.reset();}
    double process(double x) noexcept {
        if(!std::isfinite(x)){reset();return 0;}
        const auto& c=*coefficients;
        double v=c.spec.inputCore?c.input.process(x,inputState):highpass(x,inputState,c.inputG);
        v=useADAA?firstADAA.process(v,*c.first.integrated):c.first.process(v);
        if(c.spec.couplingChargeHz>0) {
            // Reduced transistor input loading: a passive quadratic port has
            // an explicit trapezoidal solution, with no runtime Newton solve.
            envelope=std::max(std::abs(x),envelope*c.envelopeRelease);
            const double gate=std::clamp((envelope-c.spec.couplingGate)/c.spec.couplingGate,0.0,1.0);
            const double D=c.couplingD*gate*gate*(3-2*gate);
            const double rhs=v-couplingState,A=1+c.couplingA;
            double coupled=rhs/A;
            if(coupled>c.spec.couplingThreshold) {
                const double delta=rhs-A*c.spec.couplingThreshold;
                coupled=c.spec.couplingThreshold+2*delta/(A+std::sqrt(A*A+4*D*delta));
            }
            couplingState=2*(v-coupled)-couplingState;v=coupled;
        } else v=highpass(v,couplingState,c.couplingG);
        v=useADAA?secondADAA.process(v,*c.second.integrated):c.second.process(v);
        v=c.spec.outputCore?c.output.process(v,outputState):highpass(v,outputState,c.outputG);
        return response(v,c);
    }
    void processBlock(float* samples,size_t count) noexcept {
        for(size_t i=0;i<count;++i)samples[i]=float(process(samples[i]));
    }
private:
    double response(double v,const ReducedCircuitCoefficients& c) noexcept {
        double y=0;
#if defined(__aarch64__) && !defined(OPENPREAMP_REDUCED_SCALAR)
        const auto input=vdupq_n_f64(v);
        auto sum=vdupq_n_f64(0);
        for(size_t i=0;i<responseStates.size();i+=2) {
            const auto state=vld1q_f64(responseStates.data()+i);
            const auto delta=vmulq_f64(vsubq_f64(input,state),vld1q_f64(c.responseG.data()+i));
            const auto low=vaddq_f64(delta,state);
            vst1q_f64(responseStates.data()+i,vaddq_f64(low,delta));
            sum=vaddq_f64(sum,vmulq_f64(vsubq_f64(input,low),vld1q_f64(c.spec.weights.data()+i)));
        }
        y=vaddvq_f64(sum);
#else
        for(size_t i=0;i<responseStates.size();++i)y+=c.spec.weights[i]*highpass(v,responseStates[i],c.responseG[i]);
#endif
        return y;
    }
    static double highpass(double x,double& state,double g) noexcept {
        const double v=(x-state)*g,low=v+state;state=low+v;return x-low;
    }
    const ReducedCircuitCoefficients* coefficients=nullptr;
    bool useADAA=false;
    CurveADAA firstADAA,secondADAA;
    double inputState=0,couplingState=0,outputState=0,envelope=0;
    std::array<double,10> responseStates{};
};
}
