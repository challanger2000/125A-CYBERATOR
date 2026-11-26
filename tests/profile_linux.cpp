#include "dsp/Engine.hpp"
#include <chrono>
#include <iostream>
#include <ctime>
using namespace cyberator;
static double now(){timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return double(t.tv_sec)*1e6+double(t.tv_nsec)*.001;}
int main(){
  for(double sr:{48000.,96000.,192000.})for(int block:{64,256})for(bool active:{false,true}){
    Engine e;e.prepare(sr);EngineParams p;
    if(active){p.granular=1;p.grain.sizeMs=250;p.grain.density=1;p.grain.pitch=12;p.grain.scatter=.7;p.grain.feedback=.5;p.cyber={8,.8,.8};p.harmony=p.ghost=p.swell=p.delay=p.space=.8;}
    std::vector<StereoFrame> input(32768);for(std::size_t i=0;i<input.size();++i)input[i]={float(.3*std::sin(.017*double(i))),float(.2*std::cos(.023*double(i)))};
    double check=0;for(auto x:input)check+=e.process(x,p).left;
    std::vector<double> samples(1024);int n=0;for(auto& t:samples){const double start=now();for(int i=0;i<block;++i){check+=e.process(input[std::size_t(n++)%input.size()],p).left;}t=now()-start;}
    std::sort(samples.begin(),samples.end());const double budget=1e6*block/sr;auto misses=std::count_if(samples.begin(),samples.end(),[&](double t){return t>budget;});
    std::cout<<sr<<' '<<block<<' '<<active<<" budget "<<budget<<" p95 "<<samples[972]<<" p99 "<<samples[1013]<<" max "<<samples.back()<<" over "<<misses<<" check "<<check<<'\n';
  }
}
