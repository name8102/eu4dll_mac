#pragma once

#include <string>

namespace eu4dll::features::save_filenames {

// Use only when the hook contract explicitly transfers/mutates its argument.
std::string &ToDiskNameInPlace(std::string &escapedName);
std::string &ToDisplayNameInPlace(std::string &utf8Name);

// Display-only hooks must put this returned value in caller-owned storage.
// The UTF-8 source is never mutated and every call owns an independent value.
std::string DisplayCopy(const std::string &utf8Name);

// Appends one converted, function-local display copy to an already-live
// destination. Its existing prefix and lifetime are preserved.
void AppendDisplayCopy(std::string &destination, const std::string &utf8Name);

// Target-hook ABI adapter for an uninitialized caller-owned std::string slot.
void ConstructDisplayCopy(const std::string *utf8Name, void *outputStorage);
void DestroyDisplayCopy(void *outputStorage);

} // namespace eu4dll::features::save_filenames
