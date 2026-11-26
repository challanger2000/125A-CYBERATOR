#pragma once
#include "dsp/GrainBank.hpp"
#include <atomic>
#include <cstdint>

namespace cyberator {
// Audio-thread writer, one GUI reader. Three owned slots, no sample-level atomics,
// blocking, allocation or overwrite of a reader-owned slot. UI may skip frames.
class SignalCapture final {
public:
    static constexpr std::size_t frames=2048;
    struct Snapshot {
        std::array<float,frames> dry{},wet{};
        double sampleRate{};
        float peakL{},peakR{},echoPeak{},swellPeak{};
        std::uint64_t grains{};
    };
    void reset(double rate) noexcept {
        if(writing_>=0)slots_[std::size_t(writing_)].state.store(free,std::memory_order_release);
        writing_=-1;offset_=0;rate_=rate;lastGrains_=0;
        published_.store(-1,std::memory_order_release);
    }
    void push(StereoFrame dry,StereoFrame wet,StereoFrame echo,StereoFrame swell,std::uint64_t launched) noexcept {
        const auto previous=lastGrains_;lastGrains_=launched;
        if(writing_<0){
            const int latest=published_.load(std::memory_order_relaxed);
            for(int i=0;i<3;++i){
                if(i==latest)continue;
                auto& state=slots_[std::size_t(i)].state;
                int expected=state.load(std::memory_order_relaxed);
                if((expected==free||expected==ready)&&state.compare_exchange_strong(expected,writing,std::memory_order_acquire)){
                    writing_=i;offset_=0;startGrains_=previous;
                    auto& s=slots_[std::size_t(i)].data;
                    s.sampleRate=rate_;s.peakL=s.peakR=s.echoPeak=s.swellPeak=0;s.grains=0;
                    break;
                }
            }
            if(writing_<0)return;
        }
        auto& slot=slots_[std::size_t(writing_)];auto& s=slot.data;
        s.dry[offset_]=finiteSample(dry.left);s.wet[offset_]=finiteSample(wet.left);
        s.peakL=std::max(s.peakL,std::abs(finiteSample(wet.left)));s.peakR=std::max(s.peakR,std::abs(finiteSample(wet.right)));
        s.echoPeak=std::max({s.echoPeak,std::abs(finiteSample(echo.left)),std::abs(finiteSample(echo.right))});
        s.swellPeak=std::max({s.swellPeak,std::abs(finiteSample(swell.left)),std::abs(finiteSample(swell.right))});
        if(++offset_==frames){
            s.grains=launched>=startGrains_?launched-startGrains_:0;
            slot.state.store(ready,std::memory_order_release);
            published_.store(writing_,std::memory_order_release);writing_=-1;
        }
    }
    [[nodiscard]] bool read(Snapshot& dest) noexcept {
        const int index=published_.load(std::memory_order_acquire);if(index<0)return false;
        auto& slot=slots_[std::size_t(index)];int expected=ready;
        if(!slot.state.compare_exchange_strong(expected,reading,std::memory_order_acquire))return false;
        dest=slot.data;
        slot.state.store(ready,std::memory_order_release);return true;
    }
private:
    enum : int {free,writing,ready,reading};
    struct Slot {std::atomic<int> state{free};Snapshot data;};
    static_assert(std::atomic<int>::is_always_lock_free);
    std::array<Slot,3> slots_{};std::atomic<int> published_{-1};
    int writing_{-1};std::size_t offset_{};double rate_{48000};std::uint64_t startGrains_{},lastGrains_{};
};
} // namespace cyberator
