#pragma once
#include "runtime/patch/patch_batch.h"
namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::search {
patch::BatchResult PreflightSearch(patch::Memory &, patch::ExecutableCodeAllocator *);
patch::BatchResult InstallSearch(patch::Memory &, patch::ExecutableCodeAllocator *);
}
