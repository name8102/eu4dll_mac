#include "targets/eu4_1_37_5/macos_x86_64/compatibility_preflight.h"

#include "features/date_formatting/date_formatting.h"
#include "targets/eu4_1_37_5/macos_x86_64/target_facts.h"
#include "targets/eu4_1_37_5/macos_x86_64/text_rendering/rendering_contract.h"

#include <array>
#include <iterator>
#include <string>
#include <utility>

namespace eu4dll::targets::eu4_1_37_5::macos_x86_64 {
namespace {

constexpr std::array<std::uint8_t, 5> kRelativeCall{{0xE8, 0, 0, 0, 0}};
constexpr std::array<std::uint8_t, 5> kRelativeCallMask{{0xFF, 0, 0, 0, 0}};

std::vector<std::uint8_t> Bytes(const std::uint8_t *begin, const std::uint8_t *end) {
    return {begin, end};
}

template<std::size_t Size>
std::vector<std::uint8_t> Bytes(const std::array<std::uint8_t, Size> &bytes) {
    return {bytes.begin(), bytes.end()};
}

patch::PatchDescription Contract(
    const char *feature, const HookSite &site, patch::MutationKind kind,
    std::vector<std::uint8_t> expected, std::ptrdiff_t expectedOffset,
    std::vector<std::uint8_t> mask = {},
    std::vector<patch::Continuation> continuations = {},
    patch::CallWidth callWidth = patch::CallWidth::Auto,
    std::vector<std::uint8_t> mutationBytes = {},
    std::vector<std::string> referencedStrings = {},
    std::string symbol = {}, std::size_t symbolSearchSize = 0) {
    patch::PatchDescription description;
    description.feature = feature;
    description.target = kDiagnosticTargetId;
    description.location.pattern = site.pattern;
    description.location.referencedStrings = std::move(referencedStrings);
    if (!symbol.empty()) {
        description.location.scope =
            patch::SearchScope::Symbol(std::move(symbol), symbolSearchSize);
    }
    description.expected = patch::ExpectedBytes{
        expectedOffset, std::move(expected), std::move(mask)};
    description.mutation.kind = kind;
    description.mutation.offset = site.mutationOffset;
    description.mutation.callWidth = callWidth;
    description.mutation.bytes = std::move(mutationBytes);
    description.continuations = std::move(continuations);
    return description;
}

patch::PatchDescription RelativeCallContract(
    const char *feature, const HookSite &site,
    std::vector<patch::Continuation> continuations = {}) {
    return Contract(feature, site, patch::MutationKind::Call, Bytes(kRelativeCall),
                    site.mutationOffset, Bytes(kRelativeCallMask),
                    std::move(continuations), patch::CallWidth::FiveBytes);
}

patch::PatchDescription RelativeCallOverwrittenByJump(
    const char *feature, const HookSite &site,
    std::vector<patch::Continuation> continuations = {}) {
    return Contract(feature, site, patch::MutationKind::Jump, Bytes(kRelativeCall),
                    site.mutationOffset, Bytes(kRelativeCallMask),
                    std::move(continuations));
}

} // namespace

std::vector<patch::PatchDescription> BuildPatchDescriptions() {
    std::vector<patch::PatchDescription> contracts;
    contracts.reserve(55);

    contracts.push_back(RelativeCallContract(
        "base.CEU3Graphics.ReadGameSpecific.allocate-font", base::kAllocateFont));
    contracts.push_back(Contract(
        "base.CBitmapFont.ParseFontFile.allow-wide-glyphs", base::kAllowWideGlyphs,
        patch::MutationKind::RawBytes, Bytes(base::kAllowWideGlyphsOriginal),
        base::kAllowWideGlyphs.mutationOffset, {}, {}, patch::CallWidth::Auto,
        {base::kExpandedTextureLimitByte}));
    contracts.push_back(Contract(
        "base.CBitmapFont.ParseFontFile.wide-glyph-offset", base::kWideGlyphOffset,
        patch::MutationKind::Jump, Bytes(base::kWideGlyphOffsetOriginal), 0, {},
        {{"return", base::kWideGlyphOffset.continuationOffset}}));
    contracts.push_back(Contract(
        "base.texture-size-limit.first", base::kTextureSizeLimit1,
        patch::MutationKind::RawBytes, {0x00}, base::kTextureSizeLimit1.mutationOffset,
        {}, {}, patch::CallWidth::Auto, {base::kExpandedTextureLimitByte}));
    contracts.push_back(Contract(
        "base.texture-size-limit.second", base::kTextureSizeLimit2,
        patch::MutationKind::RawBytes, {0x00}, base::kTextureSizeLimit2.mutationOffset,
        {}, {}, patch::CallWidth::Auto, {base::kExpandedTextureLimitByte}));

    contracts.push_back(Contract(
        "input.handle-pdx-events.commit", input::kHandlePdxEvents1,
        patch::MutationKind::Jump, Bytes(input::kHandlePdxEvents1Original), 0, {},
        {{"return", input::kHandlePdxEvents1.continuationOffset}}));
    contracts.push_back(Contract(
        "input.handle-key-event.backspace", input::kHandleKeyEvent1,
        patch::MutationKind::Jump, Bytes(input::kHandleKeyEvent1Original), 0, {},
        {{"return", input::kHandleKeyEvent1.continuationOffset},
         {"bypass", input::kHandleKeyEvent1.bypassOffset}}));
    contracts.push_back(Contract(
        "input.handle-pdx-events.editing", input::kHandlePdxEvents2,
        patch::MutationKind::Jump, Bytes(input::kHandlePdxEvents2Original), 0, {},
        {{"return", input::kHandlePdxEvents2.continuationOffset}}));
    contracts.push_back(Contract(
        "input.handle-key-event.move-left", input::kMoveLeft,
        patch::MutationKind::Call, Bytes(input::kMoveLeftOriginal),
        input::kMoveLeft.mutationOffset, {}, {}, patch::CallWidth::SixBytes));
    contracts.push_back(Contract(
        "input.handle-key-event.move-right", input::kMoveRight,
        patch::MutationKind::Call, Bytes(input::kMoveRightOriginal),
        input::kMoveRight.mutationOffset, {}, {}, patch::CallWidth::SixBytes));

    contracts.push_back(Contract(
        "date-formatting.topbar-year-month-day", localization::kDateFormat,
        patch::MutationKind::RawBytes,
        {0x64, 0x20, 0x77, 0x20, 0x6D, 0x77, 0x20, 0x77, 0x20, 0x79},
        0, {}, {}, patch::CallWidth::Auto,
        Bytes(features::date_formatting::kYearMonthDayFormat.data(),
              features::date_formatting::kYearMonthDayFormat.data() +
                  features::date_formatting::kYearMonthDayFormat.size())));
    contracts.push_back(RelativeCallContract(
        "localization-loading.utf8", localization::kLocalizeYml));
    contracts.push_back(Contract(
        "localized-search.goto-box", localization::kGotoBoxProcess,
        patch::MutationKind::Jump, {0x4D, 0x6B, 0xE7, 0x70, 0x80}, 0, {},
        {{"success", localization::kGotoBoxProcess.continuationOffset},
         {"failure", localization::kGotoBoxProcess.bypassOffset}}));
    contracts.push_back(RelativeCallContract(
        "localized-search.enable-for-chinese-mod", localization::kMain));
    contracts.push_back(Contract(
        "east-asian-names.monarch", localization::kMonarchFullName,
        patch::MutationKind::Jump, {0x4C, 0x8B, 0x7B, 0x58, 0x48}, 0, {},
        {{"return", localization::kMonarchFullName.continuationOffset}}));
    contracts.push_back(RelativeCallOverwrittenByJump(
        "east-asian-names.republic-explicit", localization::kCountryNewRepublicName,
        {{"return", localization::kCountryNewRepublicName.continuationOffset}}));
    contracts.push_back(RelativeCallOverwrittenByJump(
        "east-asian-names.republic-random",
        localization::kCountryNewRepublicNameRandom));
    contracts.push_back(RelativeCallOverwrittenByJump(
        "east-asian-names.republic-culture",
        localization::kCountryNewRepublicNameCulture));

    static constexpr HookSite removeSpecial{"55 48 89 E5", 0};
    contracts.push_back(Contract(
        "save-filenames.remove-special-characters", removeSpecial,
        patch::MutationKind::RawBytes, {0x55, 0x48, 0x89, 0xE5}, 0, {}, {},
        patch::CallWidth::Auto,
        Bytes(save_filename::kRemoveSpecialCharactersBytes,
              save_filename::kRemoveSpecialCharactersBytes +
                  sizeof(save_filename::kRemoveSpecialCharactersBytes)),
        {}, symbols::kCStringRemoveSpecialCharacters, 64));
    contracts.push_back(RelativeCallOverwrittenByJump(
        "save-filenames.save-game", save_filename::kSaveGame,
        {{"return", save_filename::kSaveGame.continuationOffset}}));
    contracts.push_back(Contract(
        "save-filenames.local-item-constructor",
        save_filename::kLocalSavegameItemConstructor, patch::MutationKind::Jump,
        {0x4C, 0x89, 0xF7, 0x4C, 0x89},
        save_filename::kLocalSavegameItemConstructor.mutationOffset, {},
        {{"return", save_filename::kLocalSavegameItemConstructor.continuationOffset}}));
    contracts.push_back(Contract(
        "save-filenames.confirm-save", save_filename::kConfirmSave,
        patch::MutationKind::Jump, {0x49, 0x8B, 0x7C, 0x24, 0x08}, 0, {},
        {{"return", save_filename::kConfirmSave.continuationOffset}},
        patch::CallWidth::Auto, {}, {save_filename::kConfirmSaveText},
        symbols::kConfirmSaveConstructor, save_filename::kConstructorSearchSize));
    static constexpr HookSite updateHeader{save_filename::kUpdateHeaderInfo.pattern, 11};
    contracts.push_back(RelativeCallContract(
        "save-filenames.update-header-display-copy", updateHeader));
    contracts.push_back(Contract(
        "save-filenames.load-game", save_filename::kDoLoadGame,
        patch::MutationKind::Jump, {0x48, 0x8D, 0x7D, 0xA8, 0xE8},
        save_filename::kDoLoadGame.mutationOffset, {},
        {{"return", save_filename::kDoLoadGame.continuationOffset}}));
    contracts.push_back(Contract(
        "save-filenames.frontend-tooltip-display-copy",
        save_filename::kGetCurrentTooltip, patch::MutationKind::Jump,
        {0x48, 0x8D, 0xBD, 0x50, 0xFF}, 0, {}, {{"return", 19}}));
    static constexpr HookSite deleteCleanup{
        "48 8D 75 A0 48 89 DF 31 D2 E8 ? ? ? ?", 0, 7};
    contracts.push_back(Contract(
        "save-filenames.confirm-delete-cleanup", deleteCleanup,
        patch::MutationKind::Jump, {0x48, 0x8D, 0x75, 0xA0, 0x48}, 0, {},
        {{"return", deleteCleanup.continuationOffset}}, patch::CallWidth::Auto,
        {}, {}, symbols::kConfirmLocalDeleteConstructor,
        save_filename::kConstructorSearchSize));
    contracts.push_back(Contract(
        "save-filenames.confirm-delete-display-copy",
        save_filename::kConfirmLocalDelete, patch::MutationKind::Jump,
        {0x48, 0x8B, 0x7B, 0x08, 0x48}, 0, {},
        {{"return", save_filename::kConfirmLocalDelete.continuationOffset}},
        patch::CallWidth::Auto, {}, {save_filename::kConfirmDeleteText},
        symbols::kConfirmLocalDeleteConstructor,
        save_filename::kConstructorSearchSize));

    for (const auto &rendering : text_rendering::descriptors()) {
        contracts.push_back(text_rendering::make_patch_description(rendering.id));
    }
    return contracts;
}

const std::vector<CompatibilityPatchContract> &CompatibilityPatchRegistry() {
    static const std::vector<CompatibilityPatchContract> registry = [] {
        std::vector<CompatibilityPatchContract> result;
        for (auto &description : BuildPatchDescriptions()) {
            std::size_t width = 5;
            if (description.mutation.kind == patch::MutationKind::RawBytes) {
                width = description.mutation.bytes.size();
            } else if (description.mutation.kind == patch::MutationKind::Call &&
                       description.mutation.callWidth == patch::CallWidth::SixBytes) {
                width = 6;
            }
            result.push_back({description.feature, width, std::move(description)});
        }
        return result;
    }();
    return registry;
}

CompatibilityPreflightResult PreflightCompatibility(patch::Memory &memory) {
    CompatibilityPreflightResult result;
    std::string error;
    for (const char *symbol : symbols::kRequiredSymbols) {
        ++result.checkedSymbols;
        if (memory.ResolveSymbol(symbol, error)) continue;
        patch::PatchDiagnostic diagnostic;
        diagnostic.feature = std::string("target.symbol.") + symbol;
        diagnostic.target = kDiagnosticTargetId;
        diagnostic.operation = patch::PatchOperation::ResolveSymbol;
        diagnostic.message = error;
        result.failures.push_back(std::move(diagnostic));
    }

    patch::PatchRuntime runtime(memory);
    for (const auto &contract : CompatibilityPatchRegistry()) {
        ++result.checkedSites;
        const auto &description = contract.description;
        if (!description.expected ||
            description.expected->bytes.size() != contract.overwriteWidth) {
            patch::PatchDiagnostic diagnostic;
            diagnostic.feature = contract.id;
            diagnostic.target = kDiagnosticTargetId;
            diagnostic.operation = patch::PatchOperation::ValidateDescription;
            diagnostic.message = "expected bytes must cover the complete overwrite width";
            result.failures.push_back(std::move(diagnostic));
            continue;
        }
        const auto preflight = runtime.Preflight(description);
        if (!preflight) result.failures.push_back(preflight.diagnostic);
    }
    return result;
}

} // namespace eu4dll::targets::eu4_1_37_5::macos_x86_64
