#include "features/escaped_text/escaped_text.h"
#include "features/text_rendering/text_rendering.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << std::endl;
        std::exit(1);
    }
}

} // namespace

int main() {
    const auto encoded = eu4dll::escaped_text::utf8_to_escaped("A中B");
    require(encoded.status == eu4dll::escaped_text::ConversionStatus::ok,
            "fixture conversion failed");
    require(eu4dll::text_rendering::logical_character_count(encoded.text) == 3,
            "rendering count must use escaped-text logical characters");

    const auto ascii = eu4dll::text_rendering::glyph_at(encoded.text, 0);
    require(ascii.code == static_cast<unsigned char>('A') &&
                    ascii.byteLength == 1 && !ascii.escaped,
            "ASCII glyph decoding changed");

    const auto wide = eu4dll::text_rendering::glyph_at(encoded.text, 1);
    require(wide.code == 0x4E2D + eu4dll::escaped_text::kUnicodeGlyphOffset &&
                    wide.byteLength == 3 && wide.escaped,
            "wide glyph decoding changed");

    const std::string truncated{
        static_cast<char>(eu4dll::escaped_text::kEscape1)};
    const auto malformed = eu4dll::text_rendering::glyph_at(truncated, 0);
    require(malformed.status == eu4dll::escaped_text::SequenceStatus::truncated &&
                    malformed.byteLength == 1,
            "malformed rendering traversal must always progress");
    return 0;
}
