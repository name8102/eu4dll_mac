#include "runtime/patch/byte_buffer_memory.h"
#include "runtime/patch/patch_batch.h"
#include "runtime/patch/patch_runtime.h"
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"
#include "targets/eu4_1_37_5/linux_x86_64/text_layout/layout_patch.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace target = eu4dll::targets::eu4_1_37_5::linux_x86_64;
using eu4dll::patch::ByteBufferMemory;

namespace {

using eu4dll::patch::Address;

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

struct FixtureLayout {
    std::size_t heightSymbol = 256;
    std::size_t heightPattern = 272;
    std::size_t widthSymbol = 1408;
    std::size_t widthPattern = 1424;
    std::size_t arsSymbol = 2048;
    std::size_t arsPattern = 2064;
    std::size_t wrapPattern = 2304;
    std::size_t requiredSymbol = 3712;
    std::size_t requiredPattern = 3728;
    std::size_t actualRealSymbol = 5760;
    std::size_t actualRealPattern = 5776;
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

    Place(fixture, layout.heightPattern,
          PatternBytes(target::layout::kGetHeightOfStringPattern));
    Place(fixture, layout.widthPattern,
          PatternBytes(target::layout::kGetWidthOfStringPattern));
    Place(fixture, layout.arsPattern,
          PatternBytes(target::layout::kGetActualRequiredSizePattern));
    Place(fixture, layout.wrapPattern,
          PatternBytes(target::layout::kWrappingGatePattern));
    Place(fixture, layout.requiredPattern,
          PatternBytes(target::layout::kGetRequiredSizePattern));
    Place(fixture, layout.actualRealPattern,
          PatternBytes(target::layout::kGetActualRealRequiredSizeActuallyPattern));

    ByteBufferMemory memory(std::move(fixture), 0x400000);
    memory.DefineSymbol(target::layout::kGetHeightOfStringSymbol,
                        0x400000 + layout.heightSymbol);
    memory.DefineSymbol(target::layout::kGetWidthOfStringSymbol,
                        0x400000 + layout.widthSymbol);
    memory.DefineSymbol(target::layout::kGetActualRequiredSizeSymbol,
                        0x400000 + layout.arsSymbol);
    memory.DefineSymbol(target::layout::kGetRequiredSizeSymbol,
                        0x400000 + layout.requiredSymbol);
    memory.DefineSymbol(target::layout::kGetActualRealRequiredSizeActuallySymbol,
                        0x400000 + layout.actualRealSymbol);
    return memory;
}

target::layout::LayoutHookTargets DummyTargets(Address base) {
    target::layout::LayoutHookTargets targets;
    targets.height = base + 0x100;
    targets.width = base + 0x200;
    targets.actualRequiredSize = base + 0x300;
    targets.requiredSize = base + 0x400;
    targets.actualRealRequiredSizeActually = base + 0x500;
    return targets;
}

void TestSixDescriptionsPreflight() {
    auto memory = BuildMemory();
    eu4dll::patch::PatchRuntime runtime(memory);
    const auto descriptions =
        target::layout::LayoutDescriptions(DummyTargets(memory.BaseAddress()));
    Require(descriptions.size() == 6, "layout group owns six patch descriptions");
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
        target::layout::LayoutDescriptions(DummyTargets(memory.BaseAddress()));
    const auto height = runtime.Preflight(descriptions[0]);
    Require(height.ContinuationAddress("return") ==
                height.diagnostic.matchAddress +
                    target::layout::kGetHeightOfStringContinuationOffset,
            "height continuation must be site + 11");
    const auto width = runtime.Preflight(descriptions[1]);
    Require(width.ContinuationAddress("return") ==
                width.diagnostic.matchAddress +
                    target::layout::kGetWidthOfStringContinuationOffset,
            "width return must be site + 8");
    Require(width.ContinuationAddress("bypass") ==
                width.diagnostic.matchAddress +
                    target::layout::kGetWidthOfStringBypassOffset,
            "width bypass must be site + 0x187");
}

void TestBatchCommitsAtomically() {
    auto memory = BuildMemory();
    eu4dll::patch::PatchBatch batch(memory, nullptr);
    for (auto &description :
         target::layout::LayoutDescriptions(DummyTargets(memory.BaseAddress()))) {
        batch.Add(std::move(description));
    }
    Require(static_cast<bool>(batch.Preflight()), "layout batch preflights cleanly");
    const auto result = batch.Commit();
    Require(static_cast<bool>(result), "layout batch commits atomically");
    Require(result.installations.size() == 6, "all six layout patches publish results");
    Require(result.installations[1].continuations.size() == 2,
            "width install publishes return + bypass");
}

void TestExpectedMismatchWritesNothing() {
    auto memory = BuildMemory();
    // Corrupt the wrapping-gate expected bytes: the whole group must refuse.
    std::vector<std::uint8_t> bad = memory.Bytes();
    bad[2304] ^= 0xFF;
    ByteBufferMemory corrupted(std::move(bad), 0x400000);
    corrupted.DefineSymbol(target::layout::kGetHeightOfStringSymbol, 0x400000 + 256);
    corrupted.DefineSymbol(target::layout::kGetWidthOfStringSymbol, 0x400000 + 1408);
    corrupted.DefineSymbol(target::layout::kGetActualRequiredSizeSymbol, 0x400000 + 2048);
    corrupted.DefineSymbol(target::layout::kGetRequiredSizeSymbol, 0x400000 + 3712);
    corrupted.DefineSymbol(target::layout::kGetActualRealRequiredSizeActuallySymbol,
                           0x400000 + 5760);
    const auto before = corrupted.Bytes();
    eu4dll::patch::PatchBatch batch(corrupted, nullptr);
    for (auto &description :
         target::layout::LayoutDescriptions(DummyTargets(corrupted.BaseAddress()))) {
        batch.Add(std::move(description));
    }
    Require(!batch.Commit(), "corrupted gate must fail the layout group");
    Require(corrupted.Bytes() == before, "failed layout group writes nothing");
}

void TestGlyphTableFactIsLinux() {
    Require(target::layout::kGlyphTableOffset == 0x100,
            "Linux glyph-table base must be 0x100, not the macOS value");
    Require(target::base::kCharacterIndexShift == 0x6ac,
            "Linux index shift must be 0x6ac");
}

}  // namespace

int main() {
    TestSixDescriptionsPreflight();
    TestContinuationsResolveFromInstallResults();
    TestBatchCommitsAtomically();
    TestExpectedMismatchWritesNothing();
    TestGlyphTableFactIsLinux();
    std::cout << "linux text layout tests passed" << std::endl;
    return 0;
}
