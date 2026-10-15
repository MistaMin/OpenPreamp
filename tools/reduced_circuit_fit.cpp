// Native evaluator for offline constrained fitting. No plug-in/SDK dependency.
#include "../Source/DSP/ReducedCircuitKernel.h"
extern "C" void openpreamp_reduced_fit(const double* input,double* output,int count,double rate,const double* p,const double* weights,int model) {
    dsp::ReducedCircuitSpec spec;
    spec.inputHz=p[0];spec.inputFlux=std::exp(p[1]);spec.inputRatio=.05;
    spec.couplingHz=p[2];spec.rail1=p[3];spec.order1=p[4];spec.bias1=p[5];
    spec.rail2=p[6];spec.order2=p[7];spec.bias2=p[8];
    spec.outputHz=p[9];spec.outputFlux=std::exp(p[10]);spec.outputRatio=model==1?.075:.1;spec.outputBias=p[11];
    spec.inputCore=spec.outputCore=model!=0;
    if(model==1){spec.couplingChargeHz=std::exp(p[12]);spec.couplingThreshold=p[13];spec.couplingGate=p[14];spec.couplingRelease=std::exp(p[15]);}
    for(size_t i=0;i<spec.weights.size();++i)spec.weights[i]=weights[i];
    dsp::ReducedCircuitCoefficients prepared;prepared.prepare(rate,spec);
    dsp::ReducedCircuitKernel kernel;kernel.configure(prepared);
    for(int i=0;i<count;++i)output[i]=kernel.process(input[i]);
}
