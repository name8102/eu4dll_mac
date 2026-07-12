
#include "input.h"
#include "features/escaped_text/escaped_text.h"
#include "features/text_input/text_input.h"
#include "platform/macos/input/input_method_adapter.h"
#include "platform/macos/live_patch_runtime.h"
#include "platform/macos/symbol_lookup.h"
#include "runtime/diagnostics/patch_diagnostic.h"
#include "runtime/diagnostics/startup_diagnostics.h"
#include "targets/eu4_1_37_5/macos_x86_64/input/input_bridge.h"
#include "targets/eu4_1_37_5/macos_x86_64/target_facts.h"

#include <initializer_list>
#include <iostream>
#include <vector>

extern "C" void eu4dll_capture_native_editing(const char *preedit) noexcept {
    eu4dll::platform::macos::input::capture_native_editing(preedit);
}

namespace input {
    namespace target = eu4dll::targets::eu4_1_37_5::macos_x86_64;
    namespace macos_input = eu4dll::platform::macos::input;
    namespace portable_input = eu4dll::text_input;
    namespace target_input = target::input;
    extern "C" {
    bool isFirstEmpty = false;
    bool g_BackspaceHandled = false;
    uintptr_t g_HandlePdxEvents_1_RetAddr = 0;
    uintptr_t g_HandleKeyEvent_1_RetAddr = 0;
    uintptr_t g_HandleKeyEvent_1_BypassAddr = 0;
    uintptr_t g_HandlePdxEvents_2_RetAddr = 0;
    void *g_InputUtf8ToEscapedStr_Addr = nullptr;
    typedef void (*fnInstanceAction_t)(void *pThis);
    typedef void (*fnCTextInputEvent_init_t)(void *pThis, char c);
    typedef void (*fnCInputEvent_init_t)(void *pThis, const void *pTextInputEvent);
    typedef int64_t (*fnCTextBuffer_GetCursorPositionInString_t)(void *pThis);
    fnCTextInputEvent_init_t fnCTextInputEvent_initCall;
    fnCInputEvent_init_t fnCInputEvent_initCall;
    fnInstanceAction_t fnCInputEvent_DestructorCall;
    fnInstanceAction_t fnCTextBuffer_EnterBackspaceCall;
    fnCTextBuffer_GetCursorPositionInString_t fnCTextBuffer_GetCursorPositionInStringCall;
    fnInstanceAction_t fnCTextBuffer_MoveLeftCall;
    fnInstanceAction_t fnCTextBuffer_MoveRightCall;
    }

    namespace {
    struct ContinuationBinding {
        const char *name;
        std::ptrdiff_t offset;
        uintptr_t *storage;
    };

    bool InstallInputPatch(const char *feature, const target::HookSite &site,
                           eu4dll::patch::MutationKind mutationKind,
                           uintptr_t mutationTarget,
                           std::vector<std::uint8_t> expectedBytes,
                           std::initializer_list<ContinuationBinding> continuations = {},
                           bool optimize = false,
                           eu4dll::patch::CallWidth callWidth =
                               eu4dll::patch::CallWidth::Auto) {
        eu4dll::patch::PatchDescription patch;
        patch.feature = feature;
        patch.target = target::kDiagnosticTargetId;
        patch.location.pattern = site.pattern;
        patch.expected = eu4dll::patch::ExpectedBytes{site.mutationOffset,
                                                       std::move(expectedBytes), {}};
        patch.mutation.kind = mutationKind;
        patch.mutation.offset = site.mutationOffset;
        patch.mutation.target = mutationTarget;
        patch.mutation.callWidth = callWidth;
        for (const auto &continuation : continuations) {
            patch.continuations.push_back({continuation.name, continuation.offset});
        }
        patch.optimization.enabled = optimize;
        patch.optimization.hookAddress = mutationTarget;

        const auto result = eu4dll::platform::macos::LivePatchRuntime().Install(patch);
        for (const auto &continuation : continuations) {
            if (continuation.storage != nullptr) {
                *continuation.storage = result.ContinuationAddress(continuation.name);
            }
        }
        if (!result) {
            eu4dll::diagnostics::StartupDiagnostics::Instance().Record(result.diagnostic);
            std::cerr << "eu4dll_mac [Error] "
                      << eu4dll::patch::FormatDiagnostic(result.diagnostic) << std::endl;
            return false;
        }
        std::cout << "eu4dll_mac [Success] " << feature
                  << " match=0x" << std::hex << result.diagnostic.matchAddress
                  << " mutation=0x" << result.diagnostic.mutationAddress
                  << std::dec << std::endl;
        return true;
    }

    template<typename Function>
    Function ResolveInputSymbol(const char *feature, const char *symbol) {
        return reinterpret_cast<Function>(eu4dll::platform::macos::ResolveLiveSymbol(
            feature, target::kDiagnosticTargetId, symbol));
    }
    } // namespace

    void inputUtf8ToEscapedStr(const char *utf8_text,          // rdi: SDL text buffer [rbp-0x5C]
                               void *pEventHandler,     // rsi: CPdxEventHandler 对象指针 [rbp-0x70]
                               void *pKeyBoard,         // rdx: CKeyBoard 对象指针 (r15)
                               uint32_t timestamp,       // rcx: 事件时间戳 [rbp-0x54]
                               void *pTextInputEventMem, // r8 : 供 CTextInputEvent 存放的栈内存 (r12)
                               void *pInputEventMem      // r9 : 供 CInputEvent 存放的栈内存 [rbp-0xE8]
    ) noexcept {
        const auto commit = macos_input::capture_native_commit(utf8_text);
        target_input::inject_bytes(commit.escaped,
                {pEventHandler, pKeyBoard, timestamp, pTextInputEventMem,
                 pInputEventMem});
    }


    __attribute__((naked)) void naked_CSdlEvents_HandlePdxEvents_1() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"

                "lea rdi, [rbp - 0x5C]\n"   // arg0: utf8_text 字符串首地址
                "mov rsi, [rbp - 0x70]\n"   // arg1: pEventHandler
                "mov rdx, r15\n"            // arg2: CKeyBoard* (原代码在 1014080F9 赋值给了r15)
                "mov ecx, [rbp - 0x54]\n"   // arg3: timestamp
                "mov r8, r12\n"             // arg4: CTextInputEvent 的内存地址 (原代码 r12=[rbp-108h])
                "lea r9, [rbp - 0xE8]\n"    // arg5: CInputEvent 的内存地址

                "push rax\n push rcx\n push rdx\n push rsi\n push rdi\n"
                "push r8\n push r9\n push r10\n push r11\n"
                "push rbp\n mov rbp, rsp\n and rsp, -16\n sub rsp, 256\n"
                "movdqu [rsp+0], xmm0\n movdqu [rsp+16], xmm1\n"
                "movdqu [rsp+32], xmm2\n movdqu [rsp+48], xmm3\n"
                "movdqu [rsp+64], xmm4\n movdqu [rsp+80], xmm5\n"
                "movdqu [rsp+96], xmm6\n movdqu [rsp+112], xmm7\n"
                "movdqu [rsp+128], xmm8\n movdqu [rsp+144], xmm9\n"
                "movdqu [rsp+160], xmm10\n movdqu [rsp+176], xmm11\n"
                "movdqu [rsp+192], xmm12\n movdqu [rsp+208], xmm13\n"
                "movdqu [rsp+224], xmm14\n movdqu [rsp+240], xmm15\n"

                // 调用我们的 C++ 处理逻辑
                "call [rip + _g_InputUtf8ToEscapedStr_Addr] \n"

                "movdqu xmm0, [rsp+0]\n movdqu xmm1, [rsp+16]\n"
                "movdqu xmm2, [rsp+32]\n movdqu xmm3, [rsp+48]\n"
                "movdqu xmm4, [rsp+64]\n movdqu xmm5, [rsp+80]\n"
                "movdqu xmm6, [rsp+96]\n movdqu xmm7, [rsp+112]\n"
                "movdqu xmm8, [rsp+128]\n movdqu xmm9, [rsp+144]\n"
                "movdqu xmm10, [rsp+160]\n movdqu xmm11, [rsp+176]\n"
                "movdqu xmm12, [rsp+192]\n movdqu xmm13, [rsp+208]\n"
                "movdqu xmm14, [rsp+224]\n movdqu xmm15, [rsp+240]\n"
                "mov rsp, rbp\n pop rbp\n"
                "pop r11\n pop r10\n pop r9\n pop r8\n pop rdi\n"
                "pop rsi\n pop rdx\n pop rcx\n pop rax\n"

                "jmp [rip + _g_HandlePdxEvents_1_RetAddr] \n"
                ".att_syntax prefix \n"
                );
    }

/**
 Hook函数：CSdlEvents::HandlePdxEvents 0x303
 作用：拦截输入法输入完成事件，使其能读取全部输入文本并转换为游戏内能显示的逃逸文本
 */
    void install_CSdlEvents_HandlePdxEvents_1() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto &site = target::input::kHandlePdxEvents1;
        g_InputUtf8ToEscapedStr_Addr = (void *) inputUtf8ToEscapedStr;
        if (!InstallInputPatch(
                "input.handle-pdx-events.commit", site,
                eu4dll::patch::MutationKind::Jump,
                reinterpret_cast<uintptr_t>(naked_CSdlEvents_HandlePdxEvents_1),
                {target::input::kHandlePdxEvents1Original.begin(),
                 target::input::kHandlePdxEvents1Original.end()},
                {{"return", site.continuationOffset, &g_HandlePdxEvents_1_RetAddr}},
                true)) return;
        installGuard.MarkSuccess();
    }

    void proxy_CTextBuffer_HandleKeyEvent_HandleBackspace_1(void *pTextBuffer) noexcept {
        if (macos_input::native_composition_active()) {
            isFirstEmpty = true;
            g_BackspaceHandled = true;
            return;
        } else if (isFirstEmpty) {
            isFirstEmpty = false;
            g_BackspaceHandled = true;
            return;
        }
        const auto buffer = target_input::view(pTextBuffer);
        const auto decision = portable_input::decide_backspace(
                buffer.escaped_text, buffer.focus);
        if (!decision.handled || decision.byte_count == 1) {
            g_BackspaceHandled = false;
            return;
        }
        target_input::backspace_bytes(pTextBuffer, decision.byte_count);
        g_BackspaceHandled = true;
    }

    extern "C" void *g_HandleKeyEvent_HandleBackspace_1_Addr = (void *) proxy_CTextBuffer_HandleKeyEvent_HandleBackspace_1;


    __attribute__((naked)) void naked_CTextBuffer_HandleKeyEvent_1() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"

                "push rax\n push rcx\n push rdx\n push rsi\n push rdi\n"
                "push r8\n push r9\n push r10\n push r11\n"
                "push rbp\n mov rbp, rsp\n and rsp, -16\n sub rsp, 256\n"
                "movdqu [rsp+0], xmm0\n movdqu [rsp+16], xmm1\n"
                "movdqu [rsp+32], xmm2\n movdqu [rsp+48], xmm3\n"
                "movdqu [rsp+64], xmm4\n movdqu [rsp+80], xmm5\n"
                "movdqu [rsp+96], xmm6\n movdqu [rsp+112], xmm7\n"
                "movdqu [rsp+128], xmm8\n movdqu [rsp+144], xmm9\n"
                "movdqu [rsp+160], xmm10\n movdqu [rsp+176], xmm11\n"
                "movdqu [rsp+192], xmm12\n movdqu [rsp+208], xmm13\n"
                "movdqu [rsp+224], xmm14\n movdqu [rsp+240], xmm15\n"

                // r15 存放的是 CTextBuffer 的 this 指针，作为参数传给我们的 C++ 函数
                "mov rdi, r15\n"
                "call [rip + _g_HandleKeyEvent_HandleBackspace_1_Addr]\n"

                "movdqu xmm0, [rsp+0]\n movdqu xmm1, [rsp+16]\n"
                "movdqu xmm2, [rsp+32]\n movdqu xmm3, [rsp+48]\n"
                "movdqu xmm4, [rsp+64]\n movdqu xmm5, [rsp+80]\n"
                "movdqu xmm6, [rsp+96]\n movdqu xmm7, [rsp+112]\n"
                "movdqu xmm8, [rsp+128]\n movdqu xmm9, [rsp+144]\n"
                "movdqu xmm10, [rsp+160]\n movdqu xmm11, [rsp+176]\n"
                "movdqu xmm12, [rsp+192]\n movdqu xmm13, [rsp+208]\n"
                "movdqu xmm14, [rsp+224]\n movdqu xmm15, [rsp+240]\n"
                "mov rsp, rbp\n pop rbp\n"
                "pop r11\n pop r10\n pop r9\n pop r8\n pop rdi\n"
                "pop rsi\n pop rdx\n pop rcx\n pop rax\n"

                // 检查 C++ 函数的返回值 (存放在 AL 寄存器)
                "cmp byte ptr [rip + _g_BackspaceHandled], 0\n"
                "jnz 1f\n" // 如果返回 true (拦截)，跳到下面标签 1

                // ===== 分支 A：不拦截，执行原游戏正常退格逻辑 =====
                // 恢复被我们 Hook 覆盖掉的指令
                "mov rax, [r15]\n"
                "mov rdi, r15\n"
                // 跳回原程序继续执行
                "jmp [rip + _g_HandleKeyEvent_1_RetAddr] \n"
                // ===== 分支 B：已拦截，跳过游戏退格逻辑直接退出 =====
                "1:\n"
                // 观察游戏原函数的出口 loc_1014FFC8F，它通过 r13b 传递 handled(1) 标志
                // 我们直接跳转到其函数收尾部分，完美结束这次事件处理
                "jmp [rip + _g_HandleKeyEvent_1_BypassAddr] \n"
                ".att_syntax prefix \n"
                );
    }

/**
 Hook函数：CTextBuffer::HandleKeyEvent
 作用：处理退格键删除逃逸文本时的逻辑，使其能正确删除
 */
    void install_CTextBuffer_HandleKeyEvent_1() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto &site = target::input::kHandleKeyEvent1;
        if (!InstallInputPatch(
                "input.handle-key-event.backspace", site,
                eu4dll::patch::MutationKind::Jump,
                reinterpret_cast<uintptr_t>(naked_CTextBuffer_HandleKeyEvent_1),
                {target::input::kHandleKeyEvent1Original.begin(),
                 target::input::kHandleKeyEvent1Original.end()},
                {{"return", site.continuationOffset, &g_HandleKeyEvent_1_RetAddr},
                 {"bypass", site.bypassOffset, &g_HandleKeyEvent_1_BypassAddr}},
                true)) return;
        installGuard.MarkSuccess();
    }


    __attribute__((naked)) void naked_CSdlEvents_HandlePdxEvents_2() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"
                "cmp eax, %c[editing] \n"
                "jz 1f \n"
                "jmp 2f \n"
                "1: \n"
                "push rax\n push rcx\n push rdx\n push rsi\n push rdi\n"
                "push r8\n push r9\n push r10\n push r11\n"
                "lea rdi, [rbp-0x5C]\n"
                "push rbp\n mov rbp, rsp\n and rsp, -16\n"
                "sub rsp, 256\n"
                "movdqu [rsp+0], xmm0\n movdqu [rsp+16], xmm1\n"
                "movdqu [rsp+32], xmm2\n movdqu [rsp+48], xmm3\n"
                "movdqu [rsp+64], xmm4\n movdqu [rsp+80], xmm5\n"
                "movdqu [rsp+96], xmm6\n movdqu [rsp+112], xmm7\n"
                "movdqu [rsp+128], xmm8\n movdqu [rsp+144], xmm9\n"
                "movdqu [rsp+160], xmm10\n movdqu [rsp+176], xmm11\n"
                "movdqu [rsp+192], xmm12\n movdqu [rsp+208], xmm13\n"
                "movdqu [rsp+224], xmm14\n movdqu [rsp+240], xmm15\n"
                "call _eu4dll_capture_native_editing\n"
                "movdqu xmm0, [rsp+0]\n movdqu xmm1, [rsp+16]\n"
                "movdqu xmm2, [rsp+32]\n movdqu xmm3, [rsp+48]\n"
                "movdqu xmm4, [rsp+64]\n movdqu xmm5, [rsp+80]\n"
                "movdqu xmm6, [rsp+96]\n movdqu xmm7, [rsp+112]\n"
                "movdqu xmm8, [rsp+128]\n movdqu xmm9, [rsp+144]\n"
                "movdqu xmm10, [rsp+160]\n movdqu xmm11, [rsp+176]\n"
                "movdqu xmm12, [rsp+192]\n movdqu xmm13, [rsp+208]\n"
                "movdqu xmm14, [rsp+224]\n movdqu xmm15, [rsp+240]\n"
                "mov rsp, rbp\n pop rbp\n"
                "pop r11\n pop r10\n pop r9\n pop r8\n pop rdi\n"
                "pop rsi\n pop rdx\n pop rcx\n pop rax\n"
                "2: \n"
                "cmp eax, %c[keydown] \n"
                "jmp [rip + _g_HandlePdxEvents_2_RetAddr] \n"
                ".att_syntax prefix \n"
                :
                : [editing] "i"(target::input::kTextEditingEvent),
                  [keydown] "i"(target::input::kKeyDownEvent)
                );
    }

/**
 Hook函数：CSdlEvents::HandlePdxEvents 0x302
 作用：检查输入法是否正在输入，用于拦截输入时的退格键
 */
    void install_CSdlEvents_HandlePdxEvents_2() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto &site = target::input::kHandlePdxEvents2;
        if (!InstallInputPatch(
                "input.handle-pdx-events.editing", site,
                eu4dll::patch::MutationKind::Jump,
                reinterpret_cast<uintptr_t>(naked_CSdlEvents_HandlePdxEvents_2),
                {target::input::kHandlePdxEvents2Original.begin(),
                 target::input::kHandlePdxEvents2Original.end()},
                {{"return", site.continuationOffset, &g_HandlePdxEvents_2_RetAddr}},
                true)) return;
        installGuard.MarkSuccess();
    }

    void proxy_CTextBuffer_HandleKeyEvent_MoveLeft(void *pTextBuffer) {
        const auto buffer = target_input::view(pTextBuffer);
        const auto decision = portable_input::decide_cursor(
                buffer.escaped_text, buffer.focus, -1);
        target_input::move_left_bytes(pTextBuffer,
                decision.handled ? decision.byte_count : 1);
    }

/**
 Hook函数：CTextBuffer::HandleKeyEvent
 作用：按下向左方向键时的光标处理
 */
    void install_CTextBuffer_HandleKeyEvent_2() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto &site = target::input::kMoveLeft;
        if (!InstallInputPatch(
                "input.handle-key-event.move-left", site,
                eu4dll::patch::MutationKind::Call,
                reinterpret_cast<uintptr_t>(proxy_CTextBuffer_HandleKeyEvent_MoveLeft),
                {target::input::kMoveLeftOriginal.begin(),
                 target::input::kMoveLeftOriginal.end()}, {}, false,
                eu4dll::patch::CallWidth::SixBytes)) return;
        installGuard.MarkSuccess();
    }

    void proxy_CTextBuffer_HandleKeyEvent_MoveRight(void *pTextBuffer) {
        const auto buffer = target_input::view(pTextBuffer);
        const auto decision = portable_input::decide_cursor(
                buffer.escaped_text, buffer.focus, 1);
        target_input::move_right_bytes(pTextBuffer,
                decision.handled ? decision.byte_count : 1);
    }

/**
 Hook函数：CTextBuffer::HandleKeyEvent
 作用：按下向右方向键时的光标处理
 */
    void install_CTextBuffer_HandleKeyEvent_3() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto &site = target::input::kMoveRight;
        if (!InstallInputPatch(
                "input.handle-key-event.move-right", site,
                eu4dll::patch::MutationKind::Call,
                reinterpret_cast<uintptr_t>(proxy_CTextBuffer_HandleKeyEvent_MoveRight),
                {target::input::kMoveRightOriginal.begin(),
                 target::input::kMoveRightOriginal.end()}, {}, false,
                eu4dll::patch::CallWidth::SixBytes)) return;
        installGuard.MarkSuccess();
    }


    void install() {
        fnCTextInputEvent_initCall = ResolveInputSymbol<fnCTextInputEvent_init_t>(
            "input.symbol.text-input-event-constructor",
            target::symbols::kTextInputEventConstructor);
        fnCInputEvent_initCall = ResolveInputSymbol<fnCInputEvent_init_t>(
            "input.symbol.input-event-constructor",
            target::symbols::kInputEventConstructor);
        fnCInputEvent_DestructorCall = ResolveInputSymbol<fnInstanceAction_t>(
            "input.symbol.input-event-destructor",
            target::symbols::kInputEventDestructor);
        fnCTextBuffer_EnterBackspaceCall = ResolveInputSymbol<fnInstanceAction_t>(
            "input.symbol.text-buffer-backspace",
            target::symbols::kTextBufferEnterBackspace);
        fnCTextBuffer_GetCursorPositionInStringCall =
            ResolveInputSymbol<fnCTextBuffer_GetCursorPositionInString_t>(
                "input.symbol.text-buffer-cursor-position",
                target::symbols::kTextBufferCursorPosition);
        fnCTextBuffer_MoveLeftCall = ResolveInputSymbol<fnInstanceAction_t>(
            "input.symbol.text-buffer-move-left", target::symbols::kTextBufferMoveLeft);
        fnCTextBuffer_MoveRightCall = ResolveInputSymbol<fnInstanceAction_t>(
            "input.symbol.text-buffer-move-right", target::symbols::kTextBufferMoveRight);
        if (fnCTextInputEvent_initCall == nullptr || fnCInputEvent_initCall == nullptr ||
            fnCInputEvent_DestructorCall == nullptr ||
            fnCTextBuffer_EnterBackspaceCall == nullptr ||
            fnCTextBuffer_GetCursorPositionInStringCall == nullptr ||
            fnCTextBuffer_MoveLeftCall == nullptr || fnCTextBuffer_MoveRightCall == nullptr) {
            return;
        }
        target_input::bind({fnCTextInputEvent_initCall, fnCInputEvent_initCall,
                fnCInputEvent_DestructorCall, fnCTextBuffer_EnterBackspaceCall,
                fnCTextBuffer_GetCursorPositionInStringCall,
                fnCTextBuffer_MoveLeftCall, fnCTextBuffer_MoveRightCall});

        //拦截输入完成事件，使其支持UTF8文本输入
        install_CSdlEvents_HandlePdxEvents_1();
        //拦截输入中事件，使其在输入法输入中时按下的退格键不对已输入文本生效
        install_CSdlEvents_HandlePdxEvents_2();
        //处理退格键删除逃逸字符时的逻辑，使其能正确删除三字节
        install_CTextBuffer_HandleKeyEvent_1();
        //在文本输入框中按下←左方向键时，使其能正确越过逃逸字符
        install_CTextBuffer_HandleKeyEvent_2();
        //在文本输入框中按下→右方向键时，使其能正确越过逃逸字符
        install_CTextBuffer_HandleKeyEvent_3();
    }

}
