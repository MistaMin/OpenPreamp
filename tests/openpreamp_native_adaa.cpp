#include "ReducedCircuitBank.h"
#include <complex>
#include <cstdio>
#include <random>
#include <vector>

static int checks=0,failures=0;
static void check(bool ok,const char* text){++checks;if(!ok)++failures;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
static void fft(std::vector<std::complex<double>>& a){
    const size_t n=a.size();for(size_t i=1,j=0;i<n;++i){size_t bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(a[i],a[j]);}
    for(size_t len=2;len<=n;len*=2){const auto root=std::polar(1.,-6.283185307179586/double(len));for(size_t start=0;start<n;start+=len){std::complex<double> w=1;for(size_t j=0;j<len/2;++j){const auto u=a[start+j],v=w*a[start+j+len/2];a[start+j]=u+v;a[start+j+len/2]=u-v;w*=root;}}}
}
static double alias(const dsp::ReducedCircuitCoefficients& c,bool aa){
    dsp::ReducedCircuitKernel kernel;kernel.configure(c,aa);
    constexpr int n=8192,tone=1501;std::vector<std::complex<double>> spectrum(size_t(n),0);
    for(int i=0;i<3*n;++i){const double x=2.8*std::sin(6.283185307179586*tone*(i%n)/n),y=kernel.process(x);if(i>=2*n)spectrum[size_t(i-2*n)]=y;}
    fft(spectrum);double energy=0;
    for(int i=3;i<n/2;++i)if(std::abs(i-tone)>2 && std::abs(i-2*tone)>2)energy+=std::norm(spectrum[size_t(i)]);
    return energy/std::norm(spectrum[tone]);
}
static double primitive(double x){const double a=std::abs(1.3*x);return .7/1.3*(a+std::log1p(std::exp(-2*a))-.69314718055994530942);}
static std::complex<double> quietResponse(const dsp::ReducedCircuitCoefficients& c,bool aa){
    dsp::ReducedCircuitKernel kernel;kernel.configure(c,aa);constexpr int n=2048,tone=853;std::complex<double> fundamental=0;
    for(int i=0;i<4*n;++i){const double phase=6.283185307179586*tone*(i%n)/n,y=kernel.process(.0001*std::sin(phase));if(i>=3*n)fundamental+=y*std::polar(1.,-phase);}
    return fundamental;
}
int main(){
    dsp::IntegratedCurve curve;curve.prepare(64,[](double x){return .7*std::tanh(1.3*x);});
    std::mt19937 random(19);std::uniform_real_distribution<double> input(-4,4);double error=0;
    for(int i=0;i<10000;++i){const double x=input(random),old=input(random);const double exact=std::abs(x-old)<1e-4?.7*std::tanh(1.3*.5*(x+old)):(primitive(x)-primitive(old))/(x-old);error=std::max(error,std::abs(exact-curve.average(x,old)));}
    std::printf("maximum primitive-average curve approximation error %.9g\n",error);check(error<3e-5,"integrated lookup tracks analytic tanh interval averages");
    bool bounded=true;for(double x:{-1e9,-64.,-.000001,0.,.000001,64.,1e9})for(double old:{-1e9,-64.,-.000001,0.,.000001,64.,1e9}){const double y=curve.average(x,old);bounded&=std::isfinite(y)&&std::abs(y)<=.70000001;}
    check(bounded,"near-equal, zero-crossing, endpoint and extreme-jump averages remain bounded");
    auto bank=std::make_unique<dsp::ReducedCircuitBank>();
    for(double rate:{44100.,48000.,96000.,192000.}){
        bank->prepare(rate);
        for(int type:{0,1,2,4}){
            const auto& c=bank->get(type,1);dsp::ReducedCircuitKernel kernel;kernel.configure(c,true);
            const auto transfer=quietResponse(c,true)/quietResponse(c,false);
            check(std::abs(20*std::log10(std::abs(transfer)))<.02 && std::abs(std::arg(transfer))<.002,"residual ADAA preserves quiet near-Nyquist gain and phase");
            bool finite=true;double peak=0;
            for(int i=0;i<20000;++i){const double x=i==0?1e6:i==1?-1e6:i<10000?50*std::sin(.43*i):0;const double y=kernel.process(x);finite&=std::isfinite(y);peak=std::max(peak,std::abs(y));}
            check(finite&&peak<20,"native ADAA Circuit remains stable through extreme drive and recovery");
            dsp::ReducedCircuitKernel blocked,sampled;blocked.configure(c,true);sampled.configure(c,true);bool identical=true;int offset=0;
            for(int n:{1,7,32,63,128,257}){std::vector<float> a(size_t(n),0);for(int i=0;i<n;++i)a[size_t(i)]=float(2*std::sin(.37*(offset+i)));auto b=a;blocked.processBlock(a.data(),a.size());for(size_t i=0;i<b.size();++i)b[i]=float(sampled.process(b[i]));identical&=a==b;offset+=n;}
            check(identical,"native ADAA histories are independent of arbitrary buffer partitioning");
            if(rate<=48000){const double plain=alias(c,false),aa=alias(c,true),improvement=10*std::log10(plain/aa);std::printf("rate=%.0f type=%d plain_alias_db=%.4f adaa_alias_db=%.4f improvement_db=%.4f\n",rate,type,10*std::log10(plain),10*std::log10(aa),improvement);check(std::isfinite(improvement)&&improvement>=1,"Circuit ADAA reduces high-frequency folded energy by at least 1 dB");}
        }
    }
    std::printf("%d checks, %d failures\n",checks,failures);return failures?1:0;
}
