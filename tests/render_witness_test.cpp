#include "support/RenderWitness.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cyberator::test;
void check(bool v,const char* m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    const std::array<float,4> ref{.5f,-.5f,.5f,-.5f},gain{.25f,-.25f,.25f,-.25f},different{.5f,.5f,-.5f,-.5f},zero{};
    auto a=compare(ref,ref);check(a.energy==1&&a.referenceEnergy==1&&a.maxPeak==.5&&a.cross==1&&a.maxError==0&&a.relativeError==0&&a.shapeError==0,"identity oracle");
    check(recallMatches(a)&&!audibleChange(a),"identity not effect");
    auto b=compare(gain,ref);check(b.energy==.25&&b.relativeError==.5&&b.shapeError==0&&!audibleChange(b)&&!recallMatches(b),"gain only rejected");
    auto c=compare(different,ref);check(c.cross==0&&c.shapeError==1&&audibleChange(c)&&!recallMatches(c),"orthogonal witness");
    check(!audibleChange(compare(zero,ref))&&!recallMatches(compare(zero,zero)),"silence never passes");
    auto bad=ref;bad[0]=std::numeric_limits<float>::quiet_NaN();bool rejected=false;try{(void)compare(bad,ref);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"nonfinite rejected");
    rejected=false;try{(void)compare(std::span(ref).first(3),ref);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"different lengths rejected");
    std::cout<<"Render witness positive and adversarial oracle PASS\n";
}
