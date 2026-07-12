#include "targets/eu4_1_37_5/macos_x86_64/text_rendering/rendering_patch.h"

#include "platform/macos/live_patch_runtime.h"
#include "runtime/diagnostics/patch_diagnostic.h"
#include "runtime/diagnostics/startup_diagnostics.h"

#include <cstring>
#include <iostream>

namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::text_rendering {

bool install(const InstallRequest &request) {
    const auto &fact = descriptor(request.id);
    if (fact.continuations.size() != request.continuations.size()) {
        diagnostics::StartupDiagnostics::Instance().RecordFailure(
            fact.feature, kDiagnosticTargetId, patch::PatchOperation::ValidateDescription,
            "continuation binding count does not match target contract");
        std::cerr << "eu4dll_mac [Error] " << fact.feature
                  << " continuation binding count does not match target contract"
                  << std::endl;
        return false;
    }
    for (const auto &continuation : fact.continuations) {
        bool found = false;
        for (const auto &binding : request.continuations) {
            if (binding.name != nullptr &&
                std::strcmp(binding.name, continuation.name) == 0 &&
                binding.storage != nullptr) {
                found = true;
                break;
            }
        }
        if (!found) {
            diagnostics::StartupDiagnostics::Instance().RecordFailure(
                fact.feature, kDiagnosticTargetId,
                patch::PatchOperation::ValidateDescription,
                std::string("missing continuation binding: ") + continuation.name);
            std::cerr << "eu4dll_mac [Error] " << fact.feature
                      << " missing continuation binding: " << continuation.name
                      << std::endl;
            return false;
        }
    }
    auto description = make_patch_description(request.id, request.mutationTarget);

    const auto result = platform::macos::LivePatchRuntime().Install(description);
    // Install calculates continuation addresses before mutation. Preserve them
    // when only the optional naked-hook optimization reports a later failure.
    for (const auto &binding : request.continuations) {
        if (binding.storage != nullptr) {
            *binding.storage = result.ContinuationAddress(binding.name);
        }
    }
    if (!result) {
        diagnostics::StartupDiagnostics::Instance().Record(result.diagnostic);
        std::cerr << "eu4dll_mac [Error] "
                  << patch::FormatDiagnostic(result.diagnostic) << std::endl;
        return false;
    }

    std::cout << "eu4dll_mac [Success] " << fact.feature
              << " match=0x" << std::hex << result.diagnostic.matchAddress
              << " mutation=0x" << result.diagnostic.mutationAddress
              << std::dec << std::endl;
    return true;
}

} // namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::text_rendering
