#include "support/PacedTiming.hpp"
#include <cstdlib>
#include <iostream>
using namespace cyberator::test;
void require(bool b,const char* msg){if(!b){std::cerr<<msg<<'\n';std::exit(1);}}
int main(){
    std::vector<TimingSample> v;
    for(int i=1;i<=100;++i)v.push_back({double(i),double(i%3),double(i)-80});
    const auto s=summarize(v,90);
    require(s.p50==50&&s.p95==95&&s.p99==99&&s.maximum==100,"nearest rank oracle");
    require(s.durationMisses==10&&s.completionMisses==20,"duration and completion deadlines distinct");
    require(s.maxStartLate==2&&s.maxFinishLate==20,"maximum lateness");
    unsigned setup=0,run=0,observe=0;
    const auto measured=pace(48000,64,4,[&](auto){++setup;},[&](auto){++run;std::this_thread::sleep_for(std::chrono::milliseconds(5));},[&](auto){++observe;});
    const auto negative=summarize(measured,64e6/48000);
    require(setup==4&&run==4&&observe==4,"exact callback count");
    require(negative.durationMisses==4&&negative.completionMisses==4,"deliberately late callback rejected");
    std::cout<<"Paced timing independent oracle and late callback negative control PASS\n";
}
