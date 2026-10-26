#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>

namespace openpreamp {
// Choice indices 0/1 deliberately retain the Peak/RMS values in older sessions.
enum MeterMode { Peak, RMS, VU, K12, K14, K20, DIN, Nordic, BBC, EBU };
enum Weighting { Z, A, B, C, D, K };
inline constexpr const char* modeNames[] = {"Peak", "RMS", "VU", "K-12", "K-14", "K-20", "DIN", "Nordic", "BBC", "EBU"};
inline bool isRMS(int mode) { return mode == RMS || (mode >= K12 && mode <= K20); }
inline double reference(int mode) { return mode == K12 ? -12 : mode == K14 ? -14 : mode == K20 ? -20 : -18; }
inline float db(double x) { return x > 1.e-10 ? float(20 * std::log10(x)) : -99.f; }

class WeightingFilter {
    struct Section {
        double b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
        double tick(double x) { const double y=b0*x+z1; z1=b1*x-a1*y+z2; z2=b2*x-a2*y; return y; }
        std::complex<double> response(double w) const {
            const auto z=std::polar(1.,-w); return (b0+b1*z+b2*z*z)/(1.+a1*z+a2*z*z);
        }
    };
    std::array<Section,3> stages{};
    int count=0;
    void analog(double fs, double b2,double b1,double b0,double a2,double a1,double a0) {
        const double t=2*fs, den=a2*t*t+a1*t+a0;
        stages[size_t(count++)]={(b2*t*t+b1*t+b0)/den,(-2*b2*t*t+2*b0)/den,
            (b2*t*t-b1*t+b0)/den,(-2*a2*t*t+2*a0)/den,(a2*t*t-a1*t+a0)/den};
    }
    // Inverse bilinear transform of the normative 48 kHz BS.1770 coefficients.
    void from48(double fs, double b0,double b1,double b2,double a1,double a2) {
        const double t=96000;
        analog(fs,(b0-b1+b2)/(t*t),2*(b0-b2)/t,b0+b1+b2,
            (1-a1+a2)/(t*t),2*(1-a2)/t,1+a1+a2);
    }
public:
    void prepare(double fs,int curve) {
        stages={};count=0;
        constexpr double pi=3.14159265358979323846;
        if(curve==K) {
            from48(fs,1.53512485958697,-2.69169618940638,1.19839281085285,-1.69065929318241,.73248077421585);
            from48(fs,1,-2,1,-1.99004745483398,.99007225036621);
        } else if(curve>=A && curve<=C) {
            const double lo=2*pi*20.598997,hi=2*pi*12194.217;
            analog(fs,1,0,0,1,2*lo,lo*lo);
            if(curve==A) analog(fs,1,0,0,1,2*pi*(107.65265+737.86223),4*pi*pi*107.65265*737.86223);
            if(curve==B) analog(fs,0,1,0,0,1,2*pi*158.489319);
            analog(fs,0,0,hi*hi,1,2*hi,hi*hi);
        } else if(curve==D) {
            // IEC 537 aircraft-noise weighting, in rad/s; normalized at 1 kHz.
            analog(fs,0,1,0,0,1,1776.3);
            analog(fs,1,6532,40975000,1,21514,388360000);
            analog(fs,0,0,7288.5,0,1,7288.5);
        }
        if(curve!=Z && curve!=K) {
            std::complex<double> h=1.;for(int i=0;i<count;++i)h*=stages[size_t(i)].response(2*pi*1000/fs);
            const double gain=1/std::abs(h);stages[0].b0*=gain;stages[0].b1*=gain;stages[0].b2*=gain;
        }
    }
    double tick(double x) { for(int i=0;i<count;++i)x=stages[size_t(i)].tick(x);return x; }
};

// Audio-clocked detectors. No allocation, locks, or GUI timing in push(). Each
// input/output tap owns its filter histories; channels never share detector state.
class Metering {
    struct Channel { WeightingFilter filter; double power=0,rectified=0,vu=0,ppm=0,peak=0; };
    std::array<Channel,2> channels;
    std::array<std::atomic<float>,2> published{{-99,-99}};
    double rate=48000,rmsC=0,vuC=0,attackC=0,releaseC=0,peakC=0;
    int mode=-1,weight=-1; bool aes=false;
public:
    void prepare(double fs) { rate=std::max(8000.,fs);mode=-1;configure(VU,Z,false); }
    void configure(int newMode,int newWeight,bool newAES) {
        newMode=std::clamp(newMode,0,9);newWeight=isRMS(newMode)?std::clamp(newWeight,0,5):Z;
        // Calibration is a scale offset, not a new detector. Preserve the
        // averaging/filter/envelope history when only AES-17 changes (including
        // modes where that option is inactive).
        if(mode==newMode && weight==newWeight) { aes=newAES; return; }
        mode=newMode;weight=newWeight;aes=newAES;
        rmsC=std::exp(-1/(rate*.3));vuC=std::exp(-1/(rate*(.3/6.63835)));
        const double integration=mode==DIN?.005:mode==Nordic?.005:.010;
        attackC=std::exp(-(2.2*std::log(5.))/(rate*integration));
        const double fallSeconds=mode==DIN?1.5:mode==Nordic?1.7:mode==BBC?3.:2.8;
        const double fallDb=mode==DIN || mode==Nordic?20.:mode==BBC?26.:24.;
        releaseC=std::pow(10.,-fallDb/(20*rate*fallSeconds));
        peakC=std::pow(10.,-24./(20*rate));
        for(size_t ch=0;ch<2;++ch){channels[ch]={};channels[ch].filter.prepare(rate,weight);published[ch].store(-99,std::memory_order_relaxed);}
    }
    void push(const float* const* data,int numChannels,int samples) noexcept {
        for(int ch=0;ch<2;++ch) {
            auto& s=channels[size_t(ch)];
            for(int i=0;i<samples;++i) {
                const double x=ch<numChannels && std::isfinite(data[ch][i]) ? data[ch][i]:0.;
                const double a=std::abs(x);
                if(mode==Peak)s.peak=std::max(a,s.peak*peakC);
                else if(isRMS(mode)) {const double y=s.filter.tick(x);s.power=rmsC*s.power+(1-rmsC)*y*y;}
                else if(mode==VU) {s.rectified=vuC*s.rectified+(1-vuC)*a;s.vu=vuC*s.vu+(1-vuC)*s.rectified;}
                else s.ppm=a>s.ppm ? attackC*s.ppm+(1-attackC)*a:s.ppm*releaseC;
            }
            double amplitude=mode==Peak?s.peak:isRMS(mode)?std::sqrt(std::max(0.,s.power))*(aes || mode!=RMS?std::sqrt(2.):1.):mode==VU?s.vu*1.5707963267948966:s.ppm;
            published[size_t(ch)].store(db(amplitude),std::memory_order_relaxed);
        }
    }
    void reset() noexcept {const int selected=mode;mode=-1;configure(selected,weight,aes);}
    float read(int ch) const {return published[size_t(std::clamp(ch,0,1))].load(std::memory_order_relaxed);}
};
}
