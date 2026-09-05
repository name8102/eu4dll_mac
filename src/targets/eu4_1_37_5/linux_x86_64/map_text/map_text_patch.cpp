#include "targets/eu4_1_37_5/linux_x86_64/map_text/map_text_patch.h"

#include <cctype>
#include <cstdio>
#include <utility>

#include "features/escaped_text/escaped_text.h"
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::map_text {
namespace {

namespace escaped = eu4dll::escaped_text;

patch::PatchDescription SymbolContract(const char *feature, const char *pattern,
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

std::uint32_t AdvanceLogicalChar(const std::uint8_t *bytes, std::uint32_t size,
                                 std::uint32_t offset) {
    if (bytes == nullptr || offset >= size) return size;
    const std::uint8_t byte = bytes[offset];
    if (byte < kEscapeFirst || byte > kEscapeLast) return offset + 1;
    // Truncated escapes advance past the marker only; matches the hooks'
    // plain-byte fallback (never skip NUL or run past the end).
    if (offset + 2 >= size || bytes[offset + 1] == 0 || bytes[offset + 2] == 0) {
        return offset + 1;
    }
    return offset + 3;
}

std::uint32_t CountLogicalGlyphs(const std::uint8_t *bytes, std::uint32_t size) {
    std::uint32_t count = 0;
    for (std::uint32_t offset = 0; offset < size;) {
        const std::uint32_t next = AdvanceLogicalChar(bytes, size, offset);
        if (next <= offset) break;  // defensive: never loop forever
        offset = next;
        ++count;
    }
    return count;
}

extern "C" std::uint8_t ClassifySpacingEscapeRaw(std::uint64_t markerIndex,
                                                       std::uint64_t lastIndex) {
    // r15 is the last valid byte index: an escape needs markerIndex+1 and
    // markerIndex+2 to both be addressable payload.
    if (markerIndex >= lastIndex) {
        return static_cast<std::uint8_t>(SpacingEscapeAction::kFinalTruncated);
    }
    if (markerIndex + 2 > lastIndex) {
        return static_cast<std::uint8_t>(SpacingEscapeAction::kFinalTruncated);
    }
    if (markerIndex + 2 == lastIndex) {
        return static_cast<std::uint8_t>(SpacingEscapeAction::kFinalComplete);
    }
    return static_cast<std::uint8_t>(SpacingEscapeAction::kContinue);
}

extern "C" {
__attribute__((visibility("hidden"))) uintptr_t g_linuxFillPreprocessingReturnAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxFillDrawingReturnAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxAddNameAreaSpacingReturnAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxAddNameAreaSpacingFinalAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxAddNameAreaGlyphReturnAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxAddNudgedNamesGlyphReturnAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxCurveTextDrawingReturnAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxMapTextCStringAppendCharAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxMapTextCStringAppendStringAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxMapTextCStringGetSizeAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxMapTextCStringIndexAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxMapTextCStringMutableIndexAddress = 0;
}  // extern "C"

void ToUpperPreservingEscapes(void *text) {
    const auto getSize =
        reinterpret_cast<CStringGetSize>(g_linuxMapTextCStringGetSizeAddress);
    const auto index = reinterpret_cast<CStringMutableIndex>(
        g_linuxMapTextCStringMutableIndexAddress);
    if (getSize == nullptr || index == nullptr || text == nullptr) return;
    const std::uint32_t size = getSize(text);
    for (std::uint32_t offset = 0; offset < size;) {
        char *current = index(text, offset);
        const std::uint8_t byte = static_cast<std::uint8_t>(*current);
        if (byte >= kEscapeFirst && byte <= kEscapeLast) {
            offset = AdvanceLogicalChar(
                reinterpret_cast<const std::uint8_t *>(current), size - offset, 0) +
                     offset;
            continue;
        }
        *current = static_cast<char>(std::toupper(byte));
        ++offset;
    }
}

__attribute__((target("general-regs-only")))
void AppendFinalSpacingByte(void *text, const char *scratch) {
    // The original final-ASCII path reuses a scratch buffer that may still
    // hold the previous Han payload. Append exactly its new first byte.
    const char finalByte[2]{scratch[0], 0};
    reinterpret_cast<void (*)(void *, const char *)>(
        g_linuxMapTextCStringAppendStringAddress)(text, finalByte);
}

__attribute__((target("general-regs-only")))
std::uint32_t CurveTextGetGlyphCount(const void *text) {
    const auto getSize =
        reinterpret_cast<CStringGetSize>(g_linuxMapTextCStringGetSizeAddress);
    const auto index =
        reinterpret_cast<CStringIndex>(g_linuxMapTextCStringIndexAddress);
    if (getSize == nullptr || index == nullptr || text == nullptr) return 0;
    const std::uint32_t size = getSize(text);
    std::uint32_t count = 0;
    for (std::uint32_t offset = 0; offset < size; ++count) {
        const std::uint8_t byte =
            static_cast<std::uint8_t>(*index(text, offset));
        offset = (byte >= kEscapeFirst && byte <= kEscapeLast)
                     ? AdvanceLogicalChar(
                           reinterpret_cast<const std::uint8_t *>(index(text, offset)),
                           size - offset, 0) +
                           offset
                     : offset + 1;
    }
    return count;
}

patch::Address &CStringAppendCharSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxMapTextCStringAppendCharAddress);
}
patch::Address &CStringAppendStringSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxMapTextCStringAppendStringAddress);
}
patch::Address &CStringGetSizeSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxMapTextCStringGetSizeAddress);
}
patch::Address &CStringIndexSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxMapTextCStringIndexAddress);
}
patch::Address &CStringMutableIndexSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxMapTextCStringMutableIndexAddress);
}
patch::Address &FillPreprocessingReturnSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxFillPreprocessingReturnAddress);
}
patch::Address &FillDrawingReturnSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxFillDrawingReturnAddress);
}
patch::Address &SpacingReturnSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxAddNameAreaSpacingReturnAddress);
}
patch::Address &SpacingFinalSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxAddNameAreaSpacingFinalAddress);
}
patch::Address &AddNameAreaGlyphReturnSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxAddNameAreaGlyphReturnAddress);
}
patch::Address &AddNudgedNamesGlyphReturnSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxAddNudgedNamesGlyphReturnAddress);
}
patch::Address &CurveDrawingReturnSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxCurveTextDrawingReturnAddress);
}

// Escape decoder with the truncated-escape guard (see text_layout): both
// payload bytes must be non-NUL before the word read.
#define EU4DLL_LINUX_DECODE_ESCAPE(indexRegister, addressRegister)       \
    "cmp " indexRegister ", %c[escape1]\n"                               \
    "je 1f\n"                                                            \
    "cmp " indexRegister ", %c[escape2]\n"                               \
    "je 2f\n"                                                            \
    "cmp " indexRegister ", %c[escape3]\n"                               \
    "je 3f\n"                                                            \
    "cmp " indexRegister ", %c[escape4]\n"                               \
    "je 4f\n"                                                            \
    "jmp 7f\n"                                                           \
    "1:\n"                                                               \
    "cmp byte ptr [" addressRegister " + 1], 0\n"                        \
    "je 7f\n"                                                            \
    "cmp byte ptr [" addressRegister " + 2], 0\n"                        \
    "je 7f\n"                                                            \
    "movzx " indexRegister ", word ptr [" addressRegister " + 1]\n"      \
    "jmp 5f\n"                                                           \
    "2:\n"                                                               \
    "cmp byte ptr [" addressRegister " + 1], 0\n"                        \
    "je 7f\n"                                                            \
    "cmp byte ptr [" addressRegister " + 2], 0\n"                        \
    "je 7f\n"                                                            \
    "movzx " indexRegister ", word ptr [" addressRegister " + 1]\n"      \
    "sub " indexRegister ", %c[shift2]\n"                                \
    "jmp 5f\n"                                                           \
    "3:\n"                                                               \
    "cmp byte ptr [" addressRegister " + 1], 0\n"                        \
    "je 7f\n"                                                            \
    "cmp byte ptr [" addressRegister " + 2], 0\n"                        \
    "je 7f\n"                                                            \
    "movzx " indexRegister ", word ptr [" addressRegister " + 1]\n"      \
    "add " indexRegister ", %c[shift3]\n"                                \
    "jmp 5f\n"                                                           \
    "4:\n"                                                               \
    "cmp byte ptr [" addressRegister " + 1], 0\n"                        \
    "je 7f\n"                                                            \
    "cmp byte ptr [" addressRegister " + 2], 0\n"                        \
    "je 7f\n"                                                            \
    "movzx " indexRegister ", word ptr [" addressRegister " + 1]\n"      \
    "add " indexRegister ", %c[shift4]\n"

#define EU4DLL_LINUX_ESCAPE_OPERANDS                                            \
    [escape1] "i"(escaped::kEscape1), [escape2] "i"(escaped::kEscape2),         \
        [escape3] "i"(escaped::kEscape3), [escape4] "i"(escaped::kEscape4),     \
        [shift2] "i"(escaped::kEscape2Shift), [shift3] "i"(escaped::kEscape3Shift), \
        [shift4] "i"(escaped::kEscape4Shift),                                   \
        [index_shift] "i"(base::kCharacterIndexShift)

__attribute__((naked)) void NakedFillVertexBufferPreprocessing() {
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "mov rdx, rax\n"
        "movzx eax, byte ptr [rax]\n" EU4DLL_LINUX_DECODE_ESCAPE("eax", "rdx")
        "5:\n"
        "push rax\n"
        "push rdx\n"
        "lea rdi, [rsp + 0x48]\n"
        "movzx esi, byte ptr [rdx + 1]\n"
        "call qword ptr [rip + g_linuxMapTextCStringAppendCharAddress]\n"
        "mov rdx, qword ptr [rsp]\n"
        "lea rdi, [rsp + 0x48]\n"
        "movzx esi, byte ptr [rdx + 2]\n"
        "call qword ptr [rip + g_linuxMapTextCStringAppendCharAddress]\n"
        "pop rdx\n"
        "pop rax\n"
        "add r14d, 2\n"
        "cmp eax, 256\n"
        "jb 7f\n"
        "add eax, %c[index_shift]\n"
        "7:\n"
        "mov rcx, qword ptr [rsp + 0x68]\n"
        "mov r15, qword ptr [rcx + rax * 8 + 0x100]\n"
        "jmp qword ptr [rip + g_linuxFillPreprocessingReturnAddress]\n"
        ".att_syntax prefix\n"
        :
        : EU4DLL_LINUX_ESCAPE_OPERANDS);
}

#define EU4DLL_DEFINE_GLYPH_HOOK(functionName, counterInstruction, tableBase, \
                                 returnAddress)                               \
    __attribute__((naked)) void functionName() {                               \
        __asm__ volatile(                                                      \
                ".intel_syntax noprefix\n"                                      \
                "mov rdx, rax\n"                                                \
                "movzx eax, byte ptr [rax]\n"                                   \
                EU4DLL_LINUX_DECODE_ESCAPE("eax", "rdx")                        \
                "5:\n"                                                          \
                counterInstruction "\n"                                        \
                "cmp eax, 256\n"                                                \
                "jb 7f\n"                                                       \
                "add eax, %c[index_shift]\n"                                    \
                "7:\n"                                                          \
                tableBase "\n"                                                  \
                "jmp qword ptr [rip + " #returnAddress "]\n"                    \
                ".att_syntax prefix\n"                                          \
                :                                                               \
                : EU4DLL_LINUX_ESCAPE_OPERANDS);                               \
    }

EU4DLL_DEFINE_GLYPH_HOOK(NakedFillVertexBufferDrawing, "add ebx, 2",
                         "mov rax, qword ptr [r12 + rax * 8 + 0x100]",
                         g_linuxFillDrawingReturnAddress)
EU4DLL_DEFINE_GLYPH_HOOK(NakedAddNameAreaGlyphCount, "add ebx, 2",
                         "mov rax, qword ptr [r12 + rax * 8 + 0x100]",
                         g_linuxAddNameAreaGlyphReturnAddress)
EU4DLL_DEFINE_GLYPH_HOOK(NakedAddNudgedNamesGlyphCount, "add ebx, 2",
                         "mov rax, qword ptr [r12 + rax * 8 + 0x100]",
                         g_linuxAddNudgedNamesGlyphReturnAddress)

#undef EU4DLL_DEFINE_GLYPH_HOOK

__attribute__((naked)) void NakedAddNameAreaSpacing() {
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        // Native scratch initialization reserves only a byte plus NUL. Four
        // bytes fit in its aligned stack slot and terminate a full escape.
        "mov dword ptr [rbp - 0x88], 0\n"
        "mov al, byte ptr [r13 + r12]\n"
        "mov byte ptr [rbp - 0x88], al\n"
        "cmp al, %c[escape1]\n"
        "je 1f\n"
        "cmp al, %c[escape2]\n"
        "je 1f\n"
        "cmp al, %c[escape3]\n"
        "je 1f\n"
        "cmp al, %c[escape4]\n"
        "je 1f\n"
        "jmp qword ptr [rip + g_linuxAddNameAreaSpacingReturnAddress]\n"
        "1:\n"
        // Escape disposition mirrors ClassifySpacingEscape exactly (unit-
        // tested): r15 is the last valid byte index, so r12+2==r15 is a
        // complete final escape (copy payload, then final), not a
        // truncation. r12/r13/r15/rbp survive the call (callee-saved);
        // rdi/rsi are saved; the helper emits no XMM (general-regs-only).
        "push rdi\n"
        "push rsi\n"
        "mov rdi, r12\n"
        "mov rsi, r15\n"
        "call ClassifySpacingEscapeRaw\n"
        "pop rsi\n"
        "pop rdi\n"
        // uint8_t returns only define AL; EAX can retain a CString pointer.
        "movzx ecx, al\n"
        "cmp ecx, 2\n"
        "je 2f\n"
        "mov ax, word ptr [r13 + r12 + 1]\n"
        "mov word ptr [rbp - 0x87], ax\n"
        "cmp ecx, 1\n"
        "je 2f\n"
        "add r12, 2\n"
        "jmp qword ptr [rip + g_linuxAddNameAreaSpacingReturnAddress]\n"
        "2:\n"
        "lea rdi, [rbp - 0x238]\n"
        "lea rsi, [rbp - 0x88]\n"
        "call qword ptr [rip + g_linuxMapTextCStringAppendStringAddress]\n"
        "jmp qword ptr [rip + g_linuxAddNameAreaSpacingFinalAddress]\n"
        ".att_syntax prefix\n"
        :
        : [escape1] "i"(escaped::kEscape1), [escape2] "i"(escaped::kEscape2),
          [escape3] "i"(escaped::kEscape3), [escape4] "i"(escaped::kEscape4));
}

__attribute__((naked)) void NakedCurveTextDrawing() {
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        // Stateless glyph->byte mapping: derive the skip from a structural
        // walk instead of any counter. Entry: rax = data + r14d (operator[]
        // with the glyph index), r14d = glyph index. Walk r14d glyphs from
        // the string base (CString* at [rbp-0x148], refreshed every loop
        // iteration by the loop head; data at +0) accumulating +2 per
        // escape; rax += skipped lands on the true byte. No global, no TLS,
        // no stack slot: pushes are balanced, no calls are made, r14d is
        // read-only, and only rax/flags (dead at the site) change.
        "push rax\n"
        "push rcx\n"
        "push rdx\n"
        "push rsi\n"
        "push rdi\n"
        "mov rdi, qword ptr [rbp - 0x148]\n"
        "mov rdi, qword ptr [rdi]\n"
        "mov ecx, r14d\n"
        "xor esi, esi\n"
        "8:\n"
        "test ecx, ecx\n"
        "jz 9f\n"
        "cmp byte ptr [rdi], 0\n"
        "je 9f\n"
        "movzx eax, byte ptr [rdi]\n"
        "sub al, %c[esc1]\n"
        "cmp al, 3\n"
        "ja 6f\n"
        "cmp byte ptr [rdi + 1], 0\n"
        "je 6f\n"
        "cmp byte ptr [rdi + 2], 0\n"
        "je 6f\n"
        "add rdi, 3\n"
        "add esi, 2\n"
        "dec ecx\n"
        "jmp 8b\n"
        "6:\n"
        "inc rdi\n"
        "dec ecx\n"
        "jmp 8b\n"
        "9:\n"
        "add qword ptr [rsp + 32], rsi\n"
        "pop rdi\n"
        "pop rsi\n"
        "pop rdx\n"
        "pop rcx\n"
        "pop rax\n"
        "mov rdx, rax\n"
        "movzx eax, byte ptr [rax]\n" EU4DLL_LINUX_DECODE_ESCAPE("eax", "rdx")
        "5:\n"
        "cmp eax, 256\n"
        "jb 7f\n"
        "add eax, %c[index_shift]\n"
        "7:\n"
        // NOTE: CurveText uses the plain [r12 + index*8] table (no +0x100),
        // exactly as the overwritten original did.
        "mov r13, qword ptr [r12 + rax * 8]\n"
        "jmp qword ptr [rip + g_linuxCurveTextDrawingReturnAddress]\n"
        ".att_syntax prefix\n"
        :
        : EU4DLL_LINUX_ESCAPE_OPERANDS,
          [esc1] "i"(escaped::kEscape1));
}

#undef EU4DLL_LINUX_ESCAPE_OPERANDS
#undef EU4DLL_LINUX_DECODE_ESCAPE

namespace {

patch::Address LiveAddress(void (*fn)()) {
    return reinterpret_cast<patch::Address>(reinterpret_cast<std::uintptr_t>(fn));
}

// Per-cluster slot clearing: a failing cluster must never clear slots
// owned by already-installed clusters (their machine code stays patched).
// Shared CString callee slots are resolved once by InstallMapText and only
// cleared when zero clusters installed successfully. Every clear below is
// additionally gated on MustRetainSlots by the caller.
void ClearCalleeSlots() {
    CStringAppendCharSlot() = 0;
    CStringAppendStringSlot() = 0;
    CStringGetSizeSlot() = 0;
    CStringIndexSlot() = 0;
    CStringMutableIndexSlot() = 0;
}
void ClearFillSlots() {
    FillPreprocessingReturnSlot() = 0;
    FillDrawingReturnSlot() = 0;
}
void ClearAddNameAreaSlots() {
    SpacingReturnSlot() = 0;
    SpacingFinalSlot() = 0;
    AddNameAreaGlyphReturnSlot() = 0;
}
void ClearAddNudgedNamesSlots() {
    AddNudgedNamesGlyphReturnSlot() = 0;
}
void ClearCurveTextSlots() {
    CurveDrawingReturnSlot() = 0;
}

bool ResolveCStringSlots(patch::Memory &memory, const char *feature,
                         patch::BatchResult &failed) {
    struct SlotRequest {
        const char *symbol;
        patch::Address *slot;
    };
    SlotRequest requests[] = {
        {kCStringAppendCharSymbol, &CStringAppendCharSlot()},
        {kCStringAppendStringSymbol, &CStringAppendStringSlot()},
        {kCStringGetSizeSymbol, &CStringGetSizeSlot()},
        {kCStringIndexSymbol, &CStringIndexSlot()},
        {kCStringMutableIndexSymbol, &CStringMutableIndexSlot()},
    };
    for (const auto &request : requests) {
        std::string error;
        const auto address = memory.ResolveSymbol(request.symbol, error);
        if (!address) {
            failed.diagnostic.feature = feature;
            failed.diagnostic.target = kDiagnosticTargetId;
            failed.diagnostic.operation = patch::PatchOperation::ResolveSymbol;
            failed.diagnostic.message = error;
            return false;
        }
        *request.slot = *address;
    }
    return true;
}

}  // namespace

patch::PatchDescription FillPreprocessingDescription(patch::Address hookTarget) {
    auto description = SymbolContract(
        "map-text.CBitmapFont.FillVertexBuffer.preprocessing",
        kFillPreprocessingPattern, kFillVertexBufferSymbol,
        kFillVertexBufferSearchSize);
    description.expected = patch::ExpectedBytes{
        0,
        {kFillPreprocessingOriginal.begin(),
         kFillPreprocessingOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::Jump;
    description.mutation.target = hookTarget;
    description.continuations = {
        {"return", kFillPreprocessingContinuationOffset}};
    return description;
}

patch::PatchDescription FillDrawingDescription(patch::Address hookTarget) {
    auto description = SymbolContract(
        "map-text.CBitmapFont.FillVertexBuffer.drawing", kFillDrawingPattern,
        kFillVertexBufferSymbol, kFillVertexBufferSearchSize);
    description.expected = patch::ExpectedBytes{
        0,
        {kFillDrawingOriginal.begin(), kFillDrawingOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::Jump;
    description.mutation.target = hookTarget;
    description.continuations = {
        {"return", kFillDrawingContinuationOffset}};
    return description;
}

patch::PatchDescription SpacingDescription(patch::Address hookTarget) {
    auto description = SymbolContract(
        "map-text.CGenerateNamesWork.AddNameArea.spacing", kSpacingPattern,
        kGenerateNamesAddNameAreaSymbol, kAddNameAreaSearchSize);
    description.expected = patch::ExpectedBytes{
        0, {kSpacingOriginal.begin(), kSpacingOriginal.end()}, {}};
    description.mutation.kind = patch::MutationKind::Jump;
    description.mutation.target = hookTarget;
    description.continuations = {
        {"return", kSpacingContinuationOffset},
        {"final", kSpacingFinalOffset}};
    return description;
}

patch::PatchDescription ToUpperCallDescription(patch::Address converterTarget) {
    auto description = SymbolContract(
        "map-text.CGenerateNamesWork.AddNameArea.escape-preserving-toupper",
        kToUpperCallPattern, kGenerateNamesAddNameAreaSymbol,
        kAddNameAreaSearchSize);
    description.expected = patch::ExpectedBytes{
        0,
        {kToUpperCallOriginal.begin(), kToUpperCallOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::Call;
    description.mutation.target = converterTarget;
    description.mutation.callWidth = patch::CallWidth::FiveBytes;
    return description;
}

patch::PatchDescription AddNameAreaGlyphDescription(patch::Address hookTarget) {
    auto description = SymbolContract(
        "map-text.CGenerateNamesWork.AddNameArea.glyph-count",
        kNameGlyphPattern, kGenerateNamesAddNameAreaSymbol,
        kAddNameAreaSearchSize);
    description.expected = patch::ExpectedBytes{
        0,
        {kNameGlyphOriginal.begin(), kNameGlyphOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::Jump;
    description.mutation.target = hookTarget;
    description.continuations = {
        {"return", kNameGlyphContinuationOffset}};
    return description;
}

patch::PatchDescription AddNudgedNamesGlyphDescription(patch::Address hookTarget) {
    auto description = SymbolContract(
        "map-text.CCountryNameCollection.AddNudgedNames.glyph-count",
        kNameGlyphPattern, kAddNudgedNamesSymbol,
        kAddNudgedNamesSearchSize);
    description.expected = patch::ExpectedBytes{
        0,
        {kNameGlyphOriginal.begin(), kNameGlyphOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::Jump;
    description.mutation.target = hookTarget;
    description.continuations = {
        {"return", kNameGlyphContinuationOffset}};
    return description;
}

patch::PatchDescription CurveDrawingDescription(patch::Address hookTarget) {
    patch::PatchDescription description;
    description.feature = "map-text.CurveText.drawing-decode";
    description.target = kDiagnosticTargetId;
    description.location.pattern = kCurveDrawingPattern;
    description.location.requireUnique = true;
    // CurveText is file-static: scoped to AddNameArea with the calibrated
    // 0x2400 window instead of a dynamic symbol.
    description.location.scope = patch::SearchScope::Symbol(
        kGenerateNamesAddNameAreaSymbol, kCurveTextSearchSize);
    description.expected = patch::ExpectedBytes{
        0,
        {kCurveDrawingOriginal.begin(), kCurveDrawingOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::Jump;
    description.mutation.target = hookTarget;
    description.continuations = {
        {"return", kCurveDrawingContinuationOffset}};
    return description;
}


namespace {

// The CurveText length-calls window holds two adjacent GetSize calls; the
// pattern match address is the first call, the second is +11. Both are
// validated (distinct rel32 operands) and redirected as one cluster batch.
struct LengthCallSites {
    patch::Address first = 0;
    patch::Address second = 0;
};

std::optional<LengthCallSites> LocateLengthCalls(patch::Memory &memory,
                                                 std::string &error) {
    patch::PatchRuntime runtime(memory);
    patch::PatternLocation location;
    location.pattern = kCurveLengthCallsPattern;
    location.requireUnique = true;
    location.scope = patch::SearchScope::Symbol(
        kGenerateNamesAddNameAreaSymbol, kCurveTextSearchSize);
    const auto located = runtime.Locate(location, "map-text.CurveText.length-calls",
                                        kDiagnosticTargetId);
    if (!located) {
        error = patch::FormatDiagnostic(located.diagnostic);
        return std::nullopt;
    }
    LengthCallSites sites{located.address, located.address + 11};
    for (const auto site : {sites.first, sites.second}) {
        if (site < located.address) {
            error = "length-call address overflows";
            return std::nullopt;
        }
    }
    return sites;
}

}  // namespace

std::vector<patch::PatchDescription> FillVertexBufferDescriptions(
    const MapHookTargets &targets) {
    return {FillPreprocessingDescription(targets.fillPreprocessing),
            FillDrawingDescription(targets.fillDrawing)};
}

std::vector<patch::PatchDescription> AddNameAreaDescriptions(
    const MapHookTargets &targets) {
    auto finalByte = SymbolContract("map-text.CGenerateNamesWork.AddNameArea.final-byte",
        "E8 16 E3 9E 00 4C 8B 6D 88", kGenerateNamesAddNameAreaSymbol, kAddNameAreaSearchSize);
    finalByte.expected = patch::ExpectedBytes{0, {0xE8,0x16,0xE3,0x9E,0}, {}};
    finalByte.mutation.kind = patch::MutationKind::Call;
    finalByte.mutation.callWidth = patch::CallWidth::FiveBytes;
    finalByte.mutation.target = targets.spacingFinalByte;
    return {SpacingDescription(targets.spacing),
            ToUpperCallDescription(targets.toUpperCall),
            AddNameAreaGlyphDescription(targets.addNameAreaGlyph), std::move(finalByte)};
}

std::vector<patch::PatchDescription> AddNudgedNamesDescriptions(
    const MapHookTargets &targets) {
    return {AddNudgedNamesGlyphDescription(targets.addNudgedNamesGlyph)};
}

std::vector<patch::PatchDescription> CurveTextDescriptions(
    const MapHookTargets &targets) {
    // NOTE: the three GetSize call redirects are staged by InstallCurveText
    // through LocateLengthCalls (address-pinned); this factory covers the
    // jump-hooked members for preflight/test use.
    return {CurveDrawingDescription(targets.curveDrawing)};
}

std::vector<patch::PatchDescription> CurveCallRedirects(patch::Address target) {
    // Both calls share the length-calls window pattern but differ in rel32,
    // so the full 5-byte expected sequence at offset 0/+11 selects each
    // call deterministically. InstallCurveText additionally pre-verifies the
    // exact bytes at the located addresses before staging these.
    patch::PatchDescription firstRedirect;
    firstRedirect.feature = "map-text.CurveText.glyph-count-call.first";
    firstRedirect.target = kDiagnosticTargetId;
    firstRedirect.location.pattern = kCurveLengthCallsPattern;
    firstRedirect.location.requireUnique = true;
    firstRedirect.location.scope = patch::SearchScope::Symbol(
        kGenerateNamesAddNameAreaSymbol, kCurveTextSearchSize);
    firstRedirect.expected = patch::ExpectedBytes{
        0, {kCurveFirstCallOriginal.begin(), kCurveFirstCallOriginal.end()}, {}};
    firstRedirect.mutation.kind = patch::MutationKind::Call;
    firstRedirect.mutation.target = target;
    firstRedirect.mutation.callWidth = patch::CallWidth::FiveBytes;
    patch::PatchDescription secondRedirect = firstRedirect;
    secondRedirect.feature = "map-text.CurveText.glyph-count-call.second";
    secondRedirect.expected = patch::ExpectedBytes{
        static_cast<std::ptrdiff_t>(kCurveSecondCallOffset),
        {kCurveSecondCallOriginal.begin(), kCurveSecondCallOriginal.end()},
        {}};
    secondRedirect.mutation.offset =
        static_cast<std::ptrdiff_t>(kCurveSecondCallOffset);
    secondRedirect.mutation.target = target;
    patch::PatchDescription interpolation = firstRedirect;
    interpolation.feature = "map-text.CurveText.glyph-count-call.interpolation";
    interpolation.location.pattern = kCurveInterpolationCallPattern;
    interpolation.expected = patch::ExpectedBytes{
        0, {kCurveInterpolationCallOriginal.begin(),
            kCurveInterpolationCallOriginal.end()}, {}};
    return {std::move(firstRedirect), std::move(secondRedirect),
            std::move(interpolation)};
}

MapHookTargets LiveHookTargets() {
    MapHookTargets targets;
    targets.fillPreprocessing = LiveAddress(NakedFillVertexBufferPreprocessing);
    targets.fillDrawing = LiveAddress(NakedFillVertexBufferDrawing);
    targets.spacing = LiveAddress(NakedAddNameAreaSpacing);
    targets.spacingFinalByte = reinterpret_cast<patch::Address>(&AppendFinalSpacingByte);
    targets.toUpperCall = reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&ToUpperPreservingEscapes));
    targets.addNameAreaGlyph = LiveAddress(NakedAddNameAreaGlyphCount);
    targets.addNudgedNamesGlyph = LiveAddress(NakedAddNudgedNamesGlyphCount);
    targets.curveDrawing = LiveAddress(NakedCurveTextDrawing);
    targets.curveGetSizeFirst = reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&CurveTextGetGlyphCount));
    targets.curveGetSizeSecond = targets.curveGetSizeFirst;
    return targets;
}

namespace {

bool CommitDescriptions(patch::Memory &memory, patch::ExecutableCodeAllocator *allocator,
                        std::vector<patch::PatchDescription> descriptions,
                        patch::BatchResult &result) {
    patch::PatchBatch batch(memory, allocator);
    for (auto &description : descriptions) batch.Add(std::move(description));
    result = batch.Commit();
    return static_cast<bool>(result);
}

}  // namespace

patch::BatchResult PreflightFillVertexBuffer(patch::Memory &memory,
                                             patch::ExecutableCodeAllocator *a) {
    patch::PatchBatch batch(memory, a);
    for (auto &d : FillVertexBufferDescriptions(LiveHookTargets())) batch.Add(std::move(d));
    return batch.Preflight();
}

patch::BatchResult InstallFillVertexBuffer(patch::Memory &memory,
                                           patch::ExecutableCodeAllocator *a) {
    patch::BatchResult result;
    if (!ResolveCStringSlots(memory, "map-text.fill-vertex-buffer", result)) return result;
    const auto targets = LiveHookTargets();
    {
        patch::PatchRuntime runtime(memory);
        const auto pre = FillPreprocessingDescription(targets.fillPreprocessing);
        const auto located = runtime.Preflight(pre);
        if (!located) {
            result.diagnostic = located.diagnostic;
            ClearFillSlots();
            return result;
        }
        FillPreprocessingReturnSlot() = located.ContinuationAddress("return");
        const auto draw = FillDrawingDescription(targets.fillDrawing);
        const auto locatedDraw = runtime.Preflight(draw);
        if (!locatedDraw) {
            result.diagnostic = locatedDraw.diagnostic;
            ClearFillSlots();
            return result;
        }
        FillDrawingReturnSlot() = locatedDraw.ContinuationAddress("return");
    }
    if (!CommitDescriptions(memory, a, FillVertexBufferDescriptions(targets), result)) {
        if (!patch::MustRetainSlots(result)) ClearFillSlots();
    }
    return result;
}

patch::BatchResult PreflightAddNameArea(patch::Memory &memory,
                                        patch::ExecutableCodeAllocator *a) {
    patch::PatchBatch batch(memory, a);
    for (auto &d : AddNameAreaDescriptions(LiveHookTargets())) batch.Add(std::move(d));
    return batch.Preflight();
}

patch::BatchResult InstallAddNameArea(patch::Memory &memory,
                                      patch::ExecutableCodeAllocator *a) {
    patch::BatchResult result;
    if (!ResolveCStringSlots(memory, "map-text.add-name-area", result)) return result;
    const auto targets = LiveHookTargets();
    {
        patch::PatchRuntime runtime(memory);
        const auto spacing = SpacingDescription(targets.spacing);
        const auto located = runtime.Preflight(spacing);
        if (!located) {
            result.diagnostic = located.diagnostic;
            ClearAddNameAreaSlots();
            return result;
        }
        SpacingReturnSlot() = located.ContinuationAddress("return");
        SpacingFinalSlot() = located.ContinuationAddress("final");
        const auto glyph = AddNameAreaGlyphDescription(targets.addNameAreaGlyph);
        const auto locatedGlyph = runtime.Preflight(glyph);
        if (!locatedGlyph) {
            result.diagnostic = locatedGlyph.diagnostic;
            ClearAddNameAreaSlots();
            return result;
        }
        AddNameAreaGlyphReturnSlot() = locatedGlyph.ContinuationAddress("return");
    }
    if (!CommitDescriptions(memory, a, AddNameAreaDescriptions(targets), result)) {
        if (!patch::MustRetainSlots(result)) ClearAddNameAreaSlots();
    }
    return result;
}

patch::BatchResult PreflightAddNudgedNames(patch::Memory &memory,
                                           patch::ExecutableCodeAllocator *a) {
    patch::PatchBatch batch(memory, a);
    for (auto &d : AddNudgedNamesDescriptions(LiveHookTargets())) batch.Add(std::move(d));
    return batch.Preflight();
}

patch::BatchResult InstallAddNudgedNames(patch::Memory &memory,
                                         patch::ExecutableCodeAllocator *a) {
    patch::BatchResult result;
    if (!ResolveCStringSlots(memory, "map-text.add-nudged-names", result)) return result;
    const auto targets = LiveHookTargets();
    {
        patch::PatchRuntime runtime(memory);
        const auto glyph = AddNudgedNamesGlyphDescription(targets.addNudgedNamesGlyph);
        const auto located = runtime.Preflight(glyph);
        if (!located) {
            result.diagnostic = located.diagnostic;
            ClearAddNudgedNamesSlots();
            return result;
        }
        AddNudgedNamesGlyphReturnSlot() = located.ContinuationAddress("return");
    }
    if (!CommitDescriptions(memory, a, AddNudgedNamesDescriptions(targets), result)) {
        if (!patch::MustRetainSlots(result)) ClearAddNudgedNamesSlots();
    }
    return result;
}

patch::BatchResult PreflightCurveText(patch::Memory &memory,
                                      patch::ExecutableCodeAllocator *a) {
    const auto targets = LiveHookTargets();
    patch::PatchBatch batch(memory, a);
    for (auto &d : CurveTextDescriptions(targets)) batch.Add(std::move(d));
    // The three GetSize redirects join the same preflight so their patterns,
    // expected bytes, spans, and reachability all verify before the first
    // map commit anywhere.
    for (auto &d : CurveCallRedirects(targets.curveGetSizeFirst)) {
        batch.Add(std::move(d));
    }
    // Length-call window must also locate uniquely (installed by address).
    std::string error;
    if (!LocateLengthCalls(memory, error)) {
        patch::BatchResult failed;
        failed.diagnostic.feature = "map-text.CurveText.length-calls";
        failed.diagnostic.target = kDiagnosticTargetId;
        failed.diagnostic.operation = patch::PatchOperation::LocatePattern;
        failed.diagnostic.message = error;
        return failed;
    }
    return batch.Preflight();
}

patch::BatchResult InstallCurveText(patch::Memory &memory,
                                    patch::ExecutableCodeAllocator *a) {
    patch::BatchResult result;
    if (!ResolveCStringSlots(memory, "map-text.curve-text", result)) return result;
    const auto targets = LiveHookTargets();
    std::string error;
    const auto lengthCalls = LocateLengthCalls(memory, error);
    if (!lengthCalls) {
        result.diagnostic.feature = "map-text.CurveText.length-calls";
        result.diagnostic.target = kDiagnosticTargetId;
        result.diagnostic.operation = patch::PatchOperation::LocatePattern;
        result.diagnostic.message = error;
        ClearCurveTextSlots();
        return result;
    }
    // Verify both call operands before staging anything.
    for (const auto &[site, expected] : {
             std::make_pair(lengthCalls->first,
                            std::vector<std::uint8_t>(
                                kCurveFirstCallOriginal.begin(),
                                kCurveFirstCallOriginal.end())),
             std::make_pair(lengthCalls->second,
                            std::vector<std::uint8_t>(
                                kCurveSecondCallOriginal.begin(),
                                kCurveSecondCallOriginal.end())),
         }) {
        std::vector<std::uint8_t> actual(expected.size());
        if (!memory.Read(site, actual.data(), actual.size(), error) ||
            actual != expected) {
            result.diagnostic.feature = "map-text.CurveText.glyph-count-call";
            result.diagnostic.target = kDiagnosticTargetId;
            result.diagnostic.operation = patch::PatchOperation::VerifyOriginalBytes;
            result.diagnostic.message =
                error.empty() ? "length-call bytes do not match" : error;
            ClearCurveTextSlots();
            return result;
        }
    }
    {
        patch::PatchRuntime runtime(memory);
        const auto drawing = CurveDrawingDescription(targets.curveDrawing);
        const auto located = runtime.Preflight(drawing);
        if (!located) {
            result.diagnostic = located.diagnostic;
            ClearCurveTextSlots();
            return result;
        }
        CurveDrawingReturnSlot() = located.ContinuationAddress("return");
    }
    // One atomic batch: drawing jump + loop-init jump + both call redirects.
    patch::PatchBatch batch(memory, a);
    batch.Add(CurveDrawingDescription(targets.curveDrawing));
    for (auto &d : CurveCallRedirects(targets.curveGetSizeFirst)) {
        batch.Add(std::move(d));
    }
    result = batch.Commit();
    if (!result && !patch::MustRetainSlots(result)) {
        ClearCurveTextSlots();
    }
    return result;
}

patch::BatchResult PreflightMapText(patch::Memory &memory,
                                    patch::ExecutableCodeAllocator *a) {
    for (const auto preflight :
         {PreflightFillVertexBuffer, PreflightAddNameArea, PreflightAddNudgedNames,
          PreflightCurveText}) {
        const auto result = preflight(memory, a);
        if (!result) return result;
    }
    patch::BatchResult ok;
    ok.diagnostic.success = true;
    ok.diagnostic.feature = "map-text";
    ok.diagnostic.target = kDiagnosticTargetId;
    ok.diagnostic.operation = patch::PatchOperation::VerifyOriginalBytes;
    ok.diagnostic.message = "all map-text clusters preflight cleanly";
    return ok;
}

patch::BatchResult InstallMapText(patch::Memory &memory,
                                  patch::ExecutableCodeAllocator *a) {
    // Shared CString callees resolve once up front: every cluster below
    // needs them, and clearing them on a later cluster failure would orphan
    // already-installed earlier clusters. They clear only when zero
    // clusters installed successfully.
    {
        patch::BatchResult probe;
        if (!ResolveCStringSlots(memory, "map-text", probe)) {
            ClearCalleeSlots();
            return probe;
        }
    }
    const char *names[] = {"fill-vertex-buffer", "add-name-area", "add-nudged-names",
                           "curve-text"};
    patch::BatchResult (*installers[])(patch::Memory &,
                                       patch::ExecutableCodeAllocator *) = {
        InstallFillVertexBuffer, InstallAddNameArea, InstallAddNudgedNames,
        InstallCurveText};
    patch::BatchResult result;
    std::size_t installed = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        result = installers[i](memory, a);
        std::fprintf(stderr, "eu4dll_linux [mapText] cluster %-18s %s: %s\n", names[i],
                     static_cast<bool>(result) ? "installed" : "FAILED",
                     patch::FormatDiagnostic(result.diagnostic).c_str());
        if (!result) {
            // Earlier clusters stay installed by design (diagnosable
            // bisection); their slots are untouched above. Callees clear
            // only on a pristine failure.
            if (installed == 0) ClearCalleeSlots();
            return result;
        }
        ++installed;
    }
    result.diagnostic.feature = "map-text";
    result.diagnostic.message = "all map-text clusters installed";
    return result;
}

}  // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::map_text
