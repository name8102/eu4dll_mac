#include "bootstrap/linux/bootstrap_linux.h"

#include "platform/linux/linux_elf_identity.h"
#include "platform/linux/linux_executable_allocator.h"
#include "platform/linux/linux_process_memory.h"
#include "runtime/patch/patch_batch.h"
#include "targets/eu4_1_37_5/linux_x86_64/base/base_patch.h"
#include "targets/eu4_1_37_5/linux_x86_64/profile.h"
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

    // 4. Atomically install base. Trampoline pages must outlive this call,
    // so the install uses a process-lifetime allocator whose destructor never
    // runs before exit.
    static linux_platform::LinuxNearAllocator s_liveAllocator;
    const auto installed = target::base::InstallBase(memory, &s_liveAllocator);
    if (!installed) {
        error = "base install failed: " +
                patch::FormatDiagnostic(installed.diagnostic);
        return false;
    }
    log << " installed=" << installed.diagnostic.message
        << " trampolines=" << s_liveAllocator.LiveAllocationCount();

    report = log.str();
    return true;
}

}  // namespace eu4dll::linux_bootstrap
