// Offline reference generator. Never linked into the reduced runtime kernel.
#include "DSP/BritCircuit.h"
#include "DSP/NTypeCircuit.h"
#include "DSP/FsfCircuit.h"
#include "DSP/ATypeCircuit.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>

constexpr double pi = 3.14159265358979323846;
struct Sample { double input, output; };
template<class Circuit>
bool capture(const char* name, double scale, const std::filesystem::path& dir, double rate, bool quick, bool fixturesOnly, bool trainOnly) {
    Circuit circuit;
    if (!circuit.prepare(96000)) return false;
    auto reset = [&] { circuit.restoreDc(rate); circuit.setFullScaleVolts(scale); circuit.setGain(1.0); };
    if (!fixturesOnly) {
    const auto csvPath = dir / (std::string(name) + "-tones.csv");
    auto* csv = std::fopen(csvPath.c_str(), "w"); if (!csv) return false;
    std::fprintf(csv,"rate,frequency,amplitude,gain_db,phase_rad,h2_dbr,h3_dbr,h4_dbr,h5_dbr,failures,hard_failures\n");
    const std::vector<double> freqs = quick ? std::vector<double>{30,100,300,1000,3000,7000,12000,18000} : std::vector<double>{20,30,50,80,100,200,300,500,1000,2000,3000,5000,7000,10000,12000,16000,18000,20000};
    const std::vector<double> levels = quick ? std::vector<double>{0.001,0.1,0.5,1,2,4,8} : std::vector<double>{0.001,0.01,0.1,0.3,0.5,1,2,4,8,16,50};
    for (double freq : freqs) for (double amp : levels) {
        reset();
        const int warm = int(std::ceil(std::max(0.08, 6.0/freq)*rate));
        const int cycles = std::max(4,int(std::ceil(0.08*freq)));
        const int n = int(std::ceil(cycles*rate/freq));
        // Exact integer cycles for projection, avoiding harmonic leakage.
        const double omega = 2*pi*cycles/n;
        for (int i=-warm; i<0; ++i) circuit.process(amp*std::sin(omega*i));
        const auto fails=circuit.core().failCount, hard=circuit.core().hardFails;
        double re[5]{}, im[5]{};
        for (int i=0;i<n;++i) {
            const double y=circuit.process(amp*std::sin(omega*i));
            for (int k=0;k<5;++k) { re[k]+=y*std::cos((k+1)*omega*i); im[k]+=y*std::sin((k+1)*omega*i); }
        }
        const double fundamental=2*std::hypot(re[0],im[0])/n;
        std::fprintf(csv,"%.0f,%.9g,%.9g,%.9g,%.9g",rate,cycles*rate/n,amp,20*std::log10(std::max(fundamental/amp,1e-30)),std::atan2(re[0],im[0]));
        for (int k=1;k<5;++k)std::fprintf(csv,",%.9g",20*std::log10(std::max(std::hypot(re[k],im[k])/std::hypot(re[0],im[0]),1e-30)));
        std::fprintf(csv,",%lld,%lld\n",circuit.core().failCount-fails,circuit.core().hardFails-hard); std::fflush(csv);
    }
    std::fclose(csv);
    }
    // Persist independent training and held-out fixtures. All input drive is external.
    for (int fixture=0;fixture<(trainOnly?1:3);++fixture) {
        reset();
        auto path=dir/(std::string(name)+(fixture==0?"-train.bin":fixture==1?"-validation.bin":"-recovery.bin"));
        auto* file=std::fopen(path.c_str(),"wb");if(!file)return false;
        auto* mask=std::fopen((path.string()+".valid").c_str(),"wb"); if(!mask){std::fclose(file);return false;}
        int invalidCountdown=0; long long invalidSamples=0;
        const int length=int(rate*(fixture==0?10.0:fixture==2?2.0:4.0));
        juce::Random rng(fixture==0?1701:9117);
        double low=0;
        for(int i=0;i<length;++i) {
            const double t=i/rate;
            const int segment=int(t/.25);
            static constexpr double amps[]={0.001,0.03,0.1,0.3,0.5,1,2,4,8,16,32,50,1,0.1,4,0.5};
            double x=0;
            if(fixture==0) {
                low+=0.15*(2*rng.nextDouble()-1-low);
                x=amps[segment%16]*(0.65*low+0.2*std::sin(2*pi*67*t)+0.1*std::sin(2*pi*991*t)+0.05*std::sin(2*pi*7013*t));
                if(t>=4 && t<5)x=t<4.15?0:t<4.5?6*std::sin(2*pi*40*t):.03*std::sin(2*pi*997*t);
                if(t>=5 && t<6)x=t<5.2?.05*std::sin(2*pi*271*t):t<5.6?3*std::sin(2*pi*150*t):.05*std::sin(2*pi*271*t);
                if(t>=6 && t<7)x=t<6.4?3:.1*std::sin(2*pi*113*t);
                if(t>=7 && t<8)x=t<7.2?-3:t<7.4?3:0;
                if(t>=8 && t<9)x=t<8.5?1.5*std::sin(2*pi*(40*(t-8)+6000*(t-8)*(t-8))):.03*std::sin(2*pi*271*t);
                if(t>=9)x=(.1+2.5*std::exp(-6*(t-9)))*(.5*std::sin(2*pi*31*t)+.2*std::sin(2*pi*297*t)+.2*std::sin(2*pi*1777*t)+.1*std::sin(2*pi*7111*t));
            } else if(fixture==1) {
                const double f=25*std::pow(700.0,t/4.0);
                x=amps[(segment*7+3)%16]*(.55*std::sin(2*pi*25*4/std::log(700.0)*(std::pow(700.0,t/4.0)-1))+.2*std::sin(2*pi*119*t)+.15*std::sin(2*pi*1733*t)+.1*(2*rng.nextDouble()-1));
                (void)f;
            } else {
                x=t<.2?0:t<.6?8*std::sin(2*pi*80*t):t<1.2?.1*std::sin(2*pi*997*t):t<1.4?-4:t<1.6?4:0;
            }
            const auto hardBefore=circuit.core().hardFails;
            const Sample sample{x,circuit.process(x)};
            if(circuit.core().hardFails!=hardBefore)invalidCountdown=int(rate*.05);
            const unsigned char valid=invalidCountdown==0 && std::isfinite(sample.output);
            if(invalidCountdown>0)--invalidCountdown; if(!valid)++invalidSamples;
            std::fwrite(&valid,1,1,mask);
            if(std::fwrite(&sample,sizeof(sample),1,file)!=1){std::fclose(file);return false;}
        }
        std::fclose(file); std::fclose(mask);
        std::printf("%s fixture=%d invalid=%lld/%d\n",name,fixture,invalidSamples,length);std::fflush(stdout);
    }
    std::printf("Captured %s at %.0f Hz\n",name,rate);std::fflush(stdout);return true;
}
int main(int argc,char** argv) {
    if(argc<2){std::fprintf(stderr,"usage: OpenPreampCapture OUTPUT_DIRECTORY [--quick] [--rate N]\n");return 2;}
    std::filesystem::path dir(argv[1]);std::filesystem::create_directories(dir);
    bool quick=false,fixturesOnly=false,trainOnly=false;double rate=96000;
    for(int i=2;i<argc;++i){std::string arg(argv[i]);if(arg=="--quick")quick=true;else if(arg=="--fixtures-only")fixturesOnly=true;else if(arg=="--train-only")trainOnly=fixturesOnly=true;else if(arg=="--rate"&&i+1<argc)rate=std::atof(argv[++i]);else return 2;}
    if(rate<44100||rate>384000)return 2;
    juce::ScopedNoDenormals noDenormals;
    return capture<BritCircuit>("brit",1.7,dir,rate,quick,fixturesOnly,trainOnly)&&capture<NTypeCircuit>("ntype",2.0,dir,rate,quick,fixturesOnly,trainOnly)&&capture<FsfCircuit>("fsf",0.8,dir,rate,quick,fixturesOnly,trainOnly)&&capture<ATypeCircuit>("atype",0.6,dir,rate,quick,fixturesOnly,trainOnly)?0:1;
}
