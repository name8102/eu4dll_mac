#include "targets/eu4_1_37_5/linux_x86_64/text_3d/text_3d_patch.h"

#include "features/escaped_text/escaped_text.h"
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::text_3d {
namespace {

namespace escaped = eu4dll::escaped_text;

patch::PatchDescription Contract(const char *feature, const char *pattern) {
    patch::PatchDescription description;
    description.feature = feature;
    description.target = kDiagnosticTargetId;
    description.location.pattern = pattern;
    description.location.requireUnique = true;
    description.location.scope = patch::SearchScope::Symbol(
        kRender3dSymbol, kRender3dSearchSize);
    return description;
}

}  // namespace

extern "C" {
__attribute__((visibility("hidden"))) uintptr_t g_linuxText3DPreprocessingReturnAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxText3DDrawingReturnAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxText3DCStringAppendCharAddress = 0;
}  // extern "C"

patch::Address &CStringAppendCharSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxText3DCStringAppendCharAddress);
}
patch::Address &PreprocessingReturnSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxText3DPreprocessingReturnAddress);
}
patch::Address &DrawingReturnSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxText3DDrawingReturnAddress);
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

__attribute__((naked)) void NakedRender3dPreprocessing() {
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "mov rdx, rax\n"
        "movzx eax, byte ptr [rax]\n" EU4DLL_LINUX_DECODE_ESCAPE("eax", "rdx")
        "5:\n"
        "push rax\n"
        "push rdx\n"
        "lea rdi, [rsp + 0x68]\n"
        "movzx esi, byte ptr [rdx + 1]\n"
        "call qword ptr [rip + g_linuxText3DCStringAppendCharAddress]\n"
        "mov rdx, qword ptr [rsp]\n"
        "lea rdi, [rsp + 0x68]\n"
        "movzx esi, byte ptr [rdx + 2]\n"
        "call qword ptr [rip + g_linuxText3DCStringAppendCharAddress]\n"
        "pop rdx\n"
        "pop rax\n"
        "add r15d, 2\n"
        "cmp eax, 256\n"
        "jb 7f\n"
        "add eax, %c[index_shift]\n"
        "7:\n"
        "mov rbx, qword ptr [r12 + rax * 8 + 0x100]\n"
        "jmp qword ptr [rip + g_linuxText3DPreprocessingReturnAddress]\n"
        ".att_syntax prefix\n"
        :
        : EU4DLL_LINUX_ESCAPE_OPERANDS);
}

__attribute__((naked)) void NakedRender3dDrawing() {
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "mov rdx, rax\n"
        "movzx eax, byte ptr [rax]\n" EU4DLL_LINUX_DECODE_ESCAPE("eax", "rdx")
        "5:\n"
        "add r15d, 2\n"
        "cmp eax, 256\n"
        "jb 7f\n"
        "add eax, %c[index_shift]\n"
        "7:\n"
        "mov rax, qword ptr [r12 + rax * 8 + 0x100]\n"
        "jmp qword ptr [rip + g_linuxText3DDrawingReturnAddress]\n"
        ".att_syntax prefix\n"
        :
        : EU4DLL_LINUX_ESCAPE_OPERANDS);
}

#undef EU4DLL_LINUX_ESCAPE_OPERANDS
#undef EU4DLL_LINUX_DECODE_ESCAPE

patch::PatchDescription PreprocessingDescription(patch::Address hookTarget) {
    auto description = Contract("text-3d.CBitmapFont.Render3d.preprocessing",
                                kPreprocessingPattern);
    description.expected = patch::ExpectedBytes{
        0,
        {kPreprocessingOriginal.begin(),
         kPreprocessingOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::Jump;
    description.mutation.target = hookTarget;
    description.continuations = {
        {"return", kPreprocessingContinuationOffset}};
    return description;
}

patch::PatchDescription DrawingDescription(patch::Address hookTarget) {
    auto description = Contract("text-3d.CBitmapFont.Render3d.drawing",
                                kDrawingPattern);
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

std::vector<patch::PatchDescription> Text3DDescriptions(
    const Text3DHookTargets &targets) {
    return {PreprocessingDescription(targets.preprocessing),
            DrawingDescription(targets.drawing)};
}

namespace {

Text3DHookTargets LiveHookTargets() {
    Text3DHookTargets targets;
    targets.preprocessing = reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&NakedRender3dPreprocessing));
    targets.drawing = reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&NakedRender3dDrawing));
    return targets;
}

void ClearSlots() {
    PreprocessingReturnSlot() = 0;
    DrawingReturnSlot() = 0;
    CStringAppendCharSlot() = 0;
}

}  // namespace

patch::BatchResult PreflightText3D(patch::Memory &memory,
                                   patch::ExecutableCodeAllocator *allocator) {
    patch::PatchBatch batch(memory, allocator);
    for (auto &description : Text3DDescriptions(LiveHookTargets())) {
        batch.Add(std::move(description));
    }
    return batch.Preflight();
}

patch::BatchResult InstallText3D(patch::Memory &memory,
                                 patch::ExecutableCodeAllocator *allocator) {
    patch::BatchResult result;
    {
        std::string error;
        const auto callee =
            memory.ResolveSymbol(kCStringAppendCharSymbol, error);
        if (!callee) {
            result.diagnostic.feature = "text-3d.CBitmapFont.Render3d.preprocessing";
            result.diagnostic.target = kDiagnosticTargetId;
            result.diagnostic.operation = patch::PatchOperation::ResolveSymbol;
            result.diagnostic.message = error;
            return result;
        }
        CStringAppendCharSlot() = *callee;
    }
    const auto targets = LiveHookTargets();
    {
        patch::PatchRuntime runtime(memory);
        const auto pre = PreprocessingDescription(targets.preprocessing);
        const auto located = runtime.Preflight(pre);
        if (!located) {
            result.diagnostic = located.diagnostic;
            ClearSlots();
            return result;
        }
        PreprocessingReturnSlot() = located.ContinuationAddress("return");
        const auto draw = DrawingDescription(targets.drawing);
        const auto locatedDraw = runtime.Preflight(draw);
        if (!locatedDraw) {
            result.diagnostic = locatedDraw.diagnostic;
            ClearSlots();
            return result;
        }
        DrawingReturnSlot() = locatedDraw.ContinuationAddress("return");
    }
    patch::PatchBatch batch(memory, allocator);
    for (auto &description : Text3DDescriptions(targets)) {
        batch.Add(std::move(description));
    }
    result = batch.Commit();
    // See base: retain slots on unconfirmed rollback (hook may be live).
    if (!result && !patch::MustRetainSlots(result)) {
        ClearSlots();
    }
    return result;
}

}  // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::text_3d
