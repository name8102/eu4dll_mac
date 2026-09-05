#pragma once

#include "runtime/patch/memory.h"
#include "runtime/patch/patch_runtime.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace eu4dll::manifest {

inline constexpr std::uint32_t kSchemaVersion = 2;
inline constexpr std::uint32_t kSchemaVersionV1 = 1;
inline constexpr std::uint32_t kDescriptorSetVersion = 1;

enum class Architecture : std::uint8_t {
    X86_64 = 1,
};

enum class ImageIdentityKind : std::uint8_t {
    MachOUuid = 1,
    FileSha256 = 2,
};

struct ImageIdentity {
    ImageIdentityKind kind = ImageIdentityKind::MachOUuid;
    std::vector<std::uint8_t> value;
};

inline bool operator==(const ImageIdentity &lhs, const ImageIdentity &rhs) {
    return lhs.kind == rhs.kind && lhs.value == rhs.value;
}

inline bool operator!=(const ImageIdentity &lhs, const ImageIdentity &rhs) {
    return !(lhs == rhs);
}

ImageIdentity MakeMachOUuidIdentity(const std::array<std::uint8_t, 16> &uuid);
ImageIdentity MakeFileSha256Identity(const std::array<std::uint8_t, 32> &digest);
bool ParseSha256Hex(const std::string &hex, std::array<std::uint8_t, 32> &digest);
std::string ToHex(const std::vector<std::uint8_t> &bytes);
const char *ToString(ImageIdentityKind kind);

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
    ImageIdentity identity;
    Architecture architecture = Architecture::X86_64;
    std::string gameVersion;
    std::uint64_t versionRva = 0;
    std::vector<PatchEntry> entries;

    // Backward-compatible accessors for macOS UUID manifests (schema v1).
    // New code should use `identity` with an explicit kind instead.
    std::array<std::uint8_t, 16> Uuid() const;
    void SetUuid(const std::array<std::uint8_t, 16> &uuid);
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
    const PatchManifest &manifest, const ImageIdentity &loadedIdentity,
    const std::string &loadedVersion, patch::Address loadedImageBase,
    patch::Memory &memory);
[[nodiscard]] LoadedImageValidation ValidateLoadedImage(
    const PatchManifest &manifest, const std::array<std::uint8_t, 16> &loadedUuid,
    const std::string &loadedVersion, patch::Address loadedImageBase,
    patch::Memory &memory);

} // namespace eu4dll::manifest
