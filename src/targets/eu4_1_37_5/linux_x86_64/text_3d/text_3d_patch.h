#pragma once

#include "runtime/patch/memory.h"
#include "runtime/patch/patch_batch.h"

#include <cstdint>
#include <string>
#include <vector>

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::text_3d {

// dlsym-resolved callee used by the preprocessing hook (fail-closed at
// install; never called on preflight paths).
patch::Address &CStringAppendCharSlot();

// Target-owned return slots, published from install results before commit.
patch::Address &PreprocessingReturnSlot();
patch::Address &DrawingReturnSlot();

// Target-owned naked hooks (System V x86-64, see ABI_NOTES.md).
void NakedRender3dPreprocessing();
void NakedRender3dDrawing();

struct Text3DHookTargets {
    patch::Address preprocessing = 0;
    patch::Address drawing = 0;
};

patch::PatchDescription PreprocessingDescription(patch::Address hookTarget);
patch::PatchDescription DrawingDescription(patch::Address hookTarget);

std::vector<patch::PatchDescription> Text3DDescriptions(
    const Text3DHookTargets &targets);

// Preflight both Render3d sites with zero writes/allocations.
patch::BatchResult PreflightText3D(patch::Memory &memory,
                                   patch::ExecutableCodeAllocator *allocator = nullptr);

// Atomically installs both Render3d hooks as one feature group.
patch::BatchResult InstallText3D(patch::Memory &memory,
                                 patch::ExecutableCodeAllocator *allocator = nullptr);

}  // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::text_3d
