#pragma once

#include "features/text_input/text_input.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace eu4dll::linux_platform::input {

struct Rect { int x, y, w, h; };
struct SdlApi {
    void (*start)() = nullptr;
    void (*stop)() = nullptr;
    void (*set_rect)(const Rect *) = nullptr;
    int (*poll)(void *) = nullptr;
};

void Bind(SdlApi api);
void Begin(Rect rect);
void End();
int Poll(void *event);
std::string Commit(std::string_view utf8);
bool Composing();

} // namespace eu4dll::linux_platform::input
