#include "control/Parameters.hpp"
#include <cstdlib>
#include <iostream>
using namespace cyberator;
void check(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    Parameters p,q;auto defaultState=p.save();
    auto d=p.engine();check(d.granular==0&&d.mix==1&&d.cyber.formant==0&&d.grain.sizeMs==60,"neutral defaults");
    for(std::size_t i=0;i<specs.size();++i){for(std::size_t j=i+1;j<specs.size();++j)check(specs[i].id!=specs[j].id,"unique ids");check(p.setNormalized(static_cast<Param>(i),.73f),"set all");}
    const auto saved=p.save();check(q.restore(saved)&&q.save()==saved,"all parameter roundtrip");
    for(std::size_t i=0;i<saved.size();++i){auto corrupt=saved;corrupt[i]^=1;check(!q.restore(corrupt)&&q.save()==saved,"corrupt rollback");}
    for(std::size_t i=0;i<saved.size();++i)check(!q.restore(std::span(saved).first(i))&&q.save()==saved,"truncated rollback");
    check(!q.set(Param::mix,std::numeric_limits<float>::quiet_NaN())&&q.save()==saved,"nonfinite rollback");
    check(p.restore(defaultState),"reset state");
    check(p.set(Param::mix,0)&&p.set(Param::output,12)&&p.set(Param::mode,9)&&p.set(Param::android,1)&&p.set(Param::granular,1),"bypass configuration");
    Engine a;a.prepare(48000,LatencyProfile::live);for(std::size_t i=0;i<a.latency()+1;++i){auto y=a.process(i==0?StereoFrame{.5f,-.25f}:StereoFrame{},p.engine());check(y.left==(i==a.latency()?.5f:0)&&y.right==(i==a.latency()?-.25f:0),"schema to PCM bypass");}
    check(p.set(Param::formant,7)&&p.set(Param::pitch,-9)&&p.set(Param::freeze,1)&&p.set(Param::sync,1),"signed discrete values");auto v=p.engine(87);check(v.cyber.formant==7&&v.grain.pitch==-9&&v.grain.freeze&&v.grain.sync&&v.grain.bpm==87&&v.mode==Mode::phantom,"mapping");
    std::cout<<"Parameter state contract PASS\n";
}
