#pragma once
#include "Parameters.hpp"
namespace cyberator {
// Legacy 104-byte parameter payload is unchanged; versioned UI suffix.
struct SessionState {
    static constexpr float minScale=.65f,maxScale=1.5f,defaultScale=.85f;
    static constexpr std::size_t bytes=Parameters::bytes+12;
    using Bytes=std::array<std::uint8_t,bytes>;
    Parameters parameters;
    float scale{defaultScale};
    [[nodiscard]] Bytes save()const noexcept {
        Bytes b{};auto p=parameters.save();std::copy(p.begin(),p.end(),b.begin());
        put(b,Parameters::bytes,0x31495543u);
        put(b,Parameters::bytes+4,std::bit_cast<std::uint32_t>(std::isfinite(scale)?std::clamp(scale,minScale,maxScale):defaultScale));
        put(b,bytes-4,checksum(std::span<const std::uint8_t>(b).first(bytes-4)));return b;
    }
    [[nodiscard]] bool restore(std::span<const std::uint8_t> b)noexcept {
        if(b.size()!=Parameters::bytes&&b.size()!=bytes)return false;
        SessionState next;
        if(!next.parameters.restore(b.first(Parameters::bytes)))return false;
        if(b.size()==bytes){
            if(word(b,Parameters::bytes)!=0x31495543u||word(b,bytes-4)!=checksum(b.first(bytes-4)))return false;
            next.scale=std::bit_cast<float>(word(b,Parameters::bytes+4));
            if(!std::isfinite(next.scale)||next.scale<minScale||next.scale>maxScale)return false;
        }
        *this=next;return true;
    }
private:
    static void put(Bytes& b,std::size_t at,std::uint32_t x)noexcept{for(unsigned j=0;j<4;++j)b[at+j]=static_cast<std::uint8_t>(x>>(8*j));}
    static std::uint32_t word(std::span<const std::uint8_t> b,std::size_t at)noexcept{std::uint32_t x=0;for(unsigned j=0;j<4;++j)x|=std::uint32_t(b[at+j])<<(8*j);return x;}
    static std::uint32_t checksum(std::span<const std::uint8_t> b)noexcept{std::uint32_t h=2166136261u;for(auto x:b)h=(h^x)*16777619u;return h;}
};
} // namespace cyberator
