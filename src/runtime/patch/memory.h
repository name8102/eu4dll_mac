#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace eu4dll::patch {

using Address = std::uint64_t;

struct MemoryRegion {
    Address address = 0;
    std::size_t size = 0;
    std::string name;
};

// Truthful outcome of a code write. `bytesWritten == false` is NOT implied by
// overall failure: several platforms copy the payload before restoring page
// protections, so a failed write may still have mutated the target bytes.
// Patch transactions must consult `bytesWritten` (never the boolean alone)
// to decide what needs rollback.
struct WriteResult {
    bool bytesWritten = false;
    bool protectionRestored = true;
    std::string error;

    bool ok() const { return bytesWritten && protectionRestored && error.empty(); }
};

enum class RegionPurpose {
    ExecutableSearch,
    ReadOnlySearch,
    Writable,
};

class Memory {
public:
    virtual ~Memory() = default;

    virtual bool Read(Address address, std::uint8_t *buffer, std::size_t size,
                      std::string &error) const = 0;
    virtual WriteResult Write(Address address, const std::uint8_t *data,
                              std::size_t size) = 0;
    virtual bool ReadCString(Address address, std::size_t maxSize, std::string &value,
                             std::string &error) const = 0;
    virtual std::optional<MemoryRegion> MainModule(std::string &error) const = 0;
    // Multi-region enumeration for ELF images with unmapped gaps. The default
    // implementation preserves the single-region Mach-O behavior so existing
    // adapters keep working until they opt into multiple regions.
    virtual std::vector<MemoryRegion> MainModuleRegions(
        RegionPurpose purpose, std::string &error) const;
    virtual std::optional<Address> ResolveSymbol(const std::string &symbol,
                                                 std::string &error) const = 0;
};

} // namespace eu4dll::patch
