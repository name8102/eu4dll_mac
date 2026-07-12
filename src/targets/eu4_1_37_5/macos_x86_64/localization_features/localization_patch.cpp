#include "localization_patch.h"

#include "platform/macos/live_patch_runtime.h"
#include "runtime/diagnostics/patch_diagnostic.h"
#include "runtime/diagnostics/startup_diagnostics.h"

#include <iostream>

namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::localization_features {

bool Install(const InstallRequest &request) {
    patch::PatchDescription description;
    try {
        description = BuildDescription(request);
    } catch (const std::exception &error) {
        diagnostics::StartupDiagnostics::Instance().RecordFailure(
            request.feature == nullptr ? "localization.unknown" : request.feature,
            kDiagnosticTargetId, patch::PatchOperation::ValidateDescription,
            error.what());
        std::cerr << "eu4dll_mac [Error] " << error.what() << std::endl;
        return false;
    }
    const auto result = platform::macos::LivePatchRuntime().Install(description);
    // Continuations are calculated before mutation. If optional hook
    // optimization fails after the jump was written, retain safe return
    // addresses while still reporting the installation as failed.
    for (const auto &continuation : request.continuations) {
        if (continuation.storage != nullptr) {
            *continuation.storage = result.ContinuationAddress(continuation.name);
        }
    }
    if (!result) {
        diagnostics::StartupDiagnostics::Instance().Record(result.diagnostic);
        std::cerr << "eu4dll_mac [Error] "
                  << patch::FormatDiagnostic(result.diagnostic) << std::endl;
        return false;
    }
    std::cout << "eu4dll_mac [Success] " << request.feature
              << " overwrite=" << request.overwrittenLength << std::endl;
    return true;
}

} // namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::localization_features
