#pragma once
#include "control/Parameters.hpp"
namespace cyberator::panel {
struct Rect {int x,y,w,h;};
struct Knob {Param param;const char* name;Rect bounds;char family;int diameter;};
inline constexpr int width=1200,height=730;
inline constexpr Rect logo{16,12,128,64},preset{760,26,320,38},previous{1088,26,42,38},next{1136,26,42,38};
inline constexpr Rect granular{16,102,356,374},signal{384,102,432,374},cybernetic{828,102,356,374};
inline constexpr Rect ghost{16,488,300,226},space{328,488,300,226},global{640,488,544,226};
inline constexpr Rect mode{852,420,308,36},freeze{1030,553,138,40},sync{1030,607,138,40};
inline constexpr Rect display{400,143,400,315};
inline constexpr std::array<Knob,20> knobs{{
    {Param::granular,"AMOUNT",{30,146,100,102},'s',64},
    {Param::size,"SIZE",{144,146,100,102},'s',64},
    {Param::density,"DENSITY",{258,146,100,102},'s',64},
    {Param::position,"POSITION",{30,254,100,102},'s',64},
    {Param::scatter,"SCATTER",{144,254,100,102},'s',64},
    {Param::pitch,"PITCH",{258,254,100,102},'s',64},
    {Param::reverse,"REVERSE",{30,362,100,102},'s',64},
    {Param::feedback,"FEEDBACK",{144,362,100,102},'s',64},
    {Param::spread,"SPREAD",{258,362,100,102},'s',64},
    {Param::formant,"FORMANT",{858,143,126,132},'m',86},
    {Param::android,"ANDROID",{1028,143,126,132},'m',86},
    {Param::spectral,"SPECTRAL",{858,277,126,132},'m',86},
    {Param::harmony,"HARMONY",{1028,277,126,132},'m',86},
    {Param::ghost,"GHOST",{34,544,130,150},'m',88},
    {Param::swell,"SWELL",{170,544,130,150},'m',88},
    {Param::delay,"DELAY",{346,544,130,150},'m',88},
    {Param::space,"SPACE",{482,544,130,150},'m',88},
    {Param::motion,"MOTION",{652,544,110,150},'m',80},
    {Param::mix,"MIX",{766,535,128,164},'l',104},
    {Param::output,"OUTPUT",{898,544,116,150},'m',80}
}};
constexpr bool contains(Rect a,Rect b){return b.x>=a.x&&b.y>=a.y&&b.x+b.w<=a.x+a.w&&b.y+b.h<=a.y+a.h;}
constexpr bool overlaps(Rect a,Rect b){return a.x<b.x+b.w&&a.x+a.w>b.x&&a.y<b.y+b.h&&a.y+a.h>b.y;}
consteval bool valid(){
    std::array<bool,Parameters::count> seen{};
    for(std::size_t i=0;i<knobs.size();++i){
        const auto& k=knobs[i];auto id=static_cast<std::size_t>(k.param);
        if(id>=seen.size()||seen[id]||specs[id].discrete||k.diameter>k.bounds.w||k.diameter+34>k.bounds.h)return false;
        seen[id]=true;
        if(!(contains(granular,k.bounds)||contains(cybernetic,k.bounds)||contains(ghost,k.bounds)||contains(space,k.bounds)||contains(global,k.bounds)))return false;
        for(std::size_t j=0;j<i;++j)if(overlaps(k.bounds,knobs[j].bounds))return false;
        if(overlaps(k.bounds,mode)||overlaps(k.bounds,freeze)||overlaps(k.bounds,sync)||overlaps(k.bounds,display))return false;
    }
    for(std::size_t i=0;i<seen.size();++i)if(!specs[i].discrete&&!seen[i])return false;
    return true;
}
static_assert(valid());
} // namespace cyberator::panel
