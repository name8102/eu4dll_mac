#pragma once

#include "features/escaped_text/escaped_text.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace eu4dll::text_rendering {

struct Glyph {
    std::uint32_t code = 0;
    std::size_t byteLength = 0;
    escaped_text::SequenceStatus status = escaped_text::SequenceStatus::end;
    bool escaped = false;
};

// Portable rendering policy. Target hooks retain register and stack adaptation,
// but C++ rendering paths use the escaped-text decoder through this surface.
[[nodiscard]] Glyph glyph_at(std::string_view text,
                             std::size_t byteOffset) noexcept;

[[nodiscard]] std::size_t logical_character_count(
        std::string_view text) noexcept;

} // namespace eu4dll::text_rendering
