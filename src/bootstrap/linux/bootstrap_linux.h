#pragma once

#include <string>

namespace eu4dll::linux_bootstrap {

// Testable Linux bootstrap flow without process-wide side effects.
// Always validates the exact target and installs base. Text layout
// installs additionally when EU4DLL_ENABLE_TEXT_LAYOUT=1, main text
// when EU4DLL_ENABLE_MAIN_TEXT=1 (requiring the layout gate), tooltip
// when EU4DLL_ENABLE_TOOLTIP_TEXT=1 (requiring the main-text gate), and
// UTF-8 localization when EU4DLL_ENABLE_LOCALIZATION_UTF8=1 (requiring
// the main-text gate), and map text when EU4DLL_ENABLE_MAP_TEXT=1
// (requiring the tooltip gate; clusters install sequentially).
// These are migration-time gates; final integration owns the permanent
// feature set.
// All steps are fail-closed: unknown ELF binaries refuse mutation before any
// write, and every failure produces an actionable diagnostic string.
bool BootstrapLinuxBase(std::string &error, std::string &report);

// Explicit development override query (EU4DLL_ALLOW_UNSUPPORTED_ELF=1).
bool AllowUnsupportedElfOverride();

}  // namespace eu4dll::linux_bootstrap
