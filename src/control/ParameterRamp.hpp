#pragma once
#include "Parameters.hpp"
namespace cyberator {
// Audio-thread owned. Retarget from the current value, never restart an unchanged ramp.
class ParameterRamp final {
public:
    void prepare(double sr) noexcept {
        duration_=std::size_t(std::round(.010*(std::isfinite(sr)?std::clamp(sr,8000.,192000.):48000.)));
        reset();
    }
    void reset() noexcept {initialized_=false;remaining_={};}
    void target(const Parameters& p) noexcept {
        if(!initialized_){current_=p;target_=p;remaining_={};initialized_=true;return;}
        for(std::size_t i=0;i<Parameters::count;++i){
            const auto id=static_cast<Param>(i);const float next=p.get(id);
            if(next==target_.get(id))continue;
            (void)target_.set(id,next);
            if(specs[i].discrete){(void)current_.set(id,next);remaining_[i]=0;}
            else remaining_[i]=duration_;
        }
    }
    [[nodiscard]] const Parameters& advance() noexcept {
        for(std::size_t i=0;i<Parameters::count;++i)if(remaining_[i]){
            const auto id=static_cast<Param>(i);const double end=target_.get(id),start=current_.get(id);
            const double next=remaining_[i]==1?end:start+(end-start)/double(remaining_[i]);
            (void)current_.set(id,float(next));--remaining_[i];
        }
        return current_;
    }
private:
    Parameters current_,target_;
    std::array<std::size_t,Parameters::count> remaining_{};
    std::size_t duration_{480};bool initialized_{};
};
} // namespace cyberator
