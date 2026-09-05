#include "patch_manifest.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <type_traits>
#include <utility>
#include <unistd.h>

namespace eu4dll::manifest {
namespace {

constexpr std::array<std::uint8_t, 8> kMagic{{'E', 'U', '4', 'P', 'M', 'F', 0, 1}};
constexpr std::size_t kMaximumEntries = 4096;
constexpr std::size_t kMaximumString = 4096;
constexpr std::size_t kMaximumExpectedBytes = 256;
constexpr std::size_t kMaximumIdentityBytes = 64;

template <typename T>
void AppendInteger(std::vector<std::uint8_t> &output, T value) {
    using U = std::make_unsigned_t<T>;
    U bits = static_cast<U>(value);
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        output.push_back(static_cast<std::uint8_t>(bits & 0xFF));
        if constexpr (sizeof(T) > 1) bits >>= 8;
    }
}

bool AppendString(std::vector<std::uint8_t> &output, const std::string &value,
                  std::string &error) {
    if (value.empty() || value.size() > kMaximumString ||
        value.size() > std::numeric_limits<std::uint16_t>::max()) {
        error = "manifest string is empty or too long";
        return false;
    }
    AppendInteger<std::uint16_t>(output, static_cast<std::uint16_t>(value.size()));
    output.insert(output.end(), value.begin(), value.end());
    return true;
}

class Reader {
public:
    explicit Reader(const std::vector<std::uint8_t> &input) : input_(input) {}

    template <typename T> bool Integer(T &value) {
        if (remaining() < sizeof(T)) return false;
        using U = std::make_unsigned_t<T>;
        U bits = 0;
        for (std::size_t index = 0; index < sizeof(T); ++index) {
            bits |= static_cast<U>(input_[offset_++]) << (index * 8);
        }
        value = static_cast<T>(bits);
        return true;
    }

    bool Bytes(std::uint8_t *output, std::size_t size) {
        if (remaining() < size) return false;
        std::memcpy(output, input_.data() + offset_, size);
        offset_ += size;
        return true;
    }

    bool String(std::string &value) {
        std::uint16_t size = 0;
        if (!Integer(size) || size == 0 || size > kMaximumString || remaining() < size) {
            return false;
        }
        value.assign(reinterpret_cast<const char *>(input_.data() + offset_), size);
        offset_ += size;
        return value.find('\0') == std::string::npos;
    }

    std::size_t remaining() const { return input_.size() - offset_; }

private:
    const std::vector<std::uint8_t> &input_;
    std::size_t offset_ = 0;
};

bool IsSupportedGameVersion(const std::string &version) {
    // Short installer form: "1.37.5". Full in-memory form may be
    // "EU4 v1.37.5.0 Inca"; accept any 1.37.x marker without hard-coding the
    // patch, build, or codename text.
    if (version.rfind("1.37.", 0) == 0) return true;
    return version.find("v1.37.") != std::string::npos;
}

bool GameVersionsMatch(const std::string &manifestVersion,
                       const std::string &actualVersion) {
    if (manifestVersion == actualVersion) return true;
    // Installer records the short "1.37.5" while the loaded image may expose
    // the full "EU4 v1.37.5.0 Inca" string. Accept containment in either
    // direction once both sides are known 1.37.x markers.
    if (!IsSupportedGameVersion(manifestVersion) ||
        !IsSupportedGameVersion(actualVersion)) {
        return false;
    }
    return actualVersion.find(manifestVersion) != std::string::npos ||
           manifestVersion.find(actualVersion) != std::string::npos;
}

bool Validate(const PatchManifest &manifest, std::string &error) {
    if (manifest.schemaVersion != kSchemaVersion &&
        manifest.schemaVersion != kSchemaVersionV1) {
        error = "unsupported manifest schema version";
        return false;
    }
    if (manifest.descriptorSetVersion != kDescriptorSetVersion) {
        error = "unsupported descriptor-set version";
        return false;
    }
    switch (manifest.identity.kind) {
        case ImageIdentityKind::MachOUuid:
            if (manifest.identity.value.size() != 16) {
                error = "Mach-O UUID identity must be 16 bytes";
                return false;
            }
            break;
        case ImageIdentityKind::FileSha256:
            if (manifest.identity.value.size() != 32) {
                error = "file SHA-256 identity must be 32 bytes";
                return false;
            }
            break;
        default:
            error = "unsupported manifest identity kind";
            return false;
    }
    if (manifest.architecture != Architecture::X86_64) {
        error = "unsupported manifest architecture";
        return false;
    }
    if (!IsSupportedGameVersion(manifest.gameVersion)) {
        error = "manifest game version is not EU4 1.37.x";
        return false;
    }
    if (manifest.entries.empty() || manifest.entries.size() > kMaximumEntries) {
        error = "manifest entry count is outside the supported range";
        return false;
    }
    std::set<std::string> ids;
    for (const auto &entry : manifest.entries) {
        if (entry.id.empty() || !ids.insert(entry.id).second) {
            error = "manifest contains an empty or duplicate patch id";
            return false;
        }
        if (entry.expectedBytes.empty() ||
            entry.expectedBytes.size() > kMaximumExpectedBytes ||
            entry.overwriteWidth == 0 ||
            entry.expectedBytes.size() < entry.overwriteWidth) {
            error = "manifest entry has an inconsistent expected-byte contract";
            return false;
        }
        std::set<std::string> continuationNames;
        for (const auto &continuation : entry.continuations) {
            if (continuation.first.empty() ||
                !continuationNames.insert(continuation.first).second) {
                error = "manifest entry has an empty or duplicate continuation";
                return false;
            }
        }
    }
    return true;
}

bool AppendIdentity(std::vector<std::uint8_t> &output,
                    const ImageIdentity &identity, std::string &error) {
    if (identity.value.empty() || identity.value.size() > kMaximumIdentityBytes) {
        error = "manifest identity value is empty or too long";
        return false;
    }
    AppendInteger(output, static_cast<std::uint8_t>(identity.kind));
    output.insert(output.end(), 7, 0);
    AppendInteger<std::uint32_t>(
        output, static_cast<std::uint32_t>(identity.value.size()));
    output.insert(output.end(), identity.value.begin(), identity.value.end());
    return true;
}

bool ReadIdentity(Reader &reader, ImageIdentity &identity, std::string &error) {
    std::uint8_t kind = 0;
    std::array<std::uint8_t, 7> reserved{};
    std::uint32_t valueSize = 0;
    if (!reader.Integer(kind) || !reader.Bytes(reserved.data(), reserved.size()) ||
        reserved != std::array<std::uint8_t, 7>{} || !reader.Integer(valueSize)) {
        error = "truncated manifest identity";
        return false;
    }
    if (kind != static_cast<std::uint8_t>(ImageIdentityKind::MachOUuid) &&
        kind != static_cast<std::uint8_t>(ImageIdentityKind::FileSha256)) {
        error = "unsupported manifest identity kind";
        return false;
    }
    if (valueSize == 0 || valueSize > kMaximumIdentityBytes ||
        reader.remaining() < valueSize) {
        error = "invalid manifest identity size";
        return false;
    }
    identity.kind = static_cast<ImageIdentityKind>(kind);
    identity.value.resize(valueSize);
    if (!reader.Bytes(identity.value.data(), valueSize)) {
        error = "truncated manifest identity value";
        return false;
    }
    return true;
}

} // namespace

ImageIdentity MakeMachOUuidIdentity(const std::array<std::uint8_t, 16> &uuid) {
    ImageIdentity identity;
    identity.kind = ImageIdentityKind::MachOUuid;
    identity.value.assign(uuid.begin(), uuid.end());
    return identity;
}

ImageIdentity MakeFileSha256Identity(const std::array<std::uint8_t, 32> &digest) {
    ImageIdentity identity;
    identity.kind = ImageIdentityKind::FileSha256;
    identity.value.assign(digest.begin(), digest.end());
    return identity;
}

bool ParseSha256Hex(const std::string &hex, std::array<std::uint8_t, 32> &digest) {
    if (hex.size() != 64) return false;
    const auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < 32; ++i) {
        const int hi = nibble(hex[2 * i]);
        const int lo = nibble(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        digest[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return true;
}

std::string ToHex(const std::vector<std::uint8_t> &bytes) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        out.push_back(kDigits[byte >> 4]);
        out.push_back(kDigits[byte & 0xF]);
    }
    return out;
}

const char *ToString(ImageIdentityKind kind) {
    switch (kind) {
        case ImageIdentityKind::MachOUuid:
            return "macho-uuid";
        case ImageIdentityKind::FileSha256:
            return "file-sha256";
    }
    return "unknown";
}

std::array<std::uint8_t, 16> PatchManifest::Uuid() const {
    std::array<std::uint8_t, 16> uuid{};
    if (identity.kind == ImageIdentityKind::MachOUuid &&
        identity.value.size() == uuid.size()) {
        std::copy(identity.value.begin(), identity.value.end(), uuid.begin());
    }
    return uuid;
}

void PatchManifest::SetUuid(const std::array<std::uint8_t, 16> &uuid) {
    identity = MakeMachOUuidIdentity(uuid);
}

bool Serialize(const PatchManifest &manifest, std::vector<std::uint8_t> &output,
               std::string &error) {
    PatchManifest normalized = manifest;
    normalized.schemaVersion = kSchemaVersion;
    if (!Validate(normalized, error)) return false;
    output.clear();
    output.insert(output.end(), kMagic.begin(), kMagic.end());
    AppendInteger(output, normalized.schemaVersion);
    AppendInteger(output, normalized.descriptorSetVersion);
    if (!AppendIdentity(output, normalized.identity, error)) return false;
    AppendInteger(output, static_cast<std::uint8_t>(normalized.architecture));
    output.insert(output.end(), 7, 0);
    if (!AppendString(output, normalized.gameVersion, error)) return false;
    AppendInteger(output, normalized.versionRva);
    AppendInteger<std::uint32_t>(output, static_cast<std::uint32_t>(normalized.entries.size()));
    for (const auto &entry : normalized.entries) {
        if (!AppendString(output, entry.id, error)) return false;
        AppendInteger(output, entry.siteRva);
        AppendInteger(output, entry.expectedOffset);
        AppendInteger<std::uint32_t>(output,
                                     static_cast<std::uint32_t>(entry.expectedBytes.size()));
        output.insert(output.end(), entry.expectedBytes.begin(), entry.expectedBytes.end());
        AppendInteger(output, entry.mutationOffset);
        AppendInteger(output, entry.overwriteWidth);
        if (entry.continuations.size() > 64) {
            error = "manifest entry has too many continuations";
            return false;
        }
        AppendInteger<std::uint16_t>(output,
                                     static_cast<std::uint16_t>(entry.continuations.size()));
        for (const auto &continuation : entry.continuations) {
            if (!AppendString(output, continuation.first, error)) return false;
            AppendInteger(output, continuation.second);
        }
        AppendInteger<std::uint8_t>(output, entry.optimizeHook ? 1 : 0);
        output.insert(output.end(), 7, 0);
    }
    return true;
}

bool Parse(const std::vector<std::uint8_t> &input, PatchManifest &manifest,
           std::string &error) {
    Reader reader(input);
    std::array<std::uint8_t, 8> magic{};
    if (!reader.Bytes(magic.data(), magic.size()) || magic != kMagic) {
        error = "invalid manifest magic";
        return false;
    }
    PatchManifest parsed;
    std::uint8_t architecture = 0;
    std::array<std::uint8_t, 7> reserved{};
    std::uint32_t entryCount = 0;
    if (!reader.Integer(parsed.schemaVersion) ||
        !reader.Integer(parsed.descriptorSetVersion)) {
        error = "truncated manifest header";
        return false;
    }
    if (parsed.schemaVersion == kSchemaVersionV1) {
        std::array<std::uint8_t, 16> uuid{};
        if (!reader.Bytes(uuid.data(), uuid.size()) ||
            !reader.Integer(architecture) ||
            !reader.Bytes(reserved.data(), reserved.size()) ||
            reserved != std::array<std::uint8_t, 7>{} ||
            !reader.String(parsed.gameVersion) || !reader.Integer(parsed.versionRva) ||
            !reader.Integer(entryCount)) {
            error = "truncated manifest header";
            return false;
        }
        parsed.identity = MakeMachOUuidIdentity(uuid);
    } else if (parsed.schemaVersion == kSchemaVersion) {
        if (!ReadIdentity(reader, parsed.identity, error)) {
            return false;
        }
        if (!reader.Integer(architecture) ||
            !reader.Bytes(reserved.data(), reserved.size()) ||
            reserved != std::array<std::uint8_t, 7>{} ||
            !reader.String(parsed.gameVersion) || !reader.Integer(parsed.versionRva) ||
            !reader.Integer(entryCount)) {
            error = "truncated manifest header";
            return false;
        }
    } else {
        error = "unsupported manifest schema version";
        return false;
    }
    parsed.architecture = static_cast<Architecture>(architecture);
    if (entryCount == 0 || entryCount > kMaximumEntries) {
        error = "invalid manifest entry count";
        return false;
    }
    parsed.entries.reserve(entryCount);
    for (std::uint32_t index = 0; index < entryCount; ++index) {
        PatchEntry entry;
        std::uint32_t expectedSize = 0;
        std::uint16_t continuationCount = 0;
        if (!reader.String(entry.id) || !reader.Integer(entry.siteRva) ||
            !reader.Integer(entry.expectedOffset) || !reader.Integer(expectedSize) ||
            expectedSize == 0 || expectedSize > kMaximumExpectedBytes ||
            reader.remaining() < expectedSize) {
            error = "malformed manifest entry";
            return false;
        }
        entry.expectedBytes.resize(expectedSize);
        if (!reader.Bytes(entry.expectedBytes.data(), expectedSize) ||
            !reader.Integer(entry.mutationOffset) ||
            !reader.Integer(entry.overwriteWidth) ||
            !reader.Integer(continuationCount) || continuationCount > 64) {
            error = "truncated manifest entry";
            return false;
        }
        for (std::uint16_t continuationIndex = 0;
             continuationIndex < continuationCount; ++continuationIndex) {
            std::string name;
            std::int64_t offset = 0;
            if (!reader.String(name) || !reader.Integer(offset)) {
                error = "truncated manifest continuation";
                return false;
            }
            entry.continuations.emplace_back(std::move(name), offset);
        }
        std::uint8_t optimize = 0;
        reserved.fill(0);
        if (!reader.Integer(optimize) || optimize > 1 ||
            !reader.Bytes(reserved.data(), reserved.size()) ||
            reserved != std::array<std::uint8_t, 7>{}) {
            error = "manifest contains unknown required entry flags";
            return false;
        }
        entry.optimizeHook = optimize != 0;
        parsed.entries.push_back(std::move(entry));
    }
    if (reader.remaining() != 0) {
        error = "manifest has trailing data";
        return false;
    }
    if (!Validate(parsed, error)) return false;
    // Normalize v1 manifests to the current schema version on load so callers
    // always observe one identity representation.
    parsed.schemaVersion = kSchemaVersion;
    manifest = std::move(parsed);
    return true;
}

bool WriteAtomically(const std::string &path, const PatchManifest &manifest,
                     std::string &error) {
    std::vector<std::uint8_t> bytes;
    if (!Serialize(manifest, bytes, error)) return false;
    const std::string temporary = path + ".tmp." + std::to_string(getpid());
    const int descriptor = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (descriptor < 0) {
        error = "could not create temporary manifest: " + std::string(std::strerror(errno));
        return false;
    }
    std::size_t written = 0;
    bool success = true;
    while (written < bytes.size()) {
        const auto count = write(descriptor, bytes.data() + written, bytes.size() - written);
        if (count <= 0) {
            error = "could not write temporary manifest: " + std::string(std::strerror(errno));
            success = false;
            break;
        }
        written += static_cast<std::size_t>(count);
    }
    if (success && fsync(descriptor) != 0) {
        error = "could not sync temporary manifest: " + std::string(std::strerror(errno));
        success = false;
    }
    if (close(descriptor) != 0 && success) {
        error = "could not close temporary manifest: " + std::string(std::strerror(errno));
        success = false;
    }
    if (success && rename(temporary.c_str(), path.c_str()) != 0) {
        error = "could not install manifest atomically: " + std::string(std::strerror(errno));
        success = false;
    }
    if (!success) unlink(temporary.c_str());
    return success;
}

bool ReadFile(const std::string &path, PatchManifest &manifest, std::string &error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "could not open manifest";
        return false;
    }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                                    std::istreambuf_iterator<char>());
    return Parse(bytes, manifest, error);
}

LoadedImageValidation ValidateLoadedImage(
    const PatchManifest &manifest, const ImageIdentity &loadedIdentity,
    const std::string &loadedVersion, patch::Address loadedImageBase,
    patch::Memory &memory) {
    LoadedImageValidation result;
    std::string validationError;
    if (!Validate(manifest, validationError)) {
        result.error = std::move(validationError);
        return result;
    }
    if (manifest.identity.kind != loadedIdentity.kind) {
        std::ostringstream message;
        message << "manifest identity kind " << ToString(manifest.identity.kind)
                << " does not match loaded image kind "
                << ToString(loadedIdentity.kind) << "; rerun the installer";
        result.error = message.str();
        return result;
    }
    if (manifest.identity != loadedIdentity) {
        if (manifest.identity.kind == ImageIdentityKind::MachOUuid) {
            result.error = "manifest LC_UUID does not match the loaded game; rerun the installer";
        } else {
            result.error = "manifest file SHA-256 does not match the loaded game; rerun the installer";
        }
        return result;
    }
    std::string regionError;
    auto executableRegions =
        memory.MainModuleRegions(patch::RegionPurpose::ExecutableSearch, regionError);
    if (!regionError.empty()) {
        result.error = "could not validate manifest RVAs against the main image: " +
                       regionError;
        return result;
    }
    if (executableRegions.empty()) {
        const auto fallback = memory.MainModule(regionError);
        if (!fallback) {
            result.error = "could not validate manifest RVAs against the main image: " +
                           regionError;
            return result;
        }
        executableRegions.push_back(*fallback);
    }
    const auto withinExecutable = [&executableRegions](patch::Address address,
                                                       std::size_t size) {
        if (size == 0) return false;
        for (const auto &region : executableRegions) {
            if (address < region.address) continue;
            const auto offset = address - region.address;
            if (offset <= region.size && size <= region.size - offset) {
                return true;
            }
        }
        return false;
    };
    std::string actualVersion = loadedVersion;
    if (actualVersion.empty()) {
        if (manifest.versionRva > std::numeric_limits<patch::Address>::max() - loadedImageBase) {
            result.error = "manifest version RVA overflows the loaded address space";
            return result;
        }
        std::vector<std::uint8_t> versionBytes(manifest.gameVersion.size());
        std::string readError;
        if (!memory.Read(loadedImageBase + manifest.versionRva, versionBytes.data(),
                         versionBytes.size(), readError)) {
            result.error = "could not validate the loaded game version: " + readError;
            return result;
        }
        actualVersion.assign(versionBytes.begin(), versionBytes.end());
    }
    if (!GameVersionsMatch(manifest.gameVersion, actualVersion)) {
        result.error = "manifest game version is stale; rerun the installer";
        return result;
    }

    result.sites.reserve(manifest.entries.size());
    for (const auto &entry : manifest.entries) {
        result.failedPatchId = entry.id;
        if (entry.siteRva > std::numeric_limits<patch::Address>::max() - loadedImageBase) {
            result.error = "manifest site RVA overflows the loaded address space";
            result.sites.clear();
            return result;
        }
        const patch::Address site = loadedImageBase + entry.siteRva;
        if (!withinExecutable(site, 1)) {
            result.error = "manifest site RVA is outside the loaded main image";
            result.sites.clear();
            return result;
        }
        const auto addOffset = [](patch::Address address, std::int64_t offset)
            -> std::optional<patch::Address> {
            if (offset >= 0) {
                const auto positive = static_cast<patch::Address>(offset);
                if (positive > std::numeric_limits<patch::Address>::max() - address) {
                    return std::nullopt;
                }
                return address + positive;
            }
            const auto magnitude = static_cast<patch::Address>(-(offset + 1)) + 1;
            if (magnitude > address) return std::nullopt;
            return address - magnitude;
        };
        const auto expectedAddress = addOffset(site, entry.expectedOffset);
        const auto mutationAddress = addOffset(site, entry.mutationOffset);
        if (!expectedAddress || !mutationAddress) {
            result.error = "manifest patch offset overflows the loaded address space";
            result.sites.clear();
            return result;
        }
        if (!withinExecutable(*expectedAddress, entry.expectedBytes.size()) ||
            !withinExecutable(*mutationAddress, entry.overwriteWidth)) {
            result.error = "manifest patch span is outside the loaded main image";
            result.sites.clear();
            return result;
        }
        std::vector<std::uint8_t> actual(entry.expectedBytes.size());
        std::string readError;
        if (!memory.Read(*expectedAddress, actual.data(), actual.size(), readError)) {
            result.error = "could not validate cached patch bytes: " + readError;
            result.sites.clear();
            return result;
        }
        if (actual != entry.expectedBytes) {
            result.error = "cached patch bytes no longer match; rerun the installer";
            result.sites.clear();
            return result;
        }
        ValidatedPatchSite validated;
        validated.id = entry.id;
        validated.siteAddress = site;
        validated.mutationAddress = *mutationAddress;
        for (const auto &continuation : entry.continuations) {
            const auto address = addOffset(site, continuation.second);
            if (!address) {
                result.error = "manifest continuation offset overflows the loaded address space";
                result.sites.clear();
                return result;
            }
            if (!withinExecutable(*address, 1)) {
                result.error = "manifest continuation is outside the loaded main image";
                result.sites.clear();
                return result;
            }
            validated.continuations.emplace_back(continuation.first, *address);
        }
        result.sites.push_back(std::move(validated));
    }
    result.failedPatchId.clear();
    return result;
}

LoadedImageValidation ValidateLoadedImage(
    const PatchManifest &manifest, const std::array<std::uint8_t, 16> &loadedUuid,
    const std::string &loadedVersion, patch::Address loadedImageBase,
    patch::Memory &memory) {
    return ValidateLoadedImage(manifest, MakeMachOUuidIdentity(loadedUuid),
                               loadedVersion, loadedImageBase, memory);
}

ManifestSiteProvider::ManifestSiteProvider(const LoadedImageValidation &validation) {
    if (!validation) return;
    for (const auto &site : validation.sites) sites_.emplace(site.id, site.siteAddress);
}

std::optional<patch::Address> ManifestSiteProvider::Resolve(
    const std::string &siteId, std::string &error) const {
    const auto found = sites_.find(siteId);
    if (found == sites_.end()) {
        error = "install-time manifest does not contain patch site " + siteId;
        return std::nullopt;
    }
    return found->second;
}

} // namespace eu4dll::manifest
