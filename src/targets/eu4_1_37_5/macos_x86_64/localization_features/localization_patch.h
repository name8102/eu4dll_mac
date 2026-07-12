#pragma once

#include "runtime/patch/patch_runtime.h"
#include "targets/eu4_1_37_5/macos_x86_64/target_facts.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::localization_features {

struct ContinuationBinding {
    const char *name;
    std::ptrdiff_t offset;
    std::uintptr_t *storage;
};

struct InstallRequest {
    const char *feature = nullptr;
    const HookSite *site = nullptr;
    patch::MutationKind mutationKind = patch::MutationKind::RawBytes;
    std::uintptr_t mutationTarget = 0;
    patch::CallWidth callWidth = patch::CallWidth::Auto;
    std::vector<std::uint8_t> mutationBytes;
    std::vector<std::uint8_t> expectedBytes;
    std::vector<std::uint8_t> expectedMask;
    std::ptrdiff_t expectedOffset = 0;
    std::size_t overwrittenLength = 0;
    std::vector<std::string> referencedStrings;
    std::string symbol;
    std::size_t symbolSearchSize = 0;
    std::vector<ContinuationBinding> continuations;
    bool optimizeNakedHook = false;
};

[[nodiscard]] patch::PatchDescription BuildDescription(const InstallRequest &request);
[[nodiscard]] bool Install(const InstallRequest &request);

} // namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::localization_features
