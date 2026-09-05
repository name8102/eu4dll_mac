#include "runtime/patch/byte_buffer_memory.h"
#include "runtime/patch/patch_batch.h"
#include "runtime/patch/patch_runtime.h"
#include "targets/eu4_1_37_5/linux_x86_64/map_text/map_text_patch.h"
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace target = eu4dll::targets::eu4_1_37_5::linux_x86_64;
namespace maptext = target::map_text;
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

struct FixtureLayout {
    std::size_t fillSymbol = 256;
    std::size_t fillPreprocessing = 320;
    std::size_t fillDrawing = 512;
    std::size_t areaSymbol = 4096;
    std::size_t toUpperCall = 4160;
    std::size_t spacing = 4352;
    std::size_t areaGlyph = 4608;
    std::size_t curveDrawing = 5632;
    std::size_t lengthCalls = 6144;
    std::size_t nudgedSymbol = 13568;
    std::size_t nudgedGlyph = 13632;
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

    Place(fixture, layout.fillPreprocessing,
          PatternBytes(maptext::kFillPreprocessingPattern));
    Place(fixture, layout.fillDrawing, PatternBytes(maptext::kFillDrawingPattern));
    Place(fixture, layout.toUpperCall, PatternBytes(maptext::kToUpperCallPattern));
    // The call rel32 is calibrated, not a wildcard: pin exact bytes.
    fixture[layout.toUpperCall + 0] = 0xE8;
    fixture[layout.toUpperCall + 1] = 0x25;
    fixture[layout.toUpperCall + 2] = 0xF2;
    fixture[layout.toUpperCall + 3] = 0x9E;
    fixture[layout.toUpperCall + 4] = 0x00;
    Place(fixture, layout.spacing, PatternBytes(maptext::kSpacingPattern));
    Place(fixture, layout.areaGlyph, PatternBytes(maptext::kNameGlyphPattern));
    Place(fixture, layout.curveDrawing, PatternBytes(maptext::kCurveDrawingPattern));
    Place(fixture, layout.lengthCalls,
          PatternBytes(maptext::kCurveLengthCallsPattern));
    // Both GetSize calls must carry their distinct rel32 operands
    // (pattern wildcards would otherwise leave filler bytes).
    fixture[layout.lengthCalls + 0] = 0xE8;
    fixture[layout.lengthCalls + 1] = 0xC2;
    fixture[layout.lengthCalls + 2] = 0xD0;
    fixture[layout.lengthCalls + 3] = 0x9E;
    fixture[layout.lengthCalls + 4] = 0x00;
    // Second GetSize call must carry its distinct rel32 operand.
    fixture[layout.lengthCalls + 11] = 0xE8;
    fixture[layout.lengthCalls + 12] = 0xB7;
    fixture[layout.lengthCalls + 13] = 0xD0;
    fixture[layout.lengthCalls + 14] = 0x9E;
    fixture[layout.lengthCalls + 15] = 0x00;
    Place(fixture, layout.nudgedGlyph, PatternBytes(maptext::kNameGlyphPattern));

    ByteBufferMemory memory(std::move(fixture), 0x400000);
    memory.DefineSymbol(maptext::kFillVertexBufferSymbol,
                        0x400000 + layout.fillSymbol);
    memory.DefineSymbol(maptext::kGenerateNamesAddNameAreaSymbol,
                        0x400000 + layout.areaSymbol);
    memory.DefineSymbol(maptext::kAddNudgedNamesSymbol,
                        0x400000 + layout.nudgedSymbol);
    return memory;
}

maptext::MapHookTargets DummyTargets(Address base) {
    maptext::MapHookTargets targets;
    targets.fillPreprocessing = base + 0x100;
    targets.fillDrawing = base + 0x200;
    targets.spacing = base + 0x300;
    targets.toUpperCall = base + 0x400;
    targets.addNameAreaGlyph = base + 0x500;
    targets.addNudgedNamesGlyph = base + 0x600;
    targets.curveDrawing = base + 0x700;
    targets.curveGetSizeFirst = base + 0x800;
    targets.curveGetSizeSecond = base + 0x900;
    return targets;
}

void CheckPreflight(eu4dll::patch::PatchRuntime &runtime,
                    const eu4dll::patch::PatchDescription &description) {
    const auto preflight = runtime.Preflight(description);
    Require(static_cast<bool>(preflight),
            eu4dll::patch::FormatDiagnostic(preflight.diagnostic).c_str());
}

void TestClusterPreflights() {
    auto memory = BuildMemory();
    eu4dll::patch::PatchRuntime runtime(memory);
    const auto targets = DummyTargets(memory.BaseAddress());
    for (const auto &d : maptext::FillVertexBufferDescriptions(targets)) CheckPreflight(runtime, d);
    for (const auto &d : maptext::AddNameAreaDescriptions(targets)) CheckPreflight(runtime, d);
    for (const auto &d : maptext::AddNudgedNamesDescriptions(targets)) CheckPreflight(runtime, d);
    for (const auto &d : maptext::CurveTextDescriptions(targets)) CheckPreflight(runtime, d);
    // Length-call window locates uniquely for address pinning.
}

void TestClusterContinuations() {
    auto memory = BuildMemory();
    eu4dll::patch::PatchRuntime runtime(memory);
    const auto targets = DummyTargets(memory.BaseAddress());
    const auto spacing = maptext::SpacingDescription(targets.spacing);
    const auto located = runtime.Preflight(spacing);
    Require(static_cast<bool>(located), "spacing must preflight");
    Require(located.ContinuationAddress("return") ==
                located.diagnostic.matchAddress + maptext::kSpacingContinuationOffset,
            "spacing return must be site + 11");
    Require(located.ContinuationAddress("final") ==
                located.diagnostic.matchAddress + maptext::kSpacingFinalOffset,
            "spacing final must be site + 0x5c");
}

void TestClusterBatchesCommit() {
    auto memory = BuildMemory();
    const auto targets = DummyTargets(memory.BaseAddress());
    // Commit each cluster with fixture-reachable targets, including the
    // address-pinned GetSize redirects for the curve cluster.
    {
        eu4dll::patch::PatchBatch batch(memory, nullptr);
        for (auto d : maptext::FillVertexBufferDescriptions(targets)) batch.Add(std::move(d));
        Require(static_cast<bool>(batch.Commit()), "fill cluster commits");
    }
    {
        eu4dll::patch::PatchBatch batch(memory, nullptr);
        for (auto d : maptext::AddNameAreaDescriptions(targets)) batch.Add(std::move(d));
        Require(static_cast<bool>(batch.Commit()), "area cluster commits");
    }
    {
        eu4dll::patch::PatchBatch batch(memory, nullptr);
        for (auto d : maptext::AddNudgedNamesDescriptions(targets)) batch.Add(std::move(d));
        Require(static_cast<bool>(batch.Commit()), "nudged cluster commits");
    }
    {
        eu4dll::patch::PatchBatch batch(memory, nullptr);
        for (auto d : maptext::CurveTextDescriptions(targets)) batch.Add(std::move(d));
        for (auto d : maptext::CurveCallRedirects(memory.BaseAddress() + 0xB00)) {
            batch.Add(std::move(d));
        }
        Require(static_cast<bool>(batch.Preflight()), "curve cluster preflights");
        const auto result = batch.Commit();
        Require(static_cast<bool>(result), "curve cluster commits");
        Require(result.installations.size() == 3, "curve installs 1 jump + 2 calls");
    }
}

void TestMismatchWritesNothing() {
    auto memory = BuildMemory();
    std::vector<std::uint8_t> bad = memory.Bytes();
    bad[5632] ^= 0xFF;  // corrupt the CurveText drawing site
    ByteBufferMemory corrupted(std::move(bad), 0x400000);
    corrupted.DefineSymbol(maptext::kFillVertexBufferSymbol, 0x400000 + 256);
    corrupted.DefineSymbol(maptext::kGenerateNamesAddNameAreaSymbol, 0x400000 + 4096);
    corrupted.DefineSymbol(maptext::kAddNudgedNamesSymbol, 0x400000 + 13568);
    const auto before = corrupted.Bytes();
    const auto result =
        maptext::PreflightCurveText(corrupted, nullptr);
    Require(!result, "corrupted CurveText site must fail preflight");
    Require(corrupted.Bytes() == before, "failed preflight writes nothing");
}

void TestEscapeWalkLogic() {
    using maptext::AdvanceLogicalChar;
    using maptext::CountLogicalGlyphs;
    const std::uint8_t ascii[] = {'A', 'B', 'C'};
    Require(CountLogicalGlyphs(ascii, 3) == 3, "ASCII counts per byte");
    Require(AdvanceLogicalChar(ascii, 3, 0) == 1, "plain byte advances one");
    const std::uint8_t escaped[] = {0x10, 0x2D, 0x4E};
    Require(CountLogicalGlyphs(escaped, 3) == 1, "escape counts one glyph");
    Require(AdvanceLogicalChar(escaped, 3, 0) == 3, "escape advances three");
    const std::uint8_t mixed[] = {'A', 0x10, 0x2D, 0x4E, 'B'};
    Require(CountLogicalGlyphs(mixed, 5) == 3, "mixed counts three glyphs");
    // Truncated escapes never skip NUL or run past the end.
    const std::uint8_t truncated[] = {0x10};
    Require(AdvanceLogicalChar(truncated, 1, 0) == 1, "lone marker advances one");
    Require(CountLogicalGlyphs(truncated, 1) == 1, "lone marker counts one");
    const std::uint8_t nulPayload[] = {0x10, 0x00, 0x41};
    Require(AdvanceLogicalChar(nulPayload, 3, 0) == 1, "NUL payload advances one");
    Require(AdvanceLogicalChar(nullptr, 0, 0) == 0, "null input is safe");
    Require(AdvanceLogicalChar(ascii, 3, 3) == 3, "offset at end clamps");
}

}  // namespace

int main() {
    TestClusterPreflights();
    TestClusterContinuations();
    TestClusterBatchesCommit();
    TestMismatchWritesNothing();
    TestEscapeWalkLogic();
    std::cout << "linux map text tests passed" << std::endl;
    return 0;
}
