#pragma once

#include "runtime/patch/branch_resolver.h"
#include "runtime/patch/memory.h"
#include "runtime/patch/patch_runtime.h"

#include <cstdint>
#include <string>
#include <vector>

namespace eu4dll::patch {

// Platform-independent atomic batch around existing patch descriptions.
// Usage: Add() every PatchDescription in a feature group, optionally
// Preflight() for a dry run, then Commit() to install all writes atomically.
// Before any write, every site is located, every expected-byte contract is
// verified, branch targets are resolved (allocating near trampolines only at
// commit time), originals are snapshotted, and overlapping staged writes are
// rejected. If any write fails after earlier writes, already-applied bytes
// are restored in reverse order and rollback failures are reported
// distinctly. Staged trampolines are released on failure and kept alive on
// success (owned by the allocator for the hook lifetime).
// Rollback outcome for slot-lifetime decisions. Adapters must retain
// hook slots while Unconfirmed: game code may still be inside a hook
// whose trampoline the batch intentionally kept mapped.
enum class RollbackState : std::uint8_t {
    NotNeeded = 0,  // nothing was applied; no rollback ran
    Complete = 1,   // rollback ran and every restore verified
    Unconfirmed = 2  // at least one restore could not be verified
};

struct BatchResult {
    PatchDiagnostic diagnostic;
    std::vector<InstallationResult> installations;
    RollbackState rollbackState = RollbackState::NotNeeded;

    explicit operator bool() const { return diagnostic.success; }
};

// Slot-clearing rule shared by every target adapter: hook continuation
// and callee slots may be cleared unless an unconfirmed rollback may still
// have game code inside a hook (the same fail-safe model as trampoline
// retention). Check this BEFORE clearing on any failed install.
inline bool MustRetainSlots(const BatchResult &result) {
    return !static_cast<bool>(result) &&
           result.rollbackState == RollbackState::Unconfirmed;
}

class PatchBatch {
public:
    explicit PatchBatch(Memory &memory, ExecutableCodeAllocator *allocator = nullptr);
    void SetResolvedSiteProvider(const ResolvedSiteProvider *provider);
    void SetAllocator(ExecutableCodeAllocator *allocator) { allocator_ = allocator; }
    void Add(PatchDescription description);
    void Clear();

    // Dry run: locate, verify expected bytes, check branch reachability
    // (assuming the allocator can provide a trampoline when needed), and
    // reject overlaps. Performs reads but zero writes and zero allocations.
    BatchResult Preflight() const;
    // Full atomic install. Returns per-patch continuations only on success.
    BatchResult Commit();

private:
    Memory &memory_;
    ExecutableCodeAllocator *allocator_;
    const ResolvedSiteProvider *siteProvider_ = nullptr;
    std::vector<PatchDescription> descriptions_;
};

}  // namespace eu4dll::patch
