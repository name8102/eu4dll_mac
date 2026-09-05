#pragma once

#include "runtime/patch/branch_resolver.h"

#include <cstddef>
#include <mutex>
#include <utility>
#include <vector>

namespace eu4dll::linux_platform {

// Linux near executable allocator for x86-64 rel32 trampolines.
// Reserves pages within signed rel32 range of a patch site with mmap()
// MAP_FIXED_NOREPLACE, writes the absolute indirect stub via the shared
// resolver, then transitions the page to RX. Trampoline lifetime is explicit:
// failed staging releases the page, successful installs keep it mapped for
// the hook lifetime.
class LinuxNearAllocator final : public patch::ExecutableCodeAllocator {
public:
    LinuxNearAllocator() = default;
    ~LinuxNearAllocator() override;

    LinuxNearAllocator(const LinuxNearAllocator &) = delete;
    LinuxNearAllocator &operator=(const LinuxNearAllocator &) = delete;

    std::optional<patch::Address> AllocateNear(
        patch::Address anchor, std::size_t size, std::string &error) override;
    bool MakeExecutable(patch::Address address, std::size_t size,
                        std::string &error) override;
    void Release(patch::Address address, std::size_t size) override;

    std::size_t LiveAllocationCount() const;

private:
    mutable std::mutex mutex_;
    std::vector<std::pair<patch::Address, std::size_t>> allocations_;
};

}  // namespace eu4dll::linux_platform
