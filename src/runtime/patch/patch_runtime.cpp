#include "patch_runtime.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>

namespace eu4dll::patch {
namespace {

constexpr int kWildcard = -1;
constexpr int kStringDisplacement = -2;

struct ParsedPattern {
    std::vector<int> bytes;
    std::vector<std::size_t> stringDisplacements;
};

PatchDiagnostic Failure(const std::string &feature, const std::string &target,
                        PatchOperation operation, std::string message) {
    PatchDiagnostic diagnostic;
    diagnostic.feature = feature;
    diagnostic.target = target;
    diagnostic.operation = operation;
    diagnostic.message = std::move(message);
    return diagnostic;
}

std::optional<Address> AddOffset(Address address, std::ptrdiff_t offset) {
    if (offset >= 0) {
        const auto positive = static_cast<Address>(offset);
        if (positive > std::numeric_limits<Address>::max() - address) {
            return std::nullopt;
        }
        return address + positive;
    }
    const auto magnitude = static_cast<Address>(-(offset + 1)) + 1;
    if (magnitude > address) {
        return std::nullopt;
    }
    return address - magnitude;
}

std::optional<Address> AddRelative(Address address, std::int64_t offset) {
    if (offset >= 0) {
        const auto positive = static_cast<Address>(offset);
        if (positive > std::numeric_limits<Address>::max() - address) {
            return std::nullopt;
        }
        return address + positive;
    }
    const auto magnitude = static_cast<Address>(-(offset + 1)) + 1;
    if (magnitude > address) {
        return std::nullopt;
    }
    return address - magnitude;
}

bool ParsePatternText(const std::string &text, ParsedPattern &parsed, std::string &error) {
    std::istringstream stream(text);
    std::string token;
    while (stream >> token) {
        if (token == "?" || token == "??") {
            parsed.bytes.push_back(kWildcard);
            continue;
        }
        if (token == "S") {
            parsed.bytes.push_back(kStringDisplacement);
            continue;
        }

        std::size_t parsedCharacters = 0;
        unsigned long value = 0;
        try {
            value = std::stoul(token, &parsedCharacters, 16);
        } catch (...) {
            error = "invalid pattern token: " + token;
            return false;
        }
        if (parsedCharacters != token.size() || value > 0xFF) {
            error = "invalid byte token: " + token;
            return false;
        }
        parsed.bytes.push_back(static_cast<int>(value));
    }

    if (parsed.bytes.empty()) {
        error = "pattern must contain at least one byte";
        return false;
    }

    for (std::size_t index = 0; index < parsed.bytes.size();) {
        if (parsed.bytes[index] != kStringDisplacement) {
            ++index;
            continue;
        }
        const std::size_t start = index;
        while (index < parsed.bytes.size() && parsed.bytes[index] == kStringDisplacement) {
            ++index;
        }
        if (index - start != sizeof(std::int32_t)) {
            error = "each S placeholder group must contain exactly four bytes";
            return false;
        }
        parsed.stringDisplacements.push_back(start);
    }
    return true;
}

bool FitsRel32(Address instruction, Address target, std::int32_t &relative) {
    if (instruction > std::numeric_limits<Address>::max() - 5) {
        return false;
    }
    const Address nextInstruction = instruction + 5;
    if (target >= nextInstruction) {
        const Address distance = target - nextInstruction;
        if (distance > static_cast<Address>(std::numeric_limits<std::int32_t>::max())) {
            return false;
        }
        relative = static_cast<std::int32_t>(distance);
        return true;
    }

    const Address distance = nextInstruction - target;
    const Address negativeLimit =
        static_cast<Address>(std::numeric_limits<std::int32_t>::max()) + 1;
    if (distance > negativeLimit) {
        return false;
    }
    relative = distance == negativeLimit
                   ? std::numeric_limits<std::int32_t>::min()
                   : -static_cast<std::int32_t>(distance);
    return true;
}

std::vector<std::uint8_t> RelativeMutation(std::uint8_t opcode, std::int32_t relative,
                                           std::size_t width) {
    std::vector<std::uint8_t> bytes(width, 0x90);
    bytes[0] = opcode;
    std::memcpy(bytes.data() + 1, &relative, sizeof(relative));
    return bytes;
}

} // namespace

SearchScope SearchScope::MainModule() {
    return SearchScope{};
}

SearchScope SearchScope::Symbol(std::string name, std::size_t maxSearchSize) {
    SearchScope scope;
    scope.kind = SearchScopeKind::Symbol;
    scope.symbol = std::move(name);
    scope.maxSize = maxSearchSize;
    return scope;
}

Address InstallationResult::ContinuationAddress(const std::string &name) const {
    const auto found = continuations.find(name);
    return found == continuations.end() ? 0 : found->second;
}

LocateResult PatchRuntime::Locate(const PatternLocation &location, const std::string &feature,
                                  const std::string &target) const {
    LocateResult result;
    result.diagnostic = Failure(feature, target, PatchOperation::ParsePattern, {});

    ParsedPattern pattern;
    std::string error;
    if (!ParsePatternText(location.pattern, pattern, error)) {
        result.diagnostic.message = error;
        return result;
    }
    if (pattern.stringDisplacements.size() != location.referencedStrings.size()) {
        std::ostringstream stream;
        stream << "pattern contains " << pattern.stringDisplacements.size()
               << " string displacement groups but " << location.referencedStrings.size()
               << " reference strings were supplied";
        result.diagnostic.message = stream.str();
        return result;
    }

    // Resolves the candidate search regions. Main-module scans aggregate every
    // executable region so ELF images with unmapped gaps never read fabricated
    // bytes; symbol scans stay bounded to the resolved symbol window.
    std::vector<MemoryRegion> regions;
    if (location.scope.kind == SearchScopeKind::MainModule) {
        regions = memory_.MainModuleRegions(RegionPurpose::ExecutableSearch, error);
        if (!error.empty()) {
            result.diagnostic.operation = PatchOperation::ResolveSearchScope;
            result.diagnostic.message = error;
            return result;
        }
        if (regions.empty()) {
            result.diagnostic.operation = PatchOperation::ResolveSearchScope;
            result.diagnostic.message = "no executable search regions were reported";
            return result;
        }
    } else {
        if (location.scope.symbol.empty() || location.scope.maxSize == 0) {
            result.diagnostic.operation = PatchOperation::ValidateDescription;
            result.diagnostic.message = "symbol search requires a symbol name and non-zero maximum size";
            return result;
        }
        const auto symbol = memory_.ResolveSymbol(location.scope.symbol, error);
        if (!symbol) {
            result.diagnostic.operation = PatchOperation::ResolveSearchScope;
            result.diagnostic.message = error;
            return result;
        }
        regions.push_back(MemoryRegion{*symbol, location.scope.maxSize, location.scope.symbol});
    }

    for (const auto &region : regions) {
        if (region.size != 0) {
            const auto lastOffset = static_cast<std::uintmax_t>(region.size - 1);
            const auto available = static_cast<std::uintmax_t>(
                std::numeric_limits<Address>::max() - region.address);
            if (lastOffset > available) {
                result.diagnostic.operation = PatchOperation::ResolveSearchScope;
                result.diagnostic.message = "search region overflows the address space";
                return result;
            }
        }
    }

    result.diagnostic.operation = PatchOperation::LocatePattern;

    const auto scanRegion = [&](const MemoryRegion &region,
                                std::vector<Address> &matches) -> bool {
        if (region.size < pattern.bytes.size()) {
            return true;  // Too small: contributes zero matches, not a failure.
        }
        std::vector<std::uint8_t> haystack(region.size);
        if (!memory_.Read(region.address, haystack.data(), haystack.size(), error)) {
            result.diagnostic.message = error;
            return false;
        }
        const std::size_t lastStart = haystack.size() - pattern.bytes.size();
        for (std::size_t start = 0; start <= lastStart; ++start) {
            bool matched = true;
            for (std::size_t index = 0; index < pattern.bytes.size(); ++index) {
                const int expected = pattern.bytes[index];
                if (expected >= 0 && haystack[start + index] != expected) {
                    matched = false;
                    break;
                }
            }
            if (!matched) {
                continue;
            }
            for (std::size_t index = 0; index < pattern.stringDisplacements.size(); ++index) {
                const std::size_t displacementIndex = pattern.stringDisplacements[index];
                std::int32_t displacement = 0;
                std::memcpy(&displacement, haystack.data() + start + displacementIndex,
                            sizeof(displacement));
                const auto rip = AddOffset(region.address, static_cast<std::ptrdiff_t>(start + displacementIndex + 4));
                const auto stringAddress = rip ? AddRelative(*rip, displacement) : std::nullopt;
                if (!stringAddress) {
                    matched = false;
                    break;
                }
                std::string actual;
                if (!memory_.ReadCString(*stringAddress,
                                         location.referencedStrings[index].size() + 1,
                                         actual, error) || actual != location.referencedStrings[index]) {
                    matched = false;
                    break;
                }
            }
            if (!matched) {
                continue;
            }
            matches.push_back(region.address + start);
            if (!location.requireUnique) {
                return true;
            }
        }
        return true;
    };

    std::vector<Address> matches;
    for (const auto &region : regions) {
        if (!scanRegion(region, matches)) {
            return result;
        }
        if (!location.requireUnique && !matches.empty()) {
            break;
        }
    }

    result.diagnostic.matchCount = matches.size();
    if (matches.empty()) {
        result.diagnostic.match = MatchStatus::NotFound;
        result.diagnostic.message = "pattern was not found";
        return result;
    }
    if (location.requireUnique && matches.size() != 1) {
        result.diagnostic.match = MatchStatus::Ambiguous;
        result.diagnostic.matchAddress = matches.front();
        result.diagnostic.message = "pattern must match exactly once";
        return result;
    }

    result.address = matches.front();
    result.diagnostic.success = true;
    result.diagnostic.match = MatchStatus::Unique;
    result.diagnostic.matchAddress = result.address;
    result.diagnostic.message = "pattern located";
    return result;
}

PatchDiagnostic PatchRuntime::ApplyMutation(Address address, const Mutation &mutation,
                                            const std::string &feature,
                                            const std::string &target) {
    PatchDiagnostic diagnostic = Failure(feature, target, PatchOperation::CalculateMutation, {});
    diagnostic.matchAddress = address;
    const auto mutationAddress = AddOffset(address, mutation.offset);
    if (!mutationAddress) {
        diagnostic.message = "mutation offset overflows the address space";
        return diagnostic;
    }
    diagnostic.mutationAddress = *mutationAddress;

    std::vector<std::uint8_t> payload;
    if (mutation.kind == MutationKind::RawBytes) {
        if (mutation.bytes.empty()) {
            diagnostic.message = "raw mutation requires at least one byte";
            return diagnostic;
        }
        payload = mutation.bytes;
    } else {
        if (mutation.target == 0) {
            diagnostic.message = "control-flow mutation requires a non-zero target";
            return diagnostic;
        }
        std::int32_t relative = 0;
        if (!FitsRel32(*mutationAddress, mutation.target, relative)) {
            diagnostic.message = "control-flow target is outside the signed rel32 range";
            return diagnostic;
        }

        if (mutation.kind == MutationKind::Jump) {
            payload = RelativeMutation(0xE9, relative, 5);
        } else {
            std::size_t width = 5;
            if (mutation.callWidth == CallWidth::SixBytes) {
                width = 6;
            } else if (mutation.callWidth == CallWidth::Auto) {
                std::uint8_t original[2] = {};
                std::string error;
                if (!memory_.Read(*mutationAddress, original, sizeof(original), error)) {
                    diagnostic.message = "could not inspect original CALL encoding: " + error;
                    return diagnostic;
                }
                width = original[0] == 0xFF ? 6 : 5;
            }
            payload = RelativeMutation(0xE8, relative, width);
        }
    }

    std::string error;
    diagnostic.operation = PatchOperation::WriteMutation;
    if (!memory_.Write(*mutationAddress, payload.data(), payload.size(), error)) {
        diagnostic.message = error;
        return diagnostic;
    }
    diagnostic.success = true;
    diagnostic.message = "mutation applied";
    return diagnostic;
}

PatchDiagnostic PatchRuntime::OptimizeIndirectBranches(Address hookAddress,
                                                        std::size_t maxScanSize,
                                                        const std::string &feature,
                                                        const std::string &target) {
    PatchDiagnostic diagnostic = Failure(feature, target, PatchOperation::OptimizeHook, {});
    diagnostic.mutationAddress = hookAddress;
    if (hookAddress == 0 || maxScanSize < 6) {
        diagnostic.message = "hook optimization requires an address and at least six bytes";
        return diagnostic;
    }

    std::vector<std::uint8_t> code(maxScanSize);
    std::string error;
    if (!memory_.Read(hookAddress, code.data(), code.size(), error)) {
        diagnostic.message = error;
        return diagnostic;
    }

    std::size_t optimized = 0;
    for (std::size_t index = 0; index + 6 <= code.size();) {
        const bool indirectJump = code[index] == 0xFF && code[index + 1] == 0x25;
        const bool indirectCall = code[index] == 0xFF && code[index + 1] == 0x15;
        if (!indirectJump && !indirectCall) {
            ++index;
            continue;
        }

        std::int32_t variableDisplacement = 0;
        std::memcpy(&variableDisplacement, code.data() + index + 2,
                    sizeof(variableDisplacement));
        const Address instruction = hookAddress + index;
        const auto variableBase = AddOffset(instruction, 6);
        const auto variableAddress = variableBase
                                         ? AddRelative(*variableBase, variableDisplacement)
                                         : std::nullopt;
        Address branchTarget = 0;
        if (!variableAddress ||
            !memory_.Read(*variableAddress,
                          reinterpret_cast<std::uint8_t *>(&branchTarget),
                          sizeof(branchTarget), error)) {
            diagnostic.message = "could not resolve indirect branch target: " + error;
            return diagnostic;
        }

        std::int32_t relative = 0;
        if (FitsRel32(instruction, branchTarget, relative)) {
            const auto payload = RelativeMutation(indirectCall ? 0xE8 : 0xE9, relative, 6);
            if (!memory_.Write(instruction, payload.data(), payload.size(), error)) {
                diagnostic.message = error;
                return diagnostic;
            }
            std::copy(payload.begin(), payload.end(), code.begin() + index);
            ++optimized;
        }

        index += 6;
        if (indirectJump && index + 1 < code.size() &&
            code[index] == 0x0F && code[index + 1] == 0x0B) {
            break;
        }
    }

    diagnostic.success = true;
    diagnostic.message = "optimized " + std::to_string(optimized) + " indirect branches";
    return diagnostic;
}

InstallationResult PatchRuntime::Preflight(const PatchDescription &description) const {
    InstallationResult result;
    if (description.feature.empty() || description.target.empty()) {
        result.diagnostic = Failure(description.feature, description.target,
                                    PatchOperation::ValidateDescription,
                                    "patch description requires feature and target identifiers");
        return result;
    }

    LocateResult located;
    if (siteProvider_ != nullptr) {
        std::string error;
        const auto address = siteProvider_->Resolve(description.feature, error);
        located.diagnostic = Failure(description.feature, description.target,
                                     PatchOperation::LocatePattern, error);
        if (address) {
            located.address = *address;
            located.diagnostic.success = true;
            located.diagnostic.match = MatchStatus::Unique;
            located.diagnostic.matchCount = 1;
            located.diagnostic.matchAddress = *address;
            located.diagnostic.message = "patch site resolved from install-time manifest";
        }
    } else {
        located = Locate(description.location, description.feature, description.target);
    }
    if (!located) {
        result.diagnostic = located.diagnostic;
        return result;
    }

    if (description.expected && !description.expected->bytes.empty()) {
        if (!description.expected->mask.empty() &&
            description.expected->mask.size() != description.expected->bytes.size()) {
            result.diagnostic = Failure(description.feature, description.target,
                                        PatchOperation::ValidateDescription,
                                        "expected-byte mask must cover the complete span");
            return result;
        }
        const auto expectedAddress = AddOffset(located.address, description.expected->offset);
        if (!expectedAddress) {
            result.diagnostic = Failure(description.feature, description.target,
                                        PatchOperation::VerifyOriginalBytes,
                                        "expected-byte offset overflows the address space");
            result.diagnostic.match = located.diagnostic.match;
            result.diagnostic.matchCount = located.diagnostic.matchCount;
            result.diagnostic.matchAddress = located.address;
            return result;
        }
        std::vector<std::uint8_t> actual(description.expected->bytes.size());
        std::string error;
        if (!memory_.Read(*expectedAddress, actual.data(), actual.size(), error)) {
            result.diagnostic = Failure(description.feature, description.target,
                                        PatchOperation::VerifyOriginalBytes, error);
            result.diagnostic.match = located.diagnostic.match;
            result.diagnostic.matchCount = located.diagnostic.matchCount;
            result.diagnostic.matchAddress = located.address;
            return result;
        }
        bool matches = true;
        for (std::size_t index = 0; index < actual.size(); ++index) {
            const auto mask = description.expected->mask.empty()
                                  ? std::uint8_t{0xFF}
                                  : description.expected->mask[index];
            if ((actual[index] & mask) !=
                (description.expected->bytes[index] & mask)) {
                matches = false;
                break;
            }
        }
        if (!matches) {
            result.diagnostic = Failure(description.feature, description.target,
                                        PatchOperation::VerifyOriginalBytes,
                                        "original bytes do not match the target profile");
            result.diagnostic.match = located.diagnostic.match;
            result.diagnostic.matchCount = located.diagnostic.matchCount;
            result.diagnostic.matchAddress = located.address;
            return result;
        }
    }

    for (const auto &continuation : description.continuations) {
        const auto continuationAddress = AddOffset(located.address, continuation.offset);
        if (continuation.name.empty() || !continuationAddress) {
            result.diagnostic = Failure(description.feature, description.target,
                                        PatchOperation::CalculateMutation,
                                        "continuation requires a name and valid address offset");
            result.diagnostic.match = located.diagnostic.match;
            result.diagnostic.matchCount = located.diagnostic.matchCount;
            result.diagnostic.matchAddress = located.address;
            return result;
        }
        result.continuations[continuation.name] = *continuationAddress;
    }

    result.diagnostic = located.diagnostic;
    result.diagnostic.success = true;
    result.diagnostic.operation = PatchOperation::VerifyOriginalBytes;
    result.diagnostic.message = "pattern, original bytes, and continuations verified";
    return result;
}

InstallationResult PatchRuntime::Install(const PatchDescription &description) {
    auto result = Preflight(description);
    if (!result) return result;
    const auto matchAddress = result.diagnostic.matchAddress;

    result.diagnostic = ApplyMutation(matchAddress, description.mutation,
                                      description.feature, description.target);
    result.diagnostic.match = MatchStatus::Unique;
    result.diagnostic.matchCount = 1;
    result.diagnostic.matchAddress = matchAddress;
    if (!result.diagnostic.success) {
        return result;
    }

    if (description.optimization.enabled) {
        const Address hookAddress = description.optimization.hookAddress != 0
                                        ? description.optimization.hookAddress
                                        : description.mutation.target;
        auto optimized = OptimizeIndirectBranches(hookAddress,
                                                  description.optimization.maxScanSize,
                                                  description.feature, description.target);
        optimized.match = MatchStatus::Unique;
        optimized.matchCount = 1;
        optimized.matchAddress = matchAddress;
        optimized.mutationAddress = result.diagnostic.mutationAddress;
        if (!optimized.success) {
            result.diagnostic = std::move(optimized);
            return result;
        }
        result.diagnostic.message += "; " + optimized.message;
    }

    return result;
}

} // namespace eu4dll::patch
