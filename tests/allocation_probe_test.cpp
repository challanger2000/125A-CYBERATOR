#include "support/AllocationProbe.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
using namespace cyberator::test;
void check(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    auto* before=::operator new(32);
    {AllocationProbe probe;const auto c=probe.finish();check(c.allocations==0&&c.frees==0,"empty probe");}
    std::thread other([]{auto* p=::operator new(64);::operator delete(p);});other.join();
    // Explicit replaceable allocation calls; volatile function pointers prevent
    // compiler elision from defeating the negative control.
    void* (*volatile ordinary)(std::size_t)=::operator new;
    void (*volatile drop)(void*)noexcept=::operator delete;
    void* (*volatile aligned)(std::size_t,std::align_val_t)=::operator new;
    void (*volatile dropAligned)(void*,std::align_val_t)noexcept=::operator delete;
    AllocationProbe probe;
    auto* p=ordinary(16);auto* q=aligned(64,std::align_val_t(64));
    drop(p);dropAligned(q,std::align_val_t(64));drop(before);
    const auto c=probe.finish();check(c.allocations==2&&c.frees==3,"intentional allocation/free detected");
    {AllocationProbe again;const auto r=again.finish();check(r.allocations==0&&r.frees==0,"new scope resets counts");}
    std::cout<<"Allocation probe positive and intentional allocation negative control PASS\n";
}
