#include "dsp/Harmony.hpp"
#include "reference/HarmonyOriginal.hpp"
#include <chrono>
#include <iostream>
#include <bit>
using namespace cyberator;
template<class G>void probe(double sr){G g;g.prepare(sr);auto n=std::size_t(sr*2);std::vector<StereoFrame> in(n);for(std::size_t i=0;i<n;++i)in[i]={float(.3*std::sin(.017*double(i))),float(.2*std::cos(.029*double(i)))};for(auto x:in)(void)g.process(x,1);auto t=std::chrono::steady_clock::now();std::uint64_t hash=14695981039346656037ull;for(auto x:in){auto y=g.process(x,1);hash=(hash^std::bit_cast<std::uint32_t>(y.left))*1099511628211ull;hash=(hash^std::bit_cast<std::uint32_t>(y.right))*1099511628211ull;}std::cout<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count()<<" ms hash "<<hash<<'\n';}
int main(){for(int rep=0;rep<3;++rep)for(double sr:{48000.,192000.}){std::cout<<sr<<" old ";probe<HarmonyOriginal>(sr);std::cout<<sr<<" new ";probe<Harmony>(sr);}}
