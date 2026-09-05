// This DSO substitutes a short test main in the real EU4 process. Game and
// preload constructors run normally, so probes execute the installed machine
// code and game CString/CTextBuffer implementations without loading a campaign.
#include "features/escaped_text/escaped_text.h"
#include "features/date_formatting/date_formatting.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>

namespace {
void Require(bool condition, const char *message) {
    if (!condition) { std::fprintf(stderr, "FAIL live probe: %s\n", message); std::exit(1); }
}
template<class T> T Symbol(const char *name) {
    auto *p = dlsym(RTLD_DEFAULT, name);
    Require(p != nullptr, name);
    return reinterpret_cast<T>(p);
}
std::string Escaped(const std::string &text) { return eu4dll::escaped_text::utf8_to_escaped(text).text; }
using GetSize = unsigned (*)(const void *);
template<class Fn = GetSize> Fn CallAt(std::uintptr_t site) {
    Require(*reinterpret_cast<unsigned char *>(site) == 0xE8, "expected installed call opcode");
    std::int32_t displacement;
    std::memcpy(&displacement, reinterpret_cast<void *>(site+1), 4);
    return reinterpret_cast<Fn>(site + 5 + displacement);
}

extern "C" {
__attribute__((visibility("hidden"))) std::uintptr_t probeSpacingStart = 0;
void ProbeSpacingSetup(char *frame, const std::string *source, std::string *result) {
    new (frame - 0x238) std::string;
    new (frame - 0x150) std::string(*source);
    std::memcpy(frame - 0x248, &result, sizeof(result));
    std::memset(frame - 0x88, 0x7f, 8); // deliberately dirty native scratch padding
}
void ProbeSpacingFinish(char *frame) {
    auto *output = reinterpret_cast<std::string *>(frame - 0x238);
    auto *source = reinterpret_cast<std::string *>(frame - 0x150);
    std::string *result;
    std::memcpy(&result, frame - 0x248, sizeof(result));
    *result = std::move(*output);
    output->~basic_string();
    source->~basic_string();
}
__attribute__((naked)) void ProbeSpacingEnd() {
    asm volatile(".intel_syntax noprefix\n"
        "mov rdi, rbp\n call ProbeSpacingFinish\n add rsp, 0x248\n"
        "pop r15\n pop r14\n pop r13\n pop r12\n pop rbx\n pop rbp\n ret\n"
        ".att_syntax prefix\n");
}
__attribute__((naked)) void ProbeSpacingRun(const std::string *, std::string *, const std::string *) {
    asm volatile(".intel_syntax noprefix\n"
        "push rbp\n mov rbp, rsp\n push rbx\n push r12\n push r13\n push r14\n push r15\n sub rsp, 0x248\n"
        "mov rbx, rdx\n mov rdx, rsi\n mov rsi, rdi\n mov rdi, rbp\n call ProbeSpacingSetup\n"
        "lea r14, [rbp - 0x238]\n mov r13, [rbp - 0x150]\n mov r15, [rbp - 0x148]\n dec r15\n xor r12d, r12d\n"
        "jmp qword ptr [rip + probeSpacingStart]\n .att_syntax prefix\n");
}
}
void Spacing() {
    const auto area = Symbol<std::uintptr_t>("_ZN18CGenerateNamesWork11AddNameAreaEPKiiiiiRK13SProvinceData");
    probeSpacingStart = area + 0xaf6;
    const auto end = area + 0xb52;
    const auto pageSize = static_cast<std::uintptr_t>(sysconf(_SC_PAGESIZE));
    const auto page = end & ~(pageSize-1);
    Require(mprotect(reinterpret_cast<void *>(page), pageSize, PROT_READ|PROT_WRITE|PROT_EXEC) == 0,
        "probe-only spacing exit redirect");
    std::array<unsigned char, 14> redirect{0xff,0x25,0,0,0,0};
    const auto finish = reinterpret_cast<std::uintptr_t>(ProbeSpacingEnd);
    std::memcpy(redirect.data()+6, &finish, sizeof(finish));
    std::memcpy(reinterpret_cast<void *>(end), redirect.data(), redirect.size());
    Require(mprotect(reinterpret_cast<void *>(page), pageSize, PROT_READ|PROT_EXEC) == 0,
        "restore spacing page permissions");
    for (const auto &name : {"黑羊", "白羊", "明", "A明", "明A", "A明B"}) {
        const auto source = Escaped(name);
        const std::string spaces = " ";
        std::string output, expected;
        for (std::size_t i=0; i<source.size();) {
            const auto next = eu4dll::escaped_text::next_cursor(source, i);
            if (i) expected += spaces;
            expected.append(source, i, next-i);
            i=next;
        }
        ProbeSpacingRun(&source, &output, &spaces);
        if (output != expected) {
            std::fprintf(stderr, "spacing %s expected=%zu actual=%zu bytes:", name, expected.size(), output.size());
            for (unsigned char byte : output) std::fprintf(stderr, " %02x", byte);
            std::fprintf(stderr, "\n");
        }
        Require(output == expected, "actual AddNameArea spacing loop must preserve exact characters and spaces");
    }
    std::fprintf(stderr, "native AddNameArea spacing loop with dirty scratch passed\n");
}
void Map() {
    const auto area = Symbol<std::uintptr_t>("_ZN18CGenerateNamesWork11AddNameAreaEPKiiiiiRK13SProvinceData");
    for (const auto &name : {"明", "吴", "越", "奥斯曼", "A明 B", "A", "AB"}) {
        const auto text = Escaped(name);
        const unsigned first = CallAt(area+0x1ff3)(&text);
        const unsigned second = CallAt(area+0x1ffe)(&text);
        const unsigned third = CallAt(area+0x2219)(&text);
        unsigned expected = 0;
        for (std::size_t i = 0; i < text.size(); ++expected)
            i = eu4dll::escaped_text::next_cursor(text, i);
        std::fprintf(stderr, "map count %s: %u/%u/%u\n", name, first, second, third);
        Require(first == expected && first == second && second == third, "CurveText lengths must all count glyphs");
        const float interpolation = third < 2 ? 1.0f : 0.0f / static_cast<float>(second-1);
        Require(std::isfinite(interpolation), "single glyph interpolation must stay finite");
    }
}

std::string committed;
void *inputBuffer = nullptr;
std::uint32_t expectedTimestamp = 0;
void Precheck(void *, int type, std::uint32_t timestamp, int) {
    Require(type == 0x303 && timestamp == expectedTimestamp, "input timestamp and type");
}
void Dispatch(void *, void *event) {
    const auto get = Symbol<const char *(*)(void *)>("_ZNK11CInputEvent17GetTextInputEventEv");
    committed += *get(event);
    if (inputBuffer) {
        Symbol<void (*)(void *, const std::string &)>("_ZN11CTextBuffer5WriteERK7CString")(
            inputBuffer, std::string(1, *get(event)));
    }
}
int FontHeight(void *, int) { return 0; }
int FontWidth(void *, const char *, int bytes, int) { return bytes; }
std::string expectedObservedText;
void ObserveAtomicEdit(void *buffer) {
    const auto *text = reinterpret_cast<const std::string *>(static_cast<char *>(buffer) + 0x30);
    Require(*text == expectedObservedText, "text-change observers must see the complete Han deletion");
}
void Input() {
    auto commit = Symbol<void (*)(const char *, void *, void *, std::uint32_t)>("LinuxCommitText");
    std::array<void *, 6> keyboardTable{};
    std::array<void *, 5> handlerTable{};
    keyboardTable[5] = reinterpret_cast<void *>(Precheck);
    handlerTable[4] = reinterpret_cast<void *>(Dispatch);
    auto *keyboard = keyboardTable.data();
    auto *handler = handlerTable.data();
    expectedTimestamp = 0x12345678;
    commit("中文A北京", &handler, &keyboard, expectedTimestamp);
    Require(committed == Escaped("中文A北京"), "whole multi-character UTF-8 commit");

    std::array<void *, 24> fontTable{};
    fontTable[0xb0/8] = reinterpret_cast<void *>(FontHeight);
    fontTable[0x68/8] = reinterpret_cast<void *>(FontWidth);
    auto *font = fontTable.data();
    alignas(16) std::array<unsigned char, 0x110> buffer{};
    unsigned char clipboard[8]{};
    Symbol<void (*)(void *, const void *, void *, bool)>("_ZN11CTextBufferC1ERK10CClipboardP5CFontb")(
        buffer.data(), clipboard, &font, false);
    const auto set = Symbol<void (*)(void *, const std::string &)>("_ZN11CTextBuffer9SetStringERK7CString");
    const auto get = Symbol<const std::string *(*)(void *)>("_ZNK11CTextBuffer9GetStringEv");
    const auto cursor = Symbol<int (*)(void *)>("_ZN11CTextBuffer25GetCursorPositionInStringEv");
    auto call = [&](std::size_t offset) {
        auto **vtable = *reinterpret_cast<void ***>(buffer.data());
        reinterpret_cast<void (*)(void *)>(vtable[offset/8])(buffer.data());
    };
    set(buffer.data(), Escaped("A明中B"));
    call(0x118); // MoveToEndOfBuffer
    Require(cursor(buffer.data()) == 8, "real buffer end position");
    call(0xd8); Require(cursor(buffer.data()) == 7, "move left over ASCII");
    call(0xd8); Require(cursor(buffer.data()) == 4, "move left over Han");
    call(0xe8); Require(cursor(buffer.data()) == 7, "move right over Han");
    auto **originalTable = *reinterpret_cast<void ***>(buffer.data());
    std::array<void *, 0x220/8> observedTable{};
    std::copy_n(originalTable, observedTable.size(), observedTable.data());
    observedTable[0x218/8] = reinterpret_cast<void *>(ObserveAtomicEdit);
    *reinterpret_cast<void ***>(buffer.data()) = observedTable.data();
    expectedObservedText = Escaped("A明B");
    call(0x140); Require(*get(buffer.data()) == expectedObservedText, "backspace removes one Han");
    *reinterpret_cast<void ***>(buffer.data()) = originalTable;
    call(0xd8); Require(cursor(buffer.data()) == 1, "remaining Han boundary");
    call(0x148); Require(*get(buffer.data()) == "AB", "delete removes one Han");
    // CEditBox replaces the CTextBuffer address point in its constructor.
    // Exercise its inherited edit entries, retaining harmless probe callbacks.
    auto **editBoxTable = reinterpret_cast<void **>(Symbol<std::uintptr_t>("_ZTV8CEditBox") + 0x2b0);
    std::copy_n(originalTable, observedTable.size(), observedTable.data());
    for (const auto offset : {0xd8,0xe8,0x140,0x148}) observedTable[offset/8] = editBoxTable[offset/8];
    *reinterpret_cast<void ***>(buffer.data()) = observedTable.data();
    set(buffer.data(), Escaped("奥地利"));
    call(0x118);
    call(0x140);
    Require(*get(buffer.data()) == Escaped("奥地"), "CEditBox inherited backspace deletes one complete Han");
    call(0xd8);
    Require(cursor(buffer.data()) == 3, "CEditBox inherited left skips one Han");
    call(0x148);
    Require(*get(buffer.data()) == Escaped("奥"), "CEditBox inherited Delete deletes one complete Han");
    *reinterpret_cast<void ***>(buffer.data()) = originalTable;
    set(buffer.data(), "");
    inputBuffer = buffer.data();
    commit("中文A北京", &handler, &keyboard, expectedTimestamp);
    inputBuffer = nullptr;
    Require(*get(buffer.data()) == Escaped("中文A北京"), "native Write receives complete IME text");
    set(buffer.data(), "");
    const auto maxChars = Symbol<void (*)(void *, int)>("_ZN11CTextBuffer11SetMaxCharsEi");
    maxChars(buffer.data(), 2);
    inputBuffer = buffer.data();
    commit("明", &handler, &keyboard, expectedTimestamp);
    inputBuffer = nullptr;
    Require(get(buffer.data())->empty() || *get(buffer.data()) == Escaped("明"),
            "character limit must not leave a partial escaped Han character");
    maxChars(buffer.data(), 0x7fffffff);

    // Dummy SDL video owns an isolated in-memory clipboard; never change the
    // user's desktop clipboard while running the integration probe.
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    Require(Symbol<int (*)(unsigned)>("SDL_Init")(0x20) == 0, "dummy SDL video initialization");
    const auto select = Symbol<void (*)(void *, int, int)>("_ZN11CTextBuffer6SelectEii");
    const auto clipGet = Symbol<char *(*)()>("SDL_GetClipboardText");
    const auto sdlFree = Symbol<void (*)(void *)>("SDL_free");
    set(buffer.data(), Escaped("A明中B"));
    select(buffer.data(), 1, 7);
    Symbol<void (*)(void *)>("_ZN11CTextBuffer4CopyEv")(buffer.data());
    auto *copied = clipGet();
    Require(std::string(copied) == "明中", "copy to UTF-8 clipboard");
    sdlFree(copied);
    Symbol<void (*)(void *, bool)>("_ZN11CTextBuffer3CutEb")(buffer.data(), true);
    copied = clipGet();
    Require(std::string(copied) == "明中" && *get(buffer.data()) == "AB", "cut whole Han selection to UTF-8");
    sdlFree(copied);
    Symbol<void (*)(void *)>("_ZN11CTextBuffer5PasteEv")(buffer.data());
    Require(*get(buffer.data()) == Escaped("A明中B"), "paste UTF-8 clipboard into native buffer");
    Symbol<void (*)()>("SDL_Quit")();
    Symbol<void (*)(void *)>("_ZN11CTextBufferD1Ev")(buffer.data());
    std::fprintf(stderr, "input native commit, CTextBuffer edits and clipboard passed\n");
}
void SaveNames() {
    const auto save = Symbol<std::uintptr_t>("_ZN15CIngameSaveMenu8SaveGameEv");
    const auto select = Symbol<std::uintptr_t>("_ZN15CIngameSaveMenu14SaveGameSelectEP9CCheckBox");
    using Convert = std::string &(*)(std::string &);
    auto name = Escaped("大明_1444测试");
    CallAt<Convert>(save+0x4f)(name);
    Require(name == "大明_1444测试", "native save site produces UTF-8 filename");
    CallAt<Convert>(select+0x85)(name);
    Require(name == Escaped("大明_1444测试"), "native selection site produces display filename");
    const auto header = Symbol<std::uintptr_t>("_ZN18CLocalSavegameItem16UpdateHeaderInfoEv");
    const auto tooltip = Symbol<std::uintptr_t>("_ZN9CFrontEnd17GetCurrentTooltipERK8CVector2IfEP10CGuiObject");
    for (const auto site : {header + 0x939, tooltip + 0x4c6}) {
        const std::string disk = "大明_1444测试.eu4";
        const std::string prefix("\xa7Y");
        auto display = prefix;
        CallAt<void (*)(std::string &, const std::string &)>(site)(display, disk);
        Require(display == prefix + Escaped(disk), "filename tooltip preserves color prefix");
        Require(disk == "大明_1444测试.eu4", "filename tooltip does not mutate UTF-8 disk name");
    }
    std::fprintf(stderr, "save filename conversion and header/continue display calls passed\n");
}
extern "C" {
__attribute__((naked)) std::string *ProbeMonarch(std::uintptr_t, std::string *, const std::string *, const void *) {
    asm volatile(".intel_syntax noprefix\n push rbx\n mov rbx, rcx\n mov rax, rdi\n mov rdi, rsi\n mov rsi, rdx\n call rax\n pop rbx\n ret\n .att_syntax prefix\n");
}
__attribute__((naked)) std::string *ProbeRepublic(std::uintptr_t, std::string *, const std::string *, const void *) {
    asm volatile(".intel_syntax noprefix\n sub rsp, 24\n mov [rsp + 8], rcx\n mov rax, rdi\n mov rdi, rsi\n mov rsi, rdx\n call rax\n add rsp, 24\n ret\n .att_syntax prefix\n");
}
}
// Replacement main has no localization database. Stub just the month lookup;
// the actual Gregorian formatter and installed topbar call still execute.
void *ProbeMonth(std::string *out, const void *, int) {
    new (out) std::string(Escaped("十一月"));
    return out;
}
void DisplayFormatting() {
    auto *month = Symbol<unsigned char *>("_ZNK14CGregorianDate14GetMonthStringE12EMonthFormat");
    std::array<unsigned char, 14> originalMonth{};
    std::memcpy(originalMonth.data(), month, originalMonth.size());
    const auto pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    auto *monthPage = reinterpret_cast<void *>(reinterpret_cast<std::uintptr_t>(month) & ~(pageSize-1));
    Require(mprotect(monthPage, pageSize, PROT_READ|PROT_WRITE|PROT_EXEC) == 0, "month fixture writable");
    std::array<unsigned char, 14> jump{{0xff,0x25,0,0,0,0}};
    auto monthTarget = reinterpret_cast<std::uintptr_t>(ProbeMonth);
    std::memcpy(jump.data()+6, &monthTarget, 8);
    std::memcpy(month, jump.data(), jump.size());
    Require(mprotect(monthPage, pageSize, PROT_READ|PROT_EXEC) == 0, "month fixture executable");
    const auto topbar = Symbol<std::uintptr_t>("_ZN10CTopbarGui26RefreshSpeedControlsWindowERK8CCountry");
    using FormatDate = void *(*)(std::string *, const void *, const std::string &);
    std::int32_t date = 0x29c55c0;
    using SetDate = void (*)(void *, int);
    Symbol<SetDate>("_ZN14CGregorianDate7SetYearEi")(&date, 1444);
    Symbol<SetDate>("_ZN14CGregorianDate8SetMonthEi")(&date, 10);
    Symbol<SetDate>("_ZN14CGregorianDate6SetDayEi")(&date, 10);
    alignas(std::string) std::array<char, sizeof(std::string)> patchedStorage{}, expectedStorage{};
    auto *patched = reinterpret_cast<std::string *>(patchedStorage.data());
    auto *expected = reinterpret_cast<std::string *>(expectedStorage.data());
    const auto &bytes = eu4dll::features::date_formatting::kYearMonthDayFormat;
    const std::string format(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    CallAt<FormatDate>(topbar + 0x4f)(patched, &date, std::string("d mw y"));
    Symbol<FormatDate>("_ZNK14CGregorianDate9GetStringERK7CString")(expected, &date, format);
    Require(*patched == *expected && patched->find("1444") == 0, "topbar uses native year-month-day formatter");
    std::fprintf(stderr, "formatted date: %s\n", eu4dll::escaped_text::escaped_to_utf8(*patched).text.c_str());
    Require(*patched == "1444\x0f" + Escaped("十一月") + "11\x0e", "date fields and year/day markers");
    patched->~basic_string(); expected->~basic_string();
    Require(mprotect(monthPage, pageSize, PROT_READ|PROT_WRITE|PROT_EXEC) == 0, "restore month fixture");
    std::memcpy(month, originalMonth.data(), originalMonth.size());
    Require(mprotect(monthPage, pageSize, PROT_READ|PROT_EXEC) == 0, "restored month executable");

    const auto monarch = Symbol<std::uintptr_t>("_ZNK8CMonarch11GetFullNameEv");
    const auto republic = Symbol<std::uintptr_t>("_ZN8CCountry18GetNewRepublicNameE11CCountryTagPK8CCultureRK20CWeightedStringTableRK6CArrayI7CStringEbiiRbRKS8_bb");
    alignas(8) std::array<char, 0xa0> culture{}, group{}, king{}, manager{};
    auto *tag = new (culture.data()+0x40) std::string;
    auto *groupTag = new (group.data()+0x38) std::string;
    const void *groupPtr = group.data(), *culturePtr = culture.data();
    std::memcpy(culture.data()+0x80, &groupPtr, 8);
    std::memcpy(king.data()+0x58, &culturePtr, 8);
    auto **managerSlot = Symbol<void **>("_ZN11CDLCManager10_pInstanceE");
    auto *savedManager = *managerSlot;
    *managerSlot = manager.data();
    const std::string mod("mod/Bimillennium_Universalis_test.mod");
    const auto *modPtr = &mod;
    std::memcpy(manager.data()+0x90, &modPtr, 8);
    struct Case { const char *group, *culture, *given, *surname, *expected; int mod; };
    const Case cases[] = {
        {"east_asian", "han", "祁镇", "朱", "朱祁镇", 0},
        {"japanese_g", "togoku", "义政", "足利", "足利 义政", 0},
        {"british", "english", "Henry", "Tudor", "Henry Tudor", 0},
        {"tibetan_group", "bai", "兴智", "段", "段兴智", 0},
        {"other", "zhuang", "Given", "Family", "Family Given", 0},
        {"mongolic", "test", "Given", "Family", "Given Family", 0},
        {"mongolic", "test", "Given", "Family", "Family Given", 1}
    };
    for (const auto &c : cases) {
        *tag = c.culture; *groupTag = c.group;
        std::memcpy(manager.data()+0x9c, &c.mod, 4);
        const auto given = Escaped(c.given), surname = Escaped(c.surname);
        auto out = given;
        const auto suffix = " " + surname;
        Require(ProbeMonarch(reinterpret_cast<std::uintptr_t>(CallAt<void (*)()>(monarch+0x11f)), &out, &suffix, king.data()) == &out,
                "monarch append return ABI");
        Require(out == Escaped(c.expected), "monarch culture and separator policy");
        Require(suffix == " " + surname, "monarch temporary remains owned by caller");
        for (auto offset : {0x1bb, 0x203, 0x25e}) {
            out = given + " ";
            Require(ProbeRepublic(reinterpret_cast<std::uintptr_t>(CallAt<void (*)()>(republic+offset)), &out, &surname, culture.data()) == &out,
                    "republic append return ABI");
            Require(out == Escaped(c.expected), "republic culture and separator policy");
        }
    }
    *managerSlot = savedManager;
    tag->~basic_string(); groupTag->~basic_string();
    std::fprintf(stderr, "native topbar date and monarch/republic name call adapters passed\n");
}
struct SearchEntry {
    int distance = 0, province = 0;
    unsigned char country[8]{};
    std::string area, original, cleaned, local;
};
struct SearchList { std::vector<SearchEntry> entries; bool matched = false; };
void Search() {
    const auto process = Symbol<void (*)(void *, const std::string &, bool)>("_ZN18CGotoBoxSearchList7ProcessERK7CStringb");
    for (const auto &query : {"北京", "beijing", "bj", "BEIJING", "北", "A明"}) {
        SearchList list;
        for (const auto &name : {"北京", "南京", "A明", "London"}) {
            SearchEntry entry;
            entry.original = entry.cleaned = Escaped(name);
            list.entries.push_back(std::move(entry));
        }
        process(&list, Escaped(query), false);
        Require(!list.entries.empty(), "native search retains matches");
        std::fprintf(stderr, "search %s first score=%d\n", query, list.entries[0].distance);
        Require(list.entries[0].original == Escaped(std::string(query)=="A明" ? "A明" : "北京"), "native search top result");
    }
}
int TestMain(int, char **, char **) {
    unsigned char version[3]{};
    Symbol<void (*)(void *)>("SDL_GetVersion")(version);
    std::fprintf(stderr, "SDL runtime %u.%u.%u\n", version[0], version[1], version[2]);
    Map();
    if (std::getenv("EU4DLL_PROBE_MAP_ONLY") == nullptr) { Spacing(); Input(); Search(); SaveNames(); DisplayFormatting(); }
    std::fprintf(stderr, "PASS Linux real ELF probe\n");
    return 0;
}
} // namespace
extern "C" int __libc_start_main(int (*)(int,char **,char **), int argc, char **argv,
    void (*init)(), void (*fini)(), void (*rtldFini)(), void *stackEnd) {
    using Start = int (*)(int (*)(int,char **,char **), int, char **, void (*)(), void (*)(), void (*)(), void *);
    auto real = reinterpret_cast<Start>(dlsym(RTLD_NEXT, "__libc_start_main"));
    return real(TestMain, argc, argv, init, fini, rtldFini, stackEnd);
}
