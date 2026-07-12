#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace eu4dll::patch {

enum class PatchOperation {
    None,
    ValidateDescription,
    ResolveSearchScope,
    ResolveSymbol,
    ParsePattern,
    LocatePattern,
    VerifyOriginalBytes,
    CalculateMutation,
    WriteMutation,
    OptimizeHook,
    InstallFeature,
};

enum class MatchStatus {
    NotSearched,
    Unique,
    NotFound,
    Ambiguous,
};

struct PatchDiagnostic {
    bool success = false;
    std::string feature;
    std::string target;
    PatchOperation operation = PatchOperation::None;
    MatchStatus match = MatchStatus::NotSearched;
    std::size_t matchCount = 0;
    std::uint64_t matchAddress = 0;
    std::uint64_t mutationAddress = 0;
    std::string message;
};

const char *ToString(PatchOperation operation);
const char *ToString(MatchStatus status);
std::string FormatDiagnostic(const PatchDiagnostic &diagnostic);

} // namespace eu4dll::patch
