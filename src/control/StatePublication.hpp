#pragma once
#include <atomic>
#include <memory>
#include <optional>
#include <utility>
namespace cyberator {
// Single non-realtime writer, serialized by the caller. Audio readers only
// inspect the lock-free generation; shared ownership is strictly non-realtime.
template<class Bytes> class StatePublication final {
public:
    [[nodiscard]] unsigned generation() const noexcept { return generation_.load(); }
    void begin(const Bytes& target) {
        pending_.store(std::make_shared<const Bytes>(target));
        generation_.fetch_add(1);
    }
    void end() noexcept { generation_.fetch_add(1); }
    // During a restore, including synchronous host reentry, return a complete
    // requested target rather than recursively inspecting a half-updated tree.
    // A save overlapping a restore may linearize at that transaction's target.
    template<class Read> [[nodiscard]] std::optional<Bytes> save(Read&& read) const {
        const auto start=generation();
        if((start&1u)==0){
            auto result=std::forward<Read>(read)();
            if(start==generation())return result;
        }
        if(auto target=pending_.load())return *target;
        return std::nullopt;
    }
private:
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    std::atomic<unsigned> generation_{0};
    std::atomic<std::shared_ptr<const Bytes>> pending_{};
};
} // namespace cyberator
