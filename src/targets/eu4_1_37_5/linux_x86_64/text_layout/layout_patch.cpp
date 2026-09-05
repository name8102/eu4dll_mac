#include "targets/eu4_1_37_5/linux_x86_64/text_layout/layout_patch.h"

#include "features/escaped_text/escaped_text.h"
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::layout {
namespace {

namespace escaped = eu4dll::escaped_text;

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

extern "C" {
__attribute__((visibility("hidden"))) uintptr_t g_linuxGetHeightOfStringReturnAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxGetWidthOfStringReturnAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxGetWidthOfStringBypassAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxGetActualRequiredSizeReturnAddress = 0;
__attribute__((visibility("hidden"))) uintptr_t g_linuxGetRequiredSizeReturnAddress = 0;
__attribute__((visibility("hidden")))
uintptr_t g_linuxGetActualRealRequiredSizeActuallyReturnAddress = 0;
}  // extern "C"

patch::Address &GetHeightOfStringReturnSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxGetHeightOfStringReturnAddress);
}
patch::Address &GetWidthOfStringReturnSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxGetWidthOfStringReturnAddress);
}
patch::Address &GetWidthOfStringBypassSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxGetWidthOfStringBypassAddress);
}
patch::Address &GetActualRequiredSizeReturnSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxGetActualRequiredSizeReturnAddress);
}
patch::Address &GetRequiredSizeReturnSlot() {
    return reinterpret_cast<patch::Address &>(g_linuxGetRequiredSizeReturnAddress);
}
patch::Address &GetActualRealRequiredSizeActuallyReturnSlot() {
    return reinterpret_cast<patch::Address &>(
        g_linuxGetActualRealRequiredSizeActuallyReturnAddress);
}

// Shared escape decoder: the marker/shift immediates reuse the portable
// escaped_text policy; only the index shift and table base are Linux target
// facts. `consumed` advances the site-specific length counter.
#define EU4DLL_LINUX_DECODE_ESCAPE(indexRegister, consumedInstruction) \
    "cmp " indexRegister ", %c[escape1]\n"                             \
    "je 1f\n"                                                          \
    "cmp " indexRegister ", %c[escape2]\n"                             \
    "je 2f\n"                                                          \
    "cmp " indexRegister ", %c[escape3]\n"                             \
    "je 3f\n"                                                          \
    "cmp " indexRegister ", %c[escape4]\n"                             \
    "je 4f\n"                                                          \
    "jmp 7f\n"                                                         \
    "1:\n"                                                             \
    "cmp byte ptr [rdx + 1], 0\n"                                     \
        "je 7f\n"                                                          \
        "cmp byte ptr [rdx + 2], 0\n"                                     \
        "je 7f\n"                                                          \
        "movzx " indexRegister ", word ptr [rdx + 1]\n"                    \
    "jmp 5f\n"                                                         \
    "2:\n"                                                             \
    "cmp byte ptr [rdx + 1], 0\n"                                     \
        "je 7f\n"                                                          \
        "cmp byte ptr [rdx + 2], 0\n"                                     \
        "je 7f\n"                                                          \
        "movzx " indexRegister ", word ptr [rdx + 1]\n"                    \
    "sub " indexRegister ", %c[shift2]\n"                              \
    "jmp 5f\n"                                                         \
    "3:\n"                                                             \
    "cmp byte ptr [rdx + 1], 0\n"                                     \
        "je 7f\n"                                                          \
        "cmp byte ptr [rdx + 2], 0\n"                                     \
        "je 7f\n"                                                          \
        "movzx " indexRegister ", word ptr [rdx + 1]\n"                    \
    "add " indexRegister ", %c[shift3]\n"                              \
    "jmp 5f\n"                                                         \
    "4:\n"                                                             \
    "cmp byte ptr [rdx + 1], 0\n"                                     \
        "je 7f\n"                                                          \
        "cmp byte ptr [rdx + 2], 0\n"                                     \
        "je 7f\n"                                                          \
        "movzx " indexRegister ", word ptr [rdx + 1]\n"                    \
    "add " indexRegister ", %c[shift4]\n"                              \
    "5:\n"                                                             \
    consumedInstruction "\n"                                          \
    "cmp " indexRegister ", 256\n"                                     \
    "jb 7f\n"                                                          \
    "add " indexRegister ", %c[index_shift]\n"                         \
    "7:\n"

#define EU4DLL_LINUX_ESCAPE_OPERANDS                                            \
    [escape1] "i"(escaped::kEscape1), [escape2] "i"(escaped::kEscape2),         \
        [escape3] "i"(escaped::kEscape3), [escape4] "i"(escaped::kEscape4),     \
        [shift2] "i"(escaped::kEscape2Shift), [shift3] "i"(escaped::kEscape3Shift), \
        [shift4] "i"(escaped::kEscape4Shift),                                   \
        [index_shift] "i"(base::kCharacterIndexShift)

__attribute__((naked)) void NakedGetHeightOfString() {
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "push rdx\n"
        "mov rdx, rax\n"
        "movzx eax, byte ptr [rax]\n" EU4DLL_LINUX_DECODE_ESCAPE("eax", "add ebp, 2")
        "pop rdx\n"
        "mov rax, qword ptr [rbx + rax*8 + 0x100]\n"
        "jmp qword ptr [rip + g_linuxGetHeightOfStringReturnAddress]\n"
        ".att_syntax prefix\n"
        :
        : EU4DLL_LINUX_ESCAPE_OPERANDS);
}

__attribute__((naked)) void NakedGetWidthOfString() {
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "push rdx\n"
        "push rcx\n"
        "mov ecx, edx\n"
        "lea rdx, [rbx + r15]\n" EU4DLL_LINUX_DECODE_ESCAPE("esi", "add r14d, 2\ncmp r14d, ecx\njge 8f")
        "pop rcx\n"
        "pop rdx\n"
        "mov rbp, qword ptr [rdi + rsi*8 + 0x100]\n"
        "jmp qword ptr [rip + g_linuxGetWidthOfStringReturnAddress]\n"
        "8:\n"
        "pop rcx\n"
        "pop rdx\n"
        "jmp qword ptr [rip + g_linuxGetWidthOfStringBypassAddress]\n"
        ".att_syntax prefix\n"
        :
        : EU4DLL_LINUX_ESCAPE_OPERANDS);
}

__attribute__((naked)) void NakedGetActualRequiredSize() {
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "push rdx\n"
        "mov rdx, rax\n"
        "movzx eax, byte ptr [rax]\n" EU4DLL_LINUX_DECODE_ESCAPE("eax", "add r13d, 2")
        "pop rdx\n"
        "mov rcx, qword ptr [rsp + 0x28]\n"
        "jmp qword ptr [rip + g_linuxGetActualRequiredSizeReturnAddress]\n"
        ".att_syntax prefix\n"
        :
        : EU4DLL_LINUX_ESCAPE_OPERANDS);
}

__attribute__((naked)) void NakedGetRequiredSize() {
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "push rdx\n"
        "mov rdx, rax\n"
        "movzx eax, byte ptr [rax]\n" EU4DLL_LINUX_DECODE_ESCAPE("eax",
                                                                "add dword ptr [rsp + 0xc], 2")
        "pop rdx\n"
        "mov rbp, qword ptr [r13 + rax*8 + 0x100]\n"
        "jmp qword ptr [rip + g_linuxGetRequiredSizeReturnAddress]\n"
        ".att_syntax prefix\n"
        :
        : EU4DLL_LINUX_ESCAPE_OPERANDS);
}

__attribute__((naked)) void NakedGetActualRealRequiredSizeActually() {
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "push rdx\n"
        "mov rdx, rax\n"
        "movzx eax, byte ptr [rax]\n" EU4DLL_LINUX_DECODE_ESCAPE("eax",
                                                                "add dword ptr [rsp + 0x8], 2")
        "pop rdx\n"
        "mov rbp, qword ptr [r15 + rax*8 + 0x100]\n"
        "jmp qword ptr [rip + g_linuxGetActualRealRequiredSizeActuallyReturnAddress]\n"
        ".att_syntax prefix\n"
        :
        : EU4DLL_LINUX_ESCAPE_OPERANDS);
}

#undef EU4DLL_LINUX_ESCAPE_OPERANDS
#undef EU4DLL_LINUX_DECODE_ESCAPE

patch::PatchDescription HeightDescription(patch::Address hookTarget) {
    auto description = Contract("layout.CBitmapFont.GetHeightOfString.glyph-decode",
                                kGetHeightOfStringPattern,
                                kGetHeightOfStringSymbol,
                                kGetHeightOfStringSearchSize);
    description.expected = patch::ExpectedBytes{
        0,
        {kGetHeightOfStringOriginal.begin(),
         kGetHeightOfStringOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::Jump;
    description.mutation.target = hookTarget;
    description.continuations = {
        {"return", kGetHeightOfStringContinuationOffset}};
    return description;
}

patch::PatchDescription WidthDescription(patch::Address hookTarget) {
    auto description = Contract("layout.CBitmapFont.GetWidthOfString.glyph-decode",
                                kGetWidthOfStringPattern,
                                kGetWidthOfStringSymbol,
                                kGetWidthOfStringSearchSize);
    description.expected = patch::ExpectedBytes{
        0,
        {kGetWidthOfStringOriginal.begin(),
         kGetWidthOfStringOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::Jump;
    description.mutation.target = hookTarget;
    description.continuations = {
        {"return", kGetWidthOfStringContinuationOffset},
        {"bypass", kGetWidthOfStringBypassOffset}};
    return description;
}

patch::PatchDescription ActualRequiredSizeDescription(patch::Address hookTarget) {
    auto description = Contract(
        "layout.CBitmapFont.GetActualRequiredSize.glyph-decode",
        kGetActualRequiredSizePattern, kGetActualRequiredSizeSymbol,
        kGetActualRequiredSizeSearchSize);
    description.expected = patch::ExpectedBytes{
        0,
        {kGetActualRequiredSizeOriginal.begin(),
         kGetActualRequiredSizeOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::Jump;
    description.mutation.target = hookTarget;
    description.continuations = {
        {"return", kGetActualRequiredSizeContinuationOffset}};
    return description;
}

patch::PatchDescription RequiredSizeDescription(patch::Address hookTarget) {
    auto description = Contract("layout.CBitmapFont.GetRequiredSize.glyph-decode",
                                kGetRequiredSizePattern,
                                kGetRequiredSizeSymbol,
                                kGetRequiredSizeSearchSize);
    description.expected = patch::ExpectedBytes{
        0,
        {kGetRequiredSizeOriginal.begin(),
         kGetRequiredSizeOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::Jump;
    description.mutation.target = hookTarget;
    description.continuations = {
        {"return", kGetRequiredSizeContinuationOffset}};
    return description;
}

patch::PatchDescription ActualRealRequiredSizeActuallyDescription(
    patch::Address hookTarget) {
    auto description = Contract(
        "layout.CBitmapFont.GetActualRealRequiredSizeActually.glyph-decode",
        kGetActualRealRequiredSizeActuallyPattern,
        kGetActualRealRequiredSizeActuallySymbol,
        kGetActualRealRequiredSizeActuallySearchSize);
    description.expected = patch::ExpectedBytes{
        0,
        {kGetActualRealRequiredSizeActuallyOriginal.begin(),
         kGetActualRealRequiredSizeActuallyOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::Jump;
    description.mutation.target = hookTarget;
    description.continuations = {
        {"return", kGetActualRealRequiredSizeActuallyContinuationOffset}};
    return description;
}

patch::PatchDescription WrappingGateDescription() {
    auto description = Contract("layout.CBitmapFont.GetActualRequiredSize.force-wrap",
                                kWrappingGatePattern,
                                kGetActualRequiredSizeSymbol,
                                kGetActualRequiredSizeSearchSize);
    description.expected = patch::ExpectedBytes{
        0,
        {kWrappingGateOriginal.begin(), kWrappingGateOriginal.end()},
        {}};
    description.mutation.kind = patch::MutationKind::RawBytes;
    description.mutation.bytes = {kWrappingGateReplacement.begin(),
                                  kWrappingGateReplacement.end()};
    return description;
}

std::vector<patch::PatchDescription> LayoutDescriptions(
    const LayoutHookTargets &targets) {
    return {HeightDescription(targets.height),
            WidthDescription(targets.width),
            ActualRequiredSizeDescription(targets.actualRequiredSize),
            RequiredSizeDescription(targets.requiredSize),
            ActualRealRequiredSizeActuallyDescription(
                targets.actualRealRequiredSizeActually),
            WrappingGateDescription()};
}

namespace {

LayoutHookTargets LiveHookTargets() {
    LayoutHookTargets targets;
    targets.height = reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&NakedGetHeightOfString));
    targets.width = reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&NakedGetWidthOfString));
    targets.actualRequiredSize = reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&NakedGetActualRequiredSize));
    targets.requiredSize = reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&NakedGetRequiredSize));
    targets.actualRealRequiredSizeActually = reinterpret_cast<patch::Address>(
        reinterpret_cast<std::uintptr_t>(&NakedGetActualRealRequiredSizeActually));
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
    GetHeightOfStringReturnSlot() = 0;
    GetWidthOfStringReturnSlot() = 0;
    GetWidthOfStringBypassSlot() = 0;
    GetActualRequiredSizeReturnSlot() = 0;
    GetRequiredSizeReturnSlot() = 0;
    GetActualRealRequiredSizeActuallyReturnSlot() = 0;
}

}  // namespace

patch::BatchResult PreflightLayout(patch::Memory &memory,
                                   patch::ExecutableCodeAllocator *allocator) {
    patch::PatchBatch batch(memory, allocator);
    for (auto &description : LayoutDescriptions(LiveHookTargets())) {
        batch.Add(std::move(description));
    }
    return batch.Preflight();
}

patch::BatchResult InstallLayout(patch::Memory &memory,
                                 patch::ExecutableCodeAllocator *allocator) {
    const auto targets = LiveHookTargets();
    // Discover every continuation first so the naked hooks' slots are
    // published before any mutation commits.
    {
        patch::PatchRuntime runtime(memory);
        struct SlotRequest {
            patch::PatchDescription description;
            const char *name;
            patch::Address *slot;
        };
        const auto height = HeightDescription(targets.height);
        const auto width = WidthDescription(targets.width);
        const auto actual = ActualRequiredSizeDescription(targets.actualRequiredSize);
        const auto required = RequiredSizeDescription(targets.requiredSize);
        const auto actualReal =
            ActualRealRequiredSizeActuallyDescription(targets.actualRealRequiredSizeActually);
        SlotRequest requests[] = {
            {height, "return", &GetHeightOfStringReturnSlot()},
            {width, "return", &GetWidthOfStringReturnSlot()},
            {width, "bypass", &GetWidthOfStringBypassSlot()},
            {actual, "return", &GetActualRequiredSizeReturnSlot()},
            {required, "return", &GetRequiredSizeReturnSlot()},
            {actualReal, "return", &GetActualRealRequiredSizeActuallyReturnSlot()},
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
                               "layout continuation is missing");
            }
            *request.slot = continuation;
        }
    }

    patch::PatchBatch batch(memory, allocator);
    for (auto &description : LayoutDescriptions(targets)) {
        batch.Add(std::move(description));
    }
    auto result = batch.Commit();
    if (!result) {
        ClearSlots();
    }
    return result;
}

}  // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::layout
