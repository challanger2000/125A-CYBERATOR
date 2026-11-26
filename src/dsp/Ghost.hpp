#pragma once
#include "GrainBank.hpp"
#include <cstdint>

namespace cyberator {
// Separate aligned dry, post-echo and anticipatory convolution outputs.
struct GhostFrame { StereoFrame dry{}, echo{}, swell{}; };
class Ghost final {
public:
    static constexpr std::size_t tapCount=96;
    void prepare(double sr) {
        if (!std::isfinite(sr)||sr<8000||sr>192000) throw std::invalid_argument("sample rate");
        const auto latency=std::size_t(std::round(sr*.160));
        const auto window=2*std::size_t(std::round(sr*.090));
        History h; h.prepare(latency+4*window+2);
        std::vector<double> w(window);
        for(std::size_t i=0;i<window;++i) {
            const auto s=std::sin(std::numbers::pi*double(i)/double(window)); w[i]=s*s;
        }
        std::array<Tap,tapCount> taps{}; std::uint32_t rng=0x71be92a5u;
        auto random=[&] {rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng;};
        double energy=0;
        for(std::size_t i=0;i<tapCount;++i) {
            const auto lo=i*latency/tapCount, hi=(i+1)*latency/tapCount;
            const auto age=lo+random()%(hi-lo);
            const double gain=std::exp(-std::log(100.0)*double(latency-1-age)/double(latency-1));
            taps[i]={age, (random()&1u)?gain:-gain, (random()&1u)?gain:-gain}; energy+=gain*gain;
        }
        for(auto& tap:taps) {tap.l/=std::sqrt(energy); tap.r/=std::sqrt(energy);}
        history_=std::move(h); weights_.swap(w); taps_=taps; latency_=latency; window_=window; reset();
    }
    void reset() noexcept {history_.reset(); phase_=0;}
    [[nodiscard]] std::size_t latency() const noexcept {return latency_;}
    [[nodiscard]] std::size_t tail() const noexcept {return latency_+4*window_;}
    [[nodiscard]] GhostFrame process(StereoFrame in,double echoAmount,double swellAmount) noexcept {
        if(!window_) return {};
        history_.capture(in);
        GhostFrame out; out.dry=history_.read(latency_);
        const double echo=amount(echoAmount), swell=amount(swellAmount);
        if(echo>0) {
            double l=0,r=0;
            constexpr std::array<double,3> gains{1,.45,.2};
            for(std::size_t repeat=0;repeat<gains.size();++repeat) {
                for(std::size_t head=0;head<2;++head) {
                    const auto k=(phase_+head*(window_/2))%window_;
                    const auto x=history_.read(latency_+1+2*k+repeat*window_);
                    const double gain=weights_[k]*gains[repeat]; l+=gain*x.left; r+=gain*x.right;
                }
            }
            out.echo={finiteSample(echo*l/1.65),finiteSample(echo*r/1.65)};
        }
        if(swell>0) {
            double l=0,r=0;
            for(const auto& t:taps_) {const auto x=history_.read(t.age);l+=t.l*x.left;r+=t.r*x.right;}
            out.swell={finiteSample(swell*l),finiteSample(swell*r)};
        }
        if(++phase_==window_) phase_=0;
        return out;
    }
private:
    struct Tap {std::size_t age{};double l{},r{};};
    static double amount(double x) noexcept {return std::isfinite(x)?std::clamp(x,0.0,1.0):0.0;}
    History history_; std::vector<double> weights_; std::array<Tap,tapCount> taps_{};
    std::size_t latency_{},window_{},phase_{};
};
} // namespace cyberator
