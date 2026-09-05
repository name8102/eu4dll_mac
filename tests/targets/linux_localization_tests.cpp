#include "features/escaped_text/escaped_text.h"
#include "runtime/patch/byte_buffer_memory.h"
#include "runtime/patch/patch_batch.h"
#include "runtime/patch/patch_runtime.h"
#include "targets/eu4_1_37_5/linux_x86_64/localization_text/localization_patch.h"
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
using eu4dll::patch::Address;
using eu4dll::patch::ByteBufferMemory;

namespace {

void Require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

ByteBufferMemory BuildMemory() {
    std::vector<std::uint8_t> fixture(4096, 0xCC);
    fixture[0] = 0x7F;
    fixture[1] = 'E';
    fixture[2] = 'L';
    fixture[3] = 'F';
    fixture[4] = 2;
    fixture[18] = 62;
    const std::string version = target::kExpectedVersionText;
    std::copy(version.begin(), version.end(), fixture.begin() + 64);
    fixture[64 + version.size()] = 0;

    // BE A0 83 43 03 | E8 80 66 1E 00
    const std::vector<std::uint8_t> site{0xBE, 0xA0, 0x83, 0x43, 0x03,
                                         0xE8, 0x80, 0x66, 0x1E, 0x00};
    Require(512 + site.size() <= fixture.size(), "fixture placement in range");
    std::copy(site.begin(), site.end(), fixture.begin() + 512);

    ByteBufferMemory memory(std::move(fixture), 0x400000);
    memory.DefineSymbol(target::localization_utf8::kLocalizeYmlAddKeySymbol,
                        0x400000 + 256);
    return memory;
}

void TestDescriptionPreflights() {
    auto memory = BuildMemory();
    eu4dll::patch::PatchRuntime runtime(memory);
    auto description = target::localization_utf8::ValueConversionDescription(
        memory.BaseAddress() + 0x100);  // reachable dummy converter
    const auto preflight = runtime.Preflight(description);
    Require(static_cast<bool>(preflight),
            eu4dll::patch::FormatDiagnostic(preflight.diagnostic).c_str());
    Require(preflight.diagnostic.matchAddress == 0x400000 + 512,
            "value-conversion call site must be located");
}

void TestBatchCommitsAtomically() {
    auto memory = BuildMemory();
    eu4dll::patch::PatchBatch batch(memory, nullptr);
    batch.Add(target::localization_utf8::ValueConversionDescription(
        memory.BaseAddress() + 0x100));
    Require(static_cast<bool>(batch.Preflight()), "localization preflights cleanly");
    const auto result = batch.Commit();
    Require(static_cast<bool>(result), "localization commits atomically");
    Require(result.installations.size() == 1, "single redirect publishes one result");
    // The call opcode stays E8; only the rel32 operand is retargeted.
    Require(memory.Bytes()[517] == 0xE8, "call opcode must be preserved");
}

void TestExpectedMismatchWritesNothing() {
    auto memory = BuildMemory();
    std::vector<std::uint8_t> bad = memory.Bytes();
    bad[517 + 1] ^= 0xFF;
    ByteBufferMemory corrupted(std::move(bad), 0x400000);
    corrupted.DefineSymbol(target::localization_utf8::kLocalizeYmlAddKeySymbol,
                           0x400000 + 256);
    const auto before = corrupted.Bytes();
    eu4dll::patch::PatchBatch batch(corrupted, nullptr);
    batch.Add(target::localization_utf8::ValueConversionDescription(
        corrupted.BaseAddress() + 0x100));
    Require(!batch.Commit(), "stale call bytes must fail the redirect");
    Require(corrupted.Bytes() == before, "failed redirect writes nothing");
}

void TestConverterEncodesChinese() {
    // U+4E2D U+534E U+793C U+4EEA U+4E4B U+4E89 ("Zhonghua liyi zhi zheng").
    // Portable canonical form escapes the reserved low byte of U+793C as
    // `11 4A 79`; pre-escaped mod files may store the raw `10 3C 79` form.
    // Both decode identically in every hook, and round-trip is exact.
    const char *input = u8"中华礼仪之争";
    std::array<char, 32768> output{};
    target::localization_utf8::ConvertUtf8Localization(input, output.data());
    const std::string actual(output.data());
    const std::string expected =
        std::string("\x10\x2D\x4E\x10\x4E\x53\x11\x4A\x79\x10\xEA\x4E\x10\x4B\x4E"
                    "\x10\x89\x4E",
                    18);
    Require(actual == expected, "converter must emit the canonical escaped encoding");
    const auto back =
        eu4dll::escaped_text::escaped_to_utf8(actual, 1024);
    Require(back.text == input, "converter output must round-trip to the input");
}

void TestConverterIsNullTolerantAndBounded() {
    std::array<char, 32768> output{};
    std::memset(output.data(), 0x7F, output.size());
    target::localization_utf8::ConvertUtf8Localization(nullptr, output.data());
    Require(output[0] == '\0', "null input must produce empty output");
    target::localization_utf8::ConvertUtf8Localization("abc", nullptr);
    target::localization_utf8::ConvertUtf8Localization(nullptr, nullptr);
    std::memset(output.data(), 0x7F, output.size());
    target::localization_utf8::ConvertUtf8Localization("", output.data());
    Require(output[0] == '\0', "empty input must produce empty output");
    // Long ASCII input: bounded and NUL-terminated within the legacy limit.
    const std::string big(100000, 'x');
    target::localization_utf8::ConvertUtf8Localization(big.c_str(), output.data());
    Require(std::strlen(output.data()) <= 32767, "output must respect the legacy bound");
}

}  // namespace

int main() {
    TestDescriptionPreflights();
    TestBatchCommitsAtomically();
    TestExpectedMismatchWritesNothing();
    TestConverterEncodesChinese();
    TestConverterIsNullTolerantAndBounded();
    std::cout << "linux localization tests passed" << std::endl;
    return 0;
}
