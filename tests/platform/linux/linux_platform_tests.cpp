#include "platform/linux/linux_elf_identity.h"
#include "platform/linux/linux_executable_allocator.h"
#include "platform/linux/linux_process_memory.h"
#include "runtime/patch/branch_resolver.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace {

void Require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void TestExecutableRegionsEnumerated() {
    eu4dll::linux_platform::LinuxProcessMemory memory;
    std::string error;
    auto regions = memory.MainModuleRegions(
        eu4dll::patch::RegionPurpose::ExecutableSearch, error);
    Require(error.empty(), error.c_str());
    Require(!regions.empty(), "main executable must expose executable PT_LOAD regions");
    for (const auto &region : regions) {
        Require(region.address != 0 && region.size != 0, "region must be valid");
    }
    auto single = memory.MainModule(error);
    Require(static_cast<bool>(single), "legacy MainModule view must succeed");
}

void TestSymbolFailureIsActionable() {
    eu4dll::linux_platform::LinuxProcessMemory memory;
    std::string error;
    const auto address =
        memory.ResolveSymbol("eu4dll_definitely_missing_symbol_xyz", error);
    Require(!address, "missing symbol must fail");
    Require(error.find("eu4dll_definitely_missing_symbol_xyz") != std::string::npos,
            "symbol failure must name the symbol");
}

void TestPermissionSafeWrite() {
    eu4dll::linux_platform::LinuxProcessMemory memory;
    // Heap-owned page: write must succeed, change bytes, and leave the page
    // usable for a second write (original permissions restored, not RX).
    alignas(4096) static std::uint8_t page[4096];
    std::memset(page, 0x90, sizeof(page));
    const auto address =
        static_cast<eu4dll::patch::Address>(reinterpret_cast<std::uintptr_t>(page));
    std::string error;
    const std::uint8_t first[4] = {0x11, 0x22, 0x33, 0x44};
    Require(memory.Write(address + 16, first, sizeof(first), error), error.c_str());
    Require(page[16] == 0x11 && page[19] == 0x44, "write must change bytes");
    const std::uint8_t second[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    Require(memory.Write(address + 16, second, sizeof(second), error), error.c_str());
    Require(page[16] == 0xAA, "second write proves permissions were restored");
    std::uint8_t back[4] = {};
    Require(memory.Read(address + 16, back, sizeof(back), error), error.c_str());
    Require(std::memcmp(back, second, sizeof(back)) == 0, "read-back must match");
}

void TestNearAllocatorAndStub() {
    eu4dll::linux_platform::LinuxNearAllocator allocator;
    const auto anchorFn = reinterpret_cast<eu4dll::patch::Address>(
        reinterpret_cast<std::uintptr_t>(&TestNearAllocatorAndStub));
    std::string error;
    // Force the trampoline path with a target far outside rel32.
    const eu4dll::patch::Address farTarget = anchorFn + 0x100000000ULL;
    const auto resolved = eu4dll::patch::ResolveBranchTarget(
        anchorFn, farTarget, eu4dll::patch::BranchKind::Jump, &allocator, error);
    Require(static_cast<bool>(resolved), error.c_str());
    Require(resolved->usesTrampoline, "far target must use a trampoline");
    Require(allocator.LiveAllocationCount() == 1, "trampoline stays alive on success");
    std::int32_t relative = 0;
    Require(eu4dll::patch::EncodeRel32(anchorFn, resolved->encodeTarget, relative),
            "trampoline must be rel32-reachable");
    // The stub must perform an absolute transfer to the final target.
    std::uint8_t stub[14] = {};
    std::memcpy(stub,
                reinterpret_cast<void *>(static_cast<std::uintptr_t>(resolved->encodeTarget)),
                sizeof(stub));
    Require(stub[0] == 0xFF && stub[1] == 0x25, "stub opcode must be FF 25");
    eu4dll::patch::Address decoded = 0;
    std::memcpy(&decoded, stub + 6, sizeof(decoded));
    Require(decoded == farTarget, "stub must reach the requested final address");
    eu4dll::patch::ReleaseResolvedBranch(&allocator, *resolved);
    Require(allocator.LiveAllocationCount() == 0, "release must unmap the trampoline");
}

void TestFileIdentityHelpers() {
    std::array<std::uint8_t, 32> digest{};
    std::string error;
    Require(eu4dll::linux_platform::ComputeFileSha256("/proc/self/exe", digest, error),
            error.c_str());
    const std::string hex = eu4dll::linux_platform::Sha256Hex(digest);
    Require(hex.size() == 64, "SHA-256 hex must be 64 chars");
    std::array<std::uint8_t, 32> reparsed{};
    Require(eu4dll::manifest::ParseSha256Hex(hex, reparsed),
            "hex digest must reparse");
    Require(reparsed == digest, "digest round trip must preserve bytes");
    Require(!eu4dll::manifest::ParseSha256Hex("xyz", reparsed),
            "malformed hex must be rejected");
}

}  // namespace

int main() {
    TestExecutableRegionsEnumerated();
    TestSymbolFailureIsActionable();
    TestPermissionSafeWrite();
    TestNearAllocatorAndStub();
    TestFileIdentityHelpers();
    std::cout << "linux platform tests passed" << std::endl;
    return 0;
}
