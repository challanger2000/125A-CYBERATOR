#include "gui/CaptureDemand.hpp"
#include <cstdlib>
#include <iostream>
#include <thread>
using namespace cyberator;
void check(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    CaptureDemand d;check(!d.active()&&!d.accepts(0)&&!d.beginBlock(),"initial closed");
    d.setReaderActive(true);check(!d.active()&&d.beginBlock()&&d.active(),"first open on audio boundary");
    auto first=d.applied();check(d.accepts(first)&&!d.beginBlock(),"current frame accepted");
    d.setReaderActive(true);check(!d.beginBlock(),"duplicate notification unchanged");
    d.setReaderActive(false);check(!d.accepts(first)&&d.active(),"closed frame rejected immediately");
    d.setReaderActive(true);check(!d.accepts(first)&&d.beginBlock()&&d.active(),"rapid reopen invalidates stale frame");
    check(d.applied()==first+2&&d.accepts(d.applied()),"new epoch");
    d.setReaderActive(false);check(d.beginBlock()&&!d.active(),"stop capture at block boundary");
    std::atomic<bool> done{false};std::thread gui([&]{for(int i=0;i<10000;++i){d.setReaderActive(true);d.setReaderActive(false);}done.store(true);});
    unsigned previous=d.applied();do{(void)d.beginBlock();auto now=d.applied();check(now>=previous&&d.active()==bool(now&1u),"audio observes monotonic coherent generation");previous=now;}while(!done.load());gui.join();
    (void)d.beginBlock();check(!d.active()&&d.applied()==first+3+20000,"no lost lifecycle notifications");
    std::cout<<"Capture demand lifecycle PASS\n";
}
