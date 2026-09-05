#include "runtime/patch/memory.h"

namespace eu4dll::patch {

std::vector<MemoryRegion> Memory::MainModuleRegions(
    RegionPurpose purpose, std::string &error) const {
    // Default single-region behavior preserves the Mach-O __TEXT model.
    // ExecutableSearch returns the legacy MainModule region; other purposes
    // intentionally return an empty set so callers fail closed rather than
    // scanning an inappropriate region.
    if (purpose != RegionPurpose::ExecutableSearch) {
        error.clear();
        return {};
    }
    auto region = MainModule(error);
    if (!region) return {};
    return {*region};
}

}  // namespace eu4dll::patch
