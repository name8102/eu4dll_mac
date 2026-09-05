#pragma once

#include "runtime/patch/memory.h"
#include "runtime/patch/patch_batch.h"

#include <cstdint>
#include <string>
#include <vector>

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::base {

// Returns the raw continuation slot read by the naked character-index hook.
// Set exactly once per install from the located site + 8 before committing.
patch::Address &ParseFontFileReturnSlot();

// Target-owned hook entry points (System V x86-64, see ABI_NOTES.md).
void *BitmapFontOperatorNewProxy();
void NakedParseFontFileCharacterIndex();

// Patch-description factories. `allocateCallTarget` is the proxy above;
// `characterIndexHookTarget` is the naked hook above. Fixture tests may pass
// any reachable address; live installs pass the real hook addresses.
patch::PatchDescription AllocateFontDescription(patch::Address allocateCallTarget);
patch::PatchDescription CharacterLimitDescription();
patch::PatchDescription CharacterIndexDescription(patch::Address hookTarget);
patch::PatchDescription TextureSizeDescription();

std::vector<patch::PatchDescription> BaseDescriptions(
    patch::Address allocateCallTarget, patch::Address characterIndexHookTarget);

// Preflight all four base sites with zero writes and zero allocations.
// Verifies pattern uniqueness, expected bytes, overlap freedom, and branch
// reachability (assuming `allocator` can provide trampolines when needed).
patch::BatchResult PreflightBase(patch::Memory &memory,
                                 patch::ExecutableCodeAllocator *allocator = nullptr);

// Atomically installs all four base modifications as one feature group.
// Discovers the character-index continuation, publishes the return slot,
// then commits. Failed installs release staged trampolines and mutate nothing.
patch::BatchResult InstallBase(patch::Memory &memory,
                               patch::ExecutableCodeAllocator *allocator = nullptr);

}  // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::base
