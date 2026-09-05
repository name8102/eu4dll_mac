#include "platform/linux/sdl_input_adapter.h"
#include "features/escaped_text/escaped_text.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>

namespace input = eu4dll::linux_platform::input;
using Event = std::array<unsigned char, 56>;
std::deque<Event> events;
input::Rect rectangle{};
bool started = false;
void Require(bool ok, const char *message) {
    if (!ok) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}
void Start() { Require(rectangle.w == 240 && rectangle.h == 30, "candidate rectangle precedes start"); started = true; }
void Stop() { started = false; }
void SetRect(const input::Rect *rect) { rectangle = *rect; }
int Poll(void *event) {
    if (events.empty()) return 0;
    if (event) { std::memcpy(event, events.front().data(), 56); events.pop_front(); }
    return 1;
}
Event Make(unsigned type, int key = 0) {
    Event event{};
    std::memcpy(event.data(), &type, 4);
    std::memcpy(event.data()+20, &key, 4);
    return event;
}
int main() {
    input::Bind({Start, Stop, SetRect, Poll});
    input::Begin({100, 200, 240, 30});
    Require(started && rectangle.x == 100 && rectangle.y == 200, "focus starts SDL input");
    auto preedit = Make(0x302);
    std::strcpy(reinterpret_cast<char *>(preedit.data()+12), "zhongwen");
    events.push_back(preedit);
    events.push_back(Make(0x300, 8));
    events.push_back(Make(0x300, 13));
    events.push_back(Make(0x300, 1073741904));
    events.push_back(Make(0x303));
    Event event{};
    Require(input::Poll(event.data()) == 1 && events.empty() && input::Composing(),
            "preedit and candidate navigation are consumed before commit");
    Require(input::Commit("中文A") == eu4dll::escaped_text::utf8_to_escaped("中文A").text && !input::Composing(),
            "commit preserves all characters and ends composition");
    events.push_back(preedit);
    auto focusLost = Make(0x200);
    focusLost[12] = 13;
    events.push_back(focusLost);
    Require(input::Poll(event.data()) == 1 && !input::Composing(), "focus loss cancels preedit");
    events.push_back(Make(0x300, 8));
    Require(input::Poll(event.data()) == 1, "ordinary backspace still reaches game");
    input::End();
    Require(!started && !input::Composing(), "end stops SDL input and clears composition");
}
