#pragma once

#include <string>

namespace eu4dll::platform::macos {

void *ResolveLiveSymbol(const char *feature, const char *target, const char *symbol);

} // namespace eu4dll::platform::macos
