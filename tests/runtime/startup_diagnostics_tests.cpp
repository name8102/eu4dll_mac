#include "runtime/diagnostics/startup_diagnostics.h"

#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

void TestDetailedFailureSuppressesGenericGuardFailure() {
    auto &diagnostics = eu4dll::diagnostics::StartupDiagnostics::Instance();
    diagnostics.Reset();
    {
        eu4dll::diagnostics::InstallGuard guard("guarded-feature", "test-target");
        diagnostics.RecordFailure("detailed-feature", "test-target",
                                  eu4dll::patch::PatchOperation::LocatePattern,
                                  "pattern was not found");
    }
    Require(diagnostics.FailureCount() == 1,
            "a detailed failure must suppress the guard fallback diagnostic");
    const auto report = diagnostics.FormatReport("EU4 v1.37.5");
    Require(report.find("feature=detailed-feature") != std::string::npos,
            "report must identify the failing feature");
    Require(report.find("operation=locate-pattern") != std::string::npos,
            "report must identify the failed operation");
    Require(report.find("restore original game files through your distribution platform") !=
                std::string::npos,
            "report must contain an actionable storefront-neutral recovery step");
}

void TestGuardFallbackAndSuccess() {
    auto &diagnostics = eu4dll::diagnostics::StartupDiagnostics::Instance();
    diagnostics.Reset();
    {
        eu4dll::diagnostics::InstallGuard guard("fallback-feature", "test-target");
    }
    Require(diagnostics.FailureCount() == 1,
            "an unmarked installer must produce one fallback diagnostic");

    diagnostics.Reset();
    {
        eu4dll::diagnostics::InstallGuard guard("successful-feature", "test-target");
        guard.MarkSuccess();
    }
    Require(!diagnostics.HasFailures(),
            "a successful installer must not produce a diagnostic");
}

} // namespace

int main() {
    TestDetailedFailureSuppressesGenericGuardFailure();
    TestGuardFallbackAndSuccess();
    return 0;
}
