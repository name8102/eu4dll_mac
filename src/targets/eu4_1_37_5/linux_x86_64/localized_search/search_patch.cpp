#include "targets/eu4_1_37_5/linux_x86_64/localized_search/search_patch.h"
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"
#include "features/localized_search/localized_search.h"
#include "pinyinHelper.h"

#include <dlfcn.h>
#include <filesystem>
#include <mutex>

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::search {
namespace {
constexpr char kProcess[] = "_ZN18CGotoBoxSearchList7ProcessERK7CStringb";
struct Entry {
    int distance, province;
    std::uint8_t country[8];
    std::string area, original, cleaned, local;
};
static_assert(sizeof(Entry) == 0x90 && offsetof(Entry, original) == 0x30);
const std::string *dictionaryPath = nullptr;

extern "C" {
__attribute__((visibility("hidden"))) std::uintptr_t g_linuxSearchReturn = 0;
__attribute__((visibility("hidden"))) std::uintptr_t g_linuxSearchMatched = 0;
}

extern "C" bool LinuxSearchMatch(Entry *entry, const std::string *query, bool exact) {
    // Process runs in a worker; keep dictionary/cache use serialized across
    // concurrent list refreshes. Both objects live through game shutdown.
    static auto *mutex = new std::mutex;
    std::lock_guard<std::mutex> lock(*mutex);
    static auto *engine = [] {
        // The preload constructor runs before cpp-pinyin's C++ globals.
        // Initialize the library only once the first game search executes.
        PinyinHelper::SetDictionaryPath(*dictionaryPath);
        return new features::localized_search::SearchEngine(PinyinHelper::getInstance());
    }();
    const auto result = engine->Match(exact, *query, entry->original, entry->cleaned);
    if (result.matched) entry->distance = result.distance;
    return result.matched;
}
__attribute__((naked)) void CompareHook() {
    asm volatile(
        ".intel_syntax noprefix\n"
        "push rax\n push rcx\n push rdx\n push rsi\n push rdi\n push r8\n push r9\n push r10\n"
        "lea rdi, [rbp + r13]\n"
        // The original CString* at +0x38 survives byte-based game cleaning.
        "mov rsi, qword ptr [rsp + 0x78]\n"
        "movzx edx, byte ptr [rsp + 0x54]\n"
        "call LinuxSearchMatch\n"
        "mov r11b, al\n"
        "pop r10\n pop r9\n pop r8\n pop rdi\n pop rsi\n pop rdx\n pop rcx\n pop rax\n"
        "test r11b, r11b\n"
        "jz 1f\n"
        "mov rax, qword ptr [rsp + 0x08]\n"
        "mov byte ptr [rax + 0x18], 1\n"
        "jmp qword ptr [rip + g_linuxSearchMatched]\n"
        "1: cmp byte ptr [rsp + 0x14], 0\n"
        "jmp qword ptr [rip + g_linuxSearchReturn]\n"
        ".att_syntax prefix\n");
}

patch::PatchDescription Description() {
    patch::PatchDescription d;
    d.feature = "localized-search.compare";
    d.target = kDiagnosticTargetId;
    d.location.pattern = "4F 8D 2C F6 49 C1 E5 04 80 7C 24 14 00 74 5A";
    d.location.scope = patch::SearchScope::Symbol(kProcess, 0x30a);
    d.expected = patch::ExpectedBytes{8, {0x80,0x7C,0x24,0x14,0}, {}};
    d.mutation.kind = patch::MutationKind::Jump;
    d.mutation.offset = 8;
    d.mutation.target = reinterpret_cast<std::uintptr_t>(CompareHook);
    d.continuations = {{"return", 13}, {"matched", 0x99}};
    return d;
}
bool Dictionary(std::filesystem::path &path, patch::BatchResult &result) {
    Dl_info info{};
    if (dladdr(reinterpret_cast<void *>(CompareHook), &info) && info.dli_fname) {
        path = std::filesystem::path(info.dli_fname).parent_path() / "chinese_dict";
        if (std::filesystem::is_regular_file(path / "mandarin/word.txt")) return true;
    }
    result.diagnostic.feature = "localized-search.dictionary";
    result.diagnostic.target = kDiagnosticTargetId;
    result.diagnostic.operation = patch::PatchOperation::ResolveSearchScope;
    result.diagnostic.message = "missing chinese_dict/mandarin/word.txt next to libeu4dll_linux.so";
    return false;
}
} // namespace
patch::BatchResult PreflightSearch(patch::Memory &memory, patch::ExecutableCodeAllocator *allocator) {
    patch::BatchResult result;
    std::filesystem::path dictionary;
    if (!Dictionary(dictionary, result)) return result;
    patch::PatchBatch batch(memory, allocator);
    batch.Add(Description());
    return batch.Preflight();
}
patch::BatchResult InstallSearch(patch::Memory &memory, patch::ExecutableCodeAllocator *allocator) {
    patch::BatchResult result;
    std::filesystem::path dictionary;
    if (!Dictionary(dictionary, result)) return result;
    const auto d = Description();
    const auto located = patch::PatchRuntime(memory).Preflight(d);
    if (!located) { result.diagnostic = located.diagnostic; return result; }
    dictionaryPath = new std::string(dictionary.string());
    g_linuxSearchReturn = located.ContinuationAddress("return");
    g_linuxSearchMatched = located.ContinuationAddress("matched");
    patch::PatchBatch batch(memory, allocator);
    batch.Add(d);
    result = batch.Commit();
    if (!result && !patch::MustRetainSlots(result)) g_linuxSearchReturn = g_linuxSearchMatched = 0;
    return result;
}
} // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::search
