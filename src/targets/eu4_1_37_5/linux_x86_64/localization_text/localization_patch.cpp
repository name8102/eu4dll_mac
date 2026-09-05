#include "targets/eu4_1_37_5/linux_x86_64/localization_text/localization_patch.h"

#include "features/escaped_text/escaped_text.h"
#include "features/localization_loading/localization_loading.h"
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::localization_utf8 {

void ConvertUtf8Localization(const char *utf8_in, char *out_buffer) {
    // Same portable conversion the canonical adapter calls: UTF-8 input
    // becomes escaped glyph sequences bounded by the legacy capacity.
    eu4dll::features::localization_loading::ConvertUtf8ForEu4(
        utf8_in, out_buffer, eu4dll::escaped_text::kLegacyOutputCapacity + 1);
}

patch::PatchDescription ValueConversionDescription(patch::Address converterTarget) {
    patch::PatchDescription description;
    description.feature = "localization-loading.utf8";
    description.target = kDiagnosticTargetId;
    description.location.pattern = localization_utf8::kValueConversionPattern;
    description.location.requireUnique = true;
    description.location.scope = patch::SearchScope::Symbol(
        localization_utf8::kLocalizeYmlAddKeySymbol,
        localization_utf8::kLocalizeYmlAddKeySearchSize);
    description.expected = patch::ExpectedBytes{
        localization_utf8::kValueConversionMutationOffset,
        {localization_utf8::kValueConversionOriginal.begin(),
         localization_utf8::kValueConversionOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::Call;
    description.mutation.offset = localization_utf8::kValueConversionMutationOffset;
    description.mutation.target = converterTarget;
    description.mutation.callWidth = patch::CallWidth::FiveBytes;
    return description;
}

std::vector<patch::PatchDescription> LocalizationDescriptions(
    patch::Address converterTarget) {
    return {ValueConversionDescription(converterTarget)};
}

namespace {

patch::Address LiveConverterTarget() {
    return reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&ConvertUtf8Localization));
}

}  // namespace

patch::BatchResult PreflightLocalization(patch::Memory &memory,
                                         patch::ExecutableCodeAllocator *allocator) {
    patch::PatchBatch batch(memory, allocator);
    for (auto &description : LocalizationDescriptions(LiveConverterTarget())) {
        batch.Add(std::move(description));
    }
    return batch.Preflight();
}

patch::BatchResult InstallLocalization(patch::Memory &memory,
                                       patch::ExecutableCodeAllocator *allocator) {
    patch::PatchBatch batch(memory, allocator);
    for (auto &description : LocalizationDescriptions(LiveConverterTarget())) {
        batch.Add(std::move(description));
    }
    return batch.Commit();
}

}  // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::localization_utf8
