#include "targets/eu4_1_37_5/linux_x86_64/base/base_patch.h"

#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"

#include <cstring>
#include <new>

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::base {

extern "C" {
__attribute__((visibility("hidden"))) uintptr_t g_linuxParseFontFileReturnAddress = 0;
}

namespace {

patch::PatchDescription Contract(const char *feature, const char *pattern,
                                 const char *symbol, std::size_t searchSize) {
    patch::PatchDescription description;
    description.feature = feature;
    description.target = kDiagnosticTargetId;
    description.location.pattern = pattern;
    description.location.requireUnique = true;
    description.location.scope = patch::SearchScope::Symbol(symbol, searchSize);
    return description;
}

}  // namespace

patch::Address &ParseFontFileReturnSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxParseFontFileReturnAddress);
}

void *BitmapFontOperatorNewProxy() {
    // Replaces the fixed 0x3560 allocation in ReadGameSpecific with the
    // expanded bitmap-font allocation, zeroed for the wider glyph table.
    void *address = ::operator new(kExpandedBitmapFontSize);
    std::memset(address, 0, kExpandedBitmapFontSize);
    return address;
}

// Naked System V x86-64 hook for the ParseFontFile character-index site.
// Contract documented in ABI_NOTES.md; behavior verified against the legacy
// Linux port: extended glyphs (index >= 256) shift by kCharacterIndexShift,
// then the two overwritten instructions replay and control jumps to the
// continuation published in ParseFontFileReturnSlot().
__attribute__((naked)) void NakedParseFontFileCharacterIndex() {
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "cmp r13d, 256\n"
        "jb 1f\n"
        "add r13d, %c[index_shift]\n"
        "1:\n"
        "mov ecx, r13d\n"
        "mov rax, [rsp + 0x8]\n"
        "jmp qword ptr [rip + g_linuxParseFontFileReturnAddress]\n"
        ".att_syntax prefix\n"
        :
        : [index_shift] "i"(kCharacterIndexShift));
}

patch::PatchDescription AllocateFontDescription(patch::Address allocateCallTarget) {
    auto description = Contract("base.CEU3Graphics.ReadGameSpecific.allocate-font",
                                kAllocateFontPattern, symbols::kReadGameSpecific,
                                kAllocateFontSearchSize);
    description.expected = patch::ExpectedBytes{
        kAllocateFontMutationOffset,
        {kAllocateFontOriginal.begin(), kAllocateFontOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::Call;
    description.mutation.offset = kAllocateFontMutationOffset;
    description.mutation.target = allocateCallTarget;
    description.mutation.callWidth = patch::CallWidth::FiveBytes;
    return description;
}

patch::PatchDescription CharacterLimitDescription() {
    auto description = Contract("base.CBitmapFont.ParseFontFile.allow-wide-glyphs",
                                kCharacterLimitPattern, symbols::kParseFontFile,
                                kParseFontFileSearchSize);
    description.expected = patch::ExpectedBytes{
        kCharacterLimitMutationOffset, {kCharacterLimitOriginal}, {}};
    description.mutation.kind = patch::MutationKind::RawBytes;
    description.mutation.offset = kCharacterLimitMutationOffset;
    description.mutation.bytes = {kCharacterLimitReplacement};
    return description;
}

patch::PatchDescription CharacterIndexDescription(patch::Address hookTarget) {
    auto description = Contract("base.CBitmapFont.ParseFontFile.wide-glyph-offset",
                                kCharacterIndexPattern, symbols::kParseFontFile,
                                kParseFontFileSearchSize);
    description.expected = patch::ExpectedBytes{
        0, {kCharacterIndexOriginal.begin(), kCharacterIndexOriginal.end()}, {}};
    description.mutation.kind = patch::MutationKind::Jump;
    description.mutation.offset = 0;
    description.mutation.target = hookTarget;
    description.continuations = {{"return", kCharacterIndexContinuationOffset}};
    return description;
}

patch::PatchDescription TextureSizeDescription() {
    auto description = Contract("base.texture-size-limit", kTextureSizePattern,
                                symbols::kLoadTexture, kLoadTextureSearchSize);
    description.expected = patch::ExpectedBytes{
        kTextureSizeMutationOffset, {kTextureSizeOriginal}, {}};
    description.mutation.kind = patch::MutationKind::RawBytes;
    description.mutation.offset = kTextureSizeMutationOffset;
    description.mutation.bytes = {kTextureSizeReplacement};
    return description;
}

std::vector<patch::PatchDescription> BaseDescriptions(
    patch::Address allocateCallTarget, patch::Address characterIndexHookTarget) {
    return {AllocateFontDescription(allocateCallTarget),
            CharacterLimitDescription(),
            CharacterIndexDescription(characterIndexHookTarget),
            TextureSizeDescription()};
}

patch::BatchResult PreflightBase(patch::Memory &memory,
                                 patch::ExecutableCodeAllocator *allocator) {
    // Fixture/live preflight uses reachable dummy targets when the caller does
    // not provide hook addresses: the branch check only needs *a* target to
    // verify encoding, and the batch path re-resolves live addresses at commit.
    const patch::Address proxy = reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&BitmapFontOperatorNewProxy));
    const patch::Address hook = reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&NakedParseFontFileCharacterIndex));
    patch::PatchBatch batch(memory, allocator);
    for (auto &description : BaseDescriptions(proxy, hook)) {
        batch.Add(std::move(description));
    }
    return batch.Preflight();
}

patch::BatchResult InstallBase(patch::Memory &memory,
                               patch::ExecutableCodeAllocator *allocator) {
    const patch::Address proxy = reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&BitmapFontOperatorNewProxy));
    const patch::Address hook = reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&NakedParseFontFileCharacterIndex));

    // Discover the character-index continuation first so the naked hook's
    // return slot is published before any mutation commits.
    {
        patch::PatchRuntime runtime(memory);
        const auto located =
            runtime.Preflight(CharacterIndexDescription(hook));
        if (!located) {
            patch::BatchResult failed;
            failed.diagnostic = located.diagnostic;
            return failed;
        }
        const auto continuation = located.ContinuationAddress("return");
        if (continuation == 0) {
            patch::BatchResult failed;
            failed.diagnostic.feature = "base.CBitmapFont.ParseFontFile.wide-glyph-offset";
            failed.diagnostic.target = kDiagnosticTargetId;
            failed.diagnostic.operation = patch::PatchOperation::CalculateMutation;
            failed.diagnostic.message = "character-index continuation is missing";
            return failed;
        }
        ParseFontFileReturnSlot() = continuation;
    }

    patch::PatchBatch batch(memory, allocator);
    for (auto &description : BaseDescriptions(proxy, hook)) {
        batch.Add(std::move(description));
    }
    auto result = batch.Commit();
    // Unconfirmed rollback may still have game code inside the hook whose
    // trampoline the batch kept mapped: retain the slot (same fail-safe
    // model as trampoline retention).
    if (!result && !patch::MustRetainSlots(result)) {
        ParseFontFileReturnSlot() = 0;
    }
    return result;
}

}  // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::base
