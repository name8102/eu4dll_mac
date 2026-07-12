#pragma once

#include <cstddef>
#include <string_view>
#include <string>
#include <utility>
#include "features/escaped_text/escaped_text.h"

namespace eu4dll::text_input {

enum class EventType {
    composition_start,
    composition_update,
    composition_commit,
    composition_cancel,
    selection,
    cursor,
    backspace,
};

struct Event {
    Event() = default;
    Event(EventType event_type) : type(event_type) {}
    Event(EventType event_type, std::string text)
        : type(event_type), utf8_text(std::move(text)) {}

    EventType type = EventType::composition_start;
    std::string utf8_text;
    std::size_t anchor = 0;
    std::size_t focus = 0;
    int cursor_delta = 0;
};

struct State {
    std::string escaped_text;
    std::string preedit_utf8;
    std::size_t anchor = 0;
    std::size_t focus = 0;
    bool composing = false;
};

struct Result {
    bool handled = false;
    bool text_changed = false;
    bool composition_changed = false;
    escaped_text::ConversionStatus conversion_status =
            escaped_text::ConversionStatus::ok;
};

struct BufferDecision {
    std::size_t logical_cursor = 0;
    std::size_t byte_count = 0;
    bool handled = false;
};

// Allocation-free decisions used by target adapters. The returned byte_count
// is the number of one-byte EU4 operations required for the logical edit.
[[nodiscard]] BufferDecision decide_backspace(std::string_view escaped_text,
                                              std::size_t cursor) noexcept;
[[nodiscard]] BufferDecision decide_cursor(std::string_view escaped_text,
                                           std::size_t cursor,
                                           int logical_delta) noexcept;

// Selection and cursor positions are byte offsets in escaped_text. They are
// normalized to logical-character boundaries before editing.
[[nodiscard]] Result apply(State &state, const Event &event);

}  // namespace eu4dll::text_input
