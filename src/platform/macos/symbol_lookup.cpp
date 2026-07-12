#include "platform/macos/symbol_lookup.h"

#include "platform/macos/live_patch_runtime.h"
#include "runtime/diagnostics/startup_diagnostics.h"

namespace eu4dll::platform::macos {

void *ResolveLiveSymbol(const char *feature, const char *target, const char *symbol) {
    std::string error;
    const auto address = LiveProcessMemory().ResolveSymbol(symbol == nullptr ? "" : symbol,
                                                            error);
    if (address) return reinterpret_cast<void *>(static_cast<std::uintptr_t>(*address));

    diagnostics::StartupDiagnostics::Instance().RecordFailure(
        feature == nullptr ? "unknown-feature" : feature,
        target == nullptr ? "unknown-target" : target,
        patch::PatchOperation::ResolveSymbol,
        "symbol=" + std::string(symbol == nullptr ? "<null>" : symbol) + " " + error);
    return nullptr;
}

} // namespace eu4dll::platform::macos
