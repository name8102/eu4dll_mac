#include "targets/eu4_1_37_5/macos_x86_64/input/input_bridge.h"

#include "targets/eu4_1_37_5/macos_x86_64/target_facts.h"

#include <string>

namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::input {
namespace {
Api bound_api;
}

void bind(Api api) noexcept { bound_api = api; }

BufferView view(void *text_buffer) noexcept {
    if (text_buffer == nullptr || bound_api.cursor_position == nullptr) return {};
    const auto *text = reinterpret_cast<const std::string *>(
            reinterpret_cast<std::uintptr_t>(text_buffer) +
            eu4dll::targets::eu4_1_37_5::macos_x86_64::input::kTextBufferStringOffset);
    const auto raw_cursor = bound_api.cursor_position(text_buffer);
    const auto cursor = raw_cursor < 0 ? 0U :
            static_cast<std::size_t>(raw_cursor);
    return {*text, cursor, cursor};
}

void inject_bytes(std::string_view bytes,
                  const EventInjectionContext &context) noexcept {
    using namespace eu4dll::targets::eu4_1_37_5::macos_x86_64;
    if (context.keyboard == nullptr || context.event_handler == nullptr) return;
    void **keyboard_vtable = *reinterpret_cast<void ***>(context.keyboard);
    using PreCheck = void (*)(void *, int, std::uint32_t, int);
    auto pre_check = reinterpret_cast<PreCheck>(
            keyboard_vtable[kKeyboardPreCheckVtableOffset / sizeof(void *)]);
    void **handler_vtable = *reinterpret_cast<void ***>(context.event_handler);
    using Dispatch = void (*)(void *, void *);
    auto dispatch = reinterpret_cast<Dispatch>(
            handler_vtable[kEventHandlerDispatchVtableOffset / sizeof(void *)]);
    for (const char byte : bytes) {
        if (byte == '\0') continue;
        pre_check(context.keyboard, kTextInputEvent, context.timestamp, 0);
        bound_api.text_input_init(context.text_input_event_memory, byte);
        bound_api.input_init(context.input_event_memory,
                             context.text_input_event_memory);
        dispatch(context.event_handler, context.input_event_memory);
        bound_api.input_destroy(context.input_event_memory);
    }
}

void backspace_bytes(void *buffer, std::size_t count) noexcept {
    while (count-- != 0) bound_api.backspace(buffer);
}
void move_left_bytes(void *buffer, std::size_t count) noexcept {
    while (count-- != 0) bound_api.move_left(buffer);
}
void move_right_bytes(void *buffer, std::size_t count) noexcept {
    while (count-- != 0) bound_api.move_right(buffer);
}

}  // namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::input
