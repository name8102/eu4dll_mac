#include "targets/eu4_1_37_5/linux_x86_64/profile.h"

#include "platform/linux/linux_elf_identity.h"
#include "runtime/manifest/patch_manifest.h"
#include "runtime/patch/patch_runtime.h"
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <vector>

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64 {
namespace {

ValidationResult Failure(std::string check, std::string message,
                         std::string versionText = {}) {
    ValidationResult result;
    result.check = std::move(check);
    result.message = std::move(message);
    result.versionText = std::move(versionText);
    return result;
}

}  // namespace

ValidationResult ValidateExecutableFacts(patch::Memory &memory) {
    std::string error;
    auto regions =
        memory.MainModuleRegions(patch::RegionPurpose::ExecutableSearch, error);
    if (!error.empty()) {
        return Failure("main-module", error);
    }
    if (regions.empty()) {
        // Fall back to the legacy single-region view for byte-buffer fixtures.
        auto single = memory.MainModule(error);
        if (!single) return Failure("main-module", error);
        regions.push_back(*single);
    }
    // The ELF header lives in the lowest PT_LOAD segment, which for PIE hosts
    // is read-only (not executable). Include every segment class when locating
    // the image base; byte-buffer fixtures return empty non-executable sets
    // and fall back to the single executable view above.
    patch::Address imageBase = regions.front().address;
    for (const auto purpose : {patch::RegionPurpose::ReadOnlySearch,
                               patch::RegionPurpose::Writable}) {
        std::string purposeError;
        const auto extra = memory.MainModuleRegions(purpose, purposeError);
        if (!purposeError.empty()) {
            return Failure("main-module", purposeError);
        }
        for (const auto &region : extra) {
            imageBase = std::min(imageBase, region.address);
        }
    }
    for (const auto &region : regions) {
        imageBase = std::min(imageBase, region.address);
    }

    std::array<std::uint8_t, 20> header{};
    if (!memory.Read(imageBase, header.data(), header.size(), error)) {
        return Failure("elf-header.read", error);
    }
    if (header[0] != executable::kElfMagic[0] || header[1] != executable::kElfMagic[1] ||
        header[2] != executable::kElfMagic[2] || header[3] != executable::kElfMagic[3]) {
        return Failure("elf-header.magic", "main executable is not an ELF image");
    }
    if (header[4] != executable::kElfClass64) {
        return Failure("elf-header.class", "main executable is not 64-bit");
    }
    std::uint16_t machine = 0;
    std::memcpy(&machine, header.data() + 18, sizeof(machine));
    if (machine != executable::kElfMachineX86_64) {
        return Failure("elf-header.machine", "main executable is not x86-64");
    }

    if (!linux_platform::ContainsVersionString(memory, kExpectedVersionText)) {
        return Failure("version-text.series",
                       std::string("expected ") + kExpectedVersionText);
    }

    ValidationResult result;
    result.supported = true;
    result.check = "complete";
    result.message = "ELF architecture and EU4 1.37.5 version passed";
    result.versionText = kExpectedVersionText;
    return result;
}

ValidationResult ValidatePatchFacts(patch::Memory &memory, std::string versionText) {
    patch::PatchRuntime runtime(memory);
    struct Probe {
        const char *check;
        const char *pattern;
        const char *symbol;
        std::size_t searchSize;
        std::ptrdiff_t expectedOffset;
        const std::uint8_t *expected;
        std::size_t expectedSize;
    };
    const Probe probes[] = {
        {"base.allocate-font", base::kAllocateFontPattern, symbols::kReadGameSpecific,
         base::kAllocateFontSearchSize, base::kAllocateFontMutationOffset,
         base::kAllocateFontOriginal.data(), base::kAllocateFontOriginal.size()},
        {"base.character-limit", base::kCharacterLimitPattern, symbols::kParseFontFile,
         base::kParseFontFileSearchSize, base::kCharacterLimitMutationOffset,
         &base::kCharacterLimitOriginal, 1},
        {"base.character-index", base::kCharacterIndexPattern, symbols::kParseFontFile,
         base::kParseFontFileSearchSize, 0, base::kCharacterIndexOriginal.data(),
         base::kCharacterIndexOriginal.size()},
        {"base.texture-size", base::kTextureSizePattern, symbols::kLoadTexture,
         base::kLoadTextureSearchSize, base::kTextureSizeMutationOffset,
         &base::kTextureSizeOriginal, 1},
    };
    for (const auto &probe : probes) {
        patch::PatternLocation location;
        location.pattern = probe.pattern;
        location.requireUnique = true;
        location.scope =
            patch::SearchScope::Symbol(probe.symbol, probe.searchSize);
        const auto located =
            runtime.Locate(location, probe.check, kDiagnosticTargetId);
        if (!located) {
            return Failure(probe.check, patch::FormatDiagnostic(located.diagnostic),
                           versionText);
        }
        std::vector<std::uint8_t> actual(probe.expectedSize);
        std::string error;
        const auto expectedAddress = located.address + probe.expectedOffset;
        // Overflow guard for the fixture path (addresses are small here).
        if (expectedAddress < located.address) {
            return Failure(std::string(probe.check) + ".original-bytes",
                           "expected-byte offset overflows the address space", versionText);
        }
        if (!memory.Read(expectedAddress, actual.data(), actual.size(), error)) {
            return Failure(std::string(probe.check) + ".original-bytes", error,
                           versionText);
        }
        if (!std::equal(actual.begin(), actual.end(), probe.expected)) {
            return Failure(std::string(probe.check) + ".original-bytes",
                           "original bytes do not match the fixed target profile",
                           versionText);
        }
    }
    ValidationResult result;
    result.supported = true;
    result.check = "patch-facts.complete";
    result.message = "all Linux base patch fingerprints passed";
    result.versionText = std::move(versionText);
    return result;
}

ValidationResult ValidateTarget(patch::Memory &memory) {
    auto executable = ValidateExecutableFacts(memory);
    if (!executable) return executable;
    return ValidatePatchFacts(memory, executable.versionText);
}

ValidationResult ValidateFileIdentity(const std::string &executablePath,
                                      bool allowUnsupportedOverride) {
    if (executablePath.empty()) {
        return Failure("elf-identity.path", "main executable path is unknown");
    }
    std::array<std::uint8_t, 32> digest{};
    std::string error;
    if (!linux_platform::ComputeFileSha256(executablePath, digest, error)) {
        return Failure("elf-identity.hash", error);
    }
    std::array<std::uint8_t, 32> expected{};
    if (!manifest::ParseSha256Hex(kSupportedElfSha256Hex, expected)) {
        return Failure("elf-identity.profile", "supported ELF digest is malformed");
    }
    if (digest != expected) {
        const std::string hex = linux_platform::Sha256Hex(digest);
        if (!allowUnsupportedOverride) {
            return Failure("elf-identity.sha256",
                           "SHA-256 mismatch (got " + hex +
                               "); refusing to install hooks");
        }
        std::fprintf(stderr,
                     "eu4dll [WARNING] EU4DLL_ALLOW_UNSUPPORTED_ELF=1: continuing "
                     "despite SHA-256 mismatch (got %s, want %s). This is NOT a "
                     "supported target.\n",
                     hex.c_str(), kSupportedElfSha256Hex);
        ValidationResult result;
        result.supported = true;
        result.check = "elf-identity.override";
        result.message = "unsupported ELF explicitly overridden; version checks still apply";
        return result;
    }
    ValidationResult result;
    result.supported = true;
    result.check = "elf-identity.sha256";
    result.message = "supported Linux ELF identity matched";
    return result;
}

ValidationResult ValidateTargetWithFile(patch::Memory &memory,
                                        const std::string &executablePath,
                                        bool allowUnsupportedOverride) {
    const auto identity = ValidateFileIdentity(executablePath, allowUnsupportedOverride);
    if (!identity) return identity;
    auto target = ValidateTarget(memory);
    if (!target) return target;
    if (identity.check == "elf-identity.override") {
        target.check = "complete-with-override";
        target.message += "; unsupported ELF override active";
    }
    return target;
}

std::string FormatValidationFailure(const ValidationResult &result) {
    std::ostringstream stream;
    stream << "target=" << kDiagnosticTargetId << " check=" << result.check
           << " message=" << result.message;
    if (!result.versionText.empty()) {
        stream << " version=" << result.versionText;
    }
    return stream.str();
}

}  // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64
