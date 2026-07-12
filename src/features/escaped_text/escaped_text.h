#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <limits>
#include <string>
#include <string_view>

namespace eu4dll::escaped_text {

inline constexpr std::uint8_t kEscape1 = 0x10;
inline constexpr std::uint8_t kEscape2 = 0x11;
inline constexpr std::uint8_t kEscape3 = 0x12;
inline constexpr std::uint8_t kEscape4 = 0x13;
inline constexpr std::uint16_t kEscape2Shift = 0x000E;
inline constexpr std::uint16_t kEscape3Shift = 0x0900;
inline constexpr std::uint16_t kEscape4Shift = 0x08F2;
inline constexpr std::uint32_t kUnicodeGlyphOffset = 1712;
inline constexpr std::size_t kLegacyOutputCapacity = 32767;
inline constexpr std::size_t kUnlimitedCapacity =
        std::numeric_limits<std::size_t>::max();

enum class ConversionStatus {
    ok,
    malformed_input,
    truncated_input,
    output_limit_exceeded,
};

struct ConversionResult {
    std::string text;
    ConversionStatus status = ConversionStatus::ok;
    std::size_t input_consumed = 0;
};

inline constexpr std::size_t kFixedInputCapacity = 256;
inline constexpr std::size_t kFixedEscapedCapacity = kFixedInputCapacity * 3;
struct FixedConversionResult {
    std::array<char, kFixedEscapedCapacity> text{};
    std::size_t size = 0;
    ConversionStatus status = ConversionStatus::ok;
    std::size_t input_consumed = 0;
};

[[nodiscard]] FixedConversionResult utf8_to_escaped_fixed(
        std::string_view utf8) noexcept;

// Results own their text. Capacity excludes any trailing NUL a C caller may add.
// Output-limit failures never append a partial UTF-8 or EU4 logical character.
[[nodiscard]] ConversionResult utf8_to_escaped(
        std::string_view utf8,
        std::size_t capacity = kUnlimitedCapacity);

[[nodiscard]] ConversionResult escaped_to_utf8(
        std::string_view escaped,
        std::size_t capacity = kUnlimitedCapacity);

[[nodiscard]] constexpr bool is_escape_marker(std::uint8_t byte) noexcept {
    return byte >= kEscape1 && byte <= kEscape4;
}

enum class SequenceStatus {
    valid,
    malformed,
    truncated,
    end,
};

struct LogicalCharacter {
    std::uint32_t code_point = 0;
    std::uint32_t glyph_code = 0;
    std::size_t byte_offset = 0;
    std::size_t byte_length = 0;
    SequenceStatus status = SequenceStatus::end;
    bool escaped = false;
};

// Malformed or truncated markers are one-byte logical characters. This makes
// every traversal operation progress while preserving the original bytes.
[[nodiscard]] LogicalCharacter character_at(
        std::string_view escaped,
        std::size_t byte_offset) noexcept;

[[nodiscard]] LogicalCharacter character_before(
        std::string_view escaped,
        std::size_t cursor) noexcept;

[[nodiscard]] std::size_t logical_length(std::string_view escaped) noexcept;
[[nodiscard]] std::size_t next_cursor(
        std::string_view escaped,
        std::size_t cursor) noexcept;
[[nodiscard]] std::size_t previous_cursor(
        std::string_view escaped,
        std::size_t cursor) noexcept;

struct EditResult {
    std::size_t cursor = 0;
    std::size_t erased_bytes = 0;
    SequenceStatus status = SequenceStatus::end;
};

// A cursor inside a valid three-byte character is normalized by deleting that
// whole character, so edits can never leave half an escape sequence behind.
[[nodiscard]] EditResult erase_previous(std::string &escaped,
                                        std::size_t cursor);
[[nodiscard]] EditResult erase_next(std::string &escaped,
                                    std::size_t cursor);

}  // namespace eu4dll::escaped_text
