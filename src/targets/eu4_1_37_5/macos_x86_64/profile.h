#pragma once

#include "runtime/patch/memory.h"

#include <string>

namespace eu4dll::targets::eu4_1_37_5::macos_x86_64 {

struct ValidationResult {
    bool supported = false;
    std::string check;
    std::string message;
    std::string versionText;

    explicit operator bool() const { return supported; }
};

ValidationResult ValidateTarget(patch::Memory &memory);
ValidationResult ValidateExecutableFacts(patch::Memory &memory);
ValidationResult ValidatePatchFacts(patch::Memory &memory,
                                    std::string versionText = {});
std::string FormatValidationFailure(const ValidationResult &result);

} // namespace eu4dll::targets::eu4_1_37_5::macos_x86_64
