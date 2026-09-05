#pragma once

#include "runtime/patch/memory.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace eu4dll::patch {

// Narrow allocator boundary so portable feature code never embeds mmap policy.
// The allocator reserves executable-capable memory near a patch site; the
// shared resolver writes the absolute transfer stub into it.
class ExecutableCodeAllocator {
public:
    virtual ~ExecutableCodeAllocator() = default;
    // Reserves `size` bytes within signed rel32 range of `anchor` with at
    // least read+write permission. Returns the base address on success.
    virtual std::optional<Address> AllocateNear(
        Address anchor, std::size_t size, std::string &error) = 0;
    // Transitions a previously allocated range to read+execute after the stub
    // has been written. Returns false with a diagnostic on failure.
    virtual bool MakeExecutable(Address address, std::size_t size,
                                std::string &error) = 0;
    virtual void Release(Address address, std::size_t size) = 0;
};

enum class BranchKind {
    Jump,
    Call,
};

struct ResolvedBranch {
    // Address the game-site rel32 should encode (final target when directly
    // reachable, otherwise the near trampoline).
    Address encodeTarget = 0;
    bool usesTrampoline = false;
    Address trampolineAddress = 0;
    std::size_t trampolineSize = 0;
};

inline constexpr std::size_t kAbsoluteJumpStubSize = 14;

// x86-64 absolute indirect transfer used by Linux near trampolines:
//   FF 25 00 00 00 00 + <8-byte absolute target>
std::array<std::uint8_t, kAbsoluteJumpStubSize> BuildAbsoluteJumpStub(
    Address finalTarget);

bool IsRel32Reachable(Address instruction, Address target);
bool EncodeRel32(Address instruction, Address target, std::int32_t &relative);

// Resolves a hook/call destination to a directly encodable rel32 target.
// When `requestedTarget` is outside rel32 range and `allocator` is provided,
// a near trampoline is allocated, the absolute stub is written into it, and
// the trampoline address is returned as the encode target. Directly reachable
// targets allocate nothing. Allocation failures are reported without mutation.
std::optional<ResolvedBranch> ResolveBranchTarget(
    Address instruction, Address requestedTarget, BranchKind kind,
    ExecutableCodeAllocator *allocator, std::string &error);

// Releases a previously resolved trampoline, if any. Successful hook
// installations must keep trampolines alive for the hook lifetime instead.
void ReleaseResolvedBranch(ExecutableCodeAllocator *allocator,
                           const ResolvedBranch &resolved);

}  // namespace eu4dll::patch
