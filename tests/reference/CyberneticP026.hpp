#pragma once
#include "dsp/Cybernetic.hpp"
#include <complex>
#include <cstdint>

namespace cyberator {
// Stereo STFT: 4x overlap, sqrt-Hann analysis/synthesis, fixed N+hop latency.
// Each job runs in bounded microbatches over the next hop; no frame-sized callback burst.
// Same spectral gain and phase rotation on both channels preserve per-bin stereo phase.
class CyberneticP026 final {
    enum class Stage { idle, analysis, forward, unpack, envelope, bins, reorder, inverse, synthesis };
    using C=std::complex<double>;
public:
    void prepare(double sr, bool shortWindow=false) {
        if(!std::isfinite(sr)||sr<8000||sr>192000) throw std::invalid_argument("sample rate");
        std::size_t n=shortWindow?128:256; while(double(n)*std::sqrt(2.0)<sr*(shortWindow?.01066666666666667:.04266666666666667)) n*=2;
        CyberneticP026 fresh; fresh.n_=n; fresh.hop_=n/4;fresh.ring_=n+fresh.hop_;
        std::size_t levels=0;for(auto m=n;m>1;m/=2)++levels;
        const auto work=4*n+n*levels+15*(n/2+1);
        fresh.quota_=(work+fresh.hop_/8-1)/(fresh.hop_/8)+16;
        fresh.radius_=std::max(std::size_t(1),std::size_t(std::round(350*double(n)/sr)));
        fresh.window_.resize(n); fresh.twiddle_.resize(n/2); fresh.rev_.resize(n);
        fresh.in_.resize(fresh.ring_); fresh.out_.resize(fresh.ring_);
        fresh.packed_.resize(n);
        for(auto& a:fresh.fft_) a.resize(n/2+1);
        for(auto& a:fresh.memory_) a.resize(n/2+1);
        fresh.env_.resize(n/2+1);fresh.prefix_.resize(n/2+2);
        for(std::size_t i=0;i<n;++i){
            fresh.window_[i]=std::sqrt(.5-.5*std::cos(2*std::numbers::pi*double(i)/double(n)));
            std::size_t b=0,x=i;for(std::size_t bit=n;bit>1;bit/=2){b=2*b+(x&1u);x>>=1;}
            fresh.rev_[i]=b;
            if(i<n/2)fresh.twiddle_[i]=std::polar(1.0,-2*std::numbers::pi*double(i)/double(n));
        }
        for(std::size_t i=0;i<4;++i)fresh.phaseStep_[i]=std::polar(1.0,std::numbers::pi*.5*double(i));
        *this=std::move(fresh);
    }
    void reset() noexcept {
        std::fill(in_.begin(),in_.end(),StereoFrame{});std::fill(out_.begin(),out_.end(),StereoFrame{});
        for(auto& m:memory_) std::fill(m.begin(),m.end(),C{});
        pos_=count_=frame_=late_=0;stage_=Stage::idle;index_=0;
    }
    [[nodiscard]] std::size_t latency() const noexcept {return ring_;}
    [[nodiscard]] std::size_t extraLatency()const noexcept{return hop_;}
    [[nodiscard]] std::size_t missedFrames()const noexcept{return late_;}
    [[nodiscard]] StereoFrame process(StereoFrame in,const CyberParams& p) noexcept {
        if(!n_)return {};
        const auto y=out_[pos_];out_[pos_]={};
        in_[pos_]={finiteSample(in.left),finiteSample(in.right)};
        if(++pos_==ring_)pos_=0;
        if((count_+1)%8==0)work();
        if(++count_==hop_){count_=0;if(stage_!=Stage::idle)++late_;begin(p);frame_=(frame_+1)%4;}
        return y;
    }
private:
    static double bounded(double x,double lo,double hi) noexcept {return std::isfinite(x)?std::clamp(x,lo,hi):0;}
    [[nodiscard]] std::size_t wrap(std::size_t x)const noexcept{return x>=ring_?x-ring_:x;}
    void begin(const CyberParams& p)noexcept{
        start_=wrap(pos_+hop_);frameJob_=frame_;f_=bounded(p.formant,-12,12);a_=bounded(p.android,0,1);s_=bounded(p.spectral,0,1);
        ratio_=std::exp2(f_/12);decay_=.97*s_;prefix_[0]=0;
        zeroL_=zeroR_=same_=anti_=outZeroL_=outZeroR_=outSame_=outAnti_=true;
        stage_=Stage::analysis;index_=0;len_=2;
    }
    void work()noexcept{
        std::size_t budget=quota_;const auto half=n_/2;
        auto take=[&](std::size_t total,std::size_t weight){const auto count=std::min(total-index_,std::max(std::size_t(1),budget/weight));budget=budget>count*weight?budget-count*weight:0;return index_+count;};
        while(budget&&stage_!=Stage::idle){
            switch(stage_){
            case Stage::analysis:{
                const auto end=take(n_,1);
                for(;index_<end;++index_){const auto x=in_[wrap(start_+index_)];zeroL_&=x.left==0;zeroR_&=x.right==0;same_&=x.left==x.right;anti_&=x.left==-x.right;packed_[rev_[index_]]={double(x.left)*window_[index_],double(x.right)*window_[index_]};}
                if(index_==n_){stage_=Stage::forward;index_=0;len_=2;}break;
            }
            case Stage::forward:case Stage::inverse:{
                const bool inverse=stage_==Stage::inverse;
                const auto end=take(half,1),h=len_/2,step=n_/len_;
                for(;index_<end;++index_){const auto j=index_&(h-1),i=2*(index_-j);const C t=twiddle_[j*step],u=packed_[i+j],v=packed_[i+j+h]*(inverse?std::conj(t):t);packed_[i+j]=u+v;packed_[i+j+h]=u-v;}
                if(index_==half){index_=0;len_*=2;if(len_>n_)stage_=inverse?Stage::synthesis:Stage::unpack;}
                break;
            }
            case Stage::unpack:{
                const auto end=take(half+1,3);
                for(;index_<end;++index_){const auto k=index_;const auto z=packed_[k],mirror=std::conj(packed_[(n_-k)%n_]);const auto sum=(z+mirror)*.5,diff=(z-mirror)*.5;
                    fft_[0][k]=zeroL_?C{}:sum;fft_[1][k]=zeroR_?C{}:same_?fft_[0][k]:anti_?-fft_[0][k]:C{diff.imag(),-diff.real()};
                    if(f_!=0)prefix_[k+1]=prefix_[k]+(.5*std::log(std::max(1e-24,std::norm(fft_[0][k])+std::norm(fft_[1][k]))));}
                if(index_==half+1){index_=0;stage_=f_!=0?Stage::envelope:Stage::bins;}break;
            }
            case Stage::envelope:{
                const auto end=take(half+1,1);
                for(;index_<end;++index_){const auto k=index_,lo=k>radius_?k-radius_:0,hi=std::min(half,k+radius_);env_[k]=(prefix_[hi+1]-prefix_[lo])/double(hi-lo+1);}
                if(index_==half+1){index_=0;stage_=Stage::bins;}break;
            }
            case Stage::bins:{
                const auto end=take(half+1,11);for(;index_<end;++index_)bin(index_,half);
                if(index_==half+1){index_=0;stage_=Stage::reorder;}break;
            }
            case Stage::reorder:{
                const auto end=take(n_,1);for(;index_<end;++index_)if(index_<rev_[index_])std::swap(packed_[index_],packed_[rev_[index_]]);
                if(index_==n_){index_=0;len_=2;stage_=Stage::inverse;}break;
            }
            case Stage::synthesis:{
                const auto end=take(n_,2);
                for(;index_<end;++index_){auto& x=out_[wrap(start_+index_)];const auto w=.5*window_[index_];const auto z=packed_[index_]/double(n_);const double l=outZeroL_?0:z.real();const double r=outZeroR_?0:outSame_?l:outAnti_?-l:z.imag();x.left=finiteSample(double(x.left)+w*l);x.right=finiteSample(double(x.right)+w*r);}
                if(index_==n_){index_=0;stage_=Stage::idle;}break;
            }
            case Stage::idle:break;
            }
        }
    }
    void bin(std::size_t k,std::size_t half)noexcept{
        double gain=1;
        if(f_!=0){const double source=std::min(double(half),double(k)/ratio_);const auto i=std::size_t(source);const double target=env_[i]+(source-double(i))*(env_[std::min(i+1,half)]-env_[i]);gain=std::exp(std::clamp(target-env_[k],-std::log(4.0),std::log(4.0)));}
        C rotation{1,0};
        if(a_>0&&k>0&&k<half){
            const double leftPower=std::norm(fft_[0][k]),rightPower=std::norm(fft_[1][k]);
            const auto ref=leftPower>=rightPower*(1-1e-9)?fft_[0][k]:fft_[1][k];
            if(std::norm(ref)>1e-24){const double target=std::numbers::pi*.5*double((frameJob_*k)%4);double delta=std::remainder(target-std::arg(ref),2*std::numbers::pi);
                if(std::abs(std::abs(delta)-std::numbers::pi)<1e-9)delta=std::numbers::pi;
                rotation=std::polar(1.0,a_*delta);}
        }
        const C step=phaseStep_[k%4];
        for(std::size_t c=0;c<2;++c){auto& value=fft_[c][k];auto& m=memory_[c][k];m=(1-decay_)*value+decay_*m*step;value=((1-s_)*value+s_*m)*gain*rotation;if(k==0||k==half)value={value.real(),0};}
        const auto l=fft_[0][k],r=fft_[1][k];outZeroL_&=l==C{};outZeroR_&=r==C{};outSame_&=l==r;outAnti_&=l==-r;
        packed_[k]=l+C{-r.imag(),r.real()};if(k>0&&k<half)packed_[n_-k]=std::conj(l)+C{r.imag(),r.real()};
    }
    std::size_t n_{},hop_{},ring_{},quota_{},radius_{},pos_{},count_{},frame_{},start_{},frameJob_{},index_{},len_{},late_{};
    double f_{},a_{},s_{},ratio_{},decay_{};Stage stage_{Stage::idle};
    bool zeroL_{},zeroR_{},same_{},anti_{},outZeroL_{},outZeroR_{},outSame_{},outAnti_{};
    std::array<C,4> phaseStep_{};
    std::vector<double> window_,env_,prefix_;std::vector<C> twiddle_,packed_;std::vector<std::size_t> rev_;
    std::vector<StereoFrame> in_,out_;std::array<std::vector<C>,2> fft_,memory_;
};
} // namespace cyberator
