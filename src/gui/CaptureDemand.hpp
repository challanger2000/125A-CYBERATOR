#pragma once
#include <atomic>
namespace cyberator {
// Single GUI subscriber; only the audio owner updates its applied generation.
// The generation rejects an old frame even when close/reopen occurs between blocks.
class CaptureDemand final {
public:
    void setReaderActive(bool active) noexcept {
        const auto g=requested_.load(std::memory_order_relaxed);
        if(bool(g&1u)!=active)requested_.store(g+1u,std::memory_order_release);
    }
    [[nodiscard]] unsigned requested()const noexcept{return requested_.load(std::memory_order_acquire);}
    [[nodiscard]] bool beginBlock()noexcept{
        const auto g=requested();const bool changed=g!=applied_;applied_=g;return changed;
    }
    [[nodiscard]] unsigned applied()const noexcept{return applied_;}
    [[nodiscard]] bool active()const noexcept{return bool(applied_&1u);}
    [[nodiscard]] bool accepts(unsigned frameGeneration)const noexcept{
        const auto g=requested();return (g&1u)!=0&&g==frameGeneration;
    }
private:
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    std::atomic<unsigned> requested_{0};unsigned applied_{};
};
} // namespace cyberator
