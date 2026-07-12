#include "profile.h"

#include "runtime/diagnostics/patch_diagnostic.h"
#include "runtime/patch/patch_runtime.h"
#include "targets/eu4_1_37_5/macos_x86_64/target_facts.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>

namespace eu4dll::targets::eu4_1_37_5::macos_x86_64 {
namespace {

ValidationResult Failure(std::string check, std::string message,
                         std::string versionText = {}) {
    ValidationResult result;
    result.check = std::move(check);
    result.message = std::move(message);
    result.versionText = std::move(versionText);
    return result;
}

bool IsSupportedVersionText(const std::string &text) {
    constexpr char marker[] = "EU4 v";
    if (text.compare(0, sizeof(marker) - 1, marker) != 0) return false;
    std::size_t cursor = sizeof(marker) - 1;
    const auto parseComponent = [&text, &cursor](unsigned &value) {
        if (cursor >= text.size() ||
            !std::isdigit(static_cast<unsigned char>(text[cursor]))) return false;
        value = 0;
        while (cursor < text.size() &&
               std::isdigit(static_cast<unsigned char>(text[cursor]))) {
            value = value * 10 + static_cast<unsigned>(text[cursor] - '0');
            ++cursor;
        }
        return true;
    };
    unsigned major = 0;
    unsigned minor = 0;
    if (!parseComponent(major) || cursor >= text.size() || text[cursor++] != '.' ||
        !parseComponent(minor)) return false;
    return major == 1 && minor == 37 && cursor < text.size() && text[cursor] == '.';
}

struct FingerprintProbe {
    const char *check;
    const char *pattern;
    std::ptrdiff_t expectedOffset;
    const std::uint8_t *expected;
    std::size_t expectedSize;
};

constexpr std::array<FingerprintProbe, 7> kFingerprintProbes{{
    {"fingerprint.input-handle-key", fingerprint::kInputHandleKeyPattern, 0,
     fingerprint::kInputHandleKeyOriginal.data(), fingerprint::kInputHandleKeyOriginal.size()},
    {"fingerprint.map-curve-text", fingerprint::kMapCurveTextPattern, 0,
     fingerprint::kMapCurveTextOriginal.data(), fingerprint::kMapCurveTextOriginal.size()},
    {"fingerprint.monarch-name", fingerprint::kMonarchNamePattern, 0,
     fingerprint::kMonarchNameOriginal.data(), fingerprint::kMonarchNameOriginal.size()},
    {"fingerprint.input-pdx-commit", input::kHandlePdxEvents1.pattern,
     input::kHandlePdxEvents1.mutationOffset, input::kHandlePdxEvents1Original.data(),
     input::kHandlePdxEvents1Original.size()},
    {"fingerprint.input-pdx-editing", input::kHandlePdxEvents2.pattern,
     input::kHandlePdxEvents2.mutationOffset, input::kHandlePdxEvents2Original.data(),
     input::kHandlePdxEvents2Original.size()},
    {"fingerprint.input-move-left", input::kMoveLeft.pattern,
     input::kMoveLeft.mutationOffset, input::kMoveLeftOriginal.data(),
     input::kMoveLeftOriginal.size()},
    {"fingerprint.input-move-right", input::kMoveRight.pattern,
     input::kMoveRight.mutationOffset, input::kMoveRightOriginal.data(),
     input::kMoveRightOriginal.size()},
}};

} // namespace

ValidationResult ValidateExecutableFacts(patch::Memory &memory) {
    std::string error;
    const auto mainModule = memory.MainModule(error);
    if (!mainModule) {
        return Failure("main-module", error);
    }

    std::array<std::uint32_t, 4> machHeader{};
    if (!memory.Read(mainModule->address,
                     reinterpret_cast<std::uint8_t *>(machHeader.data()),
                     sizeof(machHeader), error)) {
        return Failure("mach-header", error);
    }
    if (machHeader[0] != executable::kMach64Magic) {
        return Failure("mach-header.magic", "main executable is not a 64-bit Mach-O image");
    }
    if (machHeader[1] != executable::kCpuTypeX86_64) {
        return Failure("mach-header.cpu-type", "main executable is not x86-64");
    }
    if (machHeader[3] != executable::kMachExecuteFileType) {
        return Failure("mach-header.file-type", "main image is not a Mach-O executable");
    }

    patch::PatchRuntime runtime(memory);
    patch::PatternLocation versionLocation;
    versionLocation.pattern = executable::kVersionPattern;
    // The supported executable contains the identical full version marker in
    // several UI/reporting strings. Exact text plus the unique instruction
    // fingerprints below identifies the target; the marker itself is not a
    // unique code anchor.
    versionLocation.requireUnique = false;
    const auto version = runtime.Locate(versionLocation, "version-marker", kDiagnosticTargetId);
    if (!version) {
        return Failure("version-marker", patch::FormatDiagnostic(version.diagnostic));
    }

    std::string versionText;
    if (!memory.ReadCString(version.address, executable::kMaximumVersionText,
                            versionText, error)) {
        return Failure("version-text.read", error);
    }
    if (!IsSupportedVersionText(versionText)) {
        return Failure("version-text.series",
                       std::string("expected EU4 1.37.x, got ") + versionText,
                       versionText);
    }

    ValidationResult result;
    result.supported = true;
    result.check = "complete";
    result.message = "Mach-O architecture and EU4 1.37.x version passed";
    result.versionText = std::move(versionText);
    return result;
}

ValidationResult ValidatePatchFacts(patch::Memory &memory, std::string versionText) {
    std::string error;
    patch::PatchRuntime runtime(memory);
    for (const auto &probe : kFingerprintProbes) {
        patch::PatternLocation location;
        location.pattern = probe.pattern;
        location.requireUnique = true;
        const auto located = runtime.Locate(location, probe.check, kDiagnosticTargetId);
        if (!located) {
            return Failure(probe.check, patch::FormatDiagnostic(located.diagnostic), versionText);
        }
        std::array<std::uint8_t, input::kCallOverwriteWidth> actual{};
        if (!memory.Read(located.address + probe.expectedOffset, actual.data(),
                         probe.expectedSize, error)) {
            return Failure(std::string(probe.check) + ".original-bytes", error, versionText);
        }
        if (!std::equal(actual.begin(), actual.begin() + probe.expectedSize,
                        probe.expected)) {
            return Failure(std::string(probe.check) + ".original-bytes",
                           "original bytes do not match the fixed target profile", versionText);
        }
    }

    ValidationResult result;
    result.supported = true;
    result.check = "patch-facts.complete";
    result.message = "all fixed patch fingerprints passed";
    result.versionText = std::move(versionText);
    return result;
}

ValidationResult ValidateTarget(patch::Memory &memory) {
    return ValidateExecutableFacts(memory);
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

} // namespace eu4dll::targets::eu4_1_37_5::macos_x86_64
