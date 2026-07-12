#pragma once

#include "runtime/patch/memory.h"
#include "runtime/patch/patch_runtime.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace eu4dll::manifest {

inline constexpr std::uint32_t kSchemaVersion = 1;
inline constexpr std::uint32_t kDescriptorSetVersion = 1;

enum class Architecture : std::uint8_t {
    X86_64 = 1,
};

struct PatchEntry {
    std::string id;
    std::uint64_t siteRva = 0;
    std::int64_t expectedOffset = 0;
    std::vector<std::uint8_t> expectedBytes;
    std::int64_t mutationOffset = 0;
    std::uint32_t overwriteWidth = 0;
    std::vector<std::pair<std::string, std::int64_t>> continuations;
    bool optimizeHook = false;
};

struct PatchManifest {
    std::uint32_t schemaVersion = kSchemaVersion;
    std::uint32_t descriptorSetVersion = kDescriptorSetVersion;
    std::array<std::uint8_t, 16> uuid{};
    Architecture architecture = Architecture::X86_64;
    std::string gameVersion;
    std::uint64_t versionRva = 0;
    std::vector<PatchEntry> entries;
};

struct ValidatedPatchSite {
    std::string id;
    patch::Address siteAddress = 0;
    patch::Address mutationAddress = 0;
    std::vector<std::pair<std::string, patch::Address>> continuations;
};

struct LoadedImageValidation {
    std::vector<ValidatedPatchSite> sites;
    std::string failedPatchId;
    std::string error;

    explicit operator bool() const { return error.empty(); }
};

class ManifestSiteProvider final : public patch::ResolvedSiteProvider {
public:
    explicit ManifestSiteProvider(const LoadedImageValidation &validation);
    std::optional<patch::Address> Resolve(const std::string &siteId,
                                          std::string &error) const override;

private:
    std::unordered_map<std::string, patch::Address> sites_;
};

[[nodiscard]] bool Serialize(const PatchManifest &manifest,
                             std::vector<std::uint8_t> &output,
                             std::string &error);
[[nodiscard]] bool Parse(const std::vector<std::uint8_t> &input,
                         PatchManifest &manifest, std::string &error);
[[nodiscard]] bool WriteAtomically(const std::string &path,
                                   const PatchManifest &manifest,
                                   std::string &error);
[[nodiscard]] bool ReadFile(const std::string &path, PatchManifest &manifest,
                            std::string &error);
[[nodiscard]] LoadedImageValidation ValidateLoadedImage(
    const PatchManifest &manifest, const std::array<std::uint8_t, 16> &loadedUuid,
    const std::string &loadedVersion, patch::Address loadedImageBase,
    patch::Memory &memory);

} // namespace eu4dll::manifest
