#pragma once

#include "runtime/patch/memory.h"

namespace eu4dll::platform::macos {

class MachProcessMemory final : public patch::Memory {
public:
    bool Read(patch::Address address, std::uint8_t *buffer, std::size_t size,
              std::string &error) const override;
    bool Write(patch::Address address, const std::uint8_t *data, std::size_t size,
               std::string &error) override;
    bool ReadCString(patch::Address address, std::size_t maxSize, std::string &value,
                     std::string &error) const override;
    std::optional<patch::MemoryRegion> MainModule(std::string &error) const override;
    std::optional<patch::Address> ResolveSymbol(const std::string &symbol,
                                                std::string &error) const override;
};

} // namespace eu4dll::platform::macos
