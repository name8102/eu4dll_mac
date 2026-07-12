#include "live_patch_runtime.h"

namespace eu4dll::platform::macos {

MachProcessMemory &LiveProcessMemory() {
    static MachProcessMemory memory;
    return memory;
}

patch::PatchRuntime &LivePatchRuntime() {
    static patch::PatchRuntime runtime(LiveProcessMemory());
    return runtime;
}

} // namespace eu4dll::platform::macos
