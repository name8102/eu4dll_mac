#pragma once

#include <string>

namespace eu4dll::linux_bootstrap {

// Testable Linux bootstrap flow without process-wide side effects.
// Always validates the exact target and installs base. Text layout
// installs additionally when EU4DLL_ENABLE_TEXT_LAYOUT=1 (migration-time
// gate; final integration owns the permanent feature set).
// All steps are fail-closed: unknown ELF binaries refuse mutation before any
// write, and every failure produces an actionable diagnostic string.
bool BootstrapLinuxBase(std::string &error, std::string &report);

// Explicit development override query (EU4DLL_ALLOW_UNSUPPORTED_ELF=1).
bool AllowUnsupportedElfOverride();

}  // namespace eu4dll::linux_bootstrap
