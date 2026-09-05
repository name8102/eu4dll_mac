#include "runtime/patch/byte_buffer_memory.h"
#include "runtime/patch/patch_batch.h"
#include "runtime/patch/patch_runtime.h"
#include "targets/eu4_1_37_5/linux_x86_64/main_text/main_text_patch.h"
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"

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

// One RenderToScreen symbol window (bound 0x218b) holds all three sites.
struct FixtureLayout {
    std::size_t symbol = 512;
    std::size_t preprocessing = 640;
    std::size_t wrapping = 1024;
    std::size_t drawing = 2048;
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
          PatternBytes(target::main_text::kPreprocessingPattern));
    Place(fixture, layout.wrapping, PatternBytes(target::main_text::kWrappingPattern));
    Place(fixture, layout.drawing, PatternBytes(target::main_text::kDrawingPattern));

    ByteBufferMemory memory(std::move(fixture), 0x400000);
    memory.DefineSymbol(target::main_text::kRenderToScreenSymbol,
                        0x400000 + layout.symbol);
    return memory;
}

target::main_text::MainTextHookTargets DummyTargets(Address base) {
    target::main_text::MainTextHookTargets targets;
    targets.preprocessing = base + 0x100;
    targets.wrapping = base + 0x200;
    targets.drawing = base + 0x300;
    return targets;
}

void TestThreeDescriptionsPreflight() {
    auto memory = BuildMemory();
    eu4dll::patch::PatchRuntime runtime(memory);
    const auto descriptions =
        target::main_text::MainTextDescriptions(DummyTargets(memory.BaseAddress()));
    Require(descriptions.size() == 3, "main-text group owns three patch descriptions");
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
        target::main_text::MainTextDescriptions(DummyTargets(memory.BaseAddress()));
    const auto preprocessing = runtime.Preflight(descriptions[0]);
    Require(preprocessing.ContinuationAddress("return") ==
                preprocessing.diagnostic.matchAddress +
                    target::main_text::kPreprocessingContinuationOffset,
            "preprocessing return must be site + 5");
    Require(preprocessing.ContinuationAddress("bypass") ==
                preprocessing.diagnostic.matchAddress +
                    target::main_text::kPreprocessingBypassOffset,
            "preprocessing bypass must be site + 0x5e");
    const auto wrapping = runtime.Preflight(descriptions[1]);
    Require(wrapping.ContinuationAddress("bypass") ==
                wrapping.diagnostic.matchAddress +
                    target::main_text::kWrappingBypassOffset,
            "wrapping bypass must be site + 0x191");
    const auto drawing = runtime.Preflight(descriptions[2]);
    Require(drawing.ContinuationAddress("return") ==
                drawing.diagnostic.matchAddress +
                    target::main_text::kDrawingContinuationOffset,
            "drawing return must be site + 7");
    Require(drawing.ContinuationAddress("bypass") ==
                drawing.diagnostic.matchAddress +
                    target::main_text::kDrawingBypassOffset,
            "drawing bypass must be site + 0x1aa");
}

void TestBatchCommitsAtomically() {
    auto memory = BuildMemory();
    eu4dll::patch::PatchBatch batch(memory, nullptr);
    for (auto &description :
         target::main_text::MainTextDescriptions(DummyTargets(memory.BaseAddress()))) {
        batch.Add(std::move(description));
    }
    Require(static_cast<bool>(batch.Preflight()), "main-text batch preflights cleanly");
    const auto result = batch.Commit();
    Require(static_cast<bool>(result), "main-text batch commits atomically");
    Require(result.installations.size() == 3, "all three hooks publish results");
    for (const auto &installation : result.installations) {
        Require(installation.continuations.size() == 2,
                "each hook publishes return + bypass");
    }
}

void TestExpectedMismatchWritesNothing() {
    auto memory = BuildMemory();
    std::vector<std::uint8_t> bad = memory.Bytes();
    bad[1024] ^= 0xFF;  // corrupt the wrapping expected bytes
    ByteBufferMemory corrupted(std::move(bad), 0x400000);
    corrupted.DefineSymbol(target::main_text::kRenderToScreenSymbol, 0x400000 + 512);
    const auto before = corrupted.Bytes();
    eu4dll::patch::PatchBatch batch(corrupted, nullptr);
    for (auto &description : target::main_text::MainTextDescriptions(
             DummyTargets(corrupted.BaseAddress()))) {
        batch.Add(std::move(description));
    }
    Require(!batch.Commit(), "corrupted wrapping must fail the main-text group");
    Require(corrupted.Bytes() == before, "failed main-text group writes nothing");
}

void TestStagingFactsAreLinux() {
    Require(target::main_text::kPreprocessingStageAddress == 0x3345191,
            "staging buffer must be the calibrated absolute address");
    Require(target::main_text::kDrawingObjectDisplacement == 0x333d450,
            "drawing base must be [rbx + disp32]");
    Require(target::main_text::kRenderToScreenSearchSize == 0x218b,
            "all three sites share the RenderToScreen window");
}

}  // namespace

int main() {
    TestThreeDescriptionsPreflight();
    TestContinuationsResolveFromInstallResults();
    TestBatchCommitsAtomically();
    TestExpectedMismatchWritesNothing();
    TestStagingFactsAreLinux();
    std::cout << "linux main text tests passed" << std::endl;
    return 0;
}
