#include "dsp/History.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <deque>

using cyberator::History;
using cyberator::StereoFrame;
void require(bool condition) {
    if (!condition) { std::cerr << "History contract failed\n"; std::exit(1); }
}
void equal(StereoFrame actual, StereoFrame expected) {
    require(actual.left == expected.left && actual.right == expected.right);
}
int main() {
    History h;
    h.capture({1, 2});
    equal(h.read(0), {});
    for (std::size_t n : {1u, 2u, 3u, 17u, 128u}) {
        h.prepare(n);
        std::deque<StereoFrame> oracle;
        for (int i = 1; i <= 1000; ++i) {
            StereoFrame in{float(i), float(-i * 2)};
            const bool freeze = i % 11 < 4;
            h.capture(in, freeze);
            if (!freeze) {
                oracle.push_front(in);
                if (oracle.size() > n) oracle.pop_back();
            }
            require(h.valid() == oracle.size());
            for (std::size_t age = 0; age <= n; ++age)
                equal(h.read(age), age < oracle.size() ? oracle[age] : StereoFrame{});
            equal(h.read(std::numeric_limits<std::size_t>::max()), {});
        }
        h.reset();
        require(h.valid() == 0 && h.capacity() == n);
        equal(h.read(0), {});
        h.capture({7, -9});
        equal(h.read(0), {7, -9});
        equal(h.read(1), {});
    }
    h.capture({std::numeric_limits<float>::quiet_NaN(), 3});
    equal(h.read(0), {0, 3});
    h.capture({4, std::numeric_limits<float>::infinity()});
    equal(h.read(0), {4, 0});
    try { h.prepare(0); require(false); } catch (const std::invalid_argument&) {}
    equal(h.read(0), {4, 0});
    std::cout << "History contract PASS\n";
}
