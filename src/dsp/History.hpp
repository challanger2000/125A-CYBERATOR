#pragma once
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace cyberator {
struct StereoFrame { float left{}, right{}; };

// Single audio-thread owner. Prepare only while processing is stopped.
// Age 0 = most recently captured frame; unwritten ages read as silence.
class History final {
public:
    void prepare(std::size_t capacity) {
        if (capacity == 0) throw std::invalid_argument("History capacity");
        std::vector<StereoFrame> replacement(capacity);
        frames_.swap(replacement);
        reset();
    }
    void reset() noexcept { next_ = valid_ = 0; }
    void capture(StereoFrame frame, bool freeze = false) noexcept {
        if (freeze || frames_.empty()) return;
        frame.left = std::isfinite(frame.left) ? frame.left : 0.0f;
        frame.right = std::isfinite(frame.right) ? frame.right : 0.0f;
        frames_[next_] = frame;
        if (++next_ == frames_.size()) next_ = 0;
        if (valid_ < frames_.size()) ++valid_;
    }
    [[nodiscard]] StereoFrame read(std::size_t age) const noexcept {
        if (age >= valid_) return {};
        const auto distance = age + 1;
        const auto index = next_ >= distance ? next_ - distance
                                           : frames_.size() - (distance - next_);
        return frames_[index];
    }
    [[nodiscard]] std::size_t valid() const noexcept { return valid_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return frames_.size(); }
private:
    std::vector<StereoFrame> frames_;
    std::size_t next_{}, valid_{};
};
} // namespace cyberator
