#pragma once

#include "runtime/diagnostics/patch_diagnostic.h"
#include "runtime/patch/memory.h"
#include "runtime/patch/patch_runtime.h"

#include <cstddef>
#include <string>
#include <vector>

namespace eu4dll::targets::eu4_1_37_5::macos_x86_64 {

struct CompatibilityPreflightResult {
    std::size_t checkedSites = 0;
    std::size_t checkedSymbols = 0;
    std::vector<patch::PatchDiagnostic> failures;

    explicit operator bool() const { return failures.empty(); }
};

struct CompatibilityPatchContract {
    std::string id;
    std::size_t overwriteWidth = 0;
    patch::PatchDescription description;
};

[[nodiscard]] const std::vector<CompatibilityPatchContract> &
CompatibilityPatchRegistry();
[[nodiscard]] CompatibilityPreflightResult PreflightCompatibility(patch::Memory &memory);

} // namespace eu4dll::targets::eu4_1_37_5::macos_x86_64
