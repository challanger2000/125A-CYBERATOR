#pragma once
#include "GrainBank.hpp"
#include "TempoGrid.hpp"
#include <cstdint>
#include <stdexcept>

namespace cyberator {
struct GranularParams {
    double sizeMs{60}, density{0.5}, position{}, scatter{}, pitch{}, reverse{}, spread{}, feedback{};
    bool freeze{}, sync{};
    double bpm{120};
    TransportSample transport{};
};
// Provisional physical parameter mapping; the host adapter owns parameter publication.
class Granular final {
public:
    void prepare(double sr) {
        if (!std::isfinite(sr) || sr < 8000 || sr > 192000) throw std::invalid_argument("sample rate");
        bank_.prepare(std::size_t(std::ceil(3*sr)) + 64);
        sr_=sr; reset();
    }
    void reset(std::uint32_t seed=1) noexcept {
        bank_.reset(); rng_=seed ? seed : 1; phase_=1; prev_={}; launches_=0;
    }
    [[nodiscard]] StereoFrame process(StereoFrame in, const GranularParams& p) noexcept {
        if (sr_==0) return {};
        const double density=bounded(p.density,0,1,0);
        if (density>0 && p.sync && usableGrid(p.transport)) {
            if (gridTick(p.transport,sr_,gridDivision(density)) && launch(p)) ++launches_;
            phase_=1; // no off-grid backlog when transport validity changes
        } else if (density>0) {
            const double hz=density*(p.sync ? 16*bounded(p.bpm,20,400,120)/60 : 64);
            if (phase_>=1) {
                // At most one launch attempt per sample. Retain a pending tick while history is short.
                if (launch(p)) { phase_-=1; ++launches_; }
            }
            phase_=std::min(1.0+hz/sr_,phase_+hz/sr_);
        } else phase_=1;
        const double fb=0.85*bounded(p.feedback,0,1,0);
        if (fb>0) {
            in.left=finiteSample(double(in.left)+fb*soft(prev_.left));
            in.right=finiteSample(double(in.right)+fb*soft(prev_.right));
        }
        prev_=bank_.process(in,p.freeze);
        return density==0 ? StereoFrame{} : prev_;
    }
    [[nodiscard]] std::uint64_t launches() const noexcept { return launches_; }
private:
    static double bounded(double x,double lo,double hi,double fallback) noexcept {
        return std::isfinite(x) ? std::clamp(x,lo,hi) : fallback;
    }
    static double soft(double x) noexcept { return x/(1+std::abs(x)); }
    double random() noexcept {
        rng_^=rng_<<13; rng_^=rng_>>17; rng_^=rng_<<5;
        return double(rng_)/4294967296.0;
    }
    bool launch(const GranularParams& p) noexcept {
        const auto length=std::size_t(std::round(sr_*0.001*bounded(p.sizeMs,10,250,60)));
        const double magnitude=std::exp2(bounded(p.pitch,-24,24,0)/12);
        const double direction=random()<bounded(p.reverse,0,1,0) ? -1.0 : 1.0;
        const double rate=magnitude*direction, span=double(length-1);
        const double low=SincReader::before+std::max(0.0,rate*span);
        const double high=double(bank_.captured())-SincReader::after-2-std::max(0.0,(1-rate)*span);
        if (high<low) return false;
        const double position=bounded(p.position,0,1,0);
        const double scatter=bounded(p.scatter,0,1,0);
        const double age=low+(high-low)*std::clamp(position+scatter*(2*random()-1),0.0,1.0);
        const double pan=bounded(p.spread,0,1,0)*(2*random()-1);
        return bank_.trigger(age,rate,length,pan);
    }
    GrainBank bank_;
    double sr_{},phase_{1};
    std::uint32_t rng_{1};
    std::uint64_t launches_{};
    StereoFrame prev_{};
};
} // namespace cyberator
