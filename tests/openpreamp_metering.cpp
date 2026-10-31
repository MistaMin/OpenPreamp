#include "DSP/Metering.h"
#include <cstdio>
#include <vector>
int fails=0,checks=0;
void check(bool ok,const char* message){++checks;printf("%s %s\n",ok?"PASS":"FAIL",message);fails+=!ok;}
void feed(openpreamp::Metering& m,double fs,double seconds,double hz,double amp,int block=127) {
 std::vector<float> l(size_t(block),0),r(size_t(block),0);const float* d[]={l.data(),r.data()};
 const int n=int(fs*seconds);for(int t=0;t<n;t+=block){int count=std::min(block,n-t);for(int i=0;i<count;++i){l[size_t(i)]=float(amp*std::sin(6.283185307179586*hz*(t+i)/fs));r[size_t(i)]=l[size_t(i)]*.5f;}m.push(d,2,count);}
}
static void rmsStabilityChecks() {
 for(double fs:{44100.,48000.,96000.,192000.})for(double hz:{20.,31.,997.}) {
  openpreamp::Metering meter;meter.prepare(fs);meter.configure(openpreamp::RMS,0,false);
  std::vector<float> signal(64);const float* ptr[]={signal.data()};
  float lo=0,hi=-200;int nextRead=int(fs/30);
  for(int n=0;n<int(fs*6);n+=64) {
   const int count=std::min(64,int(fs*6)-n);
   for(int i=0;i<count;++i)signal[size_t(i)]=float(.125892541*std::sin(6.283185307179586*hz*(n+i)/fs));
   meter.push(ptr,1,count);
   if(n+count>=nextRead){if(n>fs*3){lo=std::min(lo,meter.read(0));hi=std::max(hi,meter.read(0));}nextRead+=int(fs/30);}
  }
  printf("RMS stability %.0f Hz @ %.0f: %.6f dB peak-to-peak\n",hz,fs,hi-lo);
  check(hi-lo<(hz<100?.12:.003),"RMS output stays stable on constant tones, including bass");
  check(std::abs(.5*(hi+lo)+21.0103)<.01,"stable RMS reading retains correct calibration");
 }
 openpreamp::Metering meter;meter.prepare(48000);meter.configure(openpreamp::RMS,0,false);
 feed(meter,48000,3,1000,.1);feed(meter,48000,.3,1000,.5);
 const double expected=10*std::log10(.5*(.25+(.01-.25)*std::exp(-1.)));
 check(std::abs(meter.read(0)-expected)<.01,"RMS follows level steps with the documented 300 ms power time constant");
 const float before=meter.read(0);feed(meter,48000,.3,1000,0);
 check(std::abs(meter.read(0)-before+10/std::log(10.))<.001,"RMS decay remains accurate after stabilizing the numeric readout");
}

int main(){
 rmsStabilityChecks();
 for(int mode : {openpreamp::RMS,openpreamp::VU,openpreamp::K14,openpreamp::BBC}) {
  openpreamp::Metering meter;meter.prepare(48000);meter.configure(mode,0,false);feed(meter,48000,4,1000,.125892541);
  const float before=meter.read(0);meter.configure(mode,0,true);float zero=0;const float* input[]={&zero};meter.push(input,1,1);
  const float expectedOffset=mode==openpreamp::RMS?3.0103f:0.f;
  check(std::abs(meter.read(0)-before-expectedOffset)<.001,"AES-17 changes scale without resetting RMS history or inactive detectors");
  if(mode==openpreamp::RMS){const float normalized=meter.read(0);meter.configure(mode,0,false);meter.push(input,1,1);check(std::abs(meter.read(0)-normalized+3.0103f)<.001,"disabling RMS calibration preserves history and immediately restores raw scale");}
 }
 for(double fs:{44100.,48000.,96000.,192000.}) {
 for(int mode=0;mode<10;++mode){openpreamp::Metering m;m.prepare(fs);m.configure(mode,0,false);feed(m,fs,4,1000,.125892541);float expected=mode==openpreamp::RMS?-21.0103f:-18.f;check(std::abs(m.read(0)-expected)<.3,"1 kHz steady-state calibration across modes and sample rates");check(std::abs(m.read(1)-m.read(0)+6.0206)<.01,"independent L/R calibration");}
 openpreamp::Metering m;m.prepare(fs);m.configure(openpreamp::RMS,0,true);feed(m,fs,4,1000,1);check(std::abs(m.read(0))<.015,"AES-17 full-scale sine reads zero");
 openpreamp::Metering v;v.prepare(fs);v.configure(openpreamp::VU,0,false);feed(v,fs,.3,1000,1);check(std::abs(v.read(0)+.0873)<.04,"VU 300 ms rise reaches 99 percent");
 {
  // IEC 268-17 step response: 99 percent in 0.3 s +/-10 percent, overswing 1 to 1.5 percent.
  openpreamp::Metering step;step.prepare(fs);step.configure(openpreamp::VU,0,false);
  const float reference=0.125892541f;std::vector<double> levels;
  for(int i=0;i<int(fs);++i){const float x=reference*std::sin(6.283185307179586*1000.0*i/fs);const float* channel[1]={&x};step.push(channel,1,1);levels.push_back(std::pow(10.,step.read(0)/20.));}
  // Compare against the settled reading, not the nominal peak: sampled sines carry a small rectified-mean bias at low rates.
  double settled=0;const int tail=int(fs*.2);for(int i=int(levels.size())-tail;i<int(levels.size());++i)settled+=levels[size_t(i)]/tail;
  double rise=-1,overswing=0;for(size_t i=0;i<levels.size();++i){const double linear=levels[i]/settled;if(rise<0&&linear>=.99)rise=double(i)/fs;overswing=std::max(overswing,linear-1.);}
  printf("VU step %.0f rise %.4f overswing %.4f\n",fs,rise,overswing);
  check(rise>.27&&rise<.33,"VU step reaches 99 percent in 0.3 s +/-10 percent");
  check(overswing>.01&&overswing<.015,"VU step overswing is 1 to 1.5 percent");
 }
 for(int mode:{openpreamp::DIN,openpreamp::Nordic,openpreamp::BBC,openpreamp::EBU}){openpreamp::Metering p;p.prepare(fs);p.configure(mode,0,false);const double sec=mode<=openpreamp::Nordic?.005:.010;feed(p,fs,sec,1000,1);const float burst=p.read(0);printf("burst %.0f mode %d %.4f\n",fs,mode,burst);check(burst> -2.6 && burst< -1.5,"PPM tone-burst integration near -2 dB");feed(p,fs,1,1000,1);const float before=p.read(0);feed(p,fs,.5,1000,0);const double slope=mode==openpreamp::DIN?20/1.5:mode==openpreamp::Nordic?20/1.7:mode==openpreamp::BBC?26/3.:24/2.8;check(std::abs(p.read(0)-before+.5*slope)<.01,"PPM standard fall rate");}
 for(int w=0;w<=5;++w){openpreamp::WeightingFilter f;f.prepare(fs,w);double power=0;for(int t=0;t<int(fs*2);++t){double y=f.tick(std::sin(6.283185307179586*1000*t/fs));if(t>=int(fs))power+=y*y;}double response=10*std::log10(2*power/fs);printf("weight %.0f curve %d 1kHz %.4f\n",fs,w,response);check(std::abs(response-(w==5?.6977:0))<.04,"weighting reference response");}
 // Published nominal weighting gains at 100 Hz (A -19.1, B -5.6, C -0.3, D -7.2).
 for(int w=1;w<=4;++w){openpreamp::Metering p;p.prepare(fs);p.configure(openpreamp::RMS,w,true);feed(p,fs,4,100,.1);const double nominal[]={0,-19.1,-5.6,-.3,-7.2};printf("weight %.0f curve %d 100Hz %.4f\n",fs,w,p.read(0)+20);check(std::abs(p.read(0)+20-nominal[w])<.3,"weighting low-frequency response independent table");}
 openpreamp::Metering a,b;a.prepare(fs);b.prepare(fs);a.configure(1,1,true);b.configure(1,1,true);feed(a,fs,2,997,.5,1);feed(b,fs,2,997,.5,511);check(std::abs(a.read(0)-b.read(0))<1.e-5,"measurement is independent of host block size and GUI refresh");
 }
 printf("%d checks %d failures\n",checks,fails);return fails?1:0;
}
