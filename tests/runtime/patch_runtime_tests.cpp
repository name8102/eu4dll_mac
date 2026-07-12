#include "runtime/diagnostics/patch_diagnostic.h"
#include "runtime/patch/byte_buffer_memory.h"
#include "runtime/patch/patch_runtime.h"

#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using eu4dll::patch::Address;
using eu4dll::patch::ByteBufferMemory;
using eu4dll::patch::CallWidth;
using eu4dll::patch::ExpectedBytes;
using eu4dll::patch::MatchStatus;
using eu4dll::patch::MutationKind;
using eu4dll::patch::PatchDescription;
using eu4dll::patch::PatchOperation;
using eu4dll::patch::PatchRuntime;

void Require(bool condition, const std::string &message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template<typename Value>
void Store(std::vector<std::uint8_t> &bytes, std::size_t offset, Value value) {
    Require(offset + sizeof(value) <= bytes.size(), "test fixture store is out of range");
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

std::int32_t LoadRel32(const std::vector<std::uint8_t> &bytes, std::size_t offset) {
    std::int32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

PatchDescription RawPatch(std::string pattern) {
    PatchDescription description;
    description.feature = "test-feature";
    description.target = "test-target";
    description.location.pattern = std::move(pattern);
    description.mutation.kind = MutationKind::RawBytes;
    description.mutation.bytes = {0xCC};
    return description;
}

void TestUniqueRawMutationAndContinuation() {
    ByteBufferMemory memory({0x90, 0xAA, 0xBB, 0xCC, 0xDD, 0x90}, 0x4000);
    PatchRuntime runtime(memory);
    auto description = RawPatch("AA BB CC DD");
    description.expected = ExpectedBytes{1, {0xBB, 0xCC}, {}};
    description.mutation.offset = 1;
    description.mutation.bytes = {0x11, 0x22};
    description.continuations = {{"resume", 4}};

    const auto result = runtime.Install(description);
    Require(static_cast<bool>(result), eu4dll::patch::FormatDiagnostic(result.diagnostic));
    Require(result.diagnostic.match == MatchStatus::Unique, "match must be unique");
    Require(result.diagnostic.matchAddress == 0x4001, "match address must be reported");
    Require(result.diagnostic.mutationAddress == 0x4002, "mutation address must be reported");
    Require(result.ContinuationAddress("resume") == 0x4005,
            "continuation must be calculated from the match");
    Require(memory.Bytes()[2] == 0x11 && memory.Bytes()[3] == 0x22,
            "raw mutation bytes were not written");
}

void TestUniquenessFailureIsStructured() {
    ByteBufferMemory memory({0xAA, 0xBB, 0x90, 0xAA, 0xBB}, 0x5000);
    PatchRuntime runtime(memory);
    const auto result = runtime.Install(RawPatch("AA BB"));

    Require(!result, "ambiguous pattern must fail");
    Require(result.diagnostic.feature == "test-feature", "diagnostic must identify feature");
    Require(result.diagnostic.target == "test-target", "diagnostic must identify target");
    Require(result.diagnostic.operation == PatchOperation::LocatePattern,
            "diagnostic must identify failed locate operation");
    Require(result.diagnostic.match == MatchStatus::Ambiguous,
            "diagnostic must identify ambiguous match");
    Require(result.diagnostic.matchCount == 2, "diagnostic must report match count");
}

void TestPatternFailuresAreStructured() {
    ByteBufferMemory memory({0xAA, 0xBB, 0xCC}, 0x5800);
    PatchRuntime runtime(memory);

    const auto invalid = runtime.Install(RawPatch("AA nope"));
    Require(!invalid, "invalid pattern text must fail");
    Require(invalid.diagnostic.operation == PatchOperation::ParsePattern,
            "invalid pattern must identify parsing as the failed operation");
    Require(invalid.diagnostic.feature == "test-feature" &&
                invalid.diagnostic.target == "test-target",
            "parse failure must retain feature and target identifiers");

    const auto missing = runtime.Install(RawPatch("11 22"));
    Require(!missing, "missing pattern must fail");
    Require(missing.diagnostic.operation == PatchOperation::LocatePattern,
            "missing pattern must identify location as the failed operation");
    Require(missing.diagnostic.match == MatchStatus::NotFound,
            "missing pattern must report not-found match status");
}

void TestExpectedOriginalBytesFailure() {
    ByteBufferMemory memory({0xAA, 0xBB, 0xCC}, 0x6000);
    PatchRuntime runtime(memory);
    auto description = RawPatch("AA BB CC");
    description.expected = ExpectedBytes{1, {0x00}, {}};

    const auto result = runtime.Install(description);
    Require(!result, "wrong original bytes must fail");
    Require(result.diagnostic.operation == PatchOperation::VerifyOriginalBytes,
            "diagnostic must identify original-byte verification");
    Require(memory.Bytes()[0] == 0xAA, "failed verification must not mutate memory");
}

void TestMaskedExpectedBytesCoverFullRelativeCall() {
    ByteBufferMemory memory({0x90, 0xE8, 0x12, 0x34, 0x56, 0x78, 0x90}, 0x6800);
    PatchRuntime runtime(memory);
    PatchDescription description;
    description.feature = "masked-call";
    description.target = "test-target";
    description.location.pattern = "90 E8 ? ? ? ? 90";
    description.expected = ExpectedBytes{
        1, {0xE8, 0, 0, 0, 0}, {0xFF, 0, 0, 0, 0}};
    description.mutation.kind = MutationKind::Call;
    description.mutation.offset = 1;
    description.mutation.target = 0x6900;
    description.mutation.callWidth = CallWidth::FiveBytes;

    const auto before = memory.Bytes();
    const auto preflight = runtime.Preflight(description);
    Require(static_cast<bool>(preflight),
            "masked relative CALL preflight must accept any rel32 operand");
    Require(memory.Bytes() == before, "preflight must not mutate memory");

    auto wrongOpcode = description;
    wrongOpcode.expected->bytes[0] = 0xE9;
    Require(!runtime.Preflight(wrongOpcode),
            "masked expected bytes must still reject the wrong opcode");
}

void TestJumpMutation() {
    ByteBufferMemory memory({0x90, 0x48, 0x89, 0xE5, 0x90, 0x90, 0x90}, 0x7000);
    PatchRuntime runtime(memory);
    PatchDescription description;
    description.feature = "jump-feature";
    description.target = "test-target";
    description.location.pattern = "48 89 E5 90 90 90";
    description.expected =
        ExpectedBytes{0, {0x48, 0x89, 0xE5, 0x90, 0x90}, {}};
    description.mutation.kind = MutationKind::Jump;
    description.mutation.target = 0x7100;

    const auto result = runtime.Install(description);
    Require(static_cast<bool>(result), eu4dll::patch::FormatDiagnostic(result.diagnostic));
    Require(memory.Bytes()[1] == 0xE9, "JMP mutation must use E9");
    const auto displacement = LoadRel32(memory.Bytes(), 2);
    Require(static_cast<Address>(0x7001 + 5 + displacement) == 0x7100,
            "JMP rel32 must resolve to the target");
}

void TestCallMutations() {
    {
        ByteBufferMemory memory({0x90, 0xE8, 0, 0, 0, 0, 0x90}, 0x8000);
        PatchRuntime runtime(memory);
        PatchDescription description;
        description.feature = "direct-call";
        description.target = "test-target";
        description.location.pattern = "90 E8 00 00 00 00 90";
        description.mutation.kind = MutationKind::Call;
        description.mutation.offset = 1;
        description.mutation.target = 0x8100;
        description.mutation.callWidth = CallWidth::Auto;

        const auto result = runtime.Install(description);
        Require(static_cast<bool>(result), eu4dll::patch::FormatDiagnostic(result.diagnostic));
        Require(memory.Bytes()[1] == 0xE8 && memory.Bytes()[6] == 0x90,
                "five-byte CALL must preserve the following byte");
        Require(static_cast<Address>(0x8001 + 5 + LoadRel32(memory.Bytes(), 2)) == 0x8100,
                "direct CALL rel32 must resolve to the target");
    }
    {
        ByteBufferMemory memory({0xFF, 0x15, 0, 0, 0, 0, 0xCC}, 0x9000);
        PatchRuntime runtime(memory);
        PatchDescription description;
        description.feature = "indirect-call";
        description.target = "test-target";
        description.location.pattern = "FF 15 00 00 00 00 CC";
        description.mutation.kind = MutationKind::Call;
        description.mutation.target = 0x9100;
        description.mutation.callWidth = CallWidth::Auto;

        const auto result = runtime.Install(description);
        Require(static_cast<bool>(result), eu4dll::patch::FormatDiagnostic(result.diagnostic));
        Require(memory.Bytes()[0] == 0xE8 && memory.Bytes()[5] == 0x90,
                "six-byte indirect CALL must become E8 rel32 plus NOP");
        Require(memory.Bytes()[6] == 0xCC, "CALL mutation must not overwrite a seventh byte");
    }
}

void TestReferencedStringPattern() {
    std::vector<std::uint8_t> bytes(64, 0x90);
    const Address base = 0xA000;
    bytes[4] = 0x48;
    bytes[5] = 0x8D;
    bytes[6] = 0x35;
    const Address stringAddress = base + 40;
    const std::int32_t displacement = static_cast<std::int32_t>(stringAddress - (base + 11));
    Store(bytes, 7, displacement);
    const std::string reference = "TARGET";
    std::memcpy(bytes.data() + 40, reference.c_str(), reference.size() + 1);

    ByteBufferMemory memory(std::move(bytes), base);
    PatchRuntime runtime(memory);
    eu4dll::patch::PatternLocation location;
    location.pattern = "48 8D 35 S S S S";
    location.referencedStrings = {reference};
    const auto result = runtime.Locate(location, "string-feature", "test-target");

    Require(static_cast<bool>(result), eu4dll::patch::FormatDiagnostic(result.diagnostic));
    Require(result.address == base + 4, "RIP-relative string placeholder must be verified");
}

void TestNegativeStringDisplacementAndAddressBounds() {
    std::vector<std::uint8_t> bytes(48, 0x90);
    const Address base = 0xA800;
    const std::string reference = "BACK";
    std::memcpy(bytes.data() + 4, reference.c_str(), reference.size() + 1);
    bytes[20] = 0x48;
    bytes[21] = 0x8D;
    bytes[22] = 0x35;
    const std::int32_t displacement =
        static_cast<std::int32_t>((base + 4) - (base + 27));
    Store(bytes, 23, displacement);

    ByteBufferMemory memory(std::move(bytes), base);
    PatchRuntime runtime(memory);
    eu4dll::patch::PatternLocation location;
    location.pattern = "48 8D 35 S S S S";
    location.referencedStrings = {reference};
    const auto located = runtime.Locate(location, "negative-displacement", "test-target");
    Require(static_cast<bool>(located),
            eu4dll::patch::FormatDiagnostic(located.diagnostic));
    Require(located.address == base + 20,
            "negative RIP-relative string displacement must resolve correctly");

    ByteBufferMemory wrapping({0xAA, 0xBB},
                              std::numeric_limits<Address>::max());
    PatchRuntime wrappingRuntime(wrapping);
    const auto overflow = wrappingRuntime.Locate(
        RawPatch("AA").location, "overflow-feature", "test-target");
    Require(!overflow, "wrapping search region must be rejected");
    Require(overflow.diagnostic.operation == PatchOperation::ResolveSearchScope,
            "search-region overflow must identify scope resolution");

    eu4dll::patch::Mutation mutation;
    mutation.kind = MutationKind::RawBytes;
    mutation.offset = -1;
    mutation.bytes = {0xCC};
    const auto underflow = runtime.ApplyMutation(
        0, mutation, "underflow-feature", "test-target");
    Require(!underflow.success &&
                underflow.operation == PatchOperation::CalculateMutation,
            "negative mutation offset before address zero must fail calculation");
}

void TestIndirectBranchOptimization() {
    std::vector<std::uint8_t> bytes(128, 0x90);
    const Address base = 0xB000;
    const Address instruction = base + 8;
    const Address pointerAddress = base + 64;
    const Address target = base + 96;
    bytes[8] = 0xFF;
    bytes[9] = 0x25;
    const std::int32_t pointerDisplacement =
        static_cast<std::int32_t>(pointerAddress - (instruction + 6));
    Store(bytes, 10, pointerDisplacement);
    Store(bytes, 64, target);
    bytes[14] = 0x0F;
    bytes[15] = 0x0B;

    ByteBufferMemory memory(std::move(bytes), base);
    PatchRuntime runtime(memory);
    const auto diagnostic = runtime.OptimizeIndirectBranches(
        instruction, 32, "optimizer-feature", "test-target");

    Require(diagnostic.success, eu4dll::patch::FormatDiagnostic(diagnostic));
    Require(memory.Bytes()[8] == 0xE9 && memory.Bytes()[13] == 0x90,
            "indirect JMP must become E9 rel32 plus NOP");
    Require(static_cast<Address>(instruction + 5 + LoadRel32(memory.Bytes(), 9)) == target,
            "optimized JMP must resolve to the indirect target");
}

void TestOptimizerRunsAfterMutationAndPropagatesFailure() {
    std::vector<std::uint8_t> bytes(64, 0x90);
    const Address base = 0xC000;
    bytes[0] = 0xAA;
    bytes[1] = 0xBB;
    bytes[32] = 0xFF;
    bytes[33] = 0x25;
    const std::int32_t outsideBuffer = 0x1000;
    Store(bytes, 34, outsideBuffer);

    ByteBufferMemory memory(std::move(bytes), base);
    PatchRuntime runtime(memory);
    PatchDescription description;
    description.feature = "optimizer-order";
    description.target = "test-target";
    description.location.pattern = "AA BB";
    description.mutation.kind = MutationKind::Jump;
    description.mutation.target = base + 32;
    description.optimization.enabled = true;
    description.optimization.hookAddress = base + 32;
    description.optimization.maxScanSize = 6;

    const auto result = runtime.Install(description);
    Require(!result, "optimizer failure must fail installation");
    Require(result.diagnostic.operation == PatchOperation::OptimizeHook,
            "optimizer failure must identify optimization operation");
    Require(result.diagnostic.feature == "optimizer-order" &&
                result.diagnostic.target == "test-target" &&
                result.diagnostic.match == MatchStatus::Unique,
            "optimizer failure must retain structured patch context");
    Require(memory.Bytes()[0] == 0xE9,
            "control-flow mutation must occur before hook optimization");
}

class FixedSiteProvider final : public eu4dll::patch::ResolvedSiteProvider {
public:
    std::optional<Address> Resolve(const std::string &siteId,
                                   std::string &error) const override {
        if (siteId == "manifest-site") return 0xD010;
        error = "site missing";
        return std::nullopt;
    }
};

void TestResolvedSiteProviderAvoidsPatternScan() {
    std::vector<std::uint8_t> bytes(64, 0x00);
    bytes[16] = 0xAA;
    ByteBufferMemory memory(std::move(bytes), 0xD000);
    PatchRuntime runtime(memory);
    FixedSiteProvider provider;
    runtime.SetResolvedSiteProvider(&provider);
    auto description = RawPatch("FF FF FF FF");
    description.feature = "manifest-site";
    description.expected = eu4dll::patch::ExpectedBytes{0, {0xAA}, {}};
    description.mutation.bytes = {0xCC};
    Require(static_cast<bool>(runtime.Install(description)),
            "resolved manifest site must install without matching its pattern");
    Require(memory.Bytes()[16] == 0xCC, "manifest-resolved mutation used wrong address");

    const auto before = memory.Bytes();
    description.feature = "missing-site";
    Require(!runtime.Install(description), "missing manifest site must fail closed");
    Require(memory.Bytes() == before, "missing manifest site must perform zero mutation");
}

} // namespace

int main() {
    try {
        TestUniqueRawMutationAndContinuation();
        TestUniquenessFailureIsStructured();
        TestPatternFailuresAreStructured();
        TestExpectedOriginalBytesFailure();
        TestMaskedExpectedBytesCoverFullRelativeCall();
        TestJumpMutation();
        TestCallMutations();
        TestReferencedStringPattern();
        TestNegativeStringDisplacementAndAddressBounds();
        TestIndirectBranchOptimization();
        TestOptimizerRunsAfterMutationAndPropagatesFailure();
        TestResolvedSiteProviderAvoidsPatternScan();
    } catch (const std::exception &error) {
        std::cerr << "patch runtime test failed: " << error.what() << std::endl;
        return 1;
    }
    std::cout << "patch runtime tests passed" << std::endl;
    return 0;
}
