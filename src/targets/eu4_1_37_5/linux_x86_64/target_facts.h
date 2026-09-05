#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64 {

inline constexpr char kTargetId[] = "linux_x86_64";
inline constexpr char kDiagnosticTargetId[] = "eu4-1.37-linux-x86_64";
inline constexpr char kGameVersionShort[] = "1.37.5";
inline constexpr char kExpectedVersionText[] = "EU4 v1.37.5.0 Inca";
inline constexpr char kSupportedElfSha256Hex[] =
    "af115d3b0e54a05eca0198ed569db90ca225728afda03b5ac4ded251520a7ce3";

// Calibrated Linux target facts. Migrated from the validated
// name8102/eu4dll_linux@linux-port prototype and re-verified through
// fixture preflight plus real-ELF validation. Linux object layout is
// intentionally not shared with the macOS target.
namespace executable {
inline constexpr std::uint8_t kElfMagic[] = {0x7F, 'E', 'L', 'F'};
inline constexpr std::uint8_t kElfClass64 = 2;
inline constexpr std::uint16_t kElfMachineX86_64 = 62;
inline constexpr std::size_t kMaximumVersionText = 96;
}  // namespace executable

namespace symbols {
inline constexpr char kReadGameSpecific[] =
    "_ZN12CEU3Graphics16ReadGameSpecificER7CReaderiRP13C2dObjectTypeRP10CPdx3DTypeRP11CBitmapFont";
inline constexpr char kParseFontFile[] = "_ZN11CBitmapFont13ParseFontFileEv";
inline constexpr char kLoadTexture[] =
    "_ZN15CTextureHandler11LoadTextureERK7CStringRiRK20SLoadTextureSettingsi";
inline constexpr std::array<const char *, 3> kRequiredSymbols{{
    kReadGameSpecific,
    kParseFontFile,
    kLoadTexture,
}};
}  // namespace symbols

namespace base {
// Expanded bitmap-font allocation replaces the legacy 0x3560 request.
inline constexpr std::size_t kExpandedBitmapFontSize = 0x86ac0;
// Extended glyphs shift the character index by this amount.
inline constexpr std::uint32_t kCharacterIndexShift = 0x6ac;
// Linux glyph-table/object layout differs from macOS; observed table stride
// elsewhere in the legacy port.
inline constexpr std::ptrdiff_t kGlyphTableOffset = 0x100;

// 1. CEU3Graphics::ReadGameSpecific bitmap-font allocation call.
inline constexpr char kAllocateFontPattern[] = "4D 89 CC BF 60 35 00 00 E8 ? ? ? ?";
inline constexpr std::size_t kAllocateFontSearchSize = 0xaf;
inline constexpr std::ptrdiff_t kAllocateFontMutationOffset = 8;
inline constexpr std::array<std::uint8_t, 5> kAllocateFontOriginal{{0xE8, 0xA4, 0x02, 0x11, 0xFF}};

// 2. CBitmapFont::ParseFontFile character-limit byte.
inline constexpr char kCharacterLimitPattern[] = "41 81 FD FF 00 00 00 0F 87 ? ? ? ?";
inline constexpr std::size_t kParseFontFileSearchSize = 0xa20;
inline constexpr std::ptrdiff_t kCharacterLimitMutationOffset = 4;
inline constexpr std::uint8_t kCharacterLimitOriginal = 0x00;
inline constexpr std::uint8_t kCharacterLimitReplacement = 0xFF;

// 3. CBitmapFont::ParseFontFile character-index hook (naked, see ABI_NOTES.md).
inline constexpr char kCharacterIndexPattern[] =
    "44 89 E9 48 8B 44 24 08 48 83 BC C8 00 01 00 00 00";
inline constexpr std::array<std::uint8_t, 8> kCharacterIndexOriginal{{
    0x44, 0x89, 0xE9, 0x48, 0x8B, 0x44, 0x24, 0x08}};
inline constexpr std::ptrdiff_t kCharacterIndexContinuationOffset = 8;

// 4. CTextureHandler::LoadTexture texture-size-limit byte (16 MiB -> 64 MiB).
inline constexpr char kTextureSizePattern[] = "81 FB 00 00 00 01 72 19";
inline constexpr std::size_t kLoadTextureSearchSize = 0x5e6;
inline constexpr std::ptrdiff_t kTextureSizeMutationOffset = 5;
inline constexpr std::uint8_t kTextureSizeOriginal = 0x01;
inline constexpr std::uint8_t kTextureSizeReplacement = 0x04;
}  // namespace base

}  // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64
