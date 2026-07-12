#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace eu4dll::targets::eu4_1_37_5::macos_x86_64 {

inline constexpr char kTargetId[] = "macos_x86_64";
inline constexpr char kDiagnosticTargetId[] = "eu4-1.37-macos-x86_64";

struct HookSite {
    const char *pattern;
    std::ptrdiff_t mutationOffset = 0;
    std::ptrdiff_t continuationOffset = 0;
    std::ptrdiff_t bypassOffset = 0;
};

namespace executable {
inline constexpr std::uint32_t kMach64Magic = 0xFEEDFACF;
inline constexpr std::uint32_t kCpuTypeX86_64 = 0x01000007;
inline constexpr std::uint32_t kMachExecuteFileType = 2;
inline constexpr char kVersionPrefix[] = "EU4 v1.37.";
inline constexpr char kVersionPattern[] = "45 55 34 20 76 31 2E";
inline constexpr std::size_t kMaximumVersionText = 96;
} // namespace executable

namespace fingerprint {
inline constexpr char kInputHandleKeyPattern[] =
    "? ? ? ? ? FF 84 DB 74 6B FF 90 40 01 00 00";
inline constexpr std::array<std::uint8_t, 5> kInputHandleKeyOriginal{{
    0x49, 0x8B, 0x07, 0x4C, 0x89}};
inline constexpr char kMapCurveTextPattern[] =
    "? ? ? ? ? EF E8 ? ? ? ? 41 89 C7 4C 89 EF E8 ? ? ? ? 48 89 85 70 FF FF FF";
inline constexpr std::array<std::uint8_t, 5> kMapCurveTextOriginal{{
    0x41, 0x89, 0xC6, 0x4C, 0x89}};
inline constexpr char kMonarchNamePattern[] =
    "? ? ? ? ? 8D 35 ? ? ? ? 48 8D 7D D0 E8 ? ? ? ? 49 83 C7 08 48 8D 7D D0 4C 89 FE";
inline constexpr std::array<std::uint8_t, 5> kMonarchNameOriginal{{
    0x4C, 0x8B, 0x7B, 0x58, 0x48}};
} // namespace fingerprint

namespace symbols {
inline constexpr char kCStringAppendChar[] = "_ZN7CStringpLEc";
inline constexpr char kCStringAppendCString[] = "_ZN7CStringpLEPKc";
inline constexpr char kCStringRemoveSpecialCharacters[] = "_ZN7CString23RemoveSpecialCharactersEv";
inline constexpr char kEu4LoadGameHelperLoad[] = "_ZN17EU4LoadGameHelper4LoadERNS_19SLoadGameParametersE";
inline constexpr char kConfirmSaveConstructor[] = "_ZN12CConfirmSaveC2EP9CEU3IdlerRK7CStringP21CloudFileCLOUDSTORAGEbbRKNSt3__16vectorIS2_NS7_9allocatorIS2_EEEE";
inline constexpr char kConfirmLocalDeleteConstructor[] = "_ZN25CConfirmLocalDeleteInGameC2EP9CEU3IdlerRK7CStringS4_RKNSt3__18functionIFvvEEE";
inline constexpr char kToUpper[] = "__toupper";
inline constexpr char kDlcManagerAccessInstance[] = "_ZN11CDLCManager14AccessInstanceEv";
inline constexpr char kCommandLineHasOption[] = "_ZNK15CPdxCommandLine9HasOptionEPKc";
inline constexpr char kTextInputEventConstructor[] = "_ZN15CTextInputEventC1Ec";
inline constexpr char kInputEventConstructor[] = "_ZN11CInputEventC1ERK15CTextInputEvent";
inline constexpr char kInputEventDestructor[] = "_ZN11CInputEventD1Ev";
inline constexpr char kTextBufferEnterBackspace[] = "_ZN11CTextBuffer14EnterBackspaceEv";
inline constexpr char kTextBufferCursorPosition[] = "_ZN11CTextBuffer25GetCursorPositionInStringEv";
inline constexpr char kTextBufferMoveLeft[] = "_ZN11CTextBuffer8MoveLeftEv";
inline constexpr char kTextBufferMoveRight[] = "_ZN11CTextBuffer9MoveRightEv";
inline constexpr std::array<const char *, 16> kRequiredSymbols{{
    kCStringAppendChar,
    kCStringAppendCString,
    kCStringRemoveSpecialCharacters,
    kEu4LoadGameHelperLoad,
    kConfirmSaveConstructor,
    kConfirmLocalDeleteConstructor,
    kToUpper,
    kDlcManagerAccessInstance,
    kCommandLineHasOption,
    kTextInputEventConstructor,
    kInputEventConstructor,
    kInputEventDestructor,
    kTextBufferEnterBackspace,
    kTextBufferCursorPosition,
    kTextBufferMoveLeft,
    kTextBufferMoveRight,
}};
} // namespace symbols

namespace base {
inline constexpr std::size_t kOriginalGraphicsAllocationSize = 0x3538;
inline constexpr std::size_t kExpandedGraphicsAllocationSize = 0x86AC8;
inline constexpr std::ptrdiff_t kGlyphTableOffset = 0xE8;
inline constexpr std::size_t kGlyphPointerStride = 8;
inline constexpr std::ptrdiff_t kBitmapCharacterLineBreakOffset = 6;
inline constexpr HookSite kAllocateFont{"BF 38 35 00 00", 5};
inline constexpr std::ptrdiff_t kAllocateFontExpectedCallOffset = 5;
inline constexpr std::array<std::uint8_t, 5> kAllocateFontOriginal{{0xE8, 0, 0, 0, 0}};
inline constexpr std::array<std::uint8_t, 5> kRelativeCallMask{{0xFF, 0, 0, 0, 0}};
inline constexpr HookSite kAllowWideGlyphs{"41 81 FE FF 00 00 00 0F 87 F8 01 00 00", 4};
inline constexpr std::array<std::uint8_t, 1> kAllowWideGlyphsOriginal{{0x00}};
inline constexpr HookSite kWideGlyphOffset{"44 89 F1 48 8B 85 F0 F2 FF FF", 0, 10};
inline constexpr std::array<std::uint8_t, 5> kWideGlyphOffsetOriginal{{
    0x44, 0x89, 0xF1, 0x48, 0x8B}};
inline constexpr HookSite kTextureSizeLimit1{"89 4D CC 81 FB FF FF FF 00", 8};
inline constexpr HookSite kTextureSizeLimit2{"41 81 FC FF FF FF 00 76", 6};
inline constexpr std::uint8_t kExpandedTextureLimitByte = 0x03;
} // namespace base

namespace rendering {
inline constexpr std::uint32_t kMissingFontGlyph = 0x98F;
inline constexpr std::uint32_t kUndefinedGlyph = 0x2026;
} // namespace rendering

namespace main_text {
inline constexpr HookSite kRenderToScreen1{"42 0F B6 14 1F 80 7D 38 00", 0, 5, 0x57};
inline constexpr HookSite kRenderToScreen2{"66 41 83 7E 06 00 0F 84", 0, 6, 0x297};
inline constexpr HookSite kRenderToScreen3{"41 0F B6 04 04 80 7D 38 00", 0, 5, 0x1B4};
} // namespace main_text

namespace texture_text {
inline constexpr HookSite kRenderToTexture1{"0F B6 00 49 8B 9C C7 E8 00 00 00", 0, 0, 0xB};
inline constexpr HookSite kRenderToTexture2{"66 83 7B 06 00 74 08 4D 89 EE", 0, 5, 0xF};
inline constexpr HookSite kRenderToTexture3{"0F B6 00 4D 8B 94 C6 E8 00 00 00", 0, 0, 0xB};
} // namespace texture_text

namespace text_3d {
inline constexpr HookSite kRender1{"44 89 E6 E8 ? ? ? ? 0F B6 00 49 8B 84 C7 E8 00 00 00 48 85 C0 0F 84", 8, 0, 0x13};
inline constexpr HookSite kRender2{"0F B6 00 48 8B 8D 10 FF FF FF 48 8B 9C C1 E8", 0, 0, 0xA};
} // namespace text_3d

namespace text_layout {
inline constexpr HookSite kHeight{"0F B6 00 48 8B 84 C3 E8 00 00 00 48 85 C0", 0, 0, 0xB};
inline constexpr HookSite kWidth{"48 8B 9C F7 E8 00 00 00 48 85 DB", 0, 8, 0x1C6};
inline constexpr HookSite kRequiredSize{"0F B6 00 49 8B 9C C4 E8 00 00 00 48 85 DB", 0, 0, 0xB};
inline constexpr HookSite kActualRealRequiredSize1{"0F B6 00 49 8B 9C C5 E8 00 00 00 48 85 DB", 0, 0, 0xB};
inline constexpr HookSite kActualRequiredSize{"0F B6 00 48 8B 8D D8 EE FF FF", 0, 0, 0xA};
inline constexpr HookSite kActualRequiredSizeCall{"41 0F BF 44 24 06 0F 57 C9 F3 0F 2A C8 F3 0F 59 D9 0F 2E 1D ? ? ? ? 75 37 7A 35", 0};
inline constexpr std::uint8_t kForceWrapBytes[] = {0xEB, 0x1A, 0x90, 0x90, 0x90, 0x90};
inline constexpr HookSite kActualRealRequiredSize2{"8B 9D 4C FF FF FF FF C3 89 9D 4C FF FF FF 4C 89 E7", 0, 0, 6};
inline constexpr HookSite kActualRealRequiredSize3{"48 83 BD E8 FE FF FF 05 0F 82 ? ? ? ? 48 8B 85 C8 FE FF FF", 0x15, 0, 0x1C};
inline constexpr HookSite kActualRealRequiredSizeBranch{"48 83 BD E8 FE FF FF 05 0F 82 ? ? ? ? 48 8B 85 C8 FE FF FF", 8};
inline constexpr std::uint8_t kDisableTruncationBytes[] = {0x90, 0xE9};
} // namespace text_layout

namespace map_text {
inline constexpr HookSite kAddNameArea1{"43 8A 04 37 88 85 28 FF FF FF", 4, 0x76, 0x20};
inline constexpr HookSite kAddNameArea2{"FF FF FF E8 ? ? ? ? 31 C0 4C 8D 85 B8 FD FF FF", 3};
inline constexpr HookSite kAddNameArea3{"0F B6 00 49 8B 84 C6 E8 00 00 00 48 85 C0 74 0D", 0, 0, 0xB};
inline constexpr HookSite kFillVertexBuffer1{"0F B6 00 49 8B 84 C7 E8 00 00 00 48 85 C0 0F 84 46 03 00 00", 0, 0, 0xB};
inline constexpr HookSite kFillVertexBuffer2{"0F B6 00 4D 8B AC C7 E8 00 00 00 4D 85 ED", 0, 0, 0xB};
inline constexpr HookSite kCurveText1{"0F B6 00 4D 8B 3C C4 4D 85 FF", 0, 0, 7};
inline constexpr HookSite kCurveText2{"F3 41 0F 2A CE 41 BE 00 00 00 00", 5, 0, 11};
inline constexpr HookSite kCurveText3{"F3 0F 11 45 CC 4C 89 EF E8", 5, 0, 13};
inline constexpr HookSite kCurveText4{"41 89 C6 4C 89 EF E8 ? ? ? ? 41 89 C7 4C 89 EF E8 ? ? ? ? 48 89 85 70 FF FF FF", 6};
inline constexpr std::ptrdiff_t kCurveText4SecondCallOffset = 17;
inline constexpr HookSite kAddNudgedNames{"44 89 EE E8 ? ? ? ? 0F B6 00 49 8B 84 C6 E8 00 00 00 48 85 C0 74", 8, 0, 0x13};
} // namespace map_text

namespace save_filename {
inline constexpr std::uint8_t kRemoveSpecialCharactersBytes[] = {0xC3, 0x90, 0x90, 0x90};
inline constexpr HookSite kSaveGame{"48 89 DF E8 ? ? ? ? 48 8D BD 60 FF FF FF 48 8D 75 A8", 3, 8};
inline constexpr HookSite kLocalSavegameItemConstructor{"49 89 C7 4D 85 F6 74 ? 4C 89 F7 4C 89 E6 31 D2 E8", 8, 14};
inline constexpr HookSite kConfirmSave{"49 8B 7C 24 08 48 8B 07 48 8D 35 ? ? ? ? FF 90 C0 00 00 00 48 89 C3 48 8D 35 S S S S 48 8D 15", 0, 5};
inline constexpr char kConfirmSaveText[] = "CONFIRMSAVETEXT";
inline constexpr std::size_t kConstructorSearchSize = 1024;
inline constexpr HookSite kUpdateHeaderInfo{"48 8D B0 20 02 00 00 48 8D 7D 90 E8", 0, 7};
inline constexpr std::ptrdiff_t kSaveHeaderFilenameOffset = 0x220;
inline constexpr HookSite kDoLoadGame{"48 8D 7D A8 E8 ? ? ? ? 48 8D 7D A8 E8 ? ? ? ? 48 8B 7B 30 E8", 9, 18};
inline constexpr HookSite kGetCurrentTooltip{"48 8D BD 50 FF FF FF 48 8D B5 20 FD FF FF E8", 0, 0xE};
inline constexpr HookSite kConfirmLocalDelete{"48 8B 7B 08 48 8B 07 48 8D 35 ? ? ? ? FF 90 C0 00 00 00 48 89 C3 48 8D 35 S S S S 48 8D 15 ? ? ? ? 48 8D 7D A0 4C 89 E1", 0, 7};
inline constexpr char kConfirmDeleteText[] = "CONFIRMDELETETEXT";
} // namespace save_filename

namespace input {
inline constexpr HookSite kHandlePdxEvents1{"8A 5D A4 84 DB 0F 89 ? ? ? ? 80 FB DF", 0, 0x325};
inline constexpr std::array<std::uint8_t, 5> kHandlePdxEvents1Original{{0x8A, 0x5D, 0xA4, 0x84, 0xDB}};
inline constexpr HookSite kHandleKeyEvent1{"49 8B 07 4C 89 FF 84 DB 74 6B FF 90 40 01 00 00", 0, 6, 0x10};
inline constexpr std::array<std::uint8_t, 5> kHandleKeyEvent1Original{{0x49, 0x8B, 0x07, 0x4C, 0x89}};
inline constexpr HookSite kHandlePdxEvents2{"3D 01 03 00 00 0F 84 ? ? ? ? 3D 03 03 00 00", 0, 5};
inline constexpr std::array<std::uint8_t, 5> kHandlePdxEvents2Original{{0x3D, 0x01, 0x03, 0x00, 0x00}};
inline constexpr HookSite kMoveLeft{"49 8B 07 4C 89 FF FF 90 D8 00 00 00 E9", 6, 12};
inline constexpr std::array<std::uint8_t, 6> kMoveLeftOriginal{{0xFF, 0x90, 0xD8, 0x00, 0x00, 0x00}};
inline constexpr HookSite kMoveRight{"49 8B 07 4C 89 FF FF 90 E8 00 00 00 E9", 6, 12};
inline constexpr std::array<std::uint8_t, 6> kMoveRightOriginal{{0xFF, 0x90, 0xE8, 0x00, 0x00, 0x00}};
inline constexpr std::size_t kJumpOverwriteWidth = 5;
inline constexpr std::size_t kCallOverwriteWidth = 6;
inline constexpr std::ptrdiff_t kTextBufferStringOffset = 0x30;
inline constexpr std::ptrdiff_t kTextBufferCursorOffset = 0x4C;
inline constexpr std::size_t kKeyboardPreCheckVtableOffset = 0x28;
inline constexpr std::size_t kEventHandlerDispatchVtableOffset = 0x20;
inline constexpr int kTextEditingEvent = 0x302;
inline constexpr int kTextInputEvent = 0x303;
inline constexpr int kKeyDownEvent = 0x301;
} // namespace input

namespace localization {
inline constexpr HookSite kDateFormat{"64 20 77 20 6D 77 20 77 20 79"};
inline constexpr std::uint8_t kDateFormatBytes[] =
    {0x79, 0x20, 0x0F, 0x20, 0x6D, 0x77, 0x20, 0x64, 0x20, 0x0E};
inline constexpr HookSite kLocalizeYml{"48 89 45 A0 4C 89 65 A8 4C 89 6D C0 48 8D 35 ? ? ? ? 48 89 DF 4C 89 EA", 0x19};
inline constexpr HookSite kGotoBoxProcess{"4D 6B E7 70 80 7D BC 00 74 21", 0, 0x1E, 0x76};
inline constexpr HookSite kMain{"48 8B BD 98 FA FF FF E8 ? ? ? ? 84 C0 0F 84", 7};
inline constexpr HookSite kMonarchFullName{"4C 8B 7B 58 48 8D 35 ? ? ? ? 48 8D 7D D0 E8 ? ? ? ? 49 83 C7 08 48 8D 7D D0 4C 89 FE", 0, 0x3F};
inline constexpr HookSite kCountryNewRepublicName{"74 0D 4C 89 F7 E8 ? ? ? ? E9 93 00", 5, 0xA2};
inline constexpr HookSite kCountryNewRepublicNameRandom{"48 8D 34 C1 4C 89 F7 E8 ? ? ? ? EB 5B", 7};
inline constexpr HookSite kCountryNewRepublicNameCulture{"48 83 C0 08 4C 89 F7 48 89 C6 E8 ? ? ? ? 48 8D 7D 80", 0xA};
inline constexpr std::ptrdiff_t kDlcManagerEnabledModsOffset = 0x70;
inline constexpr std::ptrdiff_t kMonarchCultureOffset = 0x50;
inline constexpr std::ptrdiff_t kCultureTagOffset = 0x38;
inline constexpr std::ptrdiff_t kCultureGroupOffset = 0x68;
inline constexpr std::ptrdiff_t kMonarchDynastyOffset = 0x58;
inline constexpr std::ptrdiff_t kDynastySurnameOffset = 8;
inline constexpr std::size_t kGotoSearchEntryStride = 0x70;
inline constexpr std::ptrdiff_t kGotoSearchMatchedOffset = 0x18;
inline constexpr std::ptrdiff_t kPdxArrayDataOffset = 0x08;
inline constexpr std::ptrdiff_t kPdxArrayCapacityOffset = 0x10;
inline constexpr std::ptrdiff_t kPdxArraySizeOffset = 0x14;
} // namespace localization

} // namespace eu4dll::targets::eu4_1_37_5::macos_x86_64
