#include "dsp/GrainBank.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace cyberator;
void check(bool ok, const char* name) { if (!ok) { std::cerr << name << '\n'; std::exit(1); } }
void near(double a, double b, double tol, const char* name) { check(std::abs(a-b) <= tol, name); }
int main() {
    GrainBank g; g.prepare(4096);
    check(!g.trigger(100,1,65), "empty rejects");
    auto fill = [&] { g.reset(); for (int i=0;i<4096;++i) (void)g.process({float(i),float(-2*i)},false); };
    for (double rate : {1.0,-1.0}) for (int pattern : {0,1,2}) {
        fill();
        check(g.trigger(200,rate,65), "valid grain");
        for (int n=0;n<65;++n) {
            const bool freeze = pattern==1 || (pattern==2 && n%3==0);
            const auto y = g.process({float(4096+n),float(-2*(4096+n))},freeze);
            // Source sample index does not depend on future writes or freeze switches.
            const double w = std::pow(std::sin(std::numbers::pi*n/64),2);
            const double expected = (4095-200+rate*n)*w;
            near(y.left,expected,0.002,"source direction / envelope");
            near(y.right,-2*expected,0.004,"independent right");
        }
        check(g.active()==0,"retirement");
    }
    fill();
    for (std::size_t n=0;n<GrainBank::maxGrains;++n) check(g.trigger(200,1,65),"pool accepts");
    check(!g.trigger(200,1,65),"pool rejects overflow");
    for(int n=0;n<65;++n) {
        const auto y=g.process({},true);
        if(n==32) near(y.left,3927,0.002,"normalized overlap");
    }
    check(!g.trigger(1,1,65) && !g.trigger(4090,-1,65) && !g.trigger(200,5,65) &&
          !g.trigger(200,0,65) && !g.trigger(std::numeric_limits<double>::infinity(),1,65),"invalid range");
    g.reset();
    check(g.active()==0 && !g.trigger(200,1,65),"reset clears");
    History h; h.prepare(4096); SincReader reader; reader.prepare();
    for(double freq : {0.08,0.35}) {
        h.reset();
        for(int i=0;i<4096;++i) h.capture({float(std::sin(2*std::numbers::pi*freq*i)),0});
        double energy=0, error=0;
        for(int i=0;i<512;++i) {
            const double age=100.37+2*i;
            const double target=std::sin(2*std::numbers::pi*freq*(4095-age));
            const double y=reader.read(h,age,SincReader::bandFor(2)).left;
            energy+=y*y; error+=(y-target)*(y-target);
        }
        const double rms=std::sqrt(energy/512), rmse=std::sqrt(error/512);
        std::cout << "frequency=" << freq << " rms=" << rms << " error=" << rmse << '\n';
        if(freq<0.1) check(rmse<0.005,"passband accuracy"); else check(rms<0.005,"downsampling rejection");
    }
    h.reset();
    for(int i=0;i<4096;++i) h.capture({std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()});
    const auto extreme=reader.read(h,100.37,0);
    check(std::isfinite(extreme.left) && std::isfinite(extreme.right),"finite extreme");
    std::cout << "Grain contract PASS\n";
}
