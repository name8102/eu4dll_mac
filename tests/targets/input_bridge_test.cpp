#include "targets/eu4_1_37_5/macos_x86_64/input/input_bridge.h"
#include "targets/eu4_1_37_5/macos_x86_64/target_facts.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace bridge = eu4dll::targets::eu4_1_37_5::macos_x86_64::input;
namespace facts = eu4dll::targets::eu4_1_37_5::macos_x86_64::input;

namespace {
std::vector<int> calls;
std::int64_t cursor = 0;
void Require(bool value, const char *message) {
    if (!value) { std::cerr << "FAILED: " << message << '\n'; std::exit(1); }
}
void Action(void *) { calls.push_back(7); }
void Left(void *) { calls.push_back(8); }
void Right(void *) { calls.push_back(9); }
void TextInit(void *, char value) { calls.push_back(100 + static_cast<unsigned char>(value)); }
void InputInit(void *, const void *) { calls.push_back(2); }
std::int64_t Cursor(void *) { return cursor; }
void PreCheck(void *, int event, std::uint32_t, int) { calls.push_back(event); }
void Dispatch(void *, void *) { calls.push_back(4); }

struct FakeBuffer {
    char prefix[0x30];
    std::string text;
};
static_assert(offsetof(FakeBuffer, text) == 0x30);

struct VirtualObject { void **vtable; };

void TestViewAndCallCounts() {
    bridge::bind({TextInit, InputInit, Action, Action, Cursor, Left, Right});
    FakeBuffer buffer{};
    buffer.text = "A\x10\x01\x02" "B";
    cursor = 4;
    const auto view = bridge::view(&buffer);
    Require(view.escaped_text == buffer.text && view.anchor == 4 && view.focus == 4,
            "bridge exposes non-owning CTextBuffer view and cursor");
    calls.clear();
    bridge::backspace_bytes(&buffer, 3);
    Require(calls == std::vector<int>({7, 7, 7}), "backspace invokes exact byte count");
    calls.clear();
    bridge::move_left_bytes(&buffer, 3);
    Require(calls == std::vector<int>({8, 8, 8}), "left invokes exact byte count");
    calls.clear();
    bridge::move_right_bytes(&buffer, 1);
    Require(calls == std::vector<int>({9}), "right invokes exact byte count");
}

void TestByteInjectionOrder() {
    void *keyboard_table[6]{};
    void *handler_table[5]{};
    keyboard_table[5] = reinterpret_cast<void *>(PreCheck);
    handler_table[4] = reinterpret_cast<void *>(Dispatch);
    VirtualObject keyboard{keyboard_table};
    VirtualObject handler{handler_table};
    int text_memory = 0;
    int input_memory = 0;
    calls.clear();
    bridge::inject_bytes(std::string_view("x\0y", 3),
                         {&handler, &keyboard, 1, &text_memory, &input_memory});
    Require(calls == std::vector<int>({facts::kTextInputEvent, 100 + 'x', 2, 4, 7,
                                      facts::kTextInputEvent, 100 + 'y', 2, 4, 7}),
            "bridge performs precheck/init/init/dispatch/destroy per non-NUL byte");
}

void TestFiveHookContracts() {
    Require(facts::kHandlePdxEvents1Original.size() == facts::kJumpOverwriteWidth &&
                    facts::kHandlePdxEvents1.continuationOffset == 0x325,
            "0x303 hook records original bytes, width and continuation");
    Require(facts::kHandleKeyEvent1Original.size() == facts::kJumpOverwriteWidth &&
                    facts::kHandleKeyEvent1.continuationOffset == 6 &&
                    facts::kHandleKeyEvent1.bypassOffset == 0x10,
            "backspace hook records original bytes, width, continuation and bypass");
    Require(facts::kHandlePdxEvents2Original.size() == facts::kJumpOverwriteWidth &&
                    facts::kHandlePdxEvents2.continuationOffset == 5,
            "0x302 hook records original bytes, width and continuation");
    Require(facts::kMoveLeftOriginal.size() == facts::kCallOverwriteWidth &&
                    facts::kMoveLeft.mutationOffset + facts::kCallOverwriteWidth ==
                            facts::kMoveLeft.continuationOffset,
            "left hook records original call bytes, width and continuation");
    Require(facts::kMoveRightOriginal.size() == facts::kCallOverwriteWidth &&
                    facts::kMoveRight.mutationOffset + facts::kCallOverwriteWidth ==
                            facts::kMoveRight.continuationOffset,
            "right hook records original call bytes, width and continuation");
}
}

int main() {
    TestViewAndCallCounts();
    TestByteInjectionOrder();
    TestFiveHookContracts();
}
