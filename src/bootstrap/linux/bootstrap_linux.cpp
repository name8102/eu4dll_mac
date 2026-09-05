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
#include "targets/eu4_1_37_5/linux_x86_64/target_facts.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace eu4dll::linux_bootstrap {
namespace {

namespace target = eu4dll::targets::eu4_1_37_5::linux_x86_64;

void Log(const char *message) {
    std::fprintf(stderr, "eu4dll_linux [bootstrap] %s\n", message);
}

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

    report = log.str();
    return true;
}

}  // namespace eu4dll::linux_bootstrap
