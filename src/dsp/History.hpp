#pragma once
#include <cmath>
#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace cyberator {
struct StereoFrame { float left{}, right{}; };

// Single audio-thread owner. Prepare only while processing is stopped.
// Age 0 = most recently captured frame; unwritten ages read as silence.
class History final {
public:
    void prepare(std::size_t capacity, std::size_t mirror = 47) {
        if (capacity == 0) throw std::invalid_argument("History capacity");
        mirror = std::min(mirror, capacity);
        if (capacity > std::vector<StereoFrame>().max_size() - mirror) throw std::length_error("History capacity");
        std::vector<StereoFrame> replacement(capacity + mirror);
        frames_.swap(replacement);
        capacity_ = capacity; padding_ = mirror;
        reset();
    }
    void reset() noexcept { next_ = valid_ = 0; }
    void capture(StereoFrame frame, bool freeze = false) noexcept {
        if (freeze || frames_.empty()) return;
        frame.left = std::isfinite(frame.left) ? frame.left : 0.0f;
        frame.right = std::isfinite(frame.right) ? frame.right : 0.0f;
        frames_[padding_ + next_] = frame;
        if (capacity_ - next_ <= padding_) frames_[padding_ - (capacity_ - next_)] = frame;
        if (++next_ == capacity_) next_ = 0;
        if (valid_ < capacity_) ++valid_;
    }
    [[nodiscard]] StereoFrame read(std::size_t age) const noexcept {
        if (age >= valid_) return {};
        const auto distance = age + 1;
        const auto index = next_ >= distance ? next_ - distance
                                           : capacity_ - (distance - next_);
        return frames_[padding_ + index];
    }
    [[nodiscard]] std::size_t valid() const noexcept { return valid_; }
    // A contiguous backwards view, valid until the next write/reset/prepare.
    // The prefix mirrors the configured final slots; no per-tap ring branch is needed.
    [[nodiscard]] const StereoFrame* backwardWindow(std::size_t age, std::size_t count) const noexcept {
        if (count == 0 || count > padding_ + 1 || age >= valid_ || count > valid_ - age) return nullptr;
        const auto distance = age + 1;
        const auto index = next_ >= distance ? next_ - distance : capacity_ - (distance - next_);
        return frames_.data() + padding_ + index;
    }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
private:
    std::vector<StereoFrame> frames_;
    std::size_t padding_{};
    std::size_t next_{}, valid_{}, capacity_{};
};
} // namespace cyberator
