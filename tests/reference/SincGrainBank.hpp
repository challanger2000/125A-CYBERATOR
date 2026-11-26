#pragma once
#include "SincHistory.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <limits>
#include <vector>
#include <utility>

namespace cyberator_reference {
inline float finiteSample(double value) noexcept {
    constexpr double limit = std::numeric_limits<float>::max();
    return std::isfinite(value) ? float(std::clamp(value, -limit, limit)) : 0.0f;
}
// Causal age-domain interpolation. The prepared kernels are immutable on the audio thread.
class SincReader final {
public:
    static constexpr int taps = 48, phases = 256, bands = 9;
    static constexpr int before = 23, after = 24;
    void prepare() {
        std::vector<float> table(bands * (phases + 1) * taps);
        for (int b = 0; b < bands; ++b) {
            const double cutoff = b == 0 ? 1.0 : 0.92 / std::exp2(b / 4.0);
            for (int p = 0; p <= phases; ++p) {
                const double frac = double(p) / phases;
                double sum = 0;
                for (int k = 0; k < taps; ++k) {
                    const double x = k - before - frac;
                    const double z = std::numbers::pi * cutoff * x;
                    const double window = 0.42 + 0.5 * std::cos(std::numbers::pi * x / 24.0)
                                        + 0.08 * std::cos(2 * std::numbers::pi * x / 24.0);
                    const double w = cutoff * (z == 0 ? 1.0 : std::sin(z) / z) * window;
                    table[index(b, p, k)] = float(w);
                    sum += w;
                }
                for (int k = 0; k < taps; ++k) table[index(b,p,k)] /= float(sum);
            }
        }
        table_.swap(table);
    }
    [[nodiscard]] static int bandFor(double rate) noexcept {
        if (!std::isfinite(rate) || std::abs(rate) > 4.0) return -1;
        return std::abs(rate) <= 1.0 ? 0 : std::min(bands-1, int(std::ceil(4 * std::log2(std::abs(rate)))));
    }
    [[nodiscard]] StereoFrame read(const History& history, double age, int band) const noexcept {
        if (table_.empty() || band < 0 || band >= bands || !std::isfinite(age) ||
            age < before || history.valid() <= std::size_t(after) ||
            age >= double(history.valid() - after)) return {};
        const auto i = std::size_t(age);
        const double phase = (age - double(i)) * phases;
        const int p = std::min(phases-1, int(phase));
        const double f = phase - p;
        double l = 0, r = 0;
        for (int k = 0; k < taps; ++k) {
            const double a = table_[index(band,p,k)], b = table_[index(band,p+1,k)];
            const auto v = history.read(i - before + std::size_t(k));
            const double w = a + f * (b-a);
            l += w*v.left; r += w*v.right;
        }
        return {finiteSample(l), finiteSample(r)};
    }
private:
    static constexpr std::size_t index(int b, int p, int k) noexcept {
        return std::size_t((b*(phases+1)+p)*taps+k);
    }
    std::vector<float> table_;
};

// Trigger between process calls. Positive rate traverses source forwards; negative reverses it.
// A grain is admitted only if its entire read window remains legal under ANY freeze sequence.
class GrainBank final {
public:
    static constexpr std::size_t maxGrains = 32;
    void prepare(std::size_t frames) {
        History history; history.prepare(frames);
        SincReader reader; reader.prepare();
        history_ = std::move(history); reader_ = std::move(reader); reset();
    }
    void reset() noexcept { history_.reset(); voices_ = {}; }
    [[nodiscard]] bool trigger(double age, double rate, std::size_t length, double pan = 0.0) noexcept {
        const int band = SincReader::bandFor(rate);
        if (band < 0 || std::abs(rate) < 0.25 || !std::isfinite(age) || !std::isfinite(pan) || length < 3 ||
            length > history_.capacity()) return false;
        const double span = double(length-1);
        // First output follows at most one new captured frame. Subsequent ages change by capture-rate.
        const double lo = age + std::min(0.0, -rate*span);
        const double hi = age + 1 + std::max(0.0, (1-rate)*span);
        if (lo < SincReader::before || history_.valid() <= std::size_t(SincReader::after) ||
            hi >= double(history_.valid() - SincReader::after)) return false;
        for (auto& v : voices_) if (v.length == 0) {
            pan = std::clamp(pan, -1.0, 1.0);
            v = {age, rate, length, 0, band, 1.0-std::max(0.0,pan), 1.0+std::min(0.0,pan)}; return true;
        }
        return false;
    }
    [[nodiscard]] StereoFrame process(StereoFrame input, bool freeze) noexcept {
        history_.capture(input, freeze);
        double l=0, r=0, weight=0;
        for (auto& v : voices_) if (v.length != 0) {
            if (!freeze) v.age += 1;
            const double s = std::sin(std::numbers::pi * double(v.position) / double(v.length-1));
            const double w = (v.position == 0 || v.position == v.length-1) ? 0.0 : s*s;
            const auto frame = reader_.read(history_, v.age, v.band);
            l += w*frame.left*v.leftGain; r += w*frame.right*v.rightGain; weight += w;
            v.age -= v.rate;
            if (++v.position == v.length) v = {};
        }
        const double norm = std::max(1.0, weight);
        return {finiteSample(l/norm), finiteSample(r/norm)};
    }
    [[nodiscard]] std::size_t captured() const noexcept { return history_.valid(); }
    [[nodiscard]] std::size_t active() const noexcept {
        std::size_t n=0; for (const auto& v : voices_) if (v.length) ++n; return n;
    }
private:
    struct Voice { double age{}, rate{}; std::size_t length{}, position{}; int band{}; double leftGain{}, rightGain{}; };
    History history_;
    SincReader reader_;
    std::array<Voice,maxGrains> voices_{};
};
} // namespace cyberator_reference
