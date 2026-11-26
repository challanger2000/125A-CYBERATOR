#pragma once
#include <cstddef>
namespace cyberator::test {
struct AllocationCounts {std::size_t allocations{}, frees{};};
// Only the calling thread is observed. No instrumentation in the product module.
class AllocationProbe final {
public:
    AllocationProbe() noexcept;
    ~AllocationProbe() noexcept;
    AllocationProbe(const AllocationProbe&)=delete;
    AllocationProbe& operator=(const AllocationProbe&)=delete;
    [[nodiscard]] AllocationCounts finish() noexcept;
private:
    bool active_{true};
};
}
