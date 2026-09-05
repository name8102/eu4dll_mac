#include "bootstrap/linux/bootstrap_linux.h"

#include "platform/linux/linux_elf_identity.h"
#include "platform/linux/linux_executable_allocator.h"
#include "platform/linux/linux_process_memory.h"
#include "runtime/patch/patch_batch.h"
#include "targets/eu4_1_37_5/linux_x86_64/base/base_patch.h"
#include "targets/eu4_1_37_5/linux_x86_64/profile.h"
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"
#include "targets/eu4_1_37_5/linux_x86_64/text_layout/layout_patch.h"
#include "targets/eu4_1_37_5/linux_x86_64/main_text/main_text_patch.h"
#include "targets/eu4_1_37_5/linux_x86_64/tooltip_text/tooltip_patch.h"
#include "targets/eu4_1_37_5/linux_x86_64/localization_text/localization_patch.h"
#include "targets/eu4_1_37_5/linux_x86_64/map_text/map_text_patch.h"
#include "targets/eu4_1_37_5/linux_x86_64/text_3d/text_3d_patch.h"
#include "targets/eu4_1_37_5/linux_x86_64/input/input_patch.h"
#include "targets/eu4_1_37_5/linux_x86_64/save_filenames/save_patch.h"
#include "targets/eu4_1_37_5/linux_x86_64/display_formatting/display_patch.h"
#include "targets/eu4_1_37_5/linux_x86_64/localized_search/search_patch.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace eu4dll::linux_bootstrap {
namespace {

namespace target = eu4dll::targets::eu4_1_37_5::linux_x86_64;

}  // namespace

bool AllowUnsupportedElfOverride() {
    return linux_platform::AllowUnsupportedElf();
}

bool BootstrapLinuxBase(std::string &error, std::string &report) {
    error.clear();
    report.clear();
    std::ostringstream log;

    // 1. Discover host ELF.
    linux_platform::LinuxProcessMemory memory;
    std::string discoverError;
    // Touch discovery through a regions query so failures surface early.
    auto executableRegions = memory.MainModuleRegions(
        patch::RegionPurpose::ExecutableSearch, discoverError);
    if (executableRegions.empty()) {
        error = "ELF discovery failed: " +
                (discoverError.empty() ? std::string("no executable regions")
                                       : discoverError);
        return false;
    }
    const std::string &executablePath = memory.ExecutablePath();
    log << "host=" << executablePath
        << " executable_regions=" << executableRegions.size();

    // 2. Validate exact target (file SHA-256 + ELF/version facts).
    const bool allowOverride = AllowUnsupportedElfOverride();
    auto validation =
        target::ValidateTargetWithFile(memory, executablePath, allowOverride);
    if (!validation) {
        error = "unsupported host ELF: " + target::FormatValidationFailure(validation);
        return false;
    }
    log << " target=" << target::kDiagnosticTargetId
        << " version=" << validation.versionText;

    // 3. Preflight all base sites/resources with zero mutation.
    linux_platform::LinuxNearAllocator allocator;
    {
        const auto preflight = target::base::PreflightBase(memory, &allocator);
        if (!preflight) {
            error = "base preflight failed: " +
                    patch::FormatDiagnostic(preflight.diagnostic);
            return false;
        }
        log << " preflight=" << preflight.diagnostic.message;
        if (allocator.LiveAllocationCount() != 0) {
            error = "base preflight must not retain trampolines";
            return false;
        }
    }

    // 4. Atomically install base. Trampoline pages must outlive this call
    // AND the C++ static-destruction phase at process exit: EU4 global
    // destructors in other DSOs may still call patched functions after this
    // DSO's destructors would run, and ~LinuxNearAllocator munmaps every
    // trampoline. The allocator is therefore intentionally leaked; the OS
    // reclaims the pages with the address space. A future unload/unpatch
    // protocol must unpatch sites and verify restoration before releasing.
    static auto *s_liveAllocator = new linux_platform::LinuxNearAllocator();
    const auto installed = target::base::InstallBase(memory, s_liveAllocator);
    if (!installed) {
        error = "base install failed: " +
                patch::FormatDiagnostic(installed.diagnostic);
        return false;
    }
    log << " installed=" << installed.diagnostic.message
        << " trampolines=" << s_liveAllocator->LiveAllocationCount();

    // 5. Migration-time gates: text layout on explicit opt-in, main text on
    // top of it. Base stays the default so bisection between feature levels
    // is always possible until final integration.
    const char *enableLayout = std::getenv("EU4DLL_ENABLE_TEXT_LAYOUT");
    const bool layoutOn =
        enableLayout != nullptr && std::strcmp(enableLayout, "1") == 0;
    if (layoutOn) {
        const auto layoutPreflight =
            target::layout::PreflightLayout(memory, s_liveAllocator);
        if (!layoutPreflight) {
            error = "textLayout preflight failed: " +
                    patch::FormatDiagnostic(layoutPreflight.diagnostic);
            return false;
        }
        const auto layoutInstalled =
            target::layout::InstallLayout(memory, s_liveAllocator);
        if (!layoutInstalled) {
            error = "textLayout install failed: " +
                    patch::FormatDiagnostic(layoutInstalled.diagnostic);
            return false;
        }
        log << " layout=" << layoutInstalled.diagnostic.message;
    } else {
        log << " layout=disabled";
    }

    const char *enableMainText = std::getenv("EU4DLL_ENABLE_MAIN_TEXT");
    const bool mainTextOn =
        enableMainText != nullptr && std::strcmp(enableMainText, "1") == 0;
    if (mainTextOn) {
        if (!layoutOn) {
            error = "mainText install failed: EU4DLL_ENABLE_MAIN_TEXT=1 "
                    "requires EU4DLL_ENABLE_TEXT_LAYOUT=1";
            return false;
        }
        const auto mainPreflight =
            target::main_text::PreflightMainText(memory, s_liveAllocator);
        if (!mainPreflight) {
            error = "mainText preflight failed: " +
                    patch::FormatDiagnostic(mainPreflight.diagnostic);
            return false;
        }
        const auto mainInstalled =
            target::main_text::InstallMainText(memory, s_liveAllocator);
        if (!mainInstalled) {
            error = "mainText install failed: " +
                    patch::FormatDiagnostic(mainInstalled.diagnostic);
            return false;
        }
        log << " mainText=" << mainInstalled.diagnostic.message;
    } else {
        log << " mainText=disabled";
    }

    const char *enableTooltip = std::getenv("EU4DLL_ENABLE_TOOLTIP_TEXT");
    const bool tooltipOn =
        enableTooltip != nullptr && std::strcmp(enableTooltip, "1") == 0;
    if (tooltipOn) {
        if (!mainTextOn) {
            error = "tooltip install failed: EU4DLL_ENABLE_TOOLTIP_TEXT=1 "
                    "requires EU4DLL_ENABLE_MAIN_TEXT=1";
            return false;
        }
        const auto tooltipPreflight =
            target::tooltip::PreflightTooltip(memory, s_liveAllocator);
        if (!tooltipPreflight) {
            error = "tooltip preflight failed: " +
                    patch::FormatDiagnostic(tooltipPreflight.diagnostic);
            return false;
        }
        const auto tooltipInstalled =
            target::tooltip::InstallTooltip(memory, s_liveAllocator);
        if (!tooltipInstalled) {
            error = "tooltip install failed: " +
                    patch::FormatDiagnostic(tooltipInstalled.diagnostic);
            return false;
        }
        log << " tooltip=" << tooltipInstalled.diagnostic.message;
    } else {
        log << " tooltip=disabled";
    }

    const char *enableLocalization = std::getenv("EU4DLL_ENABLE_LOCALIZATION_UTF8");
    const bool localizationOn =
        enableLocalization != nullptr && std::strcmp(enableLocalization, "1") == 0;
    if (localizationOn) {
        if (!mainTextOn) {
            error = "localization install failed: EU4DLL_ENABLE_LOCALIZATION_UTF8=1 "
                    "requires EU4DLL_ENABLE_MAIN_TEXT=1";
            return false;
        }
        const auto localizationPreflight =
            target::localization_utf8::PreflightLocalization(memory, s_liveAllocator);
        if (!localizationPreflight) {
            error = "localization preflight failed: " +
                    patch::FormatDiagnostic(localizationPreflight.diagnostic);
            return false;
        }
        const auto localizationInstalled =
            target::localization_utf8::InstallLocalization(memory, s_liveAllocator);
        if (!localizationInstalled) {
            error = "localization install failed: " +
                    patch::FormatDiagnostic(localizationInstalled.diagnostic);
            return false;
        }
        log << " localization=" << localizationInstalled.diagnostic.message;
    } else {
        log << " localization=disabled";
    }

    const char *enableMapText = std::getenv("EU4DLL_ENABLE_MAP_TEXT");
    const bool mapTextOn =
        enableMapText != nullptr && std::strcmp(enableMapText, "1") == 0;
    if (mapTextOn) {
        if (!tooltipOn) {
            error = "mapText install failed: EU4DLL_ENABLE_MAP_TEXT=1 "
                    "requires EU4DLL_ENABLE_TOOLTIP_TEXT=1";
            return false;
        }
        const auto mapPreflight =
            target::map_text::PreflightMapText(memory, s_liveAllocator);
        if (!mapPreflight) {
            error = "mapText preflight failed: " +
                    patch::FormatDiagnostic(mapPreflight.diagnostic);
            return false;
        }
        const auto mapInstalled =
            target::map_text::InstallMapText(memory, s_liveAllocator);
        if (!mapInstalled) {
            error = "mapText install failed: " +
                    patch::FormatDiagnostic(mapInstalled.diagnostic);
            return false;
        }
        log << " mapText=" << mapInstalled.diagnostic.message;
    } else {
        log << " mapText=disabled";
    }

    const char *enableText3D = std::getenv("EU4DLL_ENABLE_TEXT3D");
    const bool text3DOn =
        enableText3D != nullptr && std::strcmp(enableText3D, "1") == 0;
    if (text3DOn) {
        if (!mapTextOn) {
            error = "text3D install failed: EU4DLL_ENABLE_TEXT3D=1 "
                    "requires EU4DLL_ENABLE_MAP_TEXT=1";
            return false;
        }
        const auto text3DPreflight =
            target::text_3d::PreflightText3D(memory, s_liveAllocator);
        if (!text3DPreflight) {
            error = "text3D preflight failed: " +
                    patch::FormatDiagnostic(text3DPreflight.diagnostic);
            return false;
        }
        const auto text3DInstalled =
            target::text_3d::InstallText3D(memory, s_liveAllocator);
        if (!text3DInstalled) {
            error = "text3D install failed: " +
                    patch::FormatDiagnostic(text3DInstalled.diagnostic);
            return false;
        }
        log << " text3D=" << text3DInstalled.diagnostic.message;
    } else {
        log << " text3D=disabled";
    }

    const auto enabled = [](const char *name) {
        const char *value = std::getenv(name);
        return value != nullptr && std::strcmp(value, "1") == 0;
    };
    if (enabled("EU4DLL_ENABLE_INPUT_IME")) {
        if (!localizationOn) {
            error = "input requires EU4DLL_ENABLE_LOCALIZATION_UTF8=1";
            return false;
        }
        auto result = target::input::PreflightInput(memory, s_liveAllocator);
        if (result) result = target::input::InstallInput(memory, s_liveAllocator);
        if (!result) { error = patch::FormatDiagnostic(result.diagnostic); return false; }
        log << " input=" << result.diagnostic.message;
    } else log << " input=disabled";
    if (enabled("EU4DLL_ENABLE_CLIPBOARD_PASTE")) {
        if (!enabled("EU4DLL_ENABLE_INPUT_IME")) {
            error = "clipboard requires EU4DLL_ENABLE_INPUT_IME=1";
            return false;
        }
        auto result = target::input::PreflightClipboard(memory, s_liveAllocator);
        if (result) result = target::input::InstallClipboard(memory, s_liveAllocator);
        if (!result) { error = patch::FormatDiagnostic(result.diagnostic); return false; }
        log << " clipboard=" << result.diagnostic.message;
    } else log << " clipboard=disabled";

    if (enabled("EU4DLL_ENABLE_PINYIN_SEARCH")) {
        if (!localizationOn) {
            error = "pinyin search requires EU4DLL_ENABLE_LOCALIZATION_UTF8=1";
            return false;
        }
        auto result = target::search::PreflightSearch(memory, s_liveAllocator);
        if (result) result = target::search::InstallSearch(memory, s_liveAllocator);
        if (!result) { error = patch::FormatDiagnostic(result.diagnostic); return false; }
        log << " search=" << result.diagnostic.message;
    } else log << " search=disabled";

    if (enabled("EU4DLL_ENABLE_SAVE_FILENAME")) {
        if (!localizationOn) {
            error = "save filenames require EU4DLL_ENABLE_LOCALIZATION_UTF8=1";
            return false;
        }
        auto result = target::save_filenames::PreflightSave(memory, s_liveAllocator);
        if (result) result = target::save_filenames::InstallSave(memory, s_liveAllocator);
        if (!result) { error = patch::FormatDiagnostic(result.diagnostic); return false; }
        log << " saveFilenames=" << result.diagnostic.message;
    } else log << " saveFilenames=disabled";

    if (enabled("EU4DLL_ENABLE_DISPLAY_FORMATTING")) {
        if (!localizationOn) {
            error = "display formatting requires EU4DLL_ENABLE_LOCALIZATION_UTF8=1";
            return false;
        }
        auto result = target::display_formatting::PreflightDisplay(memory, s_liveAllocator);
        if (result) result = target::display_formatting::InstallDisplay(memory, s_liveAllocator);
        if (!result) { error = patch::FormatDiagnostic(result.diagnostic); return false; }
        log << " displayFormatting=" << result.diagnostic.message;
    } else log << " displayFormatting=disabled";

    report = log.str();
    return true;
}

}  // namespace eu4dll::linux_bootstrap
