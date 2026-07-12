#include "targets/eu4_1_37_5/macos_x86_64/text_rendering/rendering_contract.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <set>
#include <string>

namespace target =
    eu4dll::targets::eu4_1_37_5::macos_x86_64::text_rendering;

namespace {

void require(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << message << std::endl;
        std::exit(1);
    }
}

} // namespace

int main() {
    const auto &facts = target::descriptors();
    require(facts.size() == 28, "rendering inventory must contain all 28 mutation sites");

    std::set<std::string> factIds;
    std::set<std::string> features;
    std::size_t jumps = 0;
    std::size_t calls = 0;
    std::size_t rawMutations = 0;
    std::size_t optimized = 0;

    for (std::size_t index = 0; index < facts.size(); ++index) {
        const auto &fact = facts[index];
        require(static_cast<std::size_t>(fact.id) == index,
                "PatchId order must match the canonical inventory");
        require(fact.factId != nullptr && *fact.factId != '\0',
                "each rendering site needs a target fact ID");
        require(fact.feature != nullptr && *fact.feature != '\0',
                "each rendering site needs a feature ID");
        require(fact.site != nullptr && fact.site->pattern != nullptr &&
                        *fact.site->pattern != '\0',
                "each rendering site needs a target pattern");
        require(factIds.insert(fact.factId).second,
                std::string("duplicate rendering fact ID: ") + fact.factId);
        require(features.insert(fact.feature).second,
                std::string("duplicate rendering feature ID: ") + fact.feature);
        require(fact.mutationWidth > 0, "mutation width must be explicit");
        require(!fact.expectedOriginalBytes.empty(),
                "stable expected original bytes must be recorded");
        require(fact.expectedOriginalBytes.size() == fact.mutationWidth,
                "expected bytes must cover the complete overwrite width");
        require(fact.expectedMask.size() == fact.mutationWidth,
                "expected-byte mask must cover the complete overwrite width");

        if (fact.mutationKind == eu4dll::patch::MutationKind::Jump) {
            ++jumps;
            require(fact.mutationWidth == 5,
                    "x86-64 rendering JMP mutations are five bytes");
            require(fact.callWidth == eu4dll::patch::CallWidth::FiveBytes,
                    "JMP descriptor width contract changed");
            require(fact.mutationBytes.empty(),
                    "JMP descriptor must not carry raw mutation bytes");
            require(fact.optimizeNakedHook,
                    "naked rendering JMP hooks must retain optimizer ordering");
        } else if (fact.mutationKind == eu4dll::patch::MutationKind::Call) {
            ++calls;
            require(fact.mutationWidth == 5 &&
                            fact.callWidth == eu4dll::patch::CallWidth::FiveBytes,
                    "direct rendering CALL mutations must record five-byte width");
            require(fact.expectedOriginalBytes ==
                            std::vector<std::uint8_t>({0xE8, 0, 0, 0, 0}) &&
                        fact.expectedMask ==
                            std::vector<std::uint8_t>({0xFF, 0, 0, 0, 0}),
                    "direct CALL sites must validate a full masked rel32 span");
        } else {
            ++rawMutations;
            require(fact.mutationWidth == fact.mutationBytes.size(),
                    "raw mutation width must match its payload");
            require(!fact.optimizeNakedHook,
                    "raw rendering mutations do not run naked-hook optimization");
        }
        if (fact.optimizeNakedHook) {
            ++optimized;
        }

        std::set<std::string> continuationNames;
        for (const auto &continuation : fact.continuations) {
            require(continuation.name != nullptr &&
                            (std::strcmp(continuation.name, "return") == 0 ||
                             std::strcmp(continuation.name, "bypass") == 0),
                    "continuation must explicitly identify return or bypass");
            require(continuation.offset != 0,
                    "rendering continuation offsets must be fixed target facts");
            require(continuationNames.insert(continuation.name).second,
                    "duplicate continuation name in rendering descriptor");
        }

        const auto patch = target::make_patch_description(
            fact.id, fact.mutationKind == eu4dll::patch::MutationKind::RawBytes
                         ? 0
                         : 0x12345678);
        require(patch.feature == fact.feature &&
                        patch.target == eu4dll::targets::eu4_1_37_5::macos_x86_64::
                                            kDiagnosticTargetId,
                "runtime patch identity must come from the canonical descriptor");
        require(patch.location.pattern == fact.site->pattern &&
                        patch.location.requireUnique,
                "runtime location must use the canonical target pattern contract");
        require(patch.expected &&
                        patch.expected->offset == fact.expectedOffset &&
                        patch.expected->bytes == fact.expectedOriginalBytes &&
                        patch.expected->mask == fact.expectedMask,
                "runtime expected bytes must use the canonical descriptor");
        require(patch.mutation.kind == fact.mutationKind &&
                        patch.mutation.offset == fact.site->mutationOffset &&
                        patch.mutation.callWidth == fact.callWidth &&
                        patch.mutation.bytes == fact.mutationBytes,
                "runtime mutation must use the canonical descriptor");
        require(patch.continuations.size() == fact.continuations.size(),
                "runtime continuation bindings must use the canonical descriptor");
        require(patch.optimization.enabled == fact.optimizeNakedHook,
                "runtime optimizer flag must use the canonical descriptor");
    }

    require(jumps == 23 && calls == 3 && rawMutations == 2,
            "rendering mutation kind inventory changed");
    require(optimized == 24,
            "23 naked JMP hooks and the naked uppercase CALL proxy must be optimized");
    require(target::descriptor(target::PatchId::MapCurveLogicalSizeFirst).site->pattern ==
                    target::descriptor(target::PatchId::MapCurveLogicalSizeSecond).site->pattern,
            "CurveText logical-size CALLs must share the same fixed match pattern");
    require(target::descriptor(target::PatchId::MapCurveLogicalSizeFirst)
                        .site->mutationOffset !=
                    target::descriptor(target::PatchId::MapCurveLogicalSizeSecond)
                        .site->mutationOffset,
            "CurveText logical-size CALLs must remain two distinct mutation sites");
    return 0;
}
