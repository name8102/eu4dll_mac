#pragma once

#include "runtime/patch/memory.h"
#include "runtime/patch/patch_batch.h"

#include <cstdint>
#include <string>
#include <vector>

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::layout {

// Target-owned return/bypass slots for the naked glyph-decoder hooks.
// Each slot is published from the batch install result (site + offset)
// before the batch commits; failed installs clear the slots they set.
patch::Address &GetHeightOfStringReturnSlot();
patch::Address &GetWidthOfStringReturnSlot();
patch::Address &GetWidthOfStringBypassSlot();
patch::Address &GetActualRequiredSizeReturnSlot();
patch::Address &GetRequiredSizeReturnSlot();
patch::Address &GetActualRealRequiredSizeActuallyReturnSlot();

// Target-owned naked hooks (System V x86-64, see ABI_NOTES.md). They decode
// one escaped sequence with the shared escaped_text marker/shift policy,
// replay the overwritten glyph-table load, and jump to the published slot.
// No calls, no XMM use, balanced pushes on every path.
void NakedGetHeightOfString();
void NakedGetWidthOfString();
void NakedGetActualRequiredSize();
void NakedGetRequiredSize();
void NakedGetActualRealRequiredSizeActually();

struct LayoutHookTargets {
    patch::Address height = 0;
    patch::Address width = 0;
    patch::Address actualRequiredSize = 0;
    patch::Address requiredSize = 0;
    patch::Address actualRealRequiredSizeActually = 0;
};

patch::PatchDescription HeightDescription(patch::Address hookTarget);
patch::PatchDescription WidthDescription(patch::Address hookTarget);
patch::PatchDescription ActualRequiredSizeDescription(patch::Address hookTarget);
patch::PatchDescription RequiredSizeDescription(patch::Address hookTarget);
patch::PatchDescription ActualRealRequiredSizeActuallyDescription(
    patch::Address hookTarget);
patch::PatchDescription WrappingGateDescription();

std::vector<patch::PatchDescription> LayoutDescriptions(
    const LayoutHookTargets &targets);

// Preflight all six text-layout sites with zero writes and zero allocations.
patch::BatchResult PreflightLayout(patch::Memory &memory,
                                   patch::ExecutableCodeAllocator *allocator = nullptr);

// Atomically installs the five glyph decoders plus the wrapping gate as one
// feature group. Publishes every return/bypass slot before committing;
// failed installs release staged trampolines, clear published slots, and
// mutate nothing. The legacy loop-tail / ellipsis-truncation probes stay
// disabled (probe-only, never installed).
patch::BatchResult InstallLayout(patch::Memory &memory,
                                 patch::ExecutableCodeAllocator *allocator = nullptr);

}  // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::layout
