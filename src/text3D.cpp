#include "text3D.h"
#include "features/escaped_text/escaped_text.h"
#include "runtime/diagnostics/startup_diagnostics.h"
#include "targets/eu4_1_37_5/macos_x86_64/hook_symbols.h"
#include "targets/eu4_1_37_5/macos_x86_64/target_facts.h"
#include "targets/eu4_1_37_5/macos_x86_64/text_rendering/rendering_patch.h"

namespace text3D {
    namespace target = eu4dll::targets::eu4_1_37_5::macos_x86_64;
    namespace renderingPatch = target::text_rendering;

    extern "C" {
    uintptr_t g_Render3d_1_BypassAddr = 0;

    uintptr_t g_Render3d_2_BypassAddr = 0;
    }

    __attribute__((naked)) void naked_CBitmapFont_Render3d_1() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"

                "mov rdi, rax \n"
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
                "movzx eax, word ptr [rdi+1] \n"
                "jmp 5f \n"

                "2: \n"
                "movzx eax, word ptr [rdi+1] \n"
                "sub eax, %c[s2] \n"
                "jmp 5f \n"

                "3: \n"
                "movzx eax, word ptr [rdi+1] \n"
                "add eax, %c[s3] \n"
                "jmp 5f \n"

                "4: \n"
                "movzx eax, word ptr [rdi+1] \n"
                "add eax, %c[s4] \n"

                // 通用收尾
                "5: \n"

                "add r12d, 2 \n" // 增加循环总计数

                "cmp eax, 256 \n"
                "jb 7f \n"
                "add eax, %c[go] \n"
                "7: \n"
                "mov rax, [r15+rax*8+%c[glyph_table]] \n"
                "jmp qword ptr [rip + _g_Render3d_1_BypassAddr] \n"

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
        [glyph_table] "i"(target::base::kGlyphTableOffset)
        );
    }

/**
 Hook函数：CBitmapFont::Render3d 渲染循环
 作用：使其支持正确读取双字节字符
 */
    void install_CBitmapFont_Render3d_1() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        if (!renderingPatch::install({
                renderingPatch::PatchId::Render3dGlyphLoop,
                reinterpret_cast<uintptr_t>(naked_CBitmapFont_Render3d_1),
                {{"bypass", &g_Render3d_1_BypassAddr}}})) {
            return;
        }
        installGuard.MarkSuccess();
    }


    __attribute__((naked)) void naked_CBitmapFont_Render3d_2() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"

                "mov rbx, rax \n" // 保存当前指针，用于后面追加字符写入缓冲区
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

                // 通用收尾
                "5: \n"

                "add r15d, 2 \n" // 增加循环总计数

                "push rax \n" //保护RAX寄存器
                "lea rdi, [rbp-0x128] \n" //将缓冲区变量传递当作第一个参数
                "movsx rsi, byte ptr [rbx+1] \n" //将后面一个字节当作第二个参数
                "call qword ptr [rip + _g_CString_AppendCharAddress] \n"

                "lea rdi, [rbp-0x128] \n"
                "movsx rsi, byte ptr [rbx+2] \n"
                "call qword ptr [rip + _g_CString_AppendCharAddress] \n"
                "pop rax \n"

                "cmp eax, 256 \n"
                "jb 7f \n"
                "add eax, %c[go] \n"
                "7: \n"
                "mov rcx, [rbp - 0xF0] \n"
                "jmp qword ptr [rip + _g_Render3d_2_BypassAddr] \n"

                ".att_syntax prefix \n"
                :
                : [e1] "i"(eu4dll::escaped_text::kEscape1),
        [e2] "i"(eu4dll::escaped_text::kEscape2),
        [e3] "i"(eu4dll::escaped_text::kEscape3),
        [e4] "i"(eu4dll::escaped_text::kEscape4),
        [s2] "i"(eu4dll::escaped_text::kEscape2Shift),
        [s3] "i"(eu4dll::escaped_text::kEscape3Shift),
        [s4] "i"(eu4dll::escaped_text::kEscape4Shift),
        [go] "i"(eu4dll::escaped_text::kUnicodeGlyphOffset)
        );
    }

/**
 Hook函数：CBitmapFont::Render3d 字符初步处理循环，测量与处理换行
 作用：使其能够正确读取双字节字符
 */
    void install_CBitmapFont_Render3d_2() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        if (!renderingPatch::install({
                renderingPatch::PatchId::Render3dPreprocess,
                reinterpret_cast<uintptr_t>(naked_CBitmapFont_Render3d_2),
                {{"bypass", &g_Render3d_2_BypassAddr}}})) {
            return;
        }
        installGuard.MarkSuccess();
    }


    void install() {
        //渲染
        install_CBitmapFont_Render3d_1();
        //文本预处理
        install_CBitmapFont_Render3d_2();
    }
}
