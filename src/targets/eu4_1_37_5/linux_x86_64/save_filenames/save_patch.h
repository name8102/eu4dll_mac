#pragma once
#include "runtime/patch/patch_batch.h"

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::save_filenames {
patch::BatchResult PreflightSave(patch::Memory &, patch::ExecutableCodeAllocator *);
patch::BatchResult InstallSave(patch::Memory &, patch::ExecutableCodeAllocator *);
}
