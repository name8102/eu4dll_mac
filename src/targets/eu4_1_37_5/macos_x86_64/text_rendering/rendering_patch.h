#pragma once

#include "targets/eu4_1_37_5/macos_x86_64/text_rendering/rendering_contract.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::text_rendering {

struct ContinuationBinding {
    const char *name;
    std::uintptr_t *storage;
};

struct InstallRequest {
    PatchId id;
    std::uintptr_t mutationTarget = 0;
    std::initializer_list<ContinuationBinding> continuations;
};

// Installs one EU4 1.37.x macOS x86-64 rendering mutation through the
// shared runtime. The caller supplies fixed target facts and ABI continuations;
// this layer deliberately contains no escaped-text policy.
[[nodiscard]] bool install(const InstallRequest &request);

} // namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::text_rendering
