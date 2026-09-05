#include "targets/eu4_1_37_5/linux_x86_64/display_formatting/display_patch.h"
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"
#include "features/date_formatting/date_formatting.h"
#include "features/east_asian_names/east_asian_names.h"

#include <cstring>

namespace eu4dll::targets::eu4_1_37_5::linux_x86_64::display_formatting {
namespace {
namespace names = features::east_asian_names;
using DateString = void *(*)(std::string *, const void *, const std::string &);
DateString dateString = nullptr;
void **dlcManager = nullptr;
template<class T> T Field(const void *object, std::size_t offset) {
    T value;
    std::memcpy(&value, static_cast<const char *>(object) + offset, sizeof(value));
    return value;
}
names::CultureMode Mode() {
    if (dlcManager && *dlcManager) {
        // EnableMod uses CPdxArray at +0x88: data +8, count +0x14.
        const auto *mods = Field<const std::string *>(*dlcManager, 0x90);
        const auto count = Field<int>(*dlcManager, 0x9c);
        for (int i = 0; i < count; ++i) {
            if (mods[i].find("Bimillennium_Universalis_") != std::string::npos)
                return names::CultureMode::BimillenniumUniversalis;
        }
    }
    return names::CultureMode::Vanilla;
}
names::NamePolicy Policy(const void *culture) {
    if (!culture) return {};
    const auto *group = Field<const void *>(culture, 0x80);
    if (!group) return {};
    const auto &tag = *reinterpret_cast<const std::string *>(static_cast<const char *>(culture) + 0x40);
    const auto &groupTag = *reinterpret_cast<const std::string *>(static_cast<const char *>(group) + 0x38);
    return names::PolicyFor(groupTag, tag, Mode());
}
void *Date(std::string *output, const void *date, const std::string &) {
    const auto &bytes = features::date_formatting::kYearMonthDayFormat;
    const std::string format(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    return dateString(output, date, format);
}
extern "C" {
std::string *Eu4LinuxMonarchName(std::string *output, const std::string &suffix, const void *culture) {
    const auto policy = Policy(culture);
    if (policy.order == names::NameOrder::GivenThenSurname) output->append(suffix);
    else {
        // Native temporary is exactly a leading separator plus dynasty name.
        const auto surname = !suffix.empty() && suffix.front() == ' ' ? suffix.substr(1) : suffix;
        *output = names::Format(*output, surname, policy);
    }
    return output;
}
std::string *Eu4LinuxRepublicName(std::string *output, const std::string &surname, const void *culture) {
    const auto policy = Policy(culture);
    if (policy.order == names::NameOrder::GivenThenSurname) output->append(surname);
    else {
        auto given = *output;
        if (!given.empty() && given.back() == ' ') given.pop_back();
        *output = names::Format(given, surname, policy);
    }
    return output;
}
__attribute__((naked)) void Monarch() {
    asm volatile(".intel_syntax noprefix\n mov rdx, [rbx + 0x58]\n jmp Eu4LinuxMonarchName\n .att_syntax prefix\n");
}
__attribute__((naked)) void Republic() {
    // Call adds a return address to native rsp; culture was saved at +8.
    asm volatile(".intel_syntax noprefix\n mov rdx, [rsp + 0x10]\n jmp Eu4LinuxRepublicName\n .att_syntax prefix\n");
}
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
    const auto date = memory.ResolveSymbol("_ZNK14CGregorianDate9GetStringERK7CString", result.diagnostic.message);
    const auto manager = memory.ResolveSymbol("_ZN11CDLCManager10_pInstanceE", result.diagnostic.message);
    if (!date || !manager) {
        result.diagnostic.feature = "display-formatting";
        result.diagnostic.target = kDiagnosticTargetId;
        result.diagnostic.operation = patch::PatchOperation::ResolveSymbol;
        return result;
    }
    patch::PatchBatch batch(memory, allocator);
    batch.Add(Call("date-formatting.topbar", "_ZN10CTopbarGui26RefreshSpeedControlsWindowERK8CCountry",
        0x965, "E8 C2 C6 09 00", {0xE8,0xC2,0xC6,0x09,0}, Addr(Date)));
    batch.Add(Call("east-asian-names.monarch", "_ZNK8CMonarch11GetFullNameEv",
        0x273, "E8 6A 36 EC 00", {0xE8,0x6A,0x36,0xEC,0}, Addr(Monarch)));
    constexpr char republic[] = "_ZN8CCountry18GetNewRepublicNameE11CCountryTagPK8CCultureRK20CWeightedStringTableRK6CArrayI7CStringEbiiRbRKS8_bb";
    batch.Add(Call("east-asian-names.republic-explicit", republic, 0x421,
        "E8 0A 00 55 01", {0xE8,0x0A,0,0x55,1}, Addr(Republic)));
    batch.Add(Call("east-asian-names.republic-array", republic, 0x421,
        "E8 C2 FF 54 01", {0xE8,0xC2,0xFF,0x54,1}, Addr(Republic)));
    batch.Add(Call("east-asian-names.republic-dynasty", republic, 0x421,
        "E8 67 FF 54 01", {0xE8,0x67,0xFF,0x54,1}, Addr(Republic)));
    if (!install) return batch.Preflight();
    dateString = reinterpret_cast<DateString>(*date);
    dlcManager = reinterpret_cast<void **>(*manager);
    result = batch.Commit();
    if (!result && !patch::MustRetainSlots(result)) { dateString = nullptr; dlcManager = nullptr; }
    return result;
}
}
patch::BatchResult PreflightDisplay(patch::Memory &m, patch::ExecutableCodeAllocator *a) { return Run(m,a,false); }
patch::BatchResult InstallDisplay(patch::Memory &m, patch::ExecutableCodeAllocator *a) { return Run(m,a,true); }
}
