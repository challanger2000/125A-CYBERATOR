#pragma once
#include "Granular.hpp"
#include "Cybernetic.hpp"
#include "Harmony.hpp"
#include "Ghost.hpp"
#include "Space.hpp"
namespace cyberator {
struct EngineParams {
    GranularParams grain{}; CyberParams cyber{};
    double granular{}, harmony{}, ghost{}, swell{}, delay{}, space{}, mix{1}, outputDb{};
};
// Prototype graph. Granular and spectral branches share only the source, not state.
// Deliberate delay/harmony offsets are creative effect times, not host latency.
class Engine final {
public:
    void prepare(double sr){
        Engine fresh;fresh.granular_.prepare(sr);fresh.cyber_.prepare(sr);fresh.harmony_.prepare(sr);
        fresh.ghost_.prepare(sr);fresh.space_.prepare(sr);
        fresh.spectralDelay_=fresh.cyber_.latency();fresh.latency_=fresh.spectralDelay_+fresh.ghost_.latency();
        fresh.dry_.prepare(fresh.latency_+1);fresh.grainDelay_.prepare(fresh.spectralDelay_+1);
        *this=std::move(fresh);
    }
    void reset() noexcept {
        dry_.reset();grainDelay_.reset();granular_.reset();cyber_.reset();harmony_.reset();ghost_.reset();space_.reset();
    }
    [[nodiscard]] std::size_t latency()const noexcept{return latency_;}
    [[nodiscard]] StereoFrame process(StereoFrame in,const EngineParams& p) noexcept {
        if(!latency_)return {};
        dry_.capture(in);const auto dry=dry_.read(latency_),base=dry_.read(spectralDelay_);
        const auto cg=cyber_.process(in,p.cyber);
        const double g=unit(p.granular);auto gp=p.grain;if(g==0)gp.density=0;
        grainDelay_.capture(granular_.process(in,gp));const auto grain=grainDelay_.read(spectralDelay_);
        const bool cyberActive=nonzero(p.cyber.formant)||unit(p.cyber.android)>0||unit(p.cyber.spectral)>0;
        const auto main=blend(cyberActive?cg:base,grain,g);
        const auto harmony=harmony_.process(main,p.harmony);
        const auto voices=add(main,harmony);
        const auto gh=ghost_.process(voices,p.ghost,p.swell);
        const auto wet=add(add(gh.dry,gh.echo),gh.swell);
        const auto sp=space_.process(wet,p.delay,p.space);
        const double m=unit(p.mix);
        if(m==0)return dry; // Exact latency-aligned bypass, even with active engines.
        const double db=std::isfinite(p.outputDb)?std::clamp(p.outputDb,-24.,12.):0.;
        const auto effected=scale(add(add(wet,sp.delay),sp.reverb),std::exp2(db/6.020599913279624));
        return blend(dry,effected,m);
    }
private:
    static bool nonzero(double x)noexcept{return std::isfinite(x)&&x!=0;}
    static double unit(double x)noexcept{return std::isfinite(x)?std::clamp(x,0.,1.):0.;}
    static StereoFrame add(StereoFrame a,StereoFrame b)noexcept{return {finiteSample(double(a.left)+b.left),finiteSample(double(a.right)+b.right)};}
    static StereoFrame scale(StereoFrame a,double g)noexcept{return {finiteSample(a.left*g),finiteSample(a.right*g)};}
    static StereoFrame blend(StereoFrame a,StereoFrame b,double m)noexcept{
        if(m==0)return a;
        if(m==1)return b;
        return {finiteSample((1-m)*a.left+m*b.left),finiteSample((1-m)*a.right+m*b.right)};
    }
    History dry_,grainDelay_;Granular granular_;Cybernetic cyber_;Harmony harmony_;Ghost ghost_;Space space_;
    std::size_t spectralDelay_{},latency_{};
};
} // namespace cyberator
