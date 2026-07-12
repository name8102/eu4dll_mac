#include "localization.h"
#include "platform/macos/symbol_lookup.h"
#include "runtime/diagnostics/startup_diagnostics.h"
#include "pinyinHelper.h"
#include "features/east_asian_names/east_asian_names.h"
#include "features/date_formatting/date_formatting.h"
#include "features/escaped_text/escaped_text.h"
#include "features/localization_loading/localization_loading.h"
#include "features/localized_search/localized_search.h"
#include "targets/eu4_1_37_5/macos_x86_64/localization_features/localization_patch.h"
#include "targets/eu4_1_37_5/macos_x86_64/target_facts.h"
#include <CoreFoundation/CoreFoundation.h>
#include <filesystem>

namespace localization {
    namespace target = eu4dll::targets::eu4_1_37_5::macos_x86_64;
    namespace names = eu4dll::features::east_asian_names;
    namespace search = eu4dll::features::localized_search;
    namespace patcher = target::localization_features;

    bool InstallJump(const char *feature, const target::HookSite &site,
                     uintptr_t hook, std::vector<uint8_t> expected,
                     std::initializer_list<patcher::ContinuationBinding> continuations,
                     bool optimize = true) {
        patcher::InstallRequest request;
        request.feature = feature;
        request.site = &site;
        request.mutationKind = eu4dll::patch::MutationKind::Jump;
        request.mutationTarget = hook;
        request.expectedBytes = std::move(expected);
        if (request.expectedBytes.size() == 5 && request.expectedBytes[0] == 0xE8) {
            request.expectedMask = {0xFF, 0, 0, 0, 0};
        }
        request.expectedOffset = site.mutationOffset;
        request.overwrittenLength = 5;
        request.continuations = continuations;
        request.optimizeNakedHook = optimize;
        return patcher::Install(request);
    }

    bool InstallCall(const char *feature, const target::HookSite &site,
                     uintptr_t replacement, std::vector<uint8_t> expected) {
        patcher::InstallRequest request;
        request.feature = feature;
        request.site = &site;
        request.mutationKind = eu4dll::patch::MutationKind::Call;
        request.mutationTarget = replacement;
        request.callWidth = eu4dll::patch::CallWidth::Auto;
        request.expectedBytes = std::move(expected);
        if (request.expectedBytes.size() == 5 && request.expectedBytes[0] == 0xE8) {
            request.expectedMask = {0xFF, 0, 0, 0, 0};
        }
        request.expectedOffset = site.mutationOffset;
        request.overwrittenLength = 5;
        return patcher::Install(request);
    }

    static bool g_is_Bimillennium_Universalis = false;

    search::SearchEngine &LocalizedSearch() {
        // Construct only after the dictionary path has been configured.
        static search::SearchEngine engine(PinyinHelper::getInstance());
        return engine;
    }

    void ConvertUtf8Localization(const char *input, char *output) {
        eu4dll::features::localization_loading::ConvertUtf8ForEu4(
            input, output, eu4dll::escaped_text::kLegacyOutputCapacity + 1);
    }

/**
 修改函数：CTopbarGui::RefreshSpeedControlsWindow
 作用：将此函数使用的日期格式文本从"d w mw w y"修改为"y  mw d "
*/
    void install_CTopbarGui_RefreshSpeedControlsWindow() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto &site = target::localization::kDateFormat;
        const auto &format = eu4dll::features::date_formatting::kYearMonthDayFormat;
        patcher::InstallRequest request;
        request.feature = "date-formatting.topbar-year-month-day";
        request.site = &site;
        request.mutationKind = eu4dll::patch::MutationKind::RawBytes;
        request.mutationBytes.assign(format.begin(), format.end());
        request.expectedBytes = {0x64, 0x20, 0x77, 0x20, 0x6D,
                                 0x77, 0x20, 0x77, 0x20, 0x79};
        request.overwrittenLength = format.size();
        if (patcher::Install(request)) installGuard.MarkSuccess();
    }

/**
 Hook函数：LocalizeYmlAddKey
 作用：拦截替换本地化文件解析过程中对ConvertUTF8ToLatin1函数的调用，使其支持将未转码的UTF8文本转换为游戏内可显示的逃逸文本而不是用‘?’代替，从而无需预先转码yml文件，便于快速翻译MOD。
 */
    void install_LocalizeYmlAddKey() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto &site = target::localization::kLocalizeYml;
        if (InstallCall("localization-loading.utf8", site,
                        reinterpret_cast<uintptr_t>(ConvertUtf8Localization),
                        {0xE8, 0x46, 0x79, 0x1C, 0x00})) {
            installGuard.MarkSuccess();
        }
    }

    struct CCountryTag {
        char tag[4];
        int16_t nIndex;
        uint8_t type;
        bool bIsValid;
    };
    struct SGotoBoxSearchEntry {
        int nDistance;
        int nProvince;
        CCountryTag Country;
        std::string AreaTag;
        std::string OriginalName;
        std::string CleanedName;
        std::string CleanedLocalName;
    };

    bool
    Proxy_CGotoBoxSearchList_Process_Comparing(bool flag, std::string *searchString, SGotoBoxSearchEntry *entry) {
        const auto result = LocalizedSearch().Match(flag, *searchString,
                                                     entry->OriginalName,
                                                     entry->CleanedName);
        if (result.matched && !flag) entry->nDistance = result.distance;
        return result.matched;
    }

    extern "C" uintptr_t g_Process_Comparing_Addr = (uintptr_t) Proxy_CGotoBoxSearchList_Process_Comparing;
    extern "C" uintptr_t g_Process_success_RetAddr = 0;
    extern "C" uintptr_t g_Process_fail_RetAddr = 0;

    __attribute__((naked)) void naked_CGotoBoxSearchList_Process() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"
                "imul r12, r15, %c[entry_stride] \n"
                "movzx rdi, byte ptr [rbp-0x44] \n"

                "mov rsi, r13 \n"

                "push rdx \n"
                "lea rbx, [rdx + r12] \n"
                "mov rdx, rbx \n"

                "call [rip + _g_Process_Comparing_Addr] \n"
                "pop rdx \n"
                "test al, al \n"
                "jz 1f \n"
                "mov rax, [rbp - 0x30] \n"
                "mov byte ptr [rax + %c[matched]], 1 \n"
                "jmp [rip + _g_Process_success_RetAddr] \n"

                "1: \n"
                "jmp [rip + _g_Process_fail_RetAddr] \n"
                ".att_syntax prefix \n"
                :
                : [entry_stride] "i"(target::localization::kGotoSearchEntryStride),
                  [matched] "i"(target::localization::kGotoSearchMatchedOffset)
                );
    }

/**
 Hook函数：CGotoBoxSearchList::Process
 作用：拦截修改EU4查找功能的比对逻辑，使其支持拼音和首字母缩写搜索
 */
    void install_CGotoBoxSearchList_Process() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto &site = target::localization::kGotoBoxProcess;
        if (InstallJump("localized-search.goto-box", site,
                        reinterpret_cast<uintptr_t>(naked_CGotoBoxSearchList_Process),
                        {0x4D, 0x6B, 0xE7, 0x70, 0x80},
                        {{"success", site.continuationOffset, &g_Process_success_RetAddr},
                         {"failure", site.bypassOffset, &g_Process_fail_RetAddr}}, false)) {
            installGuard.MarkSuccess();
        }
    }

    std::string GetHostResourcePath() {
        CFBundleRef mainBundle = CFBundleGetMainBundle();
        if (!mainBundle) return "";

        CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
        if (!resourcesURL) return "";

        char path[PATH_MAX];
        if (CFURLGetFileSystemRepresentation(resourcesURL, true, (UInt8 *) path, PATH_MAX)) {
            CFRelease(resourcesURL);
            return {path};
        }

        CFRelease(resourcesURL);
        return "";
    }

    template<typename T, typename SizeType = int>
    struct CPdxArray {
        void *vftable;         // 0x00: 虚函数表
        T *_pData;             // 0x08: 指向数组连续内存的指针
        SizeType _nCapacity;   // 0x10: 容量 (4 字节 int)
        SizeType _nSize;       // 0x14: 实际大小 (4 字节 int)
    };
    using PdxStringArray = CPdxArray<std::string, int>;
    static_assert(offsetof(PdxStringArray, _pData) ==
                  target::localization::kPdxArrayDataOffset);
    static_assert(offsetof(PdxStringArray, _nCapacity) ==
                  target::localization::kPdxArrayCapacityOffset);
    static_assert(offsetof(PdxStringArray, _nSize) ==
                  target::localization::kPdxArraySizeOffset);

    typedef void *(*fnCDLCManager_AccessInstance_t)(void *pThis);

    typedef void *(*fnCPdxCommandLine_HasOption_t)(void *pThis, const char *str);

    void *fnCDLCManager_AccessInstance_Addr = nullptr;
    void *fnCPdxCommandLine_HasOption_Addr = nullptr;

    bool Proxy_CPdxCommandLine_HasOption(void *obj, const char *str) {
        auto getInstance = (fnCDLCManager_AccessInstance_t) fnCDLCManager_AccessInstance_Addr;
        void *instance = getInstance(nullptr);
        auto *enabledMods = reinterpret_cast<PdxStringArray *>(
                (uintptr_t) instance + target::localization::kDlcManagerEnabledModsOffset
        );

        for (int i = 0; i < enabledMods->_nSize; ++i) {
            const std::string &modNameOrPath = enabledMods->_pData[i];
            if (modNameOrPath.find("eu4_chinese.mod") != std::string::npos ||
                modNameOrPath.find("Bimillennium_Universalis_") != std::string::npos) {

                if (modNameOrPath.find("Bimillennium_Universalis_") !=
                    std::string::npos) {
                    g_is_Bimillennium_Universalis = true;
                }

                auto patch = GetHostResourcePath();
                if (patch.empty()) {
                    printf("加载字典资源路径失败！\n");
                    break;
                }
                patch += "/chinese_dict";
                if (!std::filesystem::exists(patch)) {
                    printf("字典路径不存在！%s\n", patch.c_str());
                    break;
                }
                PinyinHelper::SetDictionaryPath(patch);
                install_CGotoBoxSearchList_Process();
                break;
            }
            printf("Mod [%d]: %s\n", i, modNameOrPath.c_str());
        }
        auto org = (fnCPdxCommandLine_HasOption_t) fnCPdxCommandLine_HasOption_Addr;
        return org(obj, str);
    }

/**
 Hook函数：_main
 作用：HOOK游戏初始化函数，在加载工作完成后检查是否启用了汉化模组，如果启用了就开启拼音搜索功能
 */
    void install_main() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto &site = target::localization::kMain;
        if (InstallCall("localized-search.enable-for-chinese-mod", site,
                        reinterpret_cast<uintptr_t>(Proxy_CPdxCommandLine_HasOption),
                        {0xE8, 0x42, 0x2B, 0x11, 0x01})) {
            installGuard.MarkSuccess();
        }
    }

    void Monarch_GetFullName_Reversal(uintptr_t monarch, std::string *result, const std::string *lastName) {
        uintptr_t culture = *(uintptr_t *) (
            monarch + target::localization::kMonarchCultureOffset);
        auto *culture_tag = (std::string *) (
            culture + target::localization::kCultureTagOffset);
        uintptr_t cultureGroup = *(uintptr_t *) (
            culture + target::localization::kCultureGroupOffset);
        auto *cultureGroup_tag = (std::string *) (
            cultureGroup + target::localization::kCultureTagOffset);
        const auto policy = names::PolicyFor(
            *cultureGroup_tag, *culture_tag, g_is_Bimillennium_Universalis
                ? names::CultureMode::BimillenniumUniversalis
                : names::CultureMode::Vanilla);
        *result = names::Format(*result, *lastName, policy);
    }

    extern "C" uintptr_t g_CMonarch_GetFullName_CallAddr = (uintptr_t) Monarch_GetFullName_Reversal;
    extern "C" uintptr_t g_CMonarch_GetFullName_retAddr = 0;

    __attribute__((naked)) void naked_CMonarch_GetFullName() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"
                "mov r15, [rbx+%c[dynasty]] \n" // 君主家族信息偏移 CDynasty
                "add r15, %c[surname] \n" // CDynasty+8=家族姓

                "mov rdi, rbx \n"// obj
                "mov rsi, r14 \n"// 名
                "mov rdx, r15 \n"// 姓
                "call [rip + _g_CMonarch_GetFullName_CallAddr] \n"
                "jmp [rip + _g_CMonarch_GetFullName_retAddr] \n"
                ".att_syntax prefix \n"
                :
                : [dynasty] "i"(target::localization::kMonarchDynastyOffset),
                  [surname] "i"(target::localization::kDynastySurnameOffset)
                );
    }

/**
 Hook函数：CMonarch::GetFullName
 作用：HOOK君王获取名字函数，将东亚文化组的君王名字修改为姓在名前
 */
    void install_CMonarch_GetFullName() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto &site = target::localization::kMonarchFullName;
        if (InstallJump("east-asian-names.monarch", site,
                        reinterpret_cast<uintptr_t>(naked_CMonarch_GetFullName),
                        {0x4C, 0x8B, 0x7B, 0x58, 0x48},
                        {{"return", site.continuationOffset, &g_CMonarch_GetFullName_retAddr}})) {
            installGuard.MarkSuccess();
        }
    }

    void Country_GetNewRepublicName_Reversal(std::string *result, std::string *lastName, uintptr_t culture) {
        auto *culture_tag = (std::string *) (
            culture + target::localization::kCultureTagOffset);
        uintptr_t cultureGroup = *(uintptr_t *) (
            culture + target::localization::kCultureGroupOffset);
        auto *cultureGroup_tag = (std::string *) (
            cultureGroup + target::localization::kCultureTagOffset);

        const auto policy = names::PolicyFor(
            *cultureGroup_tag, *culture_tag, g_is_Bimillennium_Universalis
                ? names::CultureMode::BimillenniumUniversalis
                : names::CultureMode::Vanilla);
        if (policy.order == names::NameOrder::GivenThenSurname) {
            result->append(*lastName);
            return;
        }
        if (!result->empty()) result->pop_back();
        *result = names::Format(*result, *lastName, policy);
    }

    extern "C" uintptr_t g_CCountry_GetNewRepublicName_CallAddr = (uintptr_t) Country_GetNewRepublicName_Reversal;
    extern "C" uintptr_t g_CCountry_GetNewRepublicName_RetAddr = 0;

    __attribute__((naked)) void naked_CCountry_GetNewRepublicName() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"

                "mov rdx, [rbp-0x38] \n"// 文化
                "call [rip + _g_CCountry_GetNewRepublicName_CallAddr] \n"
                "jmp [rip + _g_CCountry_GetNewRepublicName_RetAddr] \n"
                ".att_syntax prefix \n"
                );
    }

/**
 Hook函数：CCountry::GetNewRepublicName 指定姓分支
 作用：HOOK商人、外交官、将领等通用名字生成函数，将东亚文化组名字修改为姓在名前
 */
    void install_CCountry_GetNewRepublicName() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto &site = target::localization::kCountryNewRepublicName;
        if (InstallJump("east-asian-names.republic-explicit", site,
                        reinterpret_cast<uintptr_t>(naked_CCountry_GetNewRepublicName),
                        {0xE8, 0x21, 0x23, 0x5E, 0x01},
                        {{"return", site.continuationOffset,
                          &g_CCountry_GetNewRepublicName_RetAddr}})) {
            installGuard.MarkSuccess();
        }
    }

/**
 Hook函数：CCountry::GetNewRepublicName 随机姓分支
 作用：HOOK商人、外交官、将领等通用名字生成函数，将东亚文化组名字修改为姓在名前
 */
    void install_CCountry_GetNewRepublicName_1() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto &site = target::localization::kCountryNewRepublicNameRandom;
        if (InstallJump("east-asian-names.republic-random", site,
                        reinterpret_cast<uintptr_t>(naked_CCountry_GetNewRepublicName),
                        {0xE8, 0xE6, 0x22, 0x5E, 0x01},
                        {}, false)) installGuard.MarkSuccess();
    }

/**
 Hook函数：CCountry::GetNewRepublicName 文化姓分支
 作用：HOOK商人、外交官、将领等通用名字生成函数，将东亚文化组名字修改为姓在名前
 */
    void install_CCountry_GetNewRepublicName_2() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto &site = target::localization::kCountryNewRepublicNameCulture;
        if (InstallJump("east-asian-names.republic-culture", site,
                        reinterpret_cast<uintptr_t>(naked_CCountry_GetNewRepublicName),
                        {0xE8, 0x89, 0x22, 0x5E, 0x01},
                        {}, false)) installGuard.MarkSuccess();
    }

    void install() {
        //游戏界面右上角日期显示格式修改为年月日
        install_CTopbarGui_RefreshSpeedControlsWindow();
        //加载本地化文件时自动将UTF8转码为逃逸文本，从而无需预先转码文件
        install_LocalizeYmlAddKey();

        fnCDLCManager_AccessInstance_Addr =
            eu4dll::platform::macos::ResolveLiveSymbol(
                "localization.symbol.dlc-manager", target::kDiagnosticTargetId,
                target::symbols::kDlcManagerAccessInstance);
        fnCPdxCommandLine_HasOption_Addr =
            eu4dll::platform::macos::ResolveLiveSymbol(
                "localization.symbol.command-line-option", target::kDiagnosticTargetId,
                target::symbols::kCommandLineHasOption);

        //根据是否加载汉化MOD来决定是否启用拼音搜索功能
        if (fnCDLCManager_AccessInstance_Addr != nullptr &&
            fnCPdxCommandLine_HasOption_Addr != nullptr) {
            install_main();
        }
        //东亚君主名字反转
        install_CMonarch_GetFullName();
        //东亚随机生成名字反转
        install_CCountry_GetNewRepublicName();
        install_CCountry_GetNewRepublicName_1();
        install_CCountry_GetNewRepublicName_2();
    }
}
