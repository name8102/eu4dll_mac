#include "targets/eu4_1_37_5/linux_x86_64/save_filenames/save_patch.h"
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"
#include "features/save_filenames/save_filenames.h"

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::save_filenames {
namespace {
namespace portable = features::save_filenames;
using ChangeString = void (*)(void *, const std::string &, bool);
using Localize = void (*)(std::string *, const char *, const char *, const std::string &);
ChangeString changeString = nullptr;
Localize localize = nullptr;
void Display(void *box, const std::string &name, bool resize) {
    const auto display = portable::DisplayCopy(name);
    changeString(box, display, resize);
}
void LocalizeDisplay(std::string *output, const char *key, const char *token,
                     const std::string &name) {
    const auto display = portable::DisplayCopy(name);
    localize(output, key, token, display);
}
template<class Fn> std::uintptr_t Addr(Fn fn) { return reinterpret_cast<std::uintptr_t>(fn); }
patch::PatchDescription Call(const char *feature, const char *symbol, std::size_t size,
                             const char *pattern, std::vector<std::uint8_t> bytes,
                             std::uintptr_t hook) {
    patch::PatchDescription d;
    d.feature = feature;
    d.target = kDiagnosticTargetId;
    d.location.pattern = pattern;
    d.location.scope = patch::SearchScope::Symbol(symbol, size);
    d.expected = patch::ExpectedBytes{0, std::move(bytes), {}};
    d.mutation.kind = patch::MutationKind::Call;
    d.mutation.callWidth = patch::CallWidth::FiveBytes;
    d.mutation.target = hook;
    return d;
}
patch::BatchResult Run(patch::Memory &memory, patch::ExecutableCodeAllocator *allocator, bool install) {
    patch::BatchResult result;
    const auto change = memory.ResolveSymbol("_ZN15CInstantTextBox12ChangeStringERK7CStringb", result.diagnostic.message);
    const auto format = memory.ResolveSymbol("_Z11PdxLocalizeIJRA5_KcRK7CStringEES3_PS0_DpOT_", result.diagnostic.message);
    if (!change || !format) {
        result.diagnostic.feature = "save-filenames";
        result.diagnostic.target = kDiagnosticTargetId;
        result.diagnostic.operation = patch::PatchOperation::ResolveSymbol;
        return result;
    }
    patch::PatchBatch batch(memory, allocator);
    // Replace only the two filename-specific transliteration calls. Native
    // RemoveSpecialCharacters only folds high bytes; it does not filter ASCII.
    batch.Add(Call("save-filenames.disk", "_ZN15CIngameSaveMenu8SaveGameEv", 0x3f5,
        "E8 5A 04 87 00", {0xE8,0x5A,0x04,0x87,0}, Addr(portable::ToDiskNameInPlace)));
    batch.Add(Call("save-filenames.select", "_ZN15CIngameSaveMenu14SaveGameSelectEP9CCheckBox", 0xfe,
        "E8 0C FE 86 00", {0xE8,0x0C,0xFE,0x86,0}, Addr(portable::ToDisplayNameInPlace)));
    constexpr char item[] = "_ZN18CLocalSavegameItemC1ERK7CStringS2_bS2_";
    batch.Add(Call("save-filenames.folder", item, 0x156, "E8 BA F0 35 00",
        {0xE8,0xBA,0xF0,0x35,0}, Addr(Display)));
    batch.Add(Call("save-filenames.item", item, 0x156, "E8 72 F0 35 00",
        {0xE8,0x72,0xF0,0x35,0}, Addr(Display)));
    batch.Add(Call("save-filenames.confirm-save",
        "_ZN12CConfirmSaveC1EP9CEU3IdlerRK7CStringP21CloudFileCLOUDSTORAGEbbRKSt6vectorIS2_SaIS2_EE", 0x23d,
        "E8 52 5A 17 FF", {0xE8,0x52,0x5A,0x17,0xFF}, Addr(LocalizeDisplay)));
    batch.Add(Call("save-filenames.confirm-load",
        "_ZN16CConfirmLoadSaveC1EP15CIngameLoadMenuRK7CString", 0x1c5,
        "E8 E9 51 17 FF", {0xE8,0xE9,0x51,0x17,0xFF}, Addr(LocalizeDisplay)));
    batch.Add(Call("save-filenames.confirm-delete-game",
        "_ZN25CConfirmLocalDeleteInGameC1EP9CEU3IdlerRK7CStringS4_RKSt8functionIFvvEE", 0x1f2,
        "E8 A8 4D 17 FF", {0xE8,0xA8,0x4D,0x17,0xFF}, Addr(LocalizeDisplay)));
    batch.Add(Call("save-filenames.confirm-delete-menu",
        "_ZN28CConfirmLocalDeleteGameSetupC1EP9CFrontEndP10CGameSetupRK7CStringS6_", 0x22f,
        "E8 4D 4B 17 FF", {0xE8,0x4D,0x4B,0x17,0xFF}, Addr(LocalizeDisplay)));
    // Both append UTF-8 filenames after a color prefix. Never convert the
    // prefix or mutate the stored name used by the file loader.
    batch.Add(Call("save-filenames.header", "_ZN18CLocalSavegameItem16UpdateHeaderInfoEv",
        0x1007, "E8 DA F3 81 00", {0xE8,0xDA,0xF3,0x81,0}, Addr(portable::AppendDisplayCopy)));
    batch.Add(Call("save-filenames.continue-tooltip", "_ZN9CFrontEnd17GetCurrentTooltipERK8CVector2IfEP10CGuiObject",
        0x10ee, "E8 7F 05 8F 00", {0xE8,0x7F,0x05,0x8F,0}, Addr(portable::AppendDisplayCopy)));
    if (!install) return batch.Preflight();
    changeString = reinterpret_cast<ChangeString>(*change);
    localize = reinterpret_cast<Localize>(*format);
    result = batch.Commit();
    if (!result && !patch::MustRetainSlots(result)) { changeString = nullptr; localize = nullptr; }
    return result;
}
}
patch::BatchResult PreflightSave(patch::Memory &m, patch::ExecutableCodeAllocator *a) { return Run(m,a,false); }
patch::BatchResult InstallSave(patch::Memory &m, patch::ExecutableCodeAllocator *a) { return Run(m,a,true); }
}
