#include "platform/linux/linux_executable_allocator.h"

#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

namespace eu4dll::linux_platform {
namespace {

uintptr_t AlignDown(uintptr_t value, size_t alignment) {
    return value & ~static_cast<uintptr_t>(alignment - 1);
}

uintptr_t AlignUp(uintptr_t value, size_t alignment) {
    return (value + alignment - 1) & ~static_cast<uintptr_t>(alignment - 1);
}

size_t PageSize(std::string &error) {
    const long pageSizeLong = sysconf(_SC_PAGESIZE);
    if (pageSizeLong <= 0) {
        error = "sysconf(_SC_PAGESIZE) failed";
        return 0;
    }
    return static_cast<size_t>(pageSizeLong);
}

// Reads /proc/self/maps and returns sorted, non-overlapping mapped ranges.
// Used to find truly unmapped gaps within rel32 range instead of blindly
// probing page by page (which stalls or truncates the search on crowded
// address spaces).
std::vector<std::pair<uintptr_t, uintptr_t>> MappedRanges() {
    std::vector<std::pair<uintptr_t, uintptr_t>> ranges;
    std::ifstream maps("/proc/self/maps");
    if (!maps) return ranges;
    std::string line;
    while (std::getline(maps, line)) {
        const auto dash = line.find('-');
        const auto space = line.find(' ', dash == std::string::npos ? 0 : dash);
        if (dash == std::string::npos || space == std::string::npos) continue;
        char *end = nullptr;
        const uintptr_t start = static_cast<uintptr_t>(
            std::strtoull(line.substr(0, dash).c_str(), &end, 16));
        if (end == line.c_str()) continue;
        const uintptr_t finish = static_cast<uintptr_t>(
            std::strtoull(line.substr(dash + 1, space - dash - 1).c_str(), nullptr, 16));
        if (finish <= start) continue;
        ranges.emplace_back(start, finish);
    }
    std::sort(ranges.begin(), ranges.end());
    std::vector<std::pair<uintptr_t, uintptr_t>> merged;
    for (const auto &range : ranges) {
        if (!merged.empty() && range.first <= merged.back().second) {
            merged.back().second = std::max(merged.back().second, range.second);
        } else {
            merged.push_back(range);
        }
    }
    return merged;
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
    constexpr uintptr_t kMaxDistance =
        static_cast<uintptr_t>(std::numeric_limits<std::int32_t>::max());
    const uintptr_t rangeLo =
        anchorPage > kMaxDistance ? anchorPage - kMaxDistance : 0;
    const uintptr_t rangeHi = anchorPage + kMaxDistance >= anchorPage
                                  ? anchorPage + kMaxDistance
                                  : std::numeric_limits<uintptr_t>::max();

    std::optional<patch::Address> placed;
    const auto tryCandidate = [&](uintptr_t candidate) {
        if (placed.has_value()) return;
        void *mapped =
            mmap(reinterpret_cast<void *>(candidate), aligned, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (mapped == MAP_FAILED) return;
        const auto address =
            static_cast<patch::Address>(reinterpret_cast<std::uintptr_t>(mapped));
        // Defensive: the kernel honors MAP_FIXED_NOREPLACE, but verify the
        // exact encode direction the resolver will use before committing.
        std::int32_t relative = 0;
        if (!patch::EncodeRel32(anchor, address, relative)) {
            munmap(mapped, aligned);
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            allocations_.emplace_back(address, aligned);
        }
        placed = address;
    };

    // Fast path: the anchor's own page is occasionally free (e.g. tests).
    tryCandidate(anchorPage);
    if (placed.has_value()) return placed;

    // Full rel32 search: walk unmapped gaps inside [anchor-2GiB,
    // anchor+2GiB], nearest first, probing only the gap edge closest to the
    // anchor. The attempt budget bounds startup latency on hostile layouts.
    constexpr std::size_t kMaxAttempts = 64;
    std::size_t attempts = 0;
    struct Gap {
        uintptr_t edge = 0;  // probe candidate
        uintptr_t distance = 0;
    };
    std::vector<Gap> gaps;
    uintptr_t cursor = rangeLo;
    for (const auto &mapped : MappedRanges()) {
        if (mapped.second <= rangeLo) continue;
        if (mapped.first >= rangeHi) break;
        if (mapped.first > cursor) {
            // Unmapped [cursor, mapped.first): probe the edge nearest anchor.
            const uintptr_t gapStart = AlignUp(cursor, pageSize);
            const uintptr_t gapEnd = mapped.first;
            if (gapEnd > gapStart && gapEnd - gapStart >= aligned) {
                uintptr_t candidate = 0;
                uintptr_t distance = 0;
                if (anchorPage < gapStart) {
                    candidate = gapStart;
                    distance = gapStart - anchorPage;
                } else if (anchorPage >= gapEnd) {
                    candidate = AlignDown(gapEnd - aligned, pageSize);
                    if (candidate < gapStart) continue;
                    distance = anchorPage - candidate;
                } else {
                    continue;  // anchor inside a gap cannot happen; skip.
                }
                gaps.push_back({candidate, distance});
            }
        }
        if (mapped.second > cursor) cursor = mapped.second;
        if (cursor >= rangeHi) break;
    }
    // Trailing gap up to rangeHi.
    if (cursor < rangeHi && rangeHi - cursor >= aligned) {
        const uintptr_t gapStart = AlignUp(cursor, pageSize);
        if (rangeHi - gapStart >= aligned) {
            if (anchorPage < gapStart) {
                gaps.push_back({gapStart, gapStart - anchorPage});
            } else {
                const uintptr_t candidate = AlignDown(rangeHi - aligned, pageSize);
                if (candidate >= gapStart) {
                    gaps.push_back({candidate, anchorPage - candidate});
                }
            }
        }
    }
    std::sort(gaps.begin(), gaps.end(),
              [](const Gap &a, const Gap &b) { return a.distance < b.distance; });
    for (const auto &gap : gaps) {
        if (placed.has_value() || attempts >= kMaxAttempts) break;
        ++attempts;
        tryCandidate(gap.edge);
    }
    if (placed.has_value()) return placed;
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
