#include "mainText.h"
#include "features/escaped_text/escaped_text.h"
#include "runtime/diagnostics/startup_diagnostics.h"
#include "targets/eu4_1_37_5/macos_x86_64/target_facts.h"
#include "targets/eu4_1_37_5/macos_x86_64/text_rendering/rendering_patch.h"

namespace mainText {
    namespace target = eu4dll::targets::eu4_1_37_5::macos_x86_64;
    namespace renderingPatch = target::text_rendering;
    extern "C" {
    uintptr_t g_RenderToScreen_3_RetAddr = 0;
    uintptr_t g_RenderToScreen_3_BypassAddr = 0;

    uintptr_t g_RenderToScreen_1_RetAddr = 0;
    uintptr_t g_RenderToScreen_1_BypassAddr = 0;
    uint32_t g_RenderToScreen_1_CurrentChar = 0;

    uintptr_t g_RenderToScreen_2_RetAddr = 0;
    uintptr_t g_RenderToScreen_2_BypassAddr = 0;

    }

    __attribute__((naked)) void naked_CBitmapFont_RenderToScreen_1() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"

                "movzx edx, byte ptr [rdi+r11] \n"

                "cmp dl, %c[e1] \n"
                "jz 1f \n"
                "cmp dl, %c[e2] \n"
                "jz 2f \n"
                "cmp dl, %c[e3] \n"
                "jz 3f \n"
                "cmp dl, %c[e4] \n"
                "jz 4f \n"

                "mov dword ptr [rip + _g_RenderToScreen_1_CurrentChar], 1\n"
                "jmp qword ptr [rip + _g_RenderToScreen_1_RetAddr] \n"

                "1: \n"
                "movzx edx, word ptr [rdi+r11+1] \n"
                "jmp 5f \n"

                "2: \n"
                "movzx edx, word ptr [rdi+r11+1] \n"
                "sub edx, %c[s2] \n"
                "jmp 5f \n"

                "3: \n"
                "movzx edx, word ptr [rdi+r11+1] \n"
                "add edx, %c[s3] \n"
                "jmp 5f \n"

                "4: \n"
                "movzx edx, word ptr [rdi+r11+1] \n"
                "add edx, %c[s4] \n"

                "5: \n"
                "mov dword ptr [rip + _g_RenderToScreen_1_CurrentChar], edx \n"

                "mov ax, [rdi+r11+1] \n" // 读取标识符后2个字节
                "mov [rsi+rcx+1], ax \n" // 写入缓存区 [rbx+1]

                //        "mov al, [rdi+r11+1]\n" // 读取标识符后第 1 个字节
                //        "mov [rsi+rcx+1], al\n" // 写入缓存区 [rbx+1]
                //        "mov al, [rdi+r11+2]\n" // 读取标识符后第 2 个字节
                //        "mov [rsi+rcx+2], al\n" // 写入缓存区 [rbx+2]

                "add r13d, 2 \n" // 源字符串读取索引 + 2
                "add r12d, 2 \n" // 缓存区索引 + 2

                "cmp edx, 256 \n"
                "jb 7f \n"
                "add edx, %c[go] \n"
                "7: \n"
                "jmp qword ptr [rip + _g_RenderToScreen_1_BypassAddr] \n"

                ".att_syntax prefix \n"
                :
                : [e1] "i"(eu4dll::escaped_text::kEscape1),
        [e2] "i"(eu4dll::escaped_text::kEscape2),
        [e3] "i"(eu4dll::escaped_text::kEscape3),
        [e4] "i"(eu4dll::escaped_text::kEscape4),
        [s2] "i"(eu4dll::escaped_text::kEscape2Shift),
        [s3] "i"(eu4dll::escaped_text::kEscape3Shift),
        [s4] "i"(eu4dll::escaped_text::kEscape4Shift),
        [go] "i"(eu4dll::escaped_text::kUnicodeGlyphOffset),
        [nf] "i"(target::rendering::kMissingFontGlyph),
        [nd] "i"(target::rendering::kUndefinedGlyph)
        );
    }

/**
 Hook函数：CBitmapFont::RenderToScreen 字符预处理循环
 作用：使其能正确识别双字节文本
 */
    void install_CBitmapFont_RenderToScreen_1() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        if (!renderingPatch::install({
                renderingPatch::PatchId::ScreenPreprocess,
                reinterpret_cast<uintptr_t>(naked_CBitmapFont_RenderToScreen_1),
                {{"return", &g_RenderToScreen_1_RetAddr},
                 {"bypass", &g_RenderToScreen_1_BypassAddr}}})) {
            return;
        }
        installGuard.MarkSuccess();
    }

    __attribute__((naked)) void naked_CBitmapFont_RenderToScreen_2() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"

                "cmp dword ptr [_g_RenderToScreen_1_CurrentChar + rip], 0xFF \n"
                "ja 1f \n"

                "cmp word ptr [r14+%c[line_break]], 0 \n"
                "jmp qword ptr [rip + _g_RenderToScreen_2_RetAddr] \n"

                "1: \n"
                "jmp qword ptr [rip + _g_RenderToScreen_2_BypassAddr] \n"

                ".att_syntax prefix \n"
                :
                : [e1] "i"(eu4dll::escaped_text::kEscape1),
        [e2] "i"(eu4dll::escaped_text::kEscape2),
        [e3] "i"(eu4dll::escaped_text::kEscape3),
        [e4] "i"(eu4dll::escaped_text::kEscape4),
        [s2] "i"(eu4dll::escaped_text::kEscape2Shift),
        [s3] "i"(eu4dll::escaped_text::kEscape3Shift),
        [s4] "i"(eu4dll::escaped_text::kEscape4Shift),
        [go] "i"(eu4dll::escaped_text::kUnicodeGlyphOffset),
        [line_break] "i"(target::base::kBitmapCharacterLineBreakOffset),
        [nf] "i"(target::rendering::kMissingFontGlyph),
        [nd] "i"(target::rendering::kUndefinedGlyph)
        );
    }

/**
 Hook函数：CBitmapFont::RenderToScreen 字符预处理循环
 作用：双字节文本时强制检测是否换行
 */
    void install_CBitmapFont_RenderToScreen_2() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        if (!renderingPatch::install({
                renderingPatch::PatchId::ScreenLineBreak,
                reinterpret_cast<uintptr_t>(naked_CBitmapFont_RenderToScreen_2),
                {{"return", &g_RenderToScreen_2_RetAddr},
                 {"bypass", &g_RenderToScreen_2_BypassAddr}}})) {
            return;
        }
        installGuard.MarkSuccess();
    }

    __attribute__((naked)) void naked_CBitmapFont_RenderToScreen_3() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"

                "lea rbx, [r12+rax] \n"
                "movzx eax, byte ptr [rbx] \n"

                "cmp al, %c[e1] \n"
                "jz 1f \n"
                "cmp al, %c[e2] \n"
                "jz 2f \n"
                "cmp al, %c[e3] \n"
                "jz 3f \n"
                "cmp al, %c[e4] \n"
                "jz 4f \n"

                "jmp qword ptr [rip + _g_RenderToScreen_3_RetAddr] \n"

                "1: \n"
                "movzx eax, word ptr [rbx+1] \n"
                "jmp 5f \n"

                "2: \n"
                "movzx eax, word ptr [rbx+1] \n"
                "sub eax, %c[s2] \n"
                "jmp 5f \n"

                "3: \n"
                "movzx eax, word ptr [rbx+1] \n"
                "add eax, %c[s3] \n"
                "jmp 5f \n"

                "4: \n"
                "movzx eax, word ptr [rbx+1] \n"
                "add eax, %c[s4] \n"

                // 通用收尾
                "5: \n"
                "add r15d, 2 \n"         // 增加循环总计数
                "add r12, 2 \n"         // 同步增加当前迭代的索引

                "cmp eax, 256 \n"
                "jb 7f \n"
                "add eax, %c[go] \n"
                "7: \n"
                "jmp qword ptr [rip + _g_RenderToScreen_3_BypassAddr] \n"

                ".att_syntax prefix \n"
                :
                : [e1] "i"(eu4dll::escaped_text::kEscape1),
        [e2] "i"(eu4dll::escaped_text::kEscape2),
        [e3] "i"(eu4dll::escaped_text::kEscape3),
        [e4] "i"(eu4dll::escaped_text::kEscape4),
        [s2] "i"(eu4dll::escaped_text::kEscape2Shift),
        [s3] "i"(eu4dll::escaped_text::kEscape3Shift),
        [s4] "i"(eu4dll::escaped_text::kEscape4Shift),
        [go] "i"(eu4dll::escaped_text::kUnicodeGlyphOffset),
        [nf] "i"(target::rendering::kMissingFontGlyph),
        [nd] "i"(target::rendering::kUndefinedGlyph)
        );
    }

/**
 Hook函数：CBitmapFont::RenderToScreen 渲染循环
 作用：使其能正确识别双字节文本
 */
    void install_CBitmapFont_RenderToScreen_3() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        if (!renderingPatch::install({
                renderingPatch::PatchId::ScreenGlyphLoop,
                reinterpret_cast<uintptr_t>(naked_CBitmapFont_RenderToScreen_3),
                {{"return", &g_RenderToScreen_3_RetAddr},
                 {"bypass", &g_RenderToScreen_3_BypassAddr}}})) {
            return;
        }
        installGuard.MarkSuccess();
    }

    void install() {
        //文本预处理
        install_CBitmapFont_RenderToScreen_1();
        //双字节文本时强制检查是否换行
        install_CBitmapFont_RenderToScreen_2();
        //渲染
        install_CBitmapFont_RenderToScreen_3();
    }

}
