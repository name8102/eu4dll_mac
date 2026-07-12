#include "runtime/diagnostics/startup_diagnostics.h"

#include <sstream>
#include <utility>

namespace eu4dll::diagnostics {

StartupDiagnostics &StartupDiagnostics::Instance() {
    static StartupDiagnostics diagnostics;
    return diagnostics;
}

void StartupDiagnostics::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    failures_.clear();
}

void StartupDiagnostics::Record(patch::PatchDiagnostic diagnostic) {
    if (diagnostic.success) return;
    std::lock_guard<std::mutex> lock(mutex_);
    failures_.push_back(std::move(diagnostic));
}

void StartupDiagnostics::RecordFailure(std::string feature, std::string target,
                                       patch::PatchOperation operation,
                                       std::string message) {
    patch::PatchDiagnostic diagnostic;
    diagnostic.feature = std::move(feature);
    diagnostic.target = std::move(target);
    diagnostic.operation = operation;
    diagnostic.message = std::move(message);
    Record(std::move(diagnostic));
}

std::size_t StartupDiagnostics::FailureCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return failures_.size();
}

bool StartupDiagnostics::HasFailures() const { return FailureCount() != 0; }

std::string StartupDiagnostics::FormatReport(const std::string &gameVersion) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream report;
    report << "Game Version: " << gameVersion << '\n'
           << "Target: eu4-1.37-macos-x86_64\n\n"
           << failures_.size() << " startup operation(s) failed. "
           << "The affected features were not installed safely:\n";
    for (std::size_t index = 0; index < failures_.size(); ++index) {
        report << "\n" << index + 1 << ". " << patch::FormatDiagnostic(failures_[index]);
    }
    report << "\n\nVerify that the executable is an unmodified macOS x86-64 EU4 1.37.x "
              "binary, restore original game files through your distribution platform, "
              "then retry with the matching eu4dll_mac release.";
    return report.str();
}

InstallGuard::InstallGuard(std::string feature, std::string target)
    : feature_(std::move(feature)), target_(std::move(target)),
      startingFailureCount_(StartupDiagnostics::Instance().FailureCount()) {}

InstallGuard::~InstallGuard() {
    if (success_) return;
    auto &diagnostics = StartupDiagnostics::Instance();
    if (diagnostics.FailureCount() != startingFailureCount_) return;
    diagnostics.RecordFailure(
        std::move(feature_), std::move(target_), patch::PatchOperation::InstallFeature,
        "installer returned without a more specific diagnostic");
}

} // namespace eu4dll::diagnostics
