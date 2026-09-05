#include "runtime/patch/patch_batch.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>
#include <utility>

namespace eu4dll::patch {
namespace {

PatchDiagnostic Failure(const std::string &feature, const std::string &target,
                        PatchOperation operation, std::string message) {
    PatchDiagnostic diagnostic;
    diagnostic.feature = feature;
    diagnostic.target = target;
    diagnostic.operation = operation;
    diagnostic.message = std::move(message);
    return diagnostic;
}

std::optional<Address> AddOffset(Address address, std::ptrdiff_t offset) {
    if (offset >= 0) {
        const auto positive = static_cast<Address>(offset);
        if (positive > std::numeric_limits<Address>::max() - address) {
            return std::nullopt;
        }
        return address + positive;
    }
    const auto magnitude = static_cast<Address>(-(offset + 1)) + 1;
    if (magnitude > address) {
        return std::nullopt;
    }
    return address - magnitude;
}

struct StagedPatch {
    const PatchDescription *description = nullptr;
    Address siteAddress = 0;
    Address mutationAddress = 0;
    std::vector<std::uint8_t> original;
    std::vector<std::uint8_t> payload;
    std::optional<ResolvedBranch> branch;
    std::unordered_map<std::string, Address> continuations;
    MatchStatus match = MatchStatus::NotSearched;
    std::size_t matchCount = 0;
    // Set from WriteResult::bytesWritten: a failed Write may still have
    // mutated the target (e.g. protection-restore failure after memcpy), so
    // rollback membership is decided by this flag, never by call success.
    bool mutationApplied = false;
    // Set only after read-back verification proves the original bytes are
    // live again. Trampoline release depends ONLY on this flag: once the
    // original jump/call bytes are back, game code no longer references the
    // trampoline, whatever the page permissions are.
    bool rollbackBytesConfirmed = false;
    // Mirrors WriteResult::protectionRestored of the restore attempt. Bytes
    // and permissions are reported independently: a restore can bring the
    // bytes back while leaving the code page writable, and the batch must
    // not claim a complete rollback in that case.
    bool rollbackProtectionRestored = true;
    bool trampolineReleased = false;
};

// Releases one staged trampoline iff no live game code can reference it:
// the site was never mutated, or the original bytes are read-back confirmed
// live again. Page permissions play no role here: an RWX-but-original site
// jumps to the old target, not the trampoline. A mutated-but-unconfirmed
// site may still jump at the trampoline, so the page is intentionally
// leaked to process exit instead of munmap-SIGSEGV.
void ReleaseEntryTrampoline(ExecutableCodeAllocator *allocator, StagedPatch &entry) {
    if (allocator == nullptr || !entry.branch.has_value() || entry.trampolineReleased) {
        return;
    }
    if (entry.mutationApplied && !entry.rollbackBytesConfirmed) return;
    ReleaseResolvedBranch(allocator, *entry.branch);
    entry.trampolineReleased = true;
}

void ReleaseAllStagedTrampolines(ExecutableCodeAllocator *allocator,
                                 std::vector<StagedPatch> &staged) {
    for (auto &entry : staged) ReleaseEntryTrampoline(allocator, entry);
}

struct RollbackOutcome {
    std::vector<std::string> unconfirmed;
    std::string firstError;
    // Entries whose bytes verified but whose restore left page protections
    // unrestored. Their trampolines are still safe to release (the original
    // jump is back), but the batch must not report a complete rollback.
    std::vector<std::string> unrestoredProtections;
    std::size_t retainedTrampolines = 0;
    // Whether any entry actually needed a restore attempt. Without it the
    // batch state stays NotNeeded instead of Complete.
    bool attemptedAny = false;
};

// Restores originals for every applied entry in reverse order, verifies each
// restore with read-back, then runs the conditional trampoline release over
// the whole staged set (unapplied entries release unconditionally).
RollbackOutcome RollbackStaged(Memory &memory, ExecutableCodeAllocator *allocator,
                               std::vector<StagedPatch> &staged) {
    RollbackOutcome outcome;
    for (auto it = staged.rbegin(); it != staged.rend(); ++it) {
        if (!it->mutationApplied || it->rollbackBytesConfirmed) continue;
        outcome.attemptedAny = true;
        const auto rewritten =
            memory.Write(it->mutationAddress, it->original.data(), it->original.size());
        it->rollbackProtectionRestored = rewritten.protectionRestored;
        std::vector<std::uint8_t> current(it->original.size());
        std::string readError;
        const bool confirmed =
            memory.Read(it->mutationAddress, current.data(), current.size(), readError) &&
            current == it->original;
        if (confirmed) {
            it->rollbackBytesConfirmed = true;
        } else {
            outcome.unconfirmed.push_back(it->description->feature);
            if (outcome.firstError.empty()) {
                if (!rewritten.ok()) {
                    outcome.firstError = rewritten.error;
                } else if (!readError.empty()) {
                    outcome.firstError = readError;
                } else {
                    outcome.firstError =
                        "restored bytes do not match the original snapshot";
                }
            }
        }
    }
    ReleaseAllStagedTrampolines(allocator, staged);
    for (auto &entry : staged) {
        if (entry.mutationApplied && entry.rollbackBytesConfirmed &&
            !entry.rollbackProtectionRestored) {
            outcome.unrestoredProtections.push_back(entry.description->feature);
        }
    }
    for (const auto &entry : staged) {
        if (entry.branch.has_value() && entry.mutationApplied &&
            !entry.rollbackBytesConfirmed && !entry.trampolineReleased) {
            ++outcome.retainedTrampolines;
        }
    }
    return outcome;
}

void AnnotateRollback(BatchResult &result, const RollbackOutcome &outcome,
                      std::size_t appliedCount) {
    auto &diagnostic = result.diagnostic;
    if (!outcome.unconfirmed.empty()) {
        diagnostic.operation = PatchOperation::Rollback;
        std::ostringstream stream;
        stream << diagnostic.message << "; rollback UNCONFIRMED at [";
        for (std::size_t i = 0; i < outcome.unconfirmed.size(); ++i) {
            if (i != 0) stream << ", ";
            stream << outcome.unconfirmed[i];
        }
        stream << "]: " << outcome.firstError;
        if (outcome.retainedTrampolines > 0) {
            stream << "; " << outcome.retainedTrampolines
                   << " trampoline(s) intentionally retained (fail-safe leak, not unmapped)";
        }
        diagnostic.message = stream.str();
        result.rollbackState = RollbackState::Unconfirmed;
        return;
    }
    diagnostic.message += "; rollback restored " + std::to_string(appliedCount) +
                          " applied site(s)";
    // Bytes are verifiably back, so trampolines were released correctly;
    // but unrestored page protections must not be reported as a complete
    // rollback.
    if (!outcome.unrestoredProtections.empty()) {
        std::ostringstream stream;
        stream << diagnostic.message << "; page protections NOT restored at [";
        for (std::size_t i = 0; i < outcome.unrestoredProtections.size(); ++i) {
            if (i != 0) stream << ", ";
            stream << outcome.unrestoredProtections[i];
        }
        stream << "]";
        diagnostic.message = stream.str();
    }
    result.rollbackState =
        outcome.attemptedAny ? RollbackState::Complete : RollbackState::NotNeeded;
}

std::size_t CountApplied(const std::vector<StagedPatch> &staged) {
    return static_cast<std::size_t>(
        std::count_if(staged.begin(), staged.end(),
                      [](const StagedPatch &entry) { return entry.mutationApplied; }));
}

// Computes the mutation address and payload size without allocating.
// For Call/Auto the width is determined by reading the live opcode.
bool ComputeMutationSpan(Memory &memory, const PatchDescription &description,
                         Address siteAddress, Address &mutationAddress,
                         std::size_t &payloadSize, std::string &error) {
    const auto mutated = AddOffset(siteAddress, description.mutation.offset);
    if (!mutated) {
        error = "mutation offset overflows the address space";
        return false;
    }
    mutationAddress = *mutated;
    switch (description.mutation.kind) {
        case MutationKind::RawBytes:
            if (description.mutation.bytes.empty()) {
                error = "raw mutation requires at least one byte";
                return false;
            }
            payloadSize = description.mutation.bytes.size();
            return true;
        case MutationKind::Jump:
            if (description.mutation.target == 0) {
                error = "control-flow mutation requires a non-zero target";
                return false;
            }
            payloadSize = 5;
            return true;
        case MutationKind::Call: {
            if (description.mutation.target == 0) {
                error = "control-flow mutation requires a non-zero target";
                return false;
            }
            if (description.mutation.callWidth == CallWidth::FiveBytes) {
                payloadSize = 5;
                return true;
            }
            if (description.mutation.callWidth == CallWidth::SixBytes) {
                payloadSize = 6;
                return true;
            }
            std::uint8_t opcode[2] = {};
            if (!memory.Read(mutationAddress, opcode, sizeof(opcode), error)) {
                error = "could not inspect original CALL encoding: " + error;
                return false;
            }
            payloadSize = opcode[0] == 0xFF ? 6 : 5;
            return true;
        }
    }
    error = "unknown mutation kind";
    return false;
}

std::vector<std::uint8_t> BuildBranchPayload(BranchKind kind, std::int32_t relative,
                                             std::size_t width) {
    std::vector<std::uint8_t> payload(width, 0x90);
    payload[0] = (kind == BranchKind::Jump) ? 0xE9 : 0xE8;
    std::memcpy(payload.data() + 1, &relative, sizeof(relative));
    return payload;
}

bool RangesOverlap(Address first, std::size_t firstSize, Address second,
                   std::size_t secondSize) {
    if (firstSize == 0 || secondSize == 0) return false;
    const Address firstEnd = first + firstSize;
    const Address secondEnd = second + secondSize;
    if (firstEnd < first || secondEnd < second) return true;  // overflow: reject
    return first < secondEnd && second < firstEnd;
}

}  // namespace

PatchBatch::PatchBatch(Memory &memory, ExecutableCodeAllocator *allocator)
    : memory_(memory), allocator_(allocator) {}

void PatchBatch::SetResolvedSiteProvider(const ResolvedSiteProvider *provider) {
    siteProvider_ = provider;
}

void PatchBatch::Add(PatchDescription description) {
    descriptions_.push_back(std::move(description));
}

void PatchBatch::Clear() {
    descriptions_.clear();
}

BatchResult PatchBatch::Preflight() const {
    BatchResult result;
    if (descriptions_.empty()) {
        result.diagnostic =
            Failure({}, {}, PatchOperation::ValidateDescription,
                    "patch batch has no writes");
        return result;
    }
    PatchRuntime runtime(const_cast<Memory &>(memory_));
    runtime.SetResolvedSiteProvider(siteProvider_);

    std::vector<std::pair<Address, std::size_t>> spans;
    spans.reserve(descriptions_.size());
    for (const auto &description : descriptions_) {
        if (description.feature.empty() || description.target.empty()) {
            result.diagnostic = Failure(
                description.feature, description.target,
                PatchOperation::ValidateDescription,
                "patch description requires feature and target identifiers");
            return result;
        }
        const auto preflight = runtime.Preflight(description);
        if (!preflight) {
            result.diagnostic = preflight.diagnostic;
            return result;
        }
        const Address site = preflight.diagnostic.matchAddress;
        Address mutationAddress = 0;
        std::size_t payloadSize = 0;
        std::string error;
        if (!ComputeMutationSpan(const_cast<Memory &>(memory_), description, site,
                                 mutationAddress, payloadSize, error)) {
            result.diagnostic =
                Failure(description.feature, description.target,
                        PatchOperation::CalculateMutation, error);
            result.diagnostic.match = preflight.diagnostic.match;
            result.diagnostic.matchCount = preflight.diagnostic.matchCount;
            result.diagnostic.matchAddress = site;
            return result;
        }
        // Branch reachability check without allocating: direct rel32 is fine,
        // otherwise an allocator must be present for commit time.
        if (description.mutation.kind != MutationKind::RawBytes) {
            const BranchKind kind = description.mutation.kind == MutationKind::Jump
                                        ? BranchKind::Jump
                                        : BranchKind::Call;
            (void)kind;
            std::int32_t relative = 0;
            if (!EncodeRel32(mutationAddress, description.mutation.target, relative) &&
                allocator_ == nullptr) {
                result.diagnostic = Failure(
                    description.feature, description.target,
                    PatchOperation::CalculateMutation,
                    "control-flow target is outside the signed rel32 range");
                result.diagnostic.match = preflight.diagnostic.match;
                result.diagnostic.matchCount = preflight.diagnostic.matchCount;
                result.diagnostic.matchAddress = site;
                result.diagnostic.mutationAddress = mutationAddress;
                return result;
            }
        }
        for (const auto &span : spans) {
            if (RangesOverlap(span.first, span.second, mutationAddress, payloadSize)) {
                result.diagnostic = Failure(
                    description.feature, description.target,
                    PatchOperation::ValidateDescription,
                    "patch batch contains overlapping writes");
                result.diagnostic.matchAddress = site;
                result.diagnostic.mutationAddress = mutationAddress;
                return result;
            }
        }
        spans.emplace_back(mutationAddress, payloadSize);
    }
    result.diagnostic.success = true;
    result.diagnostic.feature = descriptions_.front().feature;
    result.diagnostic.target = descriptions_.front().target;
    result.diagnostic.operation = PatchOperation::VerifyOriginalBytes;
    result.diagnostic.match = MatchStatus::Unique;
    result.diagnostic.matchCount = descriptions_.size();
    std::ostringstream message;
    message << "batch preflight passed for " << descriptions_.size() << " patch(es)";
    result.diagnostic.message = message.str();
    return result;
}

BatchResult PatchBatch::Commit() {
    BatchResult result;
    if (descriptions_.empty()) {
        result.diagnostic =
            Failure({}, {}, PatchOperation::ValidateDescription,
                    "patch batch has no writes");
        return result;
    }
    PatchRuntime runtime(memory_);
    runtime.SetResolvedSiteProvider(siteProvider_);

    // Phase 1: locate + verify + resolve + snapshot (zero writes).
    // Nothing is mutated yet, so any staged trampoline can be released
    // unconditionally on the failure paths below.
    std::vector<StagedPatch> staged;
    staged.reserve(descriptions_.size());
    for (const auto &description : descriptions_) {
        if (description.feature.empty() || description.target.empty()) {
            result.diagnostic = Failure(
                description.feature, description.target,
                PatchOperation::ValidateDescription,
                "patch description requires feature and target identifiers");
            ReleaseAllStagedTrampolines(allocator_, staged);
            return result;
        }
        const auto preflight = runtime.Preflight(description);
        if (!preflight) {
            result.diagnostic = preflight.diagnostic;
            ReleaseAllStagedTrampolines(allocator_, staged);
            return result;
        }
        const Address site = preflight.diagnostic.matchAddress;
        Address mutationAddress = 0;
        std::size_t payloadSize = 0;
        std::string error;
        if (!ComputeMutationSpan(memory_, description, site, mutationAddress,
                                 payloadSize, error)) {
            result.diagnostic =
                Failure(description.feature, description.target,
                        PatchOperation::CalculateMutation, error);
            result.diagnostic.match = preflight.diagnostic.match;
            result.diagnostic.matchCount = preflight.diagnostic.matchCount;
            result.diagnostic.matchAddress = site;
            ReleaseAllStagedTrampolines(allocator_, staged);
            return result;
        }
        StagedPatch entry;
        entry.description = &description;
        entry.siteAddress = site;
        entry.mutationAddress = mutationAddress;
        entry.continuations = preflight.continuations;
        entry.match = preflight.diagnostic.match;
        entry.matchCount = preflight.diagnostic.matchCount;

        if (description.mutation.kind == MutationKind::RawBytes) {
            entry.payload = description.mutation.bytes;
        } else {
            const BranchKind kind = description.mutation.kind == MutationKind::Jump
                                        ? BranchKind::Jump
                                        : BranchKind::Call;
            auto resolved = ResolveBranchTarget(mutationAddress,
                                                description.mutation.target, kind,
                                                allocator_, error);
            if (!resolved) {
                result.diagnostic = Failure(description.feature, description.target,
                                            PatchOperation::StageBranch, error);
                result.diagnostic.match = entry.match;
                result.diagnostic.matchCount = entry.matchCount;
                result.diagnostic.matchAddress = site;
                result.diagnostic.mutationAddress = mutationAddress;
                ReleaseAllStagedTrampolines(allocator_, staged);
                return result;
            }
            std::int32_t relative = 0;
            if (!EncodeRel32(mutationAddress, resolved->encodeTarget, relative)) {
                result.diagnostic = Failure(
                    description.feature, description.target,
                    PatchOperation::CalculateMutation,
                    "resolved branch target is outside the signed rel32 range");
                result.diagnostic.matchAddress = site;
                result.diagnostic.mutationAddress = mutationAddress;
                ReleaseResolvedBranch(allocator_, *resolved);
                ReleaseAllStagedTrampolines(allocator_, staged);
                return result;
            }
            entry.payload = BuildBranchPayload(kind, relative, payloadSize);
            if (resolved->usesTrampoline) {
                entry.branch = *resolved;
            }
        }
        // Snapshot originals for race detection and rollback.
        entry.original.resize(entry.payload.size());
        if (!memory_.Read(mutationAddress, entry.original.data(),
                          entry.original.size(), error)) {
            result.diagnostic =
                Failure(description.feature, description.target,
                        PatchOperation::VerifyOriginalBytes, error);
            result.diagnostic.matchAddress = site;
            result.diagnostic.mutationAddress = mutationAddress;
            // The entry is not in `staged` yet, but it may already own a
            // trampoline: release it here (nothing mutated, so unconditional
            // release is safe) along with the staged set.
            ReleaseEntryTrampoline(allocator_, entry);
            ReleaseAllStagedTrampolines(allocator_, staged);
            return result;
        }
        // Overlap rejection against already-staged writes.
        for (const auto &other : staged) {
            if (RangesOverlap(other.mutationAddress, other.payload.size(),
                              entry.mutationAddress, entry.payload.size())) {
                result.diagnostic = Failure(
                    description.feature, description.target,
                    PatchOperation::ValidateDescription,
                    "patch batch contains overlapping writes");
                result.diagnostic.matchAddress = site;
                result.diagnostic.mutationAddress = mutationAddress;
                ReleaseEntryTrampoline(allocator_, entry);
                ReleaseAllStagedTrampolines(allocator_, staged);
                return result;
            }
        }
        staged.push_back(std::move(entry));
    }

    // Phase 2: commit in deterministic order with verified rollback.
    // Rollback membership follows WriteResult::bytesWritten, never the call's
    // boolean outcome: a failed write may still have mutated the target.
    for (std::size_t index = 0; index < staged.size(); ++index) {
        auto &entry = staged[index];
        std::vector<std::uint8_t> current(entry.original.size());
        std::string error;
        if (!memory_.Read(entry.mutationAddress, current.data(), current.size(),
                          error) ||
            current != entry.original) {
            result.diagnostic = Failure(
                entry.description->feature, entry.description->target,
                PatchOperation::VerifyOriginalBytes,
                error.empty() ? "original bytes changed before commit" : error);
            result.diagnostic.matchAddress = entry.siteAddress;
            result.diagnostic.mutationAddress = entry.mutationAddress;
            const auto outcome = RollbackStaged(memory_, allocator_, staged);
            AnnotateRollback(result, outcome, CountApplied(staged));
            return result;
        }
        const auto written = memory_.Write(entry.mutationAddress, entry.payload.data(),
                                           entry.payload.size());
        entry.mutationApplied = written.bytesWritten;
        if (!written.ok()) {
            result.diagnostic =
                Failure(entry.description->feature, entry.description->target,
                        PatchOperation::WriteMutation, written.error);
            if (written.bytesWritten && written.protectionRestored) {
                result.diagnostic.message += " (payload is live in the target)";
            } else if (written.bytesWritten) {
                result.diagnostic.message +=
                    " (payload is live but page protections were not restored)";
            }
            result.diagnostic.matchAddress = entry.siteAddress;
            result.diagnostic.mutationAddress = entry.mutationAddress;
            const auto outcome = RollbackStaged(memory_, allocator_, staged);
            AnnotateRollback(result, outcome, CountApplied(staged));
            return result;
        }
    }

    // Phase 3: optional hook optimization (atomic with the batch).
    for (std::size_t index = 0; index < staged.size(); ++index) {
        const auto &entry = staged[index];
        if (!entry.description->optimization.enabled) continue;
        const Address hookAddress =
            entry.description->optimization.hookAddress != 0
                ? entry.description->optimization.hookAddress
                : entry.description->mutation.target;
        auto optimized = runtime.OptimizeIndirectBranches(
            hookAddress, entry.description->optimization.maxScanSize,
            entry.description->feature, entry.description->target);
        if (!optimized.success) {
            result.diagnostic = std::move(optimized);
            const auto outcome = RollbackStaged(memory_, allocator_, staged);
            AnnotateRollback(result, outcome, CountApplied(staged));
            return result;
        }
    }

    // Success: publish continuations, keep trampolines alive.
    result.installations.reserve(staged.size());
    for (const auto &entry : staged) {
        InstallationResult installation;
        installation.diagnostic.success = true;
        installation.diagnostic.feature = entry.description->feature;
        installation.diagnostic.target = entry.description->target;
        installation.diagnostic.operation = PatchOperation::WriteMutation;
        installation.diagnostic.match = MatchStatus::Unique;
        installation.diagnostic.matchCount = 1;
        installation.diagnostic.matchAddress = entry.siteAddress;
        installation.diagnostic.mutationAddress = entry.mutationAddress;
        installation.diagnostic.message = "batch mutation applied";
        installation.continuations = entry.continuations;
        result.installations.push_back(std::move(installation));
    }
    result.diagnostic.success = true;
    result.diagnostic.feature = descriptions_.front().feature;
    result.diagnostic.target = descriptions_.front().target;
    result.diagnostic.operation = PatchOperation::CommitBatch;
    result.diagnostic.match = MatchStatus::Unique;
    result.diagnostic.matchCount = staged.size();
    std::ostringstream message;
    message << "batch committed " << staged.size() << " patch(es)";
    result.diagnostic.message = message.str();
    return result;
}

}  // namespace eu4dll::patch
