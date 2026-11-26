#include "control/StatePublication.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <thread>
using Bytes=std::array<unsigned,4>;
using Publication=cyberator::StatePublication<Bytes>;
void check(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    Publication p;const Bytes a{1,1,1,1},b{2,2,2,2},c{3,3,3,3};
    check(p.save([&]{return std::optional{a};})==a,"normal save");
    p.begin(b);bool called=false;
    check(p.save([&]{called=true;return std::optional{Bytes{1,2,1,2}};})==b&&!called,"reentrant save never reads partial tree");
    check((p.generation()&1u)==1,"audio sees in-flight transaction");p.end();
    check(p.save([&]{return std::optional{b};})==b,"settled save");
    check(p.save([&]{p.begin(c);p.end();return std::optional{Bytes{2,2,2,3}};})==c,"save overlapping completed restore rejects hybrid");
    check(p.save([&]{return std::optional{a};})==a,"ordinary edits not replaced by stale transaction");
    check(!p.save([]{return std::optional<Bytes>{};}),"invalid ordinary snapshot remains invalid");
    std::array<std::atomic<unsigned>,4> raw{};for(auto& x:raw)x.store(3);
    std::atomic<bool> done{false};
    std::thread writer([&]{for(unsigned i=4;i<20004;++i){p.begin(Bytes{i,i,i,i});for(auto& x:raw)x.store(i);p.end();}done.store(true);});
    std::size_t reads=0;
    do{auto snapshot=p.save([&]{Bytes x{};for(std::size_t i=0;i<x.size();++i)x[i]=raw[i].load();return std::optional{x};});
        check(snapshot&&(*snapshot)[0]==(*snapshot)[1]&&(*snapshot)[1]==(*snapshot)[2]&&(*snapshot)[2]==(*snapshot)[3],"concurrent save never serializes mixed transaction");++reads;
    }while(!done.load());writer.join();
    check(reads>0&&p.save([&]{return std::optional{Bytes{20003,20003,20003,20003}};})==Bytes{20003,20003,20003,20003},"last state exact");
    std::cout<<reads<<" coherent concurrent snapshots; reentrant and overlap witnesses PASS\n";
}
