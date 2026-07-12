#include "base.h"
#include "features/escaped_text/escaped_text.h"
#include "platform/macos/live_patch_runtime.h"
#include "runtime/diagnostics/patch_diagnostic.h"
#include "runtime/diagnostics/startup_diagnostics.h"
#include "targets/eu4_1_37_5/macos_x86_64/target_facts.h"

#include <iostream>

namespace base {
    namespace {
    namespace target = eu4dll::targets::eu4_1_37_5::macos_x86_64;

    void reportPatchFailure(const eu4dll::patch::PatchDiagnostic &diagnostic) {
        eu4dll::diagnostics::StartupDiagnostics::Instance().Record(diagnostic);
        std::cerr << "eu4dll_mac [Error] "
                  << eu4dll::patch::FormatDiagnostic(diagnostic) << std::endl;
    }
    }

    extern "C" {
    uintptr_t g_ParseFontFile_RetAddr;
    }


/**
 作用：在给CEU3BitmapFont分配内存时替换operator new函数调用，将其分配空间为0x3538扩充至0x86AC8，以包容双字节文本。同时清零分配到的内存空间，
 否则会有奇奇怪怪的问题（分配合适小内存就崩溃，分配超大了就没事，最后发现是分配大空间时系统会自动清零）。
 */
    void *proxy_CEU3Graphics_ReadGameSpecific_Operator_new(unsigned long) {
        //安全大小计算：原始大小0x3538，双字节最大值(0xFFFF + (0x3538 / 8)) * 8 + 0x3538
        void *address = operator new(target::base::kExpandedGraphicsAllocationSize);
        memset(address, 0, target::base::kExpandedGraphicsAllocationSize);
        return address;
    }

/**
 修改函数：CEU3Graphics::ReadGameSpecific
 作用：CEU3BitmapFont原始大小为0x3538，本补丁将其扩充至0x86AC8以支持双字节文本。其E8偏移处指向CBitmapCharacterSet类，该类第一个成员是大小为256的数组，每个数组成员占8字节指向CBitmapCharacter类。
 */
    void install_CEU3Graphics_ReadGameSpecific() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        eu4dll::patch::PatchDescription patch;
        patch.feature = "base.CEU3Graphics.ReadGameSpecific.allocate-font";
        patch.target = target::kDiagnosticTargetId;
        patch.location.pattern = target::base::kAllocateFont.pattern;
        patch.expected = eu4dll::patch::ExpectedBytes{
            target::base::kAllocateFontExpectedCallOffset,
            std::vector<std::uint8_t>(target::base::kAllocateFontOriginal.begin(),
                                      target::base::kAllocateFontOriginal.end()),
            std::vector<std::uint8_t>(target::base::kRelativeCallMask.begin(),
                                      target::base::kRelativeCallMask.end())};
        patch.mutation.kind = eu4dll::patch::MutationKind::Call;
        patch.mutation.offset = target::base::kAllocateFont.mutationOffset;
        patch.mutation.target = reinterpret_cast<uintptr_t>(
            proxy_CEU3Graphics_ReadGameSpecific_Operator_new);
        patch.mutation.callWidth = eu4dll::patch::CallWidth::Auto;

        const auto result = eu4dll::platform::macos::LivePatchRuntime().Install(patch);
        if (!result) {
            reportPatchFailure(result.diagnostic);
            return;
        }
        printf("eu4dll_mac [Success] %s ReplaceCall 匹配地址:0x%llx 写入地址:0x%llx\n",
               __func__, result.diagnostic.matchAddress, result.diagnostic.mutationAddress);
        installGuard.MarkSuccess();

    }

/**
 修改函数：CBitmapFont::ParseFontFile
 作用：原函数在加载字体文件.fnt时会过滤大于0xFF(255)的字体信息，此补丁将其上限修改为0xFFFF(65535)以支持双字节文本
 */
    void install_CBitmapFont_ParseFontFile_1() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        eu4dll::patch::PatchDescription patch;
        patch.feature = "base.CBitmapFont.ParseFontFile.allow-wide-glyphs";
        patch.target = target::kDiagnosticTargetId;
        patch.location.pattern = target::base::kAllowWideGlyphs.pattern;
        patch.expected = eu4dll::patch::ExpectedBytes{
            target::base::kAllowWideGlyphs.mutationOffset,
            std::vector<std::uint8_t>(target::base::kAllowWideGlyphsOriginal.begin(),
                                      target::base::kAllowWideGlyphsOriginal.end()),
            {}};
        patch.mutation.kind = eu4dll::patch::MutationKind::RawBytes;
        patch.mutation.offset = target::base::kAllowWideGlyphs.mutationOffset;
        patch.mutation.bytes = {0xFF};

        const auto result = eu4dll::platform::macos::LivePatchRuntime().Install(patch);
        if (!result) {
            reportPatchFailure(result.diagnostic);
            return;
        }
        printf("eu4dll_mac [Success] %s WriteMemory 匹配地址:0x%llx 写入地址:0x%llx\n",
               __func__, result.diagnostic.matchAddress, result.diagnostic.mutationAddress);
        installGuard.MarkSuccess();
    }

    __attribute__((naked)) void naked_CBitmapFont_ParseFontFile_2() {
        __asm__ volatile (
                ".intel_syntax noprefix \n"

                "cmp r14d, 256\n"
                "jb 1f \n"
                "add r14d, %c[go]\n"

                "1: \n"
                "mov ecx, r14d \n"
                "mov rax, [rbp - 0xD10] \n"
                "jmp qword ptr [rip + _g_ParseFontFile_RetAddr] \n"
                ".att_syntax prefix \n"
                :
                : [go] "i"(eu4dll::escaped_text::kUnicodeGlyphOffset)
                );
    }

/**
 Hook函数：CBitmapFont::ParseFontFile
 作用：CEU3BitmapFont原始大小为0x3538，扩充其内存大小后为避免查询字体编码信息时访问到0x3538以内的地址，为编码大于256的字体信息添加安全距离(0x3538 / 8)
 */
    void install_CBitmapFont_ParseFontFile_2() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        eu4dll::patch::PatchDescription patch;
        patch.feature = "base.CBitmapFont.ParseFontFile.wide-glyph-offset";
        patch.target = target::kDiagnosticTargetId;
        patch.location.pattern = target::base::kWideGlyphOffset.pattern;
        patch.expected = eu4dll::patch::ExpectedBytes{
            0, std::vector<std::uint8_t>(target::base::kWideGlyphOffsetOriginal.begin(),
                                         target::base::kWideGlyphOffsetOriginal.end()),
            {}};
        patch.mutation.kind = eu4dll::patch::MutationKind::Jump;
        patch.mutation.target = reinterpret_cast<uintptr_t>(naked_CBitmapFont_ParseFontFile_2);
        patch.continuations = {{"return", target::base::kWideGlyphOffset.continuationOffset}};
        patch.optimization.enabled = true;
        patch.optimization.hookAddress = patch.mutation.target;

        const auto result = eu4dll::platform::macos::LivePatchRuntime().Install(patch);
        // Install computes continuation addresses before the control-flow mutation.
        // Preserve the hook ABI even if post-write optimization reports a failure.
        g_ParseFontFile_RetAddr = result.ContinuationAddress("return");
        if (!result) {
            reportPatchFailure(result.diagnostic);
            return;
        }
        printf("eu4dll_mac [Success] %s HookJMP 匹配地址:0x%llx Hook地址:0x%llx 返回地址:0x%lx\n",
               __func__, result.diagnostic.matchAddress, result.diagnostic.mutationAddress,
               g_ParseFontFile_RetAddr);
        installGuard.MarkSuccess();
    }


/**
 修改函数：CTextureHandler::LoadTexture
 作用：原函数在加载字体文件.dds时设置了上限16MB，此补丁将其修改为64MB。FF FF FF 00 → FF FF FF 03
 */
    void install_CTextureHandler_LoadTexture() {
        eu4dll::diagnostics::InstallGuard installGuard(__func__, target::kDiagnosticTargetId);
        const auto installLimit = [](const char *feature, const target::HookSite &site) {
            eu4dll::patch::PatchDescription patch;
            patch.feature = feature;
            patch.target = target::kDiagnosticTargetId;
            patch.location.pattern = site.pattern;
            patch.expected =
                eu4dll::patch::ExpectedBytes{site.mutationOffset, {0x00}, {}};
            patch.mutation.kind = eu4dll::patch::MutationKind::RawBytes;
            patch.mutation.offset = site.mutationOffset;
            patch.mutation.bytes = {target::base::kExpandedTextureLimitByte};
            return eu4dll::platform::macos::LivePatchRuntime().Install(patch);
        };

        const auto first = installLimit("base.texture-size-limit.first",
                                        target::base::kTextureSizeLimit1);
        if (!first) {
            reportPatchFailure(first.diagnostic);
            return;
        }
        const auto second = installLimit("base.texture-size-limit.second",
                                         target::base::kTextureSizeLimit2);
        if (!second) {
            reportPatchFailure(second.diagnostic);
            return;
        }
        printf("eu4dll_mac [Success] %s first=0x%llx second=0x%llx\n", __func__,
               first.diagnostic.mutationAddress, second.diagnostic.mutationAddress);
        installGuard.MarkSuccess();
    }


    void install() {
        //修改CEU3BitmapFont类初始大小，使其能容纳双字节文本
        install_CEU3Graphics_ReadGameSpecific();
        //允许录入编码大于255字符信息
        install_CBitmapFont_ParseFontFile_1();
        //为字体编码添加安全距离
        install_CBitmapFont_ParseFontFile_2();
        //扩充字体文件大小上限至64MB
        install_CTextureHandler_LoadTexture();

    }
}
