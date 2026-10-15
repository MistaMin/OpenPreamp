// Compiles against either the frozen original headers or the instrumented ones.
#include "BritCircuit.h"
#include "NTypeCircuit.h"
#include "FsfCircuit.h"
#include "ATypeCircuit.h"
#include <cstdio>
#include <filesystem>
#include <string>

template<class Circuit>
bool trace(const std::filesystem::path& dir,const char* name,double scale,double rate) {
    Circuit circuit;if(!circuit.prepare(96000))return false;
    circuit.restoreDc(rate);circuit.setFullScaleVolts(scale);circuit.setGain(1);
    auto file=dir/(std::string(name)+"-"+std::to_string(int(rate))+".bin");
    auto* out=std::fopen(file.c_str(),"wb");if(!out)return false;
    for(int i=0;i<16384;++i){double t=i/rate;double amp=i<4096?.001:i<8192?.5:i<12288?8.:.03;
        double x=amp*(.7*std::sin(6.283185307179586*83*t)+.2*std::sin(6.283185307179586*997*t)+.1*std::sin(6.283185307179586*7001*t));
        const double y=circuit.process(x);if(std::fwrite(&y,sizeof(y),1,out)!=1){std::fclose(out);return false;}
    }
    return std::fclose(out)==0;
}
int main(int argc,char** argv){
    if(argc!=2)return 2;const std::filesystem::path dir(argv[1]);std::filesystem::create_directories(dir);
    for(double rate:{96000.,192000.})if(!(trace<BritCircuit>(dir,"brit",1.7,rate)&&trace<NTypeCircuit>(dir,"ntype",2.,rate)&&trace<FsfCircuit>(dir,"fsf",.8,rate)&&trace<ATypeCircuit>(dir,"atype",.6,rate)))return 1;
    std::puts("Captured 131072 original circuit output samples");return 0;
}
