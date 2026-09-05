#include "features/date_formatting/date_formatting.h"
#include "features/east_asian_names/east_asian_names.h"
#include "features/localization_loading/localization_loading.h"
#include "features/localized_search/localized_search.h"
#include "features/save_filenames/save_filenames.h"
#include "runtime/patch/byte_buffer_memory.h"
#include "targets/eu4_1_37_5/macos_x86_64/localization_features/localization_patch.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

class FakePinyin final : public eu4dll::features::localized_search::PinyinProvider {
public:
    std::vector<std::string> ReadingsFor(const std::string &text) override {
        ++calls;
        characters.push_back(text);
        if (text == "重") return {"chong", "zhong"};
        if (text == "庆") return {"qing"};
        return {};
    }

    int calls = 0;
    std::vector<std::string> characters;
};

void TestLocalizedSearch() {
    using namespace eu4dll::features::localized_search;
    FakePinyin pinyin;
    SearchEngine search(pinyin);
    const auto escaped = eu4dll::features::save_filenames::DisplayCopy("重庆");

    auto match = search.Match(true, "CHONGQING", escaped, escaped);
    Require(match.matched, "full pinyin should normalize and match exactly");
    Require(pinyin.characters == std::vector<std::string>({"重", "庆"}),
            "provider should receive one decoded Chinese character at a time");

    match = search.Match(true, "cq", escaped, escaped);
    Require(match.matched, "pinyin initials should match");
    match = search.Match(true, "zhongqing", escaped, escaped);
    Require(match.matched, "polyphonic full form should match");
    match = search.Match(false, "qing", escaped, escaped);
    Require(match.matched && match.distance == 1,
            "suffix fuzzy match should preserve ranking score");
    match = search.Match(false, "chong", escaped, escaped);
    Require(match.matched && match.distance == -2,
            "prefix fuzzy match should rank before suffix matches");
    Require(pinyin.calls == 2 && search.CacheSize() == 1,
            "candidate forms should be cached by original name");

    class UmlautPinyin final : public PinyinProvider {
    public:
        std::vector<std::string> ReadingsFor(const std::string &) override {
            return {"lüe"};
        }
    } umlaut;
    const auto umlautForms = GenerateForms("略", umlaut);
    Require(std::find(umlautForms.begin(), umlautForms.end(), "lue") != umlautForms.end(),
            "localized search should own umlaut normalization");

    const std::string escapedQuery = escaped.substr(0, 3);
    match = search.Match(false, escapedQuery, escaped, escaped);
    Require(match.matched, "escaped Chinese query should use cleaned-name matching");
}

void TestNameOrdering() {
    using namespace eu4dll::features::east_asian_names;
    struct Case {
        const char *group;
        const char *culture;
        CultureMode mode;
        NameOrder order;
        const char *separator;
    };
    const std::vector<Case> cases{
        {"east_asian", "han", CultureMode::Vanilla, NameOrder::SurnameThenGiven, ""},
        {"korean_g", "korean", CultureMode::Vanilla, NameOrder::SurnameThenGiven, " "},
        {"western", "french", CultureMode::Vanilla, NameOrder::GivenThenSurname, " "},
        {"western", "zhuang", CultureMode::Vanilla, NameOrder::SurnameThenGiven, " "},
        {"mongolic", "khalkha", CultureMode::Vanilla, NameOrder::GivenThenSurname, " "},
        {"mongolic", "khalkha", CultureMode::BimillenniumUniversalis,
         NameOrder::SurnameThenGiven, " "},
    };
    for (const auto &test : cases) {
        const auto policy = PolicyFor(test.group, test.culture, test.mode);
        Require(policy.order == test.order, "culture table should select name order");
        Require(policy.separator == test.separator, "culture table should select separator");
    }
    Require(Format("名", "姓", PolicyFor("east_asian", "han", CultureMode::Vanilla)) ==
                "姓名", "Chinese names should be surname-first without separator");
}

void TestSaveAndLocalizationConversions() {
    const std::string utf8 = "大明存档";
    const auto display = eu4dll::features::save_filenames::DisplayCopy(utf8);
    auto disk = display;
    Require(eu4dll::features::save_filenames::ToDiskNameInPlace(disk) == utf8,
            "save filename conversion should round trip through escaped text");

    const std::string diskSource = utf8;
    const auto firstDisplay = eu4dll::features::save_filenames::DisplayCopy(diskSource);
    const auto secondDisplay = eu4dll::features::save_filenames::DisplayCopy(diskSource);
    Require(diskSource == utf8, "display conversion must not mutate the disk UTF-8 source");
    Require(firstDisplay == secondDisplay,
            "repeated display conversion should return independent stable values");
    auto changedDisplay = firstDisplay;
    changedDisplay.clear();
    Require(!secondDisplay.empty(), "display copies must not alias shared temporary storage");

    std::string tooltip = "\xA7Y";
    const auto *tooltipObject = &tooltip;
    eu4dll::features::save_filenames::AppendDisplayCopy(tooltip, diskSource);
    Require(&tooltip == tooltipObject,
            "append conversion must preserve the live destination object");
    Require(tooltip.rfind("\xA7Y", 0) == 0,
            "append conversion must preserve the existing tooltip prefix");
    Require(tooltip.substr(2) == secondDisplay,
            "append conversion should append exactly one converted filename");
    Require(diskSource == utf8,
            "append conversion must leave the disk filename source unchanged");

    alignas(std::string) unsigned char storage[sizeof(std::string)];
    eu4dll::features::save_filenames::ConstructDisplayCopy(&diskSource, storage);
    auto *constructed = reinterpret_cast<std::string *>(storage);
    Require(*constructed == secondDisplay && diskSource == utf8,
            "hook adapter should construct display text in caller-owned storage");
    eu4dll::features::save_filenames::DestroyDisplayCopy(constructed);

    char output[128]{};
    eu4dll::features::localization_loading::ConvertUtf8ForEu4(
        utf8.c_str(), output, sizeof(output));
    std::string localized(output);
    Require(eu4dll::features::save_filenames::ToDiskNameInPlace(localized) == utf8,
            "UTF-8 localization loading should produce EU4 escaped text");
    Require(eu4dll::features::date_formatting::kYearMonthDayFormat.front() == 'y' &&
                eu4dll::features::date_formatting::kYearMonthDayFormat.back() == 0x0E,
            "date format should retain the visible year-month-day byte sequence");
}

void TestPatchContracts() {
    namespace target = eu4dll::targets::eu4_1_37_5::macos_x86_64;
    namespace hooks = target::localization_features;
    static constexpr target::HookSite site{"AA BB CC ? EE", 0, 5};
    hooks::InstallRequest request;
    request.feature = "localization-contract-test";
    request.site = &site;
    request.mutationKind = eu4dll::patch::MutationKind::RawBytes;
    request.mutationBytes = {0x90, 0x90, 0x90, 0x90, 0x90};
    request.expectedBytes = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    request.overwrittenLength = 5;
    request.continuations = {{"return", 5, nullptr}};

    const auto description = hooks::BuildDescription(request);
    Require(description.location.requireUnique,
            "localization hook contracts must require a unique match");
    Require(description.expected && description.expected->bytes.size() == 5,
            "hook contract should retain expected original bytes");

    eu4dll::patch::ByteBufferMemory memory(
        {0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x00});
    eu4dll::patch::PatchRuntime runtime(memory);
    const auto installed = runtime.Install(description);
    Require(installed && memory.Bytes()[1] == 0x90,
            "fixture contract should mutate only after expected-byte validation");

    eu4dll::patch::ByteBufferMemory duplicate(
        {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x00,
         0xAA, 0xBB, 0xCC, 0xDD, 0xEE});
    Require(!eu4dll::patch::PatchRuntime(duplicate).Install(description),
            "duplicate hook patterns must fail instead of selecting the first match");

    eu4dll::patch::ByteBufferMemory mismatch(
        {0xAA, 0xBB, 0xCC, 0x00, 0xEE});
    Require(!eu4dll::patch::PatchRuntime(mismatch).Install(description),
            "original-byte mismatch must prevent mutation");

    auto failingRequest = request;
    failingRequest.mutationKind = eu4dll::patch::MutationKind::Jump;
    failingRequest.mutationBytes.clear();
    failingRequest.mutationTarget = ~eu4dll::patch::Address{0};
    const auto failingDescription = hooks::BuildDescription(failingRequest);
    eu4dll::patch::ByteBufferMemory outOfRange(
        {0xAA, 0xBB, 0xCC, 0xDD, 0xEE});
    Require(!eu4dll::patch::PatchRuntime(outOfRange).Install(failingDescription),
            "mutation calculation failure must propagate to the installer result");

    static constexpr target::HookSite updateHeaderCallSite{
        target::save_filename::kUpdateHeaderInfo.pattern, 11};
    hooks::InstallRequest updateHeader;
    updateHeader.feature = "save-filenames.update-header-display-copy";
    updateHeader.site = &updateHeaderCallSite;
    updateHeader.mutationKind = eu4dll::patch::MutationKind::Call;
    updateHeader.mutationTarget = 0x1800;
    updateHeader.callWidth = eu4dll::patch::CallWidth::FiveBytes;
    updateHeader.expectedBytes = {0xE8, 0x5B, 0x0B, 0x88, 0x00};
    updateHeader.expectedOffset = 11;
    updateHeader.overwrittenLength = 5;
    const auto updateDescription = hooks::BuildDescription(updateHeader);
    Require(updateDescription.mutation.offset == 11,
            "UpdateHeaderInfo must patch only the original append call");
    eu4dll::patch::ByteBufferMemory updateFixture({
        0x48, 0x8D, 0xB0, 0x20, 0x02, 0x00, 0x00,
        0x48, 0x8D, 0x7D, 0x90,
        0xE8, 0x5B, 0x0B, 0x88, 0x00,
        0x48, 0x8B, 0x45, 0xA0});
    const auto updateInstalled =
        eu4dll::patch::PatchRuntime(updateFixture).Install(updateDescription);
    Require(static_cast<bool>(updateInstalled),
            "captured UpdateHeaderInfo instruction sequence should accept the call contract");
    Require(updateFixture.Bytes()[0] == 0x48 && updateFixture.Bytes()[7] == 0x48,
            "source and destination LEAs must remain untouched");
    Require(updateFixture.Bytes()[11] == 0xE8 && updateFixture.Bytes()[16] == 0x48,
            "replacement call must return to the instruction after original append");
}

} // namespace

int main() {
    TestLocalizedSearch();
    TestNameOrdering();
    TestSaveAndLocalizationConversions();
    TestPatchContracts();
    return 0;
}
