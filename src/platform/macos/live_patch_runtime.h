#pragma once

#include "mach_process_memory.h"
#include "runtime/patch/patch_runtime.h"

namespace eu4dll::platform::macos {

MachProcessMemory &LiveProcessMemory();
patch::PatchRuntime &LivePatchRuntime();

} // namespace eu4dll::platform::macos
