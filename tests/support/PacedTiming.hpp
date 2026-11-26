#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <thread>
#include <vector>
namespace cyberator::test {
struct TimingSample {double processUs{},startLateUs{},finishLateUs{};};
struct TimingSummary {double p50{},p95{},p99{},maximum{},maxStartLate{},maxFinishLate{};std::size_t durationMisses{},completionMisses{};};
inline TimingSummary summarize(const std::vector<TimingSample>& rows,double budgetUs){
    if(rows.empty()||!std::isfinite(budgetUs)||budgetUs<=0)throw std::invalid_argument("timing data");
    TimingSummary s;std::vector<double> times;times.reserve(rows.size());
    for(const auto& r:rows){
        if(!std::isfinite(r.processUs)||!std::isfinite(r.startLateUs)||!std::isfinite(r.finishLateUs)||r.processUs<0)throw std::invalid_argument("nonfinite timing");
        times.push_back(r.processUs);s.maxStartLate=std::max(s.maxStartLate,r.startLateUs);s.maxFinishLate=std::max(s.maxFinishLate,r.finishLateUs);
        if(r.processUs>budgetUs)++s.durationMisses;
        if(r.finishLateUs>0)++s.completionMisses;
    }
    std::sort(times.begin(),times.end());
    auto q=[&](double p){return times[std::size_t(std::ceil(p*double(times.size())))-1];};
    s.p50=q(.5);s.p95=q(.95);s.p99=q(.99);s.maximum=times.back();return s;
}
// Absolute periodic release times. No deadline is silently shifted or dropped.
// Timing includes all actual process calls in a group, but not host buffer setup.
template<class Setup,class Process,class Observe>
std::vector<TimingSample> pace(double sampleRate,int frames,std::size_t groups,Setup setup,Process process,Observe observe,
    std::chrono::steady_clock::time_point release={}){
    using Clock=std::chrono::steady_clock;using Us=std::chrono::duration<double,std::micro>;
    if(!std::isfinite(sampleRate)||sampleRate<=0||frames<=0||groups==0)throw std::invalid_argument("pace configuration");
    const auto period=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(double(frames)/sampleRate));
    if(period<=Clock::duration::zero())throw std::invalid_argument("zero clock period");
    std::vector<TimingSample> result(groups);
    if(release==Clock::time_point{})release=Clock::now()+std::chrono::milliseconds(10);
    for(std::size_t i=0;i<groups;++i){
        setup(i);
        const auto early=release-std::chrono::microseconds(500);
        if(Clock::now()<early)std::this_thread::sleep_until(early);
        while(Clock::now()<release){} // bounded by this period's release time, diagnostic host only
        const auto begin=Clock::now();process(i);const auto end=Clock::now();
        result[i]={Us(end-begin).count(),Us(begin-release).count(),Us(end-(release+period)).count()};
        observe(i);release+=period;
    }
    return result;
}
} // namespace cyberator::test
