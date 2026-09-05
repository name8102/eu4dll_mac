#include "runtime/patch/branch_resolver.h"

#include <cstring>
#include <limits>

namespace eu4dll::patch {

std::array<std::uint8_t, kAbsoluteJumpStubSize> BuildAbsoluteJumpStub(
    Address finalTarget) {
    std::array<std::uint8_t, kAbsoluteJumpStubSize> stub{
        {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
         0, 0, 0, 0, 0, 0, 0, 0}};
    std::memcpy(stub.data() + 6, &finalTarget, sizeof(finalTarget));
    return stub;
}

bool IsRel32Reachable(Address instruction, Address target) {
    std::int32_t relative = 0;
    return EncodeRel32(instruction, target, relative);
}

bool EncodeRel32(Address instruction, Address target, std::int32_t &relative) {
    if (instruction > std::numeric_limits<Address>::max() - 5) {
        return false;
    }
    const Address next = instruction + 5;
    if (target >= next) {
        const Address distance = target - next;
        if (distance > static_cast<Address>(std::numeric_limits<std::int32_t>::max())) {
            return false;
        }
        relative = static_cast<std::int32_t>(distance);
        return true;
    }
    const Address distance = next - target;
    const Address negativeLimit =
        static_cast<Address>(std::numeric_limits<std::int32_t>::max()) + 1;
    if (distance > negativeLimit) {
        return false;
    }
    relative = distance == negativeLimit
                   ? std::numeric_limits<std::int32_t>::min()
                   : -static_cast<std::int32_t>(distance);
    return true;
}

std::optional<ResolvedBranch> ResolveBranchTarget(
    Address instruction, Address requestedTarget, BranchKind /*kind*/,
    ExecutableCodeAllocator *allocator, std::string &error) {
    if (requestedTarget == 0) {
        error = "branch target must be non-zero";
        return std::nullopt;
    }
    std::int32_t direct = 0;
    if (EncodeRel32(instruction, requestedTarget, direct)) {
        ResolvedBranch resolved;
        resolved.encodeTarget = requestedTarget;
        resolved.usesTrampoline = false;
        return resolved;
    }
    if (allocator == nullptr) {
        error = "control-flow target is outside the signed rel32 range";
        return std::nullopt;
    }
    constexpr std::size_t kPageSize = 4096;
    auto trampoline = allocator->AllocateNear(instruction, kPageSize, error);
    if (!trampoline) {
        return std::nullopt;
    }
    const auto stub = BuildAbsoluteJumpStub(requestedTarget);
    // The allocation is owned RW memory (not game code), so a direct copy is
    // safe here; MakeExecutable transitions it to RX below.
    std::memcpy(reinterpret_cast<void *>(static_cast<std::uintptr_t>(*trampoline)),
                stub.data(), stub.size());
#if defined(__x86_64__) || defined(__aarch64__)
    __builtin___clear_cache(
        reinterpret_cast<char *>(static_cast<std::uintptr_t>(*trampoline)),
        reinterpret_cast<char *>(static_cast<std::uintptr_t>(*trampoline + stub.size())));
#endif
    if (!allocator->MakeExecutable(*trampoline, kPageSize, error)) {
        allocator->Release(*trampoline, kPageSize);
        return std::nullopt;
    }
    // The trampoline must itself be rel32-reachable from the patch site;
    // the allocator guarantees near placement, but verify defensively.
    std::int32_t viaTrampoline = 0;
    if (!EncodeRel32(instruction, *trampoline, viaTrampoline)) {
        error = "allocated trampoline is outside the signed rel32 range";
        allocator->Release(*trampoline, kPageSize);
        return std::nullopt;
    }
    ResolvedBranch resolved;
    resolved.encodeTarget = *trampoline;
    resolved.usesTrampoline = true;
    resolved.trampolineAddress = *trampoline;
    resolved.trampolineSize = kPageSize;
    return resolved;
}

void ReleaseResolvedBranch(ExecutableCodeAllocator *allocator,
                           const ResolvedBranch &resolved) {
    if (allocator == nullptr || !resolved.usesTrampoline) return;
    allocator->Release(resolved.trampolineAddress, resolved.trampolineSize);
}

}  // namespace eu4dll::patch
