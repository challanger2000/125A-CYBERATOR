#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
namespace cyberator {
// Conservative tail reporting; never truncate actual audio to this estimate.
// A frozen capture is unbounded. For ordinary feedback, the interpolated
// sinc L1 norm is <3, the overlap weights are normalized, and soft(x)<=|x|.
// When this sufficient contraction criterion cannot be established, report
// an unbounded tail rather than pretend a finite universal decay time.
inline double tailSeconds(bool freeze,double feedback)noexcept {
    constexpr double base=30.,maxSincL1=3.,maxHistorySeconds=3.25;
    if(freeze)return std::numeric_limits<double>::infinity();
    const double f=std::isfinite(feedback)?std::clamp(feedback,0.,1.):1.;
    if(f==0)return base;
    const double q=.85*f*maxSincL1;
    if(q>=1)return std::numeric_limits<double>::infinity();
    const double cycles=std::ceil(std::log(1e-6)/std::log(q));
    const double duration=base+maxHistorySeconds*cycles;
    // JUCE's VST3 adapter uses roundToIntAccurate before uint32 conversion.
    if(!std::isfinite(duration)||duration>=double(std::numeric_limits<int>::max())/192000.)return std::numeric_limits<double>::infinity();
    return duration;
}
}
