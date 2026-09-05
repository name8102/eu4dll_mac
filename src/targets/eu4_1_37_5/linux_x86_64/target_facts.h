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

// Text-layout (glyph measurement) facts. Calibrated against the Linux
// 1.37.5 ELF; register/stack contracts live with the naked hooks in
// text_layout/layout_patch.cpp and text_layout/ABI_NOTES.md. The Linux
// glyph-table base (0x100) and index shift (0x6ac) are intentionally NOT
// the macOS values (0xE8 / 0x6B0).
namespace layout {
inline constexpr std::ptrdiff_t kGlyphTableOffset = 0x100;

inline constexpr char kGetHeightOfStringSymbol[] =
    "_ZNK11CBitmapFont17GetHeightOfStringERK7CStringiiRK8CVector2IiEb";
inline constexpr std::size_t kGetHeightOfStringSearchSize = 0x3f4;
inline constexpr char kGetHeightOfStringPattern[] =
    "0F B6 00 48 8B 84 C3 00 01 00 00 48 85 C0";
inline constexpr std::array<std::uint8_t, 11> kGetHeightOfStringOriginal{{
    0x0F, 0xB6, 0x00, 0x48, 0x8B, 0x84, 0xC3, 0x00, 0x01, 0x00, 0x00}};
inline constexpr std::ptrdiff_t kGetHeightOfStringContinuationOffset = 11;

inline constexpr char kGetWidthOfStringSymbol[] =
    "_ZN11CBitmapFont16GetWidthOfStringEPKcib";
inline constexpr std::size_t kGetWidthOfStringSearchSize = 0x257;
inline constexpr char kGetWidthOfStringPattern[] =
    "48 8B AC F7 00 01 00 00 48 85 ED";
inline constexpr std::array<std::uint8_t, 8> kGetWidthOfStringOriginal{{
    0x48, 0x8B, 0xAC, 0xF7, 0x00, 0x01, 0x00, 0x00}};
inline constexpr std::ptrdiff_t kGetWidthOfStringContinuationOffset = 8;
inline constexpr std::ptrdiff_t kGetWidthOfStringBypassOffset = 0x187;

inline constexpr char kGetActualRequiredSizeSymbol[] =
    "_ZNK11CBitmapFont21GetActualRequiredSizeERK7CStringiiR8CVector2IjERS3_IiEb";
inline constexpr std::size_t kGetActualRequiredSizeSearchSize = 0x618;
inline constexpr char kGetActualRequiredSizePattern[] =
    "0F B6 00 48 8B 4C 24 28 48 8B AC C1 00 01 00 00 48 85 ED";
inline constexpr std::array<std::uint8_t, 8> kGetActualRequiredSizeOriginal{{
    0x0F, 0xB6, 0x00, 0x48, 0x8B, 0x4C, 0x24, 0x28}};
inline constexpr std::ptrdiff_t kGetActualRequiredSizeContinuationOffset = 8;

inline constexpr char kGetRequiredSizeSymbol[] =
    "_ZNK11CBitmapFont15GetRequiredSizeERK7CStringRS0_iiR8CVector2IjEb";
inline constexpr std::size_t kGetRequiredSizeSearchSize = 0x7bc;
inline constexpr char kGetRequiredSizePattern[] =
    "0F B6 00 49 8B AC C5 00 01 00 00 48 85 ED";
inline constexpr std::array<std::uint8_t, 11> kGetRequiredSizeOriginal{{
    0x0F, 0xB6, 0x00, 0x49, 0x8B, 0xAC, 0xC5, 0x00, 0x01, 0x00, 0x00}};
inline constexpr std::ptrdiff_t kGetRequiredSizeContinuationOffset = 11;

inline constexpr char kGetActualRealRequiredSizeActuallySymbol[] =
    "_ZNK11CBitmapFont33GetActualRealRequiredSizeActuallyERK7CStringRS0_iiR8CVector2IjEbbbPiPb";
inline constexpr std::size_t kGetActualRealRequiredSizeActuallySearchSize = 0xcce;
inline constexpr char kGetActualRealRequiredSizeActuallyPattern[] =
    "0F B6 00 49 8B AC C7 00 01 00 00 48 85 ED";
inline constexpr std::array<std::uint8_t, 11>
    kGetActualRealRequiredSizeActuallyOriginal{{
        0x0F, 0xB6, 0x00, 0x49, 0x8B, 0xAC, 0xC7, 0x00, 0x01, 0x00, 0x00}};
inline constexpr std::ptrdiff_t kGetActualRealRequiredSizeActuallyContinuationOffset = 11;

// Wrapping gate inside GetActualRequiredSize: force the wrap branch.
inline constexpr char kWrappingGatePattern[] =
    "0F BF 45 06 0F 57 C9 F3 0F 2A C8 F3 0F 59 D9 "
    "0F 2E 1C 25 ? ? ? ? 48 8B 54 24 18 75 21 7A 1F "
    "0F 2E 54 24 34";
inline constexpr std::array<std::uint8_t, 4> kWrappingGateOriginal{{
    0x0F, 0xBF, 0x45, 0x06}};
inline constexpr std::array<std::uint8_t, 4> kWrappingGateReplacement{{
    0xEB, 0x15, 0x90, 0x90}};
}  // namespace layout

// Main-text (RenderToScreen) facts. Three jump hooks inside one function
// (bound 0x218b); the preprocessing hook stages decoded bytes into an
// absolute game buffer (ET_EXEC target, address stable) and publishes the
// current character for the wrapping hook. No tooltip/map/input facts here.
namespace main_text {
inline constexpr char kRenderToScreenSymbol[] =
    "_ZN11CBitmapFont14RenderToScreenERK7CString14FontFormatting22EFontVerticalAlignment"
    "15EGuiOrientationPK8CMatrix4IfE5CRectIiE8CVector2IiEPSD_ib";
inline constexpr std::size_t kRenderToScreenSearchSize = 0x218b;
// Absolute game-data staging buffer (non-PIE executable: stable address).
inline constexpr std::uint32_t kPreprocessingStageAddress = 0x3345191;
// Drawing-loop object base: [rbx + disp32].
inline constexpr std::uint32_t kDrawingObjectDisplacement = 0x333d450;

inline constexpr char kPreprocessingPattern[] =
    "41 0F B6 0C 2E 80 BC 24 18 22 00 00 00 74 4F";
inline constexpr std::array<std::uint8_t, 5> kPreprocessingOriginal{{
    0x41, 0x0F, 0xB6, 0x0C, 0x2E}};
inline constexpr std::ptrdiff_t kPreprocessingContinuationOffset = 5;
inline constexpr std::ptrdiff_t kPreprocessingBypassOffset = 0x5e;

inline constexpr char kWrappingPattern[] =
    "66 83 7D 06 00 0F 84 ? ? ? ? 80 3D ? ? ? ? 00";
inline constexpr std::array<std::uint8_t, 5> kWrappingOriginal{{
    0x66, 0x83, 0x7D, 0x06, 0x00}};
inline constexpr std::ptrdiff_t kWrappingContinuationOffset = 5;
inline constexpr std::ptrdiff_t kWrappingBypassOffset = 0x191;

inline constexpr char kDrawingPattern[] =
    "0F B6 83 50 D4 33 03 80 BC 24 18 22 00 00 00 0F 84";
inline constexpr std::array<std::uint8_t, 7> kDrawingOriginal{{
    0x0F, 0xB6, 0x83, 0x50, 0xD4, 0x33, 0x03}};
inline constexpr std::ptrdiff_t kDrawingContinuationOffset = 7;
inline constexpr std::ptrdiff_t kDrawingBypassOffset = 0x1aa;
}  // namespace main_text

// Tooltip/button (RenderToTexture) facts. Three jump hooks inside one
// function (bound 0x311c) plus a dlsym-resolved CString::operator+=(char)
// used by the preprocessing hook. Only the wrapping site has a bypass;
// preprocessing/drawing always rejoin at their single return.
namespace tooltip {
inline constexpr char kRenderToTextureSymbol[] =
    "_ZN11CBitmapFont15RenderToTextureEP21GfxDeferredContextGFXP10TextureGFXR8CVector2IiES6_"
    "RK7CStringS3_jjRKS4_IjE14FontFormattingbjbb";
inline constexpr char kCStringAppendCharSymbol[] = "_ZN7CStringpLEc";
inline constexpr std::size_t kRenderToTextureSearchSize = 0x311c;

inline constexpr char kPreprocessingPattern[] =
    "0F B6 00 48 8B 4C 24 38 48 8B AC C1 00 01 00 00 48 85 ED";
inline constexpr std::array<std::uint8_t, 8> kPreprocessingOriginal{{
    0x0F, 0xB6, 0x00, 0x48, 0x8B, 0x4C, 0x24, 0x38}};
inline constexpr std::ptrdiff_t kPreprocessingContinuationOffset = 16;

inline constexpr char kWrappingPattern[] =
    "66 83 7D 06 00 74 0D 40 8A AC 24 00 29 00 00";
inline constexpr std::array<std::uint8_t, 5> kWrappingOriginal{{
    0x66, 0x83, 0x7D, 0x06, 0x00}};
inline constexpr std::ptrdiff_t kWrappingContinuationOffset = 5;
inline constexpr std::ptrdiff_t kWrappingBypassOffset = 0x14;

inline constexpr char kDrawingPattern[] =
    "0F B6 00 4D 8B 9C C5 00 01 00 00 4D 85 DB";
inline constexpr std::array<std::uint8_t, 11> kDrawingOriginal{{
    0x0F, 0xB6, 0x00, 0x4D, 0x8B, 0x9C, 0xC5, 0x00, 0x01, 0x00, 0x00}};
inline constexpr std::ptrdiff_t kDrawingContinuationOffset = 11;
}  // namespace tooltip

}  // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64
