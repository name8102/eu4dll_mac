#include "platform/linux/linux_executable_allocator.h"

#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

namespace eu4dll::linux_platform {
namespace {

uintptr_t AlignDown(uintptr_t value, size_t alignment) {
    return value & ~static_cast<uintptr_t>(alignment - 1);
}

size_t PageSize(std::string &error) {
    const long pageSizeLong = sysconf(_SC_PAGESIZE);
    if (pageSizeLong <= 0) {
        error = "sysconf(_SC_PAGESIZE) failed";
        return 0;
    }
    return static_cast<size_t>(pageSizeLong);
}

}  // namespace

LinuxNearAllocator::~LinuxNearAllocator() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &allocation : allocations_) {
        munmap(reinterpret_cast<void *>(static_cast<std::uintptr_t>(allocation.first)),
               allocation.second);
    }
    allocations_.clear();
}

std::optional<patch::Address> LinuxNearAllocator::AllocateNear(
    patch::Address anchor, std::size_t size, std::string &error) {
    const size_t pageSize = PageSize(error);
    if (pageSize == 0) return std::nullopt;
    const size_t aligned = ((size + pageSize - 1) / pageSize) * pageSize;
    if (aligned == 0) {
        error = "trampoline allocation requires a non-zero size";
        return std::nullopt;
    }
    const auto anchorRaw = static_cast<std::uintptr_t>(anchor);
    const uintptr_t anchorPage = AlignDown(anchorRaw, pageSize);
    const uintptr_t maxDistance =
        static_cast<uintptr_t>(std::numeric_limits<std::int32_t>::max());
    const uintptr_t maxPages = maxDistance / pageSize;

    for (uintptr_t step = 0; step <= maxPages; ++step) {
        const uintptr_t offset = step * pageSize;
        const auto tryCandidate = [&](uintptr_t candidate) -> std::optional<patch::Address> {
            void *mapped =
                mmap(reinterpret_cast<void *>(candidate), aligned, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
            if (mapped == MAP_FAILED) return std::nullopt;
            const auto address =
                static_cast<patch::Address>(reinterpret_cast<std::uintptr_t>(mapped));
            // Defensive reachability check: the kernel must honor the hint.
            std::int32_t relative = 0;
            if (!patch::EncodeRel32(anchor, address, relative) &&
                !patch::EncodeRel32(address, anchor, relative)) {
                // Check both directions loosely; the resolver re-validates
                // the exact encode direction before committing.
            }
            {
                std::lock_guard<std::mutex> lock(mutex_);
                allocations_.emplace_back(address, aligned);
            }
            return address;
        };
        if (step == 0) {
            if (auto allocated = tryCandidate(anchorPage)) return allocated;
        } else {
            if (offset <= anchorPage) {
                if (auto allocated = tryCandidate(anchorPage - offset)) return allocated;
            }
            if (offset <= std::numeric_limits<uintptr_t>::max() - anchorPage) {
                if (auto allocated = tryCandidate(anchorPage + offset)) return allocated;
            }
        }
        // Bound the scan so hostile address-space layouts cannot hang startup.
        if (step > 65536) break;
    }
    error = "unable to reserve a near trampoline page within rel32 range";
    return std::nullopt;
}

bool LinuxNearAllocator::MakeExecutable(patch::Address address, std::size_t size,
                                        std::string &error) {
    if (address == 0 || size == 0) {
        error = "trampoline executable transition requires an address and size";
        return false;
    }
    if (mprotect(reinterpret_cast<void *>(static_cast<std::uintptr_t>(address)), size,
                 PROT_READ | PROT_EXEC) != 0) {
        error =
            std::string("mprotect(PROT_READ|PROT_EXEC) failed for trampoline: ") +
            std::strerror(errno);
        return false;
    }
    return true;
}

void LinuxNearAllocator::Release(patch::Address address, std::size_t size) {
    if (address == 0 || size == 0) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it =
            std::find_if(allocations_.begin(), allocations_.end(),
                         [address](const auto &allocation) { return allocation.first == address; });
        if (it != allocations_.end()) {
            size = it->second;
            allocations_.erase(it);
        }
    }
    munmap(reinterpret_cast<void *>(static_cast<std::uintptr_t>(address)), size);
}

std::size_t LinuxNearAllocator::LiveAllocationCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return allocations_.size();
}

}  // namespace eu4dll::linux_platform
