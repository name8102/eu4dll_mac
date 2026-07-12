#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace eu4dll::patch {

using Address = std::uint64_t;

struct MemoryRegion {
    Address address = 0;
    std::size_t size = 0;
    std::string name;
};

class Memory {
public:
    virtual ~Memory() = default;

    virtual bool Read(Address address, std::uint8_t *buffer, std::size_t size,
                      std::string &error) const = 0;
    virtual bool Write(Address address, const std::uint8_t *data, std::size_t size,
                       std::string &error) = 0;
    virtual bool ReadCString(Address address, std::size_t maxSize, std::string &value,
                             std::string &error) const = 0;
    virtual std::optional<MemoryRegion> MainModule(std::string &error) const = 0;
    virtual std::optional<Address> ResolveSymbol(const std::string &symbol,
                                                 std::string &error) const = 0;
};

} // namespace eu4dll::patch
