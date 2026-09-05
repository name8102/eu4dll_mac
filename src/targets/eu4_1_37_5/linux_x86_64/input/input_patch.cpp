#include "targets/eu4_1_37_5/linux_x86_64/input/input_patch.h"

#include "features/escaped_text/escaped_text.h"
#include "features/text_input/text_input.h"
#include "platform/linux/sdl_input_adapter.h"
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::input {
namespace {
namespace native = linux_platform::input;
constexpr char kEvents[] = "_ZN10CSdlEvents15HandlePdxEventsEP16CPdxEventHandlerP6CMouseP9CKeyBoardP12CTouchDevice";
using Action = void (*)(void *);
struct Api {
    native::SdlApi sdl;
    void (*text_init)(void *, char) = nullptr;
    void (*event_init)(void *, const void *) = nullptr;
    Action event_destroy = nullptr;
    int (*cursor)(void *) = nullptr;
    const std::string *(*text)(void *) = nullptr;
    bool (*selection)(void *) = nullptr;
    void (*select)(void *, int, int) = nullptr;
    std::array<Action, 4> edit{};
};
Api api;
thread_local const std::string *pendingCommit = nullptr;
thread_local bool commitWritten = false;
extern "C" {
__attribute__((visibility("hidden"))) std::uintptr_t g_linuxWriteReturn = 0;
__attribute__((visibility("hidden"))) std::uintptr_t g_linuxEditReturn[4]{};
}
__attribute__((naked)) void OriginalLeft(void *) {
    asm volatile(".intel_syntax noprefix\n push rbp\n push rbx\n push rax\n mov rbx, rdi\n"
        "jmp qword ptr [rip + g_linuxEditReturn]\n .att_syntax prefix\n");
}
__attribute__((naked)) void OriginalRight(void *) {
    asm volatile(".intel_syntax noprefix\n push rbp\n push rbx\n push rax\n mov rbx, rdi\n"
        "jmp qword ptr [rip + g_linuxEditReturn + 8]\n .att_syntax prefix\n");
}
__attribute__((naked)) void OriginalBackspace(void *) {
    asm volatile(".intel_syntax noprefix\n push r14\n push rbx\n push rax\n mov r14, rdi\n"
        "jmp qword ptr [rip + g_linuxEditReturn + 16]\n .att_syntax prefix\n");
}
__attribute__((naked)) void OriginalDelete(void *) {
    asm volatile(".intel_syntax noprefix\n push rbp\n push rbx\n push rax\n mov rbx, rdi\n"
        "jmp qword ptr [rip + g_linuxEditReturn + 24]\n .att_syntax prefix\n");
}
__attribute__((naked)) void OriginalWrite(void *, const std::string &) {
    asm volatile(
        ".intel_syntax noprefix\n"
        "push rbp\n push r14\n push rbx\n sub rsp, 0x60\n"
        "jmp qword ptr [rip + g_linuxWriteReturn]\n"
        ".att_syntax prefix\n");
}
void Write(void *buffer, const std::string &text) {
    // EU4's event object carries only one byte. Coalesce the first Write
    // reached by this commit, so length checks/observers never see fragments.
    if (pendingCommit && commitWritten) return;
    const auto &source = pendingCommit ? *pendingCommit : text;
    if (pendingCommit) commitWritten = true;
    const auto &existing = *api.text(buffer);
    const auto selected = api.selection(buffer)
        ? reinterpret_cast<const std::string *>(static_cast<char *>(buffer) + 0x70)->size() : 0;
    int limit;
    std::memcpy(&limit, static_cast<char *>(buffer) + 0xe0, sizeof(limit));
    const auto used = existing.size() - std::min(existing.size(), selected);
    const auto capacity = static_cast<std::size_t>(std::max(0, limit));
    const auto available = capacity - std::min(capacity, used);
    std::size_t end = 0;
    while (end < source.size()) {
        const auto next = escaped_text::next_cursor(source, end);
        if (next > available) break;
        end = next;
    }
    if (end != 0) OriginalWrite(buffer, source.substr(0, end));
}
thread_local unsigned byteOperation = 0;
struct ByteOperation {
    ByteOperation() { ++byteOperation; }
    ~ByteOperation() { --byteOperation; }
};
enum Edit { left, right, backspace, del };

void EditBuffer(void *buffer, Edit edit) {
    if (byteOperation != 0) { api.edit[edit](buffer); return; }
    if (native::Composing()) return;
    std::size_t count = 1;
    // +0x30 is the full string. +0x70 is selection scratch and must never
    // be used as the editable text (the macOS object layout is different).
    const auto &text = *api.text(buffer);
    const auto cursor = static_cast<std::size_t>(std::max(0, api.cursor(buffer)));
    if ((edit == backspace || edit == del) && !api.selection(buffer)) {
        const auto character = edit == backspace
            ? escaped_text::character_before(text, cursor)
            : escaped_text::character_at(text, cursor);
        if (character.escaped && character.byte_length == 3) {
            // Native byte-by-byte deletion notifies observers between bytes.
            // Select the complete character and use the game's atomic selected
            // deletion so redraw/search callbacks never see a broken escape.
            api.select(buffer, static_cast<int>(character.byte_offset),
                static_cast<int>(character.byte_offset + character.byte_length));
            api.edit[edit](buffer);
            return;
        }
    }
    if ((edit != backspace && edit != del) || !api.selection(buffer)) {
        const auto decision = edit == backspace
            ? text_input::decide_backspace(text, cursor)
            : text_input::decide_cursor(text, cursor, edit == left ? -1 : 1);
        count = std::max<std::size_t>(1, decision.byte_count);
    }
    ByteOperation guard; // native backspace calls virtual MoveLeft once per byte
    while (count-- != 0) api.edit[edit](buffer);
}
void Left(void *p) { EditBuffer(p, left); }
void Right(void *p) { EditBuffer(p, right); }
void Backspace(void *p) { EditBuffer(p, backspace); }
void Delete(void *p) { EditBuffer(p, del); }

void Begin(void *events, const int *rect) {
    *static_cast<std::uint8_t *>(static_cast<void *>(static_cast<char *>(events) + 0x38)) = 1;
    // CRect<int>::Contains adds +8/+12 to +0/+4: x/y/width/height,
    // matching SDL_Rect. These are not right/bottom coordinates.
    native::Begin({rect[0], rect[1], std::max(1, rect[2]), std::max(1, rect[3])});
}
void End(void *events) {
    *(static_cast<char *>(events) + 0x38) = 0;
    native::End();
}

extern "C" void LinuxCommitText(const char *utf8, void *handler, void *keyboard,
                                 std::uint32_t timestamp) {
    const auto bytes = native::Commit(std::string_view(utf8, strnlen(utf8, 32)));
    struct CommitScope {
        const std::string *previous = pendingCommit;
        bool wasWritten = commitWritten;
        explicit CommitScope(const std::string &text) { pendingCommit = &text; commitWritten = false; }
        ~CommitScope() { pendingCommit = previous; commitWritten = wasWritten; }
    } scope(bytes);
    using Precheck = void (*)(void *, int, std::uint32_t, int);
    using Dispatch = void (*)(void *, void *);
    const auto precheck = reinterpret_cast<Precheck>((*static_cast<void ***>(keyboard))[5]);
    const auto dispatch = reinterpret_cast<Dispatch>((*static_cast<void ***>(handler))[4]);
    alignas(16) std::array<std::uint8_t, 16> textEvent{};
    alignas(16) std::array<std::uint8_t, 80> inputEvent{};
    for (char byte : bytes) {
        if (byte == 0) continue;
        precheck(keyboard, 0x303, timestamp, 0);
        api.text_init(textEvent.data(), byte);
        api.event_init(inputEvent.data(), textEvent.data());
        dispatch(handler, inputEvent.data());
        api.event_destroy(inputEvent.data());
    }
}
extern "C" {
__attribute__((visibility("hidden"))) std::uintptr_t g_linuxInputCommitReturn = 0;
}
__attribute__((naked)) void CommitHook() {
    asm volatile(
        ".intel_syntax noprefix\n"
        "lea rdi, [rsp + 0x3c]\n"
        "mov rsi, r13\n"
        "mov rdx, r15\n"
        "mov ecx, dword ptr [rsp + 0x34]\n"
        "call LinuxCommitText\n"
        "jmp qword ptr [rip + g_linuxInputCommitReturn]\n"
        ".att_syntax prefix\n");
}

template<class Fn> bool Resolve(patch::Memory &memory, const char *name, Fn &fn,
                                patch::BatchResult &result) {
    std::string error;
    const auto address = memory.ResolveSymbol(name, error);
    if (!address) {
        result.diagnostic.feature = "input";
        result.diagnostic.target = kDiagnosticTargetId;
        result.diagnostic.operation = patch::PatchOperation::ResolveSymbol;
        result.diagnostic.message = name + std::string(": ") + error;
        return false;
    }
    fn = reinterpret_cast<Fn>(*address);
    return true;
}
patch::PatchDescription Hook(const char *feature, const char *symbol,
                              std::size_t size, const char *pattern,
                              std::vector<std::uint8_t> expected,
                              std::uintptr_t target, bool call = false) {
    patch::PatchDescription d;
    d.feature = feature;
    d.target = kDiagnosticTargetId;
    d.location.pattern = pattern;
    d.location.scope = patch::SearchScope::Symbol(symbol, size);
    d.expected = patch::ExpectedBytes{0, std::move(expected), {}};
    d.mutation.kind = call ? patch::MutationKind::Call : patch::MutationKind::Jump;
    d.mutation.target = target;
    d.mutation.callWidth = patch::CallWidth::FiveBytes;
    return d;
}
template<class F> std::uintptr_t Addr(F fn) { return reinterpret_cast<std::uintptr_t>(fn); }

bool InputPlan(patch::Memory &memory, Api &resolved,
               std::vector<patch::PatchDescription> &descriptions,
               patch::BatchResult &result) {
    if (!Resolve(memory, "SDL_StartTextInput", resolved.sdl.start, result) ||
        !Resolve(memory, "SDL_StopTextInput", resolved.sdl.stop, result) ||
        !Resolve(memory, "SDL_SetTextInputRect", resolved.sdl.set_rect, result) ||
        !Resolve(memory, "SDL_PollEvent", resolved.sdl.poll, result) ||
        !Resolve(memory, "_ZN15CTextInputEventC1Ec", resolved.text_init, result) ||
        !Resolve(memory, "_ZN11CInputEventC1ERK15CTextInputEvent", resolved.event_init, result) ||
        !Resolve(memory, "_ZN11CInputEventD1Ev", resolved.event_destroy, result) ||
        !Resolve(memory, "_ZN11CTextBuffer25GetCursorPositionInStringEv", resolved.cursor, result) ||
        !Resolve(memory, "_ZNK11CTextBuffer9GetStringEv", resolved.text, result) ||
        !Resolve(memory, "_ZN11CTextBuffer16GetSelectionModeEv", resolved.selection, result) ||
        !Resolve(memory, "_ZN11CTextBuffer6SelectEii", resolved.select, result)) return false;
    descriptions.push_back(Hook("input.focus.begin", "_ZN10CSdlEvents18BeginTextInputModeERK5CRectIiE",
        5, "C6 47 38 01 C3", {0xC6,0x47,0x38,1,0xC3}, Addr(Begin)));
    descriptions.push_back(Hook("input.focus.end", "_ZN10CSdlEvents16EndTextInputModeEv",
        5, "C6 47 38 00 C3", {0xC6,0x47,0x38,0,0xC3}, Addr(End)));
    auto commit = Hook("input.utf8-commit", kEvents, 0x966,
        "40 8A 6C 24 3C 40 84 ED 0F 89 CD 02 00 00",
        {0x40,0x8A,0x6C,0x24,0x3C}, Addr(CommitHook));
    commit.continuations = {{"return", 0x32a}};
    descriptions.push_back(std::move(commit));
    descriptions.push_back(Hook("input.poll.first", kEvents, 0x966,
        "E8 5A B1 12 00 85 C0", {0xE8,0x5A,0xB1,0x12,0}, Addr(native::Poll), true));
    descriptions.push_back(Hook("input.poll.loop", kEvents, 0x966,
        "E8 A5 AC 12 00 85 C0", {0xE8,0xA5,0xAC,0x12,0}, Addr(native::Poll), true));
    const char *symbols[] = {"_ZN11CTextBuffer8MoveLeftEv", "_ZN11CTextBuffer9MoveRightEv",
        "_ZN11CTextBuffer14EnterBackspaceEv", "_ZN11CTextBuffer11EnterDeleteEv"};
    const char *features[] = {"input.cursor.left", "input.cursor.right", "input.backspace", "input.delete"};
    const std::array<Action, 4> hooks{Left, Right, Backspace, Delete};
    const std::array<Action, 4> originals{OriginalLeft, OriginalRight, OriginalBackspace, OriginalDelete};
    for (std::size_t i = 0; i < hooks.size(); ++i) {
        // CEditBox and CNullEditBox own separate inherited vtables. Intercept
        // the shared function entry so all actual widgets use logical edits.
        const bool back = i == backspace;
        auto d = Hook(features[i], symbols[i], back ? 7 : 6,
            back ? "41 56 53 50 49 89 FE" : "55 53 50 48 89 FB",
            back ? std::vector<std::uint8_t>{0x41,0x56,0x53,0x50,0x49,0x89,0xFE}
                 : std::vector<std::uint8_t>{0x55,0x53,0x50,0x48,0x89,0xFB}, Addr(hooks[i]));
        d.continuations = {{"return", back ? 7 : 6}};
        resolved.edit[i] = originals[i];
        descriptions.push_back(std::move(d));
    }
    auto write = Hook("input.write", "_ZN11CTextBuffer5WriteERK7CString", 0x31f,
        "55 41 56 53 48 83 EC 60 49 89 FE", {0x55,0x41,0x56,0x53,0x48,0x83,0xEC,0x60}, Addr(Write));
    write.continuations = {{"return", 8}};
    descriptions.push_back(std::move(write));
    return true;
}

using ClipboardGet = void (*)(std::string *, void *);
using ClipboardSet = void (*)(void *, const std::string &);
ClipboardGet clipboardGet = nullptr;
ClipboardSet clipboardSet = nullptr;
void GetClipboard(std::string *result, void *clipboard) {
    clipboardGet(result, clipboard);
    *result = escaped_text::utf8_to_escaped(*result).text;
}
void SetClipboard(void *clipboard, const std::string &text) {
    const auto utf8 = escaped_text::escaped_to_utf8(text).text;
    clipboardSet(clipboard, utf8);
}
} // namespace

patch::BatchResult PreflightInput(patch::Memory &memory, patch::ExecutableCodeAllocator *allocator) {
    Api resolved;
    patch::BatchResult result;
    std::vector<patch::PatchDescription> descriptions;
    if (!InputPlan(memory, resolved, descriptions, result)) return result;
    patch::PatchBatch batch(memory, allocator);
    for (auto &d : descriptions) batch.Add(std::move(d));
    return batch.Preflight();
}
patch::BatchResult InstallInput(patch::Memory &memory, patch::ExecutableCodeAllocator *allocator) {
    Api resolved;
    patch::BatchResult result;
    std::vector<patch::PatchDescription> descriptions;
    if (!InputPlan(memory, resolved, descriptions, result)) return result;
    const auto commit = patch::PatchRuntime(memory).Preflight(descriptions[2]);
    if (!commit) { result.diagnostic = commit.diagnostic; return result; }
    const auto write = patch::PatchRuntime(memory).Preflight(descriptions.back());
    if (!write) { result.diagnostic = write.diagnostic; return result; }
    std::array<std::uintptr_t, 4> editReturns{};
    for (std::size_t i = 0; i < editReturns.size(); ++i) {
        const auto edit = patch::PatchRuntime(memory).Preflight(descriptions[5+i]);
        if (!edit) { result.diagnostic = edit.diagnostic; return result; }
        editReturns[i] = edit.ContinuationAddress("return");
    }
    api = resolved;
    native::Bind(api.sdl);
    g_linuxInputCommitReturn = commit.ContinuationAddress("return");
    g_linuxWriteReturn = write.ContinuationAddress("return");
    std::copy(editReturns.begin(), editReturns.end(), g_linuxEditReturn);
    patch::PatchBatch batch(memory, allocator);
    for (auto &d : descriptions) batch.Add(std::move(d));
    result = batch.Commit();
    if (!result && !patch::MustRetainSlots(result)) {
        api = {};
        native::Bind({});
        g_linuxInputCommitReturn = 0;
        g_linuxWriteReturn = 0;
        std::fill(std::begin(g_linuxEditReturn), std::end(g_linuxEditReturn), 0);
    }
    return result;
}

namespace {
patch::BatchResult Clipboard(patch::Memory &memory, patch::ExecutableCodeAllocator *allocator, bool install) {
    patch::BatchResult result;
    ClipboardGet get = nullptr;
    ClipboardSet set = nullptr;
    if (!Resolve(memory, "_ZN10CClipboard12GetClipboardEv", get, result) ||
        !Resolve(memory, "_ZN10CClipboard12SetClipboardE7CString", set, result)) return result;
    auto d = Hook("clipboard.paste", "_ZN11CTextBuffer5PasteEv", 0x86,
        "48 8D B7 96 00 00 00 48 89 E7 E8 BC 4E F0 FF",
        {0xE8,0xBC,0x4E,0xF0,0xFF}, Addr(GetClipboard), true);
    d.expected->offset = d.mutation.offset = 10;
    patch::PatchBatch batch(memory, allocator);
    batch.Add(std::move(d));
    batch.Add(Hook("clipboard.copy", "_ZN11CTextBuffer4CopyEv", 0x6b,
        "E8 33 4C F0 FF 48 8D 44 24 20", {0xE8,0x33,0x4C,0xF0,0xFF}, Addr(SetClipboard), true));
    batch.Add(Hook("clipboard.cut", "_ZN11CTextBuffer3CutEb", 0x1c4,
        "E8 4D 50 F0 FF 48 8D 44 24 58", {0xE8,0x4D,0x50,0xF0,0xFF}, Addr(SetClipboard), true));
    if (!install) return batch.Preflight();
    clipboardGet = get;
    clipboardSet = set;
    result = batch.Commit();
    if (!result && !patch::MustRetainSlots(result)) { clipboardGet = nullptr; clipboardSet = nullptr; }
    return result;
}
} // namespace
patch::BatchResult PreflightClipboard(patch::Memory &m, patch::ExecutableCodeAllocator *a) { return Clipboard(m,a,false); }
patch::BatchResult InstallClipboard(patch::Memory &m, patch::ExecutableCodeAllocator *a) { return Clipboard(m,a,true); }

} // namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::input
