#include "gui/SignalCapture.hpp"
#include <cstdlib>
#include <iostream>
using namespace cyberator;
void check(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    SignalCapture c;SignalCapture::Snapshot s;
    c.reset(48000);check(!c.beginBlock(48000,123)&&!c.readForReader(s),"no closed reader work");
    c.setReaderActive(true);check(c.beginBlock(48000,123),"open capture");
    for(std::size_t i=0;i<SignalCapture::frames;++i)c.push({.1f,.2f},{.3f,.4f},{},{},123+i/1024);
    check(c.readForReader(s)&&s.grains==1&&s.wet[0]==.3f&&s.peakR==.4f,"fresh PCM and launch baseline");
    auto serial=s.serial;
    for(std::size_t i=0;i<100;++i)c.push({.9f,.9f},{.9f,.9f},{},{},130);
    c.setReaderActive(false);c.setReaderActive(true);
    check(!c.readForReader(s),"reopen rejects old published frame before audio");
    check(c.beginBlock(96000,140),"rapid reopen active");
    for(std::size_t i=0;i<SignalCapture::frames-1;++i)c.push({.5f,.6f},{.7f,.8f},{},{},140);
    check(!c.readForReader(s),"old partial frame discarded");
    c.push({.5f,.6f},{.7f,.8f},{},{},141);
    check(c.readForReader(s)&&s.serial>serial&&s.sampleRate==96000&&s.grains==1,"fresh epoch metadata");
    for(auto v:s.wet)check(v==.7f,"only new epoch PCM");
    c.setReaderActive(false);check(!c.readForReader(s)&&!c.beginBlock(96000,500),"close stops capture");
    std::cout<<"Capture lifecycle and PCM PASS\n";
}
