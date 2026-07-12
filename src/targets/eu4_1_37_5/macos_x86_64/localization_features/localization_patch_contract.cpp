#include "localization_patch.h"

#include <stdexcept>

namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::localization_features {

patch::PatchDescription BuildDescription(const InstallRequest &request) {
    if (request.feature == nullptr || request.site == nullptr ||
        request.expectedBytes.empty() || request.overwrittenLength == 0) {
        throw std::invalid_argument("incomplete localization patch contract");
    }
    const std::size_t mutationLength = request.mutationKind == patch::MutationKind::RawBytes
        ? request.mutationBytes.size() : 5;
    if (mutationLength != request.overwrittenLength) {
        throw std::invalid_argument("mutation and overwritten lengths differ");
    }
    if (request.expectedOffset != request.site->mutationOffset ||
        request.expectedBytes.size() != request.overwrittenLength) {
        throw std::invalid_argument(
            "expected bytes must cover the complete overwritten range");
    }
    if (!request.expectedMask.empty() &&
        request.expectedMask.size() != request.expectedBytes.size()) {
        throw std::invalid_argument("expected-byte mask must cover overwritten range");
    }

    patch::PatchDescription description;
    description.feature = request.feature;
    description.target = kDiagnosticTargetId;
    description.location.pattern = request.site->pattern;
    description.location.referencedStrings = request.referencedStrings;
    description.location.requireUnique = true;
    if (!request.symbol.empty()) {
        description.location.scope = patch::SearchScope::Symbol(
            request.symbol, request.symbolSearchSize);
    }
    description.expected = patch::ExpectedBytes{
        request.expectedOffset, request.expectedBytes, request.expectedMask};
    description.mutation.kind = request.mutationKind;
    description.mutation.offset = request.site->mutationOffset;
    description.mutation.target = request.mutationTarget;
    description.mutation.callWidth = request.callWidth;
    description.mutation.bytes = request.mutationBytes;
    for (const auto &continuation : request.continuations) {
        description.continuations.push_back({continuation.name, continuation.offset});
    }
    if (request.optimizeNakedHook) {
        description.optimization.enabled = true;
        description.optimization.hookAddress = request.mutationTarget;
    }
    return description;
}

} // namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::localization_features
