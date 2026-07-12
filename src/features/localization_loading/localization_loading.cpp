#include "localization_loading.h"

#include "features/escaped_text/escaped_text.h"

#include <algorithm>
#include <cstring>

namespace eu4dll::features::localization_loading {

void ConvertUtf8ForEu4(const char *utf8, char *output, std::size_t capacity) {
    if (output == nullptr || capacity == 0) return;
    const auto converted = escaped_text::utf8_to_escaped(utf8 == nullptr ? "" : utf8,
                                                          capacity - 1);
    const auto count = std::min(converted.text.size(), capacity - 1);
    std::memcpy(output, converted.text.data(), count);
    output[count] = '\0';
}

} // namespace eu4dll::features::localization_loading
