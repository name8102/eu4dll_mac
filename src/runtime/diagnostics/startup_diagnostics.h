#pragma once

#include "runtime/diagnostics/patch_diagnostic.h"

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace eu4dll::diagnostics {

class StartupDiagnostics {
public:
    static StartupDiagnostics &Instance();

    void Reset();
    void Record(patch::PatchDiagnostic diagnostic);
    void RecordFailure(std::string feature, std::string target,
                       patch::PatchOperation operation, std::string message);

    [[nodiscard]] std::size_t FailureCount() const;
    [[nodiscard]] bool HasFailures() const;
    [[nodiscard]] std::string FormatReport(const std::string &gameVersion) const;

private:
    mutable std::mutex mutex_;
    std::vector<patch::PatchDiagnostic> failures_;
};

class InstallGuard {
public:
    InstallGuard(std::string feature, std::string target);
    ~InstallGuard();

    InstallGuard(const InstallGuard &) = delete;
    InstallGuard &operator=(const InstallGuard &) = delete;

    void MarkSuccess() noexcept { success_ = true; }

private:
    std::string feature_;
    std::string target_;
    std::size_t startingFailureCount_ = 0;
    bool success_ = false;
};

} // namespace eu4dll::diagnostics
