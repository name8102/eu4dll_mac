#pragma once

#include "runtime/patch/memory.h"
#include "runtime/patch/patch_batch.h"

#include <cstdint>
#include <string>
#include <vector>

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::tooltip {

// dlsym-resolved callee used by the preprocessing hook. Resolved through
// Memory::ResolveSymbol at install (fail-closed); never called on any
// preflight path.
patch::Address &CStringAppendCharSlot();

// Shared per-character slot: preprocessing publishes, wrapping reads within
// one character iteration (see ABI_NOTES.md assumptions).
extern std::uint32_t &RenderToTextureCurrentCharacter();

// Target-owned return/bypass slots, published from install results before
// commit; failed installs clear the slots they set.
patch::Address &PreprocessingReturnSlot();
patch::Address &WrappingReturnSlot();
patch::Address &WrappingBypassSlot();
patch::Address &DrawingReturnSlot();

// Target-owned naked hooks (System V x86-64, see ABI_NOTES.md). The
// preprocessing hook calls CString::operator+=(char); the audit is in
// ABI_NOTES.md. No XMM use anywhere.
void NakedRenderToTexturePreprocessing();
void NakedRenderToTextureWrapping();
void NakedRenderToTextureDrawing();

struct TooltipHookTargets {
    patch::Address preprocessing = 0;
    patch::Address wrapping = 0;
    patch::Address drawing = 0;
};

patch::PatchDescription PreprocessingDescription(patch::Address hookTarget);
patch::PatchDescription WrappingDescription(patch::Address hookTarget);
patch::PatchDescription DrawingDescription(patch::Address hookTarget);

std::vector<patch::PatchDescription> TooltipDescriptions(
    const TooltipHookTargets &targets);

// Preflight all three RenderToTexture sites with zero writes/allocations.
patch::BatchResult PreflightTooltip(patch::Memory &memory,
                                    patch::ExecutableCodeAllocator *allocator = nullptr);

// Atomically installs the three RenderToTexture hooks as one feature group.
patch::BatchResult InstallTooltip(patch::Memory &memory,
                                  patch::ExecutableCodeAllocator *allocator = nullptr);

}  // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::tooltip
