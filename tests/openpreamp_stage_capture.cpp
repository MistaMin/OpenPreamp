// Offline, stage-by-stage identification. Never linked into the plugin.
#include "BritCircuit.h"
#include "NTypeCircuit.h"
#include "FsfCircuit.h"
#include "ATypeCircuit.h"
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <bit>
#include <limits>

namespace fs=std::filesystem;
using File=std::unique_ptr<std::FILE,decltype(&std::fclose)>;
File open(const fs::path& p){return File(std::fopen(p.c_str(),"wb"),std::fclose);}
struct Pair{double input,output;};
static_assert(sizeof(Pair)==16 && sizeof(double)==8 && std::numeric_limits<double>::is_iec559);
static_assert(std::endian::native==std::endian::little,"Capture schema uses little-endian IEEE float64 pairs");
constexpr double rate=96000,pi=3.14159265358979323846;
// Identification is nominal 96 kHz and unity internal gain. INPUT/PAD remain
// external trims. All stages are measured with their real downstream loading.
template<class Circuit>
bool capture(const fs::path& dir,const char* name,double fullScale,std::FILE* manifest,std::FILE* ports) {
    Circuit circuit;if(!circuit.prepare(rate)){std::fprintf(stderr,"DC preparation failed: %s\n",name);return false;}
    for(int fixture=0;fixture<10;++fixture) {
        // restoreDc restores unknown voltages, but intentionally retains fixed
        // source controls. Reset only the driven source before restoring histories
        // or measuring DC; rails remain at their nominal operating voltages.
        const auto source=circuit.stageConnections().front();
        circuit.core().setFixed(source.inputPositive,0);
        if(source.inputNegative!=0)circuit.core().setFixed(source.inputNegative,0);
        circuit.restoreDc(rate);circuit.setFullScaleVolts(fullScale);circuit.setGain(1);
        const auto bias=circuit.stageProbes();
        const bool impulse=fixture==0,memory=fixture==9;const double amplitudes[]={.001,.03,.1,.3,.5,1,2,4};
        const double amplitude=impulse?.001:memory?4.:amplitudes[fixture-1];
        const int frames=memory?192000:impulse?65536:32768,begin=impulse||memory?0:20480;
        const auto stem=std::string(name)+(impulse?"-impulse":memory?"-memory":"-drive-"+std::to_string(fixture));
        std::vector<File> files;
        for(const auto& stage:bias){files.push_back(open(dir/(stem+"-"+stage.name+".bin")));if(!files.back())return false;}
        auto complete=open(dir/(stem+"-complete.bin"));auto mask=open(dir/(stem+".valid"));if(!complete||!mask)return false;
        const auto initialFailures=circuit.core().failCount,initialHard=circuit.core().hardFails;
        int quarantine=0,validCount=0,longest=0,run=0;
        for(int i=0;i<frames;++i){
            const double t=i/rate;
            const double x=memory?(t<.2?0:t<.6?4*std::sin(2*pi*80*t):t<1.2?.03*std::sin(2*pi*997*t):t<1.4?-2:t<1.6?2:0):impulse?(i==0?amplitude:0):amplitude*std::sin(2*pi*1000*i/rate);
            const auto hardBefore=circuit.core().hardFails;
            const double output=circuit.process(x);const auto probes=circuit.stageProbes();
            bool finite=std::isfinite(output);
            for(const auto& stage:probes)finite=finite && std::isfinite(stage.inputVolts) && std::isfinite(stage.outputVolts);
            if(circuit.core().hardFails!=hardBefore || !finite)quarantine=int(rate*.05);
            const unsigned char valid=quarantine==0;if(quarantine>0)--quarantine;
            if(i>=begin){if(valid){++validCount;longest=std::max(longest,++run);}else run=0;}
            if(std::fwrite(&valid,1,1,mask.get())!=1)return false;
            const Pair full{x,output};if(std::fwrite(&full,sizeof(full),1,complete.get())!=1)return false;
            for(size_t s=0;s<probes.size();++s){const Pair sample{probes[s].inputVolts-bias[s].inputVolts,probes[s].outputVolts-bias[s].outputVolts};if(std::fwrite(&sample,sizeof(sample),1,files[s].get())!=1)return false;}
        }
        const int measured=frames-begin;const double fraction=double(validCount)/measured;
        const bool accepted=fraction>=(impulse?.999: .99) && longest>=(impulse?32768:4608);
        std::fprintf(manifest,"%s,%s,%s,96000,%.17g,%.17g,%d,%d,%d,%d,%lld,%lld,%d\n",name,stem.c_str(),impulse?"impulse":memory?"memory":"curve",amplitude,fullScale,frames,begin,validCount,longest,circuit.core().failCount-initialFailures,circuit.core().hardFails-initialHard,int(accepted));
        for(const auto& stage:bias)std::fprintf(ports,"%s,%s,%s,%.17g,%.17g\n",name,stem.c_str(),stage.name,stage.inputVolts,stage.outputVolts);
        if(std::fflush(manifest)!=0 || std::fflush(ports)!=0)return false;
        std::printf("%s accepted=%d valid=%d/%d longest=%d hard_failures=%lld\n",stem.c_str(),int(accepted),validCount,measured,longest,circuit.core().hardFails-initialHard);std::fflush(stdout);
    }
    return true;
}
int main(int argc,char** argv) {
    if(argc!=2){std::fprintf(stderr,"usage: OpenPreampStageCapture NEW_OUTPUT_DIRECTORY\n");return 2;}
    const fs::path dir(argv[1]);if(fs::exists(dir)&&!fs::is_empty(dir)){std::fprintf(stderr,"Refusing to overwrite existing captures\n");return 2;}fs::create_directories(dir);
    auto manifest=open(dir/"fixtures.csv"),ports=open(dir/"ports.csv");if(!manifest||!ports)return 2;
    std::fprintf(manifest.get(),"model,fixture,kind,rate,amplitude,full_scale_volts,frames,analysis_begin,valid_frames,longest_valid_run,recovered_steps,hard_failures,accepted\n");
    std::fprintf(ports.get(),"model,fixture,stage,input_dc_volts,output_dc_volts\n");
    const bool ok=capture<BritCircuit>(dir,"brit",1.7,manifest.get(),ports.get())&&capture<NTypeCircuit>(dir,"ntype",2,manifest.get(),ports.get())&&capture<FsfCircuit>(dir,"fsf",.8,manifest.get(),ports.get())&&capture<ATypeCircuit>(dir,"atype",.6,manifest.get(),ports.get());
    return ok?0:1; // Numerical acceptance is a separate fail-closed gate tool.
}
