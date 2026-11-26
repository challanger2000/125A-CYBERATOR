#pragma once
#include "GrainBank.hpp"
#include <complex>
#include <cstdint>

namespace cyberator {
struct CyberParams { double formant{}, android{}, spectral{}; };
// Stereo STFT prototype: 4x overlap, sqrt-Hann analysis/synthesis, fixed N-sample latency.
// Same spectral gain and phase rotation on both channels preserve per-bin stereo phase.
class Cybernetic final {
    using C=std::complex<double>;
public:
    void prepare(double sr) {
        if(!std::isfinite(sr)||sr<8000||sr>192000) throw std::invalid_argument("sample rate");
        std::size_t n=256; while(double(n)*std::sqrt(2.0)<sr*.04266666666666667) n*=2;
        Cybernetic fresh; fresh.n_=n; fresh.hop_=n/4;
        fresh.radius_=std::max(std::size_t(1),std::size_t(std::round(350*double(n)/sr)));
        fresh.window_.resize(n); fresh.twiddle_.resize(n/2); fresh.rev_.resize(n);
        fresh.in_.resize(n); fresh.out_.resize(n);
        for(auto& a:fresh.fft_) a.resize(n);
        for(auto& a:fresh.memory_) a.resize(n/2+1);
        fresh.env_.resize(n/2+1);fresh.prefix_.resize(n/2+2);
        for(std::size_t i=0;i<n;++i){
            fresh.window_[i]=std::sqrt(.5-.5*std::cos(2*std::numbers::pi*double(i)/double(n)));
            std::size_t b=0,x=i;for(std::size_t bit=n;bit>1;bit/=2){b=2*b+(x&1u);x>>=1;}
            fresh.rev_[i]=b;
            if(i<n/2)fresh.twiddle_[i]=std::polar(1.0,-2*std::numbers::pi*double(i)/double(n));
        }
        *this=std::move(fresh);
    }
    void reset() noexcept {
        std::fill(in_.begin(),in_.end(),StereoFrame{});std::fill(out_.begin(),out_.end(),StereoFrame{});
        for(auto& m:memory_) std::fill(m.begin(),m.end(),C{});
        pos_=count_=frame_=0;
    }
    [[nodiscard]] std::size_t latency() const noexcept {return n_;}
    [[nodiscard]] StereoFrame process(StereoFrame in,const CyberParams& p) noexcept {
        if(!n_)return {};
        const auto y=out_[pos_];out_[pos_]={};
        in_[pos_]={finiteSample(in.left),finiteSample(in.right)};
        if(++pos_==n_)pos_=0;
        if(++count_==hop_){count_=0;transform(p);frame_=(frame_+1)%4;}
        return y;
    }
private:
    static double bounded(double x,double lo,double hi) noexcept {return std::isfinite(x)?std::clamp(x,lo,hi):0;}
    void fft(std::vector<C>& a,bool inverse) noexcept {
        for(std::size_t i=0;i<n_;++i)if(i<rev_[i])std::swap(a[i],a[rev_[i]]);
        for(std::size_t len=2;len<=n_;len*=2){
            const auto step=n_/len;
            for(std::size_t i=0;i<n_;i+=len)for(std::size_t j=0;j<len/2;++j){
                const C t=twiddle_[j*step],u=a[i+j],v=a[i+j+len/2]*(inverse?std::conj(t):t);
                a[i+j]=u+v;a[i+j+len/2]=u-v;
            }
        }
        if(inverse)for(auto& x:a)x/=double(n_);
    }
    void transform(const CyberParams& p) noexcept {
        const auto half=n_/2;
        for(std::size_t i=0;i<n_;++i){const auto x=in_[(pos_+i)%n_];fft_[0][i]=double(x.left)*window_[i];fft_[1][i]=double(x.right)*window_[i];}
        for(auto& a:fft_)fft(a,false);
        const double f=bounded(p.formant,-12,12),a=bounded(p.android,0,1),s=bounded(p.spectral,0,1);
        // Smooth log magnitude across approximately 700 Hz. This is envelope warping,
        // not pitch shifting, and its vocal accuracy must be evaluated separately.
        if(f!=0){
            prefix_[0]=0;
            for(std::size_t k=0;k<=half;++k)prefix_[k+1]=prefix_[k]+std::log(std::max(1e-12,std::hypot(std::abs(fft_[0][k]),std::abs(fft_[1][k]))));
            for(std::size_t k=0;k<=half;++k){const auto lo=k>radius_?k-radius_:0,hi=std::min(half,k+radius_);env_[k]=(prefix_[hi+1]-prefix_[lo])/double(hi-lo+1);}
        }
        const double ratio=std::exp2(f/12),decay=.97*s;
        for(std::size_t k=0;k<=half;++k){
            double gain=1;
            if(f!=0){const double source=std::min(double(half),double(k)/ratio);const auto i=std::size_t(source);const double target=env_[i]+(source-double(i))*(env_[std::min(i+1,half)]-env_[i]);gain=std::exp(std::clamp(target-env_[k],-std::log(4.0),std::log(4.0)));}
            C rotation{1,0};
            if(a>0&&k>0&&k<half){
                const auto ref=std::abs(fft_[0][k])>=std::abs(fft_[1][k])?fft_[0][k]:fft_[1][k];
                if(std::abs(ref)>1e-12){const double target=std::numbers::pi*.5*double((frame_*k)%4);rotation=std::polar(1.0,a*std::remainder(target-std::arg(ref),2*std::numbers::pi));}
            }
            const C step=std::polar(1.0,std::numbers::pi*.5*double(k%4));
            for(std::size_t c=0;c<2;++c){
                auto& bin=fft_[c][k];auto& m=memory_[c][k];
                m=(1-decay)*bin+decay*m*step;
                bin=((1-s)*bin+s*m)*gain*rotation;
                if(k==0||k==half)bin={bin.real(),0};else fft_[c][n_-k]=std::conj(bin);
            }
        }
        for(auto& v:fft_)fft(v,true);
        for(std::size_t i=0;i<n_;++i){auto& x=out_[(pos_+i)%n_];const auto w=.5*window_[i];x.left=finiteSample(double(x.left)+w*fft_[0][i].real());x.right=finiteSample(double(x.right)+w*fft_[1][i].real());}
    }
    std::size_t n_{},hop_{},radius_{},pos_{},count_{},frame_{};
    std::vector<double> window_,env_,prefix_;std::vector<C> twiddle_;std::vector<std::size_t> rev_;
    std::vector<StereoFrame> in_,out_;std::array<std::vector<C>,2> fft_,memory_;
};
} // namespace cyberator
