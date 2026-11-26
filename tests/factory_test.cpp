#include "control/FactoryBank.hpp"
#include <cstdlib>
#include <iostream>
using namespace cyberator;
void check(bool ok,const char* msg){if(!ok){std::cerr<<msg<<'\n';std::exit(1);}}
int main(){
    std::array<unsigned,7> counts{};
    std::array<Parameters::State,120> states{};
    std::array<std::uint64_t,120> hashes{};
    Engine e;e.prepare(48000,LatencyProfile::live);
    Engine dry;dry.prepare(48000,LatencyProfile::live);
    constexpr int n=48000;
    std::vector<StereoFrame> input(n),reference(n);
    for(int i=0;i<24000;++i){double t=double(i)/48000.;double env=std::pow(std::sin(2*std::numbers::pi*3*t),2);double x=0;
        for(int k=1;k<=16;++k)x+=.16/double(k)*std::sin(2*std::numbers::pi*140*k*t+std::sin(2*std::numbers::pi*4*t));
        input[std::size_t(i)]={float(env*x),float(.8*env*x)};}
    for(int i=0;i<n;++i)reference[std::size_t(i)]=dry.process(input[std::size_t(i)],{});
    double minDelta=1e30,maxPeak=0,minShape=1;
    std::array<std::vector<float>,120> normalized;
    double dryEnergy=0;for(auto x:reference)dryEnergy+=double(x.left)*x.left+double(x.right)*x.right;
    for(std::size_t k=0;k<factoryBank.size();++k){const auto& preset=factoryBank[k];auto p=preset.parameters();auto v=p.engine();
        check(!preset.name.empty()&&!v.grain.freeze,"valid name and no empty freeze startup");++counts[static_cast<unsigned>(preset.category)];
        states[k]=p.save();Parameters roundtrip;check(roundtrip.restore(states[k])&&roundtrip.save()==states[k],"factory state roundtrip");
        for(std::size_t j=0;j<k;++j)check(preset.name!=factoryBank[j].name&&states[k]!=states[j],"unique name and parameter state");
        e.reset();std::uint64_t hash=14695981039346656037ull;double delta=0,peak=0,energy=0,cross=0;
        auto& rendered=normalized[k];rendered.reserve(2*n);
        for(int i=0;i<n;++i){const auto x=e.process(input[std::size_t(i)],v);check(std::isfinite(x.left)&&std::isfinite(x.right),"finite preset output");
            peak=std::max({peak,std::abs(double(x.left)),std::abs(double(x.right))});delta+=std::abs(x.left-reference[std::size_t(i)].left);
            energy+=double(x.left)*x.left+double(x.right)*x.right;cross+=double(x.left)*reference[std::size_t(i)].left+double(x.right)*reference[std::size_t(i)].right;
            rendered.push_back(x.left);rendered.push_back(x.right);
            hash=(hash^std::bit_cast<std::uint32_t>(x.left))*1099511628211ull;hash=(hash^std::bit_cast<std::uint32_t>(x.right))*1099511628211ull;}
        check(delta>1&&peak>1e-5&&peak<1,"nontrivial finite nominal-level preset");
        const double shapeError=std::sqrt(std::max(0.,1-cross*cross/(energy*dryEnergy)));
        check(shapeError>.01,"preset differs from gain-only dry");
        const double scale=1/std::sqrt(energy);for(auto& x:rendered)x=float(x*scale);
        for(std::size_t j=0;j<k;++j){double diff=0;for(std::size_t i=0;i<rendered.size();++i){const double d=double(rendered[i])-normalized[j][i];diff+=d*d;}minShape=std::min(minShape,std::sqrt(diff));}
        minDelta=std::min(minDelta,delta);maxPeak=std::max(maxPeak,peak);hashes[k]=hash;
        for(std::size_t j=0;j<k;++j)check(hashes[j]!=hash,"no exact duplicate PCM renders");
    }
    check(counts==std::array<unsigned,7>{20,20,20,15,15,15,15},"category inventory");
    check(minShape>.01,"pairwise level-normalized distinctness");
    std::cout<<"120 authored preset drafts, min delta "<<minDelta<<", max peak "<<maxPeak<<", minimum level-normalized pair distance "<<minShape<<'\n';
}
