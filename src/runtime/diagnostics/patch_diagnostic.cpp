#include "patch_diagnostic.h"

#include <iomanip>
#include <sstream>

namespace eu4dll::patch {

const char *ToString(PatchOperation operation) {
    switch (operation) {
        case PatchOperation::None:
            return "none";
        case PatchOperation::ValidateDescription:
            return "validate-description";
        case PatchOperation::ResolveSearchScope:
            return "resolve-search-scope";
        case PatchOperation::ResolveSymbol:
            return "resolve-symbol";
        case PatchOperation::ParsePattern:
            return "parse-pattern";
        case PatchOperation::LocatePattern:
            return "locate-pattern";
        case PatchOperation::VerifyOriginalBytes:
            return "verify-original-bytes";
        case PatchOperation::CalculateMutation:
            return "calculate-mutation";
        case PatchOperation::WriteMutation:
            return "write-mutation";
        case PatchOperation::OptimizeHook:
            return "optimize-hook";
        case PatchOperation::InstallFeature:
            return "install-feature";
    }
    return "unknown";
}

const char *ToString(MatchStatus status) {
    switch (status) {
        case MatchStatus::NotSearched:
            return "not-searched";
        case MatchStatus::Unique:
            return "unique";
        case MatchStatus::NotFound:
            return "not-found";
        case MatchStatus::Ambiguous:
            return "ambiguous";
    }
    return "unknown";
}

std::string FormatDiagnostic(const PatchDiagnostic &diagnostic) {
    std::ostringstream stream;
    stream << "feature=" << diagnostic.feature
           << " target=" << diagnostic.target
           << " operation=" << ToString(diagnostic.operation)
           << " match=" << ToString(diagnostic.match)
           << " match_count=" << diagnostic.matchCount;
    if (diagnostic.matchAddress != 0) {
        stream << " match_address=0x" << std::hex << diagnostic.matchAddress << std::dec;
    }
    if (diagnostic.mutationAddress != 0) {
        stream << " mutation_address=0x" << std::hex << diagnostic.mutationAddress << std::dec;
    }
    if (!diagnostic.message.empty()) {
        stream << " message=" << diagnostic.message;
    }
    return stream.str();
}

} // namespace eu4dll::patch
