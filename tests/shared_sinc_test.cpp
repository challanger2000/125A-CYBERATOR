#include "dsp/GrainBank.hpp"
#include "reference/GrainBankP031.hpp"
#include "support/AllocationProbe.hpp"
#include <barrier>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <thread>
using namespace cyberator;
using cyberator::test::AllocationProbe;
void check(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
void same(StereoFrame a,StereoFrame b){check(a.left==b.left&&a.right==b.right,"shared sinc exact PCM");}
int main(){
    History h;h.prepare(4096);
    for(int i=0;i<4096;++i)h.capture({float(.3*std::sin(.037*i)),float(.2*std::cos(.059*i))});
    SincReader empty;same(empty.read(h,512.5,0),{});
    constexpr std::size_t workers=8;
    std::array<std::size_t,workers> allocs{},frees{};
    std::barrier start(static_cast<std::ptrdiff_t>(workers));
    std::array<std::thread,workers> threads;
    for(std::size_t t=0;t<workers;++t)threads[t]=std::thread([&,t]{
        cyberator_p031::SincReader ref;ref.prepare();SincReader reader;
        start.arrive_and_wait();
        AllocationProbe probe;reader.prepare();const auto n=probe.finish();allocs[t]=n.allocations;frees[t]=n.frees;
        SincReader::KernelCache cache;
        for(int band=0;band<9;++band)for(int phase=0;phase<256;++phase){
            const double age=100+double(t)*100+double(phase)/256.;
            same(reader.read(h,age,band,&cache),ref.read(h,age,band));
            same(reader.read(h,age+.001,band),ref.read(h,age+.001,band));
        }
        {AllocationProbe repeat;reader.prepare();const auto c=repeat.finish();check(c.allocations==0&&c.frees==0,"repeated prepare reuses shared immutable table");}
        SincReader moved=std::move(reader);same(moved.read(h,256.75,4),ref.read(h,256.75,4));
    });
    for(auto& thread:threads)thread.join();
    std::size_t total=0;for(std::size_t i=0;i<workers;++i){total+=allocs[i];check(frees[i]==0,"shared table not released by prepare");}
    check(total==1,"exactly one allocation across concurrent first prepares");
    std::mt19937 rng(9821);std::uniform_real_distribution<float> noise(-.8f,.8f);
    for(double rate:{-4.,-1.,-.75,.25,1.,1.037,2.,4.}){
        GrainBank a;cyberator_p031::GrainBank b;a.prepare(16000);b.prepare(16000);
        for(int i=0;i<16000;++i){float l=noise(rng),r=noise(rng);same(a.process({l,r},false),b.process({l,r},false));}
        for(int i=0;i<32;++i)check(a.trigger(8000+i*.124987,rate,1000,.2)==b.trigger(8000+i*.124987,rate,1000,.2),"admission unchanged");
        check(a.active()==32&&b.active()==32,"all voices active");
        for(int i=0;i<1500;++i){float l=noise(rng),r=noise(rng);same(a.process({l,r},i%17==0),b.process({l,r},i%17==0));}
        check(a.active()==0&&b.active()==0,"retirement unchanged");
    }
    using Clock=std::chrono::steady_clock;
    auto t=Clock::now();for(int i=0;i<32;++i){cyberator_p031::SincReader reader;reader.prepare();same(reader.read(h,300.125,4),reader.read(h,300.125,4));}
    const double original=std::chrono::duration<double,std::milli>(Clock::now()-t).count();
    t=Clock::now();for(int i=0;i<32;++i){SincReader reader;reader.prepare();same(reader.read(h,300.125,4),reader.read(h,300.125,4));}
    const double shared=std::chrono::duration<double,std::milli>(Clock::now()-t).count();
    std::cout<<"Concurrent first prepare allocations "<<total<<"; 32 old prepares "<<original<<" ms; 32 warm shared prepares "<<shared<<" ms; exact PCM PASS\n";
}
