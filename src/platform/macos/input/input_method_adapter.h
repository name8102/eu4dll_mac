#pragma once

#include "features/text_input/text_input.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace eu4dll::platform::macos::input {

// Owns the state derived from the macOS/SDL native event stream. Native SDL
// event objects remain at the capture site and are reduced to owned values.
class InputMethodAdapter {
public:
    [[nodiscard]] text_input::Event commit(std::string_view committed_utf8);
    [[nodiscard]] text_input::Event selection(std::size_t anchor,
                                              std::size_t focus) const;
    [[nodiscard]] text_input::Event cursor(int logical_delta) const;
    [[nodiscard]] text_input::Event backspace() const;
    [[nodiscard]] std::optional<text_input::Event> drain();

    [[nodiscard]] bool is_composing() const noexcept { return composing_; }

private:
    bool composing_ = false;
    bool pending_update_ = false;
    std::uint64_t observed_generation_ = 0;
    std::array<char, 256> drained_preedit_{};
    std::size_t drained_size_ = 0;
};

struct FixedCommitView {
    std::string_view utf8;
    std::string_view escaped;
    escaped_text::ConversionStatus status = escaped_text::ConversionStatus::ok;
};

// Called directly by the 0x302 target hook. This is the only owner/write path
// for native active/preedit state and is fixed-capacity, POD, noexcept and
// allocation-free. Oversized UTF-8 is truncated at a code-unit boundary.
void capture_native_editing(const char *preedit_utf8) noexcept;
[[nodiscard]] bool native_composition_active() noexcept;
[[nodiscard]] FixedCommitView capture_native_commit(
        const char *committed_utf8) noexcept;

}  // namespace eu4dll::platform::macos::input
