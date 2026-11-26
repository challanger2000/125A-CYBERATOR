#include "gui/SignalCapture.hpp"
#include "dsp/Engine.hpp"
#include <cstdlib>
#include <iostream>
#include <thread>
using namespace cyberator;
void check(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    SignalCapture capture;capture.reset(48000);SignalCapture::Snapshot s;
    check(!capture.read(s),"no fake initial frame");
    for(std::size_t i=0;i<SignalCapture::frames;++i){float x=float(i)/2048;capture.push({x,-x},{2*x,-x},{x/2,0},{0,x/4},i/128);}
    check(capture.read(s)&&s.sampleRate==48000&&s.grains==15,"published metadata");
    for(std::size_t i=0;i<SignalCapture::frames;++i)check(s.dry[i]==float(i)/2048&&s.wet[i]==float(i)/1024,"exact captured samples");
    check(s.peakL==2047.f/1024&&s.peakR==2047.f/2048&&s.echoPeak==2047.f/4096&&s.swellPeak==2047.f/8192,"actual peak values");
    capture.reset(96000);check(!capture.read(s),"reset invalidates old display");
    std::atomic<bool> done{false};std::thread writer([&]{for(unsigned group=1;group<=1000;++group)for(std::size_t i=0;i<SignalCapture::frames;++i)capture.push({float(group),0},{-float(group),0},{}, {},0);done.store(true);});
    unsigned reads=0;do{if(capture.read(s)){++reads;check(s.sampleRate==96000,"new sample rate");for(std::size_t i=0;i<s.dry.size();++i)check(s.dry[i]==s.dry[0]&&s.wet[i]==-s.dry[0],"no torn frame");}}while(!done.load());writer.join();check(reads>0,"concurrent reads");
    // Independent graph witness: reverse-swell impulse before aligned dry.
    Engine e;e.prepare(48000,LatencyProfile::live);EngineParams p;p.swell=1;
    double swellEnergy=0;for(std::size_t i=0;i<=e.latency();++i){EngineSignals a;auto y=e.process(i==0?StereoFrame{1,0}:StereoFrame{},p,&a);check(a.dry.left==(i==e.latency()?1:0),"actual aligned dry");check(a.echo.left==0,"zero ghost echo");if(i<e.latency()){check(y.left==a.swell.left,"actual wet swell witness");swellEnergy+=double(a.swell.left)*a.swell.left;}}
    check(std::abs(swellEnergy-1)<1e-6,"actual swell energy");std::cout<<"Capture ownership and PCM witness PASS\n";
}
