#pragma once
#include <array>
#include <cstddef>
namespace cyberator {
enum class Mode : unsigned { dream, cyber, ghost, android, pulse, shimmer, shatter, drift, abyss, phantom };
struct ModeShape { double size, density, android, spectral, harmony, ghost, swell, delay, space, motionHz; };
// Provisional creative voicings, not final curated presets. Multipliers preserve zero.
inline constexpr std::array<ModeShape,10> modeShapes{{
    {1,1,1,1,1,1,1,1,1,.20},
    {.7,1.3,1.4,.8,1.1,.7,.6,1.1,.8,.45},
    {1.4,.7,.65,1.2,.8,1.5,1.6,.8,1.3,.13},
    {.9,1.1,1.7,.6,.8,.6,.5,.8,.6,.30},
    {.5,1.7,1.1,.6,.6,.7,.5,1.5,.6,2},
    {1.2,1.2,.6,1.4,1.5,.7,1,1.1,1.5,.25},
    {.35,1.8,1.4,1.3,1.1,1.3,.6,1.2,.7,3},
    {1.8,.65,.65,1.3,1.1,.8,1.2,.9,1.4,.08},
    {1.5,.8,1.2,1.5,.8,1.4,1.5,.6,1.6,.06},
    {1.1,1.2,.9,1.1,1.3,1.6,1.4,1.3,1.1,.17}
}};
inline constexpr const ModeShape& shape(Mode mode) noexcept {
    const auto i=static_cast<std::size_t>(mode);
    return modeShapes[i<modeShapes.size()?i:0];
}
} // namespace cyberator
