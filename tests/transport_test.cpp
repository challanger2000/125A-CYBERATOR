#include "dsp/Granular.hpp"
#include <cstdlib>
#include <iostream>
using namespace cyberator;
void check(bool ok,const char* msg){if(!ok){std::cerr<<msg<<'\n';std::exit(1);}}
int main(){
    for(double sr:{8000.,44100.,48000.,96000.,192000.}){
        for(double bpm:{60.,120.,137.}){
            Granular g;g.prepare(sr);GranularParams p;p.sizeMs=10;p.density=0;
            for(int i=0;i<int(sr/4);++i)(void)g.process({.5f,-.25f},p);
            p.density=.5;p.sync=true;p.freeze=true;
            const double step=bpm/(60*sr), start=-.371;
            // Independent event oracle: integer eighth-beat boundaries converted to sample indices.
            std::vector<int> expected;
            const int frames=int(sr);
            for(int k=-32;k<64;++k){
                const int i=int(std::ceil((double(k)/8-start)/step-1e-7));
                if(i>=0&&i<frames)expected.push_back(i);
            }
            std::vector<int> actual;double energy=0;
            // Irregular partitioning; recompute each block's PPQ from its absolute index.
            for(int offset=0;offset<frames;){const int n=std::min(113,frames-offset);
                const double blockPpq=start+step*offset;
                for(int j=0;j<n;++j){p.transport={blockPpq+step*j,bpm,true,true};auto before=g.launches();auto y=g.process({},p);
                    if(g.launches()!=before)actual.push_back(offset+j);
                    energy+=std::abs(y.left);check(std::abs(y.right+.5*y.left)<1e-6,"frozen stereo");}
                offset+=n;
            }
            check(actual==expected,"absolute PPQ event alignment");check(energy>1,"frozen grain audio");
            auto before=g.launches();p.transport={3.031,bpm,true,true};(void)g.process({},p);check(g.launches()==before,"seek between ticks does not launch");
            p.transport={-1.25,bpm,true,true};(void)g.process({},p);check(g.launches()==before+1,"negative PPQ loop boundary launches");
        }
    }
    // Freeze repeats the same selected micro-grain; incoming audio cannot change it.
    Granular a,b;a.prepare(48000);b.prepare(48000);GranularParams p;p.density=0;p.sizeMs=10;
    for(int i=0;i<12000;++i){float x=float(.4*std::sin(.071*i)+.2*std::cos(.137*i));(void)a.process({x,-x},p);(void)b.process({x,-x},p);}
    p.freeze=p.sync=true;p.density=.5;std::vector<float> out;double sum=0;
    for(int i=0;i<12000;++i){p.transport={double(i)/24000,120,true,true};auto x=a.process({1,1},p),y=b.process({-1,0},p);check(x.left==y.left&&x.right==y.right,"freeze excludes new input");out.push_back(x.left);sum+=std::abs(x.left);}
    check(sum>1,"microloop nonzero");for(int i=0;i<9000;++i)check(out[std::size_t(i)]==out[std::size_t(i+3000)],"repeat frozen grain PCM");
    check(gridDivision(0)==0&&gridDivision(.5)==8&&gridDivision(1)==16,"grid mapping");
    check(!gridTick({0,120,false,true},48000,8),"stopped clock ignored");
    check(!gridTick({std::numeric_limits<double>::quiet_NaN(),120,true,true},48000,8),"invalid PPQ ignored");
    std::cout<<"Transport and frozen microloop PASS\n";
}
