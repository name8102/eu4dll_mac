#pragma once
#include "runtime/patch/patch_batch.h"

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::display_formatting {
patch::BatchResult PreflightDisplay(patch::Memory &, patch::ExecutableCodeAllocator *);
patch::BatchResult InstallDisplay(patch::Memory &, patch::ExecutableCodeAllocator *);
}
