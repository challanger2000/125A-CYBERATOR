#pragma once
#include <array>
#include <algorithm>
#include <cmath>

namespace cyberator {
// PPQ is the musical position of the current input sample. Missing/stopped
// transport uses the legacy free-running tempo-relative scheduler instead.
struct TransportSample { double ppq{}, bpm{120}; bool playing{}, valid{}; };
inline bool usableGrid(const TransportSample& t) noexcept {
    return t.playing&&t.valid&&std::isfinite(t.ppq)&&std::abs(t.ppq)<=1e6&&
           std::isfinite(t.bpm)&&t.bpm>=20&&t.bpm<=400;
}
inline unsigned gridDivision(double density) noexcept {
    if(!std::isfinite(density)||density<=0)return 0;
    constexpr std::array<unsigned,8> divisions{1,2,3,4,6,8,12,16};
    const double requested=16*std::min(1.,density);
    unsigned best=1;
    for(auto d:divisions)if(std::abs(double(d)-requested)<std::abs(double(best)-requested))best=d;
    return best;
}
// Each tick belongs to the half-open interval (previous sample, current sample].
// Stateless grid evaluation does not emit an arbitrary event on a transport seek.
inline bool gridTick(const TransportSample& t,double sr,unsigned division) noexcept {
    if(!usableGrid(t)||!division||!std::isfinite(sr)||sr<8000||sr>192000)return false;
    const double phase=t.ppq*division,step=t.bpm*division/(60*sr);
    constexpr double eps=1e-8; // grid units; much smaller than the smallest supported step
    return std::floor(phase+eps)>std::floor(phase-step+eps);
}
} // namespace cyberator
