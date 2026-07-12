#include "features/escaped_text/escaped_text.h"

#include <algorithm>

namespace eu4dll::escaped_text {
namespace {

constexpr std::uint32_t kReplacement = '?';

bool is_reserved_escape_byte(std::uint8_t byte) noexcept {
    switch (byte) {
        case 0xA4:
        case 0xA3:
        case 0xA7:
        case 0x24:
        case 0x5B:
        case 0x00:
        case 0x5C:
        case 0x20:
        case 0x0D:
        case 0x0A:
        case 0x22:
        case 0x7B:
        case 0x7D:
        case 0x40:
        case 0x80:
        case 0x7E:
        case 0x2F:
        case 0x5F:
        case 0xBD:
        case 0x3B:
        case 0x5D:
        case 0x3D:
        case 0x23:
        case 0x3F:
        case 0x3A:
        case 0x3C:
        case 0x3E:
        case 0x2A:
        case 0x7C:
            return true;
        default:
            return false;
    }
}

std::uint8_t special_unicode_to_windows_1252(std::uint32_t cp) noexcept {
    switch (cp) {
        case 0x20AC: return 0x80;
        case 0x201A: return 0x82;
        case 0x0192: return 0x83;
        case 0x201E: return 0x84;
        case 0x2026: return 0x85;
        case 0x2020: return 0x86;
        case 0x2021: return 0x87;
        case 0x02C6: return 0x88;
        case 0x2030: return 0x89;
        case 0x0160: return 0x8A;
        case 0x2039: return 0x8B;
        case 0x0152: return 0x8C;
        case 0x017D: return 0x8E;
        case 0x2018: return 0x91;
        case 0x2019: return 0x92;
        case 0x201C: return 0x93;
        case 0x201D: return 0x94;
        case 0x2022: return 0x95;
        case 0x2013: return 0x96;
        case 0x2014: return 0x97;
        case 0x02DC: return 0x98;
        case 0x2122: return 0x99;
        case 0x0161: return 0x9A;
        case 0x203A: return 0x9B;
        case 0x0153: return 0x9C;
        case 0x017E: return 0x9E;
        case 0x0178: return 0x9F;
        default: return 0;
    }
}

std::uint32_t windows_1252_to_unicode(std::uint8_t byte) noexcept {
    switch (byte) {
        case 0x80: return 0x20AC;
        case 0x82: return 0x201A;
        case 0x83: return 0x0192;
        case 0x84: return 0x201E;
        case 0x85: return 0x2026;
        case 0x86: return 0x2020;
        case 0x87: return 0x2021;
        case 0x88: return 0x02C6;
        case 0x89: return 0x2030;
        case 0x8A: return 0x0160;
        case 0x8B: return 0x2039;
        case 0x8C: return 0x0152;
        case 0x8E: return 0x017D;
        case 0x91: return 0x2018;
        case 0x92: return 0x2019;
        case 0x93: return 0x201C;
        case 0x94: return 0x201D;
        case 0x95: return 0x2022;
        case 0x96: return 0x2013;
        case 0x97: return 0x2014;
        case 0x98: return 0x02DC;
        case 0x99: return 0x2122;
        case 0x9A: return 0x0161;
        case 0x9B: return 0x203A;
        case 0x9C: return 0x0153;
        case 0x9E: return 0x017E;
        case 0x9F: return 0x0178;
        default: return byte;
    }
}

void note_issue(ConversionResult &result, ConversionStatus issue) noexcept {
    if (result.status == ConversionStatus::ok) {
        result.status = issue;
    }
}

bool append_bytes(ConversionResult &result,
                  std::string_view bytes,
                  std::size_t capacity) {
    if (bytes.size() > capacity - std::min(capacity, result.text.size())) {
        result.status = ConversionStatus::output_limit_exceeded;
        return false;
    }
    result.text.append(bytes);
    return true;
}

std::string utf8_bytes(std::uint32_t cp) {
    std::string result;
    if (cp <= 0x7F) {
        result.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        result.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        result.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return result;
}

struct Utf8Unit {
    std::uint32_t code_point = kReplacement;
    std::size_t length = 1;
    ConversionStatus status = ConversionStatus::ok;
    bool legacy_non_bmp = false;
};

bool continuation(std::uint8_t byte) noexcept {
    return (byte & 0xC0) == 0x80;
}

Utf8Unit decode_utf8(std::string_view text, std::size_t offset) noexcept {
    const auto first = static_cast<std::uint8_t>(text[offset]);
    if (first < 0x80) {
        return {first, 1, ConversionStatus::ok, false};
    }

    std::size_t expected = 0;
    if (first >= 0xC2 && first <= 0xDF) {
        expected = 2;
    } else if (first >= 0xE0 && first <= 0xEF) {
        expected = 3;
    } else if (first >= 0xF0 && first <= 0xF4) {
        expected = 4;
    } else {
        return {kReplacement, 1, ConversionStatus::malformed_input, false};
    }

    if (text.size() - offset < expected) {
        return {kReplacement, 1, ConversionStatus::truncated_input, false};
    }
    for (std::size_t index = 1; index < expected; ++index) {
        if (!continuation(static_cast<std::uint8_t>(text[offset + index]))) {
            return {kReplacement, 1, ConversionStatus::malformed_input, false};
        }
    }

    std::uint32_t cp = first & ((1U << (7 - expected)) - 1);
    for (std::size_t index = 1; index < expected; ++index) {
        cp = (cp << 6) | (static_cast<std::uint8_t>(text[offset + index]) & 0x3F);
    }
    if ((expected == 3 && cp < 0x800) ||
        (expected == 4 && cp < 0x10000) ||
        (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) {
        return {kReplacement, 1, ConversionStatus::malformed_input, false};
    }
    return {cp, expected, ConversionStatus::ok, expected == 4};
}

std::uint8_t marker_for(std::uint8_t low, std::uint8_t high) noexcept {
    return static_cast<std::uint8_t>(kEscape1 +
            (is_reserved_escape_byte(high) ? 2 : 0) +
            (is_reserved_escape_byte(low) ? 1 : 0));
}

struct EscapedUnit {
    std::uint16_t code_point = 0;
    std::uint32_t glyph_code = 0;
    std::size_t length = 1;
    SequenceStatus status = SequenceStatus::valid;
    bool escaped = false;
};

EscapedUnit decode_escaped(std::string_view text, std::size_t offset) noexcept {
    const auto marker = static_cast<std::uint8_t>(text[offset]);
    if (!is_escape_marker(marker)) {
        const auto cp = windows_1252_to_unicode(marker);
        return {static_cast<std::uint16_t>(cp), marker, 1,
                SequenceStatus::valid, false};
    }
    if (text.size() - offset < 3) {
        return {static_cast<std::uint16_t>(kReplacement), marker, 1,
                SequenceStatus::truncated, false};
    }

    auto low = static_cast<std::uint8_t>(text[offset + 1]);
    auto high = static_cast<std::uint8_t>(text[offset + 2]);
    switch (marker) {
        case kEscape2: low = static_cast<std::uint8_t>(low - 14); break;
        case kEscape3: high = static_cast<std::uint8_t>(high + 9); break;
        case kEscape4:
            low = static_cast<std::uint8_t>(low - 14);
            high = static_cast<std::uint8_t>(high + 9);
            break;
        default: break;
    }
    auto cp = static_cast<std::uint16_t>((high << 8) | low);
    auto logical_cp = cp;
    if (logical_cp > 0xE100 && logical_cp < 0xEA00) {
        logical_cp = static_cast<std::uint16_t>(logical_cp - 0xE000);
    }
    const auto expected_marker = marker_for(low, high);
    const bool surrogate = logical_cp >= 0xD800 && logical_cp <= 0xDFFF;
    const auto status = expected_marker == marker && !surrogate
            ? SequenceStatus::valid
            : SequenceStatus::malformed;
    const auto glyph = static_cast<std::uint32_t>(cp) +
            (cp >= 256 ? kUnicodeGlyphOffset : 0);
    return {static_cast<std::uint16_t>(surrogate ? kReplacement : logical_cp),
            glyph, 3, status, true};
}

}  // namespace

ConversionResult utf8_to_escaped(std::string_view utf8, std::size_t capacity) {
    ConversionResult result;
    result.text.reserve(std::min(utf8.size(), capacity));
    for (std::size_t offset = 0; offset < utf8.size();) {
        const auto unit = decode_utf8(utf8, offset);
        if (unit.status != ConversionStatus::ok) {
            note_issue(result, unit.status);
        }

        std::string encoded;
        if (unit.legacy_non_bmp) {
            encoded.assign(unit.length, '?');
        } else if (unit.code_point <= 0xFF &&
                   !(unit.code_point >= 0x80 && unit.code_point <= 0x9F)) {
            encoded.push_back(static_cast<char>(unit.code_point));
        } else if (const auto mapped =
                           special_unicode_to_windows_1252(unit.code_point)) {
            encoded.push_back(static_cast<char>(mapped));
        } else {
            auto cp = static_cast<std::uint16_t>(unit.code_point & 0xFFFF);
            if (cp > 0x100 && cp < 0xA00) {
                cp = static_cast<std::uint16_t>(cp + 0xE000);
            }
            auto low = static_cast<std::uint8_t>(cp & 0xFF);
            auto high = static_cast<std::uint8_t>(cp >> 8);
            if (high == 0) {
                encoded.push_back(static_cast<char>(low));
            } else {
                const auto marker = marker_for(low, high);
                if (marker == kEscape2 || marker == kEscape4) {
                    low = static_cast<std::uint8_t>(low + 14);
                }
                if (marker == kEscape3 || marker == kEscape4) {
                    high = static_cast<std::uint8_t>(high - 9);
                }
                encoded.push_back(static_cast<char>(marker));
                encoded.push_back(static_cast<char>(low));
                encoded.push_back(static_cast<char>(high));
            }
        }

        if (!append_bytes(result, encoded, capacity)) {
            result.input_consumed = offset;
            return result;
        }
        offset += unit.length;
        result.input_consumed = offset;
    }
    return result;
}

FixedConversionResult utf8_to_escaped_fixed(std::string_view utf8) noexcept {
    FixedConversionResult result;
    const auto append = [&result](const char *bytes, std::size_t count) noexcept {
        if (count > result.text.size() - result.size) {
            result.status = ConversionStatus::output_limit_exceeded;
            return false;
        }
        for (std::size_t index = 0; index < count; ++index)
            result.text[result.size++] = bytes[index];
        return true;
    };
    for (std::size_t offset = 0; offset < utf8.size();) {
        const auto unit = decode_utf8(utf8, offset);
        if (unit.status != ConversionStatus::ok &&
            result.status == ConversionStatus::ok) result.status = unit.status;
        char encoded[4]{};
        std::size_t encoded_size = 0;
        if (unit.legacy_non_bmp) {
            encoded_size = unit.length;
            for (std::size_t index = 0; index < encoded_size; ++index)
                encoded[index] = '?';
        } else if (unit.code_point <= 0xFF &&
                   !(unit.code_point >= 0x80 && unit.code_point <= 0x9F)) {
            encoded[encoded_size++] = static_cast<char>(unit.code_point);
        } else if (const auto mapped = special_unicode_to_windows_1252(unit.code_point)) {
            encoded[encoded_size++] = static_cast<char>(mapped);
        } else {
            auto cp = static_cast<std::uint16_t>(unit.code_point & 0xFFFF);
            if (cp > 0x100 && cp < 0xA00)
                cp = static_cast<std::uint16_t>(cp + 0xE000);
            auto low = static_cast<std::uint8_t>(cp & 0xFF);
            auto high = static_cast<std::uint8_t>(cp >> 8);
            if (high == 0) {
                encoded[encoded_size++] = static_cast<char>(low);
            } else {
                const auto marker = marker_for(low, high);
                if (marker == kEscape2 || marker == kEscape4) low += 14;
                if (marker == kEscape3 || marker == kEscape4) high -= 9;
                encoded[encoded_size++] = static_cast<char>(marker);
                encoded[encoded_size++] = static_cast<char>(low);
                encoded[encoded_size++] = static_cast<char>(high);
            }
        }
        if (!append(encoded, encoded_size)) {
            result.input_consumed = offset;
            return result;
        }
        offset += unit.length;
        result.input_consumed = offset;
    }
    return result;
}

ConversionResult escaped_to_utf8(std::string_view escaped,
                                 std::size_t capacity) {
    ConversionResult result;
    result.text.reserve(std::min(escaped.size(), capacity));
    for (std::size_t offset = 0; offset < escaped.size();) {
        const auto unit = decode_escaped(escaped, offset);
        std::string encoded;
        if (unit.status == SequenceStatus::truncated) {
            note_issue(result, ConversionStatus::truncated_input);
            encoded.push_back('?');
        } else if (unit.status == SequenceStatus::malformed) {
            note_issue(result, ConversionStatus::malformed_input);
            encoded = utf8_bytes(unit.code_point);
        } else {
            encoded = utf8_bytes(unit.code_point);
        }
        if (!append_bytes(result, encoded, capacity)) {
            result.input_consumed = offset;
            return result;
        }
        offset += unit.length;
        result.input_consumed = offset;
    }
    return result;
}

LogicalCharacter character_at(std::string_view escaped,
                              std::size_t byte_offset) noexcept {
    if (byte_offset >= escaped.size()) {
        return {0, 0, escaped.size(), 0, SequenceStatus::end, false};
    }
    const auto unit = decode_escaped(escaped, byte_offset);
    if (unit.status == SequenceStatus::malformed ||
        unit.status == SequenceStatus::truncated) {
        const auto byte = static_cast<std::uint8_t>(escaped[byte_offset]);
        return {kReplacement, byte, byte_offset, 1, unit.status, false};
    }
    return {unit.code_point, unit.glyph_code, byte_offset, unit.length,
            unit.status, unit.escaped};
}

LogicalCharacter character_before(std::string_view escaped,
                                  std::size_t cursor) noexcept {
    const auto start = previous_cursor(escaped, cursor);
    if (start == std::min(cursor, escaped.size()) && start == 0) {
        return {0, 0, 0, 0, SequenceStatus::end, false};
    }
    return character_at(escaped, start);
}

std::size_t logical_length(std::string_view escaped) noexcept {
    std::size_t count = 0;
    for (std::size_t offset = 0; offset < escaped.size();) {
        const auto character = character_at(escaped, offset);
        offset += character.byte_length;
        ++count;
    }
    return count;
}

std::size_t next_cursor(std::string_view escaped, std::size_t cursor) noexcept {
    cursor = std::min(cursor, escaped.size());
    for (std::size_t offset = 0; offset < escaped.size();) {
        const auto character = character_at(escaped, offset);
        const auto end = offset + character.byte_length;
        if (cursor <= offset || cursor < end) {
            return end;
        }
        offset = end;
    }
    return escaped.size();
}

std::size_t previous_cursor(std::string_view escaped,
                            std::size_t cursor) noexcept {
    cursor = std::min(cursor, escaped.size());
    std::size_t previous = 0;
    for (std::size_t offset = 0; offset < escaped.size();) {
        const auto character = character_at(escaped, offset);
        const auto end = offset + character.byte_length;
        if (cursor <= offset) {
            return previous;
        }
        if (cursor <= end) {
            return offset;
        }
        previous = offset;
        offset = end;
    }
    return previous;
}

EditResult erase_previous(std::string &escaped, std::size_t cursor) {
    cursor = std::min(cursor, escaped.size());
    if (cursor == 0) {
        return {0, 0, SequenceStatus::end};
    }
    const auto start = previous_cursor(escaped, cursor);
    const auto unit = character_at(escaped, start);
    const auto erased = unit.byte_length;
    escaped.erase(start, erased);
    return {start, erased, unit.status};
}

EditResult erase_next(std::string &escaped, std::size_t cursor) {
    cursor = std::min(cursor, escaped.size());
    if (cursor == escaped.size()) {
        return {cursor, 0, SequenceStatus::end};
    }
    std::size_t start = cursor;
    for (std::size_t offset = 0; offset < escaped.size();) {
        const auto unit = character_at(escaped, offset);
        const auto end = offset + unit.byte_length;
        if (cursor < end) {
            start = offset;
            break;
        }
        offset = end;
    }
    const auto unit = character_at(escaped, start);
    escaped.erase(start, unit.byte_length);
    return {start, unit.byte_length, unit.status};
}

}  // namespace eu4dll::escaped_text
