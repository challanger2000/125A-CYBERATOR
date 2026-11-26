#pragma once
#include "GrainBank.hpp"
namespace cyberator {
// Two creative octave voices; shared history, complementary raised-cosine heads.
// This is a delay-line harmonizer, not a formant-preserving pitch correction engine.
class Harmony final {
public:
    void prepare(double sr) {
        if(!std::isfinite(sr)||sr<8000||sr>192000) throw std::invalid_argument("sample rate");
        Harmony fresh; fresh.span_=sr*.04;
        fresh.history_.prepare(std::size_t(std::ceil(fresh.span_))+64);
        fresh.reader_.prepare();
        // Integer-span rates have an exact half-sample phase lattice. Precompute
        // the complementary window and use two exact sinc kernels per voice.
        if(fresh.span_==std::floor(fresh.span_)){
            fresh.period_=2*std::size_t(fresh.span_);
            fresh.window_.resize(fresh.period_);
            for(std::size_t i=0;i<fresh.period_;++i)
                fresh.window_[i]=.5-.5*std::cos(2*std::numbers::pi*double(i)/double(fresh.period_));
        }
        *this=std::move(fresh);
    }
    void reset() noexcept {history_.reset();phase_={};tick_={};}
    [[nodiscard]] StereoFrame process(StereoFrame in,double amount) noexcept {
        if(span_==0)return {};
        history_.capture(in);
        amount=std::isfinite(amount)?std::clamp(amount,0.,1.):0.;
        double l=0,r=0;
        if(period_ && history_.valid()==history_.capacity()){
            for(std::size_t v=0;v<2;++v){
                if(amount>0)for(std::size_t head=0;head<2;++head){
                    auto i=tick_[v]+head*(period_/2);if(i>=period_)i-=period_;
                    const double age=24+(v==0?.5*double(i):span_-.5*double(i));
                    const int band=v==0?0:4;
                    const auto x=reader_.read(history_,age,band,&cache_[v][i%2]);
                    const double w=window_[i];l+=w*x.left;r+=w*x.right;
                }
                tick_[v]+=v+1;if(tick_[v]>=period_)tick_[v]-=period_;
            }
            return {finiteSample(.5*amount*l),finiteSample(.5*amount*r)};
        }
        constexpr std::array<double,2> rates{.5,2.};
        for(std::size_t v=0;v<2;++v){
            if(amount>0){
                for(std::size_t head=0;head<2;++head){
                    double p=phase_[v]+.5*double(head);if(p>=1)p-=1;
                    const double age=24+span_*(rates[v]<1?p:1-p);
                    const double w=.5-.5*std::cos(2*std::numbers::pi*p);
                    const auto x=reader_.read(history_,age,SincReader::bandFor(rates[v]));
                    l+=w*x.left;r+=w*x.right;
                }
            }
            phase_[v]+=std::abs(1-rates[v])/span_;
            if(phase_[v]>=1)phase_[v]-=1;
            if(period_){tick_[v]+=v+1;if(tick_[v]>=period_)tick_[v]-=period_;}
        }
        return {finiteSample(.5*amount*l),finiteSample(.5*amount*r)};
    }
private:
    History history_;SincReader reader_;std::array<double,2> phase_{};double span_{};
    std::size_t period_{};std::array<std::size_t,2> tick_{};
    std::vector<double> window_;
    std::array<std::array<SincReader::KernelCache,2>,2> cache_{};
};
} // namespace cyberator
