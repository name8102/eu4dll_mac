#pragma once

#include "memory.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace eu4dll::patch {

class ByteBufferMemory final : public Memory {
public:
    explicit ByteBufferMemory(std::vector<std::uint8_t> bytes, Address baseAddress = 0x1000,
                              std::string regionName = "byte-buffer");

    bool Read(Address address, std::uint8_t *buffer, std::size_t size,
              std::string &error) const override;
    bool Write(Address address, const std::uint8_t *data, std::size_t size,
               std::string &error) override;
    bool ReadCString(Address address, std::size_t maxSize, std::string &value,
                     std::string &error) const override;
    std::optional<MemoryRegion> MainModule(std::string &error) const override;
    std::optional<Address> ResolveSymbol(const std::string &symbol,
                                         std::string &error) const override;

    void DefineSymbol(std::string symbol, Address address);
    const std::vector<std::uint8_t> &Bytes() const { return bytes_; }
    Address BaseAddress() const { return baseAddress_; }

private:
    std::optional<std::size_t> OffsetOf(Address address, std::size_t size,
                                        std::string &error) const;

    std::vector<std::uint8_t> bytes_;
    Address baseAddress_;
    std::string regionName_;
    std::unordered_map<std::string, Address> symbols_;
};

} // namespace eu4dll::patch
