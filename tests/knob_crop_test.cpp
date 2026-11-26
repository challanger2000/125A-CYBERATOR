#include "gui/KnobCrop.hpp"
#include <cstdlib>
#include <iostream>
using namespace cyberator::ui;
int main(){
    for(std::size_t family=0;family<3;++family){
        const auto& row=knobCrops[family];
        for(std::size_t i=0;i<4;++i){
            if(chooseCrop(family,float(row[i].size)).resolution!=row[i].resolution)return 1;
            if(i<3&&chooseCrop(family,float(row[i].size)+.01f).resolution!=row[i+1].resolution)return 2;
            for(int frame=0;frame<128;++frame){const auto& c=row[i];const int start=frame*c.frame+c.y;
                if(start<frame*c.frame||start+c.size>(frame+1)*c.frame)return 3;}
        }
        if(chooseCrop(family,10000).resolution!=300)return 4;
    }
    // At 100% the visible knob, not its transparent padded frame, determines resolution.
    if(chooseCrop(0,64).resolution!=300||chooseCrop(1,88).resolution!=200||chooseCrop(2,104).resolution!=200)return 5;
    std::cout<<"Knob crop and device resolution selection PASS\n";
}
