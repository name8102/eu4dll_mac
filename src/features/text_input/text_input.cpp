#include "features/text_input/text_input.h"

#include "features/escaped_text/escaped_text.h"

#include <algorithm>

namespace eu4dll::text_input {
namespace {

std::size_t boundary(std::string_view text, std::size_t position) {
    position = std::min(position, text.size());
    if (position == 0 || position == text.size()) return position;
    const auto before = escaped_text::previous_cursor(text, position);
    const auto after = escaped_text::next_cursor(text, before);
    return position == after ? position : before;
}

void normalize_selection(State &state) {
    state.anchor = boundary(state.escaped_text, state.anchor);
    state.focus = boundary(state.escaped_text, state.focus);
}

bool erase_selection(State &state) {
    normalize_selection(state);
    const auto first = std::min(state.anchor, state.focus);
    const auto last = std::max(state.anchor, state.focus);
    if (first == last) return false;
    state.escaped_text.erase(first, last - first);
    state.anchor = state.focus = first;
    return true;
}

}  // namespace

BufferDecision decide_backspace(std::string_view text,
                                std::size_t cursor) noexcept {
    cursor = boundary(text, cursor);
    if (cursor == 0) return {0, 0, false};
    const auto previous = escaped_text::previous_cursor(text, cursor);
    return {previous, cursor - previous, true};
}

BufferDecision decide_cursor(std::string_view text, std::size_t cursor,
                             int logical_delta) noexcept {
    cursor = boundary(text, cursor);
    auto next = cursor;
    if (logical_delta < 0) {
        for (int count = 0; count > logical_delta; --count)
            next = escaped_text::previous_cursor(text, next);
    } else {
        for (int count = 0; count < logical_delta; ++count)
            next = escaped_text::next_cursor(text, next);
    }
    const auto bytes = next > cursor ? next - cursor : cursor - next;
    return {next, bytes, bytes != 0};
}

Result apply(State &state, const Event &event) {
    Result result;
    switch (event.type) {
        case EventType::composition_start:
            state.composing = true;
            state.preedit_utf8.clear();
            result.handled = result.composition_changed = true;
            break;
        case EventType::composition_update:
            state.composing = true;
            state.preedit_utf8 = event.utf8_text;
            result.handled = result.composition_changed = true;
            break;
        case EventType::composition_commit: {
            const auto converted = escaped_text::utf8_to_escaped(event.utf8_text);
            result.conversion_status = converted.status;
            result.text_changed = erase_selection(state);
            state.escaped_text.insert(state.focus, converted.text);
            state.focus += converted.text.size();
            state.anchor = state.focus;
            state.preedit_utf8.clear();
            state.composing = false;
            result.handled = true;
            result.text_changed = result.text_changed || !converted.text.empty();
            result.composition_changed = true;
            break;
        }
        case EventType::composition_cancel:
            state.preedit_utf8.clear();
            state.composing = false;
            result.handled = result.composition_changed = true;
            break;
        case EventType::selection:
            state.anchor = event.anchor;
            state.focus = event.focus;
            normalize_selection(state);
            result.handled = true;
            break;
        case EventType::cursor: {
            normalize_selection(state);
            std::size_t cursor = state.focus;
            if (event.cursor_delta < 0) {
                for (int count = 0; count > event.cursor_delta; --count)
                    cursor = escaped_text::previous_cursor(state.escaped_text, cursor);
            } else {
                for (int count = 0; count < event.cursor_delta; ++count)
                    cursor = escaped_text::next_cursor(state.escaped_text, cursor);
            }
            state.anchor = state.focus = cursor;
            result.handled = true;
            break;
        }
        case EventType::backspace:
            result.handled = true;
            if (state.composing) break;
            result.text_changed = erase_selection(state);
            if (!result.text_changed) {
                normalize_selection(state);
                const auto edit = escaped_text::erase_previous(
                        state.escaped_text, state.focus);
                state.anchor = state.focus = edit.cursor;
                result.text_changed = edit.erased_bytes != 0;
            }
            break;
    }
    return result;
}

}  // namespace eu4dll::text_input
