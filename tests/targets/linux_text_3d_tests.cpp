#include "runtime/patch/byte_buffer_memory.h"
#include "runtime/patch/patch_batch.h"
#include "runtime/patch/patch_runtime.h"
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"
#include "targets/eu4_1_37_5/linux_x86_64/text_3d/text_3d_patch.h"

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

// One Render3d symbol window (bound 0x1000) holds both sites.
struct FixtureLayout {
    std::size_t symbol = 512;
    std::size_t preprocessing = 640;
    std::size_t drawing = 1024;
};

ByteBufferMemory BuildMemory() {
    FixtureLayout layout;
    std::vector<std::uint8_t> fixture(8192, 0xCC);
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
          PatternBytes(target::text_3d::kPreprocessingPattern));
    Place(fixture, layout.drawing, PatternBytes(target::text_3d::kDrawingPattern));

    ByteBufferMemory memory(std::move(fixture), 0x400000);
    memory.DefineSymbol(target::text_3d::kRender3dSymbol, 0x400000 + layout.symbol);
    memory.DefineSymbol(target::text_3d::kCStringAppendCharSymbol, 0x400000 + 100);
    return memory;
}

target::text_3d::Text3DHookTargets DummyTargets(Address base) {
    target::text_3d::Text3DHookTargets targets;
    targets.preprocessing = base + 0x100;
    targets.drawing = base + 0x200;
    return targets;
}

void TestDescriptionsPreflight() {
    auto memory = BuildMemory();
    eu4dll::patch::PatchRuntime runtime(memory);
    const auto descriptions =
        target::text_3d::Text3DDescriptions(DummyTargets(memory.BaseAddress()));
    Require(descriptions.size() == 2, "text-3d group owns two patch descriptions");
    for (const auto &description : descriptions) {
        const auto preflight = runtime.Preflight(description);
        Require(static_cast<bool>(preflight),
                eu4dll::patch::FormatDiagnostic(preflight.diagnostic).c_str());
    }
}

void TestContinuationsResolve() {
    auto memory = BuildMemory();
    eu4dll::patch::PatchRuntime runtime(memory);
    const auto descriptions =
        target::text_3d::Text3DDescriptions(DummyTargets(memory.BaseAddress()));
    const auto preprocessing = runtime.Preflight(descriptions[0]);
    Require(preprocessing.ContinuationAddress("return") ==
                preprocessing.diagnostic.matchAddress +
                    target::text_3d::kPreprocessingContinuationOffset,
            "preprocessing return must be site + 11");
    const auto drawing = runtime.Preflight(descriptions[1]);
    Require(drawing.ContinuationAddress("return") ==
                drawing.diagnostic.matchAddress +
                    target::text_3d::kDrawingContinuationOffset,
            "drawing return must be site + 11");
}

void TestBatchCommitsAtomically() {
    auto memory = BuildMemory();
    eu4dll::patch::PatchBatch batch(memory, nullptr);
    for (auto &description :
         target::text_3d::Text3DDescriptions(DummyTargets(memory.BaseAddress()))) {
        batch.Add(std::move(description));
    }
    Require(static_cast<bool>(batch.Preflight()), "text-3d preflights cleanly");
    const auto result = batch.Commit();
    Require(static_cast<bool>(result), "text-3d commits atomically");
    Require(result.installations.size() == 2, "both hooks publish results");
}

void TestExpectedMismatchWritesNothing() {
    auto memory = BuildMemory();
    std::vector<std::uint8_t> bad = memory.Bytes();
    bad[1024] ^= 0xFF;
    ByteBufferMemory corrupted(std::move(bad), 0x400000);
    corrupted.DefineSymbol(target::text_3d::kRender3dSymbol, 0x400000 + 512);
    corrupted.DefineSymbol(target::text_3d::kCStringAppendCharSymbol, 0x400000 + 100);
    const auto before = corrupted.Bytes();
    eu4dll::patch::PatchBatch batch(corrupted, nullptr);
    for (auto &description : target::text_3d::Text3DDescriptions(
             DummyTargets(corrupted.BaseAddress()))) {
        batch.Add(std::move(description));
    }
    Require(!batch.Commit(), "corrupted drawing must fail the group");
    Require(corrupted.Bytes() == before, "failed group writes nothing");
}

void TestLiveTargetsWithoutAllocatorFailClosed() {
    auto memory = BuildMemory();
    const auto preflight = target::text_3d::PreflightText3D(memory, nullptr);
    Require(!preflight, "unreachable live targets without allocator must fail");
    Require(preflight.diagnostic.operation ==
                eu4dll::patch::PatchOperation::CalculateMutation,
            "reachability failure must identify mutation calculation");
}

}  // namespace

int main() {
    TestDescriptionsPreflight();
    TestContinuationsResolve();
    TestBatchCommitsAtomically();
    TestExpectedMismatchWritesNothing();
    TestLiveTargetsWithoutAllocatorFailClosed();
    std::cout << "linux text3d tests passed" << std::endl;
    return 0;
}
