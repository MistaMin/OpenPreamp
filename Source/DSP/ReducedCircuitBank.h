#pragma once
#include "ReducedCircuitKernel.h"
#include "ReducedCircuitData.h"

namespace dsp {
class ReducedCircuitBank {
public:
    void prepare(double sessionRate) {
        for(int rateIndex=0;rateIndex<3;++rateIndex)for(int model=0;model<4;++model) {
            const auto& data=reduced_data::models[size_t(model)];const auto& p=data.parameters;
            ReducedCircuitSpec spec;
            spec.inputHz=p[0];spec.inputFlux=std::exp(p[1]);spec.inputRatio=.05;
            spec.couplingHz=p[2];spec.rail1=p[3];spec.order1=p[4];spec.bias1=p[5];
            spec.rail2=p[6];spec.order2=p[7];spec.bias2=p[8];
            spec.outputHz=p[9];spec.outputFlux=std::exp(p[10]);spec.outputRatio=model==1?.075:.1;spec.outputBias=p[11];
            spec.inputCore=spec.outputCore=model!=0;spec.weights=data.weights;
            if(model==1){spec.couplingChargeHz=std::exp(p[12]);spec.couplingThreshold=p[13];spec.couplingGate=p[14];spec.couplingRelease=std::exp(p[15]);}
            coefficients[size_t(rateIndex)][size_t(model)].prepare(sessionRate*(1<<rateIndex),spec,rateIndex==0);
        }
    }
    const ReducedCircuitCoefficients& get(int type,int factor) const noexcept {
        const int model=type==4?3:std::clamp(type,0,2);
        return coefficients[size_t(factor==4?2:factor==2?1:0)][size_t(model)];
    }
private:
    std::array<std::array<ReducedCircuitCoefficients,4>,3> coefficients;
};
}
