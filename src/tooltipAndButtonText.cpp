#include "tooltipAndButtonText.h"
#include "features/escaped_text/escaped_text.h"
#include "runtime/diagnostics/startup_diagnostics.h"
#include "targets/eu4_1_37_5/macos_x86_64/hook_symbols.h"
#include "targets/eu4_1_37_5/macos_x86_64/target_facts.h"
#include "targets/eu4_1_37_5/macos_x86_64/text_rendering/rendering_patch.h"

namespace tooltipAndButtonText {
    namespace target = eu4dll::targets::eu4_1_37_5::macos_x86_64;
    namespace renderingPatch = target::text_rendering;
    extern "C" {

    uintptr_t g_RenderToTexture_1_BypassAddr = 0;
    uint32_t g_RenderToTexture_1_CurrentChar = 0;

    uintptr_t g_RenderToTexture_3_BypassAddr = 0;

    uintptr_t g_RenderToTexture_2_RetAddr = 0;
    uintptr_t g_RenderToTexture_2_BypassAddr = 0;
    }

    __attribute__((naked)) void naked_CBitmapFont_RenderToTexture_1() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"

                "mov rbx, rax \n"
                "movzx eax, byte ptr [rax] \n"

                "cmp al, %c[e1] \n"
                "jz 1f \n"
                "cmp al, %c[e2] \n"
                "jz 2f \n"
                "cmp al, %c[e3] \n"
                "jz 3f \n"
                "cmp al, %c[e4] \n"
                "jz 4f \n"

                "jmp 7f \n"

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

                "5: \n"

                "push rax \n"
                "lea rdi, [rbp - 0x2740] \n"
                "movzx rsi, byte ptr [rbx+1] \n"
                "call [rip + _g_CString_AppendCharAddress] \n"

                "lea rdi, [rbp - 0x2740] \n"
                "movzx rsi, byte ptr [rbx+2] \n"
                "call [rip + _g_CString_AppendCharAddress] \n"

                "pop rax \n"
                "add r12d, 2 \n" // 增加循环总计数


                "cmp eax, 256 \n"
                "jb 7f \n"
                "add eax, %c[go] \n"
                "7: \n"
                "mov dword ptr [rip + _g_RenderToTexture_1_CurrentChar], eax \n"
                "mov rbx, qword ptr [r15+rax*8+%c[glyph_table]] \n"

                "jmp qword ptr [rip + _g_RenderToTexture_1_BypassAddr] \n"

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
        [glyph_table] "i"(target::base::kGlyphTableOffset),
        [nf] "i"(target::rendering::kMissingFontGlyph),
        [nd] "i"(target::rendering::kUndefinedGlyph)
        );
    }

/**
 Hook函数：CBitmapFont::RenderToTexture 字符预处理循环
 作用：使其正确识别双字节
 */
    void install_CBitmapFont_RenderToTexture_1() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        if (!renderingPatch::install({
                renderingPatch::PatchId::TexturePreprocess,
                reinterpret_cast<uintptr_t>(naked_CBitmapFont_RenderToTexture_1),
                {{"bypass", &g_RenderToTexture_1_BypassAddr}}})) {
            return;
        }
        installGuard.MarkSuccess();
    }

    __attribute__((naked)) void naked_CBitmapFont_RenderToTexture_2() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"

                "cmp dword ptr [_g_RenderToTexture_1_CurrentChar + rip], 0xFF \n"
                "ja 1f \n"
                "cmp word ptr [rbx+%c[line_break]], 0 \n"
                "jmp qword ptr [rip + _g_RenderToTexture_2_RetAddr] \n"

                "1: \n"
                "jmp qword ptr [rip + _g_RenderToTexture_2_BypassAddr] \n"

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
 Hook函数：CBitmapFont::RenderToTexture 字符预处理循环
 作用：强制每个双字节字符都检查是否换行
 */
    void install_CBitmapFont_RenderToTexture_2() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        if (!renderingPatch::install({
                renderingPatch::PatchId::TextureLineBreak,
                reinterpret_cast<uintptr_t>(naked_CBitmapFont_RenderToTexture_2),
                {{"return", &g_RenderToTexture_2_RetAddr},
                 {"bypass", &g_RenderToTexture_2_BypassAddr}}})) {
            return;
        }
        installGuard.MarkSuccess();
    }


    __attribute__((naked)) void naked_CBitmapFont_RenderToTexture_3() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"

                "mov r10, rax \n"
                "movzx eax, byte ptr [rax] \n"

                "cmp al, %c[e1] \n"
                "jz 1f \n"
                "cmp al, %c[e2] \n"
                "jz 2f \n"
                "cmp al, %c[e3] \n"
                "jz 3f \n"
                "cmp al, %c[e4] \n"
                "jz 4f \n"

                "jmp 7f \n"

                "1: \n"
                "movzx eax, word ptr [r10+1] \n"
                "jmp 5f \n"

                "2: \n"
                "movzx eax, word ptr [r10+1] \n"
                "sub eax, %c[s2] \n"
                "jmp 5f \n"

                "3: \n"
                "movzx eax, word ptr [r10+1] \n"
                "add eax, %c[s3] \n"
                "jmp 5f \n"

                "4: \n"
                "movzx eax, word ptr [r10+1] \n"
                "add eax, %c[s4] \n"

                "5: \n"

                "add r12d, 2 \n" // 增加循环总计数

                "cmp eax, 256 \n"
                "jb 7f \n"
                "add eax, %c[go] \n"

                "7: \n"
                "mov r10, qword ptr [r14+rax*8+%c[glyph_table]] \n"
                "jmp qword ptr [rip + _g_RenderToTexture_3_BypassAddr] \n"

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
        [glyph_table] "i"(target::base::kGlyphTableOffset),
        [nf] "i"(target::rendering::kMissingFontGlyph),
        [nd] "i"(target::rendering::kUndefinedGlyph)
        );
    }

/**
 Hook函数：CBitmapFont::RenderToTexture 渲染循环
 作用：部分UI组件使用这个渲染函数
 */
    void install_CBitmapFont_RenderToTexture_3() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        if (!renderingPatch::install({
                renderingPatch::PatchId::TextureGlyphLoop,
                reinterpret_cast<uintptr_t>(naked_CBitmapFont_RenderToTexture_3),
                {{"bypass", &g_RenderToTexture_3_BypassAddr}}})) {
            return;
        }
        installGuard.MarkSuccess();
    }

    void install() {
        //预处理
        install_CBitmapFont_RenderToTexture_1();
        //双字节文本时强制检查是否换行
        install_CBitmapFont_RenderToTexture_2();
        //渲染
        install_CBitmapFont_RenderToTexture_3();
    }
}
