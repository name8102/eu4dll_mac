#include "platform/linux/sdl_input_adapter.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace eu4dll::linux_platform::input {
namespace {
SdlApi api;
// SDL input runs on the main event thread. Function-local lifetime avoids
// dynamic initialization order with the preload constructor.
text_input::State &Composition() {
    static auto *state = new text_input::State;
    return *state;
}
template<class T> T Read(const void *event, std::size_t offset) {
    T result;
    std::memcpy(&result, static_cast<const char *>(event) + offset, sizeof(T));
    return result;
}
void Cancel() {
    (void)text_input::apply(Composition(), {text_input::EventType::composition_cancel});
}
} // namespace

void Bind(SdlApi value) { api = value; }
void Begin(Rect rect) {
    Cancel();
    api.set_rect(&rect);
    api.start();
    if (std::getenv("EU4DLL_TRACE_INPUT"))
        std::fprintf(stderr, "eu4dll input begin rect=%d,%d,%d,%d\n", rect.x, rect.y, rect.w, rect.h);
}
void End() {
    Cancel();
    api.stop();
    if (std::getenv("EU4DLL_TRACE_INPUT")) std::fprintf(stderr, "eu4dll input end\n");
}
bool Composing() { return Composition().composing; }

int Poll(void *event) {
    while (const int available = api.poll(event)) {
        if (event == nullptr) return available;
        const auto type = Read<std::uint32_t>(event, 0);
        if ((type == 0x302 || type == 0x303) && std::getenv("EU4DLL_TRACE_INPUT"))
            std::fprintf(stderr, "eu4dll input event=%x bytes=%zu\n", type,
                strnlen(static_cast<const char *>(event) + 12, 32));
        if (type == 0x302) { // SDL_TEXTEDITING: UTF-8 at +12, 32-byte capacity
            const auto *text = static_cast<const char *>(event) + 12;
            if (*text == 0) Cancel();
            else (void)text_input::apply(Composition(),
                {text_input::EventType::composition_update,
                 std::string(text, strnlen(text, 32))});
            continue; // pre-edit belongs to the native candidate UI
        }
        if (type == 0x200 && Read<std::uint8_t>(event, 12) == 13) {
            Cancel(); // SDL_WINDOWEVENT_FOCUS_LOST
        }
        // Candidate navigation must not move/delete committed game text or
        // close a dialog while the input method still owns the keystroke.
        if (type == 0x300 && Composing()) {
            const auto key = Read<std::int32_t>(event, 20);
            if (key == 8 || key == 13 || key == 27 || key == 127 ||
                (key >= 1073741903 && key <= 1073741906)) continue;
        }
        return available;
    }
    return 0;
}

std::string Commit(std::string_view utf8) {
    auto &state = Composition();
    state.escaped_text.clear();
    state.anchor = state.focus = 0;
    (void)text_input::apply(state,
        {text_input::EventType::composition_commit, std::string(utf8)});
    return std::move(state.escaped_text);
}

} // namespace eu4dll::linux_platform::input
