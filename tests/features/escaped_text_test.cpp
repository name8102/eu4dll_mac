#include "features/escaped_text/escaped_text.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace escaped_text = eu4dll::escaped_text;

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

std::string bytes(std::initializer_list<unsigned int> values) {
    std::string result;
    for (const auto value : values) {
        result.push_back(static_cast<char>(value));
    }
    return result;
}

void conversion_round_trips() {
    const std::string original = u8"ASCII café €—ŒŸ 中文 ā";
    const auto escaped = escaped_text::utf8_to_escaped(original);
    expect(escaped.status == escaped_text::ConversionStatus::ok,
           "valid mixed UTF-8 converts successfully");
    expect(escaped.input_consumed == original.size(),
           "successful encode consumes the input");

    const auto restored = escaped_text::escaped_to_utf8(escaped.text);
    expect(restored.status == escaped_text::ConversionStatus::ok,
           "valid escaped text decodes successfully");
    expect(restored.text == original, "mixed text round-trips");
    expect(restored.input_consumed == escaped.text.size(),
           "successful decode consumes the input");
}

void windows_1252_and_escape_markers() {
    const auto special = escaped_text::utf8_to_escaped(u8"€—ŒŸ");
    expect(special.text == bytes({0x80, 0x97, 0x8C, 0x9F}),
           "Windows-1252 punctuation uses its single-byte mapping");
    expect(escaped_text::escaped_to_utf8(special.text).text == u8"€—ŒŸ",
           "Windows-1252 punctuation decodes to Unicode");

    const auto marker1 = escaped_text::utf8_to_escaped(u8"中");
    const auto marker2 = escaped_text::utf8_to_escaped(u8"丠");  // U+4E20
    const auto marker3 = escaped_text::utf8_to_escaped(u8" ");  // U+2001
    const auto marker4 = escaped_text::utf8_to_escaped(u8"․");  // U+2024
    expect(marker1.text == bytes({0x10, 0x2D, 0x4E}),
           "ordinary escaped code point uses marker 1");
    expect(marker2.text == bytes({0x11, 0x2E, 0x4E}),
           "reserved low byte uses marker 2 and low-byte shift");
    expect(marker3.text == bytes({0x12, 0x01, 0x17}),
           "reserved high byte uses marker 3 and high-byte shift");
    expect(marker4.text == bytes({0x13, 0x32, 0x17}),
           "two reserved bytes use marker 4 and both shifts");
    expect(escaped_text::escaped_to_utf8(marker4.text).text == u8"․",
           "marker 4 reverses both shifts");

    const auto emoji = escaped_text::utf8_to_escaped(u8"😀");
    expect(emoji.status == escaped_text::ConversionStatus::ok,
           "valid non-BMP input is not malformed");
    expect(emoji.text == "????",
           "non-BMP input keeps the legacy four-question-mark fallback");
}

void malformed_and_truncated_input() {
    const auto malformed_utf8 =
            escaped_text::utf8_to_escaped(bytes({0xE4, 0x41, 0x80}));
    expect(malformed_utf8.status ==
                   escaped_text::ConversionStatus::malformed_input,
           "invalid UTF-8 continuation is reported");
    expect(malformed_utf8.text == "?A?",
           "each malformed UTF-8 byte has deterministic replacement");

    const auto truncated_utf8 =
            escaped_text::utf8_to_escaped(bytes({0xE4, 0xB8}));
    expect(truncated_utf8.status ==
                   escaped_text::ConversionStatus::truncated_input,
           "truncated UTF-8 is distinguished from malformed input");
    expect(truncated_utf8.text == "??",
           "truncated UTF-8 always makes progress");

    const auto truncated_escape =
            escaped_text::escaped_to_utf8(bytes({0x10, 0x41}));
    expect(truncated_escape.status ==
                   escaped_text::ConversionStatus::truncated_input,
           "truncated EU4 escape sequence is reported");
    expect(truncated_escape.text == "?A",
           "truncated escape marker is replaced and trailing byte preserved");

    const auto malformed_escape =
            escaped_text::escaped_to_utf8(bytes({0x10, 0x20, 0x4E}));
    expect(malformed_escape.status ==
                   escaped_text::ConversionStatus::malformed_input,
           "escape marker inconsistent with reserved bytes is reported");
    expect(malformed_escape.text == u8"丠",
           "malformed but complete legacy escape remains decodable");

    const auto surrogate_escape =
            escaped_text::escaped_to_utf8(bytes({0x11, 0x0E, 0xD8}));
    expect(surrogate_escape.status ==
                   escaped_text::ConversionStatus::malformed_input,
           "escaped UTF-16 surrogate is reported as malformed");
    expect(surrogate_escape.text == "?",
           "escaped UTF-16 surrogate cannot create invalid UTF-8");
}

void capacity_is_atomic() {
    const auto encode = escaped_text::utf8_to_escaped(u8"A中", 3);
    expect(encode.status ==
                   escaped_text::ConversionStatus::output_limit_exceeded,
           "escaped output capacity failure is explicit");
    expect(encode.text == "A", "capacity never writes a partial escape");
    expect(encode.input_consumed == 1,
           "capacity reports input before the blocked logical character");

    const auto chinese = escaped_text::utf8_to_escaped(u8"中").text;
    const auto decode = escaped_text::escaped_to_utf8(chinese, 2);
    expect(decode.status ==
                   escaped_text::ConversionStatus::output_limit_exceeded,
           "UTF-8 output capacity failure is explicit");
    expect(decode.text.empty(), "capacity never writes partial UTF-8");
    expect(decode.input_consumed == 0,
           "blocked decode consumes no escape bytes");

    const auto exact = escaped_text::escaped_to_utf8(chinese, 3);
    expect(exact.status == escaped_text::ConversionStatus::ok &&
                   exact.text == u8"中",
           "exact output capacity succeeds");
}

void traversal_cursor_deletion_and_glyphs() {
    const auto chinese = escaped_text::utf8_to_escaped(u8"中").text;
    const std::string text = "A" + chinese + "B";
    expect(escaped_text::logical_length(text) == 3,
           "logical length counts a three-byte escape once");
    expect(escaped_text::next_cursor(text, 0) == 1,
           "cursor moves over ASCII");
    expect(escaped_text::next_cursor(text, 1) == 4,
           "cursor moves over a complete escape");
    expect(escaped_text::next_cursor(text, 2) == 4,
           "cursor inside an escape normalizes to its end");
    expect(escaped_text::previous_cursor(text, 4) == 1,
           "cursor moves left over a complete escape");
    expect(escaped_text::previous_cursor(text, 3) == 1,
           "cursor inside an escape normalizes to its start");

    const auto character = escaped_text::character_at(text, 1);
    expect(character.status == escaped_text::SequenceStatus::valid &&
                   character.escaped && character.byte_length == 3,
           "traversal exposes a valid escaped character");
    expect(character.code_point == 0x4E2D,
           "traversal exposes the Unicode code point");
    expect(character.glyph_code == 0x4E2D +
                   escaped_text::kUnicodeGlyphOffset,
           "glyph mapping applies the EU4 Unicode table offset");

    std::string backspace = text;
    const auto backspace_result = escaped_text::erase_previous(backspace, 4);
    expect(backspace == "AB" && backspace_result.cursor == 1 &&
                   backspace_result.erased_bytes == 3,
           "backspace removes one complete multi-byte character");

    std::string delete_key = text;
    const auto delete_result = escaped_text::erase_next(delete_key, 2);
    expect(delete_key == "AB" && delete_result.cursor == 1 &&
                   delete_result.erased_bytes == 3,
           "delete inside an escape removes the complete character");

    const auto truncated = bytes({0x10, 0x41});
    const auto first = escaped_text::character_at(truncated, 0);
    expect(first.status == escaped_text::SequenceStatus::truncated &&
                   first.byte_length == 1 &&
                   escaped_text::next_cursor(truncated, 0) == 1,
           "truncated traversal consumes one byte and cannot loop forever");

    const auto malformed = bytes({0x10, 0x20, 0x4E});
    const auto malformed_first = escaped_text::character_at(malformed, 0);
    expect(malformed_first.status == escaped_text::SequenceStatus::malformed &&
                   !malformed_first.escaped &&
                   malformed_first.byte_length == 1 &&
                   escaped_text::logical_length(malformed) == 3,
           "malformed traversal preserves every original byte");

    std::string malformed_delete = malformed;
    const auto malformed_delete_result =
            escaped_text::erase_next(malformed_delete, 0);
    expect(malformed_delete == bytes({0x20, 0x4E}) &&
                   malformed_delete_result.erased_bytes == 1,
           "deleting malformed input cannot consume adjacent bytes");

    const std::string embedded_nul("A\0B", 3);
    const auto nul_escaped = escaped_text::utf8_to_escaped(embedded_nul);
    const auto nul_restored = escaped_text::escaped_to_utf8(nul_escaped.text);
    expect(nul_escaped.text == embedded_nul &&
                   nul_restored.text == embedded_nul,
           "owned conversions preserve embedded NUL bytes");

    std::string boundary = text;
    expect(escaped_text::erase_previous(boundary, 0).erased_bytes == 0 &&
                   escaped_text::erase_next(boundary, boundary.size()).erased_bytes == 0,
           "deletion at either boundary is a no-op");
}

}  // namespace

int main() {
    conversion_round_trips();
    windows_1252_and_escape_markers();
    malformed_and_truncated_input();
    capacity_is_atomic();
    traversal_cursor_deletion_and_glyphs();
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
