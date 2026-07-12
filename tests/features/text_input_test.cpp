#include "features/text_input/text_input.h"

#include "features/escaped_text/escaped_text.h"

#include <cstdlib>
#include <iostream>

namespace {
int failures = 0;
void expect(bool value, const char *message) {
    if (!value) { std::cerr << "FAILED: " << message << '\n'; ++failures; }
}
}

int main() {
    using namespace eu4dll;
    text_input::State state;
    auto apply = [&state](const text_input::Event &event) {
        return text_input::apply(state, event);
    };
    apply({text_input::EventType::composition_start});
    apply({text_input::EventType::composition_update, u8"zhong"});
    expect(state.composing && state.preedit_utf8 == "zhong",
           "composition update retains owned preedit text");
    apply({text_input::EventType::composition_commit, u8"中文"});
    expect(!state.composing && state.preedit_utf8.empty(),
           "commit ends composition");
    expect(escaped_text::escaped_to_utf8(state.escaped_text).text == u8"中文",
           "commit stores escaped text");

    text_input::Event left{text_input::EventType::cursor};
    left.cursor_delta = -1;
    apply(left);
    expect(state.focus == 3, "cursor moves by one multi-byte logical character");
    apply({text_input::EventType::backspace});
    expect(escaped_text::escaped_to_utf8(state.escaped_text).text == u8"文" &&
                   state.focus == 0,
           "backspace removes one complete escaped character");

    text_input::Event select{text_input::EventType::selection};
    select.anchor = 0;
    select.focus = state.escaped_text.size();
    apply(select);
    apply({text_input::EventType::composition_commit, u8"国"});
    expect(escaped_text::escaped_to_utf8(state.escaped_text).text == u8"国",
           "commit replaces normalized selection");

    apply({text_input::EventType::composition_start});
    const auto before = state.escaped_text;
    apply({text_input::EventType::backspace});
    expect(state.escaped_text == before, "composition consumes backspace");
    apply({text_input::EventType::composition_cancel});
    expect(!state.composing, "cancel ends composition");

    state = {"A" + escaped_text::utf8_to_escaped(u8"中").text + "B", {}, 1, 1, false};
    const auto inserted = apply({text_input::EventType::composition_commit, u8"文x"});
    expect(escaped_text::escaped_to_utf8(state.escaped_text).text == u8"A文x中B" &&
                   inserted.handled && inserted.text_changed && inserted.composition_changed,
           "mixed ASCII/CJK commit inserts at the logical cursor and reports flags");
    apply(text_input::Event{text_input::EventType::cursor});
    text_input::Event right{text_input::EventType::cursor};
    right.cursor_delta = 1;
    const auto old_cursor = state.focus;
    apply(right);
    expect(state.focus > old_cursor, "right movement advances one logical character");

    text_input::Event out_of_bounds{text_input::EventType::selection};
    out_of_bounds.anchor = 2;
    out_of_bounds.focus = 999;
    apply(out_of_bounds);
    expect(state.anchor == 1 && state.focus == state.escaped_text.size(),
           "selection clamps bounds and normalizes an escape interior");

    state.anchor = state.focus = state.escaped_text.size();
    apply({text_input::EventType::composition_start});
    const auto empty = apply({text_input::EventType::composition_commit, ""});
    expect(empty.handled && !empty.text_changed && empty.composition_changed,
           "empty commit ends composition without changing text");
    apply({text_input::EventType::composition_start});
    const auto malformed = apply({text_input::EventType::composition_commit,
                                  std::string("\xE4\x41\x80", 3)});
    expect(malformed.conversion_status == escaped_text::ConversionStatus::malformed_input,
           "malformed commit reports conversion status");
    const auto fixed = escaped_text::utf8_to_escaped_fixed(u8"A中文€");
    const auto owned = escaped_text::utf8_to_escaped(u8"A中文€");
    expect(std::string_view(fixed.text.data(), fixed.size) == owned.text &&
                   fixed.status == owned.status,
           "fixed commit conversion matches owned portable conversion");

    const auto escaped = escaped_text::utf8_to_escaped(u8"A中B").text;
    const auto before_cjk = text_input::decide_cursor(escaped, escaped.size(), -1);
    const auto cjk_begin = text_input::decide_cursor(
            escaped, before_cjk.logical_cursor, -1);
    expect(cjk_begin.handled && cjk_begin.byte_count == 3,
           "allocation-free left decision spans one escaped CJK character");
    const auto cjk_end = text_input::decide_cursor(escaped, cjk_begin.logical_cursor, 1);
    expect(cjk_end.handled && cjk_end.byte_count == 3,
           "allocation-free right decision spans one escaped CJK character");
    const auto erase = text_input::decide_backspace(escaped, cjk_end.logical_cursor);
    expect(erase.handled && erase.byte_count == 3 &&
                   erase.logical_cursor == cjk_begin.logical_cursor,
           "allocation-free backspace decision identifies escaped byte width");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
