#pragma once
#include "dsp/Engine.hpp"
#include <bit>
#include <cstdint>
#include <span>
#include <string_view>
namespace cyberator {
enum class Param : std::size_t { granular,size,density,position,scatter,pitch,reverse,feedback,spread,formant,android,spectral,harmony,ghost,swell,delay,space,motion,freeze,sync,mix,output,mode,count };
struct ParamSpec { std::string_view id; float low,high,initial; bool discrete; };
inline constexpr std::array<ParamSpec,static_cast<std::size_t>(Param::count)> specs{{
    {"granular",0,1,0,false},{"size",10,250,60,false},{"density",0,1,.5f,false},
    {"position",0,1,0,false},{"scatter",0,1,0,false},{"pitch",-24,24,0,false},
    {"reverse",0,1,0,false},{"feedback",0,1,0,false},{"spread",0,1,0,false},
    {"formant",-12,12,0,false},{"android",0,1,0,false},{"spectral",0,1,0,false},
    {"harmony",0,1,0,false},{"ghost",0,1,0,false},{"swell",0,1,0,false},
    {"delay",0,1,0,false},{"space",0,1,0,false},{"motion",0,1,0,false},
    {"freeze",0,1,0,true},{"sync",0,1,0,true},{"mix",0,1,1,false},
    {"output",-24,12,0,false},{"mode",0,9,0,true}
}};
class Parameters final {
public:
    static constexpr std::size_t count=specs.size(), bytes=8+4*count+4;
    using State=std::array<std::uint8_t,bytes>;
    Parameters() noexcept {for(std::size_t i=0;i<count;++i)values_[i]=specs[i].initial;}
    [[nodiscard]] bool set(Param param,float x) noexcept {
        const auto i=static_cast<std::size_t>(param);
        if(i>=count||!std::isfinite(x))return false;
        const auto& s=specs[i];x=std::clamp(x,s.low,s.high);values_[i]=s.discrete?std::round(x):x;return true;
    }
    [[nodiscard]] float get(Param param)const noexcept {const auto i=static_cast<std::size_t>(param);return i<count?values_[i]:0;}
    [[nodiscard]] bool setNormalized(Param param,float x) noexcept {
        const auto i=static_cast<std::size_t>(param);if(i>=count||!std::isfinite(x))return false;
        const auto& s=specs[i];return set(param,s.low+(s.high-s.low)*std::clamp(x,0.f,1.f));
    }
    [[nodiscard]] EngineParams engine(double bpm=120)const noexcept {
        EngineParams p;using P=Param;
        p.granular=get(P::granular);p.grain.sizeMs=get(P::size);p.grain.density=get(P::density);p.grain.position=get(P::position);p.grain.scatter=get(P::scatter);p.grain.pitch=get(P::pitch);p.grain.reverse=get(P::reverse);p.grain.feedback=get(P::feedback);p.grain.spread=get(P::spread);
        p.cyber={get(P::formant),get(P::android),get(P::spectral)};p.harmony=get(P::harmony);p.ghost=get(P::ghost);p.swell=get(P::swell);p.delay=get(P::delay);p.space=get(P::space);p.motion=get(P::motion);p.grain.freeze=get(P::freeze)>0;p.grain.sync=get(P::sync)>0;p.grain.bpm=std::isfinite(bpm)?std::clamp(bpm,20.,400.):120.;p.mix=get(P::mix);p.outputDb=get(P::output);p.mode=static_cast<Mode>(static_cast<unsigned>(get(P::mode)));return p;
    }
    [[nodiscard]] State save()const noexcept {
        State b{};put(b,0,0x41594243u);put(b,4,1);
        for(std::size_t i=0;i<count;++i)put(b,8+4*i,std::bit_cast<std::uint32_t>(values_[i]));
        put(b,bytes-4,hash(std::span<const std::uint8_t>(b).first(bytes-4)));return b;
    }
    [[nodiscard]] bool restore(std::span<const std::uint8_t> b)noexcept {
        if(b.size()!=bytes||word(b,0)!=0x41594243u||word(b,4)!=1||word(b,bytes-4)!=hash(b.first(bytes-4)))return false;
        Parameters next;
        for(std::size_t i=0;i<count;++i){const float x=std::bit_cast<float>(word(b,8+4*i));const auto& s=specs[i];if(!std::isfinite(x)||x<s.low||x>s.high||(s.discrete&&x!=std::round(x)))return false;next.values_[i]=x;}
        *this=next;return true;
    }
private:
    static void put(State& b,std::size_t at,std::uint32_t x)noexcept {for(unsigned j=0;j<4;++j)b[at+j]=static_cast<std::uint8_t>(x>>(8*j));}
    static std::uint32_t word(std::span<const std::uint8_t> b,std::size_t at)noexcept {std::uint32_t x=0;for(unsigned j=0;j<4;++j)x|=std::uint32_t(b[at+j])<<(8*j);return x;}
    static std::uint32_t hash(std::span<const std::uint8_t> b)noexcept {std::uint32_t h=2166136261u;for(auto x:b)h=(h^x)*16777619u;return h;}
    std::array<float,count> values_{};
};
} // namespace cyberator
