#include <CoreFoundation/CoreFoundation.h>
#include "./src/base.h"
#include "./src/mainText.h"
#include "./src/tooltipAndButtonText.h"
#include "./src/mapText.h"
#include "./src/text3D.h"
#include "./src/textLayout.h"
#include "./src/saveFileName.h"
#include "./src/input.h"
#include "./src/localization.h"
#include "./src/platform/macos/live_patch_runtime.h"
#include "./src/runtime/diagnostics/startup_diagnostics.h"
#include "./src/runtime/manifest/patch_manifest.h"
#include "./src/targets/eu4_1_37_5/macos_x86_64/compatibility_preflight.h"
#include "./src/targets/eu4_1_37_5/macos_x86_64/hook_symbols.h"
#include "./src/targets/eu4_1_37_5/macos_x86_64/profile.h"
#include "./src/targets/eu4_1_37_5/macos_x86_64/target_facts.h"
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <algorithm>
#include <array>
#include <limits.h>
#include <memory>

namespace {
std::unique_ptr<eu4dll::manifest::ManifestSiteProvider> g_manifestSites;

bool LoadedUuid(std::array<std::uint8_t, 16> &uuid, uintptr_t &imageBase) {
    const auto *header = reinterpret_cast<const mach_header_64 *>(
        _dyld_get_image_header(0));
    if (header == nullptr || header->magic != MH_MAGIC_64) return false;
    imageBase = reinterpret_cast<uintptr_t>(header);
    const auto *command = reinterpret_cast<const load_command *>(header + 1);
    for (std::uint32_t index = 0; index < header->ncmds; ++index) {
        if (command->cmd == LC_UUID && command->cmdsize == sizeof(uuid_command)) {
            const auto *uuidCommand = reinterpret_cast<const uuid_command *>(command);
            std::copy(std::begin(uuidCommand->uuid), std::end(uuidCommand->uuid), uuid.begin());
            return true;
        }
        command = reinterpret_cast<const load_command *>(
            reinterpret_cast<const std::uint8_t *>(command) + command->cmdsize);
    }
    return false;
}

std::string ResourcePath(const char *name) {
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (bundle == nullptr) return {};
    CFURLRef resources = CFBundleCopyResourcesDirectoryURL(bundle);
    if (resources == nullptr) return {};
    char path[PATH_MAX]{};
    const bool ok = CFURLGetFileSystemRepresentation(
        resources, true, reinterpret_cast<UInt8 *>(path), sizeof(path));
    CFRelease(resources);
    return ok ? std::string(path) + "/" + name : std::string{};
}

bool ManifestMatchesCanonicalRegistry(const eu4dll::manifest::PatchManifest &manifest,
                                      std::string &error) {
    namespace target = eu4dll::targets::eu4_1_37_5::macos_x86_64;
    const auto &registry = target::CompatibilityPatchRegistry();
    if (manifest.entries.size() != registry.size()) {
        error = "patch manifest descriptor count is stale; rerun the installer";
        return false;
    }
    for (const auto &contract : registry) {
        const auto found = std::find_if(manifest.entries.begin(), manifest.entries.end(),
            [&contract](const auto &entry) { return entry.id == contract.id; });
        if (found == manifest.entries.end() ||
            found->overwriteWidth != contract.overwriteWidth ||
            found->mutationOffset != contract.description.mutation.offset ||
            found->expectedOffset != contract.description.mutation.offset ||
            found->optimizeHook != contract.description.optimization.enabled ||
            found->continuations.size() != contract.description.continuations.size()) {
            error = "patch manifest descriptor contract is stale at " + contract.id +
                    "; rerun the installer";
            return false;
        }
        for (const auto &continuation : contract.description.continuations) {
            const auto continuationFound = std::find_if(
                found->continuations.begin(), found->continuations.end(),
                [&continuation](const auto &value) {
                    return value.first == continuation.name &&
                           value.second == continuation.offset;
                });
            if (continuationFound == found->continuations.end()) {
                error = "patch manifest continuation contract is stale at " + contract.id +
                        "; rerun the installer";
                return false;
            }
        }
    }
    return true;
}

void RecordManifestFailure(eu4dll::diagnostics::StartupDiagnostics &diagnostics,
                           const std::string &message) {
    eu4dll::patch::PatchDiagnostic diagnostic;
    diagnostic.feature = "install-manifest";
    diagnostic.target =
        eu4dll::targets::eu4_1_37_5::macos_x86_64::kDiagnosticTargetId;
    diagnostic.operation = eu4dll::patch::PatchOperation::ValidateDescription;
    diagnostic.message = message;
    diagnostics.Record(std::move(diagnostic));
}
void ShowErrorMessageBox(const char *title, const char *msg) {
    CFStringRef cfTitle = CFStringCreateWithCString(NULL, title, kCFStringEncodingUTF8);
    CFStringRef cfMsg = CFStringCreateWithCString(NULL, msg, kCFStringEncodingUTF8);
    CFOptionFlags result;
    CFUserNotificationDisplayAlert(
            0, kCFUserNotificationPlainAlertLevel,
            NULL, NULL, NULL,
            cfTitle, cfMsg,
            CFSTR("exit"), CFSTR("continue"), NULL,
            &result
    );
    if (cfTitle) CFRelease(cfTitle);
    if (cfMsg) CFRelease(cfMsg);
    if (result == 0) {
        exit(0);
    }
}

// 动态库加载时的构造函数
__attribute__((constructor))
static void ctor() {
    printf("eu4dll_mac [Main] 开始加载所有 HOOK 插件...\n");
    auto &diagnostics = eu4dll::diagnostics::StartupDiagnostics::Instance();
    diagnostics.Reset();

    namespace target = eu4dll::targets::eu4_1_37_5::macos_x86_64;
    eu4dll::manifest::PatchManifest manifest;
    std::string manifestError;
    const auto manifestPath = ResourcePath("eu4dll-patch-manifest.bin");
    std::array<std::uint8_t, 16> loadedUuid{};
    uintptr_t loadedImageBase = 0;
    if (manifestPath.empty()) manifestError = "could not locate the app Resources directory";
    if (manifestError.empty()) {
        const bool manifestRead =
            eu4dll::manifest::ReadFile(manifestPath, manifest, manifestError);
        if (!manifestRead && manifestError.empty()) manifestError = "could not read manifest";
    }
    if (manifestError.empty() && !LoadedUuid(loadedUuid, loadedImageBase)) {
        manifestError = "could not read LC_UUID from the loaded executable";
    }
    if (manifestError.empty()) {
        const bool registryMatches = ManifestMatchesCanonicalRegistry(manifest, manifestError);
        if (!registryMatches && manifestError.empty()) {
            manifestError = "manifest does not match the canonical patch registry";
        }
    }
    if (!manifestError.empty()) {
        RecordManifestFailure(diagnostics, manifestError + "; rerun the installer");
        const auto report = diagnostics.FormatReport(
            manifest.gameVersion.empty() ? "unknown" : manifest.gameVersion);
        ShowErrorMessageBox("eu4dll_mac startup failure", report.c_str());
        return;
    }
    const auto validatedSites = eu4dll::manifest::ValidateLoadedImage(
        manifest, loadedUuid, {}, loadedImageBase,
        eu4dll::platform::macos::LiveProcessMemory());
    if (!validatedSites) {
        RecordManifestFailure(diagnostics, validatedSites.error + " patch=" +
            validatedSites.failedPatchId + "; rerun the installer");
        const auto report = diagnostics.FormatReport(manifest.gameVersion);
        ShowErrorMessageBox("eu4dll_mac startup failure", report.c_str());
        return;
    }

    bool requiredSymbolsPresent = true;
    std::string symbolError;
    for (const char *symbol : target::symbols::kRequiredSymbols) {
        if (eu4dll::platform::macos::LiveProcessMemory().ResolveSymbol(symbol, symbolError)) {
            continue;
        }
        eu4dll::patch::PatchDiagnostic diagnostic;
        diagnostic.feature = std::string("target.symbol.") + symbol;
        diagnostic.target = target::kDiagnosticTargetId;
        diagnostic.operation = eu4dll::patch::PatchOperation::ResolveSymbol;
        diagnostic.message = symbolError;
        diagnostics.Record(std::move(diagnostic));
        requiredSymbolsPresent = false;
    }
    if (!requiredSymbolsPresent) {
        const auto report = diagnostics.FormatReport(manifest.gameVersion);
        ShowErrorMessageBox("eu4dll_mac startup failure", report.c_str());
        return;
    }
    g_manifestSites = std::make_unique<eu4dll::manifest::ManifestSiteProvider>(validatedSites);
    eu4dll::platform::macos::LivePatchRuntime().SetResolvedSiteProvider(g_manifestSites.get());
    const auto &eu4VerStr = manifest.gameVersion;
    printf("eu4dll_mac [Main] eu4版本号：%s\n", eu4VerStr.c_str());

    printf("eu4dll_mac [Main] manifest通过：%zu sites\n", validatedSites.sites.size());

    if (!target::hook_symbols::ResolveRequiredSymbols()) {
        const auto report = diagnostics.FormatReport(eu4VerStr);
        ShowErrorMessageBox("eu4dll_mac startup failure", report.c_str());
        return;
    }

    // 支持的基础
    base::install();
    // 文字排版
    textLayout::install();
    // 主要文本显示
    mainText::install();
    // 浮动提示和部分按钮
    tooltipAndButtonText::install();
    // 地图文本
    mapText::install();
    // 战斗3D文本
    text3D::install();
    // 存档名
    saveFileName::install();
    // 允许输入非ASCII编码
    input::install();
    // 符合东亚文化的本地化修改
    localization::install();

    printf("eu4dll_mac [Main] 所有插件加载完毕！\n");

    if (diagnostics.HasFailures()) {
        const auto report = diagnostics.FormatReport(eu4VerStr);
        ShowErrorMessageBox("eu4dll_mac startup failure", report.c_str());
    }
}
} // namespace
