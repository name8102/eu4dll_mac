#include "platform/macos/macho_file.h"
#include "runtime/manifest/patch_manifest.h"
#include "runtime/patch/patch_runtime.h"
#include "targets/eu4_1_37_5/macos_x86_64/compatibility_preflight.h"

#include <chrono>
#include <iostream>
#include <limits>
#include <string>

namespace target = eu4dll::targets::eu4_1_37_5::macos_x86_64;

int main(int argc, char **argv) {
    if (argc == 4 && std::string(argv[1]) == "--validate") {
        std::string error;
        auto file = eu4dll::platform::macos::MachOFile::Open(argv[2], error);
        eu4dll::manifest::PatchManifest manifest;
        if (!file || !eu4dll::manifest::ReadFile(argv[3], manifest, error)) {
            std::cerr << "manifest validation setup failed: " << error << '\n';
            return 1;
        }
        eu4dll::platform::macos::MachOFileMemory memory(*file);
        const auto started = std::chrono::steady_clock::now();
        const auto validation = eu4dll::manifest::ValidateLoadedImage(
            manifest, file->Uuid(), file->Version(), file->ImageBase(), memory);
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started);
        if (!validation) {
            std::cerr << "manifest validation failed patch=" << validation.failedPatchId
                      << " error=" << validation.error << '\n';
            return 1;
        }
        std::cout << "manifest_valid sites=" << validation.sites.size()
                  << " validation_us=" << elapsed.count() << '\n';
        return 0;
    }
    if (argc != 3) {
        std::cerr << "usage: eu4dll_manifest_tool /path/to/eu4 /path/to/patch-manifest.bin\n"
                     "       eu4dll_manifest_tool --validate /path/to/eu4 "
                     "/path/to/patch-manifest.bin\n";
        return 2;
    }
    const auto started = std::chrono::steady_clock::now();
    std::string error;
    auto file = eu4dll::platform::macos::MachOFile::Open(argv[1], error);
    if (!file) {
        std::cerr << "manifest preflight failed: " << error << '\n';
        return 1;
    }
    eu4dll::platform::macos::MachOFileMemory memory(*file);
    const auto compatibility = target::PreflightCompatibility(memory);
    if (!compatibility) {
        for (const auto &failure : compatibility.failures) {
            std::cerr << eu4dll::patch::FormatDiagnostic(failure) << '\n';
        }
        std::cerr << "manifest was not written; the installed EU4 executable is not "
                     "capable of every required patch\n";
        return 1;
    }

    eu4dll::manifest::PatchManifest manifest;
    manifest.SetUuid(file->Uuid());
    manifest.gameVersion = file->Version();
    manifest.versionRva = file->VersionRva();
    eu4dll::patch::PatchRuntime runtime(memory);
    for (const auto &contract : target::CompatibilityPatchRegistry()) {
        const auto located = runtime.Locate(contract.description.location,
                                            contract.id, contract.description.target);
        if (!located || located.address < file->ImageBase()) {
            std::cerr << "manifest address conversion failed for " << contract.id << '\n';
            return 1;
        }
        const auto mutation = contract.description.mutation.offset;
        if ((mutation < 0 && static_cast<std::uint64_t>(-(mutation + 1)) + 1 > located.address) ||
            (mutation >= 0 && static_cast<std::uint64_t>(mutation) >
                                  std::numeric_limits<std::uint64_t>::max() - located.address)) {
            std::cerr << "manifest mutation offset overflow for " << contract.id << '\n';
            return 1;
        }
        const std::uint64_t mutationAddress = mutation < 0
            ? located.address - (static_cast<std::uint64_t>(-(mutation + 1)) + 1)
            : located.address + static_cast<std::uint64_t>(mutation);
        eu4dll::manifest::PatchEntry entry;
        entry.id = contract.id;
        entry.siteRva = located.address - file->ImageBase();
        entry.expectedOffset = mutation;
        entry.mutationOffset = mutation;
        entry.overwriteWidth = static_cast<std::uint32_t>(contract.overwriteWidth);
        entry.expectedBytes.resize(contract.overwriteWidth);
        if (!memory.Read(mutationAddress, entry.expectedBytes.data(),
                         entry.expectedBytes.size(), error)) {
            std::cerr << "could not capture overwritten bytes for " << contract.id
                      << ": " << error << '\n';
            return 1;
        }
        for (const auto &continuation : contract.description.continuations) {
            entry.continuations.emplace_back(continuation.name, continuation.offset);
        }
        entry.optimizeHook = contract.description.optimization.enabled;
        manifest.entries.push_back(std::move(entry));
    }
    if (!eu4dll::manifest::WriteAtomically(argv[2], manifest, error)) {
        std::cerr << "could not write manifest: " << error << '\n';
        return 1;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);
    std::cout << "manifest=" << argv[2] << " version=" << manifest.gameVersion
              << " sites=" << manifest.entries.size()
              << " symbols=" << compatibility.checkedSymbols
              << " scan_ms=" << elapsed.count() << '\n';
    return 0;
}
