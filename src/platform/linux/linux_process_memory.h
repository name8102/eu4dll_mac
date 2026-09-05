#pragma once

#include "runtime/patch/memory.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace eu4dll::linux_platform {

struct LinuxLoadSegment {
    patch::Address address = 0;
    std::size_t size = 0;
    bool readable = false;
    bool writable = false;
    bool executable = false;
};

// Live ELF process memory for the current Linux host process.
// Discovers the main executable via dl_iterate_phdr() PT_LOAD entries,
// resolves game symbols with dlsym(RTLD_DEFAULT), and performs
// page-aligned mprotect() writes with permission restoration.
class LinuxProcessMemory final : public patch::Memory {
public:
    LinuxProcessMemory();

    bool Read(patch::Address address, std::uint8_t *buffer, std::size_t size,
              std::string &error) const override;
    bool Write(patch::Address address, const std::uint8_t *data, std::size_t size,
               std::string &error) override;
    bool ReadCString(patch::Address address, std::size_t maxSize, std::string &value,
                     std::string &error) const override;
    std::optional<patch::MemoryRegion> MainModule(std::string &error) const override;
    std::vector<patch::MemoryRegion> MainModuleRegions(
        patch::RegionPurpose purpose, std::string &error) const override;
    std::optional<patch::Address> ResolveSymbol(const std::string &symbol,
                                                std::string &error) const override;

    const std::string &ExecutablePath() const { return executablePath_; }
    patch::Address ImageBase() const { return imageBase_; }
    const std::vector<LinuxLoadSegment> &Segments() const { return segments_; }

private:
    bool EnsureDiscovered(std::string &error) const;

    mutable bool discovered_ = false;
    mutable std::string executablePath_;
    mutable patch::Address imageBase_ = 0;
    mutable std::vector<LinuxLoadSegment> segments_;
    mutable std::string discoverError_;
};

}  // namespace eu4dll::linux_platform
