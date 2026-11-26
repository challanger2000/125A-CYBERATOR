#include "control/SessionState.hpp"
#include <cstdlib>
#include <iostream>
using namespace cyberator;
void check(bool ok,const char* m){if(!ok){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    SessionState a,b;a.scale=1.25f;check(a.parameters.set(Param::formant,7),"fixture");auto bytes=a.save();
    check(b.restore(bytes)&&b.scale==1.25f&&b.parameters.get(Param::formant)==7&&b.save()==bytes,"session recall");
    for(std::size_t i=0;i<bytes.size();++i){auto damaged=bytes;damaged[i]^=1;check(!b.restore(damaged)&&b.save()==bytes,"session corruption rollback");}
    for(std::size_t i=0;i<bytes.size();++i)if(i!=Parameters::bytes)check(!b.restore(std::span(bytes).first(i))&&b.save()==bytes,"session truncation rollback");
    auto legacy=a.parameters.save();check(b.restore(legacy)&&b.scale==SessionState::defaultScale&&b.parameters.save()==legacy,"legacy compatible");
    a.scale=std::numeric_limits<float>::quiet_NaN();check(b.restore(a.save())&&b.scale==SessionState::defaultScale,"invalid scale save fallback");
    a.scale=2;check(b.restore(a.save())&&b.scale==SessionState::maxScale,"scale clamp");
    std::cout<<"Session state PASS\n";
}
