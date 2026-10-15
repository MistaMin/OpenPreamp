// Count C++ heap allocation requests only inside processBlock. This catches
// parameter-name construction, buffer growth and table creation on audio.
#include "OpenPreampPlugin/PluginProcessor.h"
#include <cstdio>
#include <cstdlib>
#include <new>
static thread_local bool track=false;
static thread_local size_t allocations=0;
static thread_local size_t frees=0;
void* operator new(size_t n){if(track)++allocations;if(auto* p=std::malloc(std::max(size_t(1),n)))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{if(track && p)++frees;std::free(p);}
void operator delete[](void* p) noexcept{if(track && p)++frees;std::free(p);}
void* operator new(size_t n,std::align_val_t a){if(track)++allocations;void* p=nullptr;if(posix_memalign(&p,size_t(a),std::max(size_t(1),n))==0)return p;throw std::bad_alloc();}
void* operator new[](size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void* p,std::align_val_t) noexcept{if(track && p)++frees;std::free(p);}
void operator delete[](void* p,std::align_val_t) noexcept{if(track && p)++frees;std::free(p);}
static void set(OpenPreampProcessor& p,const char* id,float v){auto* param=p.apvts.getParameter(id);param->setValueNotifyingHost(param->convertTo0to1(v));}
int main(){
    juce::ScopedJuceInitialiser_GUI gui;OpenPreampProcessor p;juce::MidiBuffer midi;
    int cases=0,failures=0;
    for(double rate:{48000.,96000.})for(int block:{32,64,256}) {
        p.setPlayConfigDetails(2,2,rate,block);p.prepareToPlay(rate,block);
        juce::AudioBuffer<float> audio(2,block);
        set(p,"circuitEngine",1);
        for(int type:{0,1,2,4,3})for(int os:{0,1,2,3})for(int circuit:{0,1}){
            set(p,"preampType",float(type));set(p,"preampOversampling",float(os));set(p,"preampCircuit",float(circuit));
            for(int b=0;b<4;++b){set(p,"preampGain",float(b*6));set(p,"highPass",float(20+100*b));set(p,"monoMakerEnabled",float(b%2));
                for(int ch=0;ch<2;++ch)for(int i=0;i<block;++i)audio.setSample(ch,i,float(.1*std::sin(.2*(i+b*block))));
                allocations=frees=0;track=true;p.processBlock(audio,midi);track=false;++cases;
                if(allocations || frees){++failures;std::printf("FAIL rate=%.0f block=%d type=%d oversampling=%d circuit=%d allocations=%zu frees=%zu\n",rate,block,type,os,circuit,allocations,frees);}
            }
        }
    }
    std::printf("%d audio callbacks, %d with C++ heap allocations or frees\n",cases,failures);return failures?1:0;
}
