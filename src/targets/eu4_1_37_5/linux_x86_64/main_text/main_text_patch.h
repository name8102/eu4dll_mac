#pragma once

#include "runtime/patch/memory.h"
#include "runtime/patch/patch_batch.h"

#include <cstdint>
#include <string>
#include <vector>

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::main_text {

// Shared per-character slot: the preprocessing hook publishes the decoded
// index here; the wrapping hook reads it on the next check. Written before
// use within one character iteration (see ABI_NOTES.md assumptions).
extern std::uint32_t &RenderToScreenCurrentCharacter();

// Target-owned return/bypass slots, published from install results before
// commit; failed installs clear the slots they set.
patch::Address &PreprocessingReturnSlot();
patch::Address &PreprocessingBypassSlot();
patch::Address &WrappingReturnSlot();
patch::Address &WrappingBypassSlot();
patch::Address &DrawingReturnSlot();
patch::Address &DrawingBypassSlot();

// Target-owned naked hooks (System V x86-64, see ABI_NOTES.md). No calls,
// no XMM use; every path balances its pushes.
void NakedRenderToScreenPreprocessing();
void NakedRenderToScreenWrapping();
void NakedRenderToScreenDrawing();

struct MainTextHookTargets {
    patch::Address preprocessing = 0;
    patch::Address wrapping = 0;
    patch::Address drawing = 0;
};

patch::PatchDescription PreprocessingDescription(patch::Address hookTarget);
patch::PatchDescription WrappingDescription(patch::Address hookTarget);
patch::PatchDescription DrawingDescription(patch::Address hookTarget);

std::vector<patch::PatchDescription> MainTextDescriptions(
    const MainTextHookTargets &targets);

// Preflight all three RenderToScreen sites with zero writes/allocations.
patch::BatchResult PreflightMainText(patch::Memory &memory,
                                     patch::ExecutableCodeAllocator *allocator = nullptr);

// Atomically installs the three RenderToScreen hooks as one feature group.
patch::BatchResult InstallMainText(patch::Memory &memory,
                                   patch::ExecutableCodeAllocator *allocator = nullptr);

}  // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::main_text
