#include "byte_buffer_memory.h"

#include <cstring>
#include <limits>
#include <sstream>

namespace eu4dll::patch {

ByteBufferMemory::ByteBufferMemory(std::vector<std::uint8_t> bytes, Address baseAddress,
                                   std::string regionName)
    : bytes_(std::move(bytes)), baseAddress_(baseAddress), regionName_(std::move(regionName)) {}

std::optional<std::size_t> ByteBufferMemory::OffsetOf(Address address, std::size_t size,
                                                       std::string &error) const {
    if (address < baseAddress_) {
        error = "address is before the byte-buffer base";
        return std::nullopt;
    }
    const Address offset = address - baseAddress_;
    if (offset > bytes_.size() || size > bytes_.size() - static_cast<std::size_t>(offset)) {
        std::ostringstream stream;
        stream << "range at 0x" << std::hex << address << std::dec
               << " with size " << size << " is outside the byte buffer";
        error = stream.str();
        return std::nullopt;
    }
    return static_cast<std::size_t>(offset);
}

bool ByteBufferMemory::Read(Address address, std::uint8_t *buffer, std::size_t size,
                            std::string &error) const {
    if (buffer == nullptr || size == 0) {
        error = "read requires a non-null buffer and non-zero size";
        return false;
    }
    const auto offset = OffsetOf(address, size, error);
    if (!offset) {
        return false;
    }
    std::memcpy(buffer, bytes_.data() + *offset, size);
    return true;
}

bool ByteBufferMemory::Write(Address address, const std::uint8_t *data, std::size_t size,
                             std::string &error) {
    if (data == nullptr || size == 0) {
        error = "write requires non-null data and non-zero size";
        return false;
    }
    const auto offset = OffsetOf(address, size, error);
    if (!offset) {
        return false;
    }
    std::memcpy(bytes_.data() + *offset, data, size);
    return true;
}

bool ByteBufferMemory::ReadCString(Address address, std::size_t maxSize, std::string &value,
                                   std::string &error) const {
    value.clear();
    if (maxSize == 0) {
        error = "string read requires a non-zero maximum size";
        return false;
    }
    for (std::size_t index = 0; index < maxSize; ++index) {
        std::uint8_t byte = 0;
        if (!Read(address + index, &byte, 1, error)) {
            return false;
        }
        if (byte == 0) {
            return true;
        }
        value.push_back(static_cast<char>(byte));
    }
    error = "string is not null-terminated within the requested size";
    return false;
}

std::optional<MemoryRegion> ByteBufferMemory::MainModule(std::string &error) const {
    if (bytes_.empty()) {
        error = "byte buffer is empty";
        return std::nullopt;
    }
    return MemoryRegion{baseAddress_, bytes_.size(), regionName_};
}

std::vector<MemoryRegion> ByteBufferMemory::MainModuleRegions(
    RegionPurpose purpose, std::string &error) const {
    if (purpose != RegionPurpose::ExecutableSearch) {
        error.clear();
        return {};
    }
    auto region = MainModule(error);
    if (!region) return {};
    return {*region};
}

std::optional<Address> ByteBufferMemory::ResolveSymbol(const std::string &symbol,
                                                       std::string &error) const {
    const auto found = symbols_.find(symbol);
    if (found == symbols_.end()) {
        error = "symbol not defined in byte buffer: " + symbol;
        return std::nullopt;
    }
    return found->second;
}

void ByteBufferMemory::DefineSymbol(std::string symbol, Address address) {
    symbols_[std::move(symbol)] = address;
}

} // namespace eu4dll::patch
