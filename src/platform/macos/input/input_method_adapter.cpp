#include "platform/macos/input/input_method_adapter.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace eu4dll::platform::macos::input {
namespace {
struct NativeSnapshot {
    std::array<char, 256> preedit{};
    std::size_t size = 0;
    std::uint64_t generation = 0;
    bool active = false;
};

NativeSnapshot native_snapshot;
struct CommitSnapshot {
    std::array<char, escaped_text::kFixedInputCapacity> utf8{};
    std::size_t utf8_size = 0;
    escaped_text::FixedConversionResult escaped;
};
CommitSnapshot commit_snapshot;
}

void capture_native_editing(const char *preedit_utf8) noexcept {
    std::size_t size = 0;
    if (preedit_utf8 != nullptr) {
        while (size + 1 < native_snapshot.preedit.size() &&
               preedit_utf8[size] != '\0') ++size;
    }
    const bool active = size != 0;
    if (native_snapshot.active == active && native_snapshot.size == size &&
        std::memcmp(native_snapshot.preedit.data(), preedit_utf8 == nullptr ? "" : preedit_utf8,
                    size) == 0) return;
    if (size != 0) std::memcpy(native_snapshot.preedit.data(), preedit_utf8, size);
    native_snapshot.preedit[size] = '\0';
    native_snapshot.size = size;
    native_snapshot.active = active;
    ++native_snapshot.generation;
}

bool native_composition_active() noexcept { return native_snapshot.active; }

FixedCommitView capture_native_commit(const char *committed_utf8) noexcept {
    commit_snapshot.utf8_size = 0;
    bool input_limit_exceeded = false;
    if (committed_utf8 != nullptr) {
        while (commit_snapshot.utf8_size + 1 < commit_snapshot.utf8.size() &&
               committed_utf8[commit_snapshot.utf8_size] != '\0')
            ++commit_snapshot.utf8_size;
        input_limit_exceeded =
                committed_utf8[commit_snapshot.utf8_size] != '\0';
    }
    for (std::size_t index = 0; index < commit_snapshot.utf8_size; ++index)
        commit_snapshot.utf8[index] = committed_utf8[index];
    commit_snapshot.utf8[commit_snapshot.utf8_size] = '\0';
    const std::string_view utf8(commit_snapshot.utf8.data(),
                                commit_snapshot.utf8_size);
    commit_snapshot.escaped = escaped_text::utf8_to_escaped_fixed(utf8);
    if (input_limit_exceeded &&
        commit_snapshot.escaped.status == escaped_text::ConversionStatus::ok) {
        commit_snapshot.escaped.status =
                escaped_text::ConversionStatus::output_limit_exceeded;
    }
    capture_native_editing(nullptr);
    return {utf8,
            {commit_snapshot.escaped.text.data(), commit_snapshot.escaped.size},
            commit_snapshot.escaped.status};
}

text_input::Event InputMethodAdapter::commit(std::string_view committed_utf8) {
    composing_ = false;
    pending_update_ = false;
    observed_generation_ = native_snapshot.generation;
    capture_native_editing(nullptr);
    observed_generation_ = native_snapshot.generation;
    return {text_input::EventType::composition_commit,
            std::string(committed_utf8)};
}

std::optional<text_input::Event> InputMethodAdapter::drain() {
    if (pending_update_) {
        pending_update_ = false;
        return text_input::Event{text_input::EventType::composition_update,
                std::string(drained_preedit_.data(), drained_size_)};
    }
    if (observed_generation_ == native_snapshot.generation) return std::nullopt;
    observed_generation_ = native_snapshot.generation;
    drained_size_ = native_snapshot.size;
    std::copy_n(native_snapshot.preedit.begin(), drained_size_, drained_preedit_.begin());
    if (native_snapshot.active) {
        if (!composing_) {
            composing_ = true;
            pending_update_ = true;
            return text_input::Event{text_input::EventType::composition_start};
        }
        return text_input::Event{text_input::EventType::composition_update,
                std::string(drained_preedit_.data(), drained_size_)};
    }
    if (composing_) {
        composing_ = false;
        return text_input::Event{text_input::EventType::composition_cancel};
    }
    return std::nullopt;
}

text_input::Event InputMethodAdapter::selection(std::size_t anchor,
                                                std::size_t focus) const {
    text_input::Event event{text_input::EventType::selection};
    event.anchor = anchor;
    event.focus = focus;
    return event;
}

text_input::Event InputMethodAdapter::cursor(int logical_delta) const {
    text_input::Event event{text_input::EventType::cursor};
    event.cursor_delta = logical_delta;
    return event;
}

text_input::Event InputMethodAdapter::backspace() const {
    return {text_input::EventType::backspace};
}

}  // namespace eu4dll::platform::macos::input
