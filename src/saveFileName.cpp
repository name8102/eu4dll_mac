
#include "saveFileName.h"
#include "platform/macos/symbol_lookup.h"
#include "runtime/diagnostics/startup_diagnostics.h"
#include "features/save_filenames/save_filenames.h"
#include "targets/eu4_1_37_5/macos_x86_64/localization_features/localization_patch.h"
#include "targets/eu4_1_37_5/macos_x86_64/target_facts.h"

namespace saveFileName {
    namespace target = eu4dll::targets::eu4_1_37_5::macos_x86_64;
    namespace patcher = target::localization_features;

    bool InstallJump(const char *feature, const target::HookSite &site,
                     uintptr_t hook, std::vector<uint8_t> expected,
                     std::initializer_list<patcher::ContinuationBinding> continuations,
                     bool optimize = true,
                     std::vector<std::string> referencedStrings = {},
                     std::string symbol = {}, std::size_t symbolSearchSize = 0) {
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
        request.referencedStrings = std::move(referencedStrings);
        request.symbol = std::move(symbol);
        request.symbolSearchSize = symbolSearchSize;
        request.continuations = continuations;
        request.optimizeNakedHook = optimize;
        return patcher::Install(request);
    }

    std::string *ToDiskName(std::string *text) {
        if (text != nullptr) {
            eu4dll::features::save_filenames::ToDiskNameInPlace(*text);
        }
        return text;
    }

    std::string *ToDisplayName(std::string *text) {
        if (text != nullptr) {
            eu4dll::features::save_filenames::ToDisplayNameInPlace(*text);
        }
        return text;
    }

    extern "C" uintptr_t g_SaveFilename_ToDiskName =
        reinterpret_cast<uintptr_t>(ToDiskName);
    extern "C" uintptr_t g_SaveFilename_ToDisplayName =
        reinterpret_cast<uintptr_t>(ToDisplayName);
    extern "C" uintptr_t g_SaveFilename_ConstructDisplayCopy =
        reinterpret_cast<uintptr_t>(
            eu4dll::features::save_filenames::ConstructDisplayCopy);
    void AppendDisplayCopy(std::string *destination, const std::string *source) {
        if (destination != nullptr && source != nullptr) {
            eu4dll::features::save_filenames::AppendDisplayCopy(*destination, *source);
        }
    }
    extern "C" uintptr_t g_SaveFilename_AppendDisplayCopy =
        reinterpret_cast<uintptr_t>(AppendDisplayCopy);

    bool g_ConfirmDeleteDisplayScratchEnabled = false;
    void DestroyConfirmDeleteDisplayScratch(void *storage) {
        if (g_ConfirmDeleteDisplayScratchEnabled) {
            eu4dll::features::save_filenames::DestroyDisplayCopy(storage);
        }
    }
    extern "C" uintptr_t g_SaveFilename_DestroyConfirmDeleteDisplayScratch =
        reinterpret_cast<uintptr_t>(DestroyConfirmDeleteDisplayScratch);

/**
 Hook函数：CString::RemoveSpecialCharacters
 作用：阻止其过滤字符串，直接返回
 */
    void install_CString_RemoveSpecialCharacters() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        static constexpr target::HookSite site{"55 48 89 E5", 0};
        patcher::InstallRequest request;
        request.feature = "save-filenames.remove-special-characters";
        request.site = &site;
        request.mutationKind = eu4dll::patch::MutationKind::RawBytes;
        request.mutationBytes.assign(
            std::begin(target::save_filename::kRemoveSpecialCharactersBytes),
            std::end(target::save_filename::kRemoveSpecialCharactersBytes));
        request.expectedBytes = {0x55, 0x48, 0x89, 0xE5};
        request.overwrittenLength = 4;
        request.symbol = target::symbols::kCStringRemoveSpecialCharacters;
        request.symbolSearchSize = 64;
        if (patcher::Install(request)) installGuard.MarkSuccess();
    }

    extern "C" uintptr_t g_SaveGame_RetAddr = 0;

    __attribute__((naked)) void naked_CIngameSaveMenu_SaveGame() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"

                "call [rip + _g_SaveFilename_ToDiskName] \n"
                "jmp [rip + _g_SaveGame_RetAddr] \n"
                ".att_syntax prefix \n"
                );
    }

/**
 Hook函数：CIngameSaveMenu::SaveGame
 作用：将获取到的存档名从带标识符的双字节逃逸文本（UTF16LE）转换为UTF8格式，使其能保持正确的名字存储在硬盘上。
 */
    void install_CIngameSaveMenu_SaveGame() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto &site = target::save_filename::kSaveGame;
        if (InstallJump("save-filenames.save-game", site,
                        reinterpret_cast<uintptr_t>(naked_CIngameSaveMenu_SaveGame),
                        {0xE8, 0xC2, 0x10, 0x8D, 0x00},
                        {{"return", site.continuationOffset, &g_SaveGame_RetAddr}})) {
            installGuard.MarkSuccess();
        }
    }

    extern "C" uintptr_t g_CLocalSavegameItem_RetAddr = 0;


    __attribute__((naked)) void naked_CLocalSavegameItem_CLocalSavegameItem() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"
                "mov rdi, r12 \n"
                "call [rip + _g_SaveFilename_ToDisplayName] \n"
                "mov rdi, r14 \n"
                "mov rsi, r12 \n"
                "jmp [rip + _g_CLocalSavegameItem_RetAddr] \n"
                ".att_syntax prefix \n"
                );
    }

/**
 Hook函数：CLocalSavegameItem::CLocalSavegameItem
 作用：HOOK初始化函数，将从硬盘上获取到的UTF8存档文件名转换为逃逸文本，使其能正确显示在存档列表
 */
    void install_CLocalSavegameItem_CLocalSavegameItem() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto &site = target::save_filename::kLocalSavegameItemConstructor;
        if (InstallJump("save-filenames.local-item-constructor", site,
                        reinterpret_cast<uintptr_t>(naked_CLocalSavegameItem_CLocalSavegameItem),
                        {0x4C, 0x89, 0xF7, 0x4C, 0x89},
                        {{"return", site.continuationOffset, &g_CLocalSavegameItem_RetAddr}})) {
            installGuard.MarkSuccess();
        }
    }

    extern "C" uintptr_t g_CConfirmSave_RetAddr = 0;

    __attribute__((naked)) void naked_CConfirmSave_CConfirmSave() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"
                "mov rdi, r14 \n"
                "call [rip + _g_SaveFilename_ToDisplayName] \n"
                "mov rdi, [r12+8] \n"
                "jmp [rip + _g_CConfirmSave_RetAddr] \n"
                ".att_syntax prefix \n"
                );
    }

/**
 Hook函数：CConfirmSave::CConfirmSave
 作用：存档覆盖提示的UTF8文件名转换为逃逸文本
 */
    void install_CConfirmSave_CConfirmSave() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto &site = target::save_filename::kConfirmSave;
        if (InstallJump("save-filenames.confirm-save", site,
                        reinterpret_cast<uintptr_t>(naked_CConfirmSave_CConfirmSave),
                        {0x49, 0x8B, 0x7C, 0x24, 0x08},
                        {{"return", site.continuationOffset, &g_CConfirmSave_RetAddr}}, true,
                        {target::save_filename::kConfirmSaveText},
                        target::symbols::kConfirmSaveConstructor,
                        target::save_filename::kConstructorSearchSize)) {
            installGuard.MarkSuccess();
        }
    }


/**
 Hook函数：CLocalSavegameItem::UpdateHeaderInfo
 作用：保留已构造提示前缀，将UTF8文件名的独立显示副本追加到目标字符串
 */
    void install_CLocalSavegameItem_UpdateHeaderInfo() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        static constexpr target::HookSite appendCallSite{
            target::save_filename::kUpdateHeaderInfo.pattern, 11};
        patcher::InstallRequest request;
        request.feature = "save-filenames.update-header-display-copy";
        request.site = &appendCallSite;
        request.mutationKind = eu4dll::patch::MutationKind::Call;
        request.mutationTarget = g_SaveFilename_AppendDisplayCopy;
        request.callWidth = eu4dll::patch::CallWidth::FiveBytes;
        request.expectedBytes = {0xE8, 0x5B, 0x0B, 0x88, 0x00};
        request.expectedMask = {0xFF, 0, 0, 0, 0};
        request.expectedOffset = appendCallSite.mutationOffset;
        request.overwrittenLength = 5;
        if (patcher::Install(request)) {
            installGuard.MarkSuccess();
        }
    }

    extern "C" uintptr_t g_DoLoadGame_RetAddr = 0;
    extern "C" void *g_EU4LoadGameHelper_Load_Addr = nullptr;

    __attribute__((naked)) void naked_CIngameLoadMenu_DoLoadGame() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"
                "lea rdi, [rbp-0x58] \n"
                "call [rip + _g_SaveFilename_ToDiskName] \n"
                "lea rdi, [rbp-0x58] \n"
                "call [rip + _g_EU4LoadGameHelper_Load_Addr] \n"
                "jmp [rip + _g_DoLoadGame_RetAddr] \n"
                ".att_syntax prefix \n"
                );
    }

/**
 Hook函数：CIngameLoadMenu::DoLoadGame
 作用：确认载入存档时将逃逸文本转换为UTF8真实文件名
 */
    void install_CIngameLoadMenu_DoLoadGame() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto &site = target::save_filename::kDoLoadGame;
        g_EU4LoadGameHelper_Load_Addr = eu4dll::platform::macos::ResolveLiveSymbol(
            "save-filenames.symbol.load-game", target::kDiagnosticTargetId,
            target::symbols::kEu4LoadGameHelperLoad);
        if (g_EU4LoadGameHelper_Load_Addr != nullptr &&
            InstallJump("save-filenames.load-game", site,
                        reinterpret_cast<uintptr_t>(naked_CIngameLoadMenu_DoLoadGame),
                        {0x48, 0x8D, 0x7D, 0xA8, 0xE8},
                        {{"return", site.continuationOffset, &g_DoLoadGame_RetAddr}})) {
            installGuard.MarkSuccess();
        }
    }

    extern "C" uintptr_t g_GetCurrentTooltip_RetAddr = 0;

    __attribute__((naked)) void naked_CFrontEnd_GetCurrentTooltip() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"
                "lea rdi, [rbp-0x2E0] \n"
                "lea rsi, [rbp-0xB0] \n"
                "call [rip + _g_SaveFilename_ConstructDisplayCopy] \n"
                "jmp [rip + _g_GetCurrentTooltip_RetAddr] \n"
                ".att_syntax prefix \n"
                );
    }

/**
 Hook函数：CFrontEnd::GetCurrentTooltip
 作用：开始界面继续游戏按钮的悬浮提示，将其中的UTF8存档名转换为逃逸文本，使其能正确显示
 */
    void install_CFrontEnd_GetCurrentTooltip() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto &site = target::save_filename::kGetCurrentTooltip;
        if (InstallJump("save-filenames.frontend-tooltip-display-copy", site,
                        reinterpret_cast<uintptr_t>(naked_CFrontEnd_GetCurrentTooltip),
                        {0x48, 0x8D, 0xBD, 0x50, 0xFF},
                        {{"return", 19, &g_GetCurrentTooltip_RetAddr}})) {
            installGuard.MarkSuccess();
        }
    }


    extern "C" uintptr_t g_CConfirmLocalDeleteInGame_RetAddr = 0;
    extern "C" uintptr_t g_CConfirmLocalDeleteInGame_CleanupRetAddr = 0;

    __attribute__((naked)) void naked_CConfirmLocalDeleteInGame_CConfirmLocalDeleteInGame() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"
                "mov rdi, r12 \n"
                "lea rsi, [rbp-0x90] \n"
                "call [rip + _g_SaveFilename_ConstructDisplayCopy] \n"
                "lea r12, [rbp-0x90] \n"

                "mov rdi, [rbx+8] \n"
                "mov rax, [rdi] \n"
                "jmp [rip + _g_CConfirmLocalDeleteInGame_RetAddr] \n"
                ".att_syntax prefix \n"
                );
    }

    __attribute__((naked)) void naked_CConfirmLocalDeleteInGame_CleanupDisplayCopy() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"
                "lea rdi, [rbp-0x90] \n"
                "call [rip + _g_SaveFilename_DestroyConfirmDeleteDisplayScratch] \n"
                "lea rsi, [rbp-0x60] \n"
                "mov rdi, rbx \n"
                "jmp [rip + _g_CConfirmLocalDeleteInGame_CleanupRetAddr] \n"
                ".att_syntax prefix \n"
                );
    }

/**
 Hook函数：CConfirmLocalDeleteInGame::CConfirmLocalDeleteInGame
 作用：删除存档确认提示框，将其中的UTF8存档名转换为逃逸文本，使其能正确显示
 */
    void install_CConfirmLocalDeleteInGame_CConfirmLocalDeleteInGame() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto &site = target::save_filename::kConfirmLocalDelete;
        static constexpr target::HookSite cleanupSite{
            "48 8D 75 A0 48 89 DF 31 D2 E8 ? ? ? ?", 0, 7};
        const bool cleanupInstalled = InstallJump(
            "save-filenames.confirm-delete-cleanup", cleanupSite,
            reinterpret_cast<uintptr_t>(naked_CConfirmLocalDeleteInGame_CleanupDisplayCopy),
            {0x48, 0x8D, 0x75, 0xA0, 0x48},
            {{"return", cleanupSite.continuationOffset,
              &g_CConfirmLocalDeleteInGame_CleanupRetAddr}}, false, {},
            target::symbols::kConfirmLocalDeleteConstructor,
            target::save_filename::kConstructorSearchSize);
        const bool prepareInstalled = cleanupInstalled && InstallJump(
                        "save-filenames.confirm-delete-display-copy", site,
                        reinterpret_cast<uintptr_t>(naked_CConfirmLocalDeleteInGame_CConfirmLocalDeleteInGame),
                        {0x48, 0x8B, 0x7B, 0x08, 0x48},
                        {{"return", site.continuationOffset,
                          &g_CConfirmLocalDeleteInGame_RetAddr}}, false,
                        {target::save_filename::kConfirmDeleteText},
                        target::symbols::kConfirmLocalDeleteConstructor,
                        target::save_filename::kConstructorSearchSize);
        if (prepareInstalled) {
            g_ConfirmDeleteDisplayScratchEnabled = true;
            installGuard.MarkSuccess();
        }
    }


    void install() {
        //阻止过滤字符串
        install_CString_RemoveSpecialCharacters();
        //让存档名使用UTF8编码保存
        install_CIngameSaveMenu_SaveGame();
        //让存档列表正确显示
        install_CLocalSavegameItem_CLocalSavegameItem();
        //存档覆盖提示框
        install_CConfirmSave_CConfirmSave();
        //存档列表鼠标悬浮提示
        install_CLocalSavegameItem_UpdateHeaderInfo();
        //确认载入存档提示框
        install_CIngameLoadMenu_DoLoadGame();
        //开始界面继续游戏按钮的悬浮提示
        install_CFrontEnd_GetCurrentTooltip();
        //删除存档确认提示框
        install_CConfirmLocalDeleteInGame_CConfirmLocalDeleteInGame();
    }
}
