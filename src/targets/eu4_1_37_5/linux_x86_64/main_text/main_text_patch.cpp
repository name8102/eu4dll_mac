#include "targets/eu4_1_37_5/linux_x86_64/main_text/main_text_patch.h"

#include "features/escaped_text/escaped_text.h"
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::main_text {
namespace {

namespace escaped = eu4dll::escaped_text;

patch::PatchDescription Contract(const char *feature, const char *pattern) {
    patch::PatchDescription description;
    description.feature = feature;
    description.target = kDiagnosticTargetId;
    description.location.pattern = pattern;
    description.location.requireUnique = true;
    description.location.scope = patch::SearchScope::Symbol(
        kRenderToScreenSymbol, kRenderToScreenSearchSize);
    return description;
}

}  // namespace

extern "C" {
__attribute__((visibility("hidden"))) uintptr_t g_linuxRenderToScreenPreprocessingReturnAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxRenderToScreenPreprocessingBypassAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxRenderToScreenWrappingReturnAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxRenderToScreenWrappingBypassAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxRenderToScreenDrawingReturnAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxRenderToScreenDrawingBypassAddress = 0;
__attribute__((visibility("hidden"))) uint32_t g_linuxRenderToScreenCurrentCharacter = 0;
}  // extern "C"

std::uint32_t &RenderToScreenCurrentCharacter() {
    return g_linuxRenderToScreenCurrentCharacter;
}
patch::Address &PreprocessingReturnSlot() {
    return reinterpret_cast<patch::Address &>(
        g_linuxRenderToScreenPreprocessingReturnAddress);
}
patch::Address &PreprocessingBypassSlot() {
    return reinterpret_cast<patch::Address &>(
        g_linuxRenderToScreenPreprocessingBypassAddress);
}
patch::Address &WrappingReturnSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxRenderToScreenWrappingReturnAddress);
}
patch::Address &WrappingBypassSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxRenderToScreenWrappingBypassAddress);
}
patch::Address &DrawingReturnSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxRenderToScreenDrawingReturnAddress);
}
patch::Address &DrawingBypassSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxRenderToScreenDrawingBypassAddress);
}

// Escape decoder without the tail: the caller supplies labels 5/6/7
// (compare/shift decisions differ per site). Marker/shift immediates reuse
// the portable escaped_text policy.
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
    "movzx " indexRegister ", word ptr [" addressRegister " + 1]\n"      \
    "jmp 5f\n"                                                           \
    "2:\n"                                                               \
    "movzx " indexRegister ", word ptr [" addressRegister " + 1]\n"      \
    "sub " indexRegister ", %c[shift2]\n"                                \
    "jmp 5f\n"                                                           \
    "3:\n"                                                               \
    "movzx " indexRegister ", word ptr [" addressRegister " + 1]\n"      \
    "add " indexRegister ", %c[shift3]\n"                                \
    "jmp 5f\n"                                                           \
    "4:\n"                                                               \
    "movzx " indexRegister ", word ptr [" addressRegister " + 1]\n"      \
    "add " indexRegister ", %c[shift4]\n"

#define EU4DLL_LINUX_ESCAPE_OPERANDS                                            \
    [escape1] "i"(escaped::kEscape1), [escape2] "i"(escaped::kEscape2),         \
        [escape3] "i"(escaped::kEscape3), [escape4] "i"(escaped::kEscape4),     \
        [shift2] "i"(escaped::kEscape2Shift), [shift3] "i"(escaped::kEscape3Shift), \
        [shift4] "i"(escaped::kEscape4Shift),                                   \
        [index_shift] "i"(base::kCharacterIndexShift)

__attribute__((naked)) void NakedRenderToScreenPreprocessing() {
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "lea rdx, [r14 + rbp]\n"
        "movzx ecx, byte ptr [rdx]\n" EU4DLL_LINUX_DECODE_ESCAPE("ecx", "rdx")
        "5:\n"
        "push rax\n"
        "movsxd rax, ebx\n"
        "mov dx, word ptr [r14 + rbp + 1]\n"
        "mov word ptr [rax + %c[stage]], dx\n"
        "pop rax\n"
        "add r13d, 2\n"
        "add r12d, 2\n"
        "cmp ecx, 256\n"
        "jb 6f\n"
        "add ecx, %c[index_shift]\n"
        "6:\n"
        "mov dword ptr [rip + g_linuxRenderToScreenCurrentCharacter], ecx\n"
        "jmp qword ptr [rip + g_linuxRenderToScreenPreprocessingBypassAddress]\n"
        "7:\n"
        "mov dword ptr [rip + g_linuxRenderToScreenCurrentCharacter], ecx\n"
        "jmp qword ptr [rip + g_linuxRenderToScreenPreprocessingReturnAddress]\n"
        ".att_syntax prefix\n"
        :
        : EU4DLL_LINUX_ESCAPE_OPERANDS,
          [stage] "i"(kPreprocessingStageAddress));
}

__attribute__((naked)) void NakedRenderToScreenWrapping() {
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "cmp dword ptr [rip + g_linuxRenderToScreenCurrentCharacter], 0xff\n"
        "ja 1f\n"
        "cmp word ptr [rbp + 0x6], 0\n"
        "jmp qword ptr [rip + g_linuxRenderToScreenWrappingReturnAddress]\n"
        "1:\n"
        "jmp qword ptr [rip + g_linuxRenderToScreenWrappingBypassAddress]\n"
        ".att_syntax prefix\n");
}

__attribute__((naked)) void NakedRenderToScreenDrawing() {
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "lea rdx, [rbx + %c[draw_base]]\n"
        "movzx eax, byte ptr [rdx]\n" EU4DLL_LINUX_DECODE_ESCAPE("eax", "rdx")
        "5:\n"
        "add r15d, 2\n"
        "cmp eax, 256\n"
        "jb 6f\n"
        "add eax, %c[index_shift]\n"
        "6:\n"
        "jmp qword ptr [rip + g_linuxRenderToScreenDrawingBypassAddress]\n"
        "7:\n"
        "jmp qword ptr [rip + g_linuxRenderToScreenDrawingReturnAddress]\n"
        ".att_syntax prefix\n"
        :
        : EU4DLL_LINUX_ESCAPE_OPERANDS,
          [draw_base] "i"(kDrawingObjectDisplacement));
}

#undef EU4DLL_LINUX_ESCAPE_OPERANDS
#undef EU4DLL_LINUX_DECODE_ESCAPE

patch::PatchDescription PreprocessingDescription(patch::Address hookTarget) {
    auto description = Contract(
        "main-text.CBitmapFont.RenderToScreen.preprocessing",
        kPreprocessingPattern);
    description.expected = patch::ExpectedBytes{
        0,
        {kPreprocessingOriginal.begin(),
         kPreprocessingOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::Jump;
    description.mutation.target = hookTarget;
    description.continuations = {
        {"return", kPreprocessingContinuationOffset},
        {"bypass", kPreprocessingBypassOffset}};
    return description;
}

patch::PatchDescription WrappingDescription(patch::Address hookTarget) {
    auto description =
        Contract("main-text.CBitmapFont.RenderToScreen.wrapping", kWrappingPattern);
    description.expected = patch::ExpectedBytes{
        0,
        {kWrappingOriginal.begin(), kWrappingOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::Jump;
    description.mutation.target = hookTarget;
    description.continuations = {
        {"return", kWrappingContinuationOffset},
        {"bypass", kWrappingBypassOffset}};
    return description;
}

patch::PatchDescription DrawingDescription(patch::Address hookTarget) {
    auto description =
        Contract("main-text.CBitmapFont.RenderToScreen.drawing", kDrawingPattern);
    description.expected = patch::ExpectedBytes{
        0,
        {kDrawingOriginal.begin(), kDrawingOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::Jump;
    description.mutation.target = hookTarget;
    description.continuations = {
        {"return", kDrawingContinuationOffset},
        {"bypass", kDrawingBypassOffset}};
    return description;
}

std::vector<patch::PatchDescription> MainTextDescriptions(
    const MainTextHookTargets &targets) {
    return {PreprocessingDescription(targets.preprocessing),
            WrappingDescription(targets.wrapping),
            DrawingDescription(targets.drawing)};
}

namespace {

MainTextHookTargets LiveHookTargets() {
    MainTextHookTargets targets;
    targets.preprocessing = reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&NakedRenderToScreenPreprocessing));
    targets.wrapping = reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&NakedRenderToScreenWrapping));
    targets.drawing = reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&NakedRenderToScreenDrawing));
    return targets;
}

patch::BatchResult Failure(const char *feature, patch::PatchOperation operation,
                           const std::string &message) {
    patch::BatchResult failed;
    failed.diagnostic.feature = feature;
    failed.diagnostic.target = kDiagnosticTargetId;
    failed.diagnostic.operation = operation;
    failed.diagnostic.message = message;
    return failed;
}

void ClearSlots() {
    PreprocessingReturnSlot() = 0;
    PreprocessingBypassSlot() = 0;
    WrappingReturnSlot() = 0;
    WrappingBypassSlot() = 0;
    DrawingReturnSlot() = 0;
    DrawingBypassSlot() = 0;
    RenderToScreenCurrentCharacter() = 0;
}

}  // namespace

patch::BatchResult PreflightMainText(patch::Memory &memory,
                                     patch::ExecutableCodeAllocator *allocator) {
    patch::PatchBatch batch(memory, allocator);
    for (auto &description : MainTextDescriptions(LiveHookTargets())) {
        batch.Add(std::move(description));
    }
    return batch.Preflight();
}

patch::BatchResult InstallMainText(patch::Memory &memory,
                                   patch::ExecutableCodeAllocator *allocator) {
    const auto targets = LiveHookTargets();
    {
        patch::PatchRuntime runtime(memory);
        struct SlotRequest {
            patch::PatchDescription description;
            const char *name;
            patch::Address *slot;
        };
        const auto preprocessing = PreprocessingDescription(targets.preprocessing);
        const auto wrapping = WrappingDescription(targets.wrapping);
        const auto drawing = DrawingDescription(targets.drawing);
        SlotRequest requests[] = {
            {preprocessing, "return", &PreprocessingReturnSlot()},
            {preprocessing, "bypass", &PreprocessingBypassSlot()},
            {wrapping, "return", &WrappingReturnSlot()},
            {wrapping, "bypass", &WrappingBypassSlot()},
            {drawing, "return", &DrawingReturnSlot()},
            {drawing, "bypass", &DrawingBypassSlot()},
        };
        for (const auto &request : requests) {
            const auto located = runtime.Preflight(request.description);
            if (!located) {
                patch::BatchResult failed;
                failed.diagnostic = located.diagnostic;
                ClearSlots();
                return failed;
            }
            const auto continuation = located.ContinuationAddress(request.name);
            if (continuation == 0) {
                ClearSlots();
                return Failure(request.description.feature.c_str(),
                               patch::PatchOperation::CalculateMutation,
                               "main-text continuation is missing");
            }
            *request.slot = continuation;
        }
    }

    patch::PatchBatch batch(memory, allocator);
    for (auto &description : MainTextDescriptions(targets)) {
        batch.Add(std::move(description));
    }
    auto result = batch.Commit();
    if (!result) {
        ClearSlots();
    }
    return result;
}

}  // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::main_text
