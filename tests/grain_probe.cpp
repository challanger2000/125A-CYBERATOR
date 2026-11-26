#include "dsp/GrainBank.hpp"
#include <chrono>
#include <iostream>
#include <bit>
#include <cstdint>
using namespace cyberator;
int main(){
 for(int kind:{0,1,2})for(int count:{8,16,32}){
  GrainBank g;g.prepare(192000);for(int i=0;i<192000;++i)(void)g.process({float(.3*std::sin(.017*i)),float(.2*std::cos(.027*i))},false);
  for(int j=0;j<count;++j){double rate=kind==0?2:kind==1?-.75:1.037; if(!g.trigger(90000+j*.1234567,rate,20000,.2))return 3;}
  auto start=std::chrono::steady_clock::now();std::uint64_t hash=14695981039346656037ull;
  for(int i=0;i<20000;++i){auto v=g.process({float(.3*std::sin(.031*i)),float(.2*std::cos(.039*i))},i%113==0);hash=(hash^std::bit_cast<std::uint32_t>(v.left))*1099511628211ull;hash=(hash^std::bit_cast<std::uint32_t>(v.right))*1099511628211ull;}
  std::cout<<kind<<' '<<count<<" ms "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" hash "<<hash<<'\n';
 }
}
