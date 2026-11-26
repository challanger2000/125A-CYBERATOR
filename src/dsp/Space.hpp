#pragma once
#include "GrainBank.hpp"

namespace cyberator {
struct SpaceFrame { StereoFrame delay{}, reverb{}; };
// Warm parallel wet paths. Physical constants are provisional voicing choices.
class Space final {
    struct Line {
        std::vector<double> data;
        std::size_t pos{};
        void prepare(std::size_t n) { data.assign(n,0.0); pos=0; }
        void reset() noexcept { std::fill(data.begin(),data.end(),0.0); pos=0; }
        [[nodiscard]] double read() const noexcept { return data[pos]; }
        void write(double x) noexcept { data[pos]=std::abs(x)<1e-30?0.0:x; if(++pos==data.size())pos=0; }
        double allpass(double x) noexcept { const double z=read(); const double y=z-.6*x; write(x+.6*y); return y; }
    };
public:
    void prepare(double sr) {
        if(!std::isfinite(sr)||sr<8000||sr>192000) throw std::invalid_argument("sample rate");
        Space fresh;
        fresh.delayFrames_=std::size_t(std::round(.250*sr));
        for(auto& d:fresh.delay_)d.prepare(fresh.delayFrames_);
        constexpr std::array<double,8> times{.0297,.0331,.0379,.0411,.0437,.0479,.0533,.0593};
        for(std::size_t i=0;i<times.size();++i){
            const auto n=std::size_t(std::round(times[i]*sr));
            fresh.tank_[i].prepare(n);
            fresh.gain_[i]=std::pow(10.,-3.*double(n)/(sr*4.));
        }
        constexpr std::array<double,4> diffusion{.0047,.0073,.0059,.0091};
        for(std::size_t i=0;i<diffusion.size();++i)fresh.diff_[i].prepare(std::size_t(std::round(diffusion[i]*sr)));
        fresh.alpha_=1-std::exp(-2*std::numbers::pi*std::min(6500.,sr*.35)/sr);
        *this=std::move(fresh);
    }
    void reset() noexcept {
        for(auto& d:delay_)d.reset();
        for(auto& d:tank_)d.reset();
        for(auto& d:diff_)d.reset();
        damp_={};
    }
    [[nodiscard]] std::size_t delayFrames() const noexcept {return delayFrames_;}
    [[nodiscard]] SpaceFrame process(StereoFrame input,double delayAmount,double spaceAmount) noexcept {
        if(!delayFrames_)return {};
        const double l=finiteSample(input.left),r=finiteSample(input.right);
        const double dl=delay_[0].read(),dr=delay_[1].read();
        delay_[0].write(l+.45*dr);delay_[1].write(r+.45*dl);
        const double a=diff_[1].allpass(diff_[0].allpass(l));
        const double b=diff_[3].allpass(diff_[2].allpass(r));
        std::array<double,8> v{};
        double left=0,right=0;
        for(std::size_t i=0;i<v.size();++i){
            const double z=tank_[i].read();
            left+=z;right+=(i%2? -z:z);
            damp_[i]+=alpha_*(z-damp_[i]);
            if(std::abs(damp_[i])<1e-30)damp_[i]=0;
            v[i]=gain_[i]*damp_[i];
        }
        // Normalized Hadamard feedback matrix; spectral norm = 1.
        for(std::size_t h=1;h<v.size();h*=2)for(std::size_t i=0;i<v.size();i+=2*h)for(std::size_t j=0;j<h;++j){
            const double x=v[i+j],y=v[i+j+h];v[i+j]=x+y;v[i+j+h]=x-y;
        }
        constexpr double norm=.3535533905932737622;
        for(std::size_t i=0;i<v.size();++i)tank_[i].write(norm*(v[i]+a+(i%2?-b:b)));
        const double d=amount(delayAmount),s=amount(spaceAmount);
        return {{finiteSample(d*dl),finiteSample(d*dr)}, {finiteSample(s*norm*left),finiteSample(s*norm*right)}};
    }
private:
    static double amount(double x) noexcept {return std::isfinite(x)?std::clamp(x,0.,1.):0.;}
    std::array<Line,2> delay_;
    std::array<Line,4> diff_;
    std::array<Line,8> tank_;
    std::array<double,8> gain_{},damp_{};
    std::size_t delayFrames_{};double alpha_{};
};
} // namespace cyberator
