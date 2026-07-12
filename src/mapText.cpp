#include "mapText.h"
#include "features/escaped_text/escaped_text.h"
#include "features/text_rendering/text_rendering.h"
#include "platform/macos/symbol_lookup.h"
#include "runtime/diagnostics/startup_diagnostics.h"
#include "targets/eu4_1_37_5/macos_x86_64/hook_symbols.h"
#include "targets/eu4_1_37_5/macos_x86_64/target_facts.h"
#include "targets/eu4_1_37_5/macos_x86_64/text_rendering/rendering_patch.h"

namespace mapText {
    namespace target = eu4dll::targets::eu4_1_37_5::macos_x86_64;
    namespace renderingPatch = target::text_rendering;
    extern "C" {
    uintptr_t g_AddNameArea_3_BypassAddr = 0;
    uintptr_t g_AddNameArea_1_RetAddr = 0;
    uintptr_t g_AddNameArea_1_BypassAddr = 0;

    uintptr_t g_FillVertexBuffer_1_BypassAddr = 0;
    uintptr_t g_FillVertexBuffer_2_BypassAddr = 0;

    uintptr_t g_CurveText_1_BypassAddr = 0;
    uintptr_t g_CurveText_2_BypassAddr = 0;
    uintptr_t g_CurveText_3_BypassAddr = 0;

    uint32_t g_CurveText_1_SkipByteCount = 0;

    uintptr_t g_AddNudgedNames_BypassAddr = 0;

    void *g_OriginalToUpper_Addr;

    }

    __attribute__((naked)) void naked_CGenerateNamesWork_AddNameArea_1() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"

                "mov dword ptr [rbp-0xD8], 0 \n" //清空缓冲区变量ver_D8(4字节)，使CString_Append函数能正确识别字符长度
                "mov byte ptr [rbp-0xD8], al \n" //执行被覆盖的指令，先存入当前字符到缓冲区

                "cmp al, %c[e1] \n"
                "jz 1f \n"
                "cmp al, %c[e2] \n"
                "jz 1f \n"
                "cmp al, %c[e3] \n"
                "jz 1f \n"
                "cmp al, %c[e4] \n"
                "jz 1f \n"

                "jmp 2f \n"

                "1: \n"

                "mov bx, word ptr [r15+r14+1] \n" //如果是双字节文本，就读入后面两个字节
                "mov word ptr [rbp-0xD8 + 1], bx \n" //将后面两个字节追加进缓冲区

                "add r14, 2 \n"

                "2: \n"

                "lea rbx, [rbp-0x128] \n"
                "mov rdi, rbx \n"
                "lea rsi, [rbp-0xD8] \n"
                "call qword ptr [rip + _g_CString_AppendCharConstAddress] \n"

                "cmp r13, r14 \n" //原循环是r14不等于 源文本长度-1 就继续，由于手动递增了r14，如果末尾字是双字节文本，会导致r14超过r13致使无限循环导致崩溃。所以需要手动判断并跳过追加末尾字符区域
                "jbe 3f \n"

                "jmp qword ptr [rip + _g_AddNameArea_1_BypassAddr] \n"

                "3: \n"
                "mov dword ptr [rbp-0xD8], 0 \n"
                "jmp qword ptr [rip + _g_AddNameArea_1_RetAddr] \n"

                ".att_syntax prefix \n"
                :
                : [e1] "i"(eu4dll::escaped_text::kEscape1),
        [e2] "i"(eu4dll::escaped_text::kEscape2),
        [e3] "i"(eu4dll::escaped_text::kEscape3),
        [e4] "i"(eu4dll::escaped_text::kEscape4)
        );
    }

/**
 Hook函数：CGenerateNamesWork::AddNameArea 加空格循环
 作用：不断尝试在每个字中间添加X个空格，使其总体宽度能够填满地图区域。找到最合适的间距后，调用FillVertexBuffer生成顶点。
 */
    void install_CGenerateNamesWork_AddNameArea_1() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        if (!renderingPatch::install({
                renderingPatch::PatchId::MapAddNameSpacing,
                reinterpret_cast<uintptr_t>(naked_CGenerateNamesWork_AddNameArea_1),
                {{"return", &g_AddNameArea_1_RetAddr},
                 {"bypass", &g_AddNameArea_1_BypassAddr}}})) {
            return;
        }
        installGuard.MarkSuccess();
    }

    __attribute__((naked)) void proxy_CGenerateNamesWork_AddNameArea_ToUpper_2() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"

                "push    rbp \n"
                "mov     rbp, rsp \n"
                "push    r15 \n"
                "push    r14 \n"
                "push    r12 \n"
                "push    rbx \n"
                "mov     r15, rdi \n"
                "movzx   r14d, byte ptr [rdi] \n"
                "test    r14b, 1 \n"
                "jnz     1f \n"
                "inc     r15 \n"
                "shr     r14, 1 \n"
                "jmp     2f \n"

                "1: \n"
                "mov     r14, [r15+8] \n"
                "mov     r15, [r15+0x10] \n"

                "2: \n"
                "test    r14, r14 \n"
                "jz      4f \n"
                "mov     al, [r15] \n"
                "test    al, al \n"
                "jz      4f \n"
                "mov     ebx, 1 \n"
                "mov     r12, r15 \n"

                "3: \n"
                "cmp al, %c[e1] \n"
                "jz 5f \n"
                "cmp al, %c[e2] \n"
                "jz 5f \n"
                "cmp al, %c[e3] \n"
                "jz 5f \n"
                "cmp al, %c[e4] \n"
                "jz 5f \n"

                "movsx edi, al \n"
                "call [rip + _g_OriginalToUpper_Addr] \n"
                "mov [r12], al \n"
                "jmp 6f \n"

                "5: \n"
                "add ebx, 2 \n"

                "6: \n"
                "mov     eax, ebx \n"
                "cmp     r14, rax \n"
                "jbe     4f \n"
                "lea     r12, [r15+rax] \n"
                "mov     al, [r15+rax] \n"
                "inc     ebx \n"
                "test    al, al \n"
                "jnz     3b \n"

                "4: \n"
                "pop     rbx \n"
                "pop     r12 \n"
                "pop     r14 \n"
                "pop     r15 \n"
                "pop     rbp \n"
                "retn \n"

                ".att_syntax prefix \n"
                :
                : [e1] "i"(eu4dll::escaped_text::kEscape1),
        [e2] "i"(eu4dll::escaped_text::kEscape2),
        [e3] "i"(eu4dll::escaped_text::kEscape3),
        [e4] "i"(eu4dll::escaped_text::kEscape4)
        );
    }

/**
 Hook函数：CGenerateNamesWork::AddNameArea
 作用：替换ToUpper CALL，避免将标识符后面的字节当成小写字符从而导致文字改变
 */
    void install_CGenerateNamesWork_AddNameArea_2() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        g_OriginalToUpper_Addr = eu4dll::platform::macos::ResolveLiveSymbol(
            "rendering.map.add-name-area.uppercase", target::kDiagnosticTargetId,
            target::symbols::kToUpper);
        if (g_OriginalToUpper_Addr == nullptr) {
            printf("eu4dll_mac [Error] rendering.map.add-name-area.uppercase "
                   "symbol lookup failed: %s\n", target::symbols::kToUpper);
            return;
        }
        if (!renderingPatch::install({
                renderingPatch::PatchId::MapAddNameUppercase,
                reinterpret_cast<uintptr_t>(proxy_CGenerateNamesWork_AddNameArea_ToUpper_2),
                {}})) {
            return;
        }
        installGuard.MarkSuccess();
    }


    __attribute__((naked)) void naked_CGenerateNamesWork_AddNameArea_3() {
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

                "add ebx, 2 \n" // 增加循环总计数

                "cmp eax, 256 \n"
                "jb 7f \n"
                "add eax, %c[go] \n"
                "7: \n"

                "mov rax, [r14+rax*8+%c[glyph_table]] \n"

                "jmp qword ptr [rip + _g_AddNameArea_3_BypassAddr] \n"

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
 Hook函数：CGenerateNamesWork::AddNameArea 通过循环遍历获取有效字符数，来决定顶点缓冲区大小
 作用：使其能正确识别字符数
 */
    void install_CGenerateNamesWork_AddNameArea_3() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        if (!renderingPatch::install({
                renderingPatch::PatchId::MapAddNameGlyphCount,
                reinterpret_cast<uintptr_t>(naked_CGenerateNamesWork_AddNameArea_3),
                {{"bypass", &g_AddNameArea_3_BypassAddr}}})) {
            return;
        }
        installGuard.MarkSuccess();
    }


    __attribute__((naked)) void naked_CBitmapFont_FillVertexBuffer_1() {
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

                "add ebx, 2 \n" // 增加循环总计数

                "cmp eax, 256 \n"
                "jb 7f \n"
                "add eax, %c[go] \n"

                "7: \n"

                "mov rax, [r15+rax*8+%c[glyph_table]] \n"
                "jmp qword ptr [rip + _g_FillVertexBuffer_1_BypassAddr] \n"

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
 Hook函数：CBitmapFont::FillVertexBuffer 字符顶点生成循环
 作用：使其能正确识别双字节字符
 */
    void install_CBitmapFont_FillVertexBuffer_1() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        if (!renderingPatch::install({
                renderingPatch::PatchId::MapFillVertexGlyph,
                reinterpret_cast<uintptr_t>(naked_CBitmapFont_FillVertexBuffer_1),
                {{"bypass", &g_FillVertexBuffer_1_BypassAddr}}})) {
            return;
        }
        installGuard.MarkSuccess();
    }

    __attribute__((naked)) void naked_CBitmapFont_FillVertexBuffer_2() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"

                "mov r13, rax \n"
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
                "movzx eax, word ptr [r13+1] \n"
                "jmp 5f \n"

                "2: \n"
                "movzx eax, word ptr [r13+1] \n"
                "sub eax, %c[s2] \n"
                "jmp 5f \n"

                "3: \n"
                "movzx eax, word ptr [r13+1] \n"
                "add eax, %c[s3] \n"
                "jmp 5f \n"

                "4: \n"
                "movzx eax, word ptr [r13+1] \n"
                "add eax, %c[s4] \n"

                // 通用收尾
                "5: \n"

                "push rax \n"

                "lea rdi, [rbp - 0x1000] \n"
                "movzx rsi, byte ptr [r13+1] \n"
                "call [rip + _g_CString_AppendCharAddress] \n"

                "lea rdi, [rbp - 0x1000] \n"
                "movzx rsi, byte ptr [r13+2] \n"
                "call [rip + _g_CString_AppendCharAddress] \n"

                "pop rax \n"

                "add r12d, 2 \n" // 增加循环总计数

                "cmp eax, 256 \n"
                "jb 7f \n"
                "add eax, %c[go] \n"
                "7: \n"
                "mov r13, [r15+rax*8+%c[glyph_table]] \n"
                "jmp qword ptr [rip + _g_FillVertexBuffer_2_BypassAddr] \n"

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
 Hook函数：CBitmapFont::FillVertexBuffer 字符初步处理循环，计算宽度与处理换行
 作用：使其能正确识别双字节字符
 */
    void install_CBitmapFont_FillVertexBuffer_2() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        if (!renderingPatch::install({
                renderingPatch::PatchId::MapFillVertexMeasure,
                reinterpret_cast<uintptr_t>(naked_CBitmapFont_FillVertexBuffer_2),
                {{"bypass", &g_FillVertexBuffer_2_BypassAddr}}})) {
            return;
        }
        installGuard.MarkSuccess();
    }


    __attribute__((naked)) void naked_CurveText_1() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"

                // 为指针加上跳过字节数才是当前真实索引
                "mov r15d, dword ptr [rip + _g_CurveText_1_SkipByteCount] \n"
                "add rax, r15 \n"

                "mov r15, rax \n"
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
                "movzx eax, word ptr [r15+1] \n"
                "jmp 5f \n"

                "2: \n"
                "movzx eax, word ptr [r15+1] \n"
                "sub eax, %c[s2] \n"
                "jmp 5f \n"

                "3: \n"
                "movzx eax, word ptr [r15+1] \n"
                "add eax, %c[s3] \n"
                "jmp 5f \n"

                "4: \n"
                "movzx eax, word ptr [r15+1] \n"
                "add eax, %c[s4] \n"

                // 通用收尾
                "5: \n"
                "add dword ptr [rip + _g_CurveText_1_SkipByteCount], 2 \n" //增加外部跳过字节计数

                "cmp eax, 256 \n"
                "jb 7f \n"
                "add eax, %c[go] \n"

                "7: \n"

                "mov r15, [r12+rax*8] \n"

                "jmp qword ptr [rip + _g_CurveText_1_BypassAddr] \n"

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
 Hook函数：CurveText
 作用：将FillVertexBuffer函数生成的文字顶点网格贴合地图曲线
 */
    void install_CurveText_1() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        if (!renderingPatch::install({
                renderingPatch::PatchId::MapCurveGlyph,
                reinterpret_cast<uintptr_t>(naked_CurveText_1),
                {{"bypass", &g_CurveText_1_BypassAddr}}})) {
            return;
        }
        installGuard.MarkSuccess();
    }


    __attribute__((naked)) void naked_CurveText_2() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"
                "mov r14d, 0 \n"
                "mov dword ptr[rip + _g_CurveText_1_SkipByteCount], 0 \n" //循环开始时清空上一轮计数

                "jmp qword ptr [rip + _g_CurveText_2_BypassAddr] \n"

                ".att_syntax prefix \n"
                );
    }

/**
 Hook函数：CurveText
 作用：将外部维护的计数在每一次循环前清零
 */
    void install_CurveText_2() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        if (!renderingPatch::install({
                renderingPatch::PatchId::MapCurveReset,
                reinterpret_cast<uintptr_t>(naked_CurveText_2),
                {{"bypass", &g_CurveText_2_BypassAddr}}})) {
            return;
        }
        installGuard.MarkSuccess();
    }


    __attribute__((naked)) void naked_CurveText_3() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"
                "mov eax,[rbp - 0x90] \n"

                "jmp qword ptr [rip + _g_CurveText_3_BypassAddr] \n"

                ".att_syntax prefix \n"
                );
    }

/**
 Hook函数：CurveText
 作用：将已存储的文本真实长度传递给变量进行对比，避免循环内重复遍历。
 */
    void install_CurveText_3() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        if (!renderingPatch::install({
                renderingPatch::PatchId::MapCurveLength,
                reinterpret_cast<uintptr_t>(naked_CurveText_3),
                {{"bypass", &g_CurveText_3_BypassAddr}}})) {
            return;
        }
        installGuard.MarkSuccess();
    }


    uint64_t proxy_CurveText_GetSize_CStringGetSize_4(std::string *cstring_this) {
        return eu4dll::text_rendering::logical_character_count(*cstring_this);
    }

/**
 Hook函数：CurveText
 作用：替换getSize CALL获取真实字符长度以便循环遍历
 */
    void install_CurveText_4() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        if (!renderingPatch::install({
                renderingPatch::PatchId::MapCurveLogicalSizeFirst,
                reinterpret_cast<uintptr_t>(proxy_CurveText_GetSize_CStringGetSize_4),
                {}})) {
            return;
        }
        if (!renderingPatch::install({
                renderingPatch::PatchId::MapCurveLogicalSizeSecond,
                reinterpret_cast<uintptr_t>(proxy_CurveText_GetSize_CStringGetSize_4),
                {}})) {
            return;
        }
        installGuard.MarkSuccess();
    }


    __attribute__((naked)) void naked_CCountryNameCollection_AddNudgedNames() {
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

                "5: \n"

                "add r13d, 2 \n" // 增加循环总计数

                "cmp eax, 256 \n"
                "jb 7f \n"
                "add eax, %c[go] \n"
                "7: \n"

                "mov rax, [r14+rax*8+%c[glyph_table]] \n"

                "jmp qword ptr [rip + _g_AddNudgedNames_BypassAddr] \n"

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
 Hook函数：CCountryNameCollection::AddNudgedNames
 作用：使其正确识别双字节字符。不知道做啥的，HOOK看文本也没能看出个啥，文本量远不及AddNameArea
 */
    void install_CCountryNameCollection_AddNudgedNames() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        if (!renderingPatch::install({
                renderingPatch::PatchId::MapAddNudgedNames,
                reinterpret_cast<uintptr_t>(naked_CCountryNameCollection_AddNudgedNames),
                {{"bypass", &g_AddNudgedNames_BypassAddr}}})) {
            return;
        }
        installGuard.MarkSuccess();
    }


    void install() {
        //尝试在文本中添加空格来填满地图
        install_CGenerateNamesWork_AddNameArea_1();
        //替换到大小CALL，使其能正确略过双字节文本
        install_CGenerateNamesWork_AddNameArea_2();
        //使其能正确识别字符数，来生成缓冲区
        install_CGenerateNamesWork_AddNameArea_3();
        //字符顶点生成
        install_CBitmapFont_FillVertexBuffer_1();
        //文本初处理循环
        install_CBitmapFont_FillVertexBuffer_2();
        //贴合地图曲线
        install_CurveText_1();
        install_CurveText_2();
        install_CurveText_3();
        install_CurveText_4();
        //不知道什么用
        install_CCountryNameCollection_AddNudgedNames();
    }

}
