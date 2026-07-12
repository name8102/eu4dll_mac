#include "patch_manifest.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <set>
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

bool Validate(const PatchManifest &manifest, std::string &error) {
    if (manifest.schemaVersion != kSchemaVersion) {
        error = "unsupported manifest schema version";
        return false;
    }
    if (manifest.descriptorSetVersion != kDescriptorSetVersion) {
        error = "unsupported descriptor-set version";
        return false;
    }
    if (manifest.architecture != Architecture::X86_64) {
        error = "unsupported manifest architecture";
        return false;
    }
    if (manifest.gameVersion.rfind("1.37.", 0) != 0) {
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

} // namespace

bool Serialize(const PatchManifest &manifest, std::vector<std::uint8_t> &output,
               std::string &error) {
    if (!Validate(manifest, error)) return false;
    output.clear();
    output.insert(output.end(), kMagic.begin(), kMagic.end());
    AppendInteger(output, manifest.schemaVersion);
    AppendInteger(output, manifest.descriptorSetVersion);
    output.insert(output.end(), manifest.uuid.begin(), manifest.uuid.end());
    AppendInteger(output, static_cast<std::uint8_t>(manifest.architecture));
    output.insert(output.end(), 7, 0);
    if (!AppendString(output, manifest.gameVersion, error)) return false;
    AppendInteger(output, manifest.versionRva);
    AppendInteger<std::uint32_t>(output, static_cast<std::uint32_t>(manifest.entries.size()));
    for (const auto &entry : manifest.entries) {
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
        !reader.Integer(parsed.descriptorSetVersion) ||
        !reader.Bytes(parsed.uuid.data(), parsed.uuid.size()) ||
        !reader.Integer(architecture) ||
        !reader.Bytes(reserved.data(), reserved.size()) ||
        !reader.String(parsed.gameVersion) || !reader.Integer(parsed.versionRva) ||
        !reader.Integer(entryCount)) {
        error = "truncated manifest header";
        return false;
    }
    if (reserved != std::array<std::uint8_t, 7>{}) {
        error = "manifest contains unknown required header flags";
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
    const PatchManifest &manifest, const std::array<std::uint8_t, 16> &loadedUuid,
    const std::string &loadedVersion, patch::Address loadedImageBase,
    patch::Memory &memory) {
    LoadedImageValidation result;
    std::string validationError;
    if (!Validate(manifest, validationError)) {
        result.error = std::move(validationError);
        return result;
    }
    if (manifest.uuid != loadedUuid) {
        result.error = "manifest LC_UUID does not match the loaded game; rerun the installer";
        return result;
    }
    std::string regionError;
    const auto mainModule = memory.MainModule(regionError);
    if (!mainModule) {
        result.error = "could not validate manifest RVAs against the main image: " +
                       regionError;
        return result;
    }
    const auto withinMainModule = [&mainModule](patch::Address address,
                                                std::size_t size) {
        if (size == 0 || address < mainModule->address) return false;
        const auto offset = address - mainModule->address;
        return offset <= mainModule->size && size <= mainModule->size - offset;
    };
    std::string actualVersion = loadedVersion;
    if (actualVersion.empty()) {
        if (manifest.versionRva > std::numeric_limits<patch::Address>::max() - loadedImageBase) {
            result.error = "manifest version RVA overflows the loaded address space";
            return result;
        }
        if (!withinMainModule(loadedImageBase + manifest.versionRva,
                              manifest.gameVersion.size())) {
            result.error = "manifest version RVA is outside the loaded main image";
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
    if (manifest.gameVersion != actualVersion) {
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
        if (!withinMainModule(site, 1)) {
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
        if (!withinMainModule(*expectedAddress, entry.expectedBytes.size()) ||
            !withinMainModule(*mutationAddress, entry.overwriteWidth)) {
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
            if (!withinMainModule(*address, 1)) {
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
