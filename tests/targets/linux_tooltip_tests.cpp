#include "runtime/patch/byte_buffer_memory.h"
#include "runtime/patch/patch_batch.h"
#include "runtime/patch/patch_runtime.h"
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"
#include "targets/eu4_1_37_5/linux_x86_64/tooltip_text/tooltip_patch.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace target = eu4dll::targets::eu4_1_37_5::linux_x86_64;
using eu4dll::patch::Address;
using eu4dll::patch::ByteBufferMemory;

namespace {

void Require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

std::vector<std::uint8_t> PatternBytes(const char *pattern) {
    std::istringstream stream(pattern);
    std::string token;
    std::vector<std::uint8_t> bytes;
    std::uint8_t wildcard = 0x31;
    while (stream >> token) {
        if (token == "?" || token == "??") {
            bytes.push_back(wildcard++);
        } else {
            bytes.push_back(static_cast<std::uint8_t>(std::stoul(token, nullptr, 16)));
        }
    }
    return bytes;
}

void Place(std::vector<std::uint8_t> &fixture, std::size_t offset,
           const std::vector<std::uint8_t> &bytes) {
    Require(offset + bytes.size() <= fixture.size(), "fixture placement in range");
    std::copy(bytes.begin(), bytes.end(), fixture.begin() + offset);
}

// One RenderToTexture symbol window (bound 0x311c) holds all three sites.
struct FixtureLayout {
    std::size_t symbol = 512;
    std::size_t preprocessing = 1024;
    std::size_t wrapping = 2048;
    std::size_t drawing = 4096;
};

ByteBufferMemory BuildMemory() {
    FixtureLayout layout;
    std::vector<std::uint8_t> fixture(16384, 0xCC);
    fixture[0] = 0x7F;
    fixture[1] = 'E';
    fixture[2] = 'L';
    fixture[3] = 'F';
    fixture[4] = 2;
    fixture[18] = 62;
    const std::string version = target::kExpectedVersionText;
    std::copy(version.begin(), version.end(), fixture.begin() + 64);
    fixture[64 + version.size()] = 0;

    Place(fixture, layout.preprocessing,
          PatternBytes(target::tooltip::kPreprocessingPattern));
    Place(fixture, layout.wrapping, PatternBytes(target::tooltip::kWrappingPattern));
    Place(fixture, layout.drawing, PatternBytes(target::tooltip::kDrawingPattern));

    ByteBufferMemory memory(std::move(fixture), 0x400000);
    memory.DefineSymbol(target::tooltip::kRenderToTextureSymbol,
                        0x400000 + layout.symbol);
    // The preprocessing hook calls this; fixtures define it so InstallTooltip
    // symbol resolution can also be exercised without EU4.
    memory.DefineSymbol(target::tooltip::kCStringAppendCharSymbol, 0x400000 + 100);
    return memory;
}

target::tooltip::TooltipHookTargets DummyTargets(Address base) {
    target::tooltip::TooltipHookTargets targets;
    targets.preprocessing = base + 0x100;
    targets.wrapping = base + 0x200;
    targets.drawing = base + 0x300;
    return targets;
}

void TestThreeDescriptionsPreflight() {
    auto memory = BuildMemory();
    eu4dll::patch::PatchRuntime runtime(memory);
    const auto descriptions =
        target::tooltip::TooltipDescriptions(DummyTargets(memory.BaseAddress()));
    Require(descriptions.size() == 3, "tooltip group owns three patch descriptions");
    for (const auto &description : descriptions) {
        const auto preflight = runtime.Preflight(description);
        Require(static_cast<bool>(preflight),
                eu4dll::patch::FormatDiagnostic(preflight.diagnostic).c_str());
    }
}

void TestContinuationsResolveFromInstallResults() {
    auto memory = BuildMemory();
    eu4dll::patch::PatchRuntime runtime(memory);
    const auto descriptions =
        target::tooltip::TooltipDescriptions(DummyTargets(memory.BaseAddress()));
    const auto preprocessing = runtime.Preflight(descriptions[0]);
    Require(preprocessing.ContinuationAddress("return") ==
                preprocessing.diagnostic.matchAddress +
                    target::tooltip::kPreprocessingContinuationOffset,
            "preprocessing return must be site + 16");
    Require(preprocessing.continuations.size() == 1,
            "preprocessing publishes return only (no bypass)");
    const auto wrapping = runtime.Preflight(descriptions[1]);
    Require(wrapping.ContinuationAddress("bypass") ==
                wrapping.diagnostic.matchAddress +
                    target::tooltip::kWrappingBypassOffset,
            "wrapping bypass must be site + 0x14");
    const auto drawing = runtime.Preflight(descriptions[2]);
    Require(drawing.ContinuationAddress("return") ==
                drawing.diagnostic.matchAddress +
                    target::tooltip::kDrawingContinuationOffset,
            "drawing return must be site + 11");
    Require(drawing.continuations.size() == 1,
            "drawing publishes return only (no bypass)");
}

void TestBatchCommitsAtomically() {
    auto memory = BuildMemory();
    eu4dll::patch::PatchBatch batch(memory, nullptr);
    for (auto &description :
         target::tooltip::TooltipDescriptions(DummyTargets(memory.BaseAddress()))) {
        batch.Add(std::move(description));
    }
    Require(static_cast<bool>(batch.Preflight()), "tooltip batch preflights cleanly");
    const auto result = batch.Commit();
    Require(static_cast<bool>(result), "tooltip batch commits atomically");
    Require(result.installations.size() == 3, "all three hooks publish results");
}

void TestLiveTargetsWithoutAllocatorFailClosed() {
    auto memory = BuildMemory();
    // PreflightTooltip uses live hook addresses far outside rel32 from the
    // fixture; with no allocator this must fail closed (not crash) on the
    // reachability check, while the callee symbol itself still resolves.
    std::string error;
    Require(static_cast<bool>(
                memory.ResolveSymbol(target::tooltip::kCStringAppendCharSymbol, error)),
            "fixture callee symbol must resolve");
    const auto preflight = target::tooltip::PreflightTooltip(memory, nullptr);
    Require(!preflight, "unreachable live targets without allocator must fail");
    Require(preflight.diagnostic.operation ==
                eu4dll::patch::PatchOperation::CalculateMutation,
            "reachability failure must identify mutation calculation");
}

void TestExpectedMismatchWritesNothing() {
    auto memory = BuildMemory();
    std::vector<std::uint8_t> bad = memory.Bytes();
    bad[2048] ^= 0xFF;  // corrupt the wrapping expected bytes
    ByteBufferMemory corrupted(std::move(bad), 0x400000);
    corrupted.DefineSymbol(target::tooltip::kRenderToTextureSymbol, 0x400000 + 512);
    corrupted.DefineSymbol(target::tooltip::kCStringAppendCharSymbol, 0x400000 + 100);
    const auto before = corrupted.Bytes();
    eu4dll::patch::PatchBatch batch(corrupted, nullptr);
    for (auto &description : target::tooltip::TooltipDescriptions(
             DummyTargets(corrupted.BaseAddress()))) {
        batch.Add(std::move(description));
    }
    Require(!batch.Commit(), "corrupted wrapping must fail the tooltip group");
    Require(corrupted.Bytes() == before, "failed tooltip group writes nothing");
}

}  // namespace

int main() {
    TestThreeDescriptionsPreflight();
    TestContinuationsResolveFromInstallResults();
    TestBatchCommitsAtomically();
    TestExpectedMismatchWritesNothing();
    TestLiveTargetsWithoutAllocatorFailClosed();
    std::cout << "linux tooltip tests passed" << std::endl;
    return 0;
}
