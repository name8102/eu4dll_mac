#pragma once

#include "runtime/patch/memory.h"

#include <string>

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64 {

struct ValidationResult {
    bool supported = false;
    std::string check;
    std::string message;
    std::string versionText;

    explicit operator bool() const { return supported; }
};

// Validates ELF header facts (magic, class, machine) plus the expected
// game-version text in readable memory. Performs zero mutation.
ValidationResult ValidateExecutableFacts(patch::Memory &memory);
// Verifies the four base patch sites (pattern uniqueness + expected bytes)
// without mutating memory. Requires the three base symbols to resolve.
ValidationResult ValidatePatchFacts(patch::Memory &memory,
                                    std::string versionText = {});
// Combined in-memory validation (header + version + patch facts).
ValidationResult ValidateTarget(patch::Memory &memory);
// File-identity validation: exact SHA-256 match, fail-closed by default.
// When `allowUnsupportedOverride` is true, a mismatch still fails unless the
// caller has explicitly acknowledged the development override; the message
// always distinguishes an override from real support.
ValidationResult ValidateFileIdentity(const std::string &executablePath,
                                      bool allowUnsupportedOverride = false);
// Full host validation: file identity plus in-memory facts.
ValidationResult ValidateTargetWithFile(patch::Memory &memory,
                                        const std::string &executablePath,
                                        bool allowUnsupportedOverride = false);
std::string FormatValidationFailure(const ValidationResult &result);

}  // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64
