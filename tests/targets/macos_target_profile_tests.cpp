#include "runtime/patch/byte_buffer_memory.h"
#include "targets/eu4_1_37_5/macos_x86_64/profile.h"
#include "targets/eu4_1_37_5/macos_x86_64/target_facts.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace target = eu4dll::targets::eu4_1_37_5::macos_x86_64;
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
    Require(offset + bytes.size() <= fixture.size(), "fixture placement is in range");
    std::copy(bytes.begin(), bytes.end(), fixture.begin() + offset);
}

std::vector<std::uint8_t> SupportedFixture(const std::string &version = "EU4 v1.37.5.0 Inca") {
    std::vector<std::uint8_t> fixture(4096, 0xCC);
    const std::array<std::uint32_t, 4> machHeader{{
        target::executable::kMach64Magic,
        target::executable::kCpuTypeX86_64,
        3,
        target::executable::kMachExecuteFileType,
    }};
    std::memcpy(fixture.data(), machHeader.data(), sizeof(machHeader));
    Place(fixture, 128, std::vector<std::uint8_t>(version.begin(), version.end()));
    fixture[128 + version.size()] = 0;
    Place(fixture, 384, PatternBytes(target::input::kHandleKeyEvent1.pattern));
    Place(fixture, 700, PatternBytes(target::map_text::kCurveText4.pattern));
    Place(fixture, 1100, PatternBytes(target::localization::kMonarchFullName.pattern));
    Place(fixture, 1600, PatternBytes(target::input::kHandlePdxEvents1.pattern));
    Place(fixture, 2000, PatternBytes(target::input::kHandlePdxEvents2.pattern));
    Place(fixture, 2400, PatternBytes(target::input::kMoveLeft.pattern));
    Place(fixture, 2800, PatternBytes(target::input::kMoveRight.pattern));
    return fixture;
}

void DefineRequiredSymbols(ByteBufferMemory &memory) {
    const std::array<const char *, 16> symbols{{
        target::symbols::kCStringAppendChar,
        target::symbols::kCStringAppendCString,
        target::symbols::kCStringRemoveSpecialCharacters,
        target::symbols::kEu4LoadGameHelperLoad,
        target::symbols::kConfirmSaveConstructor,
        target::symbols::kConfirmLocalDeleteConstructor,
        target::symbols::kToUpper,
        target::symbols::kDlcManagerAccessInstance,
        target::symbols::kCommandLineHasOption,
        target::symbols::kTextInputEventConstructor,
        target::symbols::kInputEventConstructor,
        target::symbols::kInputEventDestructor,
        target::symbols::kTextBufferEnterBackspace,
        target::symbols::kTextBufferCursorPosition,
        target::symbols::kTextBufferMoveLeft,
        target::symbols::kTextBufferMoveRight,
    }};
    std::uint64_t address = memory.BaseAddress() + 3500;
    for (const char *symbol : symbols) {
        memory.DefineSymbol(symbol, address++);
    }
}

ByteBufferMemory SupportedMemory(const std::string &version = "EU4 v1.37.5.0 Inca") {
    ByteBufferMemory memory(SupportedFixture(version));
    DefineRequiredSymbols(memory);
    return memory;
}

void TestSupportedProfileIsReadOnly() {
    auto memory = SupportedMemory();
    const auto before = memory.Bytes();
    const auto result = target::ValidateTarget(memory);
    Require(result.supported, "synthetic fixed profile passes");
    Require(result.check == "complete", "successful profile reports complete check");
    Require(result.versionText == "EU4 v1.37.5.0 Inca", "version text is retained");
    Require(memory.Bytes() == before, "target validation performs no mutation");
}

void TestVersionSeriesFailure() {
    auto memory = SupportedMemory("EU4 v1.38.0.0 Inca");
    const auto before = memory.Bytes();
    const auto result = target::ValidateTarget(memory);
    Require(!result, "unsupported fourth version component fails");
    Require(result.check == "version-text.series", "version failure identifies series check");
    Require(memory.Bytes() == before, "version failure performs no mutation");
}

void TestCompatiblePatchVersions() {
    auto version374 = SupportedMemory("EU4 v1.37.4.0 Inca");
    Require(target::ValidateTarget(version374).supported,
            "EU4 1.37.4 must be accepted when all capability facts pass");
    auto future137 = SupportedMemory("EU4 v1.37.99.7 Future");
    Require(target::ValidateTarget(future137).supported,
            "EU4 1.37 patch/build/codename must not be hard-coded");
}

void TestRepeatedExactVersionMarkersAreAllowed() {
    auto bytes = SupportedFixture();
    const std::string duplicate = "EU4 v1.37.5.0 Inca";
    Place(bytes, 3200, std::vector<std::uint8_t>(duplicate.begin(), duplicate.end()));
    bytes[3200 + duplicate.size()] = 0;
    ByteBufferMemory memory(std::move(bytes));
    DefineRequiredSymbols(memory);
    const auto result = target::ValidateTarget(memory);
    Require(result.supported,
            "repeated exact version display strings must not reject the fixed target");
}

void TestArchitectureFailure() {
    auto bytes = SupportedFixture();
    const std::uint32_t arm64 = 0x0100000C;
    std::memcpy(bytes.data() + sizeof(std::uint32_t), &arm64, sizeof(arm64));
    ByteBufferMemory memory(std::move(bytes));
    DefineRequiredSymbols(memory);
    const auto before = memory.Bytes();
    const auto result = target::ValidateTarget(memory);
    Require(!result, "non-x86-64 image fails");
    Require(result.check == "mach-header.cpu-type", "architecture failure identifies check");
    Require(memory.Bytes() == before, "architecture failure performs no mutation");
}

void TestMach64Failure() {
    auto bytes = SupportedFixture();
    const std::uint32_t mach32 = 0xFEEDFACE;
    std::memcpy(bytes.data(), &mach32, sizeof(mach32));
    ByteBufferMemory memory(std::move(bytes));
    DefineRequiredSymbols(memory);
    const auto before = memory.Bytes();
    const auto result = target::ValidateTarget(memory);
    Require(!result, "non-64-bit Mach-O image fails");
    Require(result.check == "mach-header.magic", "Mach-O magic failure identifies check");
    Require(memory.Bytes() == before, "Mach-O magic failure performs no mutation");
}

void TestExecutableFileTypeFailure() {
    auto bytes = SupportedFixture();
    const std::uint32_t dylibFileType = 6;
    std::memcpy(bytes.data() + 3 * sizeof(std::uint32_t), &dylibFileType,
                sizeof(dylibFileType));
    ByteBufferMemory memory(std::move(bytes));
    DefineRequiredSymbols(memory);
    const auto before = memory.Bytes();
    const auto result = target::ValidateTarget(memory);
    Require(!result, "non-executable Mach-O image fails");
    Require(result.check == "mach-header.file-type", "file type failure identifies check");
    Require(memory.Bytes() == before, "file type failure performs no mutation");
}

void TestAmbiguousFingerprintFailure() {
    auto bytes = SupportedFixture();
    Place(bytes, 512, PatternBytes(target::input::kHandleKeyEvent1.pattern));
    ByteBufferMemory memory(std::move(bytes));
    DefineRequiredSymbols(memory);
    const auto before = memory.Bytes();
    const auto result = target::ValidatePatchFacts(memory, "EU4 v1.37.5.0 Inca");
    Require(!result, "duplicate target fingerprint fails");
    Require(result.check == "fingerprint.input-handle-key",
            "ambiguous fingerprint identifies its check");
    Require(result.message.find("match_count=2") != std::string::npos,
            "ambiguous diagnostic includes match count");
    Require(memory.Bytes() == before, "ambiguous fingerprint failure performs no mutation");
}

void TestOriginalBytesFailure() {
    auto bytes = SupportedFixture();
    bytes[384] = 0x48;
    ByteBufferMemory memory(std::move(bytes));
    DefineRequiredSymbols(memory);
    const auto before = memory.Bytes();
    const auto result = target::ValidatePatchFacts(memory, "EU4 v1.37.5.0 Inca");
    Require(!result, "wrong original bytes fail");
    Require(result.check == "fingerprint.input-handle-key.original-bytes",
            "original-byte failure identifies its check");
    Require(memory.Bytes() == before, "original-byte failure performs no mutation");
}

template <std::size_t Size>
void RequireHookFixture(const std::vector<std::uint8_t> &fixture,
                        std::size_t offset, const target::HookSite &site,
                        const std::array<std::uint8_t, Size> &original,
                        std::size_t width, const char *message) {
    const auto pattern = PatternBytes(site.pattern);
    Require(std::equal(pattern.begin(), pattern.end(), fixture.begin() + offset), message);
    Require(original.size() == width, "hook original bytes cover overwrite width");
    Require(std::equal(original.begin(), original.end(),
                       fixture.begin() + offset + site.mutationOffset),
            "hook fixture contains expected original bytes at mutation");
    std::size_t count = 0;
    for (std::size_t candidate = 0;
         candidate + pattern.size() <= fixture.size(); ++candidate) {
        if (std::equal(pattern.begin(), pattern.end(), fixture.begin() + candidate))
            ++count;
    }
    Require(count == 1, "hook fixture pattern is unique");
}

void TestAllInputHookFixtures() {
    const auto fixture = SupportedFixture();
    RequireHookFixture(fixture, 1600, target::input::kHandlePdxEvents1,
                       target::input::kHandlePdxEvents1Original,
                       target::input::kJumpOverwriteWidth, "0x303 fixture pattern matches");
    RequireHookFixture(fixture, 384, target::input::kHandleKeyEvent1,
                       target::input::kHandleKeyEvent1Original,
                       target::input::kJumpOverwriteWidth, "backspace fixture pattern matches");
    RequireHookFixture(fixture, 2000, target::input::kHandlePdxEvents2,
                       target::input::kHandlePdxEvents2Original,
                       target::input::kJumpOverwriteWidth, "0x302 fixture pattern matches");
    RequireHookFixture(fixture, 2400, target::input::kMoveLeft,
                       target::input::kMoveLeftOriginal,
                       target::input::kCallOverwriteWidth, "left fixture pattern matches");
    RequireHookFixture(fixture, 2800, target::input::kMoveRight,
                       target::input::kMoveRightOriginal,
                       target::input::kCallOverwriteWidth, "right fixture pattern matches");
}

void TestProfileIdentityDoesNotDependOnSymbols() {
    ByteBufferMemory memory(SupportedFixture());
    const auto before = memory.Bytes();
    const auto result = target::ValidateTarget(memory);
    Require(result.supported,
            "profile identity must not use non-version symbol fingerprints");
    Require(memory.Bytes() == before, "profile validation performs no mutation");
}

} // namespace

int main() {
    TestSupportedProfileIsReadOnly();
    TestVersionSeriesFailure();
    TestCompatiblePatchVersions();
    TestRepeatedExactVersionMarkersAreAllowed();
    TestArchitectureFailure();
    TestMach64Failure();
    TestExecutableFileTypeFailure();
    TestAmbiguousFingerprintFailure();
    TestOriginalBytesFailure();
    TestAllInputHookFixtures();
    TestProfileIdentityDoesNotDependOnSymbols();
    std::cout << "macOS target profile tests passed\n";
}
