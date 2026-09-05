#pragma once

#include "runtime/patch/memory.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace eu4dll::platform::macos {

struct MachOSegment {
    std::string name;
    std::uint64_t vmAddress = 0;
    std::uint64_t vmSize = 0;
    std::uint64_t fileOffset = 0;
    std::uint64_t fileSize = 0;
};

class MachOFile {
public:
    [[nodiscard]] static std::optional<MachOFile> Open(const std::string &path,
                                                       std::string &error);

    [[nodiscard]] const std::array<std::uint8_t, 16> &Uuid() const { return uuid_; }
    [[nodiscard]] std::uint64_t ImageBase() const { return imageBase_; }
    [[nodiscard]] std::uint64_t ImageSize() const { return imageSize_; }
    [[nodiscard]] const std::string &Version() const { return version_; }
    [[nodiscard]] std::uint64_t VersionRva() const { return versionRva_; }
    [[nodiscard]] const std::vector<MachOSegment> &Segments() const { return segments_; }
    [[nodiscard]] bool ReadVirtual(std::uint64_t address, void *output,
                                   std::size_t size, std::string &error) const;
    [[nodiscard]] std::optional<std::uint64_t> FileOffsetToRva(
        std::uint64_t fileOffset) const;
    [[nodiscard]] std::optional<std::uint64_t> ResolveSymbol(
        const std::string &name) const;

private:
    std::vector<std::uint8_t> bytes_;
    std::array<std::uint8_t, 16> uuid_{};
    std::vector<MachOSegment> segments_;
    std::uint64_t imageBase_ = 0;
    std::uint64_t imageSize_ = 0;
    std::string version_;
    std::uint64_t versionRva_ = 0;
    std::vector<std::pair<std::string, std::uint64_t>> symbols_;
};

class MachOFileMemory final : public patch::Memory {
public:
    explicit MachOFileMemory(const MachOFile &file) : file_(file) {}

    std::optional<patch::MemoryRegion> MainModule(std::string &error) const override;
    std::vector<patch::MemoryRegion> MainModuleRegions(
        patch::RegionPurpose purpose, std::string &error) const override;
    std::optional<patch::Address> ResolveSymbol(const std::string &name,
                                                std::string &error) const override;
    bool Read(patch::Address address, std::uint8_t *output, std::size_t size,
              std::string &error) const override;
    bool ReadCString(patch::Address address, std::size_t maxSize, std::string &output,
                     std::string &error) const override;
    patch::WriteResult Write(patch::Address address, const std::uint8_t *data,
                             std::size_t size) override;

private:
    const MachOFile &file_;
};

} // namespace eu4dll::platform::macos
