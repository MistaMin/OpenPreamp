// Numerical/stability contracts for the fixed reduced topology. Fidelity to the
// full reference is measured separately; this test does not certify equivalence.
#include "DSP/ReducedCircuitBank.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cstdio>
#include <cstring>
#include <vector>

static int checks=0,failures=0;
static void check(bool ok,const char* text){++checks;if(!ok)++failures;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
int main() {
    juce::ScopedNoDenormals noDenormals;
    auto bank=std::make_unique<dsp::ReducedCircuitBank>();
    double worstCurve=0,worstInverse=0,worstPeak=0;
    for(double sessionRate:{44100.,48000.,96000.,192000.}) {
        bank->prepare(sessionRate);
        for(int type:{0,1,2,4})for(int factor:{2,4}) {
            const auto& c=bank->get(type,factor);const double rate=sessionRate*factor;
            for(const auto* clip:{&c.first,&c.second}) {
                double error=0;
                for(int i=0;i<10001;++i){double x=64.*(2.*i/10000.-1);error=std::max(error,std::abs(clip->process(x)-clip->exact(x)));}
                worstCurve=std::max(worstCurve,error);check(error<.0001,"warped clip table tracks its exact bounded curve");
            }
            if(c.spec.inputCore)for(const auto* core:{&c.input,&c.output}) {
                double residual=0;
                for(int i=0;i<10001;++i){const double w=8.*(2.*i/10000.-1),q=core->inverse.lookup(w);residual=std::max(residual,std::abs(core->phi(q)+core->a*q-w));}
                worstInverse=std::max(worstInverse,residual);check(residual<.001,"transformer inverse table satisfies the passive port equation");
            }
            dsp::ReducedCircuitKernel kernel;kernel.configure(c);
            bool silent=true;for(int i=0;i<1024;++i)silent&=kernel.process(0)==0;
            check(silent,"initial silence produces exact silence");
            for(int fixture=0;fixture<4;++fixture) {
                kernel.reset();bool finite=true;double peak=0;juce::Random random(91);
                const int count=int(rate*1.1);
                for(int i=0;i<count;++i){const double t=i/rate;double x=0;
                    if(fixture==0)x=t<.3?50.:t<.6?-50.:0.;
                    if(fixture==1)x=t<.6?50.*std::sin(2*juce::MathConstants<double>::pi*31*t):0.;
                    if(fixture==2)x=t<.6?50.*(2*random.nextDouble()-1):0.;
                    if(fixture==3)x=i==0?1e6:i==1?-1e6:.1*std::sin(2*juce::MathConstants<double>::pi*997*t);
                    const double y=kernel.process(x);finite&=std::isfinite(y);peak=std::max(peak,std::abs(y));
                }
                worstPeak=std::max(worstPeak,peak);check(finite && peak<20,"overload, impulse and recovery stay finite and bounded");
            }
            kernel.reset();double re=0,im=0;const int warm=int(rate*.5),count=int(rate*.1);
            const double w=2*juce::MathConstants<double>::pi*1000/rate;
            for(int i=-warm;i<count;++i){double y=kernel.process(.001*std::sin(w*i));if(i>=0){re+=y*std::cos(w*i);im+=y*std::sin(w*i);}}
            const double amplitude=2*std::hypot(re,im)/count;
            check(std::abs(20*std::log10(amplitude/.001))<.15 && (type==1?im<0:im>0),"quiet 1 kHz response retains unity calibration and polarity");
            dsp::ReducedCircuitKernel blocked,sampled;blocked.configure(c);sampled.configure(c);
            bool identical=true;int offset=0;
            for(int n:{1,7,32,63,128,256}){std::vector<float> a(size_t(n),0),b(size_t(n),0);
                for(int i=0;i<n;++i)a[size_t(i)]=b[size_t(i)]=float(.9*std::sin(.37*(offset+i)));
                blocked.processBlock(a.data(),size_t(n));for(int i=0;i<n;++i)b[size_t(i)]=float(sampled.process(b[size_t(i)]));
                identical&=std::memcmp(a.data(),b.data(),size_t(n)*sizeof(float))==0;offset+=n;}
            check(identical,"irregular blocks preserve sample history exactly");
        }
    }
    std::printf("worst_curve_error=%.9g worst_inverse_residual=%.9g worst_peak=%.9g\n%d checks, %d failures\n",worstCurve,worstInverse,worstPeak,checks,failures);
    return failures?1:0;
}
