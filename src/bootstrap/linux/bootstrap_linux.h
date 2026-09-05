#pragma once

#include <string>

namespace eu4dll::linux_bootstrap {

// Testable Linux bootstrap flow without process-wide side effects.
// Returns true when the supported target validated and base installed.
// All steps are fail-closed: unknown ELF binaries refuse mutation before any
// write, and every failure produces an actionable diagnostic string.
bool BootstrapLinuxBase(std::string &error, std::string &report);

// Explicit development override query (EU4DLL_ALLOW_UNSUPPORTED_ELF=1).
bool AllowUnsupportedElfOverride();

}  // namespace eu4dll::linux_bootstrap
