#include "targets/eu4_1_37_5/linux_x86_64/tooltip_text/tooltip_patch.h"

#include "features/escaped_text/escaped_text.h"
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::tooltip {
namespace {

namespace escaped = eu4dll::escaped_text;

patch::PatchDescription Contract(const char *feature, const char *pattern) {
    patch::PatchDescription description;
    description.feature = feature;
    description.target = kDiagnosticTargetId;
    description.location.pattern = pattern;
    description.location.requireUnique = true;
    description.location.scope = patch::SearchScope::Symbol(
        kRenderToTextureSymbol, kRenderToTextureSearchSize);
    return description;
}

}  // namespace

extern "C" {
__attribute__((visibility("hidden"))) uintptr_t g_linuxRenderToTexturePreprocessingReturnAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxRenderToTextureWrappingReturnAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxRenderToTextureWrappingBypassAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxRenderToTextureDrawingReturnAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxCStringAppendCharAddress = 0;
__attribute__((visibility("hidden"))) uint32_t g_linuxRenderToTextureCurrentCharacter = 0;
}  // extern "C"

patch::Address &CStringAppendCharSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxCStringAppendCharAddress);
}
std::uint32_t &RenderToTextureCurrentCharacter() {
    return g_linuxRenderToTextureCurrentCharacter;
}
patch::Address &PreprocessingReturnSlot() {
    return reinterpret_cast<patch::Address &>(
        g_linuxRenderToTexturePreprocessingReturnAddress);
}
patch::Address &WrappingReturnSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxRenderToTextureWrappingReturnAddress);
}
patch::Address &WrappingBypassSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxRenderToTextureWrappingBypassAddress);
}
patch::Address &DrawingReturnSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxRenderToTextureDrawingReturnAddress);
}

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
    "cmp byte ptr [rdx + 1], 0\n"                                     \
        "je 7f\n"                                                          \
        "cmp byte ptr [rdx + 2], 0\n"                                     \
        "je 7f\n"                                                          \
        "movzx " indexRegister ", word ptr [" addressRegister " + 1]\n"      \
    "jmp 5f\n"                                                           \
    "2:\n"                                                               \
    "cmp byte ptr [rdx + 1], 0\n"                                     \
        "je 7f\n"                                                          \
        "cmp byte ptr [rdx + 2], 0\n"                                     \
        "je 7f\n"                                                          \
        "movzx " indexRegister ", word ptr [" addressRegister " + 1]\n"      \
    "sub " indexRegister ", %c[shift2]\n"                                \
    "jmp 5f\n"                                                           \
    "3:\n"                                                               \
    "cmp byte ptr [rdx + 1], 0\n"                                     \
        "je 7f\n"                                                          \
        "cmp byte ptr [rdx + 2], 0\n"                                     \
        "je 7f\n"                                                          \
        "movzx " indexRegister ", word ptr [" addressRegister " + 1]\n"      \
    "add " indexRegister ", %c[shift3]\n"                                \
    "jmp 5f\n"                                                           \
    "4:\n"                                                               \
    "cmp byte ptr [rdx + 1], 0\n"                                     \
        "je 7f\n"                                                          \
        "cmp byte ptr [rdx + 2], 0\n"                                     \
        "je 7f\n"                                                          \
        "movzx " indexRegister ", word ptr [" addressRegister " + 1]\n"      \
    "add " indexRegister ", %c[shift4]\n"

#define EU4DLL_LINUX_ESCAPE_OPERANDS                                            \
    [escape1] "i"(escaped::kEscape1), [escape2] "i"(escaped::kEscape2),         \
        [escape3] "i"(escaped::kEscape3), [escape4] "i"(escaped::kEscape4),     \
        [shift2] "i"(escaped::kEscape2Shift), [shift3] "i"(escaped::kEscape3Shift), \
        [shift4] "i"(escaped::kEscape4Shift),                                   \
        [index_shift] "i"(base::kCharacterIndexShift)

__attribute__((naked)) void NakedRenderToTexturePreprocessing() {
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "mov rdx, rax\n"
        "movzx eax, byte ptr [rax]\n" EU4DLL_LINUX_DECODE_ESCAPE("eax", "rdx")
        "5:\n"
        "push rax\n"
        "push rdx\n"
        "lea rdi, [rsp + 0x68]\n"
        "movzx esi, byte ptr [rdx + 1]\n"
        "call qword ptr [rip + g_linuxCStringAppendCharAddress]\n"
        "mov rdx, qword ptr [rsp]\n"
        "lea rdi, [rsp + 0x68]\n"
        "movzx esi, byte ptr [rdx + 2]\n"
        "call qword ptr [rip + g_linuxCStringAppendCharAddress]\n"
        "pop rdx\n"
        "pop rax\n"
        "add r14d, 2\n"
        "cmp eax, 256\n"
        "jb 6f\n"
        "add eax, %c[index_shift]\n"
        "6:\n"
        "mov dword ptr [rip + g_linuxRenderToTextureCurrentCharacter], eax\n"
        "mov rcx, qword ptr [rsp + 0x38]\n"
        "mov rbp, qword ptr [rcx + rax*8 + 0x100]\n"
        "jmp qword ptr [rip + g_linuxRenderToTexturePreprocessingReturnAddress]\n"
        "7:\n"
        "mov dword ptr [rip + g_linuxRenderToTextureCurrentCharacter], eax\n"
        "mov rcx, qword ptr [rsp + 0x38]\n"
        "mov rbp, qword ptr [rcx + rax*8 + 0x100]\n"
        "jmp qword ptr [rip + g_linuxRenderToTexturePreprocessingReturnAddress]\n"
        ".att_syntax prefix\n"
        :
        : EU4DLL_LINUX_ESCAPE_OPERANDS);
}

__attribute__((naked)) void NakedRenderToTextureWrapping() {
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "cmp dword ptr [rip + g_linuxRenderToTextureCurrentCharacter], 0xff\n"
        "ja 1f\n"
        "cmp word ptr [rbp + 0x6], 0\n"
        "jmp qword ptr [rip + g_linuxRenderToTextureWrappingReturnAddress]\n"
        "1:\n"
        "jmp qword ptr [rip + g_linuxRenderToTextureWrappingBypassAddress]\n"
        ".att_syntax prefix\n");
}

__attribute__((naked)) void NakedRenderToTextureDrawing() {
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "mov rdx, rax\n"
        "movzx eax, byte ptr [rax]\n" EU4DLL_LINUX_DECODE_ESCAPE("eax", "rdx")
        "5:\n"
        "add r14d, 2\n"
        "cmp eax, 256\n"
        "jb 6f\n"
        "add eax, %c[index_shift]\n"
        "6:\n"
        "mov r11, qword ptr [r13 + rax*8 + 0x100]\n"
        "jmp qword ptr [rip + g_linuxRenderToTextureDrawingReturnAddress]\n"
        "7:\n"
        "mov r11, qword ptr [r13 + rax*8 + 0x100]\n"
        "jmp qword ptr [rip + g_linuxRenderToTextureDrawingReturnAddress]\n"
        ".att_syntax prefix\n"
        :
        : EU4DLL_LINUX_ESCAPE_OPERANDS);
}

#undef EU4DLL_LINUX_ESCAPE_OPERANDS
#undef EU4DLL_LINUX_DECODE_ESCAPE

patch::PatchDescription PreprocessingDescription(patch::Address hookTarget) {
    auto description = Contract(
        "tooltip.CBitmapFont.RenderToTexture.preprocessing", kPreprocessingPattern);
    description.expected = patch::ExpectedBytes{
        0,
        {kPreprocessingOriginal.begin(), kPreprocessingOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::Jump;
    description.mutation.target = hookTarget;
    description.continuations = {
        {"return", kPreprocessingContinuationOffset}};
    return description;
}

patch::PatchDescription WrappingDescription(patch::Address hookTarget) {
    auto description =
        Contract("tooltip.CBitmapFont.RenderToTexture.wrapping", kWrappingPattern);
    description.expected = patch::ExpectedBytes{
        0, {kWrappingOriginal.begin(), kWrappingOriginal.end()}, {}};
    description.mutation.kind = patch::MutationKind::Jump;
    description.mutation.target = hookTarget;
    description.continuations = {
        {"return", kWrappingContinuationOffset},
        {"bypass", kWrappingBypassOffset}};
    return description;
}

patch::PatchDescription DrawingDescription(patch::Address hookTarget) {
    auto description =
        Contract("tooltip.CBitmapFont.RenderToTexture.drawing", kDrawingPattern);
    description.expected = patch::ExpectedBytes{
        0,
        {kDrawingOriginal.begin(), kDrawingOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::Jump;
    description.mutation.target = hookTarget;
    description.continuations = {
        {"return", kDrawingContinuationOffset}};
    return description;
}

std::vector<patch::PatchDescription> TooltipDescriptions(
    const TooltipHookTargets &targets) {
    return {PreprocessingDescription(targets.preprocessing),
            WrappingDescription(targets.wrapping),
            DrawingDescription(targets.drawing)};
}

namespace {

TooltipHookTargets LiveHookTargets() {
    TooltipHookTargets targets;
    targets.preprocessing = reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&NakedRenderToTexturePreprocessing));
    targets.wrapping = reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&NakedRenderToTextureWrapping));
    targets.drawing = reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&NakedRenderToTextureDrawing));
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
    WrappingReturnSlot() = 0;
    WrappingBypassSlot() = 0;
    DrawingReturnSlot() = 0;
    CStringAppendCharSlot() = 0;
    RenderToTextureCurrentCharacter() = 0;
}

}  // namespace

patch::BatchResult PreflightTooltip(patch::Memory &memory,
                                    patch::ExecutableCodeAllocator *allocator) {
    patch::PatchBatch batch(memory, allocator);
    for (auto &description : TooltipDescriptions(LiveHookTargets())) {
        batch.Add(std::move(description));
    }
    return batch.Preflight();
}

patch::BatchResult InstallTooltip(patch::Memory &memory,
                                  patch::ExecutableCodeAllocator *allocator) {
    const auto targets = LiveHookTargets();
    // Resolve the external callee first: the preprocessing hook body calls
    // it, so a missing symbol fails closed before any slot or mutation.
    {
        std::string error;
        const auto callee =
            memory.ResolveSymbol(kCStringAppendCharSymbol, error);
        if (!callee) {
            patch::BatchResult failed;
            failed.diagnostic.feature = "tooltip.CBitmapFont.RenderToTexture.preprocessing";
            failed.diagnostic.target = kDiagnosticTargetId;
            failed.diagnostic.operation = patch::PatchOperation::ResolveSymbol;
            failed.diagnostic.message = error;
            return failed;
        }
        CStringAppendCharSlot() = *callee;
    }
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
            {wrapping, "return", &WrappingReturnSlot()},
            {wrapping, "bypass", &WrappingBypassSlot()},
            {drawing, "return", &DrawingReturnSlot()},
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
                               "tooltip continuation is missing");
            }
            *request.slot = continuation;
        }
    }

    patch::PatchBatch batch(memory, allocator);
    for (auto &description : TooltipDescriptions(targets)) {
        batch.Add(std::move(description));
    }
    auto result = batch.Commit();
    if (!result) {
        ClearSlots();
    }
    return result;
}

}  // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::tooltip
