#pragma once

#include <cstddef>

namespace eu4dll::features::localization_loading {

void ConvertUtf8ForEu4(const char *utf8, char *output, std::size_t capacity);

} // namespace eu4dll::features::localization_loading
