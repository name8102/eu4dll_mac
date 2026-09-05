#pragma once

#include "runtime/patch/branch_resolver.h"
#include "runtime/patch/memory.h"
#include "runtime/patch/patch_runtime.h"

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
struct BatchResult {
    PatchDiagnostic diagnostic;
    std::vector<InstallationResult> installations;

    explicit operator bool() const { return diagnostic.success; }
};

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
