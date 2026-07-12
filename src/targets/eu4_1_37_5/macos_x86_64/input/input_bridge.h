#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::input {

struct BufferView {
    std::string_view escaped_text;
    std::size_t anchor = 0;
    std::size_t focus = 0;
};

struct EventInjectionContext {
    void *event_handler = nullptr;
    void *keyboard = nullptr;
    std::uint32_t timestamp = 0;
    void *text_input_event_memory = nullptr;
    void *input_event_memory = nullptr;
};

struct Api {
    using InstanceAction = void (*)(void *);
    using TextInputInit = void (*)(void *, char);
    using InputInit = void (*)(void *, const void *);
    using CursorPosition = std::int64_t (*)(void *);

    TextInputInit text_input_init = nullptr;
    InputInit input_init = nullptr;
    InstanceAction input_destroy = nullptr;
    InstanceAction backspace = nullptr;
    CursorPosition cursor_position = nullptr;
    InstanceAction move_left = nullptr;
    InstanceAction move_right = nullptr;
};

void bind(Api api) noexcept;
[[nodiscard]] BufferView view(void *text_buffer) noexcept;
void inject_bytes(std::string_view escaped_bytes,
                  const EventInjectionContext &context) noexcept;
void backspace_bytes(void *text_buffer, std::size_t count) noexcept;
void move_left_bytes(void *text_buffer, std::size_t count) noexcept;
void move_right_bytes(void *text_buffer, std::size_t count) noexcept;

}  // namespace eu4dll::targets::eu4_1_37_5::macos_x86_64::input
