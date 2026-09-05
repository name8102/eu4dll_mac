#include "runtime/patch/byte_buffer_memory.h"
#include "runtime/patch/patch_batch.h"
#include "runtime/patch/patch_runtime.h"
#include "targets/eu4_1_37_5/linux_x86_64/base/base_patch.h"
#include "targets/eu4_1_37_5/linux_x86_64/profile.h"
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"

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

// Fixture layout (single byte-buffer region emulating the executable view):
//   [0,20)   ELF header (magic, class64, x86-64 machine)
//   [64, ...) version string "EU4 v1.37.5.0 Inca\0"
//   symbol windows below, each-capable of holding its search bound.
struct FixtureLayout {
    std::size_t versionOffset = 64;
    std::size_t allocSymbol = 256;
    std::size_t allocPattern = 272;
    std::size_t limitSymbol = 1024;
    std::size_t limitPattern = 1040;
    std::size_t indexSymbol = 2048;
    std::size_t indexPattern = 2064;
    std::size_t textureSymbol = 4096;
    std::size_t texturePattern = 4112;
};

std::vector<std::uint8_t> BuildFixture(FixtureLayout &layout) {
    std::vector<std::uint8_t> fixture(8192, 0xCC);
    fixture[0] = 0x7F;
    fixture[1] = 'E';
    fixture[2] = 'L';
    fixture[3] = 'F';
    fixture[4] = 2;  // ELFCLASS64
    fixture[18] = 62;  // EM_X86_64 (little-endian low byte)
    fixture[19] = 0;
    const std::string version = target::kExpectedVersionText;
    std::copy(version.begin(), version.end(),
              fixture.begin() + static_cast<std::ptrdiff_t>(layout.versionOffset));
    fixture[layout.versionOffset + version.size()] = 0;

    // Site 1: allocation call with the exact expected call bytes at +8.
    {
        auto pattern = PatternBytes(target::base::kAllocateFontPattern);
        // Fix the wildcard rel32 to the calibrated expected bytes.
        Require(pattern.size() == 13, "allocate pattern width");
        pattern[9] = 0xA4;
        pattern[10] = 0x02;
        pattern[11] = 0x11;
        pattern[12] = 0xFF;
        Place(fixture, layout.allocPattern, pattern);
    }
    // Site 2: character limit with 0x00 at +4.
    {
        auto pattern = PatternBytes(target::base::kCharacterLimitPattern);
        Require(pattern.size() == 13, "limit pattern width");
        pattern[4] = target::base::kCharacterLimitOriginal;
        Place(fixture, layout.limitPattern, pattern);
    }
    // Site 3: character index with exact overwritten bytes.
    Place(fixture, layout.indexPattern,
          std::vector<std::uint8_t>(target::base::kCharacterIndexOriginal.begin(),
                                    target::base::kCharacterIndexOriginal.end()));
    // Fill the remainder of the 17-byte pattern window with the tail bytes.
    {
        std::vector<std::uint8_t> tail = {0x48, 0x83, 0xBC, 0xC8,
                                          0x00, 0x01, 0x00, 0x00, 0x00};
        Place(fixture, layout.indexPattern + 8, tail);
    }
    // Site 4: texture limit with 0x01 at +5.
    {
        auto pattern = PatternBytes(target::base::kTextureSizePattern);
        Require(pattern.size() == 8, "texture pattern width");
        pattern[5] = target::base::kTextureSizeOriginal;
        Place(fixture, layout.texturePattern, pattern);
    }
    return fixture;
}

ByteBufferMemory BuildMemory() {
    FixtureLayout layout;
    auto fixture = BuildFixture(layout);
    ByteBufferMemory memory(std::move(fixture), 0x400000);
    memory.DefineSymbol(target::symbols::kReadGameSpecific,
                        0x400000 + layout.allocSymbol);
    memory.DefineSymbol(target::symbols::kParseFontFile,
                        0x400000 + layout.limitSymbol);
    memory.DefineSymbol(target::symbols::kLoadTexture,
                        0x400000 + layout.textureSymbol);
    // The ParseFontFile symbol window (0xa20) must cover both the limit and
    // index patterns; anchor it at the earlier of the two.
    return memory;
}

void TestTargetFactsOwnIdentity() {
    Require(std::string(target::kSupportedElfSha256Hex).size() == 64,
            "supported ELF SHA-256 must be a 64-char hex digest");
    Require(std::string(target::kExpectedVersionText) == "EU4 v1.37.5.0 Inca",
            "Linux target must own the exact version contract");
    Require(target::base::kExpandedBitmapFontSize == 0x86ac0,
            "expanded allocation size must be 0x86ac0");
    Require(target::base::kCharacterIndexShift == 0x6ac,
            "character-index shift must be 0x6ac");
}

void TestExecutableFactsOnFixture() {
    auto memory = BuildMemory();
    const auto before = memory.Bytes();
    const auto result = target::ValidateExecutableFacts(memory);
    Require(result.supported, "fixture ELF header and version must validate");
    Require(result.versionText == target::kExpectedVersionText,
            "version text is retained");
    Require(memory.Bytes() == before, "validation performs no mutation");
}

void TestPatchFactsOnFixture() {
    auto memory = BuildMemory();
    const auto result = target::ValidatePatchFacts(memory);
    Require(result.supported, "four base sites must validate on the fixture");
}

void TestBaseDescriptionsPreflightOnFixture() {
    auto memory = BuildMemory();
    eu4dll::patch::PatchRuntime runtime(memory);
    // Reachable dummy hook targets inside the fixture keep this test free of
    // real trampoline allocation.
    const auto base = memory.BaseAddress();
    const auto descriptions = target::base::BaseDescriptions(base + 0x100, base + 0x200);
    Require(descriptions.size() == 4, "base group owns four patch descriptions");
    for (const auto &description : descriptions) {
        const auto preflight = runtime.Preflight(description);
        Require(static_cast<bool>(preflight),
                eu4dll::patch::FormatDiagnostic(preflight.diagnostic).c_str());
    }
    // Continuation resolution comes from install results, not a new scanner.
    const auto index = runtime.Preflight(descriptions[2]);
    Require(index.ContinuationAddress("return") ==
                index.diagnostic.matchAddress +
                    target::base::kCharacterIndexContinuationOffset,
            "character-index continuation must be site + 8");
}

void TestBaseBatchCommitsAtomically() {
    auto memory = BuildMemory();
    const auto base = memory.BaseAddress();
    eu4dll::patch::PatchBatch batch(memory, nullptr);
    for (auto &description : target::base::BaseDescriptions(base + 0x100, base + 0x200)) {
        batch.Add(std::move(description));
    }
    Require(static_cast<bool>(batch.Preflight()), "base batch preflights cleanly");
    const auto before = memory.Bytes();
    (void)before;
    const auto result = batch.Commit();
    Require(static_cast<bool>(result), "base batch commits atomically");
    Require(result.installations.size() == 4, "all four base patches publish results");
}

void TestUnknownBinaryFailsClosed() {
    FixtureLayout layout;
    auto fixture = BuildFixture(layout);
    // Corrupt the ELF magic: header validation must fail before any mutation.
    fixture[0] = 0x00;
    ByteBufferMemory memory(std::move(fixture), 0x400000);
    const auto before = memory.Bytes();
    const auto result = target::ValidateTarget(memory);
    Require(!result, "unknown binary must fail closed");
    Require(memory.Bytes() == before, "failed validation mutates nothing");
}

}  // namespace

int main() {
    TestTargetFactsOwnIdentity();
    TestExecutableFactsOnFixture();
    TestPatchFactsOnFixture();
    TestBaseDescriptionsPreflightOnFixture();
    TestBaseBatchCommitsAtomically();
    TestUnknownBinaryFailsClosed();
    std::cout << "linux target tests passed" << std::endl;
    return 0;
}
