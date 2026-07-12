#include "platform/macos/input/input_method_adapter.h"

#include <cstdlib>
#include <string>

namespace {
void expect(bool condition) { if (!condition) std::exit(EXIT_FAILURE); }
}

int main() {
    using eu4dll::platform::macos::input::InputMethodAdapter;
    using eu4dll::text_input::EventType;
    InputMethodAdapter adapter;
    using eu4dll::platform::macos::input::capture_native_editing;
    capture_native_editing("ni");
    auto event = adapter.drain();
    expect(event && event->type == EventType::composition_start);
    event = adapter.drain();
    expect(event && event->type == EventType::composition_update && event->utf8_text == "ni");
    expect(!adapter.drain() && adapter.is_composing());

    std::string native = "nih";
    capture_native_editing(native.c_str());
    event = adapter.drain();
    native[0] = 'X';
    expect(event && event->type == EventType::composition_update && event->utf8_text == "nih");
    expect(!adapter.drain());

    const auto committed = adapter.commit(u8"你");
    expect(committed.type == EventType::composition_commit && !adapter.is_composing());
    expect(!adapter.drain());
    capture_native_editing("again");
    const auto fixed_commit =
            eu4dll::platform::macos::input::capture_native_commit(u8"中文");
    expect(fixed_commit.utf8 == u8"中文" && !fixed_commit.escaped.empty() &&
           !eu4dll::platform::macos::input::native_composition_active());
    const std::string oversized(
            eu4dll::escaped_text::kFixedInputCapacity + 16, 'a');
    const auto limited_commit =
            eu4dll::platform::macos::input::capture_native_commit(
                    oversized.c_str());
    expect(limited_commit.utf8.size() ==
                   eu4dll::escaped_text::kFixedInputCapacity - 1 &&
           limited_commit.status ==
                   eu4dll::escaped_text::ConversionStatus::output_limit_exceeded);
    const auto malformed_commit =
            eu4dll::platform::macos::input::capture_native_commit(
                    "\xE4\x41\x80");
    expect(malformed_commit.status ==
           eu4dll::escaped_text::ConversionStatus::malformed_input);

    capture_native_editing("x");
    expect(adapter.drain()->type == EventType::composition_start);
    expect(adapter.drain()->type == EventType::composition_update);
    capture_native_editing("");
    event = adapter.drain();
    expect(event && event->type == EventType::composition_cancel);
    expect(!adapter.drain() && !adapter.is_composing());

    expect(adapter.selection(2, 5).focus == 5);
    expect(adapter.cursor(-1).cursor_delta == -1);
    expect(adapter.backspace().type == EventType::backspace);
    return EXIT_SUCCESS;
}
