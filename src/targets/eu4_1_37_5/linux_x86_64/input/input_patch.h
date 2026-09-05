#pragma once

#include "runtime/patch/patch_batch.h"

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::input {
patch::BatchResult PreflightInput(patch::Memory &, patch::ExecutableCodeAllocator *);
patch::BatchResult InstallInput(patch::Memory &, patch::ExecutableCodeAllocator *);
patch::BatchResult PreflightClipboard(patch::Memory &, patch::ExecutableCodeAllocator *);
patch::BatchResult InstallClipboard(patch::Memory &, patch::ExecutableCodeAllocator *);
} // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::input
