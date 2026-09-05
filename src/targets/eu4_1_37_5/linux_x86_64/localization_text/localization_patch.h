#pragma once

#include "runtime/patch/memory.h"
#include "runtime/patch/patch_batch.h"

#include <string>
#include <vector>

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::localization_utf8 {

// Target-owned bridge with the game's C callee ABI
// `(const char *utf8_in, char *out_buffer)`. Delegates to the portable
// localization_loading conversion (same function the canonical adapter
// calls); see ABI_NOTES.md for the buffer contract. No naked code.
void ConvertUtf8Localization(const char *utf8_in, char *out_buffer);

patch::PatchDescription ValueConversionDescription(patch::Address converterTarget);

std::vector<patch::PatchDescription> LocalizationDescriptions(
    patch::Address converterTarget);

// Preflight the value-conversion call with zero writes/allocations.
patch::BatchResult PreflightLocalization(patch::Memory &memory,
                                         patch::ExecutableCodeAllocator *allocator = nullptr);

// Atomically installs the single call redirect as one feature group.
patch::BatchResult InstallLocalization(patch::Memory &memory,
                                       patch::ExecutableCodeAllocator *allocator = nullptr);

}  // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::localization_utf8
