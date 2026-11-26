#pragma once
#include <array>
#include <cstddef>
namespace cyberator::ui {
// Measured union of nonzero-alpha bounds over all 128 frames of each pinned asset.
// Square crop contains the entire visible artwork plus at least one transparent pixel.
struct KnobCrop {int resolution,frame,x,y,size;};
inline constexpr std::array<std::array<KnobCrop,4>,3> knobCrops{{
    {{{100,64,17,18,29},{150,96,27,28,42},{200,128,37,38,54},{300,192,55,57,81}}},
    {{{100,96,25,27,45},{150,144,38,40,67},{200,192,51,54,89},{300,288,77,81,133}}},
    {{{100,128,32,34,64},{150,192,49,52,94},{200,256,64,69,127},{300,384,98,105,188}}}
}};
[[nodiscard]] constexpr const KnobCrop& chooseCrop(std::size_t family,float devicePixels) noexcept {
    const auto& row=knobCrops[family<knobCrops.size()?family:0];
    for(const auto& crop:row)if(float(crop.size)>=devicePixels)return crop;
    return row.back();
}
consteval bool validCrops(){
    for(const auto& row:knobCrops){int previous=0;
        for(const auto& c:row){if(c.size<=previous||c.x<0||c.y<0||c.x+c.size>c.frame||c.y+c.size>c.frame)return false;previous=c.size;}
    }
    return true;
}
static_assert(validCrops());
} // namespace cyberator::ui
