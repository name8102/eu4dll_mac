#include "features/text_rendering/text_rendering.h"

namespace eu4dll::text_rendering {

Glyph glyph_at(std::string_view text, std::size_t byteOffset) noexcept {
    const auto character = escaped_text::character_at(text, byteOffset);
    return {character.glyph_code, character.byte_length, character.status,
            character.escaped};
}

std::size_t logical_character_count(std::string_view text) noexcept {
    return escaped_text::logical_length(text);
}

} // namespace eu4dll::text_rendering
